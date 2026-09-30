#include "platform/windows/explorer/explorer_mvp_session.h"

#include "core/behavior/move_magnet_session.h"
#include "platform/windows/explorer/explorer_mvp_attribution.h"
#include "platform/windows/explorer/explorer_mvp_end_authority.h"
#include "platform/windows/operations/gesture_input_shield.h"
#include "platform/windows/operations/live_move_writer.h"
#include "platform/windows/operations/move_frame_continuity.h"

#include <windowsx.h>
#include <wtsapi32.h>

#include <atomic>
#include <algorithm>
#include <array>
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

[[nodiscard]] std::string_view shield_removal_name(
    op::GestureShieldRemovalReason reason) noexcept {
    switch (reason) {
    case op::GestureShieldRemovalReason::NormalUp: return "normal_up";
    case op::GestureShieldRemovalReason::ExplicitStop: return "explicit_stop";
    case op::GestureShieldRemovalReason::ContextLost: return "context_lost";
    case op::GestureShieldRemovalReason::SetupFailure: return "setup_failure";
    case op::GestureShieldRemovalReason::Deadline: return "deadline";
    case op::GestureShieldRemovalReason::Shutdown: return "shutdown";
    }
    return "unknown";
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
    static constexpr std::size_t max_evidence_events = 2048;
    std::array<MvpEvidenceEvent, max_evidence_events> evidence_events{};
    std::size_t evidence_count{};
    bool stopped{}, failed{};
    std::string_view why{"none"};

    [[nodiscard]] static MvpEvidenceEvent event_for(MvpEvidenceKind kind,
        const std::shared_ptr<Gesture>& current, std::string_view reason = "none") noexcept {
        MvpEvidenceEvent event;
        event.kind = kind;
        event.reason = reason;
        if (current) {
            event.generation = current->generation;
            event.source_member = current->source;
        }
        return event;
    }

    [[nodiscard]] bool record(MvpEvidenceEvent event) noexcept {
        // Owner-STA only: this bounded memory write never touches the input
        // resource thread or the app's evidence file.
        if (GetCurrentThreadId() != owner || evidence_count == max_evidence_events) {
            failed = true;
            why = "evidence_event_capacity";
            auto current = gesture();
            revoke(current, b::MoveHandoffEscapeReason::ContextLost);
            if (current && current->isolation_requested && shield)
                (void)shield->request_remove(current->generation,
                    op::GestureShieldRemovalReason::ContextLost);
            return false;
        }
        evidence_events[evidence_count++] = event;
        return true;
    }

    [[nodiscard]] std::vector<MvpEvidenceEvent> drain_evidence_events() {
        if (GetCurrentThreadId() != owner) return {};
        std::vector<MvpEvidenceEvent> result(evidence_events.begin(),
                                              evidence_events.begin() + evidence_count);
        evidence_count = 0;
        return result;
    }

    void record_terminal_resource_events() noexcept {
        std::deque<op::GestureShieldEvent> pending;
        {
            std::lock_guard lock{event_mutex};
            pending.swap(event_queue);
        }
        for (const auto& item : pending) {
            auto current = gesture();
            MvpEvidenceEvent event;
            if (item.kind == op::GestureShieldEventKind::RawMouse) {
                if (!(item.raw_button_flags & RI_MOUSE_LEFT_BUTTON_UP)) continue;
                event = event_for(MvpEvidenceKind::RawUp, current,
                                  "receiver_packet_observed");
                event.raw_sequence = item.raw_sequence;
            } else if (item.kind == op::GestureShieldEventKind::LegacyLeftUp) {
                event = event_for(MvpEvidenceKind::LegacyUp, current,
                                  "overlay_message_observed");
            } else if (item.kind == op::GestureShieldEventKind::IsolationReady) {
                event = event_for(MvpEvidenceKind::IsolationReady, current,
                                  "readback_observed");
                event.succeeded = true;
            } else if (item.kind == op::GestureShieldEventKind::IsolationGone) {
                event = event_for(MvpEvidenceKind::IsolationGone, current,
                                  shield_removal_name(item.removal_reason));
                event.overlay_destroyed = item.overlay_destroyed;
                event.hotkey_unregistered = item.hotkey_unregistered;
            } else {
                const std::string_view reason =
                    item.kind == op::GestureShieldEventKind::Deadline ? "deadline_observed" :
                    item.kind == op::GestureShieldEventKind::HotkeyStop ? "hotkey_stop_observed" :
                    item.kind == op::GestureShieldEventKind::ContextLost ? "context_lost_observed" :
                    "resource_failure_observed";
                event = event_for(MvpEvidenceKind::Resource, current, reason);
                if (item.kind == op::GestureShieldEventKind::ResourceFailure) {
                    event.win32_error = item.win32_error;
                    event.shield_setup_stage = static_cast<std::uint32_t>(item.setup_stage);
                    event.shield_native_failure = static_cast<std::uint32_t>(item.native_failure);
                    event.shield_readback_failure = static_cast<std::uint32_t>(item.readback_failure);
                    event.shield_observed_exstyle = item.observed_exstyle;
                }
            }
            if (item.generation) event.generation = item.generation;
            event.overlay = reinterpret_cast<std::uintptr_t>(item.overlay);
            if (!record(event)) return;
        }
    }

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
        (void)record(event_for(MvpEvidenceKind::Rejected, current, reason));
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
    void native_end(const GroupEventReceipt& receipt, bool processed_batch_conflict);
    void cancel_after_ready(const std::shared_ptr<Gesture>& current);
    [[nodiscard]] bool pump();
    [[nodiscard]] bool stop() noexcept;
};

void ExplorerMvpSession::Impl::raw_down(const op::GestureShieldEvent& event) {
    if (!event.observed_foreground || !group) return;
    std::size_t source = 3;
    for (std::size_t i = 0; i < 3; ++i)
        if (group->bindings()[i].window == event.observed_foreground) source = i;
    if (source == 3) return;
    const auto reject = [&](std::string_view reason) {
        MvpEvidenceEvent evidence = event_for(MvpEvidenceKind::Rejected, gesture(), reason);
        evidence.source_member = source;
        evidence.raw_sequence = event.raw_sequence;
        if (event.cursor_available)
            evidence.cursor = g::Point{event.message_point.x, event.message_point.y};
        (void)record(evidence);
    };
    if (gesture() || !healthy()) { reject("down_while_unavailable"); return; }
    if (!event.cursor_available || !event.foreground_positioning_available ||
        !event.foreground_visible_available ||
        !valid_native_rect(event.foreground_positioning) ||
        !valid_native_rect(event.foreground_visible) || !event.message_time ||
        !event.message_point_root || event.observed_foreground != event.message_point_root ||
        !event.foreground_gui_available || event.foreground_capture ||
        event.foreground_move_size ||
        (event.foreground_gui_flags & (GUI_INMOVESIZE | GUI_INMENUMODE |
            GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) ||
        !desktop_active() || !physical_left()) {
        reject("down_snapshot_or_gui_unavailable");
        return;
    }
    const auto& member = group->bindings()[source];
    if (event.foreground_thread_id != member.thread_id ||
        event.foreground_process_id != member.process_id) {
        reject("down_process_or_thread_changed");
        return;
    }

    // A real Shell/COM capture is still required. If the frame changed between
    // the receiver's native DOWN snapshot and this owner-STA capture, the
    // original window anchor is unavailable; never replace it with "now".
    const auto captured = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!captured || (*captured)[source].positioning_rect !=
            rect(event.foreground_positioning) ||
        (*captured)[source].visible_rect != rect(event.foreground_visible)) {
        reject("down_anchor_not_fresh_or_consent_lost");
        return;
    }
    const MvpDownHit hit = hit_test(member.window, event.message_point);
    if (hit == MvpDownHit::Unknown || hit == MvpDownHit::Other) {
        reject(hit == MvpDownHit::Unknown ? "down_hit_test_unavailable" :
                                             "down_not_caption_or_resize");
        return;
    }
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
    if (!current->attribution.observe_down(facts)) {
        reject("down_attribution_rejected");
        return;
    }
    auto evidence = event_for(MvpEvidenceKind::Down, current, "candidate_only");
    evidence.raw_sequence = event.raw_sequence;
    evidence.cursor = facts.message_point;
    evidence.initial_visible = facts.initial_visible;
    evidence.actual_positioning = facts.initial_positioning;
    if (!record(evidence)) return;
    set_gesture(std::move(current));
}

void ExplorerMvpSession::Impl::native_start(
    const GroupEventReceipt& receipt, std::optional<std::size_t>& approved_ctrl,
    const detail::GroupSnapshots*& ctrl_baseline) {
    auto current = gesture();
    if (!current || current->source != receipt.member_index) return;
    if (current->raw_up || current->retired || current->phase != Phase::Down) {
        auto rejected = event_for(MvpEvidenceKind::Start, current,
                                    "start_after_up_or_wrong_phase");
        rejected.native_sequence = receipt.sequence;
        rejected.route = MvpGestureRoute::Reject;
        (void)record(rejected);
        return;
    }
    const auto& member = group->bindings()[current->source];
    const auto capture = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    if (!capture || !context_matches(*capture, *current, true)) {
        current->phase = Phase::NativeOnly;
        auto evidence = event_for(MvpEvidenceKind::Rejected, current,
                                   "start_fresh_context_unavailable");
        evidence.native_sequence = receipt.sequence;
        (void)record(evidence);
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
    auto route_event = event_for(MvpEvidenceKind::Start, current,
        current->decision.route == MvpGestureRoute::Reject ?
            "start_attribution_rejected" : "route_candidate");
    route_event.native_sequence = receipt.sequence;
    route_event.raw_sequence = current->down_packet;
    route_event.route = current->decision.route;
    if (!record(route_event)) {
        current->phase = Phase::NativeOnly;
        return;
    }
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
        (void)record(event_for(MvpEvidenceKind::Rejected, current,
                               "move_session_begin_rejected"));
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
    if (!current || current->phase != Phase::IsolationPending) return;
    if (!current->isolated || current->raw_up || current->retired ||
        !current->plain_candidate ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet) {
        (void)record(event_for(MvpEvidenceKind::Rejected, current,
                               "isolation_ready_after_up_or_revocation"));
        return;
    }
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
    auto claim = event_for(MvpEvidenceKind::Cancel, current, "claimed_before_api");
    claim.native_sequence = current->decision.start_event_sequence;
    // A claim orders UP, but does not prove WM_CANCELMODE was delivered.
    if (!record(claim)) return;
    DWORD_PTR result{};
    SetLastError(ERROR_SUCCESS);
    // A successful send is not END. A timeout/unknown result is not retried.
    const bool sent = SendMessageTimeoutW(member.window, WM_CANCELMODE, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT | SMTO_BLOCK,
            cancel_timeout_ms, &result) != 0;
    const DWORD error = sent ? ERROR_SUCCESS : GetLastError();
    auto outcome = event_for(MvpEvidenceKind::Cancel, current,
                              sent ? "api_returned" : "api_failed_or_timed_out");
    outcome.attempted = true;
    outcome.succeeded = sent;
    outcome.outcome_known = sent;
    outcome.win32_error = error;
    (void)record(outcome);
    if (!sent) {
        fail("bounded_cancel_failed_or_unknown", b::MoveHandoffEscapeReason::NativeFailure);
    }
}

void ExplorerMvpSession::Impl::native_end(const GroupEventReceipt& receipt,
                                          bool processed_batch_conflict) {
    auto current = gesture();
    if (!current || current->source != receipt.member_index) return;
    if (current->phase != Phase::AwaitEnd || !current->cancel_attempted ||
        current->raw_up || current->retired || !current->isolated ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet) {
        auto rejected = event_for(MvpEvidenceKind::Handoff, current,
                                    "end_after_up_or_takeover_revoked");
        rejected.native_sequence = receipt.sequence;
        (void)record(rejected);
        return;
    }
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
        auto rejected = event_for(MvpEvidenceKind::NativeEnd, current,
                                    "end_attribution_rejected");
        rejected.native_sequence = receipt.sequence;
        (void)record(rejected);
        fail("native_end_not_attributed", b::MoveHandoffEscapeReason::ContextLost);
        return;
    }
    auto observed = event_for(MvpEvidenceKind::NativeEnd, current,
                               "matching_receipt_observed");
    observed.native_sequence = receipt.sequence;
    if (!record(observed)) return;
    POINT cursor{};
    const auto handoff = detail::ExplorerGroupBridge::capture(*group->seal_, group->sessions());
    const bool cursor_available = GetCursorPos(&cursor) != FALSE;
    const bool pending_group_conflict =
        group->product_move_conflict_pending(current->source);
    const MvpEndAuthorityFacts end_authority{
        handoff && context_matches(*handoff, *current, true),
        gui_after_end(member.thread_id),
        GetForegroundWindow() == member.window,
        physical_left(), cursor_available,
        cursor_available && live_overlay(*current, cursor),
        desktop_active(),
        current->raw_up ||
            latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet};
    if (!mvp_end_authority_ready(end_authority, processed_batch_conflict,
                                 pending_group_conflict) ||
        !current->motion.native_end_observed(current->generation, true)) {
        const std::string_view reason = (processed_batch_conflict || pending_group_conflict) ?
            "pending_event_conflict" : end_authority.raw_up_seen ?
            "raw_up_before_handoff" : !end_authority.exact_handoff_context ?
            "handoff_context_unavailable" : !end_authority.overlay_valid ?
            "isolation_not_valid" : "fresh_end_authority_failed";
        auto rejected = event_for(MvpEvidenceKind::Handoff, current, reason);
        rejected.native_sequence = receipt.sequence;
        (void)record(rejected);
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
    auto accepted = event_for(MvpEvidenceKind::Handoff, current,
                               "writer_armed_after_real_end");
    accepted.native_sequence = receipt.sequence;
    accepted.raw_sequence = current->handoff_raw_watermark;
    accepted.succeeded = true;
    accepted.initial_visible = current->decision.initial_visible;
    accepted.actual_visible = (*handoff)[current->source].visible_rect;
    accepted.actual_positioning = (*handoff)[current->source].positioning_rect;
    (void)record(accepted);
}

void ExplorerMvpSession::Impl::raw_motion(const op::GestureShieldEvent& event) {
    auto current = gesture();
    if (!current || current->phase != Phase::Writing || current->raw_up ||
        latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet ||
        current->retired || !event.cursor_available ||
        !mvp_raw_continuation_after_handoff(event.raw_sequence,
            current->down_packet, current->handoff_raw_watermark)) return;
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
        const bool normal_up = current->raw_up ||
            latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet;
        auto rejected = event_for(MvpEvidenceKind::Writer, current,
            normal_up ? "normal_up_before_offer" : "offer_rejected");
        rejected.raw_sequence = event.raw_sequence;
        rejected.quantum = plan->quantum;
        rejected.target_visible = plan->target_visible;
        rejected.snapped = plan->snapped;
        rejected.attempted = false;
        (void)record(rejected);
        if (!normal_up)
            fail("writer_offer_failed", b::MoveHandoffEscapeReason::NativeFailure);
        return;
    }
    const auto receipt = writer->try_execute(); // owner STA: COM and SetWindowPos remain owner-affine
    if (!receipt) {
        const bool normal_up = current->raw_up ||
            latest_raw_up_sequence.load(std::memory_order_acquire) > current->down_packet;
        auto revoked = event_for(MvpEvidenceKind::Writer, current,
            normal_up ? "normal_up_before_execution" : "retired_before_execution");
        revoked.raw_sequence = event.raw_sequence;
        revoked.quantum = plan->quantum;
        revoked.target_visible = plan->target_visible;
        revoked.snapped = plan->snapped;
        revoked.attempted = false;
        (void)record(revoked);
        return; // revoked before native; no new placement
    }
    const bool committed = current->continuity.finish_attempt(current->generation,
        receipt->quantum, receipt->native_attempted, receipt->exact, receipt->after);
    const auto actual = receipt->after ? receipt->after->visible : g::Rect{};
    (void)current->motion.write_result(*plan, receipt->exact, actual);
    auto evidence = event_for(MvpEvidenceKind::Writer, current, receipt->reason);
    evidence.raw_sequence = event.raw_sequence;
    evidence.quantum = receipt->quantum;
    evidence.target_visible = receipt->target_visible;
    evidence.snapped = receipt->snapped;
    if (receipt->after) {
        evidence.actual_visible = receipt->after->visible;
        evidence.actual_positioning = receipt->after->positioning;
    }
    evidence.attempted = receipt->native_attempted;
    if (receipt->native_attempted) {
        evidence.succeeded = receipt->native_succeeded;
        evidence.outcome_known = receipt->native_outcome_known;
    }
    evidence.geometry_exact = receipt->geometry_exact;
    evidence.post_context_exact = receipt->post_context_exact;
    if (!record(evidence)) return;
    if (op::move_write_receipt_failed(*receipt, committed))
        fail("writer_receipt_failed", b::MoveHandoffEscapeReason::NativeFailure);
}

bool ExplorerMvpSession::Impl::pump() {
    if (GetCurrentThreadId() != owner || stopped) return false;
    if (deadline_expired || stop_requested || queue_overflow || resource_failure)
        record_terminal_resource_events();
    if (failed) {
        (void)stop();
        return false;
    }
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
            auto current = gesture();
            MvpEvidenceEvent evidence;
            switch (event.kind) {
            case op::GestureShieldEventKind::IsolationReady:
                evidence = event_for(MvpEvidenceKind::IsolationReady, current,
                                     "readback_observed");
                evidence.succeeded = true;
                break;
            case op::GestureShieldEventKind::LegacyLeftUp:
                evidence = event_for(MvpEvidenceKind::LegacyUp, current,
                                     "overlay_message_observed");
                break;
            case op::GestureShieldEventKind::IsolationGone:
                evidence = event_for(MvpEvidenceKind::IsolationGone, current,
                                     shield_removal_name(event.removal_reason));
                evidence.overlay_destroyed = event.overlay_destroyed;
                evidence.hotkey_unregistered = event.hotkey_unregistered;
                break;
            case op::GestureShieldEventKind::HotkeyStop:
                evidence = event_for(MvpEvidenceKind::Resource, current,
                                     "hotkey_stop_observed");
                break;
            case op::GestureShieldEventKind::Deadline:
                evidence = event_for(MvpEvidenceKind::Resource, current,
                                     "deadline_observed");
                break;
            case op::GestureShieldEventKind::ContextLost:
                evidence = event_for(MvpEvidenceKind::Resource, current,
                                     "context_lost_observed");
                break;
            case op::GestureShieldEventKind::ResourceFailure:
                evidence = event_for(MvpEvidenceKind::Resource, current,
                                     "resource_failure_observed");
                evidence.win32_error = event.win32_error;
                evidence.shield_setup_stage = static_cast<std::uint32_t>(event.setup_stage);
                evidence.shield_native_failure = static_cast<std::uint32_t>(event.native_failure);
                evidence.shield_readback_failure = static_cast<std::uint32_t>(event.readback_failure);
                evidence.shield_observed_exstyle = event.observed_exstyle;
                break;
            case op::GestureShieldEventKind::RawMouse:
                break;
            }
            if (event.generation) evidence.generation = event.generation;
            evidence.overlay = reinterpret_cast<std::uintptr_t>(event.overlay);
            if (!record(evidence)) break;
            if (event.kind == op::GestureShieldEventKind::IsolationReady) {
                if (current && event.generation == current->generation)
                    cancel_after_ready(current);
            } else if (event.kind == op::GestureShieldEventKind::IsolationGone) {
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
            auto evidence = event_for(MvpEvidenceKind::RawUp, current,
                                       "receiver_packet_observed");
            evidence.raw_sequence = event.raw_sequence;
            if (event.generation) evidence.generation = event.generation;
            if (!record(evidence)) break;
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
        auto current = gesture();
        if (already_ended || !current || current->source != receipt.member_index) {
            auto skipped = event_for(MvpEvidenceKind::Start, current,
                already_ended ? "start_and_end_same_batch" : "no_matching_raw_down");
            skipped.source_member = receipt.member_index;
            skipped.native_sequence = receipt.sequence;
            skipped.route = MvpGestureRoute::Reject;
            if (!record(skipped)) break;
            continue;
        }
        native_start(receipt, approved_ctrl, ctrl_baseline);
    }
    if (failed) {
        (void)stop();
        return false;
    }
    if (!group->process_product_events(*receipts, approved_ctrl, ctrl_baseline)) {
        fail("group_product_processing_failed", b::MoveHandoffEscapeReason::ContextLost, true);
        (void)stop();
        return false;
    }
    for (std::size_t index = 0; index < receipts->size(); ++index) {
        const auto& receipt = (*receipts)[index];
        if (receipt.kind != GroupEventKind::End) continue;
        auto current = gesture();
        if (current && current->source == receipt.member_index) {
            auto end_receipt = event_for(MvpEvidenceKind::NativeEnd, current,
                                          "event_source_receipt");
            end_receipt.native_sequence = receipt.sequence;
            if (!record(end_receipt)) break;
            const bool batch_conflict = product_move_handoff_batch_conflicts(
                current->source, receipt.sequence, *receipts);
            if (current->phase == Phase::IsolationPending) {
                fail("native_end_before_shield_ready",
                     b::MoveHandoffEscapeReason::SetupFailure);
            } else if (current->phase == Phase::AwaitEnd) {
                native_end(receipt, batch_conflict);
            } else if (current->phase == Phase::NativeOnly ||
                       current->phase == Phase::Down) {
                current->end_observed = true;
            }
        }
    }
    if (failed) {
        (void)stop();
        return false;
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
    record_terminal_resource_events();
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
    auto teardown = event_for(MvpEvidenceKind::Resource, current, "stop_cleanup");
    teardown.succeeded = shield_facts.clean() && group_stopped;
    teardown.overlay_destroyed = shield_facts.overlay_destroyed;
    teardown.hotkey_unregistered = shield_facts.hotkey_unregistered;
    teardown.receiver_destroyed = shield_facts.receiver_destroyed;
    teardown.raw_registration_removed = shield_facts.raw_registration_removed;
    teardown.winevent_unhooked = shield_facts.winevent_unhooked;
    teardown.classes_unregistered = shield_facts.classes_unregistered;
    (void)record(teardown);
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

std::vector<MvpEvidenceEvent> ExplorerMvpSession::drain_evidence_events() {
    return impl_ ? impl_->drain_evidence_events() : std::vector<MvpEvidenceEvent>{};
}

} // namespace panebind::platform::windows::explorer
