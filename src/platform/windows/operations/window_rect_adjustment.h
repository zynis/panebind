#pragma once
#include "core/geometry/checked_arithmetic.h"
#include "core/geometry/geometry.h"
#include <array>
#include <limits>
namespace panebind::platform::windows::operations {
enum class RectAdjustmentStatus { Succeeded, InvalidEdges, ArithmeticOverflow, NativeCoordinateOutOfRange };
struct RawRectEdges {std::int64_t left{},top{},right{},bottom{};};
struct RectAdjustmentResult {
    RectAdjustmentStatus status{RectAdjustmentStatus::ArithmeticOverflow};
    std::optional<core::geometry::Rect> positioning;
};
inline RectAdjustmentResult prepare_visible_rect_adjustment(const core::geometry::Rect& positioning,
    const core::geometry::Rect& visible,RawRectEdges target) noexcept {
    using core::geometry::checked_add;using core::geometry::checked_difference;
    const auto positive=[](auto right,auto left){const auto n=checked_difference(right,left);return n&&*n>0;};
    if(!positive(positioning.right(),positioning.left())||!positive(positioning.bottom(),positioning.top())||
       !positive(visible.right(),visible.left())||!positive(visible.bottom(),visible.top())||
       target.left>=target.right||target.top>=target.bottom)return {RectAdjustmentStatus::InvalidEdges,{}};
    const auto l=checked_difference(visible.left(),positioning.left());
    const auto t=checked_difference(visible.top(),positioning.top());
    const auto r=checked_difference(positioning.right(),visible.right());
    const auto b=checked_difference(positioning.bottom(),visible.bottom());
    if(!l||!t||!r||!b)return {};
    const auto left=checked_difference(target.left,*l),top=checked_difference(target.top,*t);
    const auto right=checked_add(target.right,*r),bottom=checked_add(target.bottom,*b);
    if(!left||!top||!right||!bottom)return {};
    if(*left>=*right||*top>=*bottom)return {RectAdjustmentStatus::InvalidEdges,{}};
    const auto width=checked_difference(*right,*left),height=checked_difference(*bottom,*top);
    if(!width||!height)return {};
    constexpr auto lo=std::numeric_limits<std::int32_t>::min(),hi=std::numeric_limits<std::int32_t>::max();
    for(const auto n:std::array{*left,*top,*right,*bottom,*width,*height})
        if(n<lo||n>hi)return {RectAdjustmentStatus::NativeCoordinateOutOfRange,{}};
    return {RectAdjustmentStatus::Succeeded,core::geometry::Rect{*left,*top,*right,*bottom}};
}
inline RectAdjustmentResult prepare_visible_rect_adjustment(const core::geometry::Rect& positioning,
    const core::geometry::Rect& visible,const core::geometry::Rect& target) noexcept {
    return prepare_visible_rect_adjustment(positioning,visible,RawRectEdges{target.left(),target.top(),target.right(),target.bottom()});
}
}
