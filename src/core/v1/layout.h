#pragma once

#include "core/v1/types.h"
#include "core/geometry/checked_arithmetic.h"
#include "core/topology/window_adjacency.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace panebind::core::v1 {
namespace layout_detail {

inline bool valid_rect(Rect r) noexcept {
    const auto w = geometry::checked_difference(r.right(), r.left());
    const auto h = geometry::checked_difference(r.bottom(), r.top());
    return w && h && *w > 0 && *h > 0;
}
inline bool valid_limits(const Limits& l) noexcept {
    return l.min_width > 0 && l.min_height > 0 &&
           l.max_width >= l.min_width && l.max_height >= l.min_height &&
           l.grid_width > 0 && l.grid_height > 0;
}
inline bool valid_items(std::span<const Item> items) {
    std::vector<Key> keys;
    for (const auto& item : items) {
        if (!item.id || !valid_rect(item.visible) || !valid_limits(item.limits)) return false;
        keys.push_back(item.id);
    }
    std::sort(keys.begin(), keys.end());
    return std::adjacent_find(keys.begin(), keys.end()) == keys.end();
}
inline const Item* find(std::span<const Item> items, Key id) noexcept {
    const auto found = std::find_if(items.begin(), items.end(), [id](const auto& i) { return i.id == id; });
    return found == items.end() ? nullptr : &*found;
}
inline bool contains(Rect outer, Rect inner) noexcept {
    return inner.left() >= outer.left() && inner.top() >= outer.top() &&
           inner.right() <= outer.right() && inner.bottom() <= outer.bottom();
}
inline bool positive_overlap(std::int64_t a, std::int64_t b,
                             std::int64_t c, std::int64_t d) noexcept {
    return std::max(a,c) < std::min(b,d);
}
inline bool overlaps(Rect a, Rect b) noexcept {
    return positive_overlap(a.left(),a.right(),b.left(),b.right()) &&
           positive_overlap(a.top(),a.bottom(),b.top(),b.bottom());
}
inline std::optional<Rect> translated(Rect r, Point delta) noexcept {
    const auto l = geometry::checked_add(r.left(), delta.x);
    const auto t = geometry::checked_add(r.top(), delta.y);
    const auto rr = geometry::checked_add(r.right(), delta.x);
    const auto b = geometry::checked_add(r.bottom(), delta.y);
    if (!l || !t || !rr || !b) return std::nullopt;
    return Rect{*l,*t,*rr,*b};
}
inline bool legal_size(Rect r, const Limits& l) noexcept {
    if (!valid_rect(r) || !valid_limits(l)) return false;
    return r.width() >= l.min_width && r.width() <= l.max_width &&
           r.height() >= l.min_height && r.height() <= l.max_height &&
           r.width() % l.grid_width == 0 && r.height() % l.grid_height == 0;
}
inline Plan rejected(const char* reason) { return {false,reason,{}}; }

// All arithmetic below is platform independent. Grid constraints are combined
// as congruences of the single shared line, not independently rounded windows.
using U = std::uint64_t;
inline U mod(std::int64_t n, U m) noexcept {
    const auto remainder = n % static_cast<std::int64_t>(m);
    return remainder < 0 ? m - static_cast<U>(-(remainder + 1)) - 1 : static_cast<U>(remainder);
}
inline U add_mod(U a, U b, U m) noexcept { return a >= m-b ? a-(m-b) : a+b; }
inline U sub_mod(U a, U b, U m) noexcept { return a >= b ? a-b : m-(b-a); }
inline U multiply_mod(U a, U b, U m) noexcept {
    U result{};
    for (; b; b >>= 1) {
        if (b & 1U) result = add_mod(result,a,m);
        a = add_mod(a,a,m);
    }
    return result;
}
inline std::optional<U> inverse_mod(U a, U m) noexcept {
    if (m == 1) return U{};
    U remainder=m, next=a % m, coefficient{}, next_coefficient=1;
    while (next) {
        const U q=remainder/next;
        const U rest=remainder%next;
        remainder=next; next=rest;
        const U new_coefficient=sub_mod(coefficient,multiply_mod(q % m,next_coefficient,m),m);
        coefficient=next_coefficient; next_coefficient=new_coefficient;
    }
    return remainder == 1 ? std::optional<U>{coefficient} : std::nullopt;
}
struct LineRange {
    std::int64_t low{std::numeric_limits<std::int64_t>::min()};
    std::int64_t high{std::numeric_limits<std::int64_t>::max()};
    U residue{}, step{1};
    bool include(std::int64_t lo, std::int64_t hi, std::int64_t fixed, std::int64_t grid) noexcept {
        low=std::max(low,lo); high=std::min(high,hi);
        if (low > high || grid <= 0) return false;
        const U g=static_cast<U>(grid), divisor=std::gcd(step,g);
        const U difference=sub_mod(mod(fixed,g),residue % g,g);
        if (difference % divisor) return false;
        const U factor=g/divisor;
        if (factor == 1) return true;
        constexpr auto maximum=static_cast<U>(std::numeric_limits<std::int64_t>::max());
        if (step > maximum/factor) return false;
        const auto inverse=inverse_mod((step/divisor) % factor,factor);
        if (!inverse) return false;
        const U k=multiply_mod(difference/divisor,*inverse,factor);
        const U new_step=step*factor;
        residue=add_mod(residue,multiply_mod(step,k,new_step),new_step);
        step=new_step;
        return true;
    }
    std::optional<std::int64_t> choose(std::int64_t requested) const noexcept {
        if (low > high) return std::nullopt;
        const U offset=sub_mod(residue,mod(low,step),step);
        const auto first=geometry::checked_add(low,static_cast<std::int64_t>(offset));
        if (!first || *first > high) return std::nullopt;
        const auto span=geometry::checked_difference(high,*first);
        if (!span) return std::nullopt;
        const auto last=geometry::checked_add(*first,(*span/static_cast<std::int64_t>(step))*static_cast<std::int64_t>(step));
        if (!last) return std::nullopt;
        const auto target=std::clamp(requested,*first,*last);
        const auto from_first=geometry::checked_difference(target,*first);
        if (!from_first) return std::nullopt;
        const auto below=geometry::checked_add(*first,(*from_first/static_cast<std::int64_t>(step))*static_cast<std::int64_t>(step));
        if (!below) return std::nullopt;
        const auto distance=target-*below;
        if (static_cast<U>(distance) <= step/2) return below;
        return geometry::checked_add(*below,static_cast<std::int64_t>(step));
    }
};
struct EdgeSegment {
    std::size_t index{};
    bool low{};
    std::int64_t coordinate{}, start{}, end{};
};
struct AxisPlan {
    bool accepted{};
    std::int64_t line{};
    std::vector<EdgeSegment> edges;
};
inline AxisPlan resize_axis(std::span<const Item> items, std::size_t source,
                            bool x, bool low, std::int64_t delta,
                            std::int64_t tolerance, bool shared) {
    const auto r=items[source].visible;
    const auto origin=x ? (low?r.left():r.right()) : (low?r.top():r.bottom());
    const auto requested=geometry::checked_add(origin,delta);
    if (!requested) return {};
    std::vector<EdgeSegment> candidates;
    if (shared) {
        for (std::size_t i=0;i<items.size();++i) {
            const auto rect=items[i].visible;
            for (const bool side:{false,true}) {
                const auto coordinate=x ? (side?rect.left():rect.right()) : (side?rect.top():rect.bottom());
                if (geometry::magnitude_within(coordinate,origin,tolerance)) {
                    candidates.push_back({i,side,coordinate,x?rect.top():rect.left(),x?rect.bottom():rect.right()});
                }
            }
        }
    } else {
        candidates.push_back({source,low,origin,x?r.top():r.left(),x?r.bottom():r.right()});
    }
    const auto active=std::find_if(candidates.begin(),candidates.end(),[&](const auto& edge) {
        return edge.index == source && edge.low == low;
    });
    if (active == candidates.end()) return {};
    const auto active_index=static_cast<std::size_t>(active-candidates.begin());
    std::vector<bool> participates(candidates.size(),!shared);
    participates[active_index]=true;
    for (std::size_t i=0;i<candidates.size();++i) for (std::size_t j=i+1;j<candidates.size();++j) {
        const auto& a=candidates[i]; const auto& b=candidates[j];
        if (a.index != b.index && a.low != b.low && positive_overlap(a.start,a.end,b.start,b.end) &&
            geometry::magnitude_within(a.coordinate,b.coordinate,tolerance)) {
            participates[i]=participates[j]=true;
        }
    }
    // A split line propagates only through contiguous participating contact
    // segments. A merely collinear edge elsewhere in the graph is not selected.
    std::vector<bool> selected(candidates.size()); selected[active_index]=true;
    std::vector<std::size_t> pending{active_index};
    for (std::size_t cursor=0;cursor<pending.size();++cursor) {
        const auto& current=candidates[pending[cursor]];
        for (std::size_t j=0;j<candidates.size();++j) {
            const auto& other=candidates[j];
            if (!selected[j] && participates[j] &&
                std::max(current.start,other.start) <= std::min(current.end,other.end) &&
                geometry::magnitude_within(current.coordinate,other.coordinate,tolerance)) {
                selected[j]=true; pending.push_back(j);
            }
        }
    }
    AxisPlan result; LineRange range;
    for (std::size_t i=0;i<candidates.size();++i) if (selected[i]) {
        const auto& edge=candidates[i]; const auto& item=items[edge.index];
        const auto rect=item.visible;
        const auto fixed=x ? (edge.low?rect.right():rect.left()) : (edge.low?rect.bottom():rect.top());
        const auto minimum=x?item.limits.min_width:item.limits.min_height;
        const auto maximum=x?item.limits.max_width:item.limits.max_height;
        const auto grid=x?item.limits.grid_width:item.limits.grid_height;
        const auto lower=edge.low ? geometry::checked_difference(fixed,maximum) : geometry::checked_add(fixed,minimum);
        const auto upper=edge.low ? geometry::checked_difference(fixed,minimum) : geometry::checked_add(fixed,maximum);
        if (!lower || !upper || !range.include(*lower,*upper,fixed,grid)) return {};
        result.edges.push_back(edge);
    }
    const auto line=range.choose(*requested);
    if (!line) return {};
    result.accepted=true; result.line=*line;
    return result;
}

} // namespace layout_detail

[[nodiscard]] inline std::vector<Key> connected(std::span<const Item> items, Key source,
                                               std::int64_t tolerance) {
    if (tolerance < 0 || !layout_detail::valid_items(items) || !layout_detail::find(items,source)) return {};
    std::vector<topology::WindowGeometry> windows;
    for (const auto& item:items) windows.push_back({model::WindowId{std::to_string(item.id)},item.visible});
    const auto graph=topology::WindowAdjacencyGraph::build(windows,{tolerance});
    const auto component=graph.connected_component(model::WindowId{std::to_string(source)});
    std::vector<Key> result;
    for (const auto& id:component) result.push_back(static_cast<Key>(std::stoull(id.value())));
    std::sort(result.begin(),result.end());
    return result;
}

[[nodiscard]] inline Plan move_group(std::span<const Item> items, Key source, Point delta,
                                      std::int64_t tolerance) {
    const auto component=connected(items,source,tolerance);
    if (component.empty()) return layout_detail::rejected("invalid_geometry_or_source");
    Plan result{true,"group_translation",{}};
    for (const auto id:component) {
        const auto* item=layout_detail::find(items,id);
        const auto moved=layout_detail::translated(item->visible,delta);
        if (!moved) return layout_detail::rejected("coordinate_overflow");
        result.placements.push_back({id,*moved});
    }
    return result;
}

// A drag leaving our discrete layout restores its previous normal SIZE under
// the ORIGINAL down point. This is an intent anchor, never a claimed observed
// window rectangle. The adapter retains the actual down snapshot separately
// for authority, postverification and Escape; snap feedback never updates it.
[[nodiscard]] inline std::optional<Rect> restored_move_anchor(Rect down_visible,Rect normal,
                                                             Point cursor_down) {
    if(!layout_detail::valid_rect(down_visible)||!layout_detail::valid_rect(normal))return std::nullopt;
    const auto offset_x=geometry::checked_difference(cursor_down.x,down_visible.left());
    const auto offset_y=geometry::checked_difference(cursor_down.y,down_visible.top());
    if(!offset_x||!offset_y)return std::nullopt;
    const auto x=std::clamp(*offset_x,std::int64_t{},down_visible.width());
    if(x&&normal.width()>std::numeric_limits<std::int64_t>::max()/x)return std::nullopt;
    const auto anchored_x=x*normal.width()/down_visible.width();
    const auto anchored_y=std::clamp(*offset_y,std::int64_t{},normal.height()-1);
    const auto left=geometry::checked_difference(cursor_down.x,anchored_x);
    const auto top=geometry::checked_difference(cursor_down.y,anchored_y);
    if(!left||!top)return std::nullopt;
    const auto right=geometry::checked_add(*left,normal.width());
    const auto bottom=geometry::checked_add(*top,normal.height());
    return right&&bottom?std::optional{Rect{*left,*top,*right,*bottom}}:std::nullopt;
}

[[nodiscard]] inline Plan resize_group(std::span<const Item> items, Key source, Edges edges,
                                        Point delta, std::int64_t tolerance) {
    if (tolerance < 0 || !layout_detail::valid_items(items) ||
        (edges.left && edges.right) || (edges.top && edges.bottom) ||
        !(edges.left || edges.right || edges.top || edges.bottom)) {
        return layout_detail::rejected("invalid_resize_input");
    }
    const auto component=connected(items,source,tolerance);
    if (component.empty()) return layout_detail::rejected("unknown_source");
    std::vector<Item> members;
    for (const auto id:component) members.push_back(*layout_detail::find(items,id));
    const auto source_index=static_cast<std::size_t>(layout_detail::find(std::span<const Item>{members},source)-members.data());
    for (std::size_t i=0;i<members.size();++i) for (std::size_t j=i+1;j<members.size();++j) {
        if (layout_detail::overlaps(members[i].visible,members[j].visible)) return layout_detail::rejected("overlapping_layout");
    }
    auto planned=members;
    const auto apply=[&](bool x, bool low, std::int64_t d) {
        const auto axis=layout_detail::resize_axis(members,source_index,x,low,d,tolerance,true);
        if (!axis.accepted) return false;
        for (const auto& edge:axis.edges) {
            const auto r=planned[edge.index].visible;
            planned[edge.index].visible=x ? Rect{edge.low?axis.line:r.left(),r.top(),edge.low?r.right():axis.line,r.bottom()} :
                Rect{r.left(),edge.low?axis.line:r.top(),r.right(),edge.low?r.bottom():axis.line};
        }
        return true;
    };
    if (((edges.left || edges.right) && !apply(true,edges.left,delta.x)) ||
        ((edges.top || edges.bottom) && !apply(false,edges.top,delta.y))) {
        return layout_detail::rejected("empty_shared_size_interval_or_grid");
    }
    Plan result{true,"shared_line_resize",{}};
    for (std::size_t i=0;i<planned.size();++i) {
        if (!layout_detail::legal_size(planned[i].visible,planned[i].limits)) return layout_detail::rejected("illegal_result_size");
        for (std::size_t j=i+1;j<planned.size();++j) {
            if (layout_detail::overlaps(planned[i].visible,planned[j].visible)) return layout_detail::rejected("result_overlap");
        }
        if (planned[i].visible != members[i].visible) result.placements.push_back({planned[i].id,planned[i].visible});
    }
    return result;
}

[[nodiscard]] inline Plan resize_single(Item source, Edges edges, Point delta) {
    if (!layout_detail::valid_items(std::span<const Item>{&source,1}) ||
        (edges.left && edges.right) || (edges.top && edges.bottom) ||
        !(edges.left || edges.right || edges.top || edges.bottom)) return layout_detail::rejected("invalid_resize_input");
    const auto start=source.visible;
    const auto apply=[&](bool x, bool low, std::int64_t d) {
        const auto axis=layout_detail::resize_axis(std::span<const Item>{&source,1},0,x,low,d,0,false);
        if (!axis.accepted) return false;
        const auto r=source.visible;
        source.visible=x ? Rect{low?axis.line:r.left(),r.top(),low?r.right():axis.line,r.bottom()} :
            Rect{r.left(),low?axis.line:r.top(),r.right(),low?r.bottom():axis.line};
        return true;
    };
    if (((edges.left || edges.right) && !apply(true,edges.left,delta.x)) ||
        ((edges.top || edges.bottom) && !apply(false,edges.top,delta.y)) ||
        !layout_detail::legal_size(source.visible,source.limits)) return layout_detail::rejected("empty_size_interval_or_grid");
    Plan result{true,"single_resize",{}};
    if (source.visible != start) result.placements.push_back({source.id,source.visible});
    return result;
}

[[nodiscard]] inline Rect dock(Rect work, Action action) {
    if (!layout_detail::valid_rect(work)) return {};
    const auto mx=*geometry::checked_add(work.left(),work.width()/2);
    const auto my=*geometry::checked_add(work.top(),work.height()/2);
    switch (action) {
    case Action::DockLeft: return {work.left(),work.top(),mx,work.bottom()};
    case Action::DockRight: return {mx,work.top(),work.right(),work.bottom()};
    case Action::DockTop: return {work.left(),work.top(),work.right(),my};
    case Action::DockBottom: return {work.left(),my,work.right(),work.bottom()};
    case Action::DockTopLeft: return {work.left(),work.top(),mx,my};
    case Action::DockTopRight: return {mx,work.top(),work.right(),my};
    case Action::DockBottomLeft: return {work.left(),my,mx,work.bottom()};
    case Action::DockBottomRight: return {mx,my,work.right(),work.bottom()};
    case Action::Maximize: return work;
    default: return {};
    }
}

[[nodiscard]] inline Rect smart_dock(Rect work, Action action, std::span<const Item> others, bool smart) {
    const auto ordinary=dock(work,action);
    if (!smart || !layout_detail::valid_rect(ordinary) || !layout_detail::valid_items(others)) return ordinary;
    auto result=ordinary;
    const bool left=action==Action::DockLeft || action==Action::DockTopLeft || action==Action::DockBottomLeft;
    const bool right=action==Action::DockRight || action==Action::DockTopRight || action==Action::DockBottomRight;
    const bool top=action==Action::DockTop || action==Action::DockTopLeft || action==Action::DockTopRight;
    const bool bottom=action==Action::DockBottom || action==Action::DockBottomLeft || action==Action::DockBottomRight;
    const auto choose=[&](bool x,bool low,Rect target)->std::optional<std::int64_t> {
        struct Candidate { std::int64_t distance{}, coordinate{}; Key id{}; };
        std::optional<Candidate> best;
        const auto boundary=x?(low?target.right():target.left()):(low?target.bottom():target.top());
        for (const auto& item:others) {
            const auto r=item.visible;
            if (!layout_detail::contains(work,r)) continue;
            const bool anchored=x ? (low?r.right()==work.right():r.left()==work.left()) :
                                    (low?r.bottom()==work.bottom():r.top()==work.top());
            const bool covers=x ? (r.top()<=target.top() && r.bottom()>=target.bottom()) :
                                  (r.left()<=target.left() && r.right()>=target.right());
            if (!anchored || !covers) continue;
            const auto coordinate=x?(low?r.left():r.right()):(low?r.top():r.bottom());
            const auto distance=geometry::checked_difference(coordinate,boundary);
            if (!distance || *distance==std::numeric_limits<std::int64_t>::min()) continue;
            Candidate candidate{*distance<0?-*distance:*distance,coordinate,item.id};
            if (!best || candidate.distance<best->distance || (candidate.distance==best->distance && candidate.id<best->id)) best=candidate;
        }
        return best ? std::optional<std::int64_t>{best->coordinate} : std::nullopt;
    };
    if (left || right) if (const auto line=choose(true,left,result)) result={left?work.left():*line,result.top(),left?*line:work.right(),result.bottom()};
    if (top || bottom) if (const auto line=choose(false,top,result)) result={result.left(),top?work.top():*line,result.right(),top?*line:work.bottom()};
    // A smart candidate may not consume, overlap, or move an existing window.
    if (!layout_detail::valid_rect(result)) return ordinary;
    for (const auto& item:others) if (layout_detail::overlaps(result,item.visible)) return ordinary;
    return result;
}

[[nodiscard]] inline Rect stretch(Rect initial, Rect work, Edges edges, bool both_axes,
                                   std::span<const Item> others, bool smart) {
    if (!layout_detail::valid_rect(initial) || !layout_detail::valid_rect(work) ||
        !layout_detail::contains(work,initial) || !layout_detail::valid_items(others)) return {};
    if (both_axes) {
        if (edges.left || edges.right) edges.left=edges.right=true;
        if (edges.top || edges.bottom) edges.top=edges.bottom=true;
    }
    auto l=initial.left(),t=initial.top(),r=initial.right(),b=initial.bottom();
    if (edges.left) l=work.left();
    if (edges.right) r=work.right();
    if (edges.top) t=work.top();
    if (edges.bottom) b=work.bottom();
    if (smart) for (const auto& item:others) {
        const auto other=item.visible;
        if (other==initial) continue;
        if (layout_detail::positive_overlap(initial.top(),initial.bottom(),other.top(),other.bottom())) {
            if (edges.left && other.right()<=initial.left()) l=std::max(l,other.right());
            if (edges.right && other.left()>=initial.right()) r=std::min(r,other.left());
        }
        if (layout_detail::positive_overlap(initial.left(),initial.right(),other.left(),other.right())) {
            if (edges.top && other.bottom()<=initial.top()) t=std::max(t,other.bottom());
            if (edges.bottom && other.top()>=initial.bottom()) b=std::min(b,other.top());
        }
    }
    const Rect result{l,t,r,b};
    // Corner growth can otherwise engulf a diagonal obstacle which neither
    // original edge ray intersects. Refuse, rather than create an overlap.
    if (smart) for (const auto& item:others) {
        if (item.visible!=initial && !layout_detail::overlaps(initial,item.visible) && layout_detail::overlaps(result,item.visible)) return {};
    }
    return layout_detail::valid_rect(result) ? result : Rect{};
}

} // namespace panebind::core::v1
