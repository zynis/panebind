#include "platform/windows/explorer/explorer_mvp_session.h"
#include "platform/windows/explorer/explorer_mvp_guest_contract.h"
#include "platform/windows/console/sta_console_line_reader.h"
#include "platform/windows/text_encoding.h"

#include <windows.h>
#include <wtsapi32.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif

namespace explorer = panebind::platform::windows::explorer;
namespace windows = panebind::platform::windows;
namespace operations = panebind::platform::windows::operations;
namespace console_input = panebind::platform::windows::console_input;

namespace {
constexpr wchar_t guest_marker[] = L"C:\\PaneBindMVP1\\Input\\run-id.txt";

bool interactive_default_desktop() noexcept {
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, current_name[256]{};
    DWORD bytes{};
    const bool same =
        GetUserObjectInformationW(input, UOI_NAME, input_name,
                                  sizeof(input_name), &bytes) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),
                                  UOI_NAME, current_name,
                                  sizeof(current_name), &bytes) &&
        std::wstring_view(input_name) == current_name &&
        std::wstring_view(input_name) == L"Default";
    CloseDesktop(input);
    if (!same) return false;

    LPWSTR data{};
    DWORD size{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     WTS_CURRENT_SESSION, WTSSessionInfoEx,
                                     &data, &size)) return false;
    bool active = false;
    if (data && size >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(data);
        active = info.Level == 1 &&
            info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(data);
    return active;
}

// An accidental-host-run guard, not a security boundary against a forged
// guest. It runs before opening evidence or creating any GUI / Raw resource.
bool sandbox_run_authorized(std::wstring_view run_id,
                            std::wstring_view evidence_log) noexcept {
    if (run_id.size() != 32 ||
        !std::all_of(run_id.begin(), run_id.end(), [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') ||
                   (ch >= L'a' && ch <= L'f');
        })) return false;
    wchar_t user[256]{};
    DWORD user_length = static_cast<DWORD>(std::size(user));
    if (!GetUserNameW(user, &user_length) ||
        std::wstring_view(user) != L"WDAGUtilityAccount") return false;
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) ||
        session == 0 || !interactive_default_desktop()) return false;

    wchar_t executable[MAX_PATH]{};
    const DWORD executable_length = GetModuleFileNameW(
        nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (executable_length == 0 || executable_length >= std::size(executable) ||
        !explorer::exact_mvp1_guest_launch_artifacts(
            std::wstring_view(executable, executable_length), run_id, evidence_log))
        return false;

    const HANDLE marker = CreateFileW(guest_marker, GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (marker == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char, 32> bytes{};
    DWORD read{};
    const bool valid = GetFileSizeEx(marker, &size) &&
        size.QuadPart == static_cast<LONGLONG>(bytes.size()) &&
        ReadFile(marker, bytes.data(), static_cast<DWORD>(bytes.size()),
                 &read, nullptr) && read == bytes.size() &&
        std::equal(bytes.begin(), bytes.end(), run_id.begin(),
            [](char lhs, wchar_t rhs) {
                return static_cast<unsigned char>(lhs) == rhs;
            });
    CloseHandle(marker);
    return valid;
}

bool console_handle(HANDLE handle) noexcept {
    DWORD mode{};
    return handle && handle != INVALID_HANDLE_VALUE &&
           GetFileType(handle) == FILE_TYPE_CHAR &&
           GetConsoleMode(handle, &mode);
}

bool print(std::wstring_view message) noexcept {
    DWORD written{};
    return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), message.data(),
                         static_cast<DWORD>(message.size()), &written,
                         nullptr) && written == message.size();
}

std::string quote(std::string_view value) {
    return windows::json_quote(value);
}
std::string quote(std::wstring_view value) {
    const auto converted = windows::utf16_to_utf8(value);
    if (!converted.value) return windows::json_quote("invalid_utf16");
    return quote(*converted.value);
}

class Evidence final {
public:
    explicit Evidence(const wchar_t* path) noexcept
        : file_(CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~Evidence() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }
    Evidence(const Evidence&) = delete;
    Evidence& operator=(const Evidence&) = delete;

    bool record(std::string_view type, std::string_view fields = {}) {
        if (!healthy()) return false;
        const auto line = "{\"schema\":\"mvp1/entry-v1\",\"sequence\":" +
            std::to_string(++sequence_) + ",\"type\":" + quote(type) +
            std::string(fields) + "}\n";
        DWORD written{};
        good_ = WriteFile(file_, line.data(), static_cast<DWORD>(line.size()),
                          &written, nullptr) && written == line.size();
        return good_;
    }
    [[nodiscard]] bool healthy() const noexcept {
        return good_ && file_ != INVALID_HANDLE_VALUE;
    }
private:
    HANDLE file_{INVALID_HANDLE_VALUE};
    std::uint64_t sequence_{};
    bool good_{true};
};

struct PumpContext final {
    Evidence* evidence{};
    explorer::ExplorerMvpSession* session{};
};

bool drain_gesture_events(Evidence& evidence, explorer::ExplorerMvpSession& session);

std::optional<std::wstring> read_line(
    Evidence& evidence, console_input::StaConsoleLineReader& reader,
    std::string_view kind, explorer::ExplorerMvpSession* session = nullptr) {
    PumpContext context{&evidence, session};
    const auto pump = [](void* value) noexcept {
        try {
            const auto& context = *static_cast<PumpContext*>(value);
            if (!context.evidence->healthy()) return false;
            const bool pumped = context.session->pump();
            const bool recorded = drain_gesture_events(*context.evidence, *context.session);
            return pumped && recorded;
        } catch (...) {
            return false;
        }
    };
    const auto result = reader.read(session ?
        console_input::StaOwnerWork{&context, pump} :
        console_input::StaOwnerWork{});
    std::ostringstream fields;
    fields << ",\"kind\":" << quote(kind)
           << ",\"result\":"
           << quote(console_input::line_status_name(result.status))
           << ",\"owner_thread\":" << result.owner_thread
           << ",\"pump_calls\":" << result.pump_count
           << ",\"messages\":" << result.message_dispatch_count
           << ",\"console_events\":" << result.console_input_event_count
           << ",\"mode_changed\":"
           << (result.mode_changed ? "true" : "false")
           << ",\"error\":" << result.error;
    if (!evidence.record("console_wait", fields.str())) return std::nullopt;
    if (session && result.line) {
        const bool pumped = session->pump();
        const bool recorded = drain_gesture_events(evidence, *session);
        if (!pumped || !recorded) return std::nullopt;
    }
    return result.line;
}

std::string rect_json(const panebind::core::geometry::Rect& rect) {
    return "[" + std::to_string(rect.left()) + "," +
        std::to_string(rect.top()) + "," +
        std::to_string(rect.right()) + "," +
        std::to_string(rect.bottom()) + "]";
}

std::string point_json(const panebind::core::geometry::Point& point) {
    return "[" + std::to_string(point.x) + "," +
        std::to_string(point.y) + "]";
}

std::string shield_windowpos_sample_json(
    const operations::GestureShieldWindowPosSample& value) {
    return "{\"window\":" +
        std::to_string(reinterpret_cast<std::uintptr_t>(value.window)) +
        ",\"insert_after\":" +
        std::to_string(reinterpret_cast<std::intptr_t>(value.insert_after)) +
        ",\"x\":" + std::to_string(value.x) +
        ",\"y\":" + std::to_string(value.y) +
        ",\"width\":" + std::to_string(value.width) +
        ",\"height\":" + std::to_string(value.height) +
        ",\"flags\":" + std::to_string(value.flags) + "}";
}
std::string shield_windowpos_json(
    const operations::GestureShieldWindowPosFacts& value) {
    return "{\"count\":" + std::to_string(value.count) +
        ",\"first\":" + shield_windowpos_sample_json(value.first) +
        ",\"last\":" + shield_windowpos_sample_json(value.last) + "}";
}
std::string shield_placement_json(
    const operations::GestureShieldPlacementFacts& value) {
    return "{\"attempted\":" + std::string(value.attempted ? "true" : "false") +
        ",\"window\":" +
        std::to_string(reinterpret_cast<std::uintptr_t>(value.window)) +
        ",\"insert_after\":" +
        std::to_string(reinterpret_cast<std::intptr_t>(value.insert_after)) +
        ",\"x\":" + std::to_string(value.x) +
        ",\"y\":" + std::to_string(value.y) +
        ",\"width\":" + std::to_string(value.width) +
        ",\"height\":" + std::to_string(value.height) +
        ",\"flags\":" + std::to_string(value.flags) +
        ",\"succeeded\":" + (value.succeeded ? "true" : "false") +
        ",\"win32_error\":" + std::to_string(value.win32_error) +
        ",\"after_exstyle\":" + std::to_string(value.after_exstyle) +
        ",\"changing_count_before\":" +
        std::to_string(value.changing_count_before) +
        ",\"changing_count_after\":" +
        std::to_string(value.changing_count_after) +
        ",\"changed_count_before\":" +
        std::to_string(value.changed_count_before) +
        ",\"changed_count_after\":" +
        std::to_string(value.changed_count_after) + "}";
}

constexpr std::string_view gesture_event_name(explorer::MvpEvidenceKind kind) noexcept {
    switch (kind) {
    case explorer::MvpEvidenceKind::Down: return "down";
    case explorer::MvpEvidenceKind::Start: return "start";
    case explorer::MvpEvidenceKind::IsolationReady: return "isolation_ready";
    case explorer::MvpEvidenceKind::Cancel: return "cancel";
    case explorer::MvpEvidenceKind::NativeEnd: return "native_end";
    case explorer::MvpEvidenceKind::Handoff: return "handoff";
    case explorer::MvpEvidenceKind::Writer: return "writer";
    case explorer::MvpEvidenceKind::RawUp: return "raw_up";
    case explorer::MvpEvidenceKind::LegacyUp: return "legacy_up";
    case explorer::MvpEvidenceKind::IsolationGone: return "isolation_gone";
    case explorer::MvpEvidenceKind::Rejected: return "rejected";
    case explorer::MvpEvidenceKind::Resource: return "resource";
    }
    return "unknown";
}

constexpr std::string_view gesture_route_name(explorer::MvpGestureRoute route) noexcept {
    switch (route) {
    case explorer::MvpGestureRoute::Reject: return "reject";
    case explorer::MvpGestureRoute::PlainMoveCandidate: return "plain_move_candidate";
    case explorer::MvpGestureRoute::CtrlMove: return "ctrl_move";
    case explorer::MvpGestureRoute::NativeResize: return "native_resize";
    }
    return "unknown";
}

bool drain_gesture_events(Evidence& evidence, explorer::ExplorerMvpSession& session) {
    // Only this owner STA formats or writes evidence. The Raw/shield receiver
    // contributes bounded facts to the session, never synchronous disk I/O.
    const auto events = session.drain_evidence_events();
    for (const auto& event : events) {
        std::ostringstream fields;
        fields << ",\"event\":" << quote(gesture_event_name(event.kind));
        if (event.generation) fields << ",\"generation\":" << event.generation;
        if (event.source_member < 3)
            fields << ",\"source_member\":" << event.source_member;
        if (event.raw_sequence)
            fields << ",\"raw_sequence\":" << event.raw_sequence;
        if (event.native_sequence)
            fields << ",\"native_sequence\":" << event.native_sequence;
        if (event.quantum) fields << ",\"quantum\":" << event.quantum;
        if (event.route)
            fields << ",\"route\":" << quote(gesture_route_name(*event.route));
        if (event.reason != "none")
            fields << ",\"reason\":" << quote(event.reason);
        if (event.cursor)
            fields << ",\"cursor\":" << point_json(*event.cursor);
        if (event.raw_observed_cursor)
            fields << ",\"raw_observed_cursor\":" << point_json(*event.raw_observed_cursor);
        if (event.initial_visible)
            fields << ",\"initial_visible\":" << rect_json(*event.initial_visible);
        if (event.target_visible)
            fields << ",\"target_visible\":" << rect_json(*event.target_visible);
        if (event.actual_visible)
            fields << ",\"actual_visible\":" << rect_json(*event.actual_visible);
        if (event.actual_positioning)
            fields << ",\"actual_positioning\":" << rect_json(*event.actual_positioning);
        const auto optional_bool = [&](std::string_view name,
                                       const std::optional<bool>& value) {
            if (value)
                fields << ",\"" << name << "\":" << (*value ? "true" : "false");
        };
        // A cancellation API call, a handoff verdict, and a source placement
        // are different facts. Never label permission/ready as native apply.
        const auto attempted_name = event.kind == explorer::MvpEvidenceKind::Writer ?
            "native_attempted" : event.kind == explorer::MvpEvidenceKind::Cancel ?
            "cancel_api_attempted" : "attempted";
        const auto succeeded_name = event.kind == explorer::MvpEvidenceKind::Writer ?
            "native_succeeded" : event.kind == explorer::MvpEvidenceKind::Cancel ?
            "cancel_api_succeeded" : "succeeded";
        optional_bool(attempted_name, event.attempted);
        optional_bool(succeeded_name, event.succeeded);
        optional_bool("outcome_known", event.outcome_known);
        optional_bool("snapped", event.snapped);
        optional_bool("geometry_exact", event.geometry_exact);
        optional_bool("post_context_exact", event.post_context_exact);
        optional_bool("overlay_destroyed", event.overlay_destroyed);
        optional_bool("hotkey_unregistered", event.hotkey_unregistered);
        optional_bool("receiver_destroyed", event.receiver_destroyed);
        optional_bool("raw_registration_removed", event.raw_registration_removed);
        optional_bool("winevent_unhooked", event.winevent_unhooked);
        optional_bool("classes_unregistered", event.classes_unregistered);
        optional_bool("association_detached", event.association_detached);
        if (event.win32_error)
            fields << ",\"win32_error\":" << *event.win32_error;
        if (event.isolation_timeout_ms)
            fields << ",\"isolation_timeout_ms\":" << *event.isolation_timeout_ms;
        if (event.shield_setup_stage)
            fields << ",\"shield_setup_stage\":" << *event.shield_setup_stage;
        if (event.shield_native_failure)
            fields << ",\"shield_native_failure\":" << *event.shield_native_failure;
        if (event.shield_readback_failure)
            fields << ",\"shield_readback_failure\":" << *event.shield_readback_failure;
        if (event.shield_observed_exstyle)
            fields << ",\"shield_observed_exstyle\":" << *event.shield_observed_exstyle;
        if (event.shield_initial_exstyle)
            fields << ",\"shield_initial_exstyle\":" << *event.shield_initial_exstyle;
        if (event.shield_created_exstyle)
            fields << ",\"shield_created_exstyle\":" << *event.shield_created_exstyle;
        if (event.shield_route_message)
            fields << ",\"shield_route_message\":" << *event.shield_route_message;
        if (event.shield_route_wparam)
            fields << ",\"shield_route_wparam\":" << *event.shield_route_wparam;
        if (event.shield_route_capture)
            fields << ",\"shield_route_capture\":" << *event.shield_route_capture;
        if (event.native_attempt_qpc) fields << ",\"native_attempt_qpc\":" << *event.native_attempt_qpc;
        if (event.raw_up_qpc) fields << ",\"raw_up_qpc\":" << *event.raw_up_qpc;
        if (event.shield_capture) {
            const auto& c = *event.shield_capture;
            const auto handle = [](HWND h) { return reinterpret_cast<std::uintptr_t>(h); };
            fields << ",\"shield_capture\":{\"owner_thread_id\":" << c.owner_thread_id
                << ",\"attempted\":" << (c.attempted ? "true" : "false")
                << ",\"previous\":" << handle(c.previous)
                << ",\"actual_after\":" << handle(c.actual_after)
                << ",\"foreground_before\":" << handle(c.foreground_before)
                << ",\"foreground_after\":" << handle(c.foreground_after)
                << ",\"failure\":" << static_cast<unsigned>(c.failure)
                << ",\"release_attempted\":" << (c.release_attempted ? "true" : "false")
                << ",\"release_completed\":" << (c.release_completed ? "true" : "false")
                << ",\"release_succeeded\":" << (c.release_completed ?
                    (c.release_succeeded ? "true" : "false") : "null")
                << ",\"release_before_qpc\":" << c.release_before_qpc
                << ",\"release_after_qpc\":" << c.release_after_qpc
                << ",\"release_before\":" << handle(c.release_before)
                << ",\"release_after\":" << handle(c.release_after)
                << ",\"release_error\":" << (c.release_completed ? std::to_string(c.release_error) : "null")
                << ",\"capture_changed_to\":" << handle(c.capture_changed_to)
                << ",\"own_release_message\":" << (c.own_release_message ? "true" : "false");
            const auto gui = [&](const char* name, const auto& f) {
                fields << ",\"" << name << "\":{\"thread_id\":" << f.thread_id
                    << ",\"available\":" << (f.available ? "true" : "false")
                    << ",\"capture\":" << handle(f.capture)
                    << ",\"move_size\":" << handle(f.move_size)
                    << ",\"menu_owner\":" << handle(f.menu_owner)
                    << ",\"flags\":" << f.flags
                    << ",\"active\":" << handle(f.active)
                    << ",\"focus\":" << handle(f.focus) << '}';
            };
            gui("source_before", c.source_before);
            gui("foreground_before_gui", c.foreground_before_gui);
            gui("owner_before", c.owner_before);
            gui("source_after", c.source_after);
            gui("foreground_after_gui", c.foreground_after_gui);
            gui("owner_after", c.owner_after);
            const auto& a = c.association;
            fields << ",\"association\":{\"generation\":" << a.generation
                << ",\"owner_thread_id\":" << a.owner_thread_id
                << ",\"source_thread_id\":" << a.source_thread_id
                << ",\"source_process_id\":" << a.source_process_id
                << ",\"source\":" << handle(a.source)
                << ",\"desktop_verified\":" << (a.desktop_verified ? "true" : "false")
                << ",\"owner_session_id\":" << a.owner_session_id
                << ",\"source_session_id\":" << a.source_session_id
                << ",\"owner_desktop_query\":" << (a.owner_desktop_query ? "true" : "false")
                << ",\"source_desktop_query\":" << (a.source_desktop_query ? "true" : "false")
                << ",\"input_desktop_query\":" << (a.input_desktop_query ? "true" : "false")
                << ",\"owner_desktop_input\":" << (a.owner_desktop_input ? "true" : "false")
                << ",\"source_desktop_input\":" << (a.source_desktop_input ? "true" : "false")
                << ",\"input_desktop_active\":" << (a.input_desktop_active ? "true" : "false")
                << ",\"attach_attempted\":" << (a.attach_attempted ? "true" : "false")
                << ",\"attach_completed\":" << (a.attach_completed ? "true" : "false")
                << ",\"attach_succeeded\":" << (a.attach_completed ?
                    (a.attach_succeeded ? "true" : "false") : "null")
                << ",\"attach_error\":" << (a.attach_completed ? std::to_string(a.attach_error) : "null")
                << ",\"attach_before_qpc\":" << a.attach_before_qpc
                << ",\"attach_after_qpc\":" << a.attach_after_qpc
                << ",\"foreground_before\":" << handle(a.foreground_before)
                << ",\"foreground_after\":" << handle(a.foreground_after)
                << ",\"detach_attempted\":" << (a.detach_attempted ? "true" : "false")
                << ",\"detach_completed\":" << (a.detach_completed ? "true" : "false")
                << ",\"detach_succeeded\":" << (a.detach_completed ?
                    (a.detach_succeeded ? "true" : "false") : "null")
                << ",\"detach_error\":" << (a.detach_completed ? std::to_string(a.detach_error) : "null")
                << ",\"detach_before_qpc\":" << a.detach_before_qpc
                << ",\"detach_after_qpc\":" << a.detach_after_qpc
                << ",\"foreground_before_detach\":" << handle(a.foreground_before_detach)
                << ",\"foreground_after_detach\":" << handle(a.foreground_after_detach);
            gui("owner_before", a.owner_before);
            gui("source_before", a.source_before);
            gui("owner_after", a.owner_after);
            gui("source_after", a.source_after);
            gui("owner_before_detach", a.owner_before_detach);
            gui("source_before_detach", a.source_before_detach);
            gui("owner_after_detach", a.owner_after_detach);
            gui("source_after_detach", a.source_after_detach);
            fields << '}';
            fields << '}';
        }
        if (event.shield_initial_placement)
            fields << ",\"shield_initial_placement\":" <<
                shield_placement_json(*event.shield_initial_placement);
        if (event.shield_retry_placement)
            fields << ",\"shield_retry_placement\":" <<
                shield_placement_json(*event.shield_retry_placement);
        if (event.shield_windowpos_changing)
            fields << ",\"shield_windowpos_changing\":" <<
                shield_windowpos_json(*event.shield_windowpos_changing);
        if (event.shield_windowpos_changed)
            fields << ",\"shield_windowpos_changed\":" <<
                shield_windowpos_json(*event.shield_windowpos_changed);
        optional_bool("shield_topmost_retry_attempted", event.shield_topmost_retry_attempted);
        if (event.overlay)
            fields << ",\"overlay\":" << event.overlay;
        if (!evidence.record("gesture_event", fields.str())) return false;
    }
    return true;
}

bool show_status(Evidence& evidence, explorer::ExplorerMvpSession& session) {
    if (!drain_gesture_events(evidence, session)) return false;
    const auto status = session.status();
    if (!drain_gesture_events(evidence, session)) return false;
    if (!status) {
        evidence.record("status_unavailable", ",\"reason\":" +
            quote(session.reason()));
        return print(L"状态暂不可获取。请检查日志，必要时输入 Q 停止。\r\n");
    }
    std::wstring display = L"\r\n三窗状态（实际几何）：\r\n";
    std::ostringstream fields;
    fields << ",\"gesture_active\":"
           << (status->gesture_active ? "true" : "false")
           << ",\"glue_active\":"
           << (status->glue_active ? "true" : "false")
           << ",\"relation_count\":" << status->topology.relation_count
           << ",\"topology_ready\":"
           << (status->topology.ready ? "true" : "false")
           << ",\"windows\":[";
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& snapshot = status->snapshots[i];
        const auto& rect = snapshot.visible_rect;
        display += std::wstring(1, static_cast<wchar_t>(L'A' + i)) +
            L": [" + std::to_wstring(rect.left()) + L", " +
            std::to_wstring(rect.top()) + L", " +
            std::to_wstring(rect.right()) + L", " +
            std::to_wstring(rect.bottom()) + L"]\r\n";
        if (i != 0) fields << ',';
        fields << "{\"member\":" << i << ",\"visible\":"
               << rect_json(rect) << ",\"positioning\":"
               << rect_json(snapshot.positioning_rect) << "}";
    }
    fields << "]";
    display += L"关系数=" + std::to_wstring(status->topology.relation_count) +
        (status->topology.ready ? L"，Ctrl Glue 布局可用" :
                                  L"，Ctrl Glue 布局暂不可用") +
        L"。普通移动和原生调整大小仍可继续。\r\n";
    return evidence.record("status", fields.str()) && print(display);
}

int run(Evidence& evidence, const std::wstring& run_id,
        const std::wstring& evidence_path, bool automated_guest_driver) {
    if (!evidence.record("startup", ",\"implementation_sha\":" +
            quote(PANEBIND_BUILD_SHA) +
            ",\"run_id\":" + quote(run_id) +
            ",\"owner_thread\":" + std::to_string(GetCurrentThreadId()) +
            ",\"member_count\":3,\"live_validation\":false" +
            ",\"automated_guest_driver\":" +
            (automated_guest_driver ? "true" : "false"))) return 2;
    const bool dpi_ready =
        SetProcessDpiAwarenessContext(
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) ||
        AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!dpi_ready) {
        evidence.record("shutdown", ",\"result\":\"BLOCKED\",\"reason\":\"dpi_context_unavailable\"");
        return 2;
    }

    console_input::StaConsoleLineReader reader{
        GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE)};
    std::unique_ptr<explorer::ExplorerMvpSession> session;
    const auto stop = [&](std::string_view reason) {
        const std::string saved_reason(reason);
        bool recorded_events = !session || drain_gesture_events(evidence, *session);
        bool stopped = true;
        if (session) stopped = session->stop();
        if (session) recorded_events = drain_gesture_events(evidence, *session) && recorded_events;
        evidence.record("shutdown", ",\"result\":\"BLOCKED\",\"reason\":" +
            quote(saved_reason) + ",\"resources_stopped\":" +
            (stopped ? "true" : "false") +
            ",\"gesture_events_recorded\":" +
            (recorded_events ? "true" : "false"));
        return 2;
    };
    const auto requested_stop = [&](std::string_view source) {
        bool recorded_events = session && drain_gesture_events(evidence, *session);
        const bool stopped = session && session->stop();
        if (session) recorded_events = drain_gesture_events(evidence, *session) && recorded_events;
        const bool complete = stopped && recorded_events;
        const bool recorded = evidence.record("shutdown", ",\"result\":" +
            std::string(complete ? "\"STOPPED\"" : "\"BLOCKED\"") +
            ",\"source\":" + quote(source) +
            ",\"resources_stopped\":" +
            (stopped ? "true" : "false") +
            ",\"gesture_events_recorded\":" +
            (recorded_events ? "true" : "false") +
            ",\"user_windows_closed\":false");
        print(complete ? L"PaneBind 已停止；未关闭 Explorer 窗口。\r\n" :
                        L"停止未完整确认；请保留日志。\r\n");
        return complete && recorded ? 0 : 2;
    };
    explorer::ExplorerGroupSession::OwnedMembers members;
    for (std::size_t i = 0; i < members.size(); ++i) {
        GUID guid{};
        wchar_t nonce[64]{};
        if (CoCreateGuid(&guid) != S_OK || !StringFromGUID2(guid, nonce,
                                               static_cast<int>(std::size(nonce))))
            return stop("nonce_failed");
        const auto path = std::filesystem::temp_directory_path() /
            (std::wstring(L"PaneBind-MVP1-") +
             static_cast<wchar_t>(L'A' + i) + L"-" + nonce);
        // A pre-existing path is never adopted or overwritten.
        if (!std::filesystem::create_directory(path))
            return stop("nonce_directory_not_new");
        auto begin = explorer::ExplorerConsentProvisioning::begin(path);
        if (!begin.succeeded()) return stop("baseline_failed");
        const auto prompt = begin.provisioning->record_target_prompt();
        if (!prompt.succeeded()) return stop("prompt_record_failed");
        if (!evidence.record("target_prompt", ",\"member\":" +
                std::to_string(i) + ",\"directory\":" +
                quote(path.native()) + ",\"baseline_generation\":" +
                std::to_string(begin.facts.generations.baseline_generation) +
                ",\"prompt_generation\":" +
                std::to_string(prompt.generation))) return stop("evidence_write_failed");
        const auto label = std::wstring(1, static_cast<wchar_t>(L'A' + i));
        if (!print(L"\r\n窗口 " + label +
            L"：请新建一扇 Explorer 窗口，不要复用已有窗口。打开此空目录：\r\n" +
            path.native() +
            L"\r\n完成后回到控制台按 Enter；输入其他内容将取消。\r\n"))
            return stop("console_failed");
        const auto answer = read_line(evidence, reader, "target_confirmation");
        if (!answer || !answer->empty()) return stop("target_declined");
        auto confirmed = begin.provisioning->confirm_user_target();
        if (!confirmed.succeeded()) return stop("target_confirmation_failed");
        const auto& facts = confirmed.facts;
        if (!facts.baseline_exclusion_complete || !facts.unique_new_target ||
            !facts.exact_target_location || !facts.token_issued)
            return stop("target_facts_incomplete");
        if (!evidence.record("target_confirmed", ",\"member\":" +
                std::to_string(i) + ",\"confirmation_generation\":" +
                std::to_string(facts.generations.target_confirmation_generation) +
                ",\"eligibility_generation\":" +
                std::to_string(facts.generations.eligibility_generation) +
                ",\"token_generation\":" +
                std::to_string(facts.generations.token_generation) +
                ",\"baseline_exclusion_complete\":true,\"unique_new_target\":true,\"exact_location\":true" +
                ",\"input_source\":" + quote(automated_guest_driver ?
                    "automated_guest_driver" : "interactive_console")))
            return stop("evidence_write_failed");
        members[i] = std::move(confirmed.session);
    }

    if (!print(L"\r\n三扇新建 Explorer 窗口均已分别确认。\r\n"
               L"本候选只控制这三扇窗口：普通标题栏移动使用磁吸，"
               L"Ctrl + 移动使用既有 Glue，调整大小保持原生行为。\r\n"
               L"普通移动期间可能短暂启用覆盖全虚拟屏幕的输入隔离；"
               L"不会移动其他窗口。隔离最长 30 秒，可按 Ctrl+Shift+F11 "
               L"或回到控制台输入 Q 停止。\r\n"
               L"该实现尚待隔离环境现场验证。输入 Y 并按 Enter 明确同意启动；"
               L"其他输入将取消。\r\n")) return stop("console_failed");
    const auto consent = read_line(evidence, reader, "product_consent");
    if (!consent || (*consent != L"Y" && *consent != L"y"))
        return stop("product_consent_declined");
    if (!evidence.record("product_consent",
                         ",\"confirmed\":true,\"input_source\":" +
                         quote(automated_guest_driver ? "automated_guest_driver" :
                               "interactive_console")))
        return stop("evidence_write_failed");

    session = explorer::ExplorerMvpSession::create_after_explicit_consent(
        std::move(members), [run_id, evidence_path] {
            return sandbox_run_authorized(run_id, evidence_path);
        });
    if (!session) return stop("product_session_create_failed");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& binding = session->bindings()[i];
        std::ostringstream fields;
        fields << ",\"member\":" << i
               << ",\"window_id\":" << binding.window_id
               << ",\"capability_generation\":"
               << binding.capability_generation
               << ",\"consent_generation\":" << binding.consent_generation
               << ",\"hwnd\":"
               << reinterpret_cast<std::uintptr_t>(binding.window)
               << ",\"pid\":" << binding.process_id
               << ",\"tid\":" << binding.thread_id;
        if (!evidence.record("binding", fields.str()))
            return stop("evidence_write_failed");
    }
    if (!session->healthy()) return stop(session->reason());
    if (!print(L"\r\n单入口已启动。A/B/C 均可作为普通移动的源窗口；"
               L"先按住 Ctrl 再拖动标题栏可使用 Glue。调整大小保持原生行为。\r\n"
               L"输入 S 查看实际状态，输入 Q 或按 Ctrl+Shift+F11 停止。"
               L"控制台等待期间仍处理窗口事件。\r\n"))
        return stop("console_failed");
    if (!show_status(evidence, *session)) return stop("status_failed");

    for (;;) {
        const auto command = read_line(evidence, reader, "command", session.get());
        if (!command) {
            // The resource-thread hotkey is an observed, deliberate stop, not
            // a product failure. Still require actual resource cleanup.
            if (evidence.healthy() && session->reason() == "hotkey_stop")
                return requested_stop("keyboard_hotkey");
            return stop(session->healthy() ?
                        "console_wait_failed" : session->reason());
        }
        if (!session->healthy()) return stop(session->reason());
        if (*command == L"Q" || *command == L"q" ||
            *command == L"STOP" || *command == L"stop")
            return requested_stop("console_command");
        if (*command == L"S" || *command == L"s" || command->empty()) {
            if (!show_status(evidence, *session)) return stop("status_failed");
            continue;
        }
        if (!print(L"命令：S 查看状态；Q 停止。\r\n"))
            return stop("console_failed");
    }
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    // Read-only identity inspection is safe on the host; it creates no GUI,
    // input registration, evidence file, or Explorer session.
    if (argc == 2 && std::wstring_view(argv[1]) == L"--build-identity") {
        std::cout << "{\"implementation_sha\":" << quote(PANEBIND_BUILD_SHA)
                  << ",\"interactive_validation\":false}\n";
        return 0;
    }
    const bool automated_guest_driver = argc == 6 &&
        std::wstring_view(argv[5]) == L"--guest-automated-driver";
    if ((argc != 5 && !automated_guest_driver) ||
        std::wstring_view(argv[1]) != L"--sandbox-run-id" ||
        std::wstring_view(argv[3]) != L"--evidence-log") return 64;
    const std::wstring run_id(argv[2]), evidence_path(argv[4]);
    if (!sandbox_run_authorized(run_id, evidence_path)) {
        constexpr wchar_t warning[] =
            L"尚未就绪：需要本轮精确匹配的交互式 Windows Sandbox 包；"
            L"未创建窗口或输入资源。\r\n";
        DWORD written{};
        const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
        if (!WriteConsoleW(error, warning,
                           static_cast<DWORD>(std::size(warning) - 1),
                           &written, nullptr)) {
            constexpr char fallback[] =
                "NOT_READY: exact interactive Windows Sandbox package required; "
                "no GUI or input was started.\n";
            WriteFile(error, fallback, static_cast<DWORD>(sizeof(fallback) - 1),
                      &written, nullptr);
        }
        return 78;
    }
    if (!console_handle(GetStdHandle(STD_INPUT_HANDLE)) ||
        !console_handle(GetStdHandle(STD_OUTPUT_HANDLE))) return 64;
    try {
        Evidence evidence(evidence_path.c_str());
        if (!evidence.healthy()) return 65;
        return run(evidence, run_id, evidence_path, automated_guest_driver);
    } catch (const std::exception& error) {
        print(L"Explorer MVP1 候选程序运行失败；请保留不完整证据。\r\n");
        std::cerr << error.what() << '\n';
        return 2;
    }
}
