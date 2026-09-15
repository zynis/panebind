#include "platform/windows/operations/window_rect_adjustment.h"
#include <iostream>
namespace o=panebind::platform::windows::operations;
using panebind::core::geometry::Rect;
int main(){int failures{},checks{};const auto check=[&](bool ok){++checks;if(!ok)++failures;};
    auto r=o::prepare_visible_rect_adjustment(Rect{90,95,215,220},Rect{100,100,200,200},Rect{120,130,250,270});
    check(r.positioning==Rect{110,125,265,290});
    r=o::prepare_visible_rect_adjustment(Rect{105,104,195,196},Rect{100,100,200,200},Rect{120,130,250,270});
    check(r.positioning==Rect{125,134,245,266});
    r=o::prepare_visible_rect_adjustment(Rect{105,104,195,196},Rect{100,100,200,200},Rect{120,130,125,134});
    check(!r.positioning&&r.status==o::RectAdjustmentStatus::InvalidEdges);
    r=o::prepare_visible_rect_adjustment(Rect{0,0,100,100},Rect{0,0,100,100},o::RawRectEdges{20,0,10,100});check(!r.positioning);
    r=o::prepare_visible_rect_adjustment(Rect{0,0,100,100},Rect{0,0,100,100},o::RawRectEdges{0,0,0,100});check(!r.positioning);
    const auto max=std::numeric_limits<std::int64_t>::max();
    r=o::prepare_visible_rect_adjustment(Rect{-10,-10,110,110},Rect{0,0,100,100},o::RawRectEdges{max-20,0,max,100});check(!r.positioning&&r.status==o::RectAdjustmentStatus::ArithmeticOverflow);
    r=o::prepare_visible_rect_adjustment(Rect{0,0,100,100},Rect{0,0,100,100},o::RawRectEdges{2147483640,0,2147483650LL,100});check(!r.positioning&&r.status==o::RectAdjustmentStatus::NativeCoordinateOutOfRange);
    r=o::prepare_visible_rect_adjustment(Rect{0,0,0,100},Rect{0,0,100,100},Rect{0,0,100,100});check(!r.positioning);
    std::cout<<"rect-adjustment checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
