#pragma once
#include "window_catalog.h"
#include "core/v1/timing_histogram.h"
#include "platform/windows/operations/gesture_input_shield.h"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

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
// Only this ordinary, non-maximized titlebar route changes entry mechanism.
// Ctrl, mapped actions, all resize edges, double-clicks and host operation keep
// their existing route. A candidate DOWN is passed to Windows, not consumed.
inline bool native_plain_move_entry(bool guest,const Prepared& p,int button,int clicks,unsigned mods,DownDecision route){
    return guest&&route.route==DownRoute::Gesture&&button==1&&clicks==1&&mods==0&&p.region==0&&!p.source.maximized;
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
enum class InputKind { Down, Move, Up, Abort, Mapped, Resource, NativeMovePending, NativeMoveStart, NativeMoveEnd, NativeMoveReleased };
enum class InputOrigin { MouseHook, ShieldLegacy, PlatformAbort, ShieldResource, NativeMoveEvent };
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
};
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
// Dedicated message owner. No COM, file IO, cross-process messaging or geometry
// in hook callbacks. The GUI prepares identity/hit-test snapshots beforehand.
class InputService {
public:
    bool start(HWND owner);
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
    std::uint64_t max_callback_ticks()const{return max_ticks_;}
    // Join stop() before final reporting. A live relaxed snapshot is bounded
    // and race-free but not an atomic view of simultaneous callback updates.
    core::v1::TimingSnapshot callback_timing()const{return callback_timing_.snapshot();}
    std::uint64_t counter_frequency()const{return counter_frequency_;}
    std::uint64_t emergency_hotkey_messages()const{return emergency_hotkey_messages_;}
    MouseUpCallbackSnapshot mouse_up_callbacks()const{return up_observation_.snapshot();}
    GestureCaptureLease capture_context(std::uint64_t generation)const;
    bool native_move_pending(std::uint64_t generation)const;
    bool native_move_in_progress()const;
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
    void abort(core::v1::End);
    bool begin_capture(const InputEvent&);
    void drain_capture_events();
    void request_capture_stop(operations::GestureShieldRemovalReason);
    void retire_capture_pair(std::uint64_t generation=0,core::v1::End reason=core::v1::End::SourceLost);
    static InputService* instance_;
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
    std::uint64_t up_callback_count_{};MouseUpCallbackObservation up_observation_;
    core::v1::TimingHistogram callback_timing_;
    std::atomic<std::shared_ptr<const PreparedBundle>> prepared_;
    std::array<InputEvent,256> queue_{};
    std::atomic<unsigned> read_{0},write_{0};
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
