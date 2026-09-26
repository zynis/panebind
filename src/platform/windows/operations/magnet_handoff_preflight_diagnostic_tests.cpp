#include "platform/windows/operations/magnet_handoff_preflight_diagnostic.h"
#include <iostream>
#include <string_view>

namespace h = panebind::test::handoff;
namespace {
int checks{};
bool check(bool value, std::string_view label) {
    ++checks;
    if (!value) std::cerr << "FAIL: " << label << '\n';
    return value;
}
h::HandoffPreflightDiagnostic valid() {
    h::HandoffPreflightDiagnostic d;
    d.end_observed = d.winevent_end_observed = true;
    d.own_identity = d.desktop_ready = d.source_visible = d.foreground_matches = true;
    d.gui_query_succeeded = d.capture_clear = d.menu_clear = d.move_size_clear = d.gui_in_movesize_clear = true;
    d.cursor_success = d.cursor_root_owned_or_guard = d.buttons_modifiers_clear = d.left_down = true;
    d.receiver_healthy = d.takeover_healthy = d.dpi_matches = d.monitor_matches = true;
    d.actual_positioning_available = d.actual_visible_available = true;
    d.terminal_positioning_available = d.terminal_visible_available = true;
    d.terminal_positioning_exact = d.terminal_visible_exact = true;
    return d;
}
}
int main() {
    bool passed = check(h::classify(valid()).count == 0, "complete facts pass");
    const auto test = [&](auto mutate, h::Failure expected) {
        auto d = valid(); mutate(d); const auto result = h::classify(d);
        passed = check(result.count == 1 && result.first() == expected, h::name(expected)) && passed;
    };
#define FIELD_CASE(field, value, failure) test([](auto& d) { d.field = value; }, h::Failure::failure)
    FIELD_CASE(end_observed, false, MissingNativeEnd);
    FIELD_CASE(winevent_end_observed, false, MissingWinEventEnd);
    FIELD_CASE(own_identity, false, IdentityChanged);
    FIELD_CASE(desktop_ready, false, DesktopUnavailable);
    FIELD_CASE(source_visible, false, SourceNotVisible);
    FIELD_CASE(foreground_matches, false, ForegroundChanged);
    FIELD_CASE(gui_query_succeeded, false, GuiQueryFailed);
    FIELD_CASE(capture_clear, false, CaptureStillOwned);
    FIELD_CASE(menu_clear, false, MenuStillActive);
    FIELD_CASE(move_size_clear, false, MoveSizeStillActive);
    FIELD_CASE(gui_in_movesize_clear, false, MoveSizeStillActive);
    FIELD_CASE(cursor_success, false, CursorUnavailable);
    FIELD_CASE(cursor_root_owned_or_guard, false, CursorOutsideOwnedInputGuard);
    FIELD_CASE(left_down, false, ButtonReleasedBeforeHandoff);
    FIELD_CASE(buttons_modifiers_clear, false, InputInterference);
    FIELD_CASE(raw_up_seen, true, RawUpAlreadyObserved);
    FIELD_CASE(receiver_healthy, false, ReceiverUnhealthy);
    FIELD_CASE(foreign_capture_transferred, true, ForeignCaptureTransferred);
    FIELD_CASE(dpi_matches, false, DpiChanged);
    FIELD_CASE(monitor_matches, false, MonitorChanged);
    FIELD_CASE(actual_positioning_available, false, ActualPositioningUnavailable);
    FIELD_CASE(actual_visible_available, false, ActualVisibleUnavailable);
    FIELD_CASE(terminal_positioning_available, false, TerminalPositioningUnavailable);
    FIELD_CASE(terminal_visible_available, false, TerminalVisibleUnavailable);
    FIELD_CASE(terminal_positioning_exact, false, TerminalPositioningMismatch);
    FIELD_CASE(terminal_visible_exact, false, TerminalVisibleMismatch);
    FIELD_CASE(native_drag_after_cancel_return, 1, NativeDragAfterCancelReturn);
    FIELD_CASE(native_drag_after_end, 1, NativeDragAfterEnd);
    FIELD_CASE(native_drag_after_winevent_end, 1, NativeDragAfterEnd);
    FIELD_CASE(unowned_geometry_changes, 1, UnownedGeometryChange);
    FIELD_CASE(takeover_healthy, false, TakeoverAlreadyUnhealthy);
#undef FIELD_CASE
    test([](auto& d) { d.terminal_positioning_exact = d.terminal_visible_exact = false; }, h::Failure::BothTerminalGeometryMismatch);
    auto compound = valid(); compound.desktop_ready = compound.cursor_root_owned_or_guard = false;
    const auto facts = h::classify(compound);
    passed = check(facts.count == 2 && facts.first() == h::Failure::DesktopUnavailable && facts.values[1] == h::Failure::CursorOutsideOwnedInputGuard, "guard not sole failure") && passed;
    compound = valid(); compound.native_drag_after_end = 1; compound.takeover_healthy = false;
    const auto specific = h::classify(compound);
    passed = check(specific.count == 2 && specific.first() == h::Failure::NativeDragAfterEnd, "specific before generic unhealthy") && passed;
    h::CleanupInputDiagnostic cleanup;
    passed = check(!cleanup.eligible(), "unknown cleanup authority denies input") && passed;
    cleanup.own_identity = cleanup.desktop_ready = cleanup.source_visible = cleanup.foreground_matches = cleanup.gui_query_succeeded = true;
    cleanup.no_foreign_capture = cleanup.menu_clear = cleanup.move_size_clear = cleanup.gui_mode_clear = true;
    cleanup.cursor_success = cleanup.cursor_root_owned_or_guard = cleanup.left_down = cleanup.buttons_modifiers_clear = true;
    passed = check(cleanup.eligible(), "fresh complete cleanup authority") && passed;
    cleanup.foreign_capture_transferred = true;
    passed = check(!cleanup.eligible(), "foreign capture cleanup denies input") && passed;
    if (!passed) return 1;
    std::cout << "handoff-preflight pure classifier checks=" << checks << " PASS\n";
    return 0;
}
