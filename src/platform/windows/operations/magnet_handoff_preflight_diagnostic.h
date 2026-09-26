#pragma once

// Test-only facts and classifier. This header grants no native authority.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace panebind::test::handoff {
enum class Failure {
    None, MissingNativeEnd, MissingWinEventEnd, IdentityChanged,
    DesktopUnavailable, SourceNotVisible, ForegroundChanged, GuiQueryFailed,
    CaptureStillOwned, MenuStillActive, MoveSizeStillActive, CursorUnavailable,
    CursorOutsideOwnedInputGuard, ButtonReleasedBeforeHandoff, InputInterference, RawUpAlreadyObserved,
    ReceiverUnhealthy, ForeignCaptureTransferred, DpiChanged, MonitorChanged,
    ActualPositioningUnavailable, ActualVisibleUnavailable,
    TerminalPositioningUnavailable, TerminalVisibleUnavailable,
    BothTerminalGeometryMismatch, TerminalPositioningMismatch, TerminalVisibleMismatch,
    NativeDragAfterCancelReturn, NativeDragAfterEnd, UnownedGeometryChange,
    TakeoverAlreadyUnhealthy
};
constexpr std::string_view name(Failure value) noexcept {
    switch (value) {
#define PANEBIND_HANDOFF_NAME(x) case Failure::x: return #x
    PANEBIND_HANDOFF_NAME(None); PANEBIND_HANDOFF_NAME(MissingNativeEnd);
    PANEBIND_HANDOFF_NAME(MissingWinEventEnd); PANEBIND_HANDOFF_NAME(IdentityChanged);
    PANEBIND_HANDOFF_NAME(DesktopUnavailable); PANEBIND_HANDOFF_NAME(SourceNotVisible);
    PANEBIND_HANDOFF_NAME(ForegroundChanged); PANEBIND_HANDOFF_NAME(GuiQueryFailed);
    PANEBIND_HANDOFF_NAME(CaptureStillOwned); PANEBIND_HANDOFF_NAME(MenuStillActive);
    PANEBIND_HANDOFF_NAME(MoveSizeStillActive); PANEBIND_HANDOFF_NAME(CursorUnavailable);
    PANEBIND_HANDOFF_NAME(CursorOutsideOwnedInputGuard); PANEBIND_HANDOFF_NAME(ButtonReleasedBeforeHandoff);
    PANEBIND_HANDOFF_NAME(InputInterference);
    PANEBIND_HANDOFF_NAME(RawUpAlreadyObserved); PANEBIND_HANDOFF_NAME(ReceiverUnhealthy);
    PANEBIND_HANDOFF_NAME(ForeignCaptureTransferred); PANEBIND_HANDOFF_NAME(DpiChanged);
    PANEBIND_HANDOFF_NAME(MonitorChanged); PANEBIND_HANDOFF_NAME(ActualPositioningUnavailable);
    PANEBIND_HANDOFF_NAME(ActualVisibleUnavailable); PANEBIND_HANDOFF_NAME(TerminalPositioningUnavailable);
    PANEBIND_HANDOFF_NAME(TerminalVisibleUnavailable); PANEBIND_HANDOFF_NAME(BothTerminalGeometryMismatch);
    PANEBIND_HANDOFF_NAME(TerminalPositioningMismatch); PANEBIND_HANDOFF_NAME(TerminalVisibleMismatch);
    PANEBIND_HANDOFF_NAME(NativeDragAfterCancelReturn); PANEBIND_HANDOFF_NAME(NativeDragAfterEnd);
    PANEBIND_HANDOFF_NAME(UnownedGeometryChange); PANEBIND_HANDOFF_NAME(TakeoverAlreadyUnhealthy);
#undef PANEBIND_HANDOFF_NAME
    }
    return "Unknown";
}
struct Failures {
    std::array<Failure, 32> values{};
    std::size_t count{};
    constexpr void add(bool failed, Failure value) noexcept {
        if (failed) values[count++] = value;
    }
    constexpr Failure first() const noexcept { return count ? values[0] : Failure::None; }
};
struct HandoffPreflightDiagnostic {
    bool end_observed{}, winevent_end_observed{};
    std::uint64_t end_sequence{}, winevent_end_sequence{};
    std::int64_t end_qpc{}, winevent_end_qpc{};
    bool own_identity{}, desktop_ready{}, source_visible{}, foreground_matches{};
    bool foreground_snapshot_stable{};
    std::uint32_t actual_source_pid{}, actual_source_tid{};
    std::uintptr_t foreground_hwnd{};
    std::uint32_t foreground_pid{}, foreground_tid{};
    bool gui_query_succeeded{}, capture_clear{}, menu_clear{}, move_size_clear{}, gui_in_movesize_clear{};
    std::uintptr_t capture_hwnd{}, menu_owner_hwnd{}, move_size_hwnd{};
    std::uint32_t gui_flags{}, gui_error{};
    bool cursor_success{}, cursor_root_owned_or_guard{}, buttons_modifiers_clear{}, left_down{}, raw_up_seen{};
    std::array<std::int64_t, 2> cursor{};
    std::uintptr_t cursor_root{};
    std::uint32_t cursor_error{};
    bool receiver_healthy{}, takeover_healthy{}, foreign_capture_transferred{};
    std::uint32_t dpi{};
    bool dpi_matches{}, monitor_matches{};
    std::uintptr_t monitor{};
    bool actual_positioning_available{}, actual_visible_available{};
    bool terminal_positioning_available{}, terminal_visible_available{};
    std::array<std::int64_t, 4> terminal_positioning{}, terminal_visible{}, actual_positioning{}, actual_visible{};
    std::uint32_t positioning_error{};
    std::int64_t visible_hresult{};
    bool terminal_positioning_exact{}, terminal_visible_exact{};
    int native_drag_after_cancel_return{}, native_drag_after_end{}, native_drag_after_winevent_end{}, unowned_geometry_changes{};
};
constexpr Failures classify(const HandoffPreflightDiagnostic& d) noexcept {
    Failures f;
    f.add(!d.end_observed, Failure::MissingNativeEnd);
    f.add(!d.winevent_end_observed, Failure::MissingWinEventEnd);
    f.add(!d.own_identity, Failure::IdentityChanged);
    f.add(!d.desktop_ready, Failure::DesktopUnavailable);
    f.add(!d.source_visible, Failure::SourceNotVisible);
    f.add(!d.foreground_matches, Failure::ForegroundChanged);
    f.add(!d.gui_query_succeeded, Failure::GuiQueryFailed);
    f.add(d.gui_query_succeeded && !d.capture_clear, Failure::CaptureStillOwned);
    f.add(d.gui_query_succeeded && !d.menu_clear, Failure::MenuStillActive);
    f.add(d.gui_query_succeeded && (!d.move_size_clear || !d.gui_in_movesize_clear), Failure::MoveSizeStillActive);
    f.add(!d.cursor_success, Failure::CursorUnavailable);
    f.add(d.cursor_success && !d.cursor_root_owned_or_guard, Failure::CursorOutsideOwnedInputGuard);
    f.add(!d.left_down, Failure::ButtonReleasedBeforeHandoff);
    f.add(!d.buttons_modifiers_clear, Failure::InputInterference);
    f.add(d.raw_up_seen, Failure::RawUpAlreadyObserved);
    f.add(!d.receiver_healthy, Failure::ReceiverUnhealthy);
    f.add(d.foreign_capture_transferred, Failure::ForeignCaptureTransferred);
    f.add(d.own_identity && !d.dpi_matches, Failure::DpiChanged);
    f.add(d.own_identity && !d.monitor_matches, Failure::MonitorChanged);
    f.add(!d.actual_positioning_available, Failure::ActualPositioningUnavailable);
    f.add(!d.actual_visible_available, Failure::ActualVisibleUnavailable);
    f.add(!d.terminal_positioning_available, Failure::TerminalPositioningUnavailable);
    f.add(!d.terminal_visible_available, Failure::TerminalVisibleUnavailable);
    const bool p = d.actual_positioning_available && d.terminal_positioning_available && !d.terminal_positioning_exact;
    const bool v = d.actual_visible_available && d.terminal_visible_available && !d.terminal_visible_exact;
    if (p && v) f.add(true, Failure::BothTerminalGeometryMismatch);
    else { f.add(p, Failure::TerminalPositioningMismatch); f.add(v, Failure::TerminalVisibleMismatch); }
    f.add(d.native_drag_after_cancel_return != 0, Failure::NativeDragAfterCancelReturn);
    f.add(d.native_drag_after_end != 0 || d.native_drag_after_winevent_end != 0, Failure::NativeDragAfterEnd);
    f.add(d.unowned_geometry_changes != 0, Failure::UnownedGeometryChange);
    f.add(!d.takeover_healthy, Failure::TakeoverAlreadyUnhealthy);
    return f;
}
struct CleanupInputDiagnostic {
    bool own_identity{}, desktop_ready{}, source_visible{}, foreground_matches{}, gui_query_succeeded{};
    bool no_foreign_capture{}, menu_clear{}, move_size_clear{}, gui_mode_clear{};
    bool cursor_success{}, cursor_root_owned_or_guard{}, left_down{}, buttons_modifiers_clear{}, foreign_capture_transferred{};
    constexpr bool eligible() const noexcept {
        return own_identity && desktop_ready && source_visible && foreground_matches && gui_query_succeeded &&
            no_foreign_capture && menu_clear && move_size_clear && gui_mode_clear && cursor_success &&
            cursor_root_owned_or_guard && left_down && buttons_modifiers_clear && !foreign_capture_transferred;
    }
};
} // namespace panebind::test::handoff
