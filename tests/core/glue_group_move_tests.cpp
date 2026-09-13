#include "core/behavior/glue_group_move.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace b = panebind::core::behavior;
namespace m = panebind::core::model;
namespace t = panebind::core::topology;
namespace v = panebind::core::movement;
using panebind::core::geometry::Rect;
int failures{};
void expect(bool value, const char* description) {
    if (!value) { ++failures; std::cerr << "FAIL " << description << '\n'; }
}
std::vector<b::GlueGroupMember> members() {
    return {{m::WindowId{"A"}, 11}, {m::WindowId{"B"}, 22}, {m::WindowId{"C"}, 33}};
}
std::vector<t::WindowGeometry> layout() {
    return {{m::WindowId{"A"}, {0,0,100,100}}, {m::WindowId{"B"}, {100,0,200,100}},
            {m::WindowId{"C"}, {0,100,100,200}}};
}
auto batch_for(b::GlueGroupMoveCoordinator& group) {
    auto live = layout();
    expect(group.start(members()[0], true, 10, live, {}, 3), "start");
    auto batch = group.plan(members()[0], 11, Rect{10,20,110,120});
    expect(batch.has_value(), "one batch");
    return *batch;
}
int main() {
    const auto authorized = members();
    {
        b::GlueGroupMoveCoordinator group{authorized, 77};
        auto live = layout();
        for (std::size_t i=0; i<3; ++i) {
            const auto sequence = 100 * (i+1);
            expect(group.start(authorized[i], true, sequence, live, {}, 3), "dynamic START");
            expect(group.group_generation()==77 && group.gesture_generation()==i+1, "stable group monotonic gesture");
            expect(*group.gesture_leader()==authorized[i].id, "actual source leads");
            const auto before = live;
            const auto target = v::translate_rect(live[i].visible_rect, {10,20});
            auto batch = group.plan(authorized[i], sequence+1, target);
            expect(batch && batch->followers.size()==2, "single two-follower batch");
            expect(!group.plan(authorized[(i+1)%3], sequence+2, target), "follower cannot drive");
            expect(group.register_batch(*batch, sequence+2), "all expectations registered");
            expect(group.pending().size()==2, "per member ledger");
            expect(group.postverify(*batch, true, batch->followers), "postverify exact");
            const auto feedback_order=i%2?std::array{1U,0U}:std::array{0U,1U};
            for (const auto index : feedback_order) {
                const auto& operation = batch->followers[index];
                const auto member = *std::find_if(authorized.begin(), authorized.end(), [&](const auto& a){return a.id==operation.id;});
                expect(group.feedback(member,77,i+1,1,sequence+3+index,operation.target_visible_rect)==b::GroupFeedbackResult::Acknowledged,"out of order member ACK");
                expect(group.feedback(member,77,i+1,1,sequence+5+index,operation.target_visible_rect)==b::GroupFeedbackResult::Duplicate,"duplicate separate from ACK");
            }
            for (auto& w:live) w.visible_rect=v::translate_rect(w.visible_rect,{10,20});
            expect(group.finish(authorized[i],sequence+10,live),"END exact");
            expect(group.state()==b::GlueGroupState::GroupReady && !group.gesture_leader() && group.pending().empty(),"all roles/pending cleared");
            for (std::size_t j=0;j<3;++j) expect(live[j].visible_rect==v::translate_rect(before[j].visible_rect,{10,20}),"rigid total displacement");
        }
    }
    for (std::size_t i=0;i<3;++i) {
        b::GlueGroupMoveCoordinator group{authorized,1};
        expect(!group.start(authorized[i],false,1,layout()),"plain drag no activation");
        expect(!group.plan(authorized[i],2,Rect{1,1,101,101}),"late Ctrl cannot create gesture");
        expect(group.state()==b::GlueGroupState::GroupReady,"plain remains ready");
    }
    for (bool missing_b : {false,true}) {
        b::GlueGroupMoveCoordinator group{authorized,77};
        auto batch=batch_for(group);
        expect(group.register_batch(batch,12),"missing register");
        expect(group.postverify(batch,true,batch.followers),"missing exact result");
        const auto index=missing_b?1U:0U;
        expect(group.feedback(authorized[index+1],77,1,1,13,batch.followers[index].target_visible_rect)==b::GroupFeedbackResult::Acknowledged,"other member ACK");
        auto live=layout(); for(auto& w:live) w.visible_rect=v::translate_rect(w.visible_rect,{10,20});
        expect(group.finish(authorized[0],20,live),"missing exact final reconciliation");
        expect(group.reconciled_missing()==1,"one missing not invented");
        expect(group.start(authorized[1],true,30,live,{},3),"same authority next gesture");
        auto next=group.plan(authorized[1],31,v::translate_rect(live[1].visible_rect,{5,5}));
        expect(group.register_batch(*next,32),"next register");
        expect(group.postverify(*next,true,next->followers),"next postverify");
        expect(group.feedback(authorized[2],77,1,1,40,batch.followers[1].target_visible_rect)==b::GroupFeedbackResult::Stale,"old gesture late delivery rejected");
        expect(group.feedback(authorized[2],77,2,1,20,next->followers[1].target_visible_rect)==b::GroupFeedbackResult::Stale,"old receipt cannot gain new generation");
        expect(!group.pending()[1].feedback_observed,"old receipt never ACK next");
    }
    {
        auto live=layout(); live[2].visible_rect=Rect{200,100,300,200};
        b::GlueGroupMoveCoordinator group{authorized,1};
        expect(group.start(authorized[0],true,1,live),"corner-only member excluded");
        auto batch=group.plan(authorized[0],2,Rect{5,5,105,105});
        expect(batch && batch->followers.size()==1 && batch->followers[0].id==authorized[1].id,"no disconnected move");
        b::GlueGroupMoveCoordinator gate{authorized,1};
        expect(!gate.start(authorized[0],true,1,live,{},3),"live exact three rejects pair fallback");
        live.push_back({m::WindowId{"D"},{0,-100,100,0}});
        b::GlueGroupMoveCoordinator unauthorized{authorized,1};
        expect(!unauthorized.start(authorized[0],true,1,live),"unauthorized D rejected before graph");
    }
    for (int failure=0;failure<8;++failure) {
        b::GlueGroupMoveCoordinator group{authorized,77};
        auto batch=batch_for(group);
        expect(group.register_batch(batch,12),"negative registered");
        if (failure==0) { expect(!group.postverify(batch,false,batch.followers),"native failure abort"); }
        else if (failure==1 || failure==7) {
            auto wrong=batch.followers; wrong[1].target_visible_rect=Rect{3,3,103,103};
            if(failure==7)wrong[0].target_visible_rect=Rect{4,4,104,104};
            expect(!group.postverify(batch,true,wrong),"second mismatch abort all");
        } else {
            expect(group.postverify(batch,true,batch.followers),"negative exact");
            if(failure==2) expect(group.feedback(authorized[1],77,1,9,13,batch.followers[0].target_visible_rect)==b::GroupFeedbackResult::Rejected,"wrong batch");
            if(failure==3) expect(group.feedback(authorized[1],77,1,1,13,batch.followers[1].target_visible_rect)==b::GroupFeedbackResult::Rejected,"B cannot ACK C");
            if(failure==4) expect(!group.start(authorized[1],true,14,layout()),"concurrent member START");
            if(failure==5) expect(!group.finish(authorized[1],20,layout()),"follower END rejected");
            if(failure==6) expect(!group.plan(authorized[0],20,Rect{0,0,200,100}),"resize abort");
        }
        expect(group.state()==b::GlueGroupState::GroupPoisoned,"terminal no more writes");
    }
    std::cout << "glue-group-move failures=" << failures << '\n';
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
