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
    const e::CtrlSample ctrl{true,true,true,false};
    check(e::product_ctrl_glue_eligible(0U,0,ctrl,r));
    check(e::product_ctrl_glue_eligible(1U,1,ctrl,r));
    check(e::product_ctrl_glue_eligible(2U,2,ctrl,r));
    check(!e::product_ctrl_glue_eligible(std::nullopt,0,ctrl,r));
    check(!e::product_ctrl_glue_eligible(1U,0,ctrl,r));
    check(!e::product_ctrl_glue_eligible(3U,3,ctrl,r));
    check(!e::product_ctrl_glue_eligible(0U,0,e::CtrlSample{true,false,false,false},r));
    check(!e::product_ctrl_glue_eligible(0U,0,e::CtrlSample{},r));
    const e::GroupEventReceipt start{0,11,21,reinterpret_cast<HWND>(1),
        e::GroupEventKind::Start,1,31,41,51,ctrl};
    const e::GroupEventReceipt location{0,11,21,reinterpret_cast<HWND>(1),
        e::GroupEventKind::Location,2,31,42,52,{}};
    const std::array batch{start,location};
    check(e::product_receipts_match(batch,batch));
    check(!e::product_receipts_match(batch,std::span{batch}.first(1)));
    auto changed=batch;changed[0].ctrl.ctrl=false;
    check(!e::product_receipts_match(batch,changed));
    changed=batch;changed[0].native_source=reinterpret_cast<HWND>(2);
    check(!e::product_receipts_match(batch,changed));
    changed=batch;changed[1].sequence=3;
    check(!e::product_receipts_match(batch,changed));
    auto down=s;
    for(auto& m:down)m.positioning_rect=m.visible_rect;
    auto after=down;
    after[0].visible_rect={10,20,130,200};
    after[0].positioning_rect=after[0].visible_rect;
    check(e::product_ctrl_start_baseline_matches(down,after,0));
    check(e::group_layout_readiness(down).ready);
    // START and LOCATION can be drained together after the native leader has
    // translated. The DOWN-time graph remains the Ctrl Glue origin.
    namespace b=panebind::core::behavior;
    namespace t=panebind::core::topology;
    const std::array<t::WindowGeometry,3> initial{{
        {panebind::core::model::WindowId{"0"},down[0].visible_rect},
        {panebind::core::model::WindowId{"1"},down[1].visible_rect},
        {panebind::core::model::WindowId{"2"},down[2].visible_rect}}};
    const b::GlueGroupMember leader{panebind::core::model::WindowId{std::string{"0"}},1};
    const std::vector<b::GlueGroupMember> members{{panebind::core::model::WindowId{std::string{"0"}},1},
        {panebind::core::model::WindowId{std::string{"1"}},1},
        {panebind::core::model::WindowId{std::string{"2"}},1}};
    b::GlueGroupMoveCoordinator same_path(members,1,16);
    check(same_path.start(leader,true,1,initial,{},3));
    const auto translated=same_path.plan(leader,2,after[0].visible_rect);
    check(translated.has_value() && translated->followers.size()==2);
    if(translated) {
        for(const auto& follower:translated->followers) {
            if(follower.id.value()=="1")check(follower.target_visible_rect==
                panebind::core::geometry::Rect{130,60,230,200});
            if(follower.id.value()=="2")check(follower.target_visible_rect==
                panebind::core::geometry::Rect{30,200,270,290});
        }
    }
    auto foreign=after;foreign[1].visible_rect={130,40,230,180};
    check(!e::product_ctrl_start_baseline_matches(down,foreign,0));
    auto resize=after;resize[0].visible_rect={10,20,140,200};
    check(!e::product_ctrl_start_baseline_matches(down,resize,0));
    auto wrong_context=after;wrong_context[2].dpi=96;
    check(!e::product_ctrl_start_baseline_matches(down,wrong_context,0));
    check(!e::product_ctrl_start_baseline_matches(down,after,3));
    for(const auto& component:r.components)check(component==std::vector<std::size_t>{0,1,2});
    for(const auto& pair:r.pairs)check(pair.relation&&pair.orthogonal_overlap>0&&pair.signed_gap==0);
    s[2].visible_rect={20,180,120,270};r=e::group_layout_readiness(s);
    check(r.ready&&r.relation_count==2&&!r.pairs[2].relation&&r.pairs[2].orthogonal_overlap==0);
    auto disconnected=s;disconnected[1].visible_rect={900,400,1100,600};
    check(!e::group_layout_readiness(disconnected).ready);
    check(!e::product_ctrl_glue_eligible(0U,0,ctrl,e::group_layout_readiness(disconnected)));
    auto mixed=s;mixed[1].dpi=96;check(!e::group_layout_readiness(mixed).ready);
    auto monitor=s;monitor[2].monitor_device_name=L"DISPLAY2";check(!e::group_layout_readiness(monitor).ready);
    auto empty=s;empty[0].visible_rect={0,0,0,100};check(!e::group_layout_readiness(empty).ready);
    auto overflow=s;overflow[0].visible_rect={std::numeric_limits<std::int64_t>::min(),0,std::numeric_limits<std::int64_t>::max(),100};check(!e::group_layout_readiness(overflow).ready);
    std::cout<<"group-topology-preview failures="<<failures<<'\n';return failures?1:0;
}
