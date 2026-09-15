#pragma once

#include "core/geometry/checked_arithmetic.h"
#include "core/model/window_id.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"
#include <array>
#include <limits>
#include <tuple>
#include <unordered_set>

namespace panebind::core::magnet {
using geometry::Rect;
using geometry::Edge;
using geometry::Distance;
enum class Interaction { Move, Resize };
enum class ConstraintKind { ScreenOuterEdge, WindowOuterEdge, WindowInnerEdge,
                            AdjacentCornerAlignment, ParallelEdgeAlignment };
enum class MotionState { BelowThreshold, Suppressed, InvalidSample };
struct ParticipatingEdges {
    bool left{},top{},right{},bottom{};
    friend bool operator==(const ParticipatingEdges&,const ParticipatingEdges&)=default;
    bool contains(Edge e) const noexcept {
        switch(e){case Edge::Left:return left;case Edge::Top:return top;
            case Edge::Right:return right;case Edge::Bottom:return bottom;}return false;
    }
};
struct MagnetTarget { model::WindowId id;Rect rect;bool screen{}; };
struct MagnetOptions {
    Distance attraction_distance{10}; // REFERENCE BASELINE, shared geometry units
    std::size_t max_targets{64}; // bounded caller-owned eligible snapshot
    bool screen_edges{true},outer_edges{true},inner_edges{true},corners{true},parallel_edges{true};
};
struct MotionSample {
    Rect previous;
    std::uint64_t previous_tick{},current_tick{},tick_frequency{};
    Distance speed_limit{}; // geometry units / second; caller policy, no magic speed
};
struct MagnetInput {
    Rect initial,raw;
    Interaction interaction{Interaction::Move};
    ParticipatingEdges edges; // Resize must exactly match safe inference
    std::span<const MagnetTarget> targets;
    MagnetOptions options;
    std::optional<MotionSample> motion;
};
struct SatisfiedConstraint {
    model::WindowId target;
    ConstraintKind kind;
    Edge moving_edge,target_edge;
    Distance delta{},predicted_overlap{};
    friend bool operator==(const SatisfiedConstraint&,const SatisfiedConstraint&)=default;
};
struct MagnetProposal {
    Rect corrected;
    geometry::Point move_delta; // Resize always zero; never whole-window translate
    ParticipatingEdges edges;
    std::vector<SatisfiedConstraint> satisfied;
};
struct MagnetResult {
    MotionState motion{MotionState::BelowThreshold};
    std::optional<MagnetProposal> proposal;
    std::string_view reason{"no_candidate"};
};
namespace detail {
inline constexpr std::array<Edge,4> edges{Edge::Left,Edge::Top,Edge::Right,Edge::Bottom};
inline bool x_axis(Edge e) noexcept {return e==Edge::Left||e==Edge::Right;}
inline std::uint64_t magnitude(Distance n) noexcept {return n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);}
inline bool valid_rect(const Rect& r) noexcept {
    const auto w=geometry::checked_difference(r.right(),r.left());
    const auto h=geometry::checked_difference(r.bottom(),r.top());
    return w&&h&&*w>0&&*h>0;
}
inline Distance overlap(const Rect& a,const Rect& b,bool x) {
    return x?topology::detail::positive_interval_overlap(a.top(),a.bottom(),b.top(),b.bottom()):
        topology::detail::positive_interval_overlap(a.left(),a.right(),b.left(),b.right());
}
inline bool near_endpoint(const Rect& a,const Rect& b,bool x,Distance d) noexcept {
    const std::array<Distance,2> first{x?a.top():a.left(),x?a.bottom():a.right()};
    const std::array<Distance,2> second{x?b.top():b.left(),x?b.bottom():b.right()};
    for(const auto f:first)for(const auto s:second)if(geometry::magnitude_within(f,s,d))return true;
    return false;
}
inline bool adjacent_parallel(const Rect& a,const Rect& b,bool x) noexcept {
    return x?(a.top()==b.bottom()||a.bottom()==b.top()):(a.left()==b.right()||a.right()==b.left());
}
inline std::optional<Rect> corrected(const MagnetInput& in,Distance dx,Distance dy) {
    if(in.interaction==Interaction::Move)return movement::translate_rect(in.raw,{dx,dy});
    const auto l=geometry::checked_add(in.raw.left(),in.edges.left?dx:0);
    const auto r=geometry::checked_add(in.raw.right(),in.edges.right?dx:0);
    const auto t=geometry::checked_add(in.raw.top(),in.edges.top?dy:0);
    const auto b=geometry::checked_add(in.raw.bottom(),in.edges.bottom?dy:0);
    // Rect normalizes reversed endpoints, so reject crossing BEFORE construction.
    if(!l||!r||!t||!b||*l>=*r||*t>=*b)return std::nullopt;
    const Rect result{*l,*t,*r,*b};
    return valid_rect(result)?std::optional{result}:std::nullopt;
}
struct Candidate {std::size_t target;ConstraintKind kind;Edge moving,target_edge;Distance delta;};
inline bool eligible(const Candidate& c,const Rect& result,const MagnetTarget& target,bool provisional,Distance field) {
    const bool x=x_axis(c.moving);
    const auto orth=overlap(result,target.rect,x);
    switch(c.kind) {
    case ConstraintKind::ScreenOuterEdge: return orth>0;
    case ConstraintKind::WindowOuterEdge: return orth>0;
    case ConstraintKind::WindowInnerEdge: return orth>0 && overlap(result,target.rect,!x)>0;
    case ConstraintKind::AdjacentCornerAlignment:
        return near_endpoint(result,target.rect,x,provisional?field:0);
    case ConstraintKind::ParallelEdgeAlignment:
        return adjacent_parallel(result,target.rect,x);
    }
    return false;
}
} // namespace detail

[[nodiscard]] inline std::optional<ParticipatingEdges> infer_resize_edges(const Rect& initial,const Rect& raw) noexcept {
    if(!detail::valid_rect(initial)||!detail::valid_rect(raw))return std::nullopt;
    ParticipatingEdges e{initial.left()!=raw.left(),initial.top()!=raw.top(),
                         initial.right()!=raw.right(),initial.bottom()!=raw.bottom()};
    if((e.left&&e.right)||(e.top&&e.bottom)||(!e.left&&!e.right&&!e.top&&!e.bottom))return std::nullopt;
    for(const auto edge:detail::edges)if(!geometry::checked_difference(
        geometry::edge_coordinate(raw,edge),geometry::edge_coordinate(initial,edge)))return std::nullopt;
    return e;
}
[[nodiscard]] inline MotionState classify_motion(const Rect& raw,const MotionSample& sample) noexcept {
    if(!detail::valid_rect(raw)||!detail::valid_rect(sample.previous)||!sample.tick_frequency||
       sample.current_tick<=sample.previous_tick||sample.speed_limit<=0)return MotionState::InvalidSample;
    std::uint64_t distance{};
    for(const auto e:detail::edges){
        const auto d=geometry::checked_difference(geometry::edge_coordinate(raw,e),geometry::edge_coordinate(sample.previous,e));
        if(!d)return MotionState::InvalidSample;
        distance=std::max(distance,detail::magnitude(*d));
    }
    const auto elapsed=sample.current_tick-sample.previous_tick;
    const auto limit=static_cast<std::uint64_t>(sample.speed_limit);
    const auto max=std::numeric_limits<std::uint64_t>::max();
    if(distance>max/sample.tick_frequency || elapsed>max/limit)return MotionState::InvalidSample;
    return distance*sample.tick_frequency>limit*elapsed?MotionState::Suppressed:MotionState::BelowThreshold;
}

// A pure bounded proposal, not window-operation or relation authority. Original
// implementation of the documented PaneBind equations; no upstream code reuse.
class MagnetConstraintSolver final {
public:
    [[nodiscard]] static MagnetResult solve(const MagnetInput& in) {
        MagnetResult result;
        try {
            if(!detail::valid_rect(in.initial)||!detail::valid_rect(in.raw)||in.options.attraction_distance<0||
               !in.options.max_targets||in.options.max_targets>256||in.targets.size()>in.options.max_targets) {
                result.reason="invalid_or_unbounded_input";return result;
            }
            if(in.interaction==Interaction::Resize) {
                const auto inferred=infer_resize_edges(in.initial,in.raw);
                if(!inferred||*inferred!=in.edges){result.reason="ambiguous_resize";return result;}
            } else if(in.interaction!=Interaction::Move || in.edges!=ParticipatingEdges{} ||
                      movement::classify_geometry_change(in.initial,in.raw).kind==movement::GeometryChangeKind::ResizeOrMixed) {
                result.reason="invalid_move";return result;
            }
            if(in.motion){result.motion=classify_motion(in.raw,*in.motion);
                if(result.motion!=MotionState::BelowThreshold){result.reason="motion_suppressed_or_invalid";return result;}}
            std::unordered_set<std::string> ids;
            std::vector<detail::Candidate> candidates;
            candidates.reserve(in.targets.size()*20);
            for(std::size_t i=0;i<in.targets.size();++i) {
                const auto& t=in.targets[i];
                if(!detail::valid_rect(t.rect)||!ids.insert(t.id.value()).second){result.reason="invalid_target_set";return result;}
                for(const auto moving:detail::edges)for(const auto target:detail::edges) {
                    if(!geometry::edges_share_axis(moving,target)||(in.interaction==Interaction::Resize&&!in.edges.contains(moving)))continue;
                    const auto d=geometry::checked_difference(geometry::edge_coordinate(t.rect,target),geometry::edge_coordinate(in.raw,moving));
                    if(!d||detail::magnitude(*d)>static_cast<std::uint64_t>(in.options.attraction_distance))continue;
                    const auto add=[&](ConstraintKind kind){candidates.push_back({i,kind,moving,target,*d});};
                    if(t.screen){if(in.options.screen_edges&&moving==target)add(ConstraintKind::ScreenOuterEdge);continue;}
                    if(moving!=target&&in.options.outer_edges)add(ConstraintKind::WindowOuterEdge);
                    if(moving==target&&in.options.inner_edges)add(ConstraintKind::WindowInnerEdge);
                    if(in.options.corners)add(ConstraintKind::AdjacentCornerAlignment);
                    if(moving==target&&in.options.parallel_edges)add(ConstraintKind::ParallelEdgeAlignment);
                }
            }
            // Two bounded O(N) selection passes. Pass 1 predicts each axis with
            // the other raw. Pass 2 ranks against the tentative other-axis result.
            // The final pair must validate together; no iteration/native writes.
            const auto select=[&](bool x,Distance other,bool provisional)->std::optional<detail::Candidate> {
                std::optional<detail::Candidate> best;Distance best_overlap{};
                for(const auto& c:candidates) {
                    if(detail::x_axis(c.moving)!=x)continue;
                    const auto rect=detail::corrected(in,x?c.delta:other,x?other:c.delta);
                    if(!rect||!detail::eligible(c,*rect,in.targets[c.target],provisional,in.options.attraction_distance))continue;
                    const auto overlap=detail::overlap(*rect,in.targets[c.target].rect,x);
                    const auto key=std::tuple{detail::magnitude(c.delta),-overlap,c.kind,std::string_view{in.targets[c.target].id.value()},c.moving,c.target_edge};
                    if(!best||key<std::tuple{detail::magnitude(best->delta),-best_overlap,best->kind,std::string_view{in.targets[best->target].id.value()},best->moving,best->target_edge}){best=c;best_overlap=overlap;}
                }
                return best;
            };
            auto x=select(true,0,true),y=select(false,0,true);
            const auto first_x=x?x->delta:0,first_y=y?y->delta:0;
            x=select(true,first_y,false);y=select(false,first_x,false);
            if(!x&&!y)return result;
            const auto dx=x?x->delta:0,dy=y?y->delta:0;
            // Check ranking against final predicted overlap too. If axis choices
            // would oscillate, abstain rather than loop or pick arrival order.
            const auto final_x=select(true,dy,false),final_y=select(false,dx,false);
            const auto same=[](const auto& a,const auto& b){
                return a.has_value()==b.has_value()&&(!a||(a->target==b->target&&a->kind==b->kind&&
                    a->moving==b->moving&&a->target_edge==b->target_edge&&a->delta==b->delta));};
            if(!same(x,final_x)||!same(y,final_y)){result.reason="unstable_candidate_pair";return result;}
            const auto corrected=detail::corrected(in,dx,dy);
            if(!corrected || (x&&!detail::eligible(*x,*corrected,in.targets[x->target],false,0)) ||
               (y&&!detail::eligible(*y,*corrected,in.targets[y->target],false,0))) {
                result.reason="incompatible_combined_geometry";return result;
            }
            MagnetProposal proposal{*corrected,in.interaction==Interaction::Move?geometry::Point{dx,dy}:geometry::Point{},in.edges,{}};
            for(const auto& c:candidates) {
                const auto& t=in.targets[c.target];
                if(geometry::edge_coordinate(*corrected,c.moving)==geometry::edge_coordinate(t.rect,c.target_edge)&&
                   detail::eligible(c,*corrected,t,false,0))
                    proposal.satisfied.push_back({t.id,c.kind,c.moving,c.target_edge,c.delta,detail::overlap(*corrected,t.rect,detail::x_axis(c.moving))});
            }
            std::sort(proposal.satisfied.begin(),proposal.satisfied.end(),[](const auto& a,const auto& b){
                return std::tuple{a.target.value(),a.kind,a.moving_edge,a.target_edge}<std::tuple{b.target.value(),b.kind,b.moving_edge,b.target_edge};});
            result.proposal=std::move(proposal);result.reason="proposal_only";
        }catch(const std::overflow_error&){result.reason="unrepresentable_geometry";}
        return result;
    }
};
} // namespace panebind::core::magnet
