#include "platform/windows/explorer/explorer_mvp_session.h"

#include "core/behavior/move_magnet_session.h"
#include "platform/windows/explorer/explorer_mvp_attribution.h"
#include "platform/windows/operations/gesture_input_shield.h"
#include "platform/windows/operations/live_move_writer.h"
#include "platform/windows/operations/move_frame_continuity.h"

#include <windowsx.h>
#include <wtsapi32.h>

#include <atomic>
#include <algorithm>
#include <climits>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace panebind::platform::windows::explorer {
namespace {
namespace b = core::behavior;
namespace g = core::geometry;
namespace m = core::magnet;
namespace op = operations;

constexpr UINT wake_message = WM_APP + 0x59;
constexpr UINT hit_test_timeout_ms = 60;
constexpr UINT cancel_timeout_ms = 1000;
constexpr std::size_t max_resource_events = 4096;

[[nodiscard]] bool desktop_active() noexcept {
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) || !session) return false;
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, current_name[256]{};
    DWORD bytes{};
    const bool names = GetUserObjectInformationW(input, UOI_NAME, input_name,
            sizeof(input_name), &bytes) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME,
            current_name, sizeof(current_name), &bytes) &&
        std::wstring_view(input_name) == L"Default" &&
        std::wstring_view(current_name) == L"Default";
    CloseDesktop(input);
    if (!names) return false;
    LPWSTR data{};
    DWORD length{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                     WTSSessionInfoEx, &data, &length)) return false;
    bool active{};
    if (data && length >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(data);
        active = info.Level == 1 && info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(data);
    return active;
}

[[nodiscard]] HWND root_at(POINT point) noexcept {
    HWND hit = WindowFromPoint(point);
    return hit ? GetAncestor(hit, GA_ROOT) : nullptr;
}

[[nodiscard]] bool same_static_context(ExplorerWindowSnapshot first,
                                       ExplorerWindowSnapshot second) {
    first.visible_rect = second.visible_rect = {};
    first.positioning_rect = second.positioning_rect = {};
    return first == second;
}

[[nodiscard]] bool same_frame(const op::MoveFrameGeometry& first,
                              const op::MoveFrameGeometry& second) noexcept {
    return first.visible == second.visible && first.positioning == second.positioning;
}

[[nodiscard]] op::MoveFrameGeometry frame(const ExplorerWindowSnapshot& snapshot) noexcept {
    return {snapshot.positioning_rect, snapshot.visible_rect};
}

[[nodiscard]] g::Rect rect(const RECT& native) noexcept {
    return {native.left, native.top, native.right, native.bottom};
}

[[nodiscard]] bool valid_native_rect(const RECT& native) noexcept {
    return native.left < native.right && native.top < native.bottom;
}

[[nodiscard]] std::uint64_t qpc() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart > 0 ?
        static_cast<std::uint64_t>(value.QuadPart) : 0;
}

[[nodiscard]] std::uint64_t qpc_frequency() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceFrequency(&value) && value.QuadPart > 0 ?
        static_cast<std::uint64_t>(value.QuadPart) : 0;
}

[[nodiscard]] MvpSourceBinding binding(std::size_t index,
                                       const detail::GroupMemberBinding& member) noexcept {
    return {index, member.window_id, member.capability_generation,
            member.consent_generation, reinterpret_cast<std::uintptr_t>(member.window),
            member.process_id, member.thread_id};
}

[[nodiscard]] bool physical_left() noexcept {
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

[[nodiscard]] bool other_modifiers_up() noexcept {
    for (int key : {VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN})
        if ((GetAsyncKeyState(key) & 0x8000) != 0) return false;
    return true;
}

[[nodiscard]] MvpDownHit hit_test(HWND source, POINT point) noexcept {
    if (point.x < SHRT_MIN || point.x > SHRT_MAX ||
        point.y < SHRT_MIN || point.y > SHRT_MAX) return MvpDownHit::Unknown;
    DWORD_PTR answer{};
    if (!SendMessageTimeoutW(source, WM_NCHITTEST, 0,
        MAKELPARAM(static_cast<SHORT>(point.x), static_cast<SHORT>(point.y)),
        SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT | SMTO_BLOCK,
        hit_test_timeout_ms, &answer)) return MvpDownHit::Unknown;
    if (answer == HTCAPTION) return MvpDownHit::Caption;
    for (auto edge : {HTLEFT, HTRIGHT, HTTOP, HTBOTTOM,
                      HTTOPLEFT, HTTOPRIGHT, HTBOTTOMLEFT, HTBOTTOMRIGHT})
        if (answer == static_cast<DWORD_PTR>(edge)) return MvpDownHit::Resize;
    return MvpDownHit::Other;
}

[[nodiscard]] bool gui_native_move(HWND source, DWORD thread) noexcept {
    GUITHREADINFO gui{sizeof(gui)};
    return GetGUIThreadInfo(thread, &gui) && gui.hwndMoveSize == source &&
        gui.hwndCapture == source && !gui.hwndMenuOwner &&
        !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE));
}

[[nodiscard]] bool gui_after_end(DWORD thread) noexcept {
    GUITHREADINFO gui{sizeof(gui)};
    return GetGUIThreadInfo(thread, &gui) && !gui.hwndMoveSize &&
        !gui.hwndCapture && !gui.hwndMenuOwner &&
        !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE |
                       GUI_POPUPMENUMODE | GUI_INMOVESIZE));
}
} // namespace

struct ExplorerMvpSession::Impl final {
    enum class Phase { Down, NativeOnly, IsolationPending, AwaitEnd, Writing, Retiring };

    struct Gesture final {
        Gesture(std::uint64_t number, std::uint64_t packet, std::size_t member,
                detail::GroupSnapshots at_down)
            : generation(number), down_packet(packet), source(member), down(std::move(at_down)) {}
        const std::uint64_t generation, down_packet;
        std::uint64_t handoff_raw_watermark{};
        const std::size_t source;
        const detail::GroupSnapshots down;
        ExplorerMvpAttribution attribution;
        MvpGestureAttribution decision;
        b::MoveMagnetSession motion;
        op::MoveFrameContinuity continuity;
        std::mutex writer_mutex;
        std::mutex cancel_mutex;
        std::shared_ptr<op::LiveMoveWriter> writer;
        detail::GroupSnapshots handoff{}, last_capture{};
        std::atomic<bool> raw_up{false}, legacy_up{false}, isolated{false};
        std::atomic<bool> abnormal_up{false}, end_observed{false};
        std::atomic<bool> removed{false}, retired{false};
        std::atomic<HWND> overlay{nullptr};
        std::atomic<Phase> phase{Phase::Down};
        std::atomic<bool> plain_candidate{false};
        bool cancel_claimed{}; // serialized with the Raw UP observation
        bool isolation_requested{}, cancel_attempted{};
    };

    struct CallbackBridge final {
        explicit CallbackBridge(Impl* value) : owner(value) {}
        std::mutex mutex;
        Impl* owner{};
    };

    explicit Impl(std::unique_ptr<ExplorerGroupSession> value,
                  std::function<bool()> allowed)
        : owner(GetCurrentThreadId()), group(std::move(value)), environment_allowed(std::move(allowed)) {}

    DWORD owner{};
    std::unique_ptr<ExplorerGroupSession> group;
    std::unique_ptr<op::GestureInputShield> shield;
    std::shared_ptr<CallbackBridge> callback_bridge;
    std::function<bool()> environment_allowed;
    mutable std::mutex event_mutex;
    std::deque<op::GestureShieldEvent> event_queue;
    std::shared_ptr<Gesture> active;
    std::atomic<bool> queue_overflow{false}, resource_failure{false}, stop_requested{false};
    std::atomic<bool> deadline_expired{false};
    std::atomic<std::uint64_t> latest_raw_up_sequence{0};
    std::uint64_t last_raw_sequence{}, next_generation{};
    bool stopped{}, failed{};
    std::string_view why{"none"};

    [[nodiscard]] bool healthy() const noexcept {
        return GetCurrentThreadId() == owner && !stopped && !failed &&
            !queue_overflow && !resource_failure && group && group->healthy() &&
            group->product_running();
    }

    [[nodiscard]] std::shared_ptr<Gesture> gesture() const {
        std::lock_guard lock{event_mutex};
        return active;
    }

    void set_gesture(std::shared_ptr<Gesture> value) {
        std::lock_guard lock{event_mutex};
        active = std::move(value);
    }

    static void revoke(const std::shared_ptr<Gesture>& current,
                       b::MoveHandoffEscapeReason reason) noexcept {
        if (!current) return;
        current->retired = true;
        std::shared_ptr<op::LiveMoveWriter> writer;
        {
            std::lock_guard lock{current->writer_mutex};
            writer = current->writer;
        }
        if (writer) (void)writer->retire(current->generation);
        current->continuity.retire(current->generation);
        (void)current->motion.escape(current->generation, reason);
    }

    static void release_writer(const std::shared_ptr<Gesture>& current) noexcept {
        if (!current) return;
        std::shared_ptr<op::LiveMoveWriter> old;
        {
            std::lock_guard lock{current->writer_mutex};
            old = std::move(current->writer);
        }
        old.reset(); // breaks writer callbacks -> Gesture -> writer ownership cycle
    }

    void on_resource_event(const op::GestureShieldEvent& event) noexcept {
        // Publish a reliable physical UP before queue allocation/locking.
        // A cancel claim cannot overtake an UP already entered by this receiver.
        if (event.kind == op::GestureShieldEventKind::RawMouse &&
            (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_UP))
            latest_raw_up_sequence.store(event.raw_sequence, std::memory_order_release);
        std::shared_ptr<Gesture> current;
        bool remove_normal{};
        bool remove_early{};
        {
            std::lock_guard lock{event_mutex};
            current = active;
            if (event_queue.size() < max_resource_events) {
                try { event_queue.push_back(event); }
                catch (...) { queue_overflow = true; }
            } else queue_overflow = true;
        }
        if (current && (event.generation == 0 || event.generation == current->generation)) {
            if (event.kind == op::GestureShieldEventKind::RawMouse &&
                (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_UP) &&
                event.raw_sequence > current->down_packet) {
                {
                    std::lock_guard lock{current->cancel_mutex};
                    current->raw_up = true;
                    remove_early = !current->cancel_claimed || !current->end_observed;
                    current->abnormal_up = remove_early;
                }
                std::shared_ptr<op::LiveMoveWriter> writer;
                {
                    std::lock_guard lock{current->writer_mutex};
                    writer = current->writer;
                }
                if (writer) (void)writer->retire(current->generation);
                current->continuity.retire(current->generation);
                (void)current->motion.raw_up_observed(current->generation, true);
                if (remove_early) revoke(current, b::MoveHandoffEscapeReason::EarlyUp);
                remove_normal = current->legacy_up && !remove_early;
            } else if (event.kind == op::GestureShieldEventKind::LegacyLeftUp &&
                       event.generation == current->generation) {
                current->legacy_up = true;
                (void)current->motion.legacy_up_delivery_observed(current->generation, true);
                remove_normal = current->raw_up && !current->abnormal_up;
            } else if (event.kind == op::GestureShieldEventKind::IsolationReady &&
                       event.generation == current->generation) {
                current->overlay = event.overlay;
                current->isolated = true;
            } else if (event.kind == op::GestureShieldEventKind::IsolationGone &&
                       event.generation == current->generation) {
                current->isolated = false;
                current->removed = event.overlay_destroyed && event.hotkey_unregistered;
                if (current->removed)
                    (void)current->motion.isolation_removed(current->generation);
                if (!current->raw_up && !current->retired)
                    revoke(current, b::MoveHandoffEscapeReason::ContextLost);
            }
            if (event.kind == op::GestureShieldEventKind::Deadline ||
                event.kind == op::GestureShieldEventKind::ContextLost ||
                event.kind == op::GestureShieldEventKind::ResourceFailure ||
                event.kind == op::GestureShieldEventKind::HotkeyStop)
                revoke(current, event.kind == op::GestureShieldEventKind::Deadline ?
                    b::MoveHandoffEscapeReason::Deadline :
                    event.kind == op::GestureShieldEventKind::HotkeyStop ?
                    b::MoveHandoffEscapeReason::ExplicitStop :
                    b::MoveHandoffEscapeReason::ContextLost);
        }
        if (remove_normal && current && shield)
            (void)shield->request_remove(current->generation,
                                          op::GestureShieldRemovalReason::NormalUp);
        if (remove_early && current && shield)
            (void)shield->request_remove(current->generation,
                                          op::GestureShieldRemovalReason::SetupFailure);
        if (event.kind == op::GestureShieldEventKind::ResourceFailure) resource_failure = true;
        if (event.kind == op::GestureShieldEventKind::Deadline) deadline_expired = true;
        if (event.kind == op::GestureShieldEventKind::HotkeyStop) stop_requested = true;
        if (queue_overflow && current) {
            revoke(current, b::MoveHandoffEscapeReason::ContextLost);
            if (shield) (void)shield->request_remove(current->generation,
                op::GestureShieldRemovalReason::ContextLost);
        }
        if (!PostThreadMessageW(owner, wake_message, 0, 0)) {
            resource_failure = true;
            revoke(current, b::MoveHandoffEscapeReason::ContextLost);
            if (current && shield)
                (void)shield->request_remove(current->generation,
                    op::GestureShieldRemovalReason::ContextLost);
        }
    }

    [[nodiscard]] bool shield_authorized(const op::GestureShieldRequest& request) const noexcept {
        auto current = gesture();
        if (!current || current->generation != request.generation ||
            current->phase != Phase::IsolationPending || current->retired ||
            current->raw_up || !current->plain_candidate)
            return false;
        const auto& member = group->bindings()[current->source];
        return request.source == member.window && request.process_id == member.process_id &&
            request.thread_id == member.thread_id;
    }

    void fail(std::string_view reason, b::MoveHandoffEscapeReason escape,
              bool fatal = false) noexcept {
        why = reason;
        if (fatal) failed = true;
        auto current = gesture();
        revoke(current, escape);
        if (GetCurrentThreadId() == owner && group && group->seal_)
            detail::ExplorerGroupBridge::active(*group->seal_, group->sessions(), false);
        if (current) {
            current->phase = Phase::Retiring;
            if (current->isolation_requested && shield)
                (void)shield->request_remove(current->generation,
                    op::GestureShieldRemovalReason::ContextLost);
        }
    }

    [[nodiscard]] bool context_matches(const detail::GroupSnapshots& live,
                                       const Gesture& current,
                                       bool source_rect_may_differ) const {
        for (std::size_t i = 0; i < 3; ++i) {
            if (!same_static_context(live[i], current.down[i])) return false;
            if (i != current.source || !source_rect_may_differ) {
                if (live[i].visible_rect != current.down[i].visible_rect ||
                    live[i].positioning_rect != current.down[i].positioning_rect) return false;
            } else if (live[i].visible_rect.size() != current.down[i].visible_rect.size() ||
                       live[i].positioning_rect.size() != current.down[i].positioning_rect.size())
                return false;
        }
        return true;
    }

    [[nodiscard]] bool writer_context(const detail::GroupSnapshots& live,
                                      const Gesture& current) const {
        if (!context_matches(live, current, true) || !group->healthy() ||
            !group->product_running()) return false;
        for (std::size_t i = 0; i < 3; ++i) if (i != current.source &&
            (live[i].visible_rect != current.handoff[i].visible_rect ||
             live[i].positioning_rect != current.handoff[i].positioning_rect)) return false;
        const auto facts = group->event_facts();
        return facts.running && !facts.poisoned && !facts.overflow && !facts.post_failure &&
            !group->product_move_conflict_pending(current.source) && desktop_active();
    }

    [[nodiscard]] bool live_overlay(const Gesture& current, POINT cursor) const noexcept {
        const HWND overlay = current.overlay.load();
        return current.isolated && overlay && IsWindow(overlay) &&
            IsWindowVisible(overlay) && root_at(cursor) == overlay;
    }

    [[nodiscard]] bool live_write_authority(const Gesture& current,
                                             POINT cursor) const noexcept {
        const auto& member = group->bindings()[current.source];
        return !current.retired && !current.raw_up && current.end_observed &&
            latest_raw_up_sequence.load(std::memory_order_acquire) <= current.down_packet &&
            current.cancel_attempted && physical_left() &&
            GetForegroundWindow() == member.window &&
            gui_after_end(member.thread_id) && live_overlay(current, cursor);
    }

    [[nodiscard]] op::MoveWriteCallbacks callbacks(const std::shared_ptr<Gesture>& current) {
        return {
            [this, current]() -> std::optional<op::MoveFrameGeometry> {
                if (GetCurrentThreadId() != owner) return std::nullopt;
                auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
                if (!capture || !writer_context(*capture, *current)) return std::nullopt;
                current->last_capture = *capture;
                return frame((*capture)[current->source]);
            },
            [this, current](const op::MoveFrameGeometry& before) {
                const auto version = current->continuity.snapshot_version(current->generation);
                auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
                const bool valid = capture && writer_context(*capture, *current) &&
                    same_frame(frame((*capture)[current->source]), before) &&
                    op::classify_move_sample(current->continuity.observe(
                        current->generation, frame((*capture)[current->source]), version), true) ==
                        op::MoveSampleDecision::Process;
                POINT cursor{};
                const bool live = GetCursorPos(&cursor) && live_write_authority(*current, cursor);
                return op::classify_move_preflight({valid, live, current->raw_up});
            },
            [this, current](const op::MoveFrameGeometry& before, const g::Rect& target,
                            const g::Rect& positioning, std::uint64_t quantum) {
                if (!current->continuity.begin_attempt(current->generation, quantum,
                        before, {positioning, target})) return op::MoveNativePlacement{};
                auto writer = [&] {
                    std::lock_guard lock{current->writer_mutex};
                    return current->writer;
                }();
                if (!writer) {
                    (void)current->continuity.abort_unissued(current->generation, quantum);
                    return op::MoveNativePlacement{};
                }
                struct NativeGate final {
                    Impl* owner;
                    Gesture* gesture;
                    op::LiveMoveWriter* writer;
                } gate{this, current.get(), writer.get()};
                auto begin_native = [](void* opaque) noexcept -> bool {
                    auto& claim = *static_cast<NativeGate*>(opaque);
                    std::lock_guard lock{claim.gesture->cancel_mutex};
                    if (claim.gesture->raw_up || claim.gesture->retired ||
                        claim.owner->latest_raw_up_sequence.load(std::memory_order_acquire) >
                            claim.gesture->down_packet) return false;
                    return claim.writer->begin_native_attempt(claim.gesture->generation);
                };
                const auto receipt = detail::ExplorerGroupBridge::apply_mvp_move(
                    *group->seal_, group->sessions(), current->source,
                    current->last_capture, target, positioning, begin_native, &gate);
                if (!receipt.attempted)
                    (void)current->continuity.abort_unissued(current->generation, quantum);
                return op::MoveNativePlacement{receipt.attempted, receipt.succeeded, true};
            },
            [this, current](const op::MoveFrameGeometry&,
                            const op::MoveFrameGeometry& after) {
                const auto version = current->continuity.snapshot_version(current->generation);
                auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
                const bool valid = capture && writer_context(*capture, *current);
                const bool same = valid && same_frame(frame((*capture)[current->source]), after);
                const auto observation = valid ? current->continuity.observe(
                    current->generation, frame((*capture)[current->source]), version) :
                    op::MoveFrameObservation::NotArmed;
                return op::move_post_context_valid({
                    valid, valid, observation == op::MoveFrameObservation::Expected ||
                                  observation == op::MoveFrameObservation::OwnInFlight, same});
            }
        };
    }

    void raw_down(const op::GestureShieldEvent& event);
    void raw_motion(const op::GestureShieldEvent& event);
    void native_start(const GroupEventReceipt& receipt,
                      std::optional<std::size_t>& approved_ctrl,
                      const detail::GroupSnapshots*& ctrl_baseline);
    void native_end(const GroupEventReceipt& receipt);
    void cancel_after_ready(const std::shared_ptr<Gesture>& current);
    [[nodiscard]] bool pump();
    [[nodiscard]] bool stop() noexcept;
};

void ExplorerMvpSession::Impl::raw_down(const op::GestureShieldEvent& event) {
    if (gesture() || !healthy() || !event.cursor_available ||
        !event.foreground_positioning_available || !event.foreground_visible_available ||
        !valid_native_rect(event.foreground_positioning) ||
        !valid_native_rect(event.foreground_visible) || !event.message_time ||
        !event.observed_foreground || !event.message_point_root ||
        event.observed_foreground != event.message_point_root ||
        !event.foreground_gui_available ||
        event.foreground_capture || event.foreground_move_size ||
        (event.foreground_gui_flags & (GUI_INMOVESIZE | GUI_INMENUMODE |
             GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) ||
        !desktop_active() || !physical_left()) return;

    std::size_t source = 3;
    for (std::size_t i = 0; i < 3; ++i)
        if (group->bindings()[i].window == event.observed_foreground) source = i;
    if (source == 3) return;
    const auto& member = group->bindings()[source];
    if (event.foreground_thread_id != member.thread_id ||
        event.foreground_process_id != member.process_id) return;

    // A real Shell/COM capture is still required. If the frame changed between
    // the receiver's native DOWN snapshot and this owner-STA capture, the
    // original window anchor is unavailable; never replace it with "now".
    const auto captured = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!captured || (*captured)[source].positioning_rect !=
            rect(event.foreground_positioning) ||
        (*captured)[source].visible_rect != rect(event.foreground_visible)) return;
    const MvpDownHit hit = hit_test(member.window, event.message_point);
    MvpRawDownFacts facts{};
    facts.generation = ++next_generation;
    facts.packet_sequence = event.raw_sequence;
    facts.receiver_watermark = last_raw_sequence;
    facts.message_time = event.message_time;
    facts.source = binding(source, member);
    facts.message_point = g::Point{event.message_point.x, event.message_point.y};
    facts.hit_root = reinterpret_cast<std::uintptr_t>(event.message_point_root);
    facts.foreground_root = reinterpret_cast<std::uintptr_t>(event.observed_foreground);
    facts.initial_visible = (*captured)[source].visible_rect;
    facts.initial_positioning = (*captured)[source].positioning_rect;
    facts.hit = hit;
    facts.raw_left_down = true; // this branch is reached only for actual Raw DOWN
    facts.packet_valid = true; // shield emits only GetRawInputData-valid packets
    facts.receiver_healthy = !resource_failure && !queue_overflow;
    facts.hit_test_succeeded = hit != MvpDownHit::Unknown;
    facts.pre_native_gui_clear = true; // checked from the receiver's real GUI snapshot above
    facts.exact_consent_capture = true; // successful private three-member capture above
    facts.input_desktop_valid = true; // fresh desktop_active above
    auto current = std::make_shared<Gesture>(facts.generation, event.raw_sequence,
                                             source, *captured);
    if (!current->attribution.observe_down(facts)) return;
    set_gesture(std::move(current));
}

void ExplorerMvpSession::Impl::native_start(
    const GroupEventReceipt& receipt, std::optional<std::size_t>& approved_ctrl,
    const detail::GroupSnapshots*& ctrl_baseline) {
    auto current = gesture();
    if (!current || current->source != receipt.member_index || current->raw_up ||
        current->retired || current->phase != Phase::Down) return;
    const auto& member = group->bindings()[current->source];
    const auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!capture || !context_matches(*capture, *current, true)) {
        current->phase = Phase::NativeOnly;
        return;
    }
    GUITHREADINFO gui{sizeof(gui)};
    const bool gui_available = GetGUIThreadInfo(member.thread_id, &gui) != FALSE;
    const auto events = group->event_facts();
    MvpNativeStartFacts facts{};
    facts.generation = current->generation;
    facts.event_sequence = receipt.sequence;
    facts.native_time = receipt.native_time;
    facts.source = binding(current->source, member);
    facts.event_hwnd = reinterpret_cast<std::uintptr_t>(receipt.native_source);
    facts.foreground_root = reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
    facts.gui_move_size_hwnd = gui_available ?
        reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize) : 0;
    facts.ctrl = !receipt.ctrl.available ? MvpCtrlState::Unknown :
        receipt.ctrl.ctrl ? MvpCtrlState::Down : MvpCtrlState::Up;
    facts.exact_win_event = receipt.native_source == member.window &&
        receipt.native_thread == member.thread_id &&
        receipt.window_id == member.window_id &&
        receipt.capability_generation == member.capability_generation;
    facts.object_window_self = facts.exact_win_event; // event source filters OBJID_WINDOW/CHILDID_SELF
    facts.event_stream_healthy = events.running && !events.poisoned &&
        !events.overflow && !events.post_failure;
    facts.exact_consent_capture = true;
    facts.gui_in_move_size = gui_available && gui.hwndMoveSize == member.window &&
        gui.hwndCapture == member.window && !gui.hwndMenuOwner &&
        !(gui.flags & (GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE));
    facts.physical_left_held = physical_left();
    facts.input_desktop_valid = desktop_active();
    facts.other_modifiers_up = other_modifiers_up();
    current->decision = current->attribution.observe_start(facts);
    switch (current->decision.route) {
    case MvpGestureRoute::CtrlMove:
        current->phase = Phase::NativeOnly;
        approved_ctrl = current->source;
        ctrl_baseline = &current->down;
        return;
    case MvpGestureRoute::NativeResize:
        current->phase = Phase::NativeOnly;
        return;
    case MvpGestureRoute::Reject:
        current->phase = Phase::NativeOnly;
        return;
    case MvpGestureRoute::PlainMoveCandidate:
        break;
    }
    std::vector<m::MagnetTarget> targets;
    targets.reserve(2);
    for (std::size_t i = 0; i < 3; ++i) if (i != current->source)
        targets.push_back({core::model::WindowId{std::to_string(group->bindings()[i].window_id)},
                           current->down[i].visible_rect, false});
    const auto start_qpc = qpc();
    if (!start_qpc || !current->motion.begin(current->generation,
        current->decision.original_cursor, current->decision.initial_visible,
        targets, start_qpc, qpc_frequency(), true, true, true)) {
        current->phase = Phase::NativeOnly;
        return;
    }
    current->plain_candidate = true;
    current->phase = Phase::IsolationPending;
    current->isolation_requested = true;
    if (!shield->request_isolation({current->generation, member.window,
                                    member.process_id, member.thread_id})) {
        current->isolation_requested = false;
        fail("shield_request_failed", b::MoveHandoffEscapeReason::SetupFailure);
    }
}

void ExplorerMvpSession::Impl::cancel_after_ready(const std::shared_ptr<Gesture>& current) {
    if (!current || current->phase != Phase::IsolationPending || !current->isolated ||
        current->raw_up || current->retired || !current->plain_candidate ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet) return;
    const auto& member = group->bindings()[current->source];
    const auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    POINT cursor{};
    if (!capture || !context_matches(*capture, *current, true) ||
        !GetCursorPos(&cursor) || !live_overlay(*current, cursor) ||
        !gui_native_move(member.window, member.thread_id) ||
        !physical_left() || GetForegroundWindow() != member.window ||
        !desktop_active() || !group->healthy() || !group->product_running()) {
        fail("cancel_preflight_failed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    if (!current->motion.isolation_ready(current->generation, true) ||
        !current->motion.may_cancel(current->generation)) {
        fail("isolation_gate_rejected", b::MoveHandoffEscapeReason::SetupFailure);
        return;
    }
    {
        // The claim orders an early Raw UP against this single cancel attempt.
        // Never hold this short lock across SendMessageTimeout or a writer call.
        std::lock_guard lock{current->cancel_mutex};
        if (current->raw_up || current->retired ||
            latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet ||
            !current->motion.cancel_issued(current->generation)) {
            fail("cancel_revoked_before_claim", b::MoveHandoffEscapeReason::EarlyUp);
            return;
        }
        current->cancel_claimed = true;
    }
    current->cancel_attempted = true;
    current->phase = Phase::AwaitEnd;
    DWORD_PTR result{};
    SetLastError(ERROR_SUCCESS);
    // A successful send is not END. A timeout/unknown result is not retried.
    if (!SendMessageTimeoutW(member.window, WM_CANCELMODE, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT | SMTO_BLOCK,
            cancel_timeout_ms, &result)) {
        fail("bounded_cancel_failed_or_unknown", b::MoveHandoffEscapeReason::NativeFailure);
    }
}

void ExplorerMvpSession::Impl::native_end(const GroupEventReceipt& receipt) {
    auto current = gesture();
    if (!current || current->source != receipt.member_index ||
        current->phase != Phase::AwaitEnd || !current->cancel_attempted ||
        current->raw_up || current->retired || !current->isolated ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet) return;
    const auto& member = group->bindings()[current->source];
    const auto facts = group->event_facts();
    MvpNativeEndFacts end{};
    end.generation = current->generation;
    end.event_sequence = receipt.sequence;
    end.source = binding(current->source, member);
    end.event_hwnd = reinterpret_cast<std::uintptr_t>(receipt.native_source);
    end.exact_win_event = receipt.kind == GroupEventKind::End &&
        receipt.native_source == member.window &&
        receipt.native_thread == member.thread_id &&
        receipt.window_id == member.window_id &&
        receipt.capability_generation == member.capability_generation;
    end.object_window_self = end.exact_win_event; // filtered by event source
    end.event_stream_healthy = facts.running && !facts.poisoned &&
        !facts.overflow && !facts.post_failure;
    if (!current->attribution.observe_end(end)) {
        fail("native_end_not_attributed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    POINT cursor{};
    const auto handoff = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!handoff || !context_matches(*handoff, *current, true) ||
        !gui_after_end(member.thread_id) || GetForegroundWindow() != member.window ||
        !physical_left() || !GetCursorPos(&cursor) || !live_overlay(*current, cursor) ||
        !desktop_active() || !group->product_move_conflict_pending(current->source) ||
        !current->motion.native_end_observed(current->generation, true)) {
        fail("fresh_end_authority_failed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    current->end_observed = true;
    // This owner quantum may contain Raw samples from BEFORE END. The Raw and
    // WinEvent queues have no shared total order; drop the whole drained Raw
    // batch and accept only later continuation packets for this handoff.
    current->handoff_raw_watermark = last_raw_sequence;
    current->handoff = *handoff;
    if (!current->continuity.arm(current->generation, frame((*handoff)[current->source]))) {
        fail("handoff_continuity_failed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    // The already-consented source-only bridge is activated only for this
    // post-END plain Move. Ctrl/Resize never reach this branch.
    detail::ExplorerGroupBridge::active(*group->seal_, group->sessions(), true);
    auto writer = std::make_shared<op::LiveMoveWriter>(current->generation, callbacks(current));
    if (!writer->ready()) {
        fail("writer_unavailable", b::MoveHandoffEscapeReason::NativeFailure);
        return;
    }
    {
        std::lock_guard lock{current->writer_mutex};
        current->writer = std::move(writer);
    }
    if (current->raw_up) {
        revoke(current, b::MoveHandoffEscapeReason::EarlyUp);
        return;
    }
    current->phase = Phase::Writing;
}

void ExplorerMvpSession::Impl::raw_motion(const op::GestureShieldEvent& event) {
    auto current = gesture();
    if (!current || current->phase != Phase::Writing || current->raw_up ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet ||
        current->retired || !event.cursor_available ||
        event.raw_sequence <= current->down_packet ||
        event.raw_sequence <= current->handoff_raw_watermark) return;
    const auto version = current->continuity.snapshot_version(current->generation);
    const auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!capture) {
        fail("motion_capture_failed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    const auto observation = current->continuity.observe(current->generation,
        frame((*capture)[current->source]), version);
    const auto decision = op::classify_move_sample(observation,
        writer_context(*capture, *current));
    if (decision == op::MoveSampleDecision::DropStale) return;
    if (decision == op::MoveSampleDecision::Reject) {
        fail("motion_continuity_or_context_lost", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    const POINT cursor = event.cursor_now;
    const bool authority = live_write_authority(*current, cursor);
    const g::Point point{cursor.x, cursor.y};
    if (!current->motion.sample_cursor(current->generation, point, qpc(),
                                        authority, true, live_overlay(*current, cursor))) {
        if (!current->motion.retired(current->generation)) return; // duplicate cursor
        fail("motion_sample_retired", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    auto plan = current->motion.take_pending(current->generation, authority, true,
                                              live_overlay(*current, cursor));
    if (!plan) return;
    std::shared_ptr<op::LiveMoveWriter> writer;
    {
        std::lock_guard lock{current->writer_mutex};
        writer = current->writer;
    }
    if (!writer || !writer->offer({plan->generation, plan->quantum,
                                  plan->target_visible, plan->snapped})) {
        fail("writer_offer_failed", b::MoveHandoffEscapeReason::NativeFailure);
        return;
    }
    const auto receipt = writer->try_execute(); // owner STA: COM and SetWindowPos remain owner-affine
    if (!receipt) return; // revoked before native; no new placement
    const bool committed = current->continuity.finish_attempt(current->generation,
        receipt->quantum, receipt->native_attempted, receipt->exact, receipt->after);
    const auto actual = receipt->after ? receipt->after->visible : g::Rect{};
    (void)current->motion.write_result(*plan, receipt->exact, actual);
    if (op::move_write_receipt_failed(*receipt, committed))
        fail("writer_receipt_failed", b::MoveHandoffEscapeReason::NativeFailure);
}

bool ExplorerMvpSession::Impl::pump() {
    if (GetCurrentThreadId() != owner || stopped) return false;
    if (deadline_expired) {
        fail("gesture_deadline_expired", b::MoveHandoffEscapeReason::Deadline, true);
        (void)stop();
        return false;
    }
    if (stop_requested) {
        why = "hotkey_stop";
        (void)stop();
        return false;
    }
    if (queue_overflow || resource_failure) {
        fail("resource_event_loss", b::MoveHandoffEscapeReason::ContextLost, true);
        (void)stop();
        return false;
    }
    if (!healthy()) {
        fail("group_or_event_source_unhealthy", b::MoveHandoffEscapeReason::ContextLost, true);
        (void)stop();
        return false;
    }
    std::deque<op::GestureShieldEvent> events;
    {
        std::lock_guard lock{event_mutex};
        events.swap(event_queue);
    }
    std::vector<op::GestureShieldEvent> motion_events;
    motion_events.reserve(events.size());
    for (const auto& event : events) {
        if (event.kind != op::GestureShieldEventKind::RawMouse) {
            if (event.kind == op::GestureShieldEventKind::IsolationReady) {
                auto current = gesture();
                if (current && event.generation == current->generation)
                    cancel_after_ready(current);
            } else if (event.kind == op::GestureShieldEventKind::IsolationGone) {
                auto current = gesture();
                if (current && event.generation == current->generation &&
                    (!event.overlay_destroyed || !event.hotkey_unregistered))
                    fail("shield_removal_not_confirmed",
                         b::MoveHandoffEscapeReason::ContextLost, true);
            }
            continue;
        }
        if (event.raw_sequence <= last_raw_sequence) {
            fail("raw_sequence_not_increasing", b::MoveHandoffEscapeReason::ContextLost, true);
            break;
        }
        if (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) raw_down(event);
        if (event.raw_button_flags & RI_MOUSE_LEFT_BUTTON_UP) {
            auto current = gesture();
            if (current && event.raw_sequence > current->down_packet)
                (void)current->attribution.observe_raw_up(current->generation,
                                                            event.raw_sequence);
        }
        if (!(event.raw_button_flags &
              (RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_LEFT_BUTTON_UP)))
            motion_events.push_back(event);
        last_raw_sequence = event.raw_sequence;
    }
    if (failed) {
        (void)stop();
        return false;
    }

    auto receipts = group->drain_product_events();
    if (!receipts) {
        fail("group_event_drain_failed", b::MoveHandoffEscapeReason::ContextLost, true);
        (void)stop();
        return false;
    }
    std::optional<std::size_t> approved_ctrl;
    const detail::GroupSnapshots* ctrl_baseline{};
    for (std::size_t n = 0; n < receipts->size(); ++n) {
        const auto& receipt = (*receipts)[n];
        if (receipt.kind != GroupEventKind::Start) continue;
        const bool already_ended = std::any_of(receipts->begin() + static_cast<std::ptrdiff_t>(n + 1),
            receipts->end(), [&](const auto& next) {
                return next.kind == GroupEventKind::End &&
                    next.member_index == receipt.member_index;
            });
        if (!already_ended) native_start(receipt, approved_ctrl, ctrl_baseline);
    }
    if (!group->process_product_events(*receipts, approved_ctrl, ctrl_baseline)) {
        fail("group_product_processing_failed", b::MoveHandoffEscapeReason::ContextLost, true);
        (void)stop();
        return false;
    }
    for (const auto& receipt : *receipts) if (receipt.kind == GroupEventKind::End) {
        auto current = gesture();
        if (current && current->source == receipt.member_index) {
            if (current->phase == Phase::IsolationPending) {
                fail("native_end_before_shield_ready",
                     b::MoveHandoffEscapeReason::SetupFailure);
            } else if (current->phase == Phase::AwaitEnd) {
                native_end(receipt);
            } else if (current->phase == Phase::NativeOnly ||
                       current->phase == Phase::Down) {
                current->end_observed = true;
            }
        }
    }
    for (const auto& event : motion_events) raw_motion(event);

    auto current = gesture();
    if (current && (current->raw_up || current->retired) &&
        (current->phase != Phase::NativeOnly || current->end_observed) &&
        (!current->isolation_requested || current->removed)) {
        if (current->plain_candidate && group->seal_)
            detail::ExplorerGroupBridge::active(*group->seal_, group->sessions(), false);
        release_writer(current);
        set_gesture(nullptr);
    }
    return !failed && healthy();
}

bool ExplorerMvpSession::Impl::stop() noexcept {
    if (stopped) return !failed;
    if (GetCurrentThreadId() != owner) return false;
    stopped = true;
    auto current = gesture();
    revoke(current, b::MoveHandoffEscapeReason::ExplicitStop);
    release_writer(current);
    if (current && current->isolation_requested && shield)
        (void)shield->request_remove(current->generation,
                                     op::GestureShieldRemovalReason::ExplicitStop);
    // The shield may detach after its bounded stop timeout. Its worker owns
    // its resource state, but must never call back through a dead Session.
    // Every callback takes this short bridge lock; disabling it waits only
    // for a bounded, non-COM/non-writer callback already in progress.
    if (callback_bridge) {
        std::lock_guard lock{callback_bridge->mutex};
        callback_bridge->owner = nullptr;
    }
    const auto shield_facts = shield ? shield->stop() : op::GestureShieldStopFacts{};
    bool group_stopped = true;
    if (group && group->product_running()) group_stopped = group->stop_product_monitoring();
    else if (group && group->seal_)
        detail::ExplorerGroupBridge::retire(*group->seal_, group->sessions());
    set_gesture(nullptr);
    if (!shield_facts.clean() || !group_stopped) {
        failed = true;
        why = "stop_cleanup_not_confirmed";
    } else if (why == "none") why = "stopped";
    return !failed;
}

ExplorerMvpSession::ExplorerMvpSession(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

std::unique_ptr<ExplorerMvpSession> ExplorerMvpSession::create_after_explicit_consent(
    OwnedMembers members, std::function<bool()> environment_allowed) {
    if (!environment_allowed || !environment_allowed() || !desktop_active()) return nullptr;
    auto group = ExplorerGroupSession::create(std::move(members));
    if (!group || !detail::ExplorerGroupBridge::enable_live_magnet(
            *group->seal_, group->sessions()) || !group->start_product_monitoring())
        return nullptr;
    auto impl = std::make_unique<Impl>(std::move(group), std::move(environment_allowed));
    impl->callback_bridge = std::make_shared<Impl::CallbackBridge>(impl.get());
    std::weak_ptr<Impl::CallbackBridge> callback = impl->callback_bridge;
    impl->shield = std::make_unique<op::GestureInputShield>(op::GestureShieldCallbacks{
        [callback] {
            auto bridge = callback.lock();
            if (!bridge) return false;
            std::lock_guard lock{bridge->mutex};
            return bridge->owner && bridge->owner->environment_allowed &&
                bridge->owner->environment_allowed();
        },
        [callback](const op::GestureShieldRequest& request) {
            auto bridge = callback.lock();
            if (!bridge) return false;
            std::lock_guard lock{bridge->mutex};
            return bridge->owner && bridge->owner->shield_authorized(request);
        },
        [callback](const op::GestureShieldEvent& event) {
            auto bridge = callback.lock();
            if (!bridge) return;
            std::lock_guard lock{bridge->mutex};
            if (bridge->owner) bridge->owner->on_resource_event(event);
        }
    });
    if (!impl->shield->start()) {
        (void)impl->stop();
        return nullptr;
    }
    return std::unique_ptr<ExplorerMvpSession>(new ExplorerMvpSession(std::move(impl)));
}

ExplorerMvpSession::~ExplorerMvpSession() {
    if (impl_) (void)impl_->stop();
}

bool ExplorerMvpSession::pump() { return impl_ && impl_->pump(); }

std::optional<GroupProductStatus> ExplorerMvpSession::status() {
    if (!impl_ || !impl_->healthy()) return std::nullopt;
    auto result = impl_->group->refresh_product_status();
    auto current = impl_->gesture();
    if (result && current && !current->raw_up && !current->retired &&
        (current->phase == Impl::Phase::IsolationPending ||
         current->phase == Impl::Phase::AwaitEnd ||
         current->phase == Impl::Phase::Writing))
        result->gesture_active = true;
    return result;
}

bool ExplorerMvpSession::stop() { return impl_ && impl_->stop(); }

bool ExplorerMvpSession::healthy() const noexcept { return impl_ && impl_->healthy(); }

std::string_view ExplorerMvpSession::reason() const noexcept {
    return impl_ ? impl_->why : "missing_session";
}

const std::array<detail::GroupMemberBinding, 3>& ExplorerMvpSession::bindings() const noexcept {
    return impl_->group->bindings();
}

} // namespace panebind::platform::windows::explorer
