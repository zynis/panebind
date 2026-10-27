#include "platform/windows/product/input_service.h"
#include "platform/windows/product/window_writer.h"
#include "platform/windows/product/tray_callback_route.h"
#include "core/v1/gesture.h"
#include "core/v1/layout.h"
#include "core/geometry/magnet_constraint_solver.h"
#include "core/behavior/move_handoff_gate.h"

#include <iostream>

namespace product = panebind::platform::windows::product;
namespace v1 = panebind::core::v1;
namespace {
int checks{}, failures{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
// Uses the exact table builder and route predicate called by the production
// GUI adapter / low-level callback. It never starts InputService, installs a
// hook, synthesizes input, queries foreground, or opens a user window.
int main() {
    using TrayRoute=product::TrayCallbackRoute;
    check(product::tray_callback_route(NIN_KEYSELECT)==TrayRoute::Settings,
          "actual v4 keyboard icon activation opens the existing Settings route");
    check(product::tray_callback_route(NIN_SELECT)==TrayRoute::Settings&&
          product::tray_callback_route(WM_LBUTTONDBLCLK)==TrayRoute::Settings,
          "existing select and double-click Settings routes are retained");
    check(product::tray_callback_route(WM_CONTEXTMENU)==TrayRoute::Menu&&
          product::tray_callback_route(WM_RBUTTONUP)==TrayRoute::Menu,
          "context and legacy right-up retain the existing menu route");
    const LPARAM packed_key=MAKELPARAM(NIN_KEYSELECT,1);
    check(product::tray_callback_route(LOWORD(packed_key))==TrayRoute::Settings&&HIWORD(packed_key)==1,
          "production v4 LOWORD event decoding is independent of the packed icon ID");
    for(const UINT ignored:{0u,static_cast<UINT>(WM_MOUSEMOVE),static_cast<UINT>(WM_LBUTTONDOWN),
            static_cast<UINT>(NIN_BALLOONSHOW),static_cast<UINT>(NIN_BALLOONUSERCLICK)}){
        check(product::tray_callback_route(ignored)==TrayRoute::Ignored,
              "unhandled tray observations cannot open Settings or a menu");
    }
    product::Prepared native_down;
    native_down.source.hwnd=reinterpret_cast<HWND>(101);native_down.source.pid=21;native_down.source.tid=31;
    native_down.source.key=41;native_down.source.born=51;native_down.source.exe=L"C:\\fixture.exe";
    native_down.source.window_class=L"fixture";native_down.source.dpi=96;
    native_down.source.visible={100,100,360,460};native_down.source.positioning={93,100,367,467};
    native_down.source.work={0,0,1920,1080};native_down.region=0;
    product::prepare_mappings(native_down,v1::default_settings());
    // Actual UP -> same-point second DOWN while the GUI refresh is delayed.
    // The production hook publishes this same preallocated/CAS path; no test
    // adapter can simply declare a stale cache to be ordinary preparation.
    auto click_prepared=std::make_shared<product::Prepared>(native_down);
    click_prepared->region=1;click_prepared->point={93,250};click_prepared->config_generation=8;
    product::prepare_mappings(*click_prepared,v1::default_settings());
    struct ClickBundle {std::shared_ptr<const product::Prepared> prepared;bool continuation_only{};};
    const auto first_bundle=std::make_shared<const ClickBundle>(ClickBundle{click_prepared,false});
    const auto continuation_bundle=std::make_shared<const ClickBundle>(ClickBundle{click_prepared,true});
    std::atomic<std::shared_ptr<const ClickBundle>> prepared_slot{first_bundle};
    product::ClickCandidate first_click;first_click.begin(click_prepared->point,1,true);first_click.release(1);
    const auto release_ready=[&](bool resource=false,bool aborted=false,bool available=true){
        return product::native_click_continuation_ready(true,resource,aborted,available,first_click.eligible,
            first_click.pressing,first_click.anchor,click_prepared->point,1,1);};
    check(product::publish_native_click_continuation(prepared_slot,first_bundle,continuation_bundle,release_ready()),
          "real native first UP retains only its same-version double-click classification before GUI refresh");
    const auto is_second=[&](DWORD time,POINT point){return product::prepared_double_click_matches(*click_prepared,first_click,
        point,time,500,4,4,51593,1,1,0,click_prepared->source.key,1,0);};
    const auto second=prepared_slot.load();
    check(second&&second->continuation_only&&is_second(51625,click_prepared->point)&&
          product::prepared_click_admitted(*second->prepared,click_prepared->point,second->continuation_only,is_second(51625,click_prepared->point))&&
          product::route_down(*second->prepared,1,2,0).route==product::DownRoute::Mapped,
          "deterministic 32ms second edge DOWN reaches the actual mapped route without waiting for Prepared refresh");
    check(!product::prepared_click_admitted(*click_prepared,click_prepared->point,true,is_second(52100,click_prepared->point)),
          "continuation-only cache cannot authorize a later ordinary single click");
    check(!product::prepared_click_admitted(*click_prepared,{94,250},true,is_second(51625,{94,250})),
          "even an OS double-click candidate at another point cannot use the retained exact-point classification");
    check(!release_ready(true)&&!release_ready(false,true)&&!release_ready(false,false,false),
          "live old resources or abort/stop cannot publish native-click continuation");
    first_click.begin(click_prepared->point,1,true);first_click.motion({130,250},4,4);first_click.release(1);
    check(!release_ready(),"returning after a native drag cannot retain a first-click continuation");
    first_click.begin(click_prepared->point,1,true);first_click.release(1);
    const auto newer_bundle=std::make_shared<const ClickBundle>(ClickBundle{click_prepared,false});
    prepared_slot.store(newer_bundle);
    check(!product::publish_native_click_continuation(prepared_slot,first_bundle,continuation_bundle,true)&&prepared_slot.load()==newer_bundle,
          "late native UP cannot replace a newer GUI publication");
    prepared_slot.store({});
    check(!product::publish_native_click_continuation(prepared_slot,first_bundle,continuation_bundle,true)&&!prepared_slot.load(),
          "explicit GUI invalidation cannot be resurrected by an old native UP");
    prepared_slot.store(first_bundle);
    check(product::clear_exact_preparation(prepared_slot,first_bundle)&&!prepared_slot.load(),
          "ineligible native release clears its old full preparation rather than retaining ordinary stale authority");
    prepared_slot.store(newer_bundle);
    check(!product::clear_exact_preparation(prepared_slot,first_bundle)&&prepared_slot.load()==newer_bundle,
          "old release cleanup also preserves a newer GUI preparation");
    auto corner_prepared=*click_prepared;corner_prepared.region=5;product::prepare_mappings(corner_prepared,v1::default_settings());
    check(product::route_down(corner_prepared,1,2,0).route==product::DownRoute::Gesture,
          "actual contextual corner route remains a gesture requiring separate GUI capture approval");
    product::InputEvent continuation_down;continuation_down.kind=product::InputKind::ContinuationPending;
    continuation_down.generation=81;continuation_down.down_serial=12;continuation_down.clicks=2;continuation_down.button=1;
    continuation_down.point=click_prepared->point;continuation_down.prepared=click_prepared;
    continuation_down.second_click_continuation=true;continuation_down.hook_observed_qpc=516579293;
    continuation_down.native_message_time=51625;
    const auto pending=[&](std::uint64_t authority=81|product::input_pending_capture_mask,std::uint64_t serial=12,
                          bool available=true,bool paused=false,bool stop=false){
        return product::continuation_pending_current(continuation_down,authority,serial,available,paused,stop);};
    check(pending(),"second click reserves a generation but does not yet grant writer/resource authority");
    check(!pending(0)&&!pending(82|product::input_pending_capture_mask)&&!pending(81|product::input_pending_capture_mask,13)&&
          !pending(81|product::input_pending_capture_mask,12,false)&&!pending(81|product::input_pending_capture_mask,12,true,true)&&
          !pending(81|product::input_pending_capture_mask,12,true,false,true),
          "actual early UP/new DOWN/unavailable/Pause/stop all reject delayed GUI approval");
    const auto fresh_matches=[&](const product::Snapshot& actual,std::uint64_t cfg=8,int hit=1,bool exact_hit=true,bool foreground=true,bool idle=true){
        return product::continuation_source_context_matches(*click_prepared,actual,cfg,click_prepared->point,hit,exact_hit,foreground,idle);};
    check(fresh_matches(click_prepared->source),"GUI continuation predicate accepts actual unchanged identity/geometry/point/context");
    auto changed=click_prepared->source;++changed.born;
    check(!fresh_matches(changed),"same HWND with different lifetime rejects continuation");
    changed=click_prepared->source;changed.visible={100,101,360,461};
    check(!fresh_matches(changed),"external translation cannot become the continuation command baseline");
    changed=click_prepared->source;changed.positioning={93,99,367,466};
    check(!fresh_matches(changed),"changed native frame cannot use old continuation geometry");
    changed=click_prepared->source;changed.dpi=144;
    check(!fresh_matches(changed),"DPI change rejects continuation before command or capture");
    changed=click_prepared->source;changed.monitor=reinterpret_cast<HMONITOR>(77);
    check(!fresh_matches(changed),"monitor change rejects continuation");
    changed=click_prepared->source;changed.work={0,0,1910,1080};
    check(!fresh_matches(changed)&&!fresh_matches(click_prepared->source,9)&&!fresh_matches(click_prepared->source,8,2)&&
          !fresh_matches(click_prepared->source,8,1,false)&&!fresh_matches(click_prepared->source,8,1,true,false)&&
          !fresh_matches(click_prepared->source,8,1,true,true,false),
          "work/config/native-hit/foreign-hit/foreground/GUI changes reject delayed continuation");
    auto approval=continuation_down;approval.kind=product::InputKind::Down;
    check(product::continuation_capture_approval_matches(continuation_down,approval),
          "owner capture request requires the exact GUI-approved original second DOWN");
    ++approval.down_serial;
    check(!product::continuation_capture_approval_matches(continuation_down,approval),
          "an approval for another actual DOWN serial cannot create capture resources");
    std::atomic<std::uint64_t> continuation_authority{81|product::input_pending_capture_mask};
    check(product::arm_exact_capture_generation(continuation_authority,81)&&
          product::operation_permission(81,continuation_authority.load(),0,true,false),
          "fresh mapped continuation activates only its exact reserved generation");
    continuation_authority=0;
    check(!product::arm_exact_capture_generation(continuation_authority,81)&&
          !product::operation_permission(81,continuation_authority.load(),0,true,false),
          "UP before approval cannot restore continuation writer authority");
    check(!product::capture_request_generation_current(80,81)&&product::capture_request_generation_current(81,81),
          "old first-click capture message cannot abort a current mapped second-click generation");
    std::uint64_t request_generation=80;
    check(product::capture_request_generation_current(80,request_generation),"old request may initially be current before a native query");
    request_generation=81;
    check(!product::capture_request_generation_current(80,request_generation),
          "nested UP then DOWN during the read-only query prevents old failure cleanup from revoking the new generation");
    // Same pure match that InputService::push uses after the real GUI-only
    // guest proof. This does not assert that a native guest ran or a queue filled.
    product::GuestQueueFailurePermit fault_permit{true,61,native_down.source,"0123456789abcdef0123456789abcdef","hash"};
    product::InputEvent fault_move;fault_move.kind=product::InputKind::Move;fault_move.generation=61;
    fault_move.prepared=std::make_shared<product::Prepared>(native_down);
    const auto fault_match=[&](const product::GuestQueueFailurePermit& permit,const product::InputEvent& event,
                              const product::Snapshot* actual,std::uint64_t authority=61,bool ready=true,bool up=false){
        return product::guest_queue_failure_matches(permit,event,actual,authority,true,true,false,true,ready,up);};
    check(fault_match(fault_permit,fault_move,&native_down.source),"actual push fault predicate accepts only the armed active Move identity");
    const auto callback_stop_match=[&](const product::GuestQueueFailurePermit& permit,const product::Snapshot* actual,
                                     std::uint64_t generation=61,std::uint64_t authority=61,bool up=false){
        return product::guest_callback_owner_stop_matches(permit,actual,generation,authority,true,true,false,true,true,up);};
    check(callback_stop_match(fault_permit,&native_down.source),"real callback owner handler accepts only the exact still-active stop tuple");
    auto callback_unproved=fault_permit;callback_unproved.exact_guest_verified=false;
    check(!callback_stop_match(callback_unproved,&native_down.source),"unproved host cannot stop the callback owner");
    check(!callback_stop_match(fault_permit,&native_down.source,62),"callback stop cannot follow a later source generation");
    check(!callback_stop_match(fault_permit,&native_down.source,61,0),"UP-revoked callback authority cannot be stopped by an old request");
    check(!callback_stop_match(fault_permit,&native_down.source,61,61,true),"matching UP before owner handling rejects callback stop");
    auto callback_other=native_down.source;++callback_other.key;
    check(!callback_stop_match(fault_permit,&callback_other),"same HWND without exact current key cannot stop the owner");
    check(static_cast<int>(v1::End::Exit)==8&&static_cast<int>(v1::End::InputUnavailable)==9,"new input-owner end preserves every existing terminal numeric value");
    const auto stopped_current=product::guest_callback_stop_dispatch(true,61,61,0);
    check(stopped_current.revoke_exact_generation&&stopped_current.finish_gui_gesture&&!stopped_current.clear_native_pending,
          "actual stop notification may finish only the still-current fault generation");
    const auto late_exit=product::guest_callback_stop_dispatch(true,61,62,0);
    check(late_exit.revoke_exact_generation&&!late_exit.finish_gui_gesture&&!late_exit.clear_native_pending,
          "late owner exit retains old facts without finishing the recovered next gesture");
    const auto old_gui_new_native=product::guest_callback_stop_dispatch(true,61,61,62);
    check(!old_gui_new_native.finish_gui_gesture&&!old_gui_new_native.clear_native_pending,
          "old GUI gesture cannot make callback notification finish a newer pending native handoff");
    const auto exit_before_stop=product::guest_callback_stop_dispatch(true,61,0,0);
    check(exit_before_stop.revoke_exact_generation&&!exit_before_stop.finish_gui_gesture,
          "wp4-first retirement leaves subsequent wp3 factual only and never invokes global no-gesture finish");
    const auto old_native=product::guest_callback_stop_dispatch(true,61,0,61);
    check(old_native.clear_native_pending&&!old_native.finish_gui_gesture,"old native pending is cleared without global gesture finish");
    check(!product::guest_callback_stop_dispatch(false,61,61,0).revoke_exact_generation,
          "untriggered callback request cannot revoke any generation");
    std::atomic<std::uint64_t> recovered_authority{62},recovered_completion{63};
    check(!product::revoke_exact_generation(recovered_authority,recovered_completion,61)&&recovered_authority==62&&recovered_completion==63,
          "actual old-generation revocation CAS leaves successor authority and terminal token unchanged");
    auto unproved=fault_permit;unproved.exact_guest_verified=false;
    check(!fault_match(unproved,fault_move,&native_down.source),"ordinary or unproved guest cannot use the fault predicate");
    auto other_fault_move=fault_move;other_fault_move.generation=62;
    check(!fault_match(fault_permit,other_fault_move,&native_down.source),"fault permit does not follow the next gesture generation");
    for(const auto kind:{product::InputKind::Down,product::InputKind::Up,product::InputKind::Abort,product::InputKind::Resource}){
        other_fault_move=fault_move;other_fault_move.kind=kind;
        check(!fault_match(fault_permit,other_fault_move,&native_down.source),"fault injection cannot consume DOWN UP abort or resource observations");}
    auto other_fault_source=native_down.source;other_fault_source.key++;
    check(!fault_match(fault_permit,fault_move,&other_fault_source)&&!fault_match(fault_permit,fault_move,nullptr),
          "old or missing active source cannot trigger a queued fault");
    auto other_fault_prepared=std::make_shared<product::Prepared>(native_down);other_fault_prepared->source.tid++;
    other_fault_move=fault_move;other_fault_move.prepared=other_fault_prepared;
    check(!fault_match(fault_permit,other_fault_move,&native_down.source),"actual MOVE must carry the same source HWND PID TID birth class and key");
    check(!fault_match(fault_permit,fault_move,&native_down.source,0)&&
          !fault_match(fault_permit,fault_move,&native_down.source,61,true,true)&&
          !fault_match(fault_permit,fault_move,&native_down.source,61,false),
          "UP revocation missing CaptureReady or abort cannot fire or reauthorize a fault permit");
    // The actual consumed-DOWN adapter and shield source-state gate used by
    // production. No window/API/input is mocked into a successful capture.
    namespace max_shield=panebind::platform::windows::operations;
    product::InputEvent max_down;max_down.kind=product::InputKind::Down;max_down.generation=61;
    max_down.point={27,0};max_down.native_message_time=73;max_down.hook_observed_qpc=1013;
    auto max_prepared=std::make_shared<product::Prepared>(native_down);
    max_prepared->source.maximized=true;max_prepared->source.can_resize=true;max_prepared->source.visible={0,0,1920,1040};
    max_prepared->source.positioning={-7,-7,1927,1047};max_down.prepared=max_prepared;
    max_shield::GestureShieldConsumedDownRequest max_request;
    max_request.gesture={61,max_prepared->source.hwnd,21,31,30000};
    max_request.down_point=max_down.point;max_request.down_message_time=max_down.native_message_time;
    max_request.down_qpc=max_down.hook_observed_qpc;max_request.source_was_maximized=true;
    check(product::consumed_capture_request_matches_down(max_request,max_down)&&
          max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::ConsumedDown,true,true),
          "actual plain maximized caption consumed-DOWN fact reaches the authorized shield source-state path");
    check(!max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::NativeMoveHandoff,true,false)&&
          !max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::NativeMoveHandoff,true,true)&&
          !max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::ConsumedDown,true,false),
          "native/MVP entry and a default false consumed fact still reject a maximized source");
    check(max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::NativeMoveHandoff,false,false)&&
          max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::ConsumedDown,false,false)&&
          max_shield::gesture_shield_source_state_allowed(max_shield::GestureShieldEntryMode::ConsumedDown,false,true),
          "ordinary source remains admitted and actual later normal state preserves the historical maximized DOWN fact");
    auto altered_request=max_request;altered_request.source_was_maximized=false;
    check(!product::consumed_capture_request_matches_down(altered_request,max_down),
          "adapter cannot drop the actual maximized source fact to borrow the ordinary contract");
    auto altered_down=max_down;auto ordinary_prepared=std::make_shared<product::Prepared>(native_down);
    altered_down.prepared=ordinary_prepared;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "a request cannot invent maximized source authority for an ordinary immutable Prepared");
    altered_request.source_was_maximized=false;
    check(product::consumed_capture_request_matches_down(altered_request,altered_down),
          "unchanged ordinary consumed-DOWN request still matches its actual immutable source");
    auto resize_prepared=std::make_shared<product::Prepared>(*max_prepared);resize_prepared->region=2;
    altered_down=max_down;altered_down.prepared=resize_prepared;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "maximized Resize is not admitted by the plain-caption repair");
    auto mapped_prepared=std::make_shared<product::Prepared>(*max_prepared);
    mapped_prepared->mappings[0]=v1::Action::Center;altered_down.prepared=mapped_prepared;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "an explicit mapped action cannot borrow maximized gesture capture");
    altered_down=max_down;altered_down.modifiers=4;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "Shift bypass is not converted to maximized consumed capture");
    altered_down.modifiers=2;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "the plain-maximized repair does not expand Ctrl entry");
    altered_down=max_down;altered_down.clicks=2;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "maximized double-click remains its distinct route");
    altered_down=max_down;altered_down.kind=product::InputKind::NativeMovePending;
    check(!product::consumed_capture_request_matches_down(max_request,altered_down),
          "a real native pending entry cannot borrow the consumed-DOWN maximum-state exception");
    bool wrong_identity_rejected=true;
    altered_request=max_request;++altered_request.gesture.generation;wrong_identity_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    altered_request=max_request;altered_request.gesture.source=reinterpret_cast<HWND>(102);wrong_identity_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    altered_request=max_request;++altered_request.gesture.process_id;wrong_identity_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    altered_request=max_request;++altered_request.gesture.thread_id;wrong_identity_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    check(wrong_identity_rejected,"consumed max source exception stays bound to exact generation HWND PID TID");
    bool wrong_down_rejected=true;
    altered_request=max_request;++altered_request.down_point.x;wrong_down_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    altered_request=max_request;++altered_request.down_message_time;wrong_down_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    altered_request=max_request;++altered_request.down_qpc;wrong_down_rejected&=!product::consumed_capture_request_matches_down(altered_request,max_down);
    check(wrong_down_rejected,"consumed max source exception stays bound to actual DOWN point message-time and QPC");
    auto corner_prepared=std::make_shared<product::Prepared>(native_down);corner_prepared->region=8;corner_prepared->source.can_resize=true;
    altered_down=max_down;altered_down.prepared=corner_prepared;altered_down.clicks=2;altered_down.modifiers=4;
    altered_request=max_request;altered_request.source_was_maximized=false;
    check(product::consumed_capture_request_matches_down(altered_request,altered_down),
          "existing nonmaximized Shift corner double-click consumed route is not narrowed");
    check(!max_shield::gesture_shield_consumed_down_active(max_request,61,true,true,true,false,100,200)&&
          !max_shield::gesture_shield_consumed_down_active(max_request,61,true,true,false,true,100,200)&&
          !product::operation_permission(61,0,0,true,false),
          "valid maximum-state facts never undo actual UP revocation abort or create placement permission");
    auto diagnostic_prepared=native_down;diagnostic_prepared.point={19,23};diagnostic_prepared.stamp=29;
    diagnostic_prepared.config_generation=37;diagnostic_prepared.region=8;
    diagnostic_prepared.source.feature_mask=13;diagnostic_prepared.source.can_resize=true;
    const auto copied=product::button_prepared_facts(&diagnostic_prepared);
    check(copied.present&&copied.root==diagnostic_prepared.source.hwnd&&copied.pid==21&&copied.tid==31&&
          copied.key==41&&copied.born==51&&copied.point.x==19&&copied.point.y==23&&copied.stamp==29&&
          copied.config_generation==37&&copied.region==8&&copied.feature_mask==13&&copied.can_resize,
          "button admission copies the actual cache identity and qualification fields without native queries");
    const auto absent=product::button_prepared_facts(nullptr);
    check(!absent.present&&!absent.root&&!absent.key&&absent.region==-1,
          "an actually read null Prepared is distinct from a qualified source");
    product::ButtonAdmissionQueue admission_queue;product::ButtonAdmission admission,received;
    admission.prepared=copied;admission.cache_read=true;admission.before.generation=7;admission.before.authority=7;
    check(!admission_queue.push(false,admission)&&!admission_queue.pop(received)&&admission_queue.dropped()==0,
          "ordinary-mode disabled admission recording publishes nothing and records no false loss");
    bool filled=true;for(unsigned n=0;n<product::ButtonAdmissionQueue::capacity;++n){
        admission.sequence=n+1;filled=admission_queue.push(true,admission)&&filled;
    }
    admission.sequence=65;admission.before.authority=0;
    check(filled&&!admission_queue.push(true,admission)&&admission_queue.dropped()==1,
          "all 64 diagnostic slots are usable and overflow is counted independently");
    bool ordered=true;for(unsigned n=0;n<product::ButtonAdmissionQueue::capacity;++n){
        const bool read=admission_queue.pop(received);ordered=read&&received.sequence==n+1&&
            received.before.authority==7&&received.prepared.key==41&&ordered;
    }
    check(ordered&&!admission_queue.pop(received),
          "diagnostic overflow does not overwrite queued identity, authority observations or callback order");
    filled=true;ordered=true;for(unsigned n=0;n<product::ButtonAdmissionQueue::capacity;++n){
        admission.sequence=101+n;filled=admission_queue.push(true,admission)&&filled;
    }
    for(unsigned n=0;n<product::ButtonAdmissionQueue::capacity;++n)
        ordered=admission_queue.pop(received)&&received.sequence==101+n&&ordered;
    check(filled&&ordered&&admission_queue.dropped()==1&&!admission_queue.pop(received),
          "the same production diagnostic queue wraps without losing or duplicating accepted facts");
    const product::ButtonAdmission unexamined;
    check(!unexamined.cache_read&&!unexamined.route_evaluated&&!unexamined.foreground_read&&
          !unexamined.matching_up&&!unexamined.up_push_attempted,
          "default route and zero values remain explicitly unexamined rather than admission evidence");
    check(product::route_down(native_down,1,1,0).route==product::DownRoute::Gesture&&native_down.region==0&&
          diagnostic_prepared.source.key==copied.key&&diagnostic_prepared.region==copied.region,
          "diagnostic copy and queue operations do not mutate the existing production route inputs");
    product::InputEvent native_release;native_release.kind=product::InputKind::NativeMoveReleased;native_release.generation=40;
    check(product::native_release_requests_preparation(native_release,40,false,true,true),
          "real early native UP requests fresh preparation after its matching GUI pending slot retires");
    check(product::native_release_requests_preparation(native_release,0,false,true,true),
          "START rejection clearing GUI pending before actual release does not lose the fresh preparation request");
    check(!product::native_release_requests_preparation(native_release,41,false,true,true)&&
          !product::native_release_requests_preparation(native_release,0,true,true,true)&&
          !product::native_release_requests_preparation(native_release,0,false,false,true)&&
          !product::native_release_requests_preparation(native_release,0,false,true,false),
          "a late release cannot refresh over a newer native/active generation, pending input resources or pause");
    native_release.kind=product::InputKind::NativeMoveStart;
    check(!product::native_release_requests_preparation(native_release,0,false,true,true),
          "native START alone cannot request the early-UP cache retirement repair");
    native_release.kind=product::InputKind::NativeMoveReleased;native_release.generation=0;
    check(!product::native_release_requests_preparation(native_release,0,false,true,true),
          "unknown release generation cannot request this fresh preparation path");
    const auto plain=product::route_down(native_down,1,1,0);
    check(product::native_plain_move_entry(true,native_down,1,1,0,plain)&&
          !product::native_plain_move_entry(false,native_down,1,1,0,plain)&&
          !product::native_plain_move_entry(true,native_down,1,1,2,plain)&&
          !product::native_plain_move_entry(true,native_down,1,2,0,plain)&&
          !product::native_plain_move_entry(true,native_down,1,1,0,{product::DownRoute::Mapped,v1::Action::Center}),
          "legacy plain-Move predicate excludes disabled, Ctrl, double-click and mapped routes");
    auto native_edge=native_down;native_edge.region=1;
    auto native_max=native_down;native_max.source.maximized=true;
    check(!product::native_plain_move_entry(true,native_edge,1,1,0,plain)&&
          !product::native_plain_move_entry(true,native_max,1,1,0,plain),
          "legacy plain-Move predicate remains separate from Resize and maximized restore entry");
    auto geometry_down=native_down;geometry_down.source.can_resize=true;
    geometry_down.source.monitor=reinterpret_cast<HMONITOR>(401);
    check(product::native_geometry_entry(true,geometry_down,1,1,0,product::route_down(geometry_down,1,1,0))&&
          product::native_geometry_entry(true,geometry_down,1,1,2,product::route_down(geometry_down,1,1,2)),
          "production geometry entry passes real ordinary and Ctrl single-left Move DOWN to Windows");
    for(int region=1;region<=8;++region){
        auto resize_down=geometry_down;resize_down.region=region;
        product::prepare_mappings(resize_down,v1::default_settings());
        check(product::native_geometry_entry(true,resize_down,1,1,0,product::route_down(resize_down,1,1,0))&&
              product::native_geometry_entry(true,resize_down,1,1,2,product::route_down(resize_down,1,1,2)),
              "each actual edge/corner supports ordinary and Ctrl native Resize entry");
        const bool left=region==1||region==5||region==7,right=region==2||region==6||region==8;
        const bool top=region==3||region==5||region==6,bottom=region==4||region==7||region==8;
        auto end=resize_down.source;
        end.visible={left?90:100,top?85:100,right?380:360,bottom?485:460};
        end.positioning={left?83:93,top?85:100,right?387:367,bottom?492:467};
        check(product::native_geometry_handoff_compatible(resize_down,end),
              "actual native Resize prefix accepts only its locked active edges while retaining A");
        // Mutate one inactive edge, preserving its frame offset so this
        // rejection really exercises the operation's edge contract.
        auto foreign=end;
        if(!left){foreign.visible={end.visible.left()+1,end.visible.top(),end.visible.right(),end.visible.bottom()};
            foreign.positioning={end.positioning.left()+1,end.positioning.top(),end.positioning.right(),end.positioning.bottom()};}
        else if(!right){foreign.visible={end.visible.left(),end.visible.top(),end.visible.right()+1,end.visible.bottom()};
            foreign.positioning={end.positioning.left(),end.positioning.top(),end.positioning.right()+1,end.positioning.bottom()};}
        else if(!top){foreign.visible={end.visible.left(),end.visible.top()+1,end.visible.right(),end.visible.bottom()};
            foreign.positioning={end.positioning.left(),end.positioning.top()+1,end.positioning.right(),end.positioning.bottom()};}
        check(!product::native_geometry_handoff_compatible(resize_down,foreign),
              "native Resize cannot rebaseline an inactive-edge movement");
        foreign=end;foreign.positioning={end.positioning.left(),end.positioning.top(),end.positioning.right()+1,end.positioning.bottom()};
        check(!product::native_geometry_handoff_compatible(resize_down,foreign),
              "native Resize rejects a changed right frame offset on every operation edge");
    }
    auto fixed=geometry_down;fixed.region=2;fixed.source.can_resize=false;
    check(!product::native_geometry_entry(true,fixed,1,1,0,plain)&&
          !product::native_geometry_handoff_compatible(fixed,fixed.source),"fixed-size source cannot enter native Resize handoff");
    for(const unsigned mods:{1u,3u,4u,6u,8u,16u})
        check(!product::native_geometry_entry(true,geometry_down,1,1,mods,plain),"Alt Shift Super and unknown modifier routes are not silently reclassified");
    check(!product::native_geometry_entry(false,geometry_down,1,1,0,plain)&&
          !product::native_geometry_entry(true,geometry_down,2,1,0,plain)&&
          !product::native_geometry_entry(true,geometry_down,1,2,0,plain)&&
          !product::native_geometry_entry(true,native_max,1,1,0,plain)&&
          !product::native_geometry_entry(true,geometry_down,1,1,0,{product::DownRoute::Mapped,v1::Action::Center})&&
          !product::native_geometry_entry(true,geometry_down,1,1,0,{}),
          "disabled, foreign button, double-click, maximized, mapped and passthrough retain their separate entry semantics");
    auto right_down=geometry_down;right_down.region=2;
    product::prepare_mappings(right_down,v1::default_settings());
    auto right_end=right_down.source;right_end.visible={100,100,380,460};right_end.positioning={93,100,387,467};
    auto invalid_end=right_end;invalid_end.dpi=144;
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"native Resize cannot accept changed DPI as an active-edge effect");
    invalid_end=right_end;invalid_end.monitor=reinterpret_cast<HMONITOR>(402);
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"native Resize cannot accept changed monitor context");
    invalid_end=right_end;invalid_end.work={0,0,1920,1040};
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"native Resize cannot accept changed work area");
    invalid_end=right_end;++invalid_end.born;
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"native Resize retains exact process birth identity");
    invalid_end=right_end;invalid_end.maximized=true;
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"native Resize rejects concurrent maximized state change");
    auto limited_down=right_down;limited_down.source.limits.max_width=270;
    check(!product::native_geometry_handoff_compatible(limited_down,right_end),"native Resize prefix obeys original maximum width");
    limited_down=right_down;limited_down.source.limits.min_width=290;
    check(!product::native_geometry_handoff_compatible(limited_down,right_end),"native Resize prefix obeys original minimum width");
    limited_down=right_down;limited_down.source.limits.grid_width=3;
    check(!product::native_geometry_handoff_compatible(limited_down,right_end),"native Resize prefix obeys an explicitly known original size grid");
    invalid_end=right_end;invalid_end.visible={100,100,100,460};
    check(!product::native_geometry_handoff_compatible(right_down,invalid_end),"invalid native Resize extent is not a new baseline");
    auto move_end=geometry_down.source;move_end.visible={120,125,380,485};move_end.positioning={113,125,387,492};
    check(product::native_geometry_handoff_compatible(geometry_down,move_end),
          "unified native geometry helper preserves Move translation-only handoff");
    move_end.visible={120,125,381,485};move_end.positioning={113,125,388,492};
    check(!product::native_geometry_handoff_compatible(geometry_down,move_end),
          "unified Move branch cannot accept Resize merely because active Resize branches exist");
    // Production's native-arranged branch must pass the same real native
    // START/cancel/END geometry gate before it can arm a restore request. The
    // API bit is explicit: neither half-screen dimensions nor a missing export
    // can manufacture it, and a rejected handoff must not fall into the large
    // ordinary writer branch.
    using RestoreKind=product::NativeRestoreKind;
    using RestoreProgress=product::NativeRestoreProgress;
    auto arranged_down=geometry_down;arranged_down.source.native_arranged=true;
    arranged_down.source.visible={0,0,960,1040};arranged_down.source.positioning={-7,0,967,1047};
    auto arranged_end=arranged_down.source;
    check(product::native_geometry_entry(true,arranged_down,1,1,0,product::route_down(arranged_down,1,1,0))&&
          product::native_geometry_handoff_compatible(arranged_down,arranged_end)&&
          product::native_arranged_handoff_compatible(arranged_down,arranged_end,1,1,0)&&
          product::native_restore_entry_kind(arranged_down,arranged_end,true,1,1,0)==RestoreKind::Arranged,
          "actual plain arranged DOWN and fresh END retain native-first handoff then arm native restore instead of large Move");
    auto unknown_arranged=arranged_end;unknown_arranged.native_arranged.reset();
    auto normal_arranged=arranged_end;normal_arranged.native_arranged=false;
    check(product::Catalog::same(arranged_end,unknown_arranged)&&product::Catalog::same(arranged_end,normal_arranged),
          "unknown or changed arranged state is not a new window lifetime identity");
    check(!product::native_arranged_handoff_compatible(arranged_down,unknown_arranged,1,1,0)&&
          !product::native_arranged_handoff_compatible(arranged_down,normal_arranged,1,1,0)&&
          product::native_restore_entry_kind(arranged_down,unknown_arranged,true,1,1,0)==RestoreKind::None,
          "original arranged true plus unknown or false END is rejected before production can select ordinary writing");
    auto unknown_down=arranged_down;unknown_down.source.native_arranged.reset();
    auto normal_down=arranged_down;normal_down.source.native_arranged=false;
    check(!product::native_arranged_handoff_compatible(unknown_down,arranged_end,1,1,0)&&
          !product::native_arranged_handoff_compatible(normal_down,arranged_end,1,1,0)&&
          !product::native_arranged_handoff_compatible(unknown_down,unknown_arranged,1,1,0)&&
          !product::native_arranged_handoff_compatible(normal_down,unknown_arranged,1,1,0),
          "unknown DOWN true END disagreement and unavailable export fail closed for plain native-first Move rather than relabel normal");
    unsigned arranged_restore_branches{},ordinary_large_writer_branches{},arranged_handoff_rejections{};
    for(const auto& actual_end:std::array{arranged_end,unknown_arranged,normal_arranged}){
        // This is the same reject-before-begin then locked restore dispatch
        // used by Application, not an adapter which returns permission true.
        if(!product::native_geometry_handoff_compatible(arranged_down,actual_end)||
           !product::native_arranged_handoff_compatible(arranged_down,actual_end,1,1,0)){
            ++arranged_handoff_rejections;continue;
        }
        const auto kind=product::native_restore_entry_kind(arranged_down,actual_end,true,1,1,0);
        if(kind!=RestoreKind::None)++arranged_restore_branches;else ++ordinary_large_writer_branches;
    }
    check(arranged_restore_branches==1&&arranged_handoff_rejections==2&&ordinary_large_writer_branches==0,
          "production combined handoff and restore dispatch cannot use rejected arranged snapshots as ordinary placement authority");
    check(product::native_arranged_handoff_compatible(normal_down,normal_arranged,1,1,0)&&
          product::native_restore_entry_kind(normal_down,normal_arranged,true,1,1,0)==RestoreKind::None&&
          product::native_restore_entry_kind(arranged_down,arranged_end,false,1,1,0)==RestoreKind::None,
          "known ordinary remains ordinary and arranged state cannot borrow consumed-DOWN capture without real native handoff");
    auto contradictory=arranged_end;contradictory.maximized=true;
    check(!product::native_arranged_handoff_compatible(arranged_down,contradictory,1,1,0)&&
          product::native_restore_entry_kind(arranged_down,contradictory,true,1,1,0)==RestoreKind::None,
          "contradictory arranged and maximized facts cannot authorize native-arranged restore");
    auto arranged_resize=arranged_down;arranged_resize.region=2;
    check(product::native_arranged_handoff_compatible(arranged_down,unknown_arranged,1,1,2)&&
          product::native_arranged_handoff_compatible(arranged_resize,unknown_arranged,1,1,0)&&
          product::native_restore_entry_kind(arranged_down,arranged_end,true,1,1,2)==RestoreKind::None&&
          product::native_restore_entry_kind(arranged_resize,arranged_end,true,1,1,0)==RestoreKind::None&&
          product::native_restore_entry_kind(arranged_down,arranged_end,true,1,2,0)==RestoreKind::None&&
          product::native_restore_entry_kind(arranged_down,arranged_end,true,2,1,0)==RestoreKind::None,
          "Ctrl Resize double-click and other buttons retain old routes and never gain arranged restore");
    auto mapped_arranged=arranged_down;mapped_arranged.mappings[0]=v1::Action::Center;
    check(product::native_restore_entry_kind(mapped_arranged,arranged_end,true,1,1,0)==RestoreKind::None&&
          product::native_restore_entry_kind(*max_prepared,max_prepared->source,false,1,1,0)==RestoreKind::Maximized&&
          product::native_restore_entry_kind(*max_prepared,max_prepared->source,true,1,1,0)==RestoreKind::None,
          "mapped action cannot borrow restoration and existing real maximized consumed-DOWN route remains distinct");
    // The accepted native prefix can translate A to B. Restore admission
    // checks actual B while original A/cursor still defines the eventual intent.
    auto arranged_baseline=arranged_end;
    arranged_baseline.visible={5,0,965,1040};arranged_baseline.positioning={-2,0,972,1047};
    check(product::native_restore_request_ready(RestoreKind::Arranged,arranged_down.source,arranged_baseline,
              arranged_baseline,SW_SHOWNORMAL,true),
          "arranged restore preflight uses confirmed post-END B without changing original DOWN A");
    check(!product::native_restore_request_ready(RestoreKind::Arranged,arranged_down.source,arranged_baseline,
              arranged_baseline,SW_SHOWNORMAL,false)&&
          !product::native_restore_request_ready(RestoreKind::Arranged,arranged_down.source,arranged_baseline,
              arranged_end,SW_SHOWNORMAL,true)&&
          !product::native_restore_request_ready(RestoreKind::Arranged,arranged_down.source,arranged_baseline,
              unknown_arranged,SW_SHOWNORMAL,true)&&
          !product::native_restore_request_ready(RestoreKind::Arranged,arranged_down.source,arranged_baseline,
              arranged_baseline,SW_SHOWMAXIMIZED,true),
          "UP stop external geometry unknown arranged or contradictory placement state cannot issue a restore request");
    check(product::native_restore_request_ready(RestoreKind::Maximized,max_prepared->source,max_prepared->source,
              max_prepared->source,SW_SHOWMAXIMIZED,true)&&
          !product::native_restore_request_ready(RestoreKind::Maximized,max_prepared->source,max_prepared->source,
              max_prepared->source,SW_SHOWNORMAL,true),
          "maximized restore request still requires its real maximum show state");
    auto actual_normal=arranged_down.source;actual_normal.native_arranged=false;
    actual_normal.visible={55,45,455,345};actual_normal.positioning={48,45,462,352};
    check(product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,arranged_end,414,307,true)==RestoreProgress::Pending&&
          product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,unknown_arranged,414,307,true)==RestoreProgress::Pending&&
          product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,normal_arranged,414,307,true)==RestoreProgress::Pending&&
          product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,actual_normal,414,307,true)==RestoreProgress::Ready,
          "actual arranged false and actual normal native size are both required before common writer resumes; unknown stays bounded pending");
    auto wrong_restore_context=actual_normal;wrong_restore_context.dpi=144;
    auto wrong_restore_identity=actual_normal;++wrong_restore_identity.key;
    check(product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,actual_normal,414,307,false)==RestoreProgress::Rejected&&
          product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,wrong_restore_context,414,307,true)==RestoreProgress::Rejected&&
          product::native_restore_progress(RestoreKind::Arranged,arranged_down.source,wrong_restore_identity,414,307,true)==RestoreProgress::Rejected,
          "pending restore never reinstates UP or stop authority and rejects real DPI or lifetime changes");
    auto restored_max=max_prepared->source;restored_max.maximized=false;
    restored_max.visible={200,160,600,460};restored_max.positioning={193,160,607,467};
    check(product::native_restore_progress(RestoreKind::Maximized,max_prepared->source,max_prepared->source,414,307,true)==RestoreProgress::Pending&&
          product::native_restore_progress(RestoreKind::Maximized,max_prepared->source,restored_max,414,307,true)==RestoreProgress::Ready&&
          product::native_restore_progress(RestoreKind::Maximized,max_prepared->source,restored_max,414,307,false)==RestoreProgress::Rejected,
          "maximum restoration retains actual normal observation without borrowing arranged facts or post-UP permission");
    const auto snap_anchor=v1::restored_move_anchor(arranged_down.source.visible,actual_normal.visible,{10,9});
    const auto snap_intent=snap_anchor?std::optional{v1::intent(*snap_anchor,{10,9},{50,54},{})}:std::nullopt;
    check(snap_anchor&&*snap_anchor==v1::Rect{6,0,406,300}&&snap_intent&&*snap_intent==v1::Rect{46,45,446,345},
          "fresh native normal size computes original DOWN proportional anchor, never END or rcNormalPosition screen coordinates");
    product::NativeMoveReceipt real_start{70,1,EVENT_SYSTEM_MOVESIZESTART,31,110,native_down.source.hwnd,OBJID_WINDOW,CHILDID_SELF};
    check(product::native_move_receipt_matches(real_start,native_down,70,100,EVENT_SYSTEM_MOVESIZESTART),
          "actual native START receipt binds source thread, generation and native DOWN time");
    auto wrong_native=real_start;wrong_native.thread_id=32;
    auto stale_native=real_start;stale_native.stamp=99;
    check(!product::native_move_receipt_matches(wrong_native,native_down,70,100,EVENT_SYSTEM_MOVESIZESTART)&&
          !product::native_move_receipt_matches(stale_native,native_down,70,100,EVENT_SYSTEM_MOVESIZESTART)&&
          !product::native_move_receipt_matches(real_start,native_down,71,100,EVENT_SYSTEM_MOVESIZESTART)&&
          !product::native_move_receipt_matches(real_start,native_down,70,100,EVENT_SYSTEM_MOVESIZEEND),
          "foreign thread, stale native time, wrong generation and START-as-END reject");
    auto ordered_end=real_start;ordered_end.event=EVENT_SYSTEM_MOVESIZEEND;ordered_end.sequence=2;ordered_end.stamp=120;
    auto late_old_end=ordered_end;late_old_end.stamp=105;
    check(product::native_move_end_follows(real_start,ordered_end)&&!product::native_move_end_follows(real_start,late_old_end),
          "late callback arrival cannot turn an END stamped before the actual START into this handoff END");
    auto actual_end=native_down.source;
    actual_end.visible={120,125,380,485};actual_end.positioning={113,125,387,492};
    check(product::native_move_handoff_compatible(native_down.source,actual_end),
          "real native translation can provide B continuity while retaining original A intent");
    auto resized_end=actual_end;resized_end.visible={120,125,381,485};
    auto changed_context=actual_end;changed_context.dpi=144;
    auto changed_identity=actual_end;changed_identity.key=42;
    check(!product::native_move_handoff_compatible(native_down.source,resized_end)&&
          !product::native_move_handoff_compatible(native_down.source,changed_context)&&
          !product::native_move_handoff_compatible(native_down.source,changed_identity),
          "handoff rejects unobserved resize, DPI change and Catalog identity generation replacement");
    const v1::Point full_down_delta{50,40};
    const std::array<v1::Item,1> down_members{{{native_down.source.key,native_down.source.visible,native_down.source.limits}}};
    const std::array<v1::Item,1> end_members{{{actual_end.key,actual_end.visible,actual_end.limits}}};
    const auto anchored_plan=v1::move_group(down_members,
        native_down.source.key,full_down_delta,0);
    const auto wrongly_reanchored=v1::move_group(end_members,
        actual_end.key,full_down_delta,0);
    check(anchored_plan.accepted&&anchored_plan.placements.size()==1&&
          anchored_plan.placements.front().visible==v1::Rect{150,140,410,500}&&
          wrongly_reanchored.placements.front().visible!=anchored_plan.placements.front().visible,
          "actual production move_group uses A plus full DOWN delta, not B plus the same delta twice");
    panebind::core::behavior::MoveHandoffGate native_gate;
    check(native_gate.begin(70,true,true,true)&&!native_gate.may_write(70,true,true,true)&&
          native_gate.isolation_ready(70,true)&&native_gate.cancel_issued(70)&&
          !native_gate.may_write(70,true,true,true)&&native_gate.native_end_observed(70,true)&&
          native_gate.may_write(70,true,true,true)&&native_gate.raw_up_observed(70,true)&&
          !native_gate.may_write(70,true,true,true),
          "same GUI production gate requires real isolation/cancel/END and never undoes actual UP revocation");
    std::atomic<std::uint64_t> captured_authority{40|product::input_pending_capture_mask};
    check(!product::operation_permission(40,captured_authority,0,true,false),
          "consumed DOWN pending shield readiness never grants a continuous placement");
    check(product::arm_exact_capture_generation(captured_authority,40)&&captured_authority==40&&
          product::operation_permission(40,captured_authority,0,true,false),
          "actual CaptureReady may promote only its exact pending generation");
    captured_authority=0;
    check(!product::arm_exact_capture_generation(captured_authority,40)&&captured_authority==0,
          "an early actual UP prevents late CaptureReady from restoring issue authority");
    for(int region:{0,1,4,8}){
        auto early=geometry_down;early.region=region;
        check(product::native_geometry_entry(true,early,1,1,2,product::route_down(early,1,1,2))&&
              !product::arm_exact_capture_generation(captured_authority,40)&&
              !product::operation_permission(40,captured_authority,40,true,false),
              "Ctrl Move/Resize route classification cannot restore new continuous issue permission after actual early UP");
    }
    captured_authority=41|product::input_pending_capture_mask;
    check(!product::arm_exact_capture_generation(captured_authority,40)&&captured_authority==(41|product::input_pending_capture_mask),
          "a stale resource receipt cannot authorize a later DOWN generation");
    check(!product::arm_exact_capture_generation(captured_authority,0)&&
          !product::arm_exact_capture_generation(captured_authority,product::input_pending_capture_mask),
          "zero and reserved pending-bit values are not real gesture identities");
    // Reproduce the former producer ordering: READY armed g while its GUI
    // DOWN still lived in the resource queue, then a real LL MOVE arrived.
    // The old direct allowed(g) test would enqueue MOVE ahead of DOWN; the
    // production slot keeps exactly one actual sample until DOWN is queued.
    product::CapturedMoveSlot consumed_slot;
    consumed_slot.begin(40,true,true);
    const product::CapturedMoveSlot::Sample first_move{{130,145},1100,501,150};
    const product::CapturedMoveSlot::Sample latest_move{{170,185},1110,502,151};
    std::atomic<std::uint64_t> ready_authority{40|product::input_pending_capture_mask};
    check(consumed_slot.observe(40,first_move,false)&&!consumed_slot.direct_move_allowed(40)&&
          product::arm_exact_capture_generation(ready_authority,40)&&
          product::operation_permission(40,ready_authority,0,true,false)&&
          consumed_slot.observe(40,latest_move,true)&&!consumed_slot.direct_move_allowed(40),
          "actual pending and READY-window LL MOVEs stay behind the not-yet-enqueued DOWN despite active authority");
    check(consumed_slot.mark_down_enqueued(40)&&consumed_slot.direct_move_allowed(40),
          "only a successfully queued same-generation DOWN opens direct MOVE publication");
    const auto flushed=consumed_slot.take_after_down(40,ready_authority==40);
    check(flushed&&flushed->point.x==170&&flushed->point.y==185&&flushed->tick==1110&&
          flushed->hook_qpc==502&&flushed->native_message_time==151&&
          !consumed_slot.take_after_down(40,true),
          "DOWN then one latest real LL MOVE retains original event time and cannot replay");
    product::InputEvent continued_move;continued_move.kind=product::InputKind::Move;
    continued_move.point={190,205};continued_move.generation=40;
    check(consumed_slot.direct_move_allowed(40)&&
          product::dispatch_motion_permission(continued_move,false,40,0,true,false),
          "continued held motion after queued DOWN follows unchanged active MOVE authority");
    consumed_slot.begin(41,true,true);consumed_slot.observe(41,latest_move,false);
    check(consumed_slot.mark_down_enqueued(41)&&!consumed_slot.take_after_down(41,false)&&
          !consumed_slot.take_after_down(41,true),
          "UP or abort before owner flush discards the pending sample without terminal regrant");
    consumed_slot.begin(42,true,true);
    check(!consumed_slot.observe(41,first_move,true)&&!consumed_slot.mark_down_enqueued(41)&&
          !consumed_slot.take_after_down(41,true)&&!consumed_slot.direct_move_allowed(41),
          "old-generation LL MOVE and READY cannot cross a new DOWN generation");
    product::CapturedMoveSlot native_slot;native_slot.begin(43,true,false);
    check(!native_slot.observe(43,first_move,false)&&!native_slot.direct_move_allowed(43),
          "native-first pre-END and still-pending capture MOVE is never retained as post-READY motion");
    check(native_slot.observe(43,latest_move,true)&&!native_slot.direct_move_allowed(43)&&
          native_slot.may_queue_down(43)&&native_slot.mark_down_enqueued(43),
          "native-first real post-READY MOVE waits for its actual GUI DOWN publication");
    const auto native_flushed=native_slot.take_after_down(43,true);
    check(native_flushed&&native_flushed->point.x==170&&native_flushed->hook_qpc==502&&
          native_slot.direct_move_allowed(43)&&!native_slot.may_queue_down(43)&&
          !native_slot.mark_down_enqueued(43)&&!native_slot.take_after_down(43,true),
          "one native post-READY sample follows one DOWN and duplicate READY cannot enqueue another DOWN or replay MOVE");
    consumed_slot.begin(44,true,true);consumed_slot.observe(44,first_move,false);consumed_slot.retire();
    check(consumed_slot.may_queue_down(44)&&consumed_slot.mark_down_enqueued(44)&&
          !consumed_slot.direct_move_allowed(44)&&!consumed_slot.take_after_down(44,true),
          "UP after READY but before GUI drain can preserve one historical DOWN, never its pending continuous motion");
    consumed_slot.begin(45,false,false);
    check(!consumed_slot.observe(45,first_move,true)&&!consumed_slot.may_queue_down(45),
          "noncapture mapped or passthrough route cannot borrow the capture MOVE slot");
    check(product::captured_release_can_complete(true,true,false,true,true),
          "actual captured legacy UP with confirmed own cleanup permits only release-dock completion");
    check(!product::captured_release_can_complete(true,false,false,true,true)&&
          !product::captured_release_can_complete(false,true,false,true,true)&&
          !product::captured_release_can_complete(true,true,true,true,true)&&
          !product::captured_release_can_complete(true,true,false,false,true)&&
          !product::captured_release_can_complete(true,true,false,true,false),
          "Raw-only UP, unknown cleanup, abort, later DOWN and stop cannot mint a normal release token");
    namespace shield=panebind::platform::windows::operations;
    check(product::native_move_resources_can_retire(shield::GestureShieldEventKind::IsolationGone,true)&&
          !product::native_move_resources_can_retire(shield::GestureShieldEventKind::IsolationGone,false)&&
          !product::native_move_resources_can_retire(shield::GestureShieldEventKind::CaptureReleased,true)&&
          !product::native_move_resources_can_retire(shield::GestureShieldEventKind::CaptureReady,true),
          "next native generation preparation waits for actual Gone and confirmed cleanup, never a remove request or partial release");
    product::GestureCaptureContext cleanup_context;
    cleanup_context.generation=40;cleanup_context.overlay=reinterpret_cast<HWND>(400);
    cleanup_context.owner_tid=60;cleanup_context.owner_pid=70;
    cleanup_context.source.hwnd=reinterpret_cast<HWND>(100);cleanup_context.source.pid=20;cleanup_context.source.tid=30;
    shield::GestureShieldEvent absent_capture;
    absent_capture.kind=shield::GestureShieldEventKind::CaptureReleased;
    absent_capture.entry_mode=shield::GestureShieldEntryMode::ConsumedDown;
    absent_capture.generation=40;absent_capture.overlay=cleanup_context.overlay;
    absent_capture.consumed_down.gesture={40,cleanup_context.source.hwnd,20,30,30000};
    absent_capture.capture.owner_thread_id=60;absent_capture.capture.attempted=true;
    auto aborted_gone=absent_capture;
    aborted_gone.kind=shield::GestureShieldEventKind::IsolationGone;
    aborted_gone.removal_reason=shield::GestureShieldRemovalReason::ContextLost;
    aborted_gone.overlay_destroyed=true;aborted_gone.hotkey_unregistered=true;
    auto& detached=aborted_gone.capture.association;
    detached.generation=40;detached.owner_thread_id=60;detached.source_thread_id=30;
    detached.source_process_id=20;detached.source=cleanup_context.source.hwnd;
    detached.attach_attempted=true;detached.attach_completed=true;detached.attach_succeeded=true;
    detached.detach_attempted=true;detached.detach_completed=true;detached.detach_succeeded=true;
    const auto abort_clean=[&](const shield::GestureShieldEvent* receipt,const shield::GestureShieldEvent& gone,bool aborted=true){
        return product::captured_abort_cleanup_confirmed(&cleanup_context,receipt,gone,aborted);
    };
    check(abort_clean(&absent_capture,aborted_gone),
          "actual resource-owner absent capture receipt plus real detach and destruction permits abort cleanup");
    auto foreign_observation=absent_capture,foreign_gone=aborted_gone;
    foreign_observation.capture.release_before=reinterpret_cast<HWND>(900);
    foreign_observation.capture.release_after=foreign_observation.capture.release_before;
    foreign_gone.capture.release_before=foreign_observation.capture.release_before;
    foreign_gone.capture.release_after=foreign_observation.capture.release_after;
    check(abort_clean(&foreign_observation,foreign_gone)&&!foreign_observation.capture.release_attempted,
          "an observed non-own capture is left untouched while this exact overlay and association are retired");
    check(!abort_clean(nullptr,aborted_gone)&&!abort_clean(&absent_capture,aborted_gone,false),
          "default NULL fields or a non-aborted generation never substitute for a real cleanup observation");
    auto changed=absent_capture;changed.generation=41;
    check(!abort_clean(&changed,aborted_gone),"stale capture receipt cannot clean another generation");
    changed=absent_capture;changed.capture.owner_thread_id=61;
    check(!abort_clean(&changed,aborted_gone),"a different thread's NULL capture is not the resource-owner observation");
    changed=absent_capture;changed.capture.owner_thread_id=0;
    check(!abort_clean(&changed,aborted_gone),"unknown observer identity cannot turn default NULL fields into proof");
    changed=absent_capture;changed.overlay=reinterpret_cast<HWND>(401);
    check(!abort_clean(&changed,aborted_gone),"another overlay's cleanup receipt is rejected");
    changed=absent_capture;changed.consumed_down.gesture.thread_id=31;
    check(!abort_clean(&changed,aborted_gone),"cleanup observation retains the actual source thread identity");
    changed=absent_capture;changed.capture.release_before=cleanup_context.overlay;changed.capture.release_after=cleanup_context.overlay;
    check(!abort_clean(&changed,aborted_gone),"still-own capture is not an absent-capture abort cleanup");
    changed=absent_capture;changed.capture.release_attempted=true;changed.capture.release_completed=true;
    check(!abort_clean(&changed,aborted_gone),"failed native ReleaseCapture cannot be relabeled as the no-call observation");
    auto incomplete=aborted_gone;incomplete.capture.association.detach_succeeded=false;
    check(!abort_clean(&absent_capture,incomplete),"failed detach still leaves input unhealthy");
    incomplete=aborted_gone;incomplete.overlay_destroyed=false;
    check(!abort_clean(&absent_capture,incomplete),"an undestroyed overlay is not clean");
    incomplete=aborted_gone;incomplete.hotkey_unregistered=false;
    check(!abort_clean(&absent_capture,incomplete),"unconfirmed hotkey cleanup remains a failure");
    incomplete=aborted_gone;incomplete.removal_reason=shield::GestureShieldRemovalReason::NormalUp;
    check(!abort_clean(&absent_capture,incomplete)&&
          !product::captured_release_can_complete(true,false,true,true,true),
          "abort cleanup cannot bypass strict own-release NormalUp or mint its completion capability");
    product::MouseUpCallbackObservation up_observation;
    const auto initial_up=up_observation.snapshot();
    check(initial_up.known&&initial_up.callback_count==0,"initial UP diagnostic snapshot reports no observed callback");
    check(!product::MouseUpCallbackObservation::coherent(1,1)&&!product::MouseUpCallbackObservation::coherent(2,4)&&
          product::MouseUpCallbackObservation::coherent(4,4),"actual bounded snapshot predicate rejects in-progress and changed publication");
    up_observation.publish({true,1,123,16,1,true,true,true,true,true});const auto matched_up=up_observation.snapshot();
    check(matched_up.known&&matched_up.callback_count==1&&matched_up.hook_observed_qpc==123&&matched_up.generation_at_entry==16&&
          matched_up.button==1&&matched_up.consumed_before&&matched_up.aborted_before&&matched_up.matching_branch_taken&&
          matched_up.push_attempted&&matched_up.push_succeeded,"actual atomic publication retains one matched aborted UP and queue result");
    up_observation.publish({true,2,456,16,2,false,true,false,false,false});const auto unpaired_up=up_observation.snapshot();
    check(unpaired_up.known&&unpaired_up.callback_count==2&&unpaired_up.hook_observed_qpc==456&&unpaired_up.button==2&&
          !unpaired_up.consumed_before&&unpaired_up.aborted_before&&!unpaired_up.matching_branch_taken&&
          !unpaired_up.push_attempted&&!unpaired_up.push_succeeded,"unpaired actual UP callback is not fabricated as normalized UP delivery");
    product::Prepared prepared;
    auto settings = v1::default_settings();
    prepared.region = 0;
    product::prepare_mappings(prepared, settings);
    check(product::route_down(prepared,1,1,0).route == product::DownRoute::Gesture,
          "plain title left DOWN selects gesture");
    check(product::route_down(prepared,1,1,2).route == product::DownRoute::Gesture,
          "Ctrl title left DOWN retains group route at dispatcher");
    check(product::route_down(prepared,1,1,4).route == product::DownRoute::Pass &&
          product::route_down(prepared,1,1,6).route == product::DownRoute::Pass,
          "Shift and Ctrl Shift ordinary move bypass");
    product::ClickCandidate click;
    click.begin({498,296},1,true);
    click.motion({578,296},4,4);
    check(!click.eligible&&click.pressing,"production press tracking retires the 80 pixel drag before its UP");
    click.motion({498,296},4,4);click.release(1);
    check(!click.eligible&&!click.pressing,"return to DOWN point and matching UP cannot revive dragged click candidate");
    click.begin({-100,-200},1,true);click.motion({-104,-196},4,4);
    check(click.eligible&&click.pressing,"system drag distances are inclusive on both signed axes");
    click.release(2);
    check(click.pressing,"unrelated button UP cannot stop click candidate tracking");
    click.release(1);click.motion({10000,20000},4,4);
    check(click.eligible&&!click.pressing,"matching UP preserves undragged first click and later hover cannot retire it");
    click.begin({0,0},1,true);click.motion({0,-5},4,4);
    check(!click.eligible,"vertical movement beyond drag distance retires first click");
    click.begin({LONG_MIN,LONG_MAX},1,true);click.motion({LONG_MAX,LONG_MIN},4,4);
    check(!click.eligible,"full signed physical coordinate difference cannot overflow click motion check");
    click.begin({50,50},1,true);click.motion({54,54},4,4);click.release(1);
    const auto unchanged_double=product::route_down(prepared,1,click.eligible?2:1,0);
    check(unchanged_double.route==product::DownRoute::Mapped&&unchanged_double.action==v1::Action::Maximize,
          "true undragged title double click retains existing mapped action");
    check(product::route_down(prepared,1,1,4).route==product::DownRoute::Pass,
          "passthrough press fixture uses unchanged native bypass route");
    click.begin({10,10},1,true);click.motion({15,10},4,4);click.release(1);
    check(!click.eligible&&!click.pressing,"same production tracker retires a passthrough drag independently of consumption");
    click.begin({10,10},1,false);click.motion({10,10},4,4);click.release(1);
    check(!click.eligible,"second click does not seed a spurious triple click candidate");
    const auto title_double = product::route_down(prepared,1,2,0);
    check(title_double.route == product::DownRoute::Mapped && title_double.action == v1::Action::Maximize,
          "default title double click is explicit command not native evidence");
    for (int button=2; button<=5; ++button)
        check(product::route_down(prepared,button,1,0).route == product::DownRoute::Pass &&
              product::route_down(prepared,button,2,0).route == product::DownRoute::Pass,
              "unmapped extra button singles and doubles pass through");
    prepared.region = 1;
    product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,2,0).action == v1::Action::StretchLeft,
          "left border default single-axis stretch");
    check(product::route_down(prepared,1,2,4).action == v1::Action::StretchHorizontal,
          "Shift border double click maps before drag bypass");
    prepared.region = 3;
    product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,2,4).action == v1::Action::StretchVertical,
          "top border Shift double maps vertical not horizontal");
    for (int region=5;region<=8;++region) {
        prepared.region=region;product::prepare_mappings(prepared,settings);
        const auto corner=product::route_down(prepared,1,2,0);
        check(corner.route==product::DownRoute::Gesture && corner.action==v1::Action::None,
              "corner double click is contextual two-edge route not StretchAll");
    }
    settings.mouse_bindings.clear();
    // Full five-button/one-two click/modifier index coverage on one exact
    // region; changing region must remove every previous mapping.
    for(int button=1;button<=5;++button)
        for(int clicks=1;clicks<=2;++clicks)
            for(unsigned mods=0;mods<16;++mods)
                settings.mouse_bindings.push_back({2,button,clicks,mods,
                    clicks==1?v1::Action::Center:v1::Action::Restore});
    check(v1::validate(settings).empty(),"complete single-region mapping fixture valid");
    prepared.region=2;product::prepare_mappings(prepared,settings);
    for(int button=1;button<=5;++button)
        for(int clicks=1;clicks<=2;++clicks)
            for(unsigned mods=0;mods<16;++mods){
                const auto decision=product::route_down(prepared,button,clicks,mods);
                check(decision.route==product::DownRoute::Mapped &&
                    decision.action==(clicks==1?v1::Action::Center:v1::Action::Restore),
                    "actual builder and callback index agree for five keys two clicks sixteen modifier masks");
            }
    prepared.region=0;product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,5,2,15).route==product::DownRoute::Pass,
          "region change clears stale mapping table");
    prepared.region=-1;product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,1,0).route==product::DownRoute::Pass,
          "system buttons and unsupported client region cannot acquire gesture");
    prepared.region=9;
    check(product::route_down(prepared,1,1,0).route==product::DownRoute::Pass,
          "unknown tenth region rejected");
    prepared.region=0;
    check(product::route_down(prepared,0,1,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,6,1,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,1,3,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,1,1,16).route==product::DownRoute::Pass,
          "out-of-range route inputs do not index table");
    check(!product::native_event_is_current(99,100)&&product::native_event_is_current(100,100)&&
          product::native_event_is_current(101,100),"previous gesture native event cannot invalidate new DOWN");
    check(product::native_event_is_current(2,0xfffffff0u)&&!product::native_event_is_current(0xfffffff0u,2),
          "native 32 bit timestamp fence handles system tick rollover");
    bool pair_consumed=true;std::uint64_t pair_authority=0;
    const bool resync=product::retire_revoked_pair_for_down(pair_consumed,pair_authority,1,1,100,200);
    check(resync,"actual aborted revoked pair retires on a current same button DOWN rather than stealing its UP");
    if(resync)pair_consumed=false;
    product::Prepared unprepared;unprepared.region=-1;
    check(product::route_down(unprepared,1,1,0).route==product::DownRoute::Pass&&
          !product::button_up_matches_current_pair(pair_consumed,1,1,100,250)&&pair_authority==0,
          "retirement precedes preparation so an unprepared new DOWN and UP both pass without reviving old writes");
    prepared.region=0;product::prepare_mappings(prepared,v1::default_settings());
    check(product::route_down(prepared,1,1,0).route==product::DownRoute::Gesture,
          "resynchronized current DOWN still uses the unchanged production routing table");
    pair_consumed=true;pair_authority=17;
    check(!product::button_up_matches_current_pair(pair_consumed,1,1,200,150)&&pair_authority==17,
          "late old timestamp UP cannot match or revoke the newer pair");
    check(product::button_up_matches_current_pair(pair_consumed,1,1,200,250)&&
          !product::button_up_matches_current_pair(pair_consumed,1,2,200,250),
          "current matching UP retains its route while an unrelated button does not match");
    check(!product::retire_revoked_pair_for_down(true,17,1,1,100,200)&&
          !product::retire_revoked_pair_for_down(true,0,1,2,100,200),
          "live authority and other button cannot resynchronize");
    check(!product::retire_revoked_pair_for_down(true,0,1,1,200,100)&&
          product::retire_revoked_pair_for_down(true,0,1,1,0xfffffff0u,2)&&
          product::button_up_matches_current_pair(true,1,1,0xfffffff0u,2)&&
          !product::button_up_matches_current_pair(true,1,1,2,0xfffffff0u),
          "actual pair predicates preserve the native timestamp wrap fence and reject stale DOWN or UP");
    product::ClickCandidate stale_release;stale_release.begin({10,10},1,true);
    if(product::native_event_is_current(150,200))stale_release.release(1);
    stale_release.motion({30,10},4,4);
    check(!stale_release.eligible&&stale_release.pressing,
          "stale UP cannot stop the new click press tracker before its actual drag retires the candidate");
    check(product::retire_revoked_pair_for_down(true,0,1,1,100,200),
          "actual hard revocation permits new DOWN resync before queued abort updates the owner flag");
    check(product::motion_requests_preparation(true,0)&&product::motion_requests_preparation(false,0)&&
          !product::motion_requests_preparation(true,17),
          "actual revoked waiting-pair MOVE refreshes preparation while an authorized gesture does not");
    check(!product::idle_catalog_observation_allowed(false,true),
          "a retired GUI gesture cannot probe its source while the issued native call is still pending");
    check(product::idle_catalog_observation_allowed(false,false),
          "actual completion permits the same idle catalog refresh skipped during the issued call");
    check(!product::idle_catalog_observation_allowed(true,false),
          "an unrelated completion does not replace the active gesture's prepared source");
    check(!product::idle_catalog_observation_allowed(true,true),
          "an active gesture with pending native work remains outside idle observation");
    // This is the production guard predicate, including the formerly unsafe
    // normal-UP completion route. Follow the actual receive/dispatch order.
    std::uint64_t active=1,completion=0;
    check(product::operation_permission(1,active,completion,true,false),"g1 DOWN owns active writes");
    active=0;completion=1;
    check(!product::operation_permission(1,active,completion,true,false)&&
          product::operation_permission(1,active,completion,true,true),"actual normal UP permits only its distinct completion route");
    completion=0;active=2;
    check(!product::operation_permission(1,active,completion,true,true),"new DOWN retires g1 completion even when its captured allow_completion boolean was true");
    active=0;completion=2;
    check(!product::operation_permission(1,active,completion,true,true)&&product::operation_permission(2,active,completion,true,true),
          "g2 normal UP never revives g1");
    completion=0;
    check(!product::operation_permission(2,active,completion,true,true),"abort retires completion without pretending the native call was cancelled");
    check(!product::operation_permission(2,0,2,false,true),"pause or stop independently rejects same-generation completion");
    check(product::matching_up_end(false,v1::End::NormalUp,true,true)==v1::End::NormalUp,
          "actual foreground and current source generation permit matching normal UP");
    check(product::matching_up_end(false,v1::End::NormalUp,true,false)==v1::End::FocusLost,
          "UP arriving before queued foreground WinEvent still records actual focus loss");
    check(product::matching_up_end(false,v1::End::NormalUp,false,true)==v1::End::SourceLost&&
          product::matching_up_end(false,v1::End::NormalUp,false,false)==v1::End::SourceLost,
          "destroyed replaced or explicitly retired source generation cannot receive normal completion");
    check(product::matching_up_end(true,v1::End::FocusLost,true,true)==v1::End::FocusLost&&
          product::matching_up_end(true,v1::End::Deadline,true,true)==v1::End::Deadline&&
          product::matching_up_end(true,v1::End::Paused,true,true)==v1::End::Paused,
          "foreground return after an observed abort cannot rewrite failure as NormalUp");
    check(product::matching_up_end(true,v1::End::NormalUp,true,true)!=v1::End::NormalUp,
          "already aborted admission cannot mint a completion capability even if its old end was normal");
    active=5;completion=0;
    const auto up_end=product::matching_up_end(false,v1::End::NormalUp,true,false);
    completion=up_end==v1::End::NormalUp?active:0;active=0;
    check(!product::operation_permission(5,active,completion,true,true),
          "production matching-UP failure leaves both active writes and discrete completion revoked");
    std::atomic<std::uint64_t> exact_active{20},exact_completion{0};
    check(product::revoke_exact_generation(exact_active,exact_completion,20)&&exact_active==0&&exact_completion==0,
          "actual mapped completion CAS retires its remaining active generation");
    exact_active=21;exact_completion=21;
    check(!product::revoke_exact_generation(exact_active,exact_completion,20)&&exact_active==21&&exact_completion==21,
          "old mapped completion cannot revoke a later DOWN or its actual normal-UP capability");
    exact_active=0;exact_completion=20;
    check(!product::revoke_exact_generation(exact_active,exact_completion,20)&&exact_active==0&&exact_completion==0,
          "mapped handler returning after its own UP retires only its exact old completion token");
    exact_active=21;exact_completion=20;
    check(!product::revoke_exact_generation(exact_active,exact_completion,20)&&exact_active==21&&exact_completion==0,
          "active CAS and completion CAS independently preserve a newer gesture while retiring an old token");
    check(!product::revoke_exact_generation(exact_active,exact_completion,0)&&exact_active==21,
          "zero-generation discrete command cannot revoke a gesture");
    // Deterministic producer-ahead-of-GUI order: DOWN/MOVE/normal UP have all
    // been observed by the hook before the GUI drains their queued records.
    product::InputEvent delayed_move;delayed_move.kind=product::InputKind::Move;delayed_move.generation=30;
    product::InputEvent actual_up=delayed_move;actual_up.kind=product::InputKind::Up;
    actual_up.end=v1::End::NormalUp;actual_up.point={180,150};
    check(!product::dispatch_motion_permission(delayed_move,false,0,30,true,false),
          "hook UP still retires old queued MOVE even when the GUI has not consumed it");
    check(!product::dispatch_motion_permission(actual_up,true,0,30,true,false),
          "actual same-generation normal UP cannot submit a new continuous native operation when GUI dispatch is late");
    check(!product::dispatch_motion_permission(actual_up,false,0,30,true,false)&&
          !product::dispatch_motion_permission(delayed_move,true,0,30,true,false),
          "terminal capability cannot authorize ordinary dispatch or turn an old MOVE into UP");
    check(!product::dispatch_motion_permission(actual_up,true,0,30,true,true),
          "the serialized dispatcher cannot claim a second final quantum for the same UP");
    check(!product::dispatch_motion_permission(actual_up,true,31,30,true,false)&&
          !product::dispatch_motion_permission(actual_up,true,31,0,true,false)&&
          !product::operation_permission(30,31,30,true,true),
          "a later actual DOWN rejects the older UP even for contradictory stale completion readings");
    check(!product::dispatch_motion_permission(actual_up,true,0,30,false,false)&&
          !product::dispatch_motion_permission(actual_up,true,0,0,true,false),
          "pause stop callback failure or revoked terminal token cannot issue final geometry");
    auto invalid_up=actual_up;invalid_up.end=v1::End::FocusLost;
    check(!product::dispatch_motion_permission(invalid_up,true,0,30,true,false),
          "source or foreground failure at the actual UP is not a normal final motion");
    invalid_up=actual_up;invalid_up.kind=product::InputKind::Abort;
    check(!product::dispatch_motion_permission(invalid_up,true,0,30,true,false),
          "abort receipt cannot masquerade as physical normal UP even with an old token");
    invalid_up=actual_up;invalid_up.generation=0;
    check(!product::dispatch_motion_permission(invalid_up,true,0,0,true,false),
          "generation zero discrete commands never mint terminal gesture motion");
    // Follow the exact production decisions, not an always-true test adapter:
    // hook ahead of GUI -> stale MOVE rejected -> real UP cannot compensate
    // with a never-issued continuous native call. No active rearming.
    std::uint64_t lag_active=0,lag_completion=30;bool terminal_consumed=false;
    unsigned active_issues{},terminal_issues{};
    std::optional<v1::Rect> final_geometry;
    if(product::dispatch_motion_permission(delayed_move,false,lag_active,lag_completion,true,terminal_consumed))++active_issues;
    if(product::dispatch_motion_permission(actual_up,true,lag_active,lag_completion,true,terminal_consumed)){
        terminal_consumed=true;
        if(product::operation_permission(30,lag_active,lag_completion,true,true)){
            final_geometry=v1::intent({100,100,500,400},{120,120},{actual_up.point.x,actual_up.point.y},{});
            ++terminal_issues;
        }
    }
    check(active_issues==0&&terminal_issues==0&&!final_geometry&&lag_active==0,
          "late GUI cannot disguise an unissued post-UP continuous placement as an in-flight completion");
    lag_completion=0;
    check(!product::dispatch_motion_permission(actual_up,true,lag_active,lag_completion,true,terminal_consumed)&&
          !product::dispatch_motion_permission(delayed_move,false,lag_active,lag_completion,true,terminal_consumed),
          "retiring completion leaves no late active or repeated terminal submission");
    // Actual production solver sequence behind the observed snap -> normal-UP
    // free-rect jump. A duplicate cursor can carry the same native millisecond;
    // invalid speed evidence must not undo an already exact completed quantum.
    namespace magnet=panebind::core::magnet;
    const v1::Rect magnetic_initial{100,80,360,440},magnetic_raw{115,84,375,444},magnetic_exact{120,80,380,440};
    const std::array<magnet::MagnetTarget,1> magnetic_targets{{
        {panebind::core::model::WindowId("neighbour"),{380,80,640,440},false}}};
    magnet::MagnetInput magnetic_request;
    magnetic_request.initial=magnetic_initial;magnetic_request.raw=magnetic_raw;
    magnetic_request.targets=magnetic_targets;magnetic_request.motion=magnet::MotionSample{magnetic_initial,1000,1100,1000,1000};
    const auto magnetic_move=magnet::MagnetConstraintSolver::solve(magnetic_request);
    check(magnetic_move.proposal&&magnetic_move.proposal->corrected==magnetic_exact,
          "actual product solver first consumes one exact XY snapped cursor quantum");
    magnetic_request.motion=magnet::MotionSample{magnetic_raw,1100,1100,1000,1000};
    const auto repeated_terminal=magnet::MagnetConstraintSolver::solve(magnetic_request);
    check(repeated_terminal.motion==magnet::MotionState::InvalidSample&&!repeated_terminal.proposal,
          "duplicate same-tick UP retains the real solver invalid-sample fact rather than inventing elapsed time");
    auto magnetic_up=actual_up;magnetic_up.point={315,104};
    const std::optional<product::ExactMotionPoint> magnetic_progress{{30,magnetic_up.point}};
    const bool same_point_complete=product::motion_point_already_exact(magnetic_up,true,magnetic_progress);
    const auto repeated_terminal_geometry=same_point_complete?magnetic_exact:
        repeated_terminal.proposal?repeated_terminal.proposal->corrected:magnetic_raw;
    check(repeated_terminal_geometry==magnetic_exact,
          "same-point normal UP cannot replace an already exact snapped receipt with its free intent");
    auto duplicate_active_move=magnetic_up;duplicate_active_move.kind=product::InputKind::Move;
    const auto duplicate_active_geometry=product::motion_point_already_exact(duplicate_active_move,false,magnetic_progress)?magnetic_exact:
        repeated_terminal.proposal?repeated_terminal.proposal->corrected:magnetic_raw;
    check(duplicate_active_geometry==magnetic_exact,
          "same-point active MOVE cannot replace an already exact snapped receipt with its free intent");
    auto changed_active_move=duplicate_active_move;++changed_active_move.point.x;
    check(product::dispatch_motion_permission(duplicate_active_move,false,30,0,true,false)&&
          product::motion_point_already_exact(duplicate_active_move,false,magnetic_progress)&&
          !product::motion_point_already_exact(changed_active_move,false,magnetic_progress)&&
          !product::dispatch_motion_permission(duplicate_active_move,false,0,30,true,false)&&
          !product::dispatch_motion_permission(duplicate_active_move,false,31,0,true,false),
          "active exact-point dedup still needs real MOVE authority and never consumes a new cursor or revives a retired generation");
    check(same_point_complete&&!product::motion_point_already_exact(magnetic_up,false,magnetic_progress)&&
          !product::motion_point_already_exact(magnetic_up,true,std::nullopt),
          "actual terminal branch only skips an already exact completed cursor, not active or unconsumed motion");
    auto different_point_up=magnetic_up;different_point_up.point.x+=4;
    auto later_up=magnetic_up;++later_up.generation;
    auto aborted_up=magnetic_up;aborted_up.end=v1::End::FocusLost;
    check(!product::motion_point_already_exact(different_point_up,true,magnetic_progress)&&
          !product::motion_point_already_exact(later_up,true,magnetic_progress)&&
          !product::motion_point_already_exact(aborted_up,true,magnetic_progress),
          "new cursor displacement, new generation and abnormal UP never reuse an older exact cursor receipt");
    // The same production static writer preflight used before the no-op branch
    // checks actual participant geometry; opaque identities are never queried.
    product::Snapshot confirmed;
    confirmed.hwnd=reinterpret_cast<HWND>(std::uintptr_t{0x1234});confirmed.monitor=reinterpret_cast<HMONITOR>(std::uintptr_t{0x4321});
    confirmed.pid=20;confirmed.tid=30;confirmed.born=40;confirmed.key=50;
    confirmed.visible=magnetic_exact;confirmed.positioning={112,80,388,448};confirmed.work={0,0,1920,1040};
    confirmed.dpi=96;confirmed.exe=L"C:\\fixture\\owned-test.exe";confirmed.window_class=L"PaneBind.Test.Writer";confirmed.can_resize=true;
    const auto same_frame=product::WindowWriter::prepare(confirmed,confirmed,confirmed.visible);
    check(same_frame.accepted&&same_frame.noop,
          "same-point terminal branch accepts only actual expected exact geometry without issuing native placement");
    auto translated=confirmed;translated.visible=magnetic_raw;translated.positioning={107,84,383,452};
    auto different_positioning=confirmed;different_positioning.positioning={113,80,389,448};
    auto different_dpi=confirmed;different_dpi.dpi=120;
    auto different_monitor=confirmed;different_monitor.monitor=reinterpret_cast<HMONITOR>(std::uintptr_t{0x8765});
    auto different_work=confirmed;different_work.work={0,0,1900,1040};
    auto different_identity=confirmed;++different_identity.born;
    check(!product::WindowWriter::prepare(confirmed,translated,confirmed.visible).accepted&&
          !product::WindowWriter::prepare(confirmed,different_positioning,confirmed.visible).accepted&&
          !product::WindowWriter::prepare(confirmed,different_dpi,confirmed.visible).accepted&&
          !product::WindowWriter::prepare(confirmed,different_monitor,confirmed.visible).accepted&&
          !product::WindowWriter::prepare(confirmed,different_work,confirmed.visible).accepted&&
          !product::WindowWriter::prepare(confirmed,different_identity,confirmed.visible).accepted,
          "same completed cursor cannot hide external geometry, positioning, DPI, monitor, work-area or identity change");
    // A real fast move remains free at a duplicate UP; merely waiting until UP
    // must not create a brand-new attraction quantum at the unchanged cursor.
    magnetic_request.motion=magnet::MotionSample{magnetic_initial,1000,1001,1000,1000};
    const auto fast_move=magnet::MagnetConstraintSolver::solve(magnetic_request);
    check(fast_move.motion==magnet::MotionState::Suppressed&&!fast_move.proposal,
          "actual production speed suppression produces the completed free rect");
    magnetic_request.motion=magnet::MotionSample{magnetic_raw,1001,1200,1000,1000};
    const auto idle_terminal=magnet::MagnetConstraintSolver::solve(magnetic_request);
    const auto fast_terminal_geometry=product::motion_point_already_exact(magnetic_up,true,magnetic_progress)?magnetic_raw:
        idle_terminal.proposal?idle_terminal.proposal->corrected:magnetic_raw;
    check(idle_terminal.proposal&&fast_terminal_geometry==magnetic_raw,
          "already exact fast-free move does not suddenly snap merely because normal UP has a later tick");
    const auto fast_duplicate_geometry=product::motion_point_already_exact(duplicate_active_move,false,magnetic_progress)?magnetic_raw:
        idle_terminal.proposal?idle_terminal.proposal->corrected:magnetic_raw;
    check(fast_duplicate_geometry==magnetic_raw,
          "same-point active MOVE also preserves exact speed-suppressed free geometry instead of creating new attraction");
    unsigned duplicate_terminal_issues{};
    if(product::dispatch_motion_permission(magnetic_up,true,0,30,true,false)&&
       !product::motion_point_already_exact(magnetic_up,true,magnetic_progress))++duplicate_terminal_issues;
    unsigned unconsumed_terminal_issues{};
    if(product::dispatch_motion_permission(different_point_up,true,0,30,true,false)&&
       !product::motion_point_already_exact(different_point_up,true,magnetic_progress))++unconsumed_terminal_issues;
    check(duplicate_terminal_issues==0&&unconsumed_terminal_issues==0&&
          !product::dispatch_motion_permission(different_point_up,true,31,30,true,false)&&
          !product::dispatch_motion_permission(delayed_move,false,0,30,true,false),
          "actual UP routing issues no new continuous quantum, even at an unconsumed cursor, and never rearms stale MOVE");
    std::atomic<std::uint64_t> finish_active{31},finish_completion{31};
    check(!product::finish_exact_generation(finish_active,finish_completion,30,v1::End::NormalUp)&&
          finish_active==31&&finish_completion==31,
          "GUI finishing older normal UP cannot revoke the newer actual DOWN or its completion token");
    check(!product::finish_exact_generation(finish_active,finish_completion,30,v1::End::SourceLost)&&
          finish_active==31&&finish_completion==31,
          "older GUI context failure also cannot revoke newer generation input");
    finish_active=0;finish_completion=30;
    check(!product::finish_exact_generation(finish_active,finish_completion,30,v1::End::NormalUp)&&
          finish_active==0&&finish_completion==30,
          "same generation normal finish preserves only its separate release-dock command token until consumed");
    check(!product::finish_exact_generation(finish_active,finish_completion,30,v1::End::FocusLost)&&
          finish_active==0&&finish_completion==0,
          "actual context loss retires terminal permission instead of ignoring an authority failure");
    // Actual apply() uses this exact receipt classifier with the lease it kept
    // before batch(). No test-only adapter can redefine the retirement result.
    auto sampled_capture=std::make_shared<product::GestureCaptureContext>();sampled_capture->generation=30;
    product::WindowWriter::Receipt withdrawn;
    withdrawn.reason="batch_preflight_capture_matching_UP_retired_before_native_call";
    check(!product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
          "unissued capture rejection alone is not actual matching UP evidence");
    sampled_capture->matching_up_observed=true;
    using GuiDisposition=product::GestureGuiDisposition;
    check(product::gesture_gui_disposition(false,product::capture_matching_up_retired_reason,30,sampled_capture,false)==
              GuiDisposition::MatchingUpRetired&&
          !sampled_capture->normal_up_observed.load()&&
          !product::captured_release_can_complete(true,false,false,true,true),
          "actual GUI pre-writer rejection after matched Raw UP waits without FocusLost, native permission or synthetic NormalUp");
    check(product::gesture_gui_disposition(false,product::capture_matching_up_retired_reason,30,sampled_capture,true)==GuiDisposition::Rejected&&
          product::gesture_gui_disposition(false,product::capture_matching_up_retired_reason,31,sampled_capture,false)==GuiDisposition::Rejected&&
          product::gesture_gui_disposition(false,product::capture_matching_up_retired_reason,30,{},false)==GuiDisposition::Rejected,
          "GUI retirement requires revoked same generation and the original capture lease, never newer or unknown context");
    for(const auto* actual_context_failure:{"capture_foreign_window_present","capture_source_foreground_changed",
            "capture_source_identity_or_GUI_unknown","capture_owner_identity_or_GUI_unknown_after_matching_UP",
            "capture_source_native_interaction_present"})
        check(product::gesture_gui_disposition(false,actual_context_failure,30,sampled_capture,false)==GuiDisposition::Rejected,
              "GUI pre-writer terminal split preserves actual foreground, identity, foreign and unknown context rejection");
    check(product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false)&&
          !product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,true)&&
          !product::WindowWriter::preserves_terminal_receipt(withdrawn,31,sampled_capture,false)&&
          !product::WindowWriter::preserves_terminal_receipt(withdrawn,30,{},false),
          "only matching retired generation and its retained lease wait for actual terminal receipts");
    check(!product::captured_release_can_complete(true,false,false,true,true),
          "waiting after matching UP withdrawal still cannot complete without actual Raw plus legacy UP facts");
    withdrawn.completion_known=false;
    check(!product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
          "unknown native completion cannot be relabeled as an unissued matching-UP withdrawal");
    withdrawn.completion_known=true;withdrawn.native_attempted=true;
    check(!product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
          "an attempted native failure cannot be relabeled as an unissued matching-UP withdrawal");
    withdrawn.native_attempted=false;
    for(const auto* actual_failure:{"batch_preflight_capture_foreign_window_present",
            "batch_preflight_capture_source_identity_or_GUI_unknown","batch_preflight_geometry_context_changed",
            "batch_preflight_unattributed_geometry_change","batch_preflight_capture_source_foreground_changed"}){
        withdrawn.reason=actual_failure;
        check(!product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
              "actual identity, geometry, DPI/monitor, foreground or foreign-capture failure still rejects after matching UP");
    }
    withdrawn.native_attempted=withdrawn.native_succeeded=withdrawn.identity_valid=withdrawn.geometry_exact=true;
    withdrawn.reason="post_native_capture_cleanup_not_yet_confirmed";
    withdrawn.post_context_reason=withdrawn.reason;
    check(product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
          "already issued successful exact geometry retains the existing narrow cleanup transition semantics");
    withdrawn.post_context_reason="capture_foreign_window_present";
    check(!product::WindowWriter::preserves_terminal_receipt(withdrawn,30,sampled_capture,false),
          "an issued call with actual foreign capture is not a normal cleanup transition");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures==0?0:1;
}
