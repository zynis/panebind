#pragma once

#include <algorithm>
#include <cstdint>

namespace panebind::core::v1 {

// Opaque attribute flags/colors cross the adapter boundary only as values for
// equality. The core neither interprets native style constants nor calls APIs.
struct VisualState {
    bool topmost{}, layered{}, attributes_known{true};
    std::uint8_t alpha{255};
    std::uint32_t color{}, flags{};
};
inline bool same_alpha(const VisualState& a,const VisualState& b) noexcept {
    if(a.layered!=b.layered)return false;
    if(!a.layered)return true;
    return a.attributes_known&&b.attributes_known&&a.alpha==b.alpha&&a.color==b.color&&a.flags==b.flags;
}
struct VisualOwnership {
    VisualState original,applied;
    bool changed_top{},changed_alpha{};
};
inline bool owns_top(const VisualOwnership& record,const VisualState& current) noexcept {
    return record.changed_top&&current.topmost==record.applied.topmost;
}
inline bool owns_alpha(const VisualOwnership& record,const VisualState& current) noexcept {
    return record.changed_alpha&&same_alpha(current,record.applied);
}
inline void copy_alpha(VisualState& destination,const VisualState& source) noexcept {
    destination.layered=source.layered;destination.attributes_known=source.attributes_known;
    destination.alpha=source.alpha;destination.color=source.color;destination.flags=source.flags;
}
// Reconcile each attribute independently. Observed external changes revoke old
// ownership; an explicit NEW user action may then capture that current state
// as its new original, but cleanup cannot write the abandoned original back.
inline void observe_visual(VisualOwnership& record,const VisualState& current) noexcept {
    if(record.changed_top&&!owns_top(record,current))record.changed_top=false;
    if(record.changed_alpha&&!owns_alpha(record,current))record.changed_alpha=false;
    if(!record.changed_top)record.original.topmost=record.applied.topmost=current.topmost;
    if(!record.changed_alpha){copy_alpha(record.original,current);copy_alpha(record.applied,current);}
}
inline void record_top(VisualOwnership& record,const VisualState& actual) noexcept {
    record.applied.topmost=actual.topmost;
    record.changed_top=actual.topmost!=record.original.topmost;
}
inline void record_alpha(VisualOwnership& record,const VisualState& actual) noexcept {
    copy_alpha(record.applied,actual);record.changed_alpha=!same_alpha(actual,record.original);
}
inline int effective_alpha(const VisualState& current,std::uint32_t alpha_flag) noexcept {
    return current.layered&&current.attributes_known&&(current.flags&alpha_flag)?current.alpha:255;
}
inline int stepped_alpha(int current,bool increase) noexcept {
    current=std::clamp(current,0,255);
    // Never make an already extremely transparent application more opaque in
    // response to the user's "decrease opacity" command.
    return increase?std::min(255,current+32):(current<=16?current:std::max(16,current-32));
}
inline bool visual_changes(const VisualOwnership& record) noexcept {
    return record.changed_top||record.changed_alpha;
}

} // namespace panebind::core::v1
