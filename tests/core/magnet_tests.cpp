#include "core/geometry/magnet_constraint_solver.h"
#include <iostream>

namespace m=panebind::core::magnet;
namespace t=panebind::core::topology;
namespace g=panebind::core::geometry;
using panebind::core::model::WindowId;
using g::Rect;
namespace {
int failures{},checks{};
void check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
m::MagnetTarget target(const char* id,Rect r,bool screen=false){return {WindowId{id},r,screen};}
m::MagnetResult move(Rect raw,std::span<const m::MagnetTarget> targets,m::MagnetOptions options={}) {
    return m::MagnetConstraintSolver::solve({raw,raw,m::Interaction::Move,{},targets,options,{}});
}
m::MagnetResult resize(Rect initial,Rect raw,std::span<const m::MagnetTarget> targets,m::MagnetOptions options={}) {
    return m::MagnetConstraintSolver::solve({initial,raw,m::Interaction::Resize,m::infer_resize_edges(initial,raw).value_or(m::ParticipatingEdges{}),targets,options,{}});
}
bool is(const m::MagnetResult& r,Rect expected){return r.proposal&&r.proposal->corrected==expected;}
t::WindowAdjacencyGraph graph(Rect a,Rect b,Rect c){const std::array<t::WindowGeometry,3> w{{{WindowId{"A"},a},{WindowId{"B"},b},{WindowId{"C"},c}}};return t::WindowAdjacencyGraph::build(w,{});}
bool relation(const t::WindowAdjacencyGraph& g,const char* a,const char* b){return std::any_of(g.relations().begin(),g.relations().end(),[&](const auto& r){return r.first==WindowId{a}&&r.second==WindowId{b};});}
bool kind(const m::MagnetResult& r,m::ConstraintKind k){return r.proposal&&std::any_of(r.proposal->satisfied.begin(),r.proposal->satisfied.end(),[&](const auto& c){return c.kind==k;});}
}
int main(){
    const Rect a{0,0,120,180},b{120,40,220,180},c{20,180,260,270};
    auto three=graph(a,b,c);
    check(relation(three,"A","B"),"A unequal A/B");check(relation(three,"A","C"),"B unequal A/C");
    auto two=graph(a,b,{20,180,120,270});
    check(two.relations().size()==2&&two.connected_component(WindowId{"B"}).size()==3,"C two-edge component");
    check(three.relations().size()==3&&three.connected_component(WindowId{"C"}).size()==3,"D three-edge component");
    check(!relation(two,"B","C"),"E point corner no relation");
    std::array targets{target("A",a),target("C",c)};
    auto r=move({124,30,224,140},targets);
    check(is(r,{120,30,220,140}),"F B to A only");
    if(r.proposal){auto gr=graph(a,r.proposal->corrected,c);check(relation(gr,"A","B")&&!relation(gr,"B","C"),"F actual graph");}
    r=move({150,64,240,174},targets);
    check(is(r,{150,70,240,180}),"G B to C only");
    if(r.proposal){auto gr=graph(a,r.proposal->corrected,c);check(!relation(gr,"A","B")&&relation(gr,"B","C"),"G actual graph");}
    r=move({124,36,224,176},targets);
    check(is(r,b)&&r.proposal->move_delta==g::Point{-4,4},"H/P one combined XY proposal");
    if(r.proposal)check(graph(a,r.proposal->corrected,c).relations().size()==3,"H all three actual relations");
    r=resize({120,40,220,150},{120,40,220,175},targets);
    check(is(r,b)&&r.proposal->move_delta==g::Point{}&&r.proposal->edges.bottom,"I B.bottom only");
    r=resize(b,{120,5,220,180},targets);
    check(is(r,{120,0,220,180})&&r.proposal->edges.top&&!r.proposal->edges.bottom,"J B.top keeps bottom");
    r=resize({140,40,220,170},{124,40,220,170},targets);
    check(is(r,{120,40,220,170}),"left resize preserves right and both Y edges");
    r=resize({120,40,200,150},{120,40,255,175},targets);
    check(is(r,{120,40,260,180})&&r.proposal->edges.right&&r.proposal->edges.bottom,"corner resize preserves left/top");
    check(!relation(graph(a,b,{20,180,120,270}),"B","C"),"K actual C.right shrink detaches");
    check(!relation(graph(a,b,{20,180,119,270}),"B","C"),"K negative overlap detaches");
    std::array detach_targets{target("A",a),target("B",b)};
    r=resize(c,{20,180,120,270},detach_targets);
    check(is(r,{20,180,120,270})&&!relation(graph(a,b,r.proposal->corrected),"B","C"),"K resize proposal then actual graph detaches");
    std::array stacked{target("stack",{0,0,200,250})};
    r=move({4,45,104,145},stacked);check(is(r,{0,45,100,145})&&kind(r,m::ConstraintKind::WindowInnerEdge),"L inner same-side");
    std::array outer{target("outer",{0,0,100,100})};
    r=move({104,25,164,85},outer);check(is(r,{100,25,160,85})&&kind(r,m::ConstraintKind::WindowOuterEdge),"M outer opposing edges");
    r=move({104,104,164,164},outer);check(is(r,{100,100,160,160})&&kind(r,m::ConstraintKind::AdjacentCornerAlignment),"N corner-only alignment");
    if(r.proposal){std::array<t::WindowGeometry,2> w{{{WindowId{"X"},{0,0,100,100}},{WindowId{"Y"},r.proposal->corrected}}};check(t::WindowAdjacencyGraph::build(w,{}).relations().empty(),"N Magnet corner is not relation");}
    auto options=m::MagnetOptions{};options.corners=false;options.inner_edges=false;
    std::array tied{target("Z",{0,0,100,200}),target("A",{108,0,220,200})};
    r=move({102,30,106,170},tied,options);check(is(r,{104,30,108,170}),"O stable logical ID tie");
    const auto first=r;std::reverse(tied.begin(),tied.end());r=move({102,30,106,170},tied,options);
    check(r.proposal&&first.proposal&&r.proposal->corrected==first.proposal->corrected&&r.proposal->satisfied==first.proposal->satisfied,"O target permutation invariant");
    std::array overlapRank{target("A",{0,40,100,60}),target("Z",{108,0,220,200})};
    check(is(move({102,30,106,170},overlapRank,options),{104,30,108,170}),"O overlap precedes ID");
    std::array compatible{target("first",{0,0,100,60}),target("second",{0,70,100,130})};
    r=move({104,20,164,110},compatible,options);check(is(r,{100,20,160,110})&&r.proposal->satisfied.size()==2,"same-axis compatible constraints retained");
    check(!resize({10,10,110,110},{11,10,111,110},outer).proposal,"Q translated resize ambiguous");
    check(!resize({10,10,110,110},{9,10,111,110},outer).proposal,"Q both opposite resize edges ambiguous");
    check(!m::infer_resize_edges(a,a),"Q unchanged resize ambiguous");
    m::MagnetInput wrong{b,{120,40,220,175},m::Interaction::Resize,{true,false,false,false},targets,{},{}};
    check(!m::MagnetConstraintSolver::solve(wrong).proposal,"Q wrong supplied edge fails closed");
    m::MotionSample motion{{114,36,214,176},100,200,1000,100};
    m::MagnetInput input{{124,36,224,176},{124,36,224,176},m::Interaction::Move,{},targets,{},motion};
    check(m::MagnetConstraintSolver::solve(input).proposal.has_value(),"R threshold equality enabled");
    input.motion->current_tick=199;r=m::MagnetConstraintSolver::solve(input);check(!r.proposal&&r.motion==m::MotionState::Suppressed,"R fast suppression");
    input.motion->current_tick=100;r=m::MagnetConstraintSolver::solve(input);check(!r.proposal&&r.motion==m::MotionState::InvalidSample,"R zero elapsed fail closed");
    input.motion->current_tick=200;input.motion->tick_frequency=std::numeric_limits<std::uint64_t>::max();
    check(!m::MagnetConstraintSolver::solve(input).proposal,"R velocity multiplication overflow");
    std::array screen{target("screen",{0,0,1000,800},true)};
    check(is(move({4,37,104,137},screen),{0,37,100,137}),"screen edge correction");
    options=m::MagnetOptions{};options.inner_edges=false;options.corners=false;
    std::array parallel{target("parallel",{0,0,100,100})};
    r=move({4,100,84,180},parallel,options);
    check(is(r,{0,100,80,180})&&kind(r,m::ConstraintKind::ParallelEdgeAlignment),"parallel top boundary alignment");
    options=m::MagnetOptions{};options.attraction_distance=3;
    check(!move({104,25,164,85},outer,options).proposal,"outside field");
    options.max_targets=0;check(!move(b,targets,options).proposal,"zero bound");
    options.max_targets=257;check(!move(b,targets,options).proposal,"hard target bound");
    options.max_targets=1;check(!move(b,targets,options).proposal,"target count bound");
    std::array duplicate{target("same",a),target("same",c)};check(!move(b,duplicate).proposal,"duplicate IDs");
    std::array invalid{target("empty",{0,0,0,100})};check(!move(b,invalid).proposal,"empty target");
    const auto min=std::numeric_limits<g::Coordinate>::min(),max=std::numeric_limits<g::Coordinate>::max();
    check(!move({min,0,max,100},targets).proposal,"overflow extent");
    check(!resize({min,0,min+10,100},{max-10,0,max,100},targets).proposal,"unrepresentable resize");
    std::array crossing{target("cross",{0,0,105,200})};
    options=m::MagnetOptions{};options.inner_edges=false;options.corners=false;options.parallel_edges=false;
    check(!resize({90,40,100,140},{99,40,100,140},crossing,options).proposal,"resize edge cannot cross opposite edge");
    // Deterministic bounded sweep: no Move extent changes, no unsatisfied facts.
    for(int dx=-10;dx<=10;++dx)for(int dy=-10;dy<=10;++dy){
        const Rect raw{120+dx,40+dy,220+dx,180+dy};const auto result=move(raw,targets);
        if(!result.proposal)continue;const auto& p=*result.proposal;
        check(p.corrected.width()==raw.width()&&p.corrected.height()==raw.height(),"sweep rigid translation");
        for(const auto& constraint:p.satisfied){const auto& target=constraint.target==WindowId{"A"}?a:c;
            check(g::edge_coordinate(p.corrected,constraint.moving_edge)==g::edge_coordinate(target,constraint.target_edge),"sweep satisfied equality");}
    }
    std::cout<<"magnet checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
