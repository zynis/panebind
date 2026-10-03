#pragma once

#include <windows.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace panebind::platform::windows::operations {

// This owns input-observation/isolation resources, not gesture attribution or
// authority to cancel or move a foreign window. A queued request is never an
// isolation-ready fact; callers must wait for the observed IsolationReady event.
struct GestureShieldRequest {
    std::uint64_t generation{};
    HWND source{};
    DWORD process_id{};
    DWORD thread_id{};
    // Internal per-gesture bounded escape deadline, not a user performance SLA.
    // The default and maximum remain 30 s; no caller may disable or extend it.
    std::uint32_t isolation_timeout_ms{30000};
};

[[nodiscard]] constexpr bool gesture_shield_timeout_valid(
    std::uint32_t timeout_ms) noexcept {
    return timeout_ms >= 1 && timeout_ms <= 30000;
}

enum class GestureShieldEventKind {
    RawMouse,
    LegacyLeftUp,
    IsolationReady,
    IsolationGone,
    HotkeyStop,
    Deadline,
    ContextLost,
    ResourceFailure,
    OverlayMouseRoute,
    CaptureReady,
    CaptureFailure,
    CaptureLost,
    CaptureReleased,
    ThreadAssociated,
    ThreadDetached,
    ThreadAssociationFailure,
    ThreadAssociationAttempt,
    CaptureReleaseAttempt,
    // Preparation only: no AttachThreadInput call has entered yet. The
    // callback may revoke this generation before the final native-entry check.
    ThreadAssociationPrepared,
};

enum class GestureShieldRemovalReason {
    NormalUp,
    ExplicitStop,
    ContextLost,
    SetupFailure,
    Deadline,
    Shutdown,
};

// Setup diagnostics are observations, never authority to cancel or write.
// None in a ResourceFailure means the failure was outside overlay setup.
enum class GestureShieldSetupStage {
    None,
    VirtualScreen,
    PreLive,
    Authorization,
    PostLive,
    CreateWindow,
    SetAlpha,
    PlaceWindow,
    Readback,
    RegisterHotkey,
    Deadline,
};

enum class GestureShieldNativeFailure {
    None,
    SourceIdentity,
    InputDesktop,
    Foreground,
    PhysicalButtons,
    GuiQuery,
    Capture,
    MoveSize,
    MenuMode,
};

enum class GestureShieldReadbackFailure {
    None,
    OverlayIdentity,
    Rectangle,
    Style,
    Alpha,
    Visibility,
    CursorHit,
    MonitorHit,
    Foreground,
    NativeLive,
};

// Bounded as-received WINDOWPOS facts for this self-owned overlay only. The
// window procedure still forwards both messages to DefWindowProc unchanged.
struct GestureShieldWindowPosSample {
    HWND window{};
    HWND insert_after{};
    int x{};
    int y{};
    int width{};
    int height{};
    UINT flags{};
};

struct GestureShieldWindowPosFacts {
    std::uint32_t count{};
    GestureShieldWindowPosSample first{};
    GestureShieldWindowPosSample last{};
};

struct GestureShieldPlacementFacts {
    bool attempted{};
    HWND window{};
    HWND insert_after{};
    int x{};
    int y{};
    int width{};
    int height{};
    UINT flags{};
    bool succeeded{};
    DWORD win32_error{};
    std::uint32_t after_exstyle{};
    std::uint32_t changing_count_before{};
    std::uint32_t changing_count_after{};
    std::uint32_t changed_count_before{};
    std::uint32_t changed_count_after{};
};

// These are observations of named threads, not a whole-desktop capture query.
struct GestureShieldGuiFacts {
    DWORD thread_id{};
    bool available{};
    HWND capture{};
    HWND move_size{};
    HWND menu_owner{};
    DWORD flags{};
    HWND active{};
    HWND focus{};
};

// A single guest-authorized resource/source input-queue pair. Native BOOL
// results and named-thread snapshots are facts, not evidence of mouse UP.
struct GestureShieldAssociationFacts {
    std::uint64_t generation{};
    DWORD owner_thread_id{};
    DWORD source_thread_id{};
    DWORD source_process_id{};
    HWND source{};
    bool desktop_verified{};
    DWORD owner_session_id{};
    DWORD source_session_id{};
    bool owner_desktop_query{};
    bool source_desktop_query{};
    bool input_desktop_query{};
    bool owner_desktop_input{};
    bool source_desktop_input{};
    bool input_desktop_active{};
    bool attach_attempted{};
    bool attach_completed{};
    bool attach_succeeded{};
    DWORD attach_error{};
    std::uint64_t attach_before_qpc{};
    std::uint64_t attach_after_qpc{};
    HWND foreground_before{};
    HWND foreground_after{};
    GestureShieldGuiFacts owner_before{};
    GestureShieldGuiFacts source_before{};
    GestureShieldGuiFacts owner_after{};
    GestureShieldGuiFacts source_after{};
    bool detach_attempted{};
    bool detach_completed{};
    bool detach_succeeded{};
    DWORD detach_error{};
    std::uint64_t detach_before_qpc{};
    std::uint64_t detach_after_qpc{};
    HWND foreground_before_detach{};
    HWND foreground_after_detach{};
    GestureShieldGuiFacts owner_before_detach{};
    GestureShieldGuiFacts source_before_detach{};
    GestureShieldGuiFacts owner_after_detach{};
    GestureShieldGuiFacts source_after_detach{};
};

[[nodiscard]] constexpr bool gesture_shield_association_owned_pair(
    const GestureShieldAssociationFacts& facts, std::uint64_t generation) noexcept {
    return generation && facts.generation == generation && facts.attach_attempted &&
        facts.attach_completed && facts.attach_succeeded && facts.owner_thread_id && facts.source_thread_id &&
        facts.owner_thread_id != facts.source_thread_id;
}

[[nodiscard]] constexpr bool gesture_shield_association_cleanup_confirmed(
    const GestureShieldAssociationFacts& facts, std::uint64_t generation) noexcept {
    // No successful attach gives this gesture no pair to detach. A successful
    // pair is clean only with a native TRUE detach for that same generation.
    if (!facts.attach_succeeded)
        return (!facts.attach_attempted || facts.attach_completed) && !facts.detach_attempted;
    return gesture_shield_association_owned_pair(facts, generation) &&
        facts.detach_attempted && facts.detach_completed && facts.detach_succeeded;
}

enum class GestureShieldCaptureFailure {
    None,
    Revoked,
    Authorization,
    SourceIdentity,
    OverlayIdentity,
    InputDesktop,
    Foreground,
    PhysicalButtons,
    GuiQuery,
    ForeignCapture,
    MoveSize,
    MenuMode,
    Readback,
    AssociationAttach,
    AssociationDetach,
};

[[nodiscard]] constexpr GestureShieldCaptureFailure gesture_shield_native_entry_failure(
    bool revoked, bool exact_source, bool physical_buttons,
    bool foreground_matches) noexcept {
    if (revoked) return GestureShieldCaptureFailure::Revoked;
    if (!exact_source) return GestureShieldCaptureFailure::SourceIdentity;
    if (!physical_buttons) return GestureShieldCaptureFailure::PhysicalButtons;
    if (!foreground_matches) return GestureShieldCaptureFailure::Foreground;
    return GestureShieldCaptureFailure::None;
}

struct GestureShieldCaptureFacts {
    DWORD owner_thread_id{};
    bool attempted{};
    // SetCapture returns the previous capture HWND, not a success BOOL.
    HWND previous{};
    HWND actual_after{};
    HWND foreground_before{};
    HWND foreground_after{};
    GestureShieldGuiFacts source_before{};
    GestureShieldGuiFacts foreground_before_gui{};
    GestureShieldGuiFacts owner_before{};
    GestureShieldGuiFacts source_after{};
    GestureShieldGuiFacts foreground_after_gui{};
    GestureShieldGuiFacts owner_after{};
    GestureShieldCaptureFailure failure{GestureShieldCaptureFailure::None};
    bool release_attempted{};
    bool release_completed{};
    bool release_succeeded{};
    HWND release_before{};
    HWND release_after{};
    DWORD release_error{};
    std::uint64_t release_before_qpc{};
    std::uint64_t release_after_qpc{};
    HWND capture_changed_to{};
    bool own_release_message{};
    GestureShieldAssociationFacts association{};
};

// Read-only, precise same-process/same-thread capture check. Callers must also
// bind overlay and owner_thread_id to their current generation's CaptureReady.
[[nodiscard]] bool gesture_shield_exact_capture(HWND overlay,
                                                DWORD owner_thread_id) noexcept;

struct GestureShieldEvent {
    GestureShieldEventKind kind{};
    std::uint64_t generation{};
    // Actual request value; zero means no matching isolation request observed.
    std::uint32_t isolation_timeout_ms{};
    std::uint64_t raw_sequence{};
    // MSG.pt and MSG.time are observations from this receiver's queue, not
    // cross-process DOWN attribution or a claim about the target HWND.
    POINT message_point{};
    DWORD message_time{};
    // Queried while this receiver dispatches WM_INPUT. The hit test is of
    // MSG.pt at dispatch time, not proof of which process received a DOWN.
    HWND observed_foreground{};
    HWND message_point_root{};
    // Same receiver-dispatch snapshot of the observed foreground root. Both
    // rectangles remain UNKNOWN if their individual native queries fail.
    bool foreground_positioning_available{};
    RECT foreground_positioning{};
    bool foreground_visible_available{};
    RECT foreground_visible{};
    DWORD foreground_process_id{};
    DWORD foreground_thread_id{};
    bool foreground_gui_available{};
    HWND foreground_capture{};
    HWND foreground_move_size{};
    HWND foreground_menu_owner{};
    DWORD foreground_gui_flags{};
    POINT cursor_now{};
    bool cursor_available{};
    USHORT raw_button_flags{};
    LONG raw_delta_x{};
    LONG raw_delta_y{};
    HWND overlay{};
    // Bounded route diagnostics from this self-owned overlay's WndProc only.
    // GetCapture observes this resource thread, not arbitrary foreign capture.
    UINT route_message{};
    std::uintptr_t route_wparam{};
    HWND route_capture{};
    GestureShieldCaptureFacts capture{};
    DWORD win32_error{};
    GestureShieldSetupStage setup_stage{GestureShieldSetupStage::None};
    GestureShieldNativeFailure native_failure{GestureShieldNativeFailure::None};
    GestureShieldReadbackFailure readback_failure{GestureShieldReadbackFailure::None};
    // The first placement can report success while WS_EX_TOPMOST is absent.
    // These facts record one bounded retry, not proof of final isolation.
    std::uint32_t created_exstyle{};
    GestureShieldPlacementFacts initial_placement{};
    GestureShieldPlacementFacts retry_placement{};
    GestureShieldWindowPosFacts windowpos_changing{};
    GestureShieldWindowPosFacts windowpos_changed{};
    std::uint32_t initial_exstyle{};
    bool topmost_retry_attempted{};
    std::uint32_t observed_exstyle{};
    GestureShieldRemovalReason removal_reason{GestureShieldRemovalReason::Shutdown};
    bool overlay_destroyed{};
    bool hotkey_unregistered{};
};

struct GestureShieldCallbacks {
    // The entry point must implement its exact disposable-guest/run-marker/
    // interactive-session guard here. This generic resource has no --force path.
    std::function<bool()> environment_allowed;
    // Recheck the adapter's exact consent, DOWN/START and ordinary-Move facts
    // at isolation creation time. This callback executes on the resource
    // thread: inspect already-published facts only, not STA COM or placement.
    // The shield additionally checks Win32 identity, foreground, physical
    // button, native move loop and input desktop itself.
    std::function<bool(const GestureShieldRequest&)> gesture_authorized;
    // Invoked only on the shield's resource/message thread. It must be quick:
    // enqueue state changes/revoke writer, never perform COM or placement here.
    std::function<void(const GestureShieldEvent&)> on_event;
    // Published exact-generation real native END and fresh ordinary-Move
    // authority only. This is separate from the pre-cancel START authority.
    // Missing/false authorization can never be replaced by the queued request.
    std::function<bool(const GestureShieldRequest&)> capture_authorized;
};

struct GestureShieldStopFacts {
    bool worker_exited{};
    bool overlay_destroyed{};
    bool receiver_destroyed{};
    bool raw_registration_removed{};
    bool hotkey_unregistered{};
    bool winevent_unhooked{};
    bool classes_unregistered{};
    bool callback_delivery_ok{};
    bool capture_released{};
    bool association_detached{};

    [[nodiscard]] bool clean() const noexcept {
        return worker_exited && overlay_destroyed && receiver_destroyed &&
               raw_registration_removed && hotkey_unregistered &&
               winevent_unhooked && classes_unregistered && callback_delivery_ok &&
               capture_released && association_detached;
    }
};

class GestureInputShield {
public:
    explicit GestureInputShield(GestureShieldCallbacks callbacks);
    ~GestureInputShield();

    GestureInputShield(const GestureInputShield&) = delete;
    GestureInputShield& operator=(const GestureInputShield&) = delete;

    // Starts a dedicated message thread and waits at most 2 s for the sole
    // process-local mouse Raw Input receiver to register and read back.
    [[nodiscard]] bool start() noexcept;
    // These methods are cross-thread and nonblocking. The on_event facts, not
    // their return values, establish overlay readiness or actual removal.
    [[nodiscard]] bool request_isolation(GestureShieldRequest request) noexcept;
    // Guest-only adapters queue this after their observed real native END.
    // CaptureReady, not the queue result or SetCapture return, gates writing.
    [[nodiscard]] bool request_capture_after_native_end(
        GestureShieldRequest request) noexcept;
    [[nodiscard]] bool request_remove(std::uint64_t generation,
                                      GestureShieldRemovalReason reason) noexcept;
    // Stops resources independently of the geometry writer. A false clean()
    // is a real cleanup failure and must not be reported as product PASS.
    [[nodiscard]] GestureShieldStopFacts stop(DWORD timeout_ms = 7000) noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    std::thread worker_;
};

} // namespace panebind::platform::windows::operations
