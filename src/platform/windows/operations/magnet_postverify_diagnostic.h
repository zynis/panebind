#pragma once
#include "core/geometry/geometry.h"
#include "core/geometry/checked_arithmetic.h"
#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>

namespace panebind::platform::windows::operations {
enum class MagnetPostverifyFailure {
    None, NativeCallFailed, CaptureFailed, PositioningMismatch, VisibleMismatch,
    BothGeometryMismatch, OtherMemberChanged, ReceiptHealthFailed, SourceContextChanged
};
inline std::string_view magnet_postverify_failure_name(MagnetPostverifyFailure value) {
    switch(value) {
    case MagnetPostverifyFailure::None:return "None";
    case MagnetPostverifyFailure::NativeCallFailed:return "NativeCallFailed";
    case MagnetPostverifyFailure::CaptureFailed:return "CaptureFailed";
    case MagnetPostverifyFailure::PositioningMismatch:return "PositioningMismatch";
    case MagnetPostverifyFailure::VisibleMismatch:return "VisibleMismatch";
    case MagnetPostverifyFailure::BothGeometryMismatch:return "BothGeometryMismatch";
    case MagnetPostverifyFailure::OtherMemberChanged:return "OtherMemberChanged";
    case MagnetPostverifyFailure::ReceiptHealthFailed:return "ReceiptHealthFailed";
    case MagnetPostverifyFailure::SourceContextChanged:return "SourceContextChanged";
    }
    return "Unknown";
}
// Placement observations, not capability/capture failures. No native authority.
struct MagnetPostverifyDiagnostic {
    bool capture_succeeded{},native_success{};
    std::uint32_t win32_error{};
    std::size_t source_member{};
    core::geometry::Rect requested_visible,requested_positioning;
    std::optional<core::geometry::Rect> actual_visible,actual_positioning;
    bool visible_exact{},positioning_exact{},other_members_exact{},receipt_health{},source_context_exact{};
    std::optional<std::array<std::int64_t,4>> visible_edge_delta,positioning_edge_delta;
    MagnetPostverifyFailure failure_class{MagnetPostverifyFailure::None};
    void classify() {
        visible_exact=actual_visible&&*actual_visible==requested_visible;
        positioning_exact=actual_positioning&&*actual_positioning==requested_positioning;
        const auto delta=[](const auto& a,const auto& t) {
            return std::array<std::int64_t,4>{core::geometry::checked_difference(a.left(),t.left()).value(),
                core::geometry::checked_difference(a.top(),t.top()).value(),core::geometry::checked_difference(a.right(),t.right()).value(),
                core::geometry::checked_difference(a.bottom(),t.bottom()).value()};
        };
        visible_edge_delta=actual_visible?std::optional{delta(*actual_visible,requested_visible)}:std::nullopt;
        positioning_edge_delta=actual_positioning?std::optional{delta(*actual_positioning,requested_positioning)}:std::nullopt;
        failure_class=!native_success?MagnetPostverifyFailure::NativeCallFailed:
            !capture_succeeded?MagnetPostverifyFailure::CaptureFailed:
            !positioning_exact&&!visible_exact?MagnetPostverifyFailure::BothGeometryMismatch:
            !positioning_exact?MagnetPostverifyFailure::PositioningMismatch:
            !visible_exact?MagnetPostverifyFailure::VisibleMismatch:
            !source_context_exact?MagnetPostverifyFailure::SourceContextChanged:
            !other_members_exact?MagnetPostverifyFailure::OtherMemberChanged:
            !receipt_health?MagnetPostverifyFailure::ReceiptHealthFailed:MagnetPostverifyFailure::None;
    }
};
inline std::string magnet_postverify_json(const MagnetPostverifyDiagnostic& d) {
    const auto rect=[](const core::geometry::Rect& r){return "["+std::to_string(r.left())+","+std::to_string(r.top())+","+std::to_string(r.right())+","+std::to_string(r.bottom())+"]";};
    const auto delta=[](const auto& v){return v?"["+std::to_string((*v)[0])+","+std::to_string((*v)[1])+","+std::to_string((*v)[2])+","+std::to_string((*v)[3])+"]":std::string{"null"};};
    std::ostringstream s;s<<std::boolalpha<<"{\"capture_succeeded\":"<<d.capture_succeeded<<",\"native_success\":"<<d.native_success
        <<",\"win32_error\":"<<d.win32_error<<",\"source_member\":"<<d.source_member
        <<",\"requested_visible\":"<<rect(d.requested_visible)<<",\"requested_positioning\":"<<rect(d.requested_positioning)
        <<",\"actual_visible\":"<<(d.actual_visible?rect(*d.actual_visible):"null")<<",\"actual_positioning\":"<<(d.actual_positioning?rect(*d.actual_positioning):"null")
        <<",\"visible_exact\":"<<d.visible_exact<<",\"positioning_exact\":"<<d.positioning_exact
        <<",\"visible_edge_delta\":"<<delta(d.visible_edge_delta)<<",\"positioning_edge_delta\":"<<delta(d.positioning_edge_delta)
        <<",\"other_members_exact\":"<<d.other_members_exact<<",\"receipt_health\":"<<d.receipt_health<<",\"source_context_exact\":"<<d.source_context_exact
        <<",\"failure_class\":\""<<magnet_postverify_failure_name(d.failure_class)<<"\"}";return s.str();
}
}
