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
};

enum class GestureShieldEventKind {
    RawMouse,
    LegacyLeftUp,
    IsolationReady,
    IsolationGone,
    HotkeyStop,
    Deadline,
    ContextLost,
    ResourceFailure,
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

struct GestureShieldEvent {
    GestureShieldEventKind kind{};
    std::uint64_t generation{};
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
    DWORD foreground_gui_flags{};
    POINT cursor_now{};
    bool cursor_available{};
    USHORT raw_button_flags{};
    LONG raw_delta_x{};
    LONG raw_delta_y{};
    HWND overlay{};
    DWORD win32_error{};
    GestureShieldSetupStage setup_stage{GestureShieldSetupStage::None};
    GestureShieldNativeFailure native_failure{GestureShieldNativeFailure::None};
    GestureShieldReadbackFailure readback_failure{GestureShieldReadbackFailure::None};
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

    [[nodiscard]] bool clean() const noexcept {
        return worker_exited && overlay_destroyed && receiver_destroyed &&
               raw_registration_removed && hotkey_unregistered &&
               winevent_unhooked && classes_unregistered && callback_delivery_ok;
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
