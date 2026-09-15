#include "platform/windows/explorer/explorer_group_session.h"
#include <iostream>
#include <limits>
namespace e=panebind::platform::windows::explorer;
int main() {
    int failures{};const auto check=[&](bool ok){if(!ok)++failures;};
    e::detail::GroupSnapshots s;
    for(auto& m:s){m.dpi=192;m.monitor_device_name=L"DISPLAY1";m.monitor_work_area={0,0,3072,1824};}
    s[0].visible_rect={0,0,120,180};s[1].visible_rect={120,40,220,180};s[2].visible_rect={20,180,260,270};
    auto r=e::group_layout_readiness(s);check(r.ready&&r.relation_count==3);
    for(const auto& component:r.components)check(component==std::vector<std::size_t>{0,1,2});
    for(const auto& pair:r.pairs)check(pair.relation&&pair.orthogonal_overlap>0&&pair.signed_gap==0);
    s[2].visible_rect={20,180,120,270};r=e::group_layout_readiness(s);
    check(r.ready&&r.relation_count==2&&!r.pairs[2].relation&&r.pairs[2].orthogonal_overlap==0);
    auto disconnected=s;disconnected[1].visible_rect={900,400,1100,600};
    check(!e::group_layout_readiness(disconnected).ready);
    auto mixed=s;mixed[1].dpi=96;check(!e::group_layout_readiness(mixed).ready);
    auto monitor=s;monitor[2].monitor_device_name=L"DISPLAY2";check(!e::group_layout_readiness(monitor).ready);
    auto empty=s;empty[0].visible_rect={0,0,0,100};check(!e::group_layout_readiness(empty).ready);
    auto overflow=s;overflow[0].visible_rect={std::numeric_limits<std::int64_t>::min(),0,std::numeric_limits<std::int64_t>::max(),100};check(!e::group_layout_readiness(overflow).ready);
    std::cout<<"group-topology-preview failures="<<failures<<'\n';return failures?1:0;
}
