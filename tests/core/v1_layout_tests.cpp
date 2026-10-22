#include "core/v1/layout.h"
#include "core/geometry/magnet_constraint_solver.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace v=panebind::core::v1;
using v::Rect;
int failures{}, checks{};
void expect(bool value,const char* message) {
    ++checks;
    if (!value) { ++failures; std::cerr<<"FAIL "<<message<<'\n'; }
}
Rect actual(const v::Plan& p,v::Key id,Rect unchanged={}) {
    const auto i=std::find_if(p.placements.begin(),p.placements.end(),[id](const auto& w){return w.id==id;});
    return i==p.placements.end()?unchanged:i->visible;
}
v::Item item(v::Key id,Rect r,std::int64_t minw=1,std::int64_t minh=1) {
    return {id,r,{minw,minh,2147483647,2147483647,1,1}};
}
void translation_and_graph() {
    const std::array pair{item(1,{100,100,500,400}),item(2,{500,100,900,400})};
    const auto g02=v::move_group(pair,1,{-125,60},0);
    expect(g02.accepted && g02.placements.size()==2,"G02 pair accepted");
    expect(actual(g02,1)==Rect{-25,160,375,460} && actual(g02,2)==Rect{375,160,775,460},"G02 independent total translation");
    const auto repeated=v::move_group(pair,2,{-125,60},0);
    expect(actual(repeated,1)==actual(g02,1) && actual(repeated,2)==actual(g02,2),"G02 uses immutable origin not cumulative delta");
    const std::array tee{item(1,{100,100,500,500}),item(2,{500,100,900,300}),
                         item(3,{500,300,900,500}),item(4,{900,500,1100,700})};
    expect(v::connected(tee,1,0)==std::vector<v::Key>{1,2,3},"G05 positive-length AB AC BC excludes D corner");
    for (const auto source:{1U,2U,3U}) {
        const auto moved=v::move_group(tee,source,{40,-20},0);
        expect(moved.accepted && moved.placements.size()==3,"G05 dynamic source freezes component");
        expect(actual(moved,1)==Rect{140,80,540,480} && actual(moved,2)==Rect{540,80,940,280} &&
               actual(moved,3)==Rect{540,280,940,480} && actual(moved,4)==Rect{},"G05 independent T translation excludes corner member");
    }
    std::array<v::Item,8> eight;
    for (std::size_t i=0;i<eight.size();++i) eight[i]=item(i+1,{static_cast<std::int64_t>(i)*100,0,static_cast<std::int64_t>(i+1)*100,100});
    const auto moved=v::move_group(eight,8,{-1000,50},0);
    expect(moved.accepted && moved.placements.size()==8 && actual(moved,1)==Rect{-1000,50,-900,150} &&
           actual(moved,8)==Rect{-300,50,-200,150},"8 members and negative coordinate rigid move");
    auto duplicate=pair; duplicate[1].id=1;
    expect(!v::move_group(duplicate,1,{1,0},0).accepted,"duplicate identities reject");
    expect(!v::move_group(pair,9,{1,0},0).accepted && !v::move_group(pair,1,{1,0},-1).accepted,"unknown source and negative tolerance reject");
    const auto huge=std::numeric_limits<std::int64_t>::max();
    const std::array extreme{item(1,{huge-100,0,huge,100})};
    expect(!v::move_group(extreme,1,{1,0},0).accepted,"translate overflow rejects whole plan");
    const std::array invalid{item(1,{std::numeric_limits<std::int64_t>::min(),0,huge,100})};
    expect(!v::move_group(invalid,1,{0,0},0).accepted,"unrepresentable size rejected before Rect width arithmetic");
}
void shared_resize() {
    const v::Edges right{false,false,true,false},bottom{false,false,false,true};
    std::array pair{item(1,{100,100,500,500},250),item(2,{500,100,900,500},300)};
    auto p=v::resize_group(pair,1,right,{80,0},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,580,500} && actual(p,2)==Rect{580,100,900,500},"G06 plus80 shared line580");
    p=v::resize_group(pair,1,right,{180,0},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,600,500} && actual(p,2)==Rect{600,100,900,500},"G06 plus180 clamp600");
    p=v::resize_group(pair,2,{true,false,false,false},{-500,0},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,350,500} && actual(p,2)==Rect{350,100,900,500},"G06 reverse source same lower350");
    std::array horizontal{item(1,{100,100,500,500},1,250),item(2,{100,500,500,900},1,300)};
    p=v::resize_group(horizontal,1,bottom,{0,180},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,500,600} && actual(p,2)==Rect{100,600,500,900},"G06 horizontal transposed min limits");
    pair[1].limits.max_width=400; pair[1].limits.min_width=400;
    p=v::resize_group(pair,1,right,{80,0},0);
    expect(p.accepted && p.placements.empty(),"fixed-size neighbor clamps whole line to original");
    pair[0].limits.max_width=300;
    expect(!v::resize_group(pair,1,right,{80,0},0).accepted,"empty min/max shared interval rejects");
    const std::array tee{item(1,{100,100,500,500}),item(2,{500,100,900,300}),item(3,{500,300,900,500})};
    p=v::resize_group(tee,2,{true,false,false,false},{60,0},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,560,500} && actual(p,2)==Rect{560,100,900,300} &&
           actual(p,3)==Rect{560,300,900,500},"G07 T split560 all contacts y300 unchanged");
    const std::array ell{tee[0],tee[1]};
    p=v::resize_group(ell,1,right,{60,0},0);
    expect(p.accepted && p.placements.size()==2 && actual(p,1)==Rect{100,100,560,500} &&
           actual(p,2)==Rect{560,100,900,300},"G07 L no invented member or fill-hole");
    auto corner=ell;
    corner[0].limits.max_width=420; corner[1].limits.min_width=370;
    p=v::resize_group(corner,2,{true,false,false,true},{80,30},0);
    // X intersection [350,520] clamps requested580; Y remains requested330.
    // These independent constants do not use the returned solver plan.
    expect(p.accepted && p.placements.size()==2 && actual(p,1)==Rect{100,100,520,500} &&
           actual(p,2)==Rect{520,100,900,330},"G07 L sourceB bottom-left X max420 clamp520 preserves independent Y330 and missing C");
    const std::array four{item(1,{100,100,500,300}),item(2,{500,100,900,300}),
                          item(3,{100,300,500,500}),item(4,{500,300,900,500})};
    p=v::resize_group(four,1,{false,false,true,true},{40,30},0);
    expect(p.accepted && p.placements.size()==4,"G07 four-grid both axes accepted once");
    expect(actual(p,1)==Rect{100,100,540,330} && actual(p,2)==Rect{540,100,900,330} &&
           actual(p,3)==Rect{100,330,540,500} && actual(p,4)==Rect{540,330,900,500},"G07 independent four-grid exact shared node540330");
    auto limited=four;
    limited[0].limits.max_width=420; limited[1].limits.min_width=370;
    limited[2].limits.max_height=190; limited[3].limits.min_height=180;
    p=v::resize_group(limited,1,{false,false,true,true},{80,80},0);
    expect(p.accepted && actual(p,1)==Rect{100,100,520,320} && actual(p,2)==Rect{520,100,900,320} &&
           actual(p,3)==Rect{100,320,520,500} && actual(p,4)==Rect{520,320,900,500},"four-grid independent axes intersection min/max limits");
    // The lower contact has the same x but is not contiguous. A far-left
    // bridge makes it part of the overall graph without joining split lines.
    const std::array separate{item(1,{100,100,500,300}),item(2,{500,100,900,300}),
                              item(3,{100,500,500,700}),item(4,{500,500,900,700}),
                              item(5,{0,100,100,700})};
    p=v::resize_group(separate,1,right,{40,0},0);
    expect(p.accepted && p.placements.size()==2 && actual(p,3)==Rect{} && actual(p,4)==Rect{},"collinear disjoint contacts not resized through unrelated graph bridge");
    std::array grids{item(1,{0,0,400,200}),item(2,{400,0,1000,200})};
    grids[0].limits.grid_width=20; grids[1].limits.grid_width=30;
    p=v::resize_group(grids,1,right,{31,0},0);
    expect(p.accepted && actual(p,1)==Rect{0,0,460,200} && actual(p,2)==Rect{460,0,1000,200},"shared line combines width20 and30 congruences");
    grids[1].visible={400,0,1001,200}; grids[1].limits.grid_width=20;
    expect(!v::resize_group(grids,1,right,{10,0},0).accepted,"incompatible shared size grid rejects not independently rounded");
    auto overlap=pair; overlap[1].visible={490,100,900,500};
    expect(!v::resize_group(overlap,1,right,{5,0},20).accepted,"positive-area overlapping layout rejected");
    expect(!v::resize_group(four,1,{true,false,true,false},{5,0},0).accepted,"ambiguous opposing active edges rejected");
    namespace m=panebind::core::magnet;
    const auto g04_source=item(1,{100,100,490,400},300);
    const auto g04_plan=v::resize_single(g04_source,right,{104,0});
    expect(g04_plan.accepted && actual(g04_plan,1)==Rect{100,100,594,400},"G04 production Resize plan preserves the three inactive edges");
    const std::array g04_targets{m::MagnetTarget{panebind::core::model::WindowId{"G04-neighbor"},{600,100,1000,500},false}};
    m::MagnetInput g04_request;
    g04_request.initial=g04_source.visible;g04_request.raw=actual(g04_plan,1);g04_request.targets=g04_targets;
    g04_request.interaction=m::Interaction::Resize;g04_request.edges={false,false,true,false};g04_request.latched_resize=true;
    g04_request.options.attraction_distance=10;g04_request.options.max_targets=256;
    // Independent 104px / 100ms sample is below the 2000px/s policy.
    g04_request.motion=m::MotionSample{g04_source.visible,100,200,1000,2000};
    const auto g04=m::MagnetConstraintSolver::solve(g04_request);
    expect(g04.proposal && g04.proposal->corrected==Rect{100,100,600,400} &&
        g04.proposal->move_delta==panebind::core::geometry::Point{} &&
        g04.proposal->edges==m::ParticipatingEdges{false,false,true,false} &&
        g04.proposal->selected_x && !g04.proposal->selected_y,"G04 exact594 to600 Resize magnet preserves left/top/bottom");
    auto single=item(1,{100,100,490,400},300);
    p=v::resize_single(single,right,{-290,0});
    expect(p.accepted && actual(p,1)==Rect{100,100,400,400},"G04 single active-edge min300 clamped without fixed-edge drift");
    single.limits.max_width=200;
    expect(!v::resize_single(single,right,{5,0}).accepted,"invalid dimension interval rejects");
}
void docking_and_stretch() {
    const auto restored=v::restored_move_anchor({0,0,960,1040},{100,100,700,500},{480,20});
    expect(restored&&*restored==Rect{180,0,780,400},"drag-out restoration preserves original down title offset and proportional X anchor");
    const auto repeated_anchor=v::restored_move_anchor({0,0,960,1040},{100,100,700,500},{480,20});
    expect(restored==repeated_anchor,"restoration intent anchor independent of corrected later rectangles");
    const auto negative_anchor=v::restored_move_anchor({-1920,0,-960,1040},{-1600,100,-1000,500},{-1440,20});
    expect(negative_anchor&&*negative_anchor==Rect{-1740,0,-1140,400},"drag-out original anchor supports negative monitor coordinates");
    const Rect work{0,0,1919,1039};
    expect(v::dock(work,v::Action::DockLeft)==Rect{0,0,959,1039} &&
           v::dock(work,v::Action::DockRight)==Rect{959,0,1919,1039},"G09 odd half split x959 no gap");
    expect(v::dock(work,v::Action::DockTop)==Rect{0,0,1919,519} &&
           v::dock(work,v::Action::DockBottom)==Rect{0,519,1919,1039},"G09 odd half split y519 no gap");
    expect(v::dock(work,v::Action::DockTopLeft)==Rect{0,0,959,519} &&
           v::dock(work,v::Action::DockTopRight)==Rect{959,0,1919,519} &&
           v::dock(work,v::Action::DockBottomLeft)==Rect{0,519,959,1039} &&
           v::dock(work,v::Action::DockBottomRight)==Rect{959,519,1919,1039},"G09 all four quarter independent rectangles");
    expect(v::dock({-1920,0,0,1040},v::Action::DockLeft)==Rect{-1920,0,-960,1040},"G09 negative monitor signed split");
    const Rect full{0,0,1920,1040};
    const std::array left{item(1,{0,0,700,1040})};
    expect(v::smart_dock(full,v::Action::DockRight,left,true)==Rect{700,0,1920,1040},"G10 smart dock fills remainder700");
    expect(v::smart_dock(full,v::Action::DockRight,left,false)==Rect{960,0,1920,1040},"G10 disabled smart fixed ordinary half");
    const std::array distant{item(1,{-1000,0,-100,1040}),item(2,{0,1200,700,1500})};
    expect(v::smart_dock(full,v::Action::DockRight,distant,true)==Rect{960,0,1920,1040},"G10 distant or no-overlap windows do not change split");
    const Rect initial{300,200,700,600};
    const std::array blockers{item(1,{1000,100,1400,800}),item(2,{800,700,900,900})};
    expect(v::stretch(initial,full,{false,false,true,false},false,blockers,true)==Rect{300,200,1000,600},"G11 right nearest orthogonal blocker");
    expect(v::stretch(initial,full,{false,false,true,false},true,blockers,true)==Rect{0,200,1000,600},"G11 Shift right extends both horizontal ends");
    expect(v::stretch(initial,full,{false,true,true,false},false,blockers,true)==Rect{300,0,1000,600},"G11 righttop grows two axes same rectangle");
    expect(v::stretch(initial,full,{false,false,true,false},false,blockers,false)==Rect{300,200,1920,600},"smart stretch disabled ignores neighbors");
    const std::array<v::Item,0> none{};
    const std::array<v::Edges,4> sides{{{true,false,false,false},{false,true,false,false},{false,false,true,false},{false,false,false,true}}};
    const std::array<Rect,4> expected{{{0,200,700,600},{300,0,700,600},{300,200,1920,600},{300,200,700,1040}}};
    for (std::size_t i=0;i<sides.size();++i) expect(v::stretch(initial,full,sides[i],false,none,true)==expected[i],"four single edges preserve other three");
    const std::array<v::Edges,4> corners{{{true,true,false,false},{false,true,true,false},{true,false,false,true},{false,false,true,true}}};
    const std::array<Rect,4> corners_expected{{{0,0,700,600},{300,0,1920,600},{0,200,700,1040},{300,200,1920,1040}}};
    for (std::size_t i=0;i<corners.size();++i) expect(v::stretch(initial,full,corners[i],false,none,true)==corners_expected[i],"four corners preserve unselected sides");
    expect(v::stretch(initial,full,{false,true,false,false},true,none,true)==Rect{300,0,700,1040},"Shift top extends vertical axis only");
    const std::array nearer{item(1,{1200,100,1400,800}),item(2,{900,100,1000,700}),item(3,{750,600,800,700})};
    expect(v::stretch(initial,full,{false,false,true,false},false,nearer,true)==Rect{300,200,900,600},"nearest positive overlap not corner contact or farther window");
    const auto huge=std::numeric_limits<std::int64_t>::max();
    expect(v::dock({std::numeric_limits<std::int64_t>::min(),0,huge,100},v::Action::DockLeft)==Rect{},"dock overflow rejected");
}
void adjacent_displays() {
    const std::array scrambled{Rect{0,0,1920,1080},Rect{3840,0,5760,1080},Rect{1920,0,3840,1080}};
    const auto next=v::nearest_adjacent_display(scrambled,0);
    expect(next&&scrambled[*next]==Rect{1920,0,3840,1080},"command must not skip the actual neighbour because of enumeration order");
    const std::array reordered{scrambled[2],scrambled[0],scrambled[1]};
    const auto same=v::nearest_adjacent_display(reordered,1);
    expect(same&&reordered[*same]==scrambled[2],"enumeration permutation keeps the same destination");
    const std::array negative{Rect{0,0,1920,1080},Rect{1920,0,3840,1080},Rect{-1920,0,0,1080}};
    const auto left=v::nearest_adjacent_display(negative,0);
    expect(left&&negative[*left]==negative[2],"equal nearest contacts use signed geometry, not enumeration order");
    const std::array contact{Rect{0,0,100,100},Rect{100,100,200,200},Rect{100,0,200,100}};
    expect(v::nearest_adjacent_display(contact,0)==2,"positive face contact beats a corner-only neighbour");
    const std::array<Rect,1> one{Rect{0,0,100,100}};
    expect(!v::nearest_adjacent_display(one,0)&&!v::nearest_adjacent_display(contact,9),"missing display or stale source index rejects selection");
    auto invalid=contact;invalid[1]={0,0,0,100};
    expect(!v::nearest_adjacent_display(invalid,0),"invalid topology cannot manufacture a destination");
    const std::array duplicate{Rect{0,0,100,100},Rect{100,0,200,100},Rect{100,0,200,100}};
    expect(!v::nearest_adjacent_display(duplicate,0),"indistinguishable monitor bounds reject rather than select an enumeration-dependent identity");
}
int main() {
    translation_and_graph(); shared_resize(); docking_and_stretch(); adjacent_displays();
    if (failures) { std::cerr<<failures<<'/'<<checks<<" failed\n"; return EXIT_FAILURE; }
    std::cout<<"V1 layout "<<checks<<" checks passed\n";
    return EXIT_SUCCESS;
}
