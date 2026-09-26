#pragma once

// Test-only split authority model. No Win32 API and no input/write authority.
#include "platform/windows/operations/magnet_handoff_preflight_diagnostic.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace panebind::test::isolation {
namespace h = panebind::test::handoff;
struct ProductHandoffAuthorityDiagnostic {
    // Cursor availability, coordinates and hit-test roots are intentionally
    // absent. They belong to the synthetic-input isolation/data proof.
    bool end_observed{}, winevent_end_observed{}, own_identity{}, desktop_ready{}, source_visible{}, foreground_matches{};
    bool gui_query_succeeded{}, capture_clear{}, menu_clear{}, move_size_clear{}, gui_in_movesize_clear{};
    bool left_down{}, buttons_modifiers_clear{}, raw_up_seen{}, receiver_healthy{}, takeover_healthy{}, foreign_capture_transferred{};
    bool dpi_matches{}, monitor_matches{}, actual_positioning_available{}, actual_visible_available{};
    bool terminal_positioning_available{}, terminal_visible_available{}, terminal_positioning_exact{}, terminal_visible_exact{};
    int native_drag_after_cancel_return{}, native_drag_after_end{}, native_drag_after_winevent_end{}, unowned_geometry_changes{};
    constexpr bool operator==(const ProductHandoffAuthorityDiagnostic&) const = default;
};
constexpr ProductHandoffAuthorityDiagnostic product_facts(const h::HandoffPreflightDiagnostic& d) noexcept {
    return {d.end_observed,d.winevent_end_observed,d.own_identity,d.desktop_ready,d.source_visible,d.foreground_matches,
        d.gui_query_succeeded,d.capture_clear,d.menu_clear,d.move_size_clear,d.gui_in_movesize_clear,
        d.left_down,d.buttons_modifiers_clear,d.raw_up_seen,d.receiver_healthy,d.takeover_healthy,d.foreign_capture_transferred,
        d.dpi_matches,d.monitor_matches,d.actual_positioning_available,d.actual_visible_available,
        d.terminal_positioning_available,d.terminal_visible_available,d.terminal_positioning_exact,d.terminal_visible_exact,
        d.native_drag_after_cancel_return,d.native_drag_after_end,d.native_drag_after_winevent_end,d.unowned_geometry_changes};
}
constexpr h::Failures classify_product(const ProductHandoffAuthorityDiagnostic& d) noexcept {
    h::Failures f;
    f.add(!d.end_observed,h::Failure::MissingNativeEnd);f.add(!d.winevent_end_observed,h::Failure::MissingWinEventEnd);
    f.add(!d.own_identity,h::Failure::IdentityChanged);f.add(!d.desktop_ready,h::Failure::DesktopUnavailable);
    f.add(!d.source_visible,h::Failure::SourceNotVisible);f.add(!d.foreground_matches,h::Failure::ForegroundChanged);
    f.add(!d.gui_query_succeeded,h::Failure::GuiQueryFailed);
    f.add(d.gui_query_succeeded&&!d.capture_clear,h::Failure::CaptureStillOwned);
    f.add(d.gui_query_succeeded&&!d.menu_clear,h::Failure::MenuStillActive);
    f.add(d.gui_query_succeeded&&(!d.move_size_clear||!d.gui_in_movesize_clear),h::Failure::MoveSizeStillActive);
    f.add(!d.left_down,h::Failure::ButtonReleasedBeforeHandoff);f.add(!d.buttons_modifiers_clear,h::Failure::InputInterference);
    f.add(d.raw_up_seen,h::Failure::RawUpAlreadyObserved);f.add(!d.receiver_healthy,h::Failure::ReceiverUnhealthy);
    f.add(d.foreign_capture_transferred,h::Failure::ForeignCaptureTransferred);
    f.add(d.own_identity&&!d.dpi_matches,h::Failure::DpiChanged);f.add(d.own_identity&&!d.monitor_matches,h::Failure::MonitorChanged);
    f.add(!d.actual_positioning_available,h::Failure::ActualPositioningUnavailable);f.add(!d.actual_visible_available,h::Failure::ActualVisibleUnavailable);
    f.add(!d.terminal_positioning_available,h::Failure::TerminalPositioningUnavailable);f.add(!d.terminal_visible_available,h::Failure::TerminalVisibleUnavailable);
    const bool p=d.actual_positioning_available&&d.terminal_positioning_available&&!d.terminal_positioning_exact;
    const bool v=d.actual_visible_available&&d.terminal_visible_available&&!d.terminal_visible_exact;
    if(p&&v)f.add(true,h::Failure::BothTerminalGeometryMismatch);
    else{f.add(p,h::Failure::TerminalPositioningMismatch);f.add(v,h::Failure::TerminalVisibleMismatch);}
    f.add(d.native_drag_after_cancel_return!=0,h::Failure::NativeDragAfterCancelReturn);
    f.add(d.native_drag_after_end!=0||d.native_drag_after_winevent_end!=0,h::Failure::NativeDragAfterEnd);
    f.add(d.unowned_geometry_changes!=0,h::Failure::UnownedGeometryChange);f.add(!d.takeover_healthy,h::Failure::TakeoverAlreadyUnhealthy);
    return f;
}
struct Identity {
    std::uintptr_t hwnd{};std::uint32_t pid{},tid{};std::uintptr_t nonce{};
    constexpr bool operator==(const Identity&) const = default;
};
constexpr bool exact_identity(const Identity& actual,const Identity& expected) noexcept {
    return expected.hwnd&&expected.pid&&expected.tid&&expected.nonce&&actual==expected;
}
using Point=std::array<std::int64_t,2>;
using Rect=std::array<std::int64_t,4>;
constexpr bool valid(const Rect& r) noexcept {return r[0]<r[2]&&r[1]<r[3];}
constexpr bool contains(const Rect& r,const Point& p) noexcept {return valid(r)&&p[0]>=r[0]&&p[0]<r[2]&&p[1]>=r[1]&&p[1]<r[3];}
constexpr bool overlaps(const Rect& a,const Rect& b) noexcept {return valid(a)&&valid(b)&&a[0]<b[2]&&b[0]<a[2]&&a[1]<b[3]&&b[1]<a[3];}
struct TestInputIsolationDiagnostic {
    bool cursor_available{},source_positioning_available{},foreground_matches{},shield_needed{},shield_active{};
    bool shield_noactivate{},shield_topmost{},shield_visible{},shield_never_activated{};
    Identity expected_source,actual_source,expected_shield,actual_shield;
    Rect source_positioning{},shield_positioning{};Point destination{};std::uintptr_t root{};
    constexpr bool passed() const noexcept {
        if(!cursor_available||!source_positioning_available||!valid(source_positioning)||!foreground_matches||!exact_identity(actual_source,expected_source))return false;
        if(shield_needed&&(!valid(shield_positioning)||!shield_active||!shield_noactivate||!shield_topmost||!shield_visible||!shield_never_activated||!exact_identity(actual_shield,expected_shield)||overlaps(source_positioning,shield_positioning)))return false;
        if(contains(source_positioning,destination))return root==expected_source.hwnd;
        return shield_needed&&contains(shield_positioning,destination)&&root==expected_shield.hwnd;
    }
};
constexpr std::size_t planned_point_count=19; // current + samples 3..20
struct ShieldPlan {bool valid{},needed{},top_margin_clipped{};Rect positioning{};std::int64_t min_x{},max_x{},min_y{},max_y{};};
constexpr bool native_coordinate(std::int64_t x) noexcept {return x>=-2147483648LL&&x<=2147483647LL;}
constexpr bool contained_rect(const Rect& area,const Rect& r) noexcept {return valid(area)&&valid(r)&&r[0]>=area[0]&&r[1]>=area[1]&&r[2]<=area[2]&&r[3]<=area[3];}
constexpr bool matches_planned_cursor(const Point& actual,const Point& planned) noexcept {
    for(auto coordinate:actual)if(!native_coordinate(coordinate))return false;
    for(auto coordinate:planned)if(!native_coordinate(coordinate))return false;
    return actual[0]>=planned[0]-1&&actual[0]<=planned[0]+1&&actual[1]>=planned[1]-1&&actual[1]<=planned[1]+1;
}
constexpr ShieldPlan bottom_shield_plan(const Rect& terminal,const Rect& source,const std::array<Point,planned_point_count>& points) noexcept {
    ShieldPlan plan;if(!valid(terminal)||!valid(source))return plan;
    for(auto coordinate:terminal)if(!native_coordinate(coordinate))return plan;
    for(auto coordinate:source)if(!native_coordinate(coordinate))return plan;
    plan.min_x=plan.max_x=points[0][0];plan.min_y=plan.max_y=points[0][1];
    for(const auto& point:points){
        if(!native_coordinate(point[0])||!native_coordinate(point[1]))return {};
        plan.min_x=std::min(plan.min_x,point[0]);plan.max_x=std::max(plan.max_x,point[0]);
        plan.min_y=std::min(plan.min_y,point[1]);plan.max_y=std::max(plan.max_y,point[1]);
        if(!contains(source,point))plan.needed=true;
    }
    if(!plan.needed){plan.valid=true;return plan;}
    plan.positioning={plan.min_x-50,std::max(terminal[3],source[3]),plan.max_x+51,plan.max_y+51};
    for(auto coordinate:plan.positioning)if(!native_coordinate(coordinate))return {};
    if(!valid(plan.positioning)||overlaps(plan.positioning,source))return {};
    for(const auto& point:points)if(!contains(source,point)&&!contains(plan.positioning,point))return {};
    plan.top_margin_clipped=plan.positioning[1]>plan.min_y-50;plan.valid=true;return plan;
}
constexpr bool source_write_after_gates(std::int64_t entry,std::int64_t native_end,std::int64_t winevent_end,std::int64_t isolation_ready) noexcept {
    return native_end>0&&winevent_end>0&&isolation_ready>winevent_end&&isolation_ready>native_end&&entry>isolation_ready;
}
constexpr ShieldPlan position_existing_shield(const ShieldPlan& frozen,const Rect& terminal,const Rect& source) noexcept {
    if(!frozen.valid||!frozen.needed||!valid(terminal)||!valid(source))return {};
    ShieldPlan plan=frozen;plan.positioning={frozen.min_x-50,std::max(terminal[3],source[3]),frozen.max_x+51,frozen.max_y+51};
    for(auto coordinate:plan.positioning)if(!native_coordinate(coordinate))return {};
    if(!valid(plan.positioning)||overlaps(plan.positioning,source))return {};
    plan.top_margin_clipped=plan.positioning[1]>frozen.min_y-50;return plan;
}
constexpr bool destroy_after_acceptance(std::int64_t destroyed,std::int64_t accepted,std::int64_t raw_up) noexcept {
    return accepted>0&&raw_up>0&&destroyed>accepted&&destroyed>raw_up;
}
} // namespace panebind::test::isolation
