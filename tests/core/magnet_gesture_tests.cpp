#include "core/behavior/magnet_gesture.h"
#include <iostream>
namespace b=panebind::core::behavior;
namespace m=panebind::core::magnet;
namespace t=panebind::core::topology;
using panebind::core::geometry::Rect;
using panebind::core::model::WindowId;
namespace {
int failures{},checks{};
void check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
std::vector<b::MagnetMember> members(Rect source={150,20,250,160}) {
    return {{WindowId{"A"},11,5,{0,0,120,180}},{WindowId{"B"},22,6,source},{WindowId{"C"},33,7,{20,180,300,300}}};
}
auto start(b::MagnetGestureCoordinator& c,bool ctrl=false,Rect initial={150,20,250,160}) {
    auto all=members(initial);check(c.start(55,1,1,ctrl,1,1000,1000,all),"START");return all;
}
void commit(b::MagnetGestureCoordinator& c,const b::MagnetCorrection& op,std::uint64_t watermark,std::uint64_t tick){
    check(c.register_pending(op,watermark,tick),"pending before native");
    check(c.postverify(op.operation,true,op.proposal.corrected,true),"exact postverify");
}
}
int main(){
    for(bool ctrl:{false,true}){b::MagnetGestureCoordinator c;auto all=start(c,ctrl);
        check(!c.sample(2,110,1100,all[1].visible)&&c.route()==b::MagnetRoute::Classifying,"unchanged stays classifying");
        auto op=c.sample(3,200,1200,{126,36,226,176});
        check(c.route()==(ctrl?b::MagnetRoute::GlueMove:b::MagnetRoute::MagnetMove),"latched Ctrl routing");
        if(ctrl)check(!op&&c.counters().solver_calls==0&&c.counters().corrections==0,"Glue never runs Magnet solver");
        else {check(op&&op->proposal.move_delta.x==-6&&op->proposal.move_delta.y==4,"one XY correction");
            if(op){commit(c,*op,4,201);const auto calls=c.counters().solver_calls;
                check(!c.sample(5,202,1210,op->proposal.corrected)&&c.last_sample_result()=="acknowledged","matching self feedback");
                check(!c.sample(6,203,1220,op->proposal.corrected)&&c.last_sample_result()=="duplicate"&&c.counters().solver_calls==calls,"duplicate not raw input");
                all[1].visible=op->proposal.corrected;check(c.finish(7,all),"exact END");
                check(!c.start(55,1,1,false,8,1300,1000,all),"gesture generation cannot replay");
            }
        }
    }
    for(int edge=0;edge<8;++edge){
        const Rect initial{150,40,250,160};auto raw=initial;
        const bool l=edge==0||edge==4||edge==6,r=edge==1||edge==5||edge==7;
        const bool top=edge==2||edge==4||edge==5,bottom=edge==3||edge==6||edge==7;
        raw={initial.left()-(l?4:0),initial.top()-(top?4:0),initial.right()+(r?4:0),initial.bottom()+(bottom?4:0)};
        for(bool ctrl:{false,true}){b::MagnetGestureCoordinator c;start(c,ctrl,initial);const auto op=c.sample(2,200,1200,raw);
            check(c.route()==(ctrl?b::MagnetRoute::CtrlResizeUnsupported:b::MagnetRoute::MagnetResize),"four edges/four corners classify");
            check(c.edges()==m::ParticipatingEdges{l,top,r,bottom},"latched edge mask");
            if(ctrl)check(!op&&c.reason()=="ctrl_resize_not_implemented"&&c.counters().solver_calls==0,"Ctrl Resize zero native proposals");
        }
    }
    {b::MagnetGestureCoordinator c;start(c);check(!c.sample(2,200,1200,{149,20,251,160})&&c.route()==b::MagnetRoute::Aborted,"ambiguous resize abort");}
    {b::MagnetGestureCoordinator c;start(c);c.sample(2,200,1200,{151,20,251,160});check(!c.sample(3,300,1400,{151,20,252,160})&&c.route()==b::MagnetRoute::Aborted,"Move size conflict");}
    {b::MagnetGestureCoordinator c;start(c);c.sample(2,200,1200,{150,20,254,160});check(!c.sample(3,300,1400,{149,20,254,160})&&c.route()==b::MagnetRoute::Aborted,"Resize opposite edge conflict");}
    for(int fault=0;fault<8;++fault){b::MagnetGestureCoordinator c;auto all=start(c);auto op=c.sample(2,200,1200,{126,36,226,176});check(op.has_value(),"feedback fixture proposal");if(!op)continue;
        commit(c,*op,4,201);
        auto member=all[fault==0?0:1];if(fault==6)++member.capability;if(fault==7)++member.consent;
        const auto status=c.feedback(true,member,fault==1?56:55,fault==2?2:1,fault==3?99:1,fault==4?4:5,202,fault==5?Rect{0,0,10,10}:op->proposal.corrected);
        check(status!="acknowledged"&&status!="duplicate","wrong member/group/gesture/op/watermark/geometry reject");
    }
    for(bool exact:{false,true}){b::MagnetGestureCoordinator c;auto all=start(c);auto op=c.sample(2,200,1200,{126,36,226,176});if(!op)continue;commit(c,*op,3,201);
        all[1].visible=exact?op->proposal.corrected:Rect{121,40,221,180};
        if(exact){check(!c.sample(4,300,1400,op->proposal.corrected,false)&&c.counters().acknowledged==0,"END snapshot is reconciliation, not self LOCATION ACK");}
        check(c.finish(4,all)==exact,"missing feedback exact END only");if(exact)check(c.counters().missing==1,"missing receipt counted");
    }
    {b::MagnetGestureCoordinator c;auto all=start(c);all[0].visible={1,0,121,180};check(!c.validate_members(all)&&c.route()==b::MagnetRoute::Aborted,"frozen target mutation abort");}
    {b::MagnetGestureCoordinator c;start(c);auto op=c.sample(2,200,1200,{126,36,226,176});if(op){commit(c,*op,3,201);check(!c.sample(4,202,1400,op->raw)&&c.route()==b::MagnetRoute::Aborted&&c.counters().corrections==1,"raw reassertion cannot cause write storm");}}
    {b::MagnetGestureCoordinator c;start(c);auto op=c.sample(2,200,1200,{129,20,229,160});if(op){commit(c,*op,3,201);auto hold=c.sample(4,300,1400,{133,20,233,160});check(hold&&hold->proposal.corrected.left()==120,"hysteresis 13 holds");if(hold)commit(c,*hold,5,301);
        auto hold15=c.sample(6,400,1600,{135,20,235,160});check(hold15&&hold15->proposal.corrected.left()==120,"hysteresis 15 holds");if(hold15)commit(c,*hold15,7,401);
        check(!c.sample(8,500,1800,{137,20,237,160})&&c.counters().latch_releases>0,"17 releases");
        auto back=c.sample(9,600,2000,{128,20,228,160});check(back&&back->proposal.corrected.left()==120,"8 reacquires");}}
    {b::MagnetGestureCoordinator c;start(c);auto op=c.sample(2,200,1200,{129,20,229,160});if(op){commit(c,*op,3,201);check(!c.sample(4,300,1201,{135,20,235,160})&&c.counters().motion_suppressed==1&&c.counters().latch_releases>0,"fast motion clears latch");}}
    {m::MagnetOptions opts;std::array targets{m::MagnetTarget{WindowId{"T"},{0,0,100,100},false}};
        m::MagnetInput input{{109,30,169,90},{109,30,169,90},m::Interaction::Move,{},targets,opts,{}};
        auto r=m::MagnetConstraintSolver::solve(input);check(r.proposal&&r.proposal->selected_x,"solver explicit selected X");
        input.preferred_x=m::MagnetAxisPreference{WindowId{"T"},m::ConstraintKind::WindowOuterEdge,panebind::core::geometry::Edge::Left,panebind::core::geometry::Edge::Right,16};
        input.raw=input.initial={115,30,175,90};check(m::MagnetConstraintSolver::solve(input).proposal.has_value(),"solver preferred outside enter");
        input.targets={};check(!m::MagnetConstraintSolver::solve(input).proposal,"disappeared candidate releases");
    }
    {b::MagnetGestureCoordinator c;start(c);const auto op=c.sample(2,200,1200,{116,20,216,160});
        check(op&&op->proposal.move_delta.x==4&&op->proposal.corrected.left()==120,"negative gap shallow overlap snaps outward");}
    {std::array targets{m::MagnetTarget{WindowId{"A"},{0,0,120,180},false},m::MagnetTarget{WindowId{"C"},{20,180,300,300},false}};
        m::MagnetInput in{{129,31,229,171},{129,31,229,171},m::Interaction::Move,{},targets,{},{}};
        auto r=m::MagnetConstraintSolver::solve(in);check(r.proposal&&r.proposal->selected_x&&r.proposal->selected_y,"XY acquire");
        if(r.proposal){const auto& x=*r.proposal->selected_x;const auto& y=*r.proposal->selected_y;
            in.preferred_x=m::MagnetAxisPreference{x.target,x.kind,x.moving_edge,x.target_edge,16};
            in.preferred_y=m::MagnetAxisPreference{y.target,y.kind,y.moving_edge,y.target_edge,16};
            in.initial=in.raw={133,27,233,167};r=m::MagnetConstraintSolver::solve(in);
            check(r.proposal&&r.proposal->corrected==Rect{120,40,220,180},"XY hold at 13");
            in.preferred_x.reset();in.initial=in.raw={150,27,250,167};r=m::MagnetConstraintSolver::solve(in);
            check(r.proposal&&r.proposal->corrected.bottom()==180&&!r.proposal->selected_x,"Y-only hold");
            in.initial=in.raw={150,23,250,163};check(!m::MagnetConstraintSolver::solve(in).proposal,"Y release at 17");
        }
    }
    {b::MagnetGestureCoordinator c;auto all=start(c);all[2].capability++;check(!c.validate_members(all),"target capability invalidation");}
    {b::MagnetGestureCoordinator c;start(c);auto op=c.sample(2,200,1200,{126,36,226,176});if(op){auto wrong=*op;wrong.capability++;
        check(!c.register_pending(wrong,3,201)&&c.pending().empty(),"wrong permit never registers pending");
        check(c.register_pending(*op,3,201),"correct permit registers");
        check(!c.postverify(op->operation,true,op->proposal.corrected,false),"positioning mismatch aborts despite visible exact");}}
    {std::array targets{m::MagnetTarget{WindowId{"A"},{0,0,120,180},false}};
        m::MagnetInput in{{120,6,220,180},{120,6,220,180},m::Interaction::Resize,{false,true,false,false},targets,{},{}};
        check(!m::MagnetConstraintSolver::solve(in).proposal,"unlatched unchanged resize still rejected");
        in.latched_resize=true;const auto out=m::MagnetConstraintSolver::solve(in);
        check(out.proposal&&out.proposal->corrected==Rect{120,0,220,180},"classified resize may return to its start rect");
        in.raw={119,6,220,180};check(!m::MagnetConstraintSolver::solve(in).proposal,"latched mode still rejects nonparticipating edge drift");
    }
    {b::MagnetGestureCoordinator c;auto all=start(c);auto op=c.sample(2,200,1200,{126,36,226,176});if(op){
        c.discard_superseded_proposal();check(c.pending().empty()&&c.counters().corrections==0,"superseded proposal does not register or write");
        auto next=c.sample(3,300,1400,{126,36,226,176});check(next&&next->operation>op->operation,"fresh receipt may retry discarded raw with new operation generation");
    }}
    std::cout<<"magnet-gesture checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
