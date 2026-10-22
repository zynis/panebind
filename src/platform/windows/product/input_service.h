#pragma once
#include "window_catalog.h"
#include "core/v1/timing_histogram.h"
#include "platform/windows/operations/gesture_input_shield.h"
#include <array>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

namespace panebind::platform::windows::product {
constexpr UINT input_message=WM_APP+41, prepare_message=WM_APP+42, catalog_message=WM_APP+43;
constexpr std::uint64_t input_pending_capture_mask=std::uint64_t{1}<<63;
inline bool arm_exact_capture_generation(std::atomic<std::uint64_t>& authority,std::uint64_t generation){
    if(!generation||(generation&input_pending_capture_mask))return false;
    auto pending=generation|input_pending_capture_mask;return authority.compare_exchange_strong(pending,generation);
}
inline bool captured_release_can_complete(bool resources_clean,bool actual_normal_up,bool aborted,
                                         bool same_down_serial,bool available){
    return resources_clean&&actual_normal_up&&!aborted&&same_down_serial&&available;
}
inline bool native_move_resources_can_retire(operations::GestureShieldEventKind kind,bool actual_cleanup_confirmed){
    return kind==operations::GestureShieldEventKind::IsolationGone&&actual_cleanup_confirmed;
}
// CaptureReleased is emitted by the resource owner only after its actual
// GetCapture observation. On the no-call branch release_after copies that
// single observation; neither NULL field alone is evidence of a query.
// This proves only that this generation no longer owns capture, not that
// there is no capture elsewhere and not that a normal UP was delivered.
inline bool captured_abort_cleanup_confirmed(const GestureCaptureContext* context,
    const operations::GestureShieldEvent* observed,const operations::GestureShieldEvent& gone,bool aborted){
    using Kind=operations::GestureShieldEventKind;
    if(!aborted||!context||!observed||!context->generation||!context->overlay||
       !context->owner_tid||!context->owner_pid||!context->source.hwnd||!context->source.pid||!context->source.tid||
       observed->kind!=Kind::CaptureReleased||gone.kind!=Kind::IsolationGone||
       gone.removal_reason==operations::GestureShieldRemovalReason::NormalUp||
       !gone.overlay_destroyed||!gone.hotkey_unregistered)return false;
    const auto exact=[&](const operations::GestureShieldEvent& event){
        const auto& request=event.entry_mode==operations::GestureShieldEntryMode::ConsumedDown?
            event.consumed_down.gesture:event.gesture;
        return
            event.generation==context->generation&&event.overlay==context->overlay&&
            request.generation==context->generation&&request.source==context->source.hwnd&&
            request.process_id==context->source.pid&&request.thread_id==context->source.tid&&
            event.capture.owner_thread_id==context->owner_tid&&event.capture.attempted;
    };
    const auto observed_not_own=[&](const operations::GestureShieldCaptureFacts& facts){
        return !facts.release_attempted&&!facts.release_completed&&!facts.release_succeeded&&
            facts.release_before!=context->overlay&&facts.release_after==facts.release_before;
    };
    const auto& association=gone.capture.association;
    return exact(*observed)&&exact(gone)&&observed_not_own(observed->capture)&&observed_not_own(gone.capture)&&
        observed->capture.release_before==gone.capture.release_before&&
        association.generation==context->generation&&association.owner_thread_id==context->owner_tid&&
        association.source==context->source.hwnd&&association.source_process_id==context->source.pid&&
        association.source_thread_id==context->source.tid&&
        operations::gesture_shield_association_owned_pair(association,context->generation)&&
        operations::gesture_shield_association_cleanup_confirmed(association,context->generation);
}
struct Prepared {
    Snapshot source;
    POINT point{};
    DWORD stamp{};
    int region{-1};
    std::uint64_t config_generation{};
    // Prepared on the GUI owner, only for this exact hit-test region.
    // Index: (button-1)*32 + (clicks-1)*16 + locked modifiers.
    std::array<core::v1::Action,160> mappings{};
    // GUI-owned layout record verified against the actual DOWN preparation.
    // Only restoration intent; every later native write still needs fresh B.
    std::optional<core::v1::Rect> layout_restore_normal;
    // GUI-owned original-DOWN group. The LL callback neither enumerates nor
    // allocates this topology; END geometry is only the later write baseline.
    std::vector<Snapshot> native_ctrl_members;
    bool native_ctrl_members_valid{};
};
enum class DownRoute { Pass, Gesture, Mapped };
struct DownDecision { DownRoute route{DownRoute::Pass}; core::v1::Action action{core::v1::Action::None}; };
inline void prepare_mappings(Prepared& prepared,const core::v1::Settings& settings) {
    prepared.mappings.fill(core::v1::Action::None);
    for(const auto& mapping:settings.mouse_bindings)
        if(mapping.region==prepared.region&&mapping.button>=1&&mapping.button<=5&&
            mapping.clicks>=1&&mapping.clicks<=2&&mapping.modifiers<=15)
            prepared.mappings[static_cast<std::size_t>((mapping.button-1)*32+(mapping.clicks-1)*16)+mapping.modifiers]=mapping.action;
}
// The callback uses only this bounded precomputed table; no configuration
// searches or native queries are hidden in the routing predicate.
inline DownDecision route_down(const Prepared& prepared,int button,int clicks,unsigned mods) {
    if(prepared.region<0||prepared.region>8||button<1||button>5||clicks<1||clicks>2||mods>15)return {};
    const auto action=prepared.mappings[static_cast<std::size_t>((button-1)*32+(clicks-1)*16)+mods];
    if(action!=core::v1::Action::None)return {DownRoute::Mapped,action};
    // Shift bypasses enhanced dragging, not an explicitly configured Shift
    // double-click. Unmapped extra buttons and native double-clicks pass.
    if(button!=1||(mods&8u))return {};
    // Corners have a contextual two-edge stretch route; they are not mapped
    // to StretchAll (which would also alter the two opposite edges).
    if(clicks==2&&prepared.region>=5)return {DownRoute::Gesture,core::v1::Action::None};
    if((mods&4u)||clicks==2)return {};
    return {DownRoute::Gesture,core::v1::Action::None};
}
// A press that became a drag cannot become the first click of a double-click,
// even if the cursor subsequently returns to its DOWN point. Track native
// passthrough presses too; consumption is unrelated to click eligibility.
struct ClickCandidate {
    POINT anchor{};
    int button{};
    bool eligible{},pressing{};
    void begin(POINT point,int pressed_button,bool candidate){
        anchor=point;button=pressed_button;eligible=candidate;pressing=true;
    }
    void motion(POINT point,std::int64_t drag_x,std::int64_t drag_y){
        if(!eligible||!pressing)return;
        const auto dx=static_cast<std::int64_t>(point.x)-anchor.x;
        const auto dy=static_cast<std::int64_t>(point.y)-anchor.y;
        if(drag_x<0||drag_y<0||dx < -drag_x||dx > drag_x||dy < -drag_y||dy > drag_y)eligible=false;
    }
    void release(int released_button){if(pressing&&released_button==button)pressing=false;}
};
inline bool native_event_is_current(DWORD event_stamp,DWORD down_stamp) {
    return static_cast<DWORD>(event_stamp-down_stamp)<0x80000000u;
}
// Hard revocation precedes dispatch of its queued abort notification. A new
// actual DOWN may retire that revoked pair without waiting for the notification;
// an active authorized pair is never retired by an overlapping DOWN.
inline bool retire_revoked_pair_for_down(bool consumed,std::uint64_t authority,
                                        int paired_button,int incoming_button,DWORD old_down,DWORD new_down){
    return consumed&&authority==0&&paired_button>=1&&paired_button<=5&&
        incoming_button==paired_button&&native_event_is_current(new_down,old_down);
}
inline bool motion_requests_preparation(bool consumed,std::uint64_t authority){return !consumed||authority==0;}
// Catalog notifications may outlive the gesture which issued a native call.
// Observation must wait for that real completion, not probe a still-blocked
// source merely because the GUI has already retired its gesture state.
inline bool idle_catalog_observation_allowed(bool gesture_active,bool native_pending){
    return !gesture_active&&!native_pending;
}
inline bool button_up_matches_current_pair(bool consumed,int paired_button,int incoming_button,DWORD down_stamp,DWORD up_stamp){
    return consumed&&paired_button>=1&&paired_button<=5&&incoming_button==paired_button&&
        native_event_is_current(up_stamp,down_stamp);
}
inline bool operation_permission(std::uint64_t generation,std::uint64_t active,std::uint64_t normal_up,
                                 bool available,bool allow_completion){
    return available&&(!generation||(!allow_completion&&active==generation)||
        (allow_completion&&active==0&&normal_up==generation));
}
inline bool revoke_exact_generation(std::atomic<std::uint64_t>& active,std::atomic<std::uint64_t>& completion,
                                    std::uint64_t generation){
    if(!generation)return false;
    auto expected=generation;const bool retired=active.compare_exchange_strong(expected,0);
    expected=generation;completion.compare_exchange_strong(expected,0);
    return retired;
}
inline bool finish_exact_generation(std::atomic<std::uint64_t>& active,std::atomic<std::uint64_t>& completion,
                                    std::uint64_t generation,core::v1::End reason){
    if(reason!=core::v1::End::NormalUp)return revoke_exact_generation(active,completion,generation);
    if(!generation)return false;
    // The actual matching UP already published its separate release-dock token.
    // Retiring the GUI's older gesture must not revoke a newer hook DOWN.
    auto expected=generation;return active.compare_exchange_strong(expected,0);
}
// Actual matching-UP decision used by the callback. Native foreground events
// may still be queued: current facts, not their eventual delivery, determine
// whether this generation can receive a NormalUp completion capability.
inline core::v1::End matching_up_end(bool aborted,core::v1::End observed_end,
                                   bool source_current,bool foreground_matches){
    if(observed_end!=core::v1::End::NormalUp)return observed_end;
    if(!source_current||aborted)return core::v1::End::SourceLost;
    if(!foreground_matches)return core::v1::End::FocusLost;
    return core::v1::End::NormalUp;
}
enum class NativeMovePhase { AwaitStart,IsolationPending,AwaitEnd,AwaitCapture,Writing,Retired };
struct NativeMoveReceipt {
    std::uint64_t generation{},sequence{};
    DWORD event{},thread_id{},stamp{};
    HWND source{};
    LONG object{},child{};
};
// Historical plain-Move predicate retained for its existing tests. Production
// uses the explicitly classified native_geometry_entry below, not this subset.
inline bool native_plain_move_entry(bool guest,const Prepared& p,int button,int clicks,unsigned mods,DownDecision route){
    return guest&&route.route==DownRoute::Gesture&&button==1&&clicks==1&&mods==0&&p.region==0&&!p.source.maximized;
}
inline bool native_geometry_entry(bool resource_enabled,const Prepared& p,int button,int clicks,unsigned mods,DownDecision route){
    return resource_enabled&&route.route==DownRoute::Gesture&&button==1&&clicks==1&&(mods==0||mods==2)&&
        p.region>=0&&p.region<=8&&!p.source.maximized&&(p.region==0||p.source.can_resize);
}
enum class NativeRestoreKind { None,Maximized,Arranged };
[[nodiscard]] constexpr const char* native_restore_kind_name(NativeRestoreKind kind)noexcept{
    switch(kind){case NativeRestoreKind::Maximized:return "maximized";
        case NativeRestoreKind::Arranged:return "native_arranged";default:return "none";}
}
inline bool plain_caption_gesture(const Prepared& p,int button,int clicks,unsigned mods){
    return p.region==0&&button==1&&clicks==1&&mods==0&&route_down(p,button,clicks,mods).route==DownRoute::Gesture;
}
// A native-arranged plain Move keeps the real START/cancel/END route. A true
// state on only one side of that handoff cannot silently become a large normal
// Move. Ctrl and Resize retain their existing, separately classified contracts.
inline bool native_arranged_handoff_compatible(const Prepared& down,const Snapshot& fresh,
                                               int button,int clicks,unsigned mods){
    if(!plain_caption_gesture(down,button,clicks,mods))return true;
    if(!down.source.native_arranged||!fresh.native_arranged||down.source.native_arranged!=fresh.native_arranged)return false;
    return !*fresh.native_arranged||
        (!down.source.maximized&&!down.source.minimized&&!fresh.maximized&&!fresh.minimized);
}
inline NativeRestoreKind native_restore_entry_kind(const Prepared& down,const Snapshot& fresh,
    bool native_handoff,int button,int clicks,unsigned mods){
    if(!plain_caption_gesture(down,button,clicks,mods))return NativeRestoreKind::None;
    if(!native_handoff&&down.source.maximized&&fresh.maximized&&!down.source.minimized&&!fresh.minimized)
        return NativeRestoreKind::Maximized;
    if(native_handoff&&native_arranged_handoff_compatible(down,fresh,button,clicks,mods)&&
       down.source.native_arranged==true&&fresh.native_arranged==true)return NativeRestoreKind::Arranged;
    return NativeRestoreKind::None;
}
inline bool native_restore_same_context(const Snapshot& original,const Snapshot& fresh){
    return Catalog::same(original,fresh)&&original.dpi==fresh.dpi&&original.monitor==fresh.monitor&&
        original.work==fresh.work&&original.feature_mask==fresh.feature_mask&&original.can_resize==fresh.can_resize;
}
// Baseline is the actual post-END/Capture geometry B. The immutable DOWN A is
// only the restoration-intent/state anchor, never a substitute continuity B.
inline bool native_restore_request_ready(NativeRestoreKind kind,const Snapshot& original,
    const Snapshot& baseline,const Snapshot& fresh,UINT placement_show_cmd,bool active){
    if(!active||!native_restore_same_context(original,fresh)||!native_restore_same_context(baseline,fresh)||
       baseline.visible!=fresh.visible||baseline.positioning!=fresh.positioning||fresh.minimized)return false;
    if(kind==NativeRestoreKind::Maximized)return original.maximized&&fresh.maximized&&placement_show_cmd==SW_SHOWMAXIMIZED;
    return kind==NativeRestoreKind::Arranged&&!original.maximized&&!original.minimized&&!fresh.maximized&&
        original.native_arranged==true&&baseline.native_arranged==true&&fresh.native_arranged==true&&placement_show_cmd==SW_SHOWNORMAL;
}
enum class NativeRestoreProgress { Pending,Ready,Rejected };
inline NativeRestoreProgress native_restore_progress(NativeRestoreKind kind,const Snapshot& original,const Snapshot& fresh,
    std::int64_t normal_native_width,std::int64_t normal_native_height,bool active){
    if(!active||kind==NativeRestoreKind::None||!native_restore_same_context(original,fresh))return NativeRestoreProgress::Rejected;
    if(fresh.maximized||fresh.minimized)return NativeRestoreProgress::Pending;
    if(kind==NativeRestoreKind::Arranged){
        // A false arranged bit alone can race its actual restored geometry.
        // Compare sizes only: rcNormalPosition may be WORKSPACE coordinates.
        if(fresh.native_arranged!=false||normal_native_width<=0||normal_native_height<=0||
           fresh.positioning.width()!=normal_native_width||fresh.positioning.height()!=normal_native_height)
            return NativeRestoreProgress::Pending;
    }
    return NativeRestoreProgress::Ready;
}
inline bool native_move_receipt_matches(const NativeMoveReceipt& r,const Prepared& p,
    std::uint64_t generation,DWORD down_stamp,DWORD event){
    return generation&&r.generation==generation&&r.sequence&&r.event==event&&r.source==p.source.hwnd&&
        r.thread_id==p.source.tid&&r.object==OBJID_WINDOW&&r.child==CHILDID_SELF&&
        native_event_is_current(r.stamp,down_stamp);
}
inline bool native_move_end_follows(const NativeMoveReceipt& start,const NativeMoveReceipt& end){
    return start.event==EVENT_SYSTEM_MOVESIZESTART&&end.event==EVENT_SYSTEM_MOVESIZEEND&&
        start.generation==end.generation&&start.source==end.source&&start.thread_id==end.thread_id&&
        end.sequence>start.sequence&&native_event_is_current(end.stamp,start.stamp);
}
// END geometry is a continuity baseline, never a replacement intent anchor.
// Native translation may change position only; unobserved resize/context change
// is not silently accepted as the handoff's new identity.
inline bool native_move_handoff_compatible(const Snapshot& down,const Snapshot& end){
    return Catalog::same(down,end)&&down.visible.width()==end.visible.width()&&down.visible.height()==end.visible.height()&&
        down.positioning.width()==end.positioning.width()&&down.positioning.height()==end.positioning.height()&&
        down.dpi==end.dpi&&down.monitor==end.monitor&&down.work==end.work&&
        down.maximized==end.maximized&&down.minimized==end.minimized&&down.feature_mask==end.feature_mask&&
        down.can_resize==end.can_resize&&
        down.visible.left()-down.positioning.left()==end.visible.left()-end.positioning.left()&&
        down.visible.top()-down.positioning.top()==end.visible.top()-end.positioning.top();
}
// The real native prefix may already have translated/resized the source. Keep
// A as intent and accept B only for the exact locked operation: an inactive
// edge may not move, nor may identity/display/frame/state silently change.
inline bool native_geometry_handoff_compatible(const Prepared& prepared,const Snapshot& end){
    const auto& down=prepared.source;
    const auto native_frame=[](const core::v1::Rect& rect){
        constexpr auto low=std::numeric_limits<LONG>::min(),high=std::numeric_limits<LONG>::max();
        for(const auto value:std::array{rect.left(),rect.top(),rect.right(),rect.bottom()})if(value<low||value>high)return false;
        return rect.right()>rect.left()&&rect.bottom()>rect.top()&&rect.width()<=high&&rect.height()<=high;
    };
    if(!native_frame(down.visible)||!native_frame(down.positioning)||!native_frame(end.visible)||!native_frame(end.positioning))return false;
    if(prepared.region==0)return native_move_handoff_compatible(down,end);
    if(prepared.region<1||prepared.region>8||!down.can_resize||!Catalog::same(down,end)||
       down.dpi!=end.dpi||down.monitor!=end.monitor||down.work!=end.work||
       down.maximized!=end.maximized||down.minimized!=end.minimized||down.feature_mask!=end.feature_mask||down.can_resize!=end.can_resize||
       down.visible.left()-down.positioning.left()!=end.visible.left()-end.positioning.left()||
       down.visible.top()-down.positioning.top()!=end.visible.top()-end.positioning.top()||
       down.positioning.right()-down.visible.right()!=end.positioning.right()-end.visible.right()||
       down.positioning.bottom()-down.visible.bottom()!=end.positioning.bottom()-end.visible.bottom())return false;
    const auto& limits=down.limits;
    if(limits.min_width<=0||limits.min_height<=0||limits.max_width<limits.min_width||limits.max_height<limits.min_height||
       limits.grid_width<=0||limits.grid_height<=0||end.visible.width()<limits.min_width||end.visible.width()>limits.max_width||
       end.visible.height()<limits.min_height||end.visible.height()>limits.max_height||
       end.visible.width()%limits.grid_width||end.visible.height()%limits.grid_height)return false;
    const bool left=prepared.region==1||prepared.region==5||prepared.region==7;
    const bool right=prepared.region==2||prepared.region==6||prepared.region==8;
    const bool top=prepared.region==3||prepared.region==5||prepared.region==6;
    const bool bottom=prepared.region==4||prepared.region==7||prepared.region==8;
    return (left||(end.visible.left()==down.visible.left()&&end.positioning.left()==down.positioning.left()))&&
        (right||(end.visible.right()==down.visible.right()&&end.positioning.right()==down.positioning.right()))&&
        (top||(end.visible.top()==down.visible.top()&&end.positioning.top()==down.positioning.top()))&&
        (bottom||(end.visible.bottom()==down.visible.bottom()&&end.positioning.bottom()==down.positioning.bottom()));
}
enum class InputKind { Down, Move, Up, Abort, Mapped, Resource, NativeMovePending, NativeMoveStart, NativeMoveEnd, NativeMoveReleased };
enum class InputOrigin { MouseHook, ShieldLegacy, PlatformAbort, ShieldResource, NativeMoveEvent };
enum class InputAbortOrigin { Unknown,WinEventForeground,WinEventCaptureStart };
[[nodiscard]] constexpr const char* input_abort_origin_name(InputAbortOrigin value)noexcept{
    switch(value){case InputAbortOrigin::WinEventForeground:return "winevent_foreground";
        case InputAbortOrigin::WinEventCaptureStart:return "winevent_capture_start";default:return "unknown";}
}
// One bounded observation AFTER the existing WinEvent abort decision. These
// facts do not decide authority or imply an atomic cross-queue snapshot.
struct InputAbortObservation {
    InputAbortOrigin origin{InputAbortOrigin::Unknown};
    DWORD trigger_event{},native_time{};
    std::uint64_t observed_qpc{};
    HWND trigger_hwnd{},foreground{},source_capture{},source_move_size{},source_menu_owner{},owner_capture{};
    bool source_identity_current{},source_gui_attempted{},source_gui_known{},owner_gui_attempted{},owner_gui_known{};
    DWORD source_gui_error{},source_gui_flags{},owner_thread_id{},owner_gui_error{};
};
struct InputEvent {
    InputKind kind{};
    POINT point{};
    std::uint64_t generation{}, tick{};
    unsigned modifiers{};
    int button{1}, clicks{1};
    core::v1::End end{core::v1::End::NormalUp};
    std::shared_ptr<const Prepared> prepared;
    core::v1::Action action{core::v1::Action::None};
    // Actual QPC at the beginning of this mouse-hook observation. Zero means
    // no such observation (e.g. WinEvent abort), never an invented timestamp.
    std::uint64_t hook_observed_qpc{};
    InputOrigin origin{InputOrigin::MouseHook};
    DWORD native_message_time{};
    std::uint64_t resource_observed_qpc{};
    std::shared_ptr<const operations::GestureShieldEvent> resource_event;
    std::shared_ptr<const NativeMoveReceipt> native_move_receipt;
    std::shared_ptr<const Snapshot> handoff_snapshot;
    std::optional<InputAbortObservation> abort_observation;
};
// The resource adapter calls this exact immutable-DOWN matching predicate.
// It does not query a window or grant live authority: CaptureBridge separately
// checks current generation, DOWN serial, actual UP/revocation and deadline.
inline bool consumed_capture_request_matches_down(
    const operations::GestureShieldConsumedDownRequest& request,const InputEvent& down)noexcept{
    if(down.kind!=InputKind::Down||!down.prepared||
       !operations::gesture_shield_consumed_down_receipt_valid(request))return false;
    const auto& prepared=*down.prepared;const auto& source=prepared.source;
    if(request.gesture.generation!=down.generation||request.gesture.source!=source.hwnd||
       request.gesture.process_id!=source.pid||request.gesture.thread_id!=source.tid||
       request.down_point.x!=down.point.x||request.down_point.y!=down.point.y||
       request.down_message_time!=down.native_message_time||request.down_qpc!=down.hook_observed_qpc||
       request.source_was_maximized!=source.maximized||source.minimized||
       route_down(prepared,down.button,down.clicks,down.modifiers).route!=DownRoute::Gesture)return false;
    // This repair only opens the already-implemented plain maximized
    // titlebar-restore path, not Ctrl, Resize, mapped or double-click entry.
    return !source.maximized ||
        (prepared.region==0&&down.button==1&&down.clicks==1&&down.modifiers==0);
}
// A real early native UP may arrive after START rejection already cleared the
// GUI pending slot. Refresh the current hover after either retirement order,
// never by reviving the released Prepared or touching a newer active gesture.
inline bool native_release_requests_preparation(const InputEvent& event,std::uint64_t gui_pending_generation,
    bool gui_gesture_active,bool input_idle,bool enabled)noexcept{
    return event.kind==InputKind::NativeMoveReleased&&event.generation&&
        (!gui_pending_generation||gui_pending_generation==event.generation)&&
        !gui_gesture_active&&input_idle&&enabled;
}
// Input-owner-only, one real LL MOVE observed while CaptureReady/GUI DOWN
// publication is pending. Consumed DOWN may retain pending-capture motion;
// native-first may retain only post-READY motion, never pre-END movement.
// No resource thread writes this slot or turns UP into motion permission.
struct CapturedMoveSlot {
    struct Sample {POINT point{};std::uint64_t tick{},hook_qpc{};DWORD native_message_time{};};
    std::uint64_t generation{};
    bool down_enqueued{},pending{},retired{},retain_before_ready{};
    Sample latest{};
    void begin(std::uint64_t g,bool capture_pair,bool retain_pending)noexcept{
        generation=capture_pair?g:0;down_enqueued=false;pending=false;retired=false;
        retain_before_ready=capture_pair&&retain_pending;
    }
    void retire()noexcept{retired=true;pending=false;}
    bool observe(std::uint64_t g,Sample actual,bool actual_authority)noexcept{
        if(!g||g!=generation||down_enqueued||retired||(!retain_before_ready&&!actual_authority))return false;
        latest=actual;pending=true;return true;
    }
    [[nodiscard]] bool may_queue_down(std::uint64_t g)const noexcept{return g&&g==generation&&!down_enqueued;}
    bool mark_down_enqueued(std::uint64_t g)noexcept{
        if(!may_queue_down(g))return false;down_enqueued=true;return true;
    }
    [[nodiscard]] bool direct_move_allowed(std::uint64_t g)const noexcept{return g&&g==generation&&down_enqueued&&!retired;}
    std::optional<Sample> take_after_down(std::uint64_t g,bool active)noexcept{
        if(!g||g!=generation||!down_enqueued||!pending)return {};
        pending=false;return active&&!retired?std::optional<Sample>{latest}:std::nullopt;
    }
};
// Same dispatcher predicate as the production motion path. A release-only
// docking command is not permission to submit continuous motion after UP.
inline bool dispatch_motion_permission(const InputEvent& event,bool terminal,
                                      std::uint64_t active,std::uint64_t completion,
                                      bool available,bool terminal_consumed){
    if(!event.generation)return false;
    (void)terminal_consumed;
    if(terminal)return false;
    return event.kind==InputKind::Move&&operation_permission(event.generation,active,completion,available,false);
}
// Serialized GUI progress, recorded only after every placement in a quantum
// was actually exact. It is not input authority: active MOVE still requires
// active permission and every participant's fresh actual frame. UP never
// authorizes a new continuous quantum (including an unconsumed cursor).
struct ExactMotionPoint {std::uint64_t generation{};POINT point{};};
inline bool motion_point_already_exact(const InputEvent& event,bool terminal,
                                      const std::optional<ExactMotionPoint>& exact){
    const bool motion_kind=terminal?(event.kind==InputKind::Up&&event.end==core::v1::End::NormalUp):
        event.kind==InputKind::Move;
    return motion_kind&&exact&&event.generation!=0&&exact->generation==event.generation&&
        event.point.x==exact->point.x&&event.point.y==exact->point.y;
}
struct MouseUpCallbackSnapshot {
    bool known{};
    std::uint64_t callback_count{},hook_observed_qpc{},generation_at_entry{};
    int button{};
    bool consumed_before{},aborted_before{},matching_branch_taken{},push_attempted{},push_succeeded{};
};
// One hook owner publishes; the GUI makes at most three coherent reads. Every
// payload word is atomic too: an inconsistent observation is UNKNOWN, never a
// mixed packet or a C++ data race. There are no waits/retries on the publisher.
class MouseUpCallbackObservation {
public:
    static constexpr bool coherent(std::uint64_t before,std::uint64_t after){return !(before&1u)&&before==after;}
    void publish(const MouseUpCallbackSnapshot& value){
        sequence_.fetch_add(1);
        count_.store(value.callback_count);qpc_.store(value.hook_observed_qpc);generation_.store(value.generation_at_entry);
        const auto flags=static_cast<std::uint64_t>(value.button)&255u;
        details_.store(flags|(static_cast<std::uint64_t>(value.consumed_before)<<8)|
            (static_cast<std::uint64_t>(value.aborted_before)<<9)|(static_cast<std::uint64_t>(value.matching_branch_taken)<<10)|
            (static_cast<std::uint64_t>(value.push_attempted)<<11)|(static_cast<std::uint64_t>(value.push_succeeded)<<12));
        sequence_.fetch_add(1);
    }
    MouseUpCallbackSnapshot snapshot()const{
        for(unsigned attempt=0;attempt<3;++attempt){
            const auto before=sequence_.load();if(before&1u)continue;
            MouseUpCallbackSnapshot out;out.callback_count=count_.load();out.hook_observed_qpc=qpc_.load();
            out.generation_at_entry=generation_.load();const auto flags=details_.load();
            if(!coherent(before,sequence_.load()))continue;
            out.known=true;out.button=static_cast<int>(flags&255u);out.consumed_before=(flags&(1u<<8))!=0;
            out.aborted_before=(flags&(1u<<9))!=0;out.matching_branch_taken=(flags&(1u<<10))!=0;
            out.push_attempted=(flags&(1u<<11))!=0;out.push_succeeded=(flags&(1u<<12))!=0;return out;
        }
        return {};
    }
private:
    // Sequentially consistent operations retain a single order across the
    // sequence and payload words; a relaxed multiword read is not substituted.
    std::atomic<std::uint64_t> sequence_{},count_{},qpc_{},generation_{},details_{};
};
// Diagnostic-only copies of the immutable cache actually read by the DOWN
// branch. No native identity query, qualification or write permission is added.
struct ButtonPreparedFacts {
    bool present{};HWND root{};DWORD pid{},tid{},stamp{};
    std::uint64_t born{},key{},config_generation{};POINT point{};int region{-1};
    std::uint32_t feature_mask{};bool maximized{},minimized{},can_resize{};
};
inline ButtonPreparedFacts button_prepared_facts(const Prepared* p)noexcept{
    if(!p)return {};
    return {true,p->source.hwnd,p->source.pid,p->source.tid,p->stamp,p->source.born,p->source.key,
        p->config_generation,p->point,p->region,p->source.feature_mask,
        p->source.maximized,p->source.minimized,p->source.can_resize};
}
struct ButtonAdmissionState {
    std::uint64_t generation{},authority{},completion{};
    bool consumed{},native_move{},capture_pair{},paused{},healthy{},available{},aborted{},stop_requested{};
};
struct ButtonAdmission {
    std::uint64_t sequence{},hook_qpc{},tick{};UINT message{};DWORD native_time{},flags{},mouse_data{};
    POINT point{};int button{};bool down{};
    ButtonAdmissionState before,after;ButtonPreparedFacts prepared,active_prepared;
    bool cache_read{},down_branch_entered{},prepared_gate_passed{},foreground_read{},route_evaluated{};
    HWND foreground{};DownRoute route{DownRoute::Pass};core::v1::Action action{core::v1::Action::None};
    unsigned modifiers{},clicks{};bool routed{},consume{};
    bool candidate_eligible{},candidate_pressing{};POINT candidate_anchor{};
    DWORD previous_down_time{};std::uint64_t previous_source_key{};
    int previous_region{-1},previous_button{};unsigned previous_modifiers{},double_click_time{};
    bool matching_up{},up_push_attempted{},up_push_succeeded{};
    bool native_release_push_attempted{},native_release_push_succeeded{};
};
static_assert(std::is_trivially_copyable_v<ButtonAdmission>);
// Separate 64-slot SPSC: a diagnostic loss must never fail the input queue,
// revoke authority or overwrite unread facts. Only the hook owner publishes.
class ButtonAdmissionQueue {
public:
    static constexpr unsigned capacity=64;
    bool push(bool enabled,const ButtonAdmission& value)noexcept{
        if(!enabled)return false;
        const auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=capacity){dropped_.fetch_add(1,std::memory_order_relaxed);return false;}
        rows_[w%capacity]=value;write_.store(w+1,std::memory_order_release);return true;
    }
    bool pop(ButtonAdmission& value)noexcept{
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        value=rows_[r%capacity];read_.store(r+1,std::memory_order_release);return true;
    }
    std::uint64_t dropped()const noexcept{return dropped_.load(std::memory_order_relaxed);}
private:
    std::array<ButtonAdmission,capacity> rows_{};
    std::atomic<unsigned> read_{},write_{};std::atomic<std::uint64_t> dropped_{};
};
// One process-lifetime TEST permit. Only the GUI's expensive exact disposable
// guest proof may publish it. The input callback reads immutable identity, not
// markers/files/tokens, and never grants operation authority from this permit.
struct GuestQueueFailurePermit {
    bool exact_guest_verified{};
    std::uint64_t generation{};
    Snapshot source;
    std::string run_id,driver_sha256;
};
inline bool guest_queue_failure_matches(const GuestQueueFailurePermit& permit,
    const InputEvent& event,const Snapshot* active_source,std::uint64_t authority,
    bool healthy,bool available,bool paused,bool active_pair,bool capture_ready,bool up_or_abort) noexcept {
    const auto same=[](const Snapshot& a,const Snapshot& b){return a.hwnd&&a.key&&a.born&&
        a.hwnd==b.hwnd&&a.key==b.key&&a.pid&&a.pid==b.pid&&a.tid&&a.tid==b.tid&&a.born==b.born&&
        a.exe==b.exe&&a.window_class==b.window_class;};
    return permit.exact_guest_verified&&permit.generation&&event.kind==InputKind::Move&&
        event.generation==permit.generation&&authority==permit.generation&&healthy&&available&&!paused&&
        active_pair&&capture_ready&&!up_or_abort&&active_source&&event.prepared&&
        same(permit.source,*active_source)&&same(permit.source,event.prepared->source);
}
struct GuestQueueFailureObservation {
    std::uint64_t generation{},qpc{};HWND source{};DWORD pid{},tid{};
    unsigned read{},write{},next{};bool actual_full{};
};
// Written once by the real input owner after the explicit guest-only stop.
// Publication is separate from the GUI's later observation of a signaled
// thread HANDLE; neither a posted message nor healthy=false proves exit.
struct GuestCallbackOwnerStopObservation {
    std::uint64_t generation{},trigger_qpc{},exit_record_qpc{},unhook_before_qpc{},unhook_after_qpc{};
    DWORD owner_tid{},unhook_error{},hotkey_error{},raw_error{},receiver_error{};
    std::uintptr_t hook{};
    bool unhook_attempted{},unhook_return{},hotkey_attempted{},hotkey_return{},raw_attempted{},raw_return{};
    bool receiver_attempted{},receiver_return{},shield_stop_clean{};
    std::array<bool,6> event_attempted{},event_return{};
    std::array<DWORD,6> event_error{};
};
struct GuestCallbackStopDispatch {
    bool revoke_exact_generation{},finish_gui_gesture{},clear_native_pending{};
};
inline GuestCallbackStopDispatch guest_callback_stop_dispatch(bool triggered,std::uint64_t fault_generation,
    std::uint64_t gui_generation,std::uint64_t native_generation)noexcept{
    const bool exact=triggered&&fault_generation;
    return {exact,exact&&gui_generation==fault_generation&&(!native_generation||native_generation==fault_generation),
        exact&&native_generation==fault_generation};
}
inline bool guest_callback_owner_stop_matches(const GuestQueueFailurePermit& permit,const Snapshot* actual,
    std::uint64_t generation,std::uint64_t authority,bool healthy,bool available,bool paused,
    bool active_pair,bool capture_ready,bool up_or_abort)noexcept{
    return permit.exact_guest_verified&&permit.generation&&generation==permit.generation&&authority==permit.generation&&
        healthy&&available&&!paused&&active_pair&&capture_ready&&!up_or_abort&&actual&&
        actual->hwnd&&actual->hwnd==permit.source.hwnd&&actual->key&&actual->key==permit.source.key&&
        actual->pid&&actual->pid==permit.source.pid&&actual->tid&&actual->tid==permit.source.tid&&
        actual->born&&actual->born==permit.source.born&&actual->exe==permit.source.exe&&actual->window_class==permit.source.window_class;
}

// Dedicated message owner. No COM, file IO, cross-process messaging or geometry
// in hook callbacks. The GUI prepares identity/hit-test snapshots beforehand.
class InputService {
public:
    explicit InputService(operations::InteractiveDesktopContextRef desktop_context={});
    bool start(HWND owner,bool diagnostic=false);
    void stop();
    ~InputService(){stop();}
    void prepare(std::shared_ptr<const Prepared> value);
    bool pop(InputEvent&);
    POINT latest()const;
    void prepared_point(){prepare_pending_=false;}
    void pause(bool value);
    bool allowed(std::uint64_t g)const {return g && authorized_->load()==g && !paused_;}
    bool context_allowed(std::uint64_t g)const {
        const auto active=authorized_->load();const auto terminal=completion_->load();
        return g&&available_->load()&&!paused_&&
            (operation_permission(g,active,terminal,true,false)||operation_permission(g,active,terminal,true,true));
    }
    bool completion_allowed(std::uint64_t g)const {
        const auto active=authorized_->load();const auto terminal=completion_->load();
        return g&&operation_permission(g,active,terminal,available_->load()&&!paused_,true);
    }
    bool motion_allowed(const InputEvent& event,bool terminal,bool terminal_consumed)const {
        const auto active=authorized_->load();const auto completion=completion_->load();
        return dispatch_motion_permission(event,terminal,active,completion,
            available_->load()&&!paused_,terminal_consumed);
    }
    void revoke(core::v1::End reason=core::v1::End::SourceLost){
        if(reason!=core::v1::End::NormalUp)retire_capture_pair(0,reason);
        if(reason!=core::v1::End::NormalUp)*completion_=0;
        const auto retired=authorized_->exchange(0);const auto id=thread_id_.load();
        if(retired&&id)PostThreadMessageW(id,WM_APP+2,static_cast<WPARAM>(retired&~pending_capture_mask),static_cast<LPARAM>(reason));
    }
    bool revoke_generation(std::uint64_t generation,core::v1::End reason=core::v1::End::SourceLost){
        if(reason!=core::v1::End::NormalUp)retire_capture_pair(generation,reason);
        bool retired=revoke_exact_generation(*authorized_,*completion_,generation);
        auto pending=generation|pending_capture_mask;
        retired=authorized_->compare_exchange_strong(pending,0)||retired;const auto id=thread_id_.load();
        if(retired&&id)PostThreadMessageW(id,WM_APP+2,static_cast<WPARAM>(generation),static_cast<LPARAM>(reason));
        return retired;
    }
    void finish_generation(std::uint64_t generation,core::v1::End reason){
        if(reason!=core::v1::End::NormalUp){revoke_generation(generation,reason);return;}
        finish_exact_generation(*authorized_,*completion_,generation,reason);
    }
    // Safe to retain at a bounded native worker boundary, even if the UI
    // retires/destroys this service. Only shared atomics are captured.
    void finish_completion(std::uint64_t generation){completion_->compare_exchange_strong(generation,0);}
    std::function<bool()> operation_guard(std::uint64_t generation,bool normal_up_completion=false,HWND source=nullptr)const {
        return [authority=authorized_,completion=completion_,available=available_,generation,normal_up_completion,source]{
            // Explicit order matters: an older terminal token must not be
            // paired with active=0 read after a newer DOWN/UP cycle. Function
            // argument evaluation order alone does not establish this order.
            const auto active=authority->load();const auto terminal=completion->load();
            return (!generation||(source&&GetForegroundWindow()==source))&&
                operation_permission(generation,active,terminal,available->load(),normal_up_completion);
        };
    }
    bool healthy()const {return healthy_;}
    // GUI only. An ordinary host/unknown guest can never arm this operation.
    // The Run and exact already-active owned source are checked independently
    // of normal Catalog eligibility. Failed proof does not fall back to it.
    bool arm_guest_queue_failure(std::string_view run_id,std::uint64_t generation,HWND source,std::string& why);
    std::string guest_queue_failure_json()const;
    bool request_guest_callback_owner_stop(std::string_view run_id,std::uint64_t generation,HWND source,std::string& why);
    std::string guest_callback_owner_stop_json();
    bool guest_callback_owner_stop_clean()const;
    bool guest_callback_owner_stop_triggered()const{return guest_callback_stop_triggered_.load(std::memory_order_acquire);}
    std::uint64_t guest_callback_owner_stop_generation()const{
        return guest_callback_stop_state_.load(std::memory_order_acquire)>=2?guest_callback_stop_permit_.generation:0;
    }
    std::uint64_t max_callback_ticks()const{return max_ticks_;}
    // Join stop() before final reporting. A live relaxed snapshot is bounded
    // and race-free but not an atomic view of simultaneous callback updates.
    core::v1::TimingSnapshot callback_timing()const{return callback_timing_.snapshot();}
    std::uint64_t counter_frequency()const{return counter_frequency_;}
    std::uint64_t emergency_hotkey_messages()const{return emergency_hotkey_messages_;}
    MouseUpCallbackSnapshot mouse_up_callbacks()const{return up_observation_.snapshot();}
    bool pop_button_admission(ButtonAdmission& value){return button_admissions_.pop(value);}
    std::uint64_t button_admission_dropped()const{return button_admissions_.dropped();}
    std::uint64_t button_admission_wake_failures()const{return button_admission_wake_failures_.load();}
    GestureCaptureLease capture_context(std::uint64_t generation)const;
    bool native_move_pending(std::uint64_t generation)const;
    bool native_move_in_progress()const;
    bool native_release_preparation_idle()const{
        return !paused_.load()&&!stop_requested_.load()&&available_->load()&&
            !authorized_->load()&&!completion_->load()&&!native_move_in_progress();
    }
    bool request_native_move_isolation(std::uint64_t,const std::shared_ptr<const NativeMoveReceipt>&);
    bool claim_native_move_cancel(std::uint64_t);
    bool request_native_move_capture(std::uint64_t,const std::shared_ptr<const NativeMoveReceipt>&,const Snapshot&);
    bool capture_route_enabled()const{return capture_route_enabled_;}
    bool capture_route_is_guest()const{return capture_route_is_guest_;}
    const std::string& capture_route_mode()const{return capture_route_mode_;}
    const std::string& capture_route_reason()const{return capture_route_reason_;}
    operations::GestureShieldStopFacts capture_stop_facts()const{return capture_stop_facts_;}
private:
    static constexpr std::uint64_t pending_capture_mask=input_pending_capture_mask;
    struct CaptureBridge;
    struct PreparedBundle;
    static LRESULT CALLBACK mouse(int,WPARAM,LPARAM);
    static void CALLBACK event(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
    static LRESULT CALLBACK receiver(HWND,UINT,WPARAM,LPARAM);
    void run();
    bool push(InputEvent);
    void abort(core::v1::End,std::optional<InputAbortObservation> observation=std::nullopt);
    bool begin_capture(const InputEvent&);
    void drain_capture_events();
    void request_capture_stop(operations::GestureShieldRemovalReason);
    void retire_capture_pair(std::uint64_t generation=0,core::v1::End reason=core::v1::End::SourceLost);
    bool arm_guest_input_failure(std::string_view,std::uint64_t,HWND,bool callback_owner_stop,std::string&);
    static VOID CALLBACK guest_callback_owner_exited(PVOID,BOOLEAN);
    struct GuestCallbackWaitContext {
        std::atomic<HWND> owner{};
        std::atomic<bool> signaled{},notified{};
    };
    static InputService* instance_;
    operations::InteractiveDesktopContextRef desktop_context_;
    HWND owner_{},receiver_{};
    std::atomic<DWORD> thread_id_{};
    HANDLE ready_{};
    std::thread thread_;
    std::atomic<bool> paused_{false},healthy_{false},prepare_pending_{false},stop_requested_{false};
    std::shared_ptr<std::atomic<std::uint64_t>> authorized_{std::make_shared<std::atomic<std::uint64_t>>(0)};
    std::shared_ptr<std::atomic<std::uint64_t>> completion_{std::make_shared<std::atomic<std::uint64_t>>(0)};
    std::shared_ptr<std::atomic<bool>> available_{std::make_shared<std::atomic<bool>>(false)};
    std::atomic<std::uint64_t> point_{0},max_ticks_{0},emergency_hotkey_messages_{0};
    std::uint64_t counter_frequency_{};
    std::atomic<std::uint64_t> input_owner_start_serial_{};
    std::uint64_t up_callback_count_{};MouseUpCallbackObservation up_observation_;
    bool button_diagnostic_{};std::uint64_t button_admission_sequence_{};
    ButtonAdmissionQueue button_admissions_;std::atomic<std::uint64_t> button_admission_wake_failures_{};
    core::v1::TimingHistogram callback_timing_;
    std::atomic<std::shared_ptr<const PreparedBundle>> prepared_;
    std::array<InputEvent,256> queue_{};
    std::atomic<unsigned> read_{0},write_{0};
    // 0 empty, 1 publishing, 2 armed, 3 consumed. Never reset by Pause/Resume.
    // The permit and fired observation are each immutable after release
    // publication, so GUI and input callbacks never race a rewritten tuple.
    std::atomic<unsigned> guest_queue_failure_state_{};
    GuestQueueFailurePermit guest_queue_failure_permit_;
    GuestQueueFailureObservation guest_queue_failure_observation_;
    std::atomic<bool> guest_queue_failure_observation_ready_{};
    // A second narrowly scoped one-shot. Neither permit resets on recovery.
    std::atomic<unsigned> guest_callback_stop_state_{};
    GuestQueueFailurePermit guest_callback_stop_permit_;
    GuestCallbackOwnerStopObservation guest_callback_stop_observation_;
    std::atomic<bool> guest_callback_stop_triggered_{},guest_callback_stop_exit_ready_{};
    std::atomic<bool> guest_callback_stop_posted_{},guest_callback_wait_signaled_{},guest_callback_wait_notified_{};
    std::atomic<DWORD> guest_callback_stop_post_error_{};
    std::atomic<unsigned> guest_callback_stop_rejection_{};
    HANDLE guest_callback_wait_{},guest_callback_wait_thread_{};
    std::unique_ptr<GuestCallbackWaitContext> guest_callback_wait_context_;
    bool guest_callback_wait_registered_{},guest_callback_gui_exit_observed_{};
    DWORD guest_callback_wait_register_error_{},guest_callback_gui_exit_query_error_{};
    bool guest_callback_gui_exit_query_attempted_{},guest_callback_gui_exit_query_return_{};
    bool guest_callback_wait_unregister_attempted_{},guest_callback_wait_unregister_return_{};
    DWORD guest_callback_wait_unregister_error_{};
    DWORD guest_callback_gui_wait_result_{WAIT_FAILED},guest_callback_gui_wait_error_{},guest_callback_gui_exit_code_{STILL_ACTIVE};
    bool consumed_down_{},native_move_{};
    bool aborted_{};
    int button_{};
    std::uint64_t generation_{},next_generation_{};
    unsigned modifiers_{};
    DWORD last_down_{};
    DWORD active_down_stamp_{};
    int last_region_{-1};
    int last_button_{};
    unsigned last_modifiers_{};
    std::uint64_t last_source_key_{},deadline_{};
    ClickCandidate click_candidate_;
    UINT double_click_time_{};
    int double_click_x_{},double_click_y_{};
    std::int64_t drag_x_{},drag_y_{};
    std::shared_ptr<const Prepared> active_prepared_;
    core::v1::Action active_action_{core::v1::Action::None};
    core::v1::End active_end_{core::v1::End::NormalUp};
    HHOOK hook_{};
    std::array<HWINEVENTHOOK,6> events_{};
    std::uint64_t native_receipt_sequence_{};
    std::shared_ptr<CaptureBridge> capture_bridge_;
    std::unique_ptr<operations::GestureInputShield> shield_;
    operations::GestureShieldStopFacts capture_stop_facts_{};
    bool capture_route_enabled_{},capture_route_is_guest_{},capture_pair_{};
    CapturedMoveSlot captured_move_;
    std::optional<InputEvent> capture_pending_down_;
    std::string capture_route_reason_{"input_resource_route_not_initialized"};
    std::string capture_route_mode_{"unavailable"};
};
}
