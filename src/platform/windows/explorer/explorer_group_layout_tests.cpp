#include "platform/windows/explorer/explorer_group_session.h"
#include <iostream>
#include <limits>
namespace e=panebind::platform::windows::explorer;
using panebind::core::geometry::Rect;
int main() {
    int failures{};const auto check=[&](bool ok){if(!ok)++failures;};
    e::detail::GroupSnapshots s;
    for(auto& m:s){m.dpi=192;m.monitor_device_name=L"DISPLAY1";m.monitor_work_area={0,0,3072,1824};m.visible_rect={100,100,1338,971};}
    auto r=e::group_layout_readiness(s);check(r.ready);
    check(r.targets[0].right()==r.targets[1].left() && r.targets[0].bottom()==r.targets[2].top());
    auto large=s;large[2].visible_rect={0,0,1238,1200};r=e::group_layout_readiness(large);check(!r.ready&&r.height_deficit==247);
    auto mixed=s;mixed[1].dpi=96;check(!e::group_layout_readiness(mixed).ready);
    auto monitor=s;monitor[2].monitor_device_name=L"DISPLAY2";check(!e::group_layout_readiness(monitor).ready);
    auto empty=s;empty[0].visible_rect={0,0,0,100};check(!e::group_layout_readiness(empty).ready);
    auto overflow=s;overflow[0].visible_rect={std::numeric_limits<std::int64_t>::min(),0,std::numeric_limits<std::int64_t>::max(),100};check(!e::group_layout_readiness(overflow).ready);
    std::cout<<"group-layout failures="<<failures<<'\n';return failures?1:0;
}
