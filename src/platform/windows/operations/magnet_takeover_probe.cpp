// Test executable only: never linked into the product / Explorer runtime.
#include "platform/windows/operations/test_foreground_bootstrap_model.h"
#include "platform/windows/operations/window_rect_adjustment.h"
#include "platform/windows/operations/magnet_postverify_diagnostic.h"
#include "platform/windows/operations/magnet_handoff_preflight_diagnostic.h"
#include "platform/windows/operations/magnet_input_isolation_diagnostic.h"
#include "platform/windows/operations/magnet_owned_abort_diagnostic.h"
#include <windows.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
constexpr UINT finish_message=WM_APP+52;
constexpr UINT handoff_message=WM_APP+53,raw_notice_message=WM_APP+54;
constexpr UINT winevent_notice_message=WM_APP+55,native_preflight_message=WM_APP+56;
constexpr UINT final_acceptance_message=WM_APP+57;
constexpr UINT abort_quiescence_message=WM_APP+58;
constexpr int samples=20, interval_ms=30;
constexpr ULONG_PTR input_tag=0x50424D41;
constexpr ULONG_PTR abort_cleanup_tag=0x50424655;
HWND owned{};
DWORD ui_thread{};
HANDLE log_file=INVALID_HANDLE_VALUE,entered{},exited{},stepped{},timer{};
HANDLE activation_event{},pointer_arrived{},nonclient_down{},correction_finished{},driver_finished{};
HANDLE activation_down_received{},activation_up_received{};
HWND guard{};std::atomic<bool> cancelled{};
std::atomic<bool> cancel_pending{},foreign_capture_transferred{};
std::atomic<std::int64_t> cancel_return_boundary{};
std::atomic<int> native_drag_after_return{};
HANDLE raw_pipe{},receiver_stop{},receiver_armed{},receiver_ready{},raw_motion{},raw_up{};
PROCESS_INFORMATION receiver_process{};std::thread receiver_reader;
std::atomic<bool> receiver_ok{true},receiver_removed{},receiver_destroyed{};
std::atomic<int> raw_packets{},raw_movements{},raw_ups{};
std::atomic<std::uint32_t> receiver_watermark{};
std::atomic<HWND> receiver_hwnd{};std::atomic<DWORD> receiver_tid{};
std::atomic<bool> activate_seen{},focus_seen{},activation_armed{},topmost_active{};
std::atomic<bool> direct_activate_seen{},direct_focus_seen{};
std::atomic<bool> stop_requested{};
std::atomic<const char*> probe_failure{"none"};
bool set_foreground_success{},bootstrap_emitted{},activation_down{};
POINT activation_point{};
namespace fg=panebind::test::foreground;
struct Bootstrap {
    bool attempted{},click_required{},temporary_topmost{},move_success{},down_success{},up_success{},event_seen{},topmost_restored{true};
    fg::ActivationProof last_proof;
    LRESULT hit{};HWND root{};
} bootstrap;
std::mutex log_mutex;
std::uint64_t sequence{};
std::atomic<bool> log_ok{true};
std::atomic<int> gesture{},callbacks{};
bool active{}; // UI thread only
RECT work_area{},virtual_area{};
POINT saved_cursor{};
bool driver_pass{},restored{}; // read by main only after join
std::string failure;
bool end_mode{},diagnostic_mode{},isolation_mode{};int selected_gesture=1;
bool reliability_mode{},controlled_abort{};
namespace ab=panebind::test::abort;
std::atomic<bool> abort_retiring{},abort_quiescent{};
HANDLE abort_quiescence_finished{};
std::atomic<std::uint64_t> abort_quiescence_sequence{};
std::atomic<int> native_down_count{},native_enter_count{};
std::atomic<std::int64_t> native_down_qpc{};
const char* driver_phase="startup";int driver_sample{}; // driver thread only
std::string abort_cleanup_result="NOT_RUN";
bool abort_cleanup_attempted{}; // driver thread: actual API entry, never receipt inference
struct DownLedger {
    ab::PendingDownLedger facts;
    bool tracking{},request_consumed{},request_sent{},raw_down_recorded{};
    DWORD request_flags{};ULONG_PTR request_tag{};
    std::int64_t request_start{};
    std::uint32_t request_watermark{};
    std::uint64_t intent_sequence{},up_input_sequence{};
    std::uint64_t non_test_movements{};
    std::int64_t snapshot_qpc{};
    std::uint32_t observed_receiver_sequence{};
};
DownLedger down_ledger;std::mutex down_ledger_mutex;
std::mutex abort_request_mutex;std::uint64_t abort_request_sequence{};
const char* abort_request_phase="initial";
namespace ti=panebind::test::isolation;
std::uintptr_t run_nonce{};
HWND input_shield{};
std::atomic<bool> shield_active{},shield_healthy{true},shield_activated{};
std::atomic<bool> source_retired{};
bool shield_created{},shield_destroyed{},shield_needed{}; // owner UI only
ti::ShieldPlan frozen_shield_plan;
std::array<POINT,18> remaining_input_points{}; // continuation_mutex; frozen before native DOWN
POINT planned_current_cursor{}; // same frozen original-anchor path, sample 2
std::atomic<std::int64_t> isolation_ready_qpc{};
std::atomic<std::uint64_t> isolation_ready_sequence{};
std::uint64_t product_preflight_sequence{},source_acceptance_sequence{}; // UI only
HANDLE final_acceptance_finished{};
std::uint64_t takeover_end_sequence{}; // UI only
std::atomic<std::int64_t> native_end_qpc{},native_enter_qpc{};
std::atomic<std::uint64_t> native_end_sequence{},native_enter_sequence{},native_down_sequence{};
std::atomic<int> native_drag_after_end{},unowned_geometry_changes{};
std::atomic<bool> takeover_scope{},handoff_ready{},takeover_healthy{true};
HANDLE handoff_finished{},quantum_finished{},takeover_finished{};
HMONITOR frozen_monitor{};UINT frozen_dpi{};
namespace hd=panebind::test::handoff;
HWINEVENTHOOK movesize_hook{};
HANDLE winevent_finished{},native_preflight_finished{};
std::atomic<bool> winevent_healthy{true},cleanup_observation{};
std::atomic<std::uint64_t> active_gesture_id{};
std::atomic<std::int64_t> gesture_arm_qpc{},winevent_end_qpc{};
std::atomic<DWORD> gesture_arm_tick{};
std::atomic<std::uint64_t> winevent_end_sequence{};
std::atomic<int> native_drag_after_winevent_end{};
struct WinEventReceipt {
    HWINEVENTHOOK hook{};DWORD event{},event_thread{},event_time{},callback_tid{};
    HWND window{};LONG object{},child{};
    std::uint64_t callback_sequence{},record_sequence{},gesture_id{};
    std::int64_t callback_qpc{},arm_qpc{};DWORD arm_tick{};
};
std::mutex winevent_mutex;
std::array<WinEventReceipt,128> winevent_queue{};
std::size_t winevent_queue_size{};
bool winevent_notice_posted{};
bool movesize_hook_removal_attempted{};
std::uint64_t winevent_callback_counter{};
std::optional<WinEventReceipt> matched_winevent_start,matched_winevent_end;
std::array<std::int64_t,128> drag_receipts{};std::size_t drag_receipt_count{};

std::int64_t qpc(){LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart;}
std::string flag(bool b){return b?"true":"false";}
std::string rect(const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+std::to_string(r.right)+","+std::to_string(r.bottom)+"]";}
std::string point(POINT p){return "["+std::to_string(p.x)+","+std::to_string(p.y)+"]";}
std::uintptr_t number(HWND h){return reinterpret_cast<std::uintptr_t>(h);}
bool equal(const RECT& a,const RECT& b){return EqualRect(&a,&b)!=FALSE;}
std::uint64_t record(std::string_view type,const std::string& fields={}){
    std::lock_guard lock{log_mutex};
    if(!log_ok)return 0;
    if(sequence>=4096){log_ok=false;return 0;}
    const auto assigned=++sequence;
    const auto line="{\"schema\":\""+std::string(reliability_mode?"r1c4b-takeover-owned/v5":(isolation_mode?"r1c4b-takeover-owned/v4":(diagnostic_mode?"r1c4b-takeover-owned/v3":(end_mode?"r1c4b-takeover-owned/v2":"r1c4b-takeover-owned/v1"))))+"\",\"sequence\":"+std::to_string(assigned)+",\"type\":\""+std::string(type)+"\",\"gesture\":"+std::to_string(gesture.load())+",\"qpc\":"+std::to_string(qpc())+fields+(end_mode&&type=="EXIT"?",\"end_sequence\":"+std::to_string(assigned):"")+(reliability_mode?",\"event_scope\":\""+std::string(abort_retiring?"cleanup":"gesture")+"\",\"event_acceptance_eligible\":"+flag(!abort_retiring&&takeover_scope):"")+"}\n";
    DWORD written{};log_ok=WriteFile(log_file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
    return sequence;
}
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
bool identity(){DWORD pid{};return owned&&GetWindowThreadProcessId(owned,&pid)==ui_thread&&pid==GetCurrentProcessId()&&(!isolation_mode||static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA))==run_nonce);}
bool input_root_owned(HWND root){
    DWORD pid{};
    if(isolation_mode&&winevent_end_qpc){
        if(root==owned)return identity();
        return root&&root==input_shield&&shield_active&&GetWindowThreadProcessId(root,&pid)==ui_thread&&pid==GetCurrentProcessId()&&static_cast<std::uintptr_t>(GetWindowLongPtrW(root,GWLP_USERDATA))==run_nonce;
    }
    return (root==owned||root==guard)&&GetWindowThreadProcessId(root,&pid)==ui_thread&&pid==GetCurrentProcessId();
}
bool post_source_work(UINT message) noexcept {
    if(!isolation_mode)return PostMessageW(owned,message,0,0)!=FALSE;
    try{
        DWORD pid{};const auto tid=GetWindowThreadProcessId(owned,&pid);
        const auto nonce=pid==GetCurrentProcessId()&&tid==ui_thread?static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA)):0;
        const bool exact=owned&&pid==GetCurrentProcessId()&&tid==ui_thread&&nonce==run_nonce;
        if(stop_requested||source_retired||!exact){
            record("source_work_post_skipped",",\"message\":"+std::to_string(message)+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+std::to_string(pid)+",\"actual_source_tid\":"+std::to_string(tid)+",\"actual_source_nonce\":"+std::to_string(nonce)+",\"own_identity\":"+flag(exact)+",\"stop_requested\":"+flag(stop_requested)+",\"source_retired\":"+flag(source_retired));
            if(message==winevent_notice_message&&winevent_finished)SetEvent(winevent_finished);
            if(message==native_preflight_message&&native_preflight_finished)SetEvent(native_preflight_finished);
            if(message==handoff_message&&handoff_finished)SetEvent(handoff_finished);
            if(message==raw_notice_message){if(quantum_finished)SetEvent(quantum_finished);if(takeover_finished)SetEvent(takeover_finished);}
            if(message==final_acceptance_message&&final_acceptance_finished)SetEvent(final_acceptance_finished);
            return false;
        }
        return PostMessageW(owned,message,0,0)!=FALSE;
    }catch(...){return false;}
}
bool cancellation_gui_safe(const GUITHREADINFO& gui){
    const bool no_move=!gui.hwndMoveSize&&!(gui.flags&GUI_INMOVESIZE);
    const bool own_pending=cancel_pending&&gui.hwndMoveSize==owned;
    return !gui.hwndCapture&&!gui.hwndMenuOwner&&!(gui.flags&(GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE))&&(no_move||own_pending);
}
bool desktop_available(){
    HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!input)return false;
    wchar_t input_name[256]{},current_name[256]{};DWORD needed{};
    const bool matches=GetUserObjectInformationW(input,UOI_NAME,input_name,sizeof(input_name),&needed)&&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,current_name,sizeof(current_name),&needed)&&
        std::wstring_view(input_name)==current_name&&std::wstring_view(input_name)==L"Default";
    CloseDesktop(input);
    LPWSTR memory{};DWORD bytes{};
    if(!matches||!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,WTS_CURRENT_SESSION,WTSSessionInfoEx,&memory,&bytes))return false;
    bool ready=false;
    if(bytes>=sizeof(WTSINFOEXW)){
        const auto& info=*reinterpret_cast<const WTSINFOEXW*>(memory);
        ready=info.Level==1&&info.Data.WTSInfoExLevel1.SessionState==WTSActive&&info.Data.WTSInfoExLevel1.SessionFlags==WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(memory);return ready&&GetForegroundWindow()!=nullptr;
}
bool pressed(int key){return (GetAsyncKeyState(key)&0x8000)!=0;}
bool other_input(){
    for(const int key:{VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_ESCAPE})if(pressed(key))return true;
    return false;
}
void pace(int milliseconds){
    LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(milliseconds)*10000;
    require(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)&&WaitForSingleObject(timer,2000)==WAIT_OBJECT_0,"test_pacing_failed");
}
struct Geometry{RECT p{},v{};bool p_ok{},v_ok{};DWORD error{};HRESULT hr{E_PENDING};};
std::mutex continuation_mutex;
Geometry intent_start,terminal_geometry;
POINT intent_pointer{}; // frozen from actual native button DOWN
Geometry expected_live,inflight_target; // owner UI only
std::uint64_t active_operation{},operation_counter{},quantum_counter{};
std::uint32_t consumed_motion{};
bool audit_live{}; // owner UI only, retired before test cleanup
void retire_source_fixture() noexcept {if(isolation_mode){source_retired=true;stop_requested=true;takeover_scope=false;audit_live=false;}}
void process_handoff();void process_raw_notice();
void process_winevents();void process_native_preflight();void remove_movesize_hook();
void destroy_input_shield(const char* reason,std::uint64_t acceptance,std::int64_t observed_up);
bool fresh_synthetic_input_isolation(POINT destination,DWORD flags,const char* scope);
void process_final_acceptance();
void process_abort_quiescence();
void perform_reliability_cleanup(bool test_down_owned);
struct PositionReceipt {std::int64_t qpc{};Geometry geometry;};
std::array<PositionReceipt,128> position_receipts{};std::size_t position_receipt_count{};
bool winevent_audit_started{}; // UI owner only
std::uint64_t final_preflight_sequence{}; // UI owner only
Geometry capture(){
    Geometry g;if(!identity())return g;SetLastError(0);g.p_ok=GetWindowRect(owned,&g.p)!=FALSE;g.error=g.p_ok?0:GetLastError();
    g.hr=DwmGetWindowAttribute(owned,DWMWA_EXTENDED_FRAME_BOUNDS,&g.v,sizeof(g.v));g.v_ok=SUCCEEDED(g.hr);return g;
}
std::string geometry(const Geometry& g){return ",\"positioning\":"+(g.p_ok?rect(g.p):"null")+",\"visible\":"+(g.v_ok?rect(g.v):"null")+",\"positioning_error\":"+std::to_string(g.error)+",\"visible_hresult\":"+std::to_string(g.hr);}
bool contained(const RECT& r){return r.left>=work_area.left+100&&r.top>=work_area.top+100&&r.right<=work_area.right-100&&r.bottom<=work_area.bottom-100;}
bool tick_not_before(DWORD value,DWORD boundary){return static_cast<LONG>(value-boundary)>=0;}
void CALLBACK movesize_event_callback(HWINEVENTHOOK hook,DWORD event,HWND window,LONG object,LONG child,DWORD event_thread,DWORD event_time) noexcept {
    try{
    // Envelope ingress only. No capture, input, pumping, or geometry operation.
    WinEventReceipt r{hook,event,event_thread,event_time,GetCurrentThreadId(),window,object,child};
    r.callback_qpc=qpc();r.callback_sequence=++winevent_callback_counter;
    r.gesture_id=active_gesture_id;r.arm_qpc=gesture_arm_qpc;r.arm_tick=gesture_arm_tick;
    r.record_sequence=record("winevent_callback",",\"hook\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hook))+",\"event\":"+std::to_string(event)+",\"hwnd\":"+std::to_string(number(window))
        +",\"event_thread\":"+std::to_string(event_thread)+",\"event_time\":"+std::to_string(event_time)+",\"object_id\":"+std::to_string(object)+",\"child_id\":"+std::to_string(child)+",\"callback_tid\":"+std::to_string(r.callback_tid)
        +",\"callback_sequence\":"+std::to_string(r.callback_sequence)+",\"callback_qpc\":"+std::to_string(r.callback_qpc)+",\"gesture_id\":"+std::to_string(r.gesture_id)+",\"arm_qpc\":"+std::to_string(r.arm_qpc)+",\"arm_tick\":"+std::to_string(r.arm_tick));
    bool notify=false;
    {std::lock_guard lock(winevent_mutex);
        if(winevent_queue_size==winevent_queue.size()){winevent_healthy=false;}
        else{winevent_queue[winevent_queue_size++]=r;if(!winevent_notice_posted){winevent_notice_posted=true;notify=true;}}
    }
    if(!r.record_sequence)winevent_healthy=false;
    if(notify&&!post_source_work(winevent_notice_message))winevent_healthy=false;
    }catch(...){winevent_healthy=false;post_source_work(winevent_notice_message);}
}
void install_movesize_hook(){
    require(GetCurrentThreadId()==ui_thread&&identity(),"winevent_install_identity_failed");
    SetLastError(0);movesize_hook=SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART,EVENT_SYSTEM_MOVESIZEEND,nullptr,movesize_event_callback,GetCurrentProcessId(),ui_thread,WINEVENT_OUTOFCONTEXT);
    const auto error=movesize_hook?0:GetLastError();
    record("winevent_hook_installed",",\"hook\":"+std::to_string(reinterpret_cast<std::uintptr_t>(movesize_hook))+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"install_tid\":"+std::to_string(GetCurrentThreadId())+",\"event_min\":10,\"event_max\":11,\"flags\":0,\"dll\":0,\"skip_own_process\":false,\"skip_own_thread\":false,\"install_success\":"+flag(movesize_hook!=nullptr)+",\"error\":"+std::to_string(error));
    require(movesize_hook!=nullptr,"winevent_install_failed");
}
void remove_movesize_hook(){
    if(!diagnostic_mode||!movesize_hook||movesize_hook_removal_attempted)return;
    movesize_hook_removal_attempted=true;
    const auto hook=movesize_hook;SetLastError(0);
    const bool removed=GetCurrentThreadId()==ui_thread&&UnhookWinEvent(hook)!=FALSE;
    const auto error=removed?0:GetLastError();if(removed)movesize_hook=nullptr;else winevent_healthy=false;
    record("winevent_hook_removed",",\"hook\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hook))+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"remove_tid\":"+std::to_string(GetCurrentThreadId())+",\"remove_success\":"+flag(removed)+",\"error\":"+std::to_string(error));
}
void enable_winevent_audit(){
    if(winevent_audit_started||!native_end_qpc||!winevent_end_qpc)return;
    Geometry previous;{std::lock_guard lock(continuation_mutex);previous=intent_start;}
    for(std::size_t i=0;i<position_receipt_count;++i){
        const auto& r=position_receipts[i];
        if(!r.geometry.p_ok||!r.geometry.v_ok){takeover_healthy=false;continue;}
        if(r.qpc>winevent_end_qpc&&previous.p_ok&&previous.v_ok&&(!equal(r.geometry.p,previous.p)||!equal(r.geometry.v,previous.v)))++unowned_geometry_changes;
        previous=r.geometry;
    }
    int later{};for(std::size_t i=0;i<drag_receipt_count;++i)if(drag_receipts[i]>winevent_end_qpc)++later;
    native_drag_after_winevent_end=later;
    if(later||unowned_geometry_changes)takeover_healthy=false;
    const auto g=capture();expected_live=g;audit_live=true;winevent_audit_started=true;
    record("winevent_barrier_snapshot",",\"winevent_end_sequence\":"+std::to_string(winevent_end_sequence.load())+",\"winevent_end_qpc\":"+std::to_string(winevent_end_qpc.load())+",\"native_drag_after_winevent_end\":"+std::to_string(later)+",\"unowned_geometry_changes\":"+std::to_string(unowned_geometry_changes.load())+geometry(g));
}
void process_winevents(){
    std::array<WinEventReceipt,128> received{};std::size_t count{};
    {std::lock_guard lock(winevent_mutex);count=winevent_queue_size;std::copy_n(winevent_queue.begin(),count,received.begin());winevent_queue_size=0;winevent_notice_posted=false;}
    for(std::size_t i=0;i<count;++i){
        const auto& r=received[i];const char* reason="None";bool accepted=false;
        DWORD actual_source_pid{};const auto actual_source_tid=GetWindowThreadProcessId(owned,&actual_source_pid);
        const auto actual_source_nonce=isolation_mode?static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA)):0;
        const bool source_identity=actual_source_pid==GetCurrentProcessId()&&actual_source_tid==ui_thread&&(!isolation_mode||actual_source_nonce==run_nonce);
        if(r.hook!=movesize_hook)reason="WrongHook";
        else if(r.window!=owned)reason="WrongWindow";
        else if(r.event_thread!=ui_thread)reason="WrongEventThread";
        else if(r.callback_tid!=ui_thread)reason="WrongCallbackThread";
        else if(!source_identity)reason="SourceIdentityChanged";
        else if(!r.gesture_id||r.gesture_id!=active_gesture_id||r.arm_qpc!=gesture_arm_qpc||r.arm_tick!=gesture_arm_tick)reason="WrongGesture";
        else if(r.callback_qpc<=r.arm_qpc||!native_down_sequence)reason="CallbackBeforeGesture";
        else if(!tick_not_before(r.event_time,r.arm_tick))reason="EventBeforeGesture";
        else if(r.event==EVENT_SYSTEM_MOVESIZESTART){
            if(matched_winevent_start)reason="StartAlreadyObserved";
            else{matched_winevent_start=r;accepted=true;}
        }else if(r.event==EVENT_SYSTEM_MOVESIZEEND){
            if(!matched_winevent_start)reason="EndWithoutStart";
            else if(matched_winevent_end)reason="EndAlreadyObserved";
            else if(r.callback_sequence<=matched_winevent_start->callback_sequence||r.callback_qpc<=matched_winevent_start->callback_qpc||!tick_not_before(r.event_time,matched_winevent_start->event_time))reason="EventBeforeStart";
            else{matched_winevent_end=r;accepted=true;winevent_end_qpc=r.callback_qpc;winevent_end_sequence=r.record_sequence;}
        }else reason="InvalidEvent";
        record("winevent_match",",\"callback_sequence\":"+std::to_string(r.callback_sequence)+",\"callback_record_sequence\":"+std::to_string(r.record_sequence)+",\"gesture_id\":"+std::to_string(active_gesture_id.load())+",\"source_identity\":"+flag(source_identity)+",\"actual_source_pid\":"+std::to_string(actual_source_pid)+",\"actual_source_tid\":"+std::to_string(actual_source_tid)+",\"accepted\":"+flag(accepted)+",\"reason\":\""+reason+"\",\"matched_start_callback_sequence\":"+std::to_string(matched_winevent_start?matched_winevent_start->callback_sequence:0)
            +(isolation_mode?",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_nonce\":"+std::to_string(actual_source_nonce):""));
        if(accepted&&r.event==EVENT_SYSTEM_MOVESIZEEND){if(!reliability_mode||!abort_retiring)enable_winevent_audit();SetEvent(winevent_finished);}
    }
    if(!winevent_healthy){takeover_healthy=false;record("winevent_error",",\"reason\":\"winevent_ingress_unhealthy\"");SetEvent(winevent_finished);}
}
// A separate empty child process makes RIM_INPUTSINK empirically meaningful.
// It can observe mouse packets; it has no target or geometry-writing authority.
struct RawPacket {
    std::uint32_t kind{},serial{},pid{},tid{},input_code{},flags{},buttons{},error{},cursor_error{};
    std::int32_t dx{},dy{};
    std::uintptr_t hwnd{},foreground{};
    DWORD foreground_pid{};
    std::int64_t observed_qpc{};
    POINT cursor{};
    ULONG extra_information{};
    bool cursor_sampled{},cursor_ok{},left_down{},device_present{},tag_matches{},cleanup_tag_matches{},registration_ok{},removed{},destroyed{};
};
RawPacket last_motion,last_up;std::mutex raw_mutex;
RawPacket pending_motion,published_up;
std::uint32_t pending_first{},pending_count{};
bool motion_pending{},raw_up_seen{},notice_posted{}; // continuation_mutex
std::atomic<std::uint32_t> owner_processed_sequence{};
std::atomic<int> takeover_native_calls{};
struct InputReceipt {INPUT input;std::int64_t start{};std::uint32_t watermark{};};
DownLedger ledger_snapshot(){
    std::lock_guard lock(down_ledger_mutex);auto s=down_ledger;
    s.facts.native_down_sequence=native_down_sequence;s.facts.native_down_qpc=native_down_qpc;
    s.facts.native_down_matches=native_down_count==1&&native_down_sequence!=0;
    s.facts.native_enter_sequence=native_enter_sequence;s.facts.native_enter_qpc=native_enter_qpc;
    s.facts.native_enter_matches=native_enter_count==1&&native_enter_sequence!=0;
    s.snapshot_qpc=qpc(); // actual clone boundary while the ledger mutex is held
    return s;
}
std::string ledger_fields(const DownLedger& s){
    const auto& d=s.facts;
    return ",\"down_input_sequence\":"+std::to_string(d.down_input_sequence)+",\"down_start_qpc\":"+std::to_string(d.down_start_qpc)+",\"down_return_qpc\":"+std::to_string(d.down_return_qpc)+",\"down_sent\":"+flag(d.down_sent)
        +",\"receiver_watermark\":"+std::to_string(d.receiver_watermark)+",\"raw_down_receiver_sequence\":"+std::to_string(d.raw_down_receiver_sequence)+",\"raw_down_receiver_qpc\":"+std::to_string(d.raw_down_receiver_qpc)+",\"raw_down_matches\":"+flag(d.raw_down_matches)
        +",\"native_down_sequence\":"+std::to_string(d.native_down_sequence)+",\"native_down_qpc\":"+std::to_string(d.native_down_qpc)+",\"native_down_matches\":"+flag(d.native_down_matches)+",\"native_enter_sequence\":"+std::to_string(d.native_enter_sequence)+",\"native_enter_qpc\":"+std::to_string(d.native_enter_qpc)+",\"native_enter_matches\":"+flag(d.native_enter_matches)
        +",\"matching_up_seen\":"+flag(d.matching_up_seen)+",\"up_command_sent\":"+flag(d.up_command_sent)+",\"non_test_button_transitions\":"+std::to_string(d.non_test_button_transitions)+",\"unmatched_button_transitions\":"+std::to_string(d.unmatched_button_transitions)+",\"non_test_movements\":"+std::to_string(s.non_test_movements)+",\"ledger_pending\":"+flag(d.pending_owned())+",\"ledger_snapshot_qpc\":"+std::to_string(s.snapshot_qpc)+",\"ledger_receiver_sequence\":"+std::to_string(s.observed_receiver_sequence);
}
std::int64_t observe_ledger_packet(const RawPacket& p){
    if(!reliability_mode)return 0;
    std::lock_guard lock(down_ledger_mutex);auto& s=down_ledger;
    if(s.tracking){
        s.observed_receiver_sequence=p.serial;
        if(p.cursor_sampled&&!p.tag_matches&&!p.cleanup_tag_matches)++s.non_test_movements;
        constexpr unsigned transitions=0x03ff;
        if(p.buttons&transitions){
            if(!p.tag_matches&&!p.cleanup_tag_matches)++s.facts.non_test_button_transitions;
            const unsigned wanted=s.request_flags==MOUSEEVENTF_LEFTDOWN?RI_MOUSE_LEFT_BUTTON_DOWN:RI_MOUSE_LEFT_BUTTON_UP;
            const bool matches=s.request_start>0&&!s.request_consumed&&!p.device_present&&p.buttons==wanted&&p.extra_information==s.request_tag&&p.serial>s.request_watermark&&p.observed_qpc>=s.request_start&&p.input_code==RIM_INPUTSINK&&p.foreground==number(owned)&&p.foreground_pid==GetCurrentProcessId();
            if(!matches)++s.facts.unmatched_button_transitions;
            else{
                s.request_consumed=true;
                if(s.request_flags==MOUSEEVENTF_LEFTDOWN){s.facts.raw_down_receiver_sequence=p.serial;s.facts.raw_down_receiver_qpc=p.observed_qpc;s.facts.raw_down_matches=true;}
                else if(s.request_sent)s.facts.matching_up_seen=true;
            }
        }
    }
    // Includes pretracking packets. This is parent processing completion, not
    // the child receipt time or the later log-serialization time.
    return qpc();
}
std::uint64_t begin_ledger_intent(DWORD flags,ULONG_PTR tag,std::uint32_t watermark){
    if(!reliability_mode||gesture==0||(flags!=MOUSEEVENTF_LEFTDOWN&&flags!=MOUSEEVENTF_LEFTUP))return 0;
    {
        std::lock_guard lock(down_ledger_mutex);
        if(flags==MOUSEEVENTF_LEFTDOWN){down_ledger={};down_ledger.tracking=true;down_ledger.facts.receiver_watermark=watermark;down_ledger.observed_receiver_sequence=watermark;}
        down_ledger.request_flags=flags;down_ledger.request_tag=tag;down_ledger.request_watermark=watermark;down_ledger.request_start=0;down_ledger.request_consumed=false;down_ledger.request_sent=false;
    }
    const auto ref=record("ledger_input_intent",",\"flags\":"+std::to_string(flags)+",\"input_tag\":"+std::to_string(tag)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"intent_purpose\":\""+(flags==MOUSEEVENTF_LEFTDOWN?"native_down":(tag==abort_cleanup_tag?"abort_up":"gesture_up"))+"\"");
    {std::lock_guard lock(down_ledger_mutex);down_ledger.intent_sequence=ref;}
    require(ref&&log_ok,"evidence_capture_failed");return ref;
}
void ledger_api_started(std::uint64_t intent,std::int64_t start){if(intent){std::lock_guard lock(down_ledger_mutex);down_ledger.request_start=start;}}
void ledger_api_committed(std::uint64_t intent,std::uint64_t input_sequence,UINT sent,std::int64_t start,std::int64_t returned){
    if(!intent)return;
    {std::lock_guard lock(down_ledger_mutex);auto& s=down_ledger;s.request_sent=sent==1;
        if(s.request_flags==MOUSEEVENTF_LEFTDOWN){s.facts.down_input_sequence=input_sequence;s.facts.down_start_qpc=start;s.facts.down_return_qpc=returned;s.facts.down_sent=sent==1;}
        else{s.up_input_sequence=input_sequence;if(sent==1){s.facts.up_command_sent=true;if(s.request_consumed)s.facts.matching_up_seen=true;}}
        if(sent!=1&&s.request_consumed)++s.facts.unmatched_button_transitions;
    }
    const auto snapshot=ledger_snapshot();
    if(snapshot.raw_down_recorded&&snapshot.facts.native_down_matches&&snapshot.facts.native_enter_matches&&snapshot.facts.raw_down_matches)record("test_down_ledger",",\"ledger_intent_sequence\":"+std::to_string(intent)+ledger_fields(snapshot));
}
HANDLE child_pipe{},child_stop{},child_armed{};std::uint32_t child_sequence{};
bool send_packet(RawPacket packet){
    packet.serial=++child_sequence;packet.pid=GetCurrentProcessId();packet.tid=GetCurrentThreadId();packet.observed_qpc=qpc();
    DWORD written{};return WriteFile(child_pipe,&packet,sizeof(packet),&written,nullptr)&&written==sizeof(packet);
}
LRESULT CALLBACK raw_procedure(HWND h,UINT message,WPARAM w,LPARAM l){
    if(message==WM_INPUT){
        if(WaitForSingleObject(child_armed,0)==WAIT_OBJECT_0){
            RAWINPUT input{};UINT bytes=sizeof(input);SetLastError(0);
            const auto copied=GetRawInputData(reinterpret_cast<HRAWINPUT>(l),RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER));
            RawPacket packet;packet.kind=2;packet.hwnd=number(h);packet.input_code=GET_RAWINPUT_CODE_WPARAM(w);
            if(copied==static_cast<UINT>(-1)||copied<sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE)||copied!=input.header.dwSize||input.header.dwType!=RIM_TYPEMOUSE){packet.kind=3;packet.error=GetLastError();}
            else{
                const auto& mouse=input.data.mouse;packet.flags=mouse.usFlags;packet.buttons=mouse.usButtonFlags;packet.dx=mouse.lLastX;packet.dy=mouse.lLastY;
                packet.cursor_sampled=(mouse.usFlags&MOUSE_MOVE_ABSOLUTE)||mouse.lLastX||mouse.lLastY;
                if(packet.cursor_sampled){SetLastError(0);packet.cursor_ok=GetCursorPos(&packet.cursor)!=FALSE;packet.cursor_error=packet.cursor_ok?0:GetLastError();}
                packet.left_down=pressed(VK_LBUTTON);packet.device_present=input.header.hDevice!=nullptr;packet.extra_information=mouse.ulExtraInformation;packet.tag_matches=mouse.ulExtraInformation==input_tag;packet.cleanup_tag_matches=mouse.ulExtraInformation==abort_cleanup_tag;
                const HWND foreground=GetForegroundWindow();packet.foreground=number(foreground);GetWindowThreadProcessId(foreground,&packet.foreground_pid);
            }
            if(child_sequence>=2048){
                RawPacket overflow;overflow.kind=3;overflow.hwnd=number(h);overflow.error=ERROR_BUFFER_OVERFLOW;
                send_packet(overflow);SetEvent(child_stop);
            }else if(!send_packet(packet)){SetEvent(child_stop);}
        }
        // Read first, perform required system cleanup; never manufacture input.
        DefWindowProcW(h,message,w,l);return 0;
    }
    return DefWindowProcW(h,message,w,l);
}
void read_receiver(){
    std::uint32_t serial{};std::int64_t previous_qpc{};
    for(;;){
        RawPacket p{};DWORD bytes{};const bool received=ReadFile(raw_pipe,&p,sizeof(p),&bytes,nullptr)!=FALSE;
        if(!received||!bytes)break;
        if(bytes!=sizeof(p)||p.serial!=++serial||p.observed_qpc<previous_qpc||p.pid!=receiver_process.dwProcessId){receiver_ok=false;break;}previous_qpc=p.observed_qpc;
        receiver_watermark=p.serial;
        const std::string common=",\"receiver_sequence\":"+std::to_string(p.serial)+",\"receiver_qpc\":"+std::to_string(p.observed_qpc)+",\"receiver_hwnd\":"+std::to_string(p.hwnd)+",\"receiver_pid\":"+std::to_string(p.pid)+",\"receiver_tid\":"+std::to_string(p.tid);
        if(p.kind==1){
            DWORD pid{};const auto tid=GetWindowThreadProcessId(reinterpret_cast<HWND>(p.hwnd),&pid);
            receiver_hwnd=reinterpret_cast<HWND>(p.hwnd);receiver_tid=p.tid;
            const bool valid=pid==p.pid&&tid==p.tid&&p.registration_ok;
            record("receiver",common+",\"registration_verified\":"+flag(valid)+",\"usage_page\":1,\"usage\":2,\"registration_flags\":256,\"keyboard_registered\":false");
            if(!valid)receiver_ok=false;SetEvent(receiver_ready);
        }else if(p.kind==2){
            if(p.hwnd!=number(receiver_hwnd)||p.tid!=receiver_tid){receiver_ok=false;break;}
            if(p.input_code!=RIM_INPUTSINK||p.foreground_pid==p.pid||p.foreground_pid!=GetCurrentProcessId())receiver_ok=false;
            const auto ledger_observed_qpc=observe_ledger_packet(p);
            bool notify=false;
            if(end_mode&&takeover_scope){
                // Publish terminal lifecycle before any potentially delayed
                // logging. A pending motion can never override a received UP.
                std::lock_guard lock(continuation_mutex);
                if(!reliability_mode||!abort_retiring){
                    if(p.buttons&RI_MOUSE_LEFT_BUTTON_UP){published_up=p;raw_up_seen=true;motion_pending=false;pending_count=0;}
                    else if(p.cursor_sampled&&!raw_up_seen){
                        if(!motion_pending){pending_first=p.serial;pending_count=0;}
                        pending_motion=p;motion_pending=true;
                        if(++pending_count>128)receiver_ok=false;
                    }
                    if(handoff_ready&&!notice_posted){notice_posted=true;notify=true;}
                }
            }
            const bool cleanup_flag=cleanup_observation.load()||(reliability_mode&&abort_retiring);
            const bool raw_acceptance_eligible=!cleanup_flag&&takeover_scope.load();
            ++raw_packets;
            record("raw_input",common+",\"input_code\":"+std::to_string(p.input_code)+",\"raw_flags\":"+std::to_string(p.flags)+",\"dx\":"+std::to_string(p.dx)+",\"dy\":"+std::to_string(p.dy)+",\"button_flags\":"+std::to_string(p.buttons)+",\"cursor_sampled\":"+flag(p.cursor_sampled)+",\"cursor_success\":"+flag(p.cursor_ok)+",\"cursor\":"+(p.cursor_sampled&&p.cursor_ok?point(p.cursor):"null")+",\"left_down\":"+flag(p.left_down)+",\"foreground_hwnd\":"+std::to_string(p.foreground)+",\"foreground_pid\":"+std::to_string(p.foreground_pid)+",\"device_handle_present\":"+flag(p.device_present)+",\"test_tag_matches\":"+flag(p.tag_matches)
                +(diagnostic_mode?",\"cleanup_observation\":"+flag(cleanup_flag)+",\"raw_scope\":\""+std::string(cleanup_flag?"cleanup":"gesture")+"\",\"acceptance_eligible\":"+flag(raw_acceptance_eligible)+",\"gesture_id\":"+std::to_string(active_gesture_id.load()):"")+(reliability_mode?",\"extra_information\":"+std::to_string(p.extra_information)+",\"cleanup_tag_matches\":"+flag(p.cleanup_tag_matches)+",\"ledger_observed_qpc\":"+std::to_string(ledger_observed_qpc):""));
            if(reliability_mode){std::lock_guard lock(down_ledger_mutex);if(down_ledger.facts.raw_down_matches&&down_ledger.facts.raw_down_receiver_sequence==p.serial)down_ledger.raw_down_recorded=log_ok;}
            record("raw_cursor_status",common+",\"cursor_error\":"+std::to_string(p.cursor_error));
            if(notify&&!post_source_work(raw_notice_message)){receiver_ok=false;takeover_healthy=false;record("receiver_error",",\"reason\":\"owner_notice_post_failed\"");}
            if(p.cursor_sampled){std::lock_guard lock(raw_mutex);last_motion=p;++raw_movements;SetEvent(raw_motion);}
            if(p.buttons&RI_MOUSE_LEFT_BUTTON_UP){std::lock_guard lock(raw_mutex);last_up=p;++raw_ups;SetEvent(raw_up);}
        }else if(p.kind==4){receiver_removed=p.removed;receiver_destroyed=p.destroyed;record("receiver_shutdown",common+",\"registration_removed\":"+flag(p.removed)+",\"window_destroyed\":"+flag(p.destroyed));}
        else {receiver_ok=false;record("receiver_error",common+",\"error\":"+std::to_string(p.error));}
    }
}
void start_receiver(){
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};HANDLE writer{};
    require(CreatePipe(&raw_pipe,&writer,&security,4096),"receiver_pipe_failed");
    require(SetHandleInformation(raw_pipe,HANDLE_FLAG_INHERIT,0),"receiver_pipe_failed");
    receiver_stop=CreateEventW(&security,TRUE,FALSE,nullptr);receiver_armed=CreateEventW(&security,TRUE,FALSE,nullptr);
    receiver_ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);raw_motion=CreateEventW(nullptr,TRUE,FALSE,nullptr);raw_up=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    require(receiver_stop&&receiver_armed&&receiver_ready&&raw_motion&&raw_up,"receiver_event_failed");
    wchar_t executable[32768]{};require(GetModuleFileNameW(nullptr,executable,32768)!=0,"receiver_path_failed");
    std::wstring command=L"\""+std::wstring(executable)+L"\" --raw-receiver "+std::to_wstring(reinterpret_cast<std::uintptr_t>(writer))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(receiver_stop))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(receiver_armed));
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    const bool created=CreateProcessW(executable,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&receiver_process)!=FALSE;CloseHandle(writer);
    require(created,"receiver_creation_failed");receiver_reader=std::thread(read_receiver);
    require(WaitForSingleObject(receiver_ready,3000)==WAIT_OBJECT_0&&receiver_ok,"receiver_registration_failed");
}
void stop_receiver(){
    if(receiver_stop)SetEvent(receiver_stop);
    if(receiver_process.hProcess){
        if(WaitForSingleObject(receiver_process.hProcess,4000)!=WAIT_OBJECT_0){
            receiver_ok=false;record("receiver_error",",\"reason\":\"test_child_shutdown_timeout\"");
            // Exact empty child created by this test only, never a user app.
            TerminateProcess(receiver_process.hProcess,2);WaitForSingleObject(receiver_process.hProcess,2000);
        }
        DWORD code{};if(!GetExitCodeProcess(receiver_process.hProcess,&code)||code!=0)receiver_ok=false;
    }
    // All inherited writers belong to the exact exited child. EOF drains its
    // final cleanup packet; canceling the reader here could discard evidence.
    if(receiver_reader.joinable())receiver_reader.join();
    for(HANDLE h:{raw_pipe,receiver_stop,receiver_armed,receiver_ready,raw_motion,raw_up,receiver_process.hThread,receiver_process.hProcess})if(h)CloseHandle(h);
}
void process_abort_quiescence(){
    std::uint64_t request{};const char* phase{};
    {std::lock_guard lock(abort_request_mutex);request=abort_request_sequence;phase=abort_request_phase;}
    const auto ack_qpc=qpc();
    DWORD actual_pid{};const auto actual_tid=GetWindowThreadProcessId(owned,&actual_pid);
    const bool nonce_available=actual_pid==GetCurrentProcessId()&&actual_tid==ui_thread;
    const auto actual_nonce=nonce_available?static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA)):0;
    const bool exact=owned&&nonce_available&&actual_nonce==run_nonce;
    // Owner dispatch is serialized after any prior owner operation. A reentrant
    // handler cannot acquire a mutex held by an in-flight SetWindowPos.
    bool acknowledged=GetCurrentThreadId()==ui_thread&&exact&&!stop_requested&&!source_retired&&abort_retiring&&active_operation==0&&log_ok;
    if(acknowledged){
        std::lock_guard lock(continuation_mutex);takeover_scope=false;handoff_ready=false;
        motion_pending=false;pending_count=0;pending_first=0;notice_posted=false;audit_live=false;
    }
    const auto row=record("abort_writer_quiescence",",\"phase\":\"owner_ack\",\"cleanup_phase\":\""+std::string(phase)+"\",\"request_sequence\":"+std::to_string(request)+",\"ack_tid\":"+std::to_string(GetCurrentThreadId())+",\"ack_qpc\":"+std::to_string(ack_qpc)
        +",\"active_operation_id\":"+std::to_string(active_operation)+",\"pending_raw_count\":"+std::to_string(pending_count)+",\"motion_pending\":"+flag(motion_pending)+",\"notice_posted\":"+flag(notice_posted)+",\"acceptance_retired\":"+flag(abort_retiring&&!takeover_scope)+",\"takeover_scope\":"+flag(takeover_scope)+",\"native_calls\":"+std::to_string(takeover_native_calls.load())+",\"acknowledged\":"+flag(acknowledged)
        +",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+std::to_string(actual_pid)+",\"actual_source_tid\":"+std::to_string(actual_tid)+",\"actual_source_nonce\":"+(nonce_available?std::to_string(actual_nonce):"null")+",\"own_identity\":"+flag(exact));
    abort_quiescent=acknowledged&&row&&log_ok;abort_quiescence_sequence=row;SetEvent(abort_quiescence_finished);
}
bool request_abort_quiescence(const char* phase){
    abort_quiescent=false;abort_quiescence_sequence=0;ResetEvent(abort_quiescence_finished);std::uint64_t request{};bool posted{};
    {
        // No event wait while holding this short publication lock. It prevents
        // an early owner ACK from inventing a future request reference.
        std::lock_guard lock(abort_request_mutex);abort_request_phase=phase;const auto requested=qpc();
        posted=post_source_work(abort_quiescence_message);
        request=record("abort_quiescence_request",",\"phase\":\""+std::string(phase)+"\",\"message\":"+std::to_string(abort_quiescence_message)+",\"posted\":"+flag(posted)+",\"request_qpc\":"+std::to_string(requested)+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce));
        abort_request_sequence=request;
    }
    const auto wait=posted&&request&&log_ok?WaitForSingleObject(abort_quiescence_finished,2000):WAIT_FAILED;
    record("abort_quiescence_wait",",\"phase\":\""+std::string(phase)+"\",\"request_sequence\":"+std::to_string(request)+",\"ack_sequence\":"+std::to_string(abort_quiescence_sequence.load())+",\"timeout_ms\":2000,\"wait_result\":"+std::to_string(wait)+",\"quiescent\":"+flag(wait==WAIT_OBJECT_0&&abort_quiescent));
    return wait==WAIT_OBJECT_0&&abort_quiescent&&log_ok;
}
void wait_raw_motion(POINT expected,const InputReceipt& receipt){
    record("raw_wait_begin",",\"expected_cursor\":"+point(expected)+",\"input_start_qpc\":"+std::to_string(receipt.start)+",\"timeout_ms\":2000");
    const auto wait=WaitForSingleObject(raw_motion,2000);
    record("raw_wait_result",",\"wait_result\":"+std::to_string(wait)+",\"raw_packets\":"+std::to_string(raw_packets.load())+",\"movement_count\":"+std::to_string(raw_movements.load()));
    require(wait==WAIT_OBJECT_0,"BLOCKED_BY_RAW_INPUT_DELIVERY");
    std::lock_guard lock(raw_mutex);
    // The observed first packet preceded cursor/legacy update. Correlate test
    // delivery using the actual absolute INPUT, not a historical cursor claim.
    const bool delivered=receiver_ok&&last_motion.cursor_ok&&last_motion.input_code==RIM_INPUTSINK&&last_motion.serial>receipt.watermark&&last_motion.observed_qpc>=receipt.start&&last_motion.tag_matches&&(last_motion.flags&(MOUSE_MOVE_ABSOLUTE|MOUSE_VIRTUAL_DESKTOP))==(MOUSE_MOVE_ABSOLUTE|MOUSE_VIRTUAL_DESKTOP)&&last_motion.dx==receipt.input.mi.dx&&last_motion.dy==receipt.input.mi.dy;
    record("raw_motion_correlation",",\"receiver_sequence\":"+std::to_string(last_motion.serial)+",\"delivered\":"+flag(delivered)+",\"snapshot_cursor_matches\":"+flag(last_motion.cursor_ok&&std::abs(last_motion.cursor.x-expected.x)<=1&&std::abs(last_motion.cursor.y-expected.y)<=1));
    require(delivered,"BLOCKED_BY_RAW_INPUT_CORRELATION");
}
void wait_raw_up(const InputReceipt& receipt){
    require(WaitForSingleObject(raw_up,2000)==WAIT_OBJECT_0,"BLOCKED_BY_RAW_BUTTON_UP");
    std::lock_guard lock(raw_mutex);
    require(receiver_ok&&last_up.input_code==RIM_INPUTSINK&&last_up.serial>receipt.watermark&&last_up.observed_qpc>=receipt.start&&last_up.tag_matches&&(last_up.buttons&RI_MOUSE_LEFT_BUTTON_UP),"BLOCKED_BY_RAW_BUTTON_UP");
}
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l){
    if(reliability_mode&&message==abort_quiescence_message){process_abort_quiescence();return 0;}
    if(reliability_mode&&abort_retiring&&(message==handoff_message||message==raw_notice_message||message==native_preflight_message||message==final_acceptance_message)){
        record("abort_owner_work_skipped",",\"message\":"+std::to_string(message)+",\"acceptance_retired\":true");
        if(message==handoff_message)SetEvent(handoff_finished);
        if(message==raw_notice_message){SetEvent(quantum_finished);SetEvent(takeover_finished);}
        if(message==native_preflight_message)SetEvent(native_preflight_finished);
        if(message==final_acceptance_message)SetEvent(final_acceptance_finished);
        return 0;
    }
    if(isolation_mode&&message==final_acceptance_message){
        try{process_final_acceptance();}catch(const std::exception& e){takeover_healthy=false;record("takeover_failure",",\"reason\":\""+std::string(e.what())+"\",\"phase\":\"final_acceptance\"");SetEvent(final_acceptance_finished);}return 0;
    }
    if(diagnostic_mode&&(message==winevent_notice_message||message==native_preflight_message)){
        try{if(message==winevent_notice_message)process_winevents();else process_native_preflight();}
        catch(const std::exception& e){takeover_healthy=false;audit_live=false;
            record("takeover_failure",",\"reason\":\""+std::string(e.what())+"\",\"phase\":\"diagnostic_owner\"");
            SetEvent(native_preflight_finished);SetEvent(winevent_finished);
        }
        return 0;
    }
    if(end_mode&&(message==handoff_message||message==raw_notice_message)){
        try{if(message==handoff_message)process_handoff();else process_raw_notice();}
        catch(const std::exception& e){
            takeover_healthy=false;audit_live=false;
            probe_failure=message==handoff_message?"handoff_owner_failure":"raw_owner_failure";
            record("takeover_failure",",\"reason\":\""+std::string(e.what())+"\",\"phase\":\""+(message==handoff_message?"handoff":"raw_quantum")+"\"");
            SetEvent(handoff_finished);SetEvent(quantum_finished);SetEvent(takeover_finished);
        }
        return 0;
    }
    if(h==owned&&message==WM_ACTIVATE&&LOWORD(w)!=WA_INACTIVE){
        const bool armed=activation_armed;
        if(armed){activate_seen=true;if(activation_event)SetEvent(activation_event);}else direct_activate_seen=true;
        record("activation_event",",\"message\":\"WM_ACTIVATE\",\"activation_code\":"+std::to_string(LOWORD(w))+",\"activation_epoch\":"+std::to_string(armed?1:0)+",\"target\":"+std::to_string(number(h))+",\"foreground_matches\":"+flag(GetForegroundWindow()==h));
    }
    if(h==owned&&message==WM_SETFOCUS){
        const bool armed=activation_armed;
        if(armed){focus_seen=true;if(activation_event)SetEvent(activation_event);}else direct_focus_seen=true;
        record("activation_event",",\"message\":\"WM_SETFOCUS\",\"activation_epoch\":"+std::to_string(armed?1:0)+",\"target\":"+std::to_string(number(h))+",\"foreground_matches\":"+flag(GetForegroundWindow()==h));
    }
    if(h==owned&&message==WM_MOUSEMOVE){
        if(activation_armed)SetEvent(pointer_arrived);
        else {POINT location{static_cast<SHORT>(LOWORD(l)),static_cast<SHORT>(HIWORD(l))};const bool converted=ClientToScreen(h,&location)!=FALSE;record("LEGACY_MOVE",",\"screen_point\":"+point(location)+",\"coordinate_valid\":"+flag(converted));}
    }
    if(h==owned&&activation_armed&&(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP)){
        record("activation_button",",\"message\":\""+std::string(message==WM_LBUTTONDOWN?"WM_LBUTTONDOWN":"WM_LBUTTONUP")+"\",\"target\":"+std::to_string(number(h)));
        SetEvent(message==WM_LBUTTONDOWN?activation_down_received:activation_up_received);
    }
    if(h==owned&&message==WM_NCLBUTTONDOWN&&(w==HTCAPTION||w==HTBOTTOM)){
        const auto down_receipt=qpc();if(reliability_mode){++native_down_count;native_down_qpc=down_receipt;}
        POINT down_cursor{};const bool observed=GetCursorPos(&down_cursor)!=FALSE;
        if(diagnostic_mode&&native_down_sequence){takeover_healthy=false;record("native_button_down",",\"target\":"+std::to_string(number(h))+",\"hit_test\":"+std::to_string(w)+",\"cursor\":"+point(down_cursor)+",\"cursor_success\":"+flag(observed)+",\"duplicate\":true");return DefWindowProcW(h,message,w,l);}
        if(end_mode){std::lock_guard lock(continuation_mutex);intent_pointer=down_cursor;if(!observed)takeover_healthy=false;}
        native_down_sequence=record("native_button_down",",\"target\":"+std::to_string(number(h))+",\"hit_test\":"+std::to_string(w)+(end_mode?",\"cursor\":"+point(down_cursor)+",\"cursor_success\":"+flag(observed):"")+(reliability_mode?",\"receipt_qpc\":"+std::to_string(down_receipt):""));SetEvent(nonclient_down);
    }
    if(message==WM_ENTERSIZEMOVE){
        if(reliability_mode)++native_enter_count;
        active=true;const auto receipt=qpc();const auto g=capture();
        if(diagnostic_mode&&native_enter_qpc){takeover_healthy=false;record("ENTER",",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(g)+",\"receipt_qpc\":"+std::to_string(receipt)+",\"duplicate\":true");SetEvent(entered);return 0;}
        native_enter_qpc=receipt;
        native_enter_sequence=record("ENTER",",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(g)+(end_mode?",\"receipt_qpc\":"+std::to_string(receipt):""));
        if(end_mode){
            POINT down_cursor{};{std::lock_guard lock(continuation_mutex);intent_start=g;down_cursor=intent_pointer;}
            record("intent_anchor",",\"native_enter_sequence\":"+std::to_string(native_enter_sequence.load())+",\"native_enter_qpc\":"+std::to_string(receipt)+",\"native_down_sequence\":"+std::to_string(native_down_sequence.load())+",\"pointer_down\":"+point(down_cursor)+",\"start_positioning\":"+rect(g.p)+",\"start_visible\":"+rect(g.v)+",\"operation\":\""+(selected_gesture==1?"Move":"BottomResize")+"\"");
        }
        SetEvent(entered);return 0;
    }
    if(message==WM_MOVING||message==WM_SIZING){
        const auto receipt=qpc();
        if(cancel_return_boundary&&receipt>cancel_return_boundary)++native_drag_after_return;
        if(end_mode&&native_end_qpc){++native_drag_after_end;takeover_healthy=false;}
        if(diagnostic_mode){
            if(drag_receipt_count==drag_receipts.size()){takeover_healthy=false;winevent_healthy=false;}
            else drag_receipts[drag_receipt_count++]=receipt;
            if(winevent_end_qpc&&receipt>winevent_end_qpc){++native_drag_after_winevent_end;takeover_healthy=false;}
        }
        POINT cursor{};GetCursorPos(&cursor);
        record("DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"cursor\":"+point(cursor)+",\"proposed\":"+rect(*reinterpret_cast<RECT*>(l))+",\"after_cancel\":"+flag(cancelled)+geometry(capture())+(diagnostic_mode?",\"receipt_qpc\":"+std::to_string(receipt):""));
        ++callbacks;SetEvent(stepped);return DefWindowProcW(h,message,w,l);
    }
    if(message==WM_CANCELMODE)record("cancel_message",",\"target\":"+std::to_string(number(h))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(capture()));
    if(message==WM_CAPTURECHANGED){
        const auto next=reinterpret_cast<HWND>(l);if(next&&!input_root_owned(next))foreign_capture_transferred=true;
        record("CAPTURE_CHANGED",",\"new_capture\":"+std::to_string(number(next))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(capture()));
    }
    if(message==WM_WINDOWPOSCHANGED&&gesture>0){
        const auto receipt=qpc();const auto g=capture();bool unexpected=false;
        if(diagnostic_mode&&takeover_scope&&active_operation==0){
            if(position_receipt_count==position_receipts.size()){takeover_healthy=false;winevent_healthy=false;}
            else position_receipts[position_receipt_count++]={receipt,g};
        }
        if(end_mode&&audit_live){
            if(diagnostic_mode&&(!g.p_ok||!g.v_ok))takeover_healthy=false;
            else unexpected=!g.p_ok||!g.v_ok||(active_operation?!equal(g.p,inflight_target.p):(!equal(g.p,expected_live.p)||!equal(g.v,expected_live.v)));
            if(unexpected){++unowned_geometry_changes;takeover_healthy=false;}
        }
        record("POSITION_CHANGED",",\"after_cancel\":"+flag(cancelled)+geometry(g)+(end_mode?",\"operation_id\":"+std::to_string(active_operation)+",\"after_end\":"+flag(native_end_qpc!=0)+",\"unexpected_change\":"+flag(unexpected):"")
            +(diagnostic_mode?",\"position_receipt_qpc\":"+std::to_string(receipt)+",\"winevent_audit_active\":"+flag(winevent_audit_started&&audit_live)+",\"cleanup_observation\":"+flag(cleanup_observation):""));
    }
    if(message==WM_EXITSIZEMOVE){
        const auto receipt=qpc();const auto g=capture();
        if(diagnostic_mode&&native_end_qpc){takeover_healthy=false;record("EXIT",",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(g)+",\"receipt_qpc\":"+std::to_string(receipt)+",\"duplicate\":true");SetEvent(exited);return 0;}
        native_end_qpc=receipt;
        if(end_mode){std::lock_guard lock(continuation_mutex);terminal_geometry=g;expected_live=g;audit_live=!diagnostic_mode;}
        native_end_sequence=record("EXIT",",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(g)+(end_mode?",\"receipt_qpc\":"+std::to_string(receipt):""));
        active=false;SetEvent(exited);return 0;
    }
    if(message==finish_message){if(end_mode){audit_live=false;takeover_scope=false;}retire_source_fixture();if(isolation_mode){try{destroy_input_shield("failure",0,0);}catch(...){shield_healthy=false;}}if(diagnostic_mode)remove_movesize_hook();if(!isolation_mode||(h==owned&&identity()))DestroyWindow(h);return 0;}
    if(message==WM_DESTROY){retire_source_fixture();PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,message,w,l);
}
void fence(POINT expected,bool down,bool priming=false,bool post_cancel=false){
    if(reliability_mode){
        using P=ab::FencePredicate;using S=ab::PredicateState;
        ab::FenceFailureSnapshot snapshot;
        const auto state=[](bool v){return v?S::Pass:S::Fail;};
        const auto started=qpc();const bool running=!stop_requested,healthy=log_ok;
        snapshot.set(P::OwnerRunning,state(running));snapshot.set(P::LogHealthy,state(healthy));
        struct Timing{bool evaluated{};std::int64_t start{},finish{};};
        std::array<Timing,7> queries{};
        const auto begin=[&](std::size_t i){queries[i].evaluated=true;queries[i].start=qpc();};
        const auto finish=[&](std::size_t i){queries[i].finish=qpc();};
        bool desktop=false,own=false,fg_matches=false,cursor_ok=false,queried=false,buttons_evaluated=false,nonce_evaluated=false;
        DWORD actual_pid{},actual_tid{},fg_pid{},fg_tid{},cursor_error{},gui_error{};std::uintptr_t actual_nonce{};
        HWND foreground{},root{};POINT actual{};GUITHREADINFO gui{sizeof(gui)};
        bool left=false;std::array<bool,10> others{};
        const bool foreign_clear=!foreign_capture_transferred;
        snapshot.set(P::NoForeignCaptureTransfer,state(foreign_clear));
        if(post_cancel||gesture==0){snapshot.set(P::CancelledGuiSafe,S::NotEvaluated);if(down)snapshot.set(P::InputRootOwned,S::NotEvaluated);}
        else if(down)snapshot.set(P::Capture,S::NotEvaluated);
        if(running&&healthy){
            begin(0);desktop=desktop_available();finish(0);snapshot.set(P::DesktopReady,state(desktop));
            if(desktop){
                begin(1);actual_tid=GetWindowThreadProcessId(owned,&actual_pid);
                if(actual_pid==GetCurrentProcessId()&&actual_tid==ui_thread){nonce_evaluated=true;actual_nonce=static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA));}
                own=owned&&actual_pid==GetCurrentProcessId()&&actual_tid==ui_thread&&actual_nonce==run_nonce;finish(1);snapshot.set(P::SourceIdentity,state(own));
                begin(2);foreground=GetForegroundWindow();fg_tid=GetWindowThreadProcessId(foreground,&fg_pid);fg_matches=foreground==owned;finish(2);snapshot.set(P::Foreground,state(fg_matches));
                // Only a reliable own foreground/desktop permits button-state
                // interpretation. Later safe reads diagnose compound failures.
                if(own&&fg_matches){
                    begin(3);SetLastError(0);cursor_ok=GetCursorPos(&actual)!=FALSE;cursor_error=cursor_ok?0:GetLastError();finish(3);
                    snapshot.set(P::CursorQuery,state(cursor_ok));snapshot.set(P::CursorPosition,cursor_ok?state(std::abs(actual.x-expected.x)<=1&&std::abs(actual.y-expected.y)<=1):S::Unknown);
                    begin(4);left=pressed(VK_LBUTTON);constexpr int keys[]={VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_ESCAPE};
                    for(std::size_t i=0;i<others.size();++i)others[i]=pressed(keys[i]);buttons_evaluated=true;finish(4);
                    snapshot.set(P::LeftState,state(left==down));snapshot.set(P::OtherInputClear,state(std::none_of(others.begin(),others.end(),[](bool v){return v;})));
                    begin(5);SetLastError(0);queried=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;gui_error=queried?0:GetLastError();finish(5);snapshot.set(P::GuiQuery,state(queried));
                    if(cursor_ok){begin(6);root=GetAncestor(WindowFromPoint(actual),GA_ROOT);finish(6);}
                    if(post_cancel||gesture==0){
                        snapshot.set(P::CancelledGuiSafe,queried?state(cancellation_gui_safe(gui)):S::Unknown);
                        if(down)snapshot.set(P::InputRootOwned,cursor_ok?state(input_root_owned(root)):S::Unknown);
                    }else if(down){snapshot.set(P::Capture,queried&&cursor_ok?state(gui.hwndCapture==owned||(priming&&!gui.hwndCapture&&root==owned)):S::Unknown);}
                }
            }
        }
        const auto result=ab::classify_fence(snapshot);
        RawPacket motion,up;{std::lock_guard lock(raw_mutex);motion=last_motion;up=last_up;}
        const auto finished=qpc();const auto nullable=[](bool known,bool value){return known?flag(value):"null";};
        const auto handle=[](bool known,HWND h){return known?std::to_string(number(h)):"null";};
        std::string times="{",predicates="{",required="[",failed="[",other="{";
        constexpr const char* qnames[]={"desktop","identity","foreground","cursor","buttons","gui","root"};
        constexpr const char* onames[]={"ctrl","shift","alt","lwin","rwin","rbutton","mbutton","xbutton1","xbutton2","escape"};
        for(std::size_t i=0;i<queries.size();++i){if(i)times+=",";const auto& t=queries[i];times+="\""+std::string(qnames[i])+"\":{\"evaluated\":"+flag(t.evaluated)+",\"start_qpc\":"+(t.evaluated?std::to_string(t.start):"null")+",\"finish_qpc\":"+(t.evaluated?std::to_string(t.finish):"null")+"}";}times+="}";
        bool first=true;for(std::size_t i=0;i<snapshot.count;++i){if(i)predicates+=",";const auto name=std::string(ab::predicate_name(static_cast<P>(i)));predicates+="\""+name+"\":\""+std::string(ab::state_name(snapshot.predicates[i]))+"\"";if(snapshot.required[i]){if(!first)required+=",";required+="\""+name+"\"";first=false;}}predicates+="}";required+="]";
        for(std::size_t i=0;i<result.failed_count;++i){if(i)failed+=",";failed+="\""+std::string(ab::predicate_name(result.failed[i]))+"\"";}failed+="]";
        for(std::size_t i=0;i<others.size();++i){if(i)other+=",";other+="\""+std::string(onames[i])+"\":"+nullable(buttons_evaluated,others[i]);}other+="}";
        auto reason=ab::fence_reason(result.first);if(result.first==P::InputRootOwned&&winevent_end_qpc)reason="TestInputIsolationUnavailable";
        const auto diagnostic=record("input_fence_diagnostic",",\"phase\":\""+std::string(driver_phase)+"\",\"sample\":"+std::to_string(driver_sample)+",\"operation\":\""+(selected_gesture==1?"Move":"BottomResize")+"\",\"expected_cursor\":"+point(expected)+",\"expected_left_down\":"+flag(down)
            +",\"query_start_qpc\":"+std::to_string(started)+",\"query_finish_qpc\":"+std::to_string(finished)+",\"queries\":"+times+",\"owner_running\":"+flag(running)+",\"log_healthy\":"+flag(healthy)+",\"desktop_ready\":"+nullable(queries[0].evaluated,desktop)+",\"source_identity\":"+nullable(queries[1].evaluated,own)+",\"foreign_capture_clear\":"+flag(foreign_clear)
            +",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+(queries[1].evaluated?std::to_string(actual_pid):"null")+",\"actual_source_tid\":"+(queries[1].evaluated?std::to_string(actual_tid):"null")+",\"actual_source_nonce\":"+(nonce_evaluated?std::to_string(actual_nonce):"null")
            +",\"foreground_hwnd\":"+handle(queries[2].evaluated,foreground)+",\"foreground_pid\":"+(queries[2].evaluated?std::to_string(fg_pid):"null")+",\"foreground_tid\":"+(queries[2].evaluated?std::to_string(fg_tid):"null")+",\"cursor_query_succeeded\":"+nullable(queries[3].evaluated,cursor_ok)+",\"cursor_error\":"+(queries[3].evaluated?std::to_string(cursor_error):"null")+",\"actual_cursor\":"+(cursor_ok?point(actual):"null")+",\"left_down\":"+nullable(buttons_evaluated,left)+",\"other_states\":"+other
            +",\"gui_query_succeeded\":"+nullable(queries[5].evaluated,queried)+",\"gui_error\":"+(queries[5].evaluated?std::to_string(gui_error):"null")+",\"capture_hwnd\":"+handle(queried,gui.hwndCapture)+",\"menu_owner_hwnd\":"+handle(queried,gui.hwndMenuOwner)+",\"move_size_hwnd\":"+handle(queried,gui.hwndMoveSize)+",\"gui_flags\":"+(queried?std::to_string(gui.flags):"null")+",\"root\":"+handle(cursor_ok,root)
            +",\"priming\":"+flag(priming)+",\"post_cancel\":"+flag(post_cancel)+",\"cancel_pending\":"+flag(cancel_pending)+",\"receiver_watermark\":"+std::to_string(receiver_watermark.load())+",\"raw_movement_receiver_sequence\":"+std::to_string(motion.serial)+",\"raw_movement_receiver_qpc\":"+std::to_string(motion.observed_qpc)+",\"raw_up_receiver_sequence\":"+std::to_string(up.serial)+",\"raw_up_receiver_qpc\":"+std::to_string(up.observed_qpc)
            +",\"predicates\":"+predicates+",\"required_predicates\":"+required+",\"failed_predicates\":"+failed+",\"first_failed_predicate\":\""+std::string(ab::predicate_name(result.first))+"\",\"passed\":"+flag(result.passed)+",\"failure_reason\":\""+std::string(reason)+"\",\"failure_subreason\":\""+std::string(ab::fence_subreason(result.first))+"\"");
        require(diagnostic&&log_ok,"evidence_capture_failed");require(result.passed,reason.data());
        record("input_fence",",\"cursor\":"+point(actual)+",\"expected_cursor\":"+point(expected)+",\"foreground\":"+std::to_string(number(foreground))+",\"target\":"+std::to_string(number(owned))+",\"left_down\":"+flag(down)+",\"cancel_pending\":"+flag(cancel_pending)+",\"post_cancel\":"+flag(post_cancel)+",\"gui_flags\":"+std::to_string(gui.flags)+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"priming\":"+flag(priming)+",\"gui_query_succeeded\":"+flag(queried)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"window_from_point_root\":"+std::to_string(number(root))+",\"diagnostic_sequence\":"+std::to_string(diagnostic));
        return;
    }
    require(!stop_requested,"owner_message_wait_failed");
    require(log_ok,"evidence_capture_failed");require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    require(!foreign_capture_transferred,"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
    POINT actual{};require(GetCursorPos(&actual)!=FALSE,"BLOCKED_BY_INPUT_INTERFERENCE");
    require(std::abs(actual.x-expected.x)<=1&&std::abs(actual.y-expected.y)<=1&&pressed(VK_LBUTTON)==down&&!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    GUITHREADINFO gui{sizeof(gui)};const bool queried=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const HWND root=GetAncestor(WindowFromPoint(actual),GA_ROOT);
    require(queried,"BLOCKED_BY_GUI_STATE");
    if(post_cancel||gesture==0){
        require(cancellation_gui_safe(gui),"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
        if(down)require(input_root_owned(root),isolation_mode&&winevent_end_qpc?"TestInputIsolationUnavailable":"BLOCKED_BY_INPUT_HIT_AUTHORITY");
    }else if(down){require(gui.hwndCapture==owned||(priming&&!gui.hwndCapture&&root==owned),"BLOCKED_BY_INPUT_INTERFERENCE");}
    record("input_fence",",\"cursor\":"+point(actual)+",\"expected_cursor\":"+point(expected)+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"target\":"+std::to_string(number(owned))+",\"left_down\":"+flag(down)+",\"cancel_pending\":"+flag(cancel_pending)+",\"post_cancel\":"+flag(post_cancel)+",\"gui_flags\":"+std::to_string(gui.flags)+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"priming\":"+flag(priming)+",\"gui_query_succeeded\":"+flag(queried)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"window_from_point_root\":"+std::to_string(number(root)));
}
INPUT mouse_input(DWORD flags,POINT p){
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;input.mi.dwExtraInfo=input_tag;
    if(flags&MOUSEEVENTF_MOVE){
        const auto x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        require(width>1&&height>1&&p.x>=x&&p.y>=y&&p.x<x+width&&p.y<y+height,"BLOCKED_BY_INTERACTIVE_DESKTOP");
        require(x==virtual_area.left&&y==virtual_area.top&&x+width==virtual_area.right&&y+height==virtual_area.bottom,"BLOCKED_BY_DISPLAY_CONTEXT_CHANGE");
        input.mi.dwFlags|=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
        // Address the pixel cell centre rather than its normalized boundary.
        input.mi.dx=static_cast<LONG>((2LL*(p.x-x)+1)*65536/(2LL*width));
        input.mi.dy=static_cast<LONG>((2LL*(p.y-y)+1)*65536/(2LL*height));
    }
    return input;
}
InputReceipt inject(DWORD flags,POINT p={},bool restoring_cursor=false){
    require(!stop_requested,"owner_message_wait_failed");
    // Recheck immediately at the API boundary as well as at the recorded
    // path fence. The activation exception lives in its separate helper.
    require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&IsWindowVisible(owned)&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    require(!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    if(restoring_cursor)require(flags==MOUSEEVENTF_MOVE&&!pressed(VK_LBUTTON)&&p.x==saved_cursor.x&&p.y==saved_cursor.y,"BLOCKED_BY_INPUT_INTERFERENCE");
    require(!foreign_capture_transferred,"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
    if(flags==MOUSEEVENTF_LEFTDOWN||flags==MOUSEEVENTF_LEFTUP){
        // Buttons carry no mouse coordinates. Reconcile the actual current
        // cursor against the driver's last planned point at this API boundary.
        fence(p,flags==MOUSEEVENTF_LEFTUP,false,gesture==0||cancelled);
        require(input_root_owned(GetAncestor(WindowFromPoint(p),GA_ROOT)),isolation_mode&&winevent_end_qpc?"TestInputIsolationUnavailable":"BLOCKED_BY_INPUT_HIT_AUTHORITY");
        GUITHREADINFO gui{sizeof(gui)};
        require(GetGUIThreadInfo(ui_thread,&gui)&&cancellation_gui_safe(gui),"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
    }
    if(!restoring_cursor&&(gesture==0||cancelled)&&(flags&MOUSEEVENTF_MOVE)){
        const HWND root=GetAncestor(WindowFromPoint(p),GA_ROOT);
        require(input_root_owned(root),isolation_mode&&winevent_end_qpc?"TestInputIsolationUnavailable":"BLOCKED_BY_INPUT_HIT_AUTHORITY");
        GUITHREADINFO gui{sizeof(gui)};
        require(GetGUIThreadInfo(ui_thread,&gui)&&cancellation_gui_safe(gui),"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
        record("destination_fence",",\"point\":"+point(p)+",\"root\":"+std::to_string(number(root))+",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"capture_hwnd\":0,\"menu_owner_hwnd\":0,\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"gui_flags\":"+std::to_string(gui.flags)+",\"cancel_pending\":"+flag(cancel_pending)+",\"left_down\":"+flag(pressed(VK_LBUTTON)));
    }
    auto input=mouse_input(flags,p);
    if(isolation_mode&&winevent_end_qpc){
        require(isolation_ready_sequence&&isolation_ready_qpc&&!restoring_cursor,"TestInputIsolationUnavailable");
        require(fresh_synthetic_input_isolation(p,flags,"gesture"),"TestInputIsolationUnavailable");
    }
    const auto watermark=receiver_watermark.load();const auto intent=begin_ledger_intent(flags,input_tag,watermark);const auto start=qpc();ledger_api_started(intent,start);SetLastError(0);const UINT sent=SendInput(1,&input,sizeof(input));const DWORD error=sent==1?0:GetLastError();const auto returned=qpc();
    const auto input_sequence=record("input",",\"flags\":"+std::to_string(flags)+",\"point\":"+point(p)+",\"restoring_cursor\":"+flag(restoring_cursor)+",\"actual_mouse_flags\":"+std::to_string(input.mi.dwFlags)+",\"normalized_dx\":"+std::to_string(input.mi.dx)+",\"normalized_dy\":"+std::to_string(input.mi.dy)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned)+",\"virtual_screen\":"+rect(virtual_area)+(reliability_mode?",\"ledger_intent_sequence\":"+std::to_string(intent):""));
    ledger_api_committed(intent,input_sequence,sent,start,returned);
    require(sent==1,"BLOCKED_BY_SENDINPUT");
    return {input,start,watermark};
}
LRESULT hit_test(POINT p){
    require(p.x>=-32768&&p.x<=32767&&p.y>=-32768&&p.y<=32767,"BLOCKED_BY_HIT_TEST");
    DWORD_PTR result{};
    require(SendMessageTimeoutW(owned,WM_NCHITTEST,0,MAKELPARAM(static_cast<SHORT>(p.x),static_cast<SHORT>(p.y)),SMTO_ABORTIFHUNG,500,&result)!=0,"BLOCKED_BY_HIT_TEST");
    return static_cast<LRESULT>(result);
}
fg::ActivationProof activation_proof(std::string_view phase,bool held){
    fg::ActivationProof proof;
    proof.own_identity=identity();proof.same_integrity=proof.own_identity;
    proof.desktop_ready=desktop_available();proof.visible=proof.own_identity&&IsWindowVisible(owned);
    proof.temporary_topmost=proof.own_identity&&(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    const HWND root=GetAncestor(WindowFromPoint(activation_point),GA_ROOT);bootstrap.root=root;
    proof.point_root_matches=root==owned&&proof.own_identity;
    bootstrap.hit=proof.own_identity?hit_test(activation_point):HTERROR;proof.client_hit=bootstrap.hit==HTCLIENT;
    const auto foreground_before_qpc=qpc();const HWND foreground=GetForegroundWindow();DWORD foreground_pid{};const DWORD tid=GetWindowThreadProcessId(foreground,&foreground_pid);
    GUITHREADINFO gui{sizeof(gui)};bool query=false;std::int64_t gui_start{},gui_finish{};DWORD gui_error{};
    if(tid){gui_start=qpc();SetLastError(0);query=GetGUIThreadInfo(tid,&gui)!=FALSE;gui_error=query?0:GetLastError();gui_finish=qpc();}
    const HWND foreground_after=foreground?GetForegroundWindow():nullptr;
    const bool stable=foreground&&foreground_after==foreground;
    DWORD foreground_after_pid{};const DWORD foreground_after_tid=foreground_after?GetWindowThreadProcessId(foreground_after,&foreground_after_pid):0;const auto foreground_after_qpc=qpc();
    const bool tuple_stable=stable&&tid!=0&&foreground_pid!=0&&foreground_after_pid==foreground_pid&&foreground_after_tid==tid;
    constexpr DWORD modal_flags=GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE;
    proof.foreign_capture_clear=query&&stable&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&modal_flags);
    // The existing authorization above is unchanged. Tuple/time/error fields
    // describe this ordered query; they do not claim a desktop-wide snapshot.
    const char* gui_subreason=!query?"GUI_QUERY_FAILED":(!tuple_stable?"FOREGROUND_CHANGED":(gui.hwndCapture?"CAPTURE_NONZERO":(gui.hwndMenuOwner?"MENU_ACTIVE":(gui.hwndMoveSize?"MOVE_SIZE_ACTIVE":((gui.flags&modal_flags)?"DISALLOWED_GUI_FLAGS":"none")))));
    proof.modifiers_clear=!pressed(VK_CONTROL)&&!pressed(VK_SHIFT)&&!pressed(VK_MENU)&&!pressed(VK_LWIN)&&!pressed(VK_RWIN)&&!pressed(VK_ESCAPE);
    proof.button_state_matches=pressed(VK_LBUTTON)==held&&!pressed(VK_RBUTTON)&&!pressed(VK_MBUTTON)&&!pressed(VK_XBUTTON1)&&!pressed(VK_XBUTTON2);
    POINT cursor{};const bool cursor_ok=GetCursorPos(&cursor)!=FALSE;
    if(phase!="move"&&(!cursor_ok||std::abs(cursor.x-activation_point.x)>1||std::abs(cursor.y-activation_point.y)>1))proof.button_state_matches=false;
    bootstrap.last_proof=proof;
    record("activation_fence",",\"phase\":\""+std::string(phase)+"\",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(foreground))+",\"cursor\":"+point(cursor)+",\"activation_point\":"+point(activation_point)
        +",\"own_identity\":"+flag(proof.own_identity)+",\"desktop_ready\":"+flag(proof.desktop_ready)+",\"same_integrity\":"+flag(proof.same_integrity)+",\"visible\":"+flag(proof.visible)+",\"temporary_topmost\":"+flag(proof.temporary_topmost)
        +",\"window_from_point_root\":"+std::to_string(number(root))+",\"window_from_point_root_matches\":"+flag(proof.point_root_matches)+",\"hit_test\":"+std::to_string(bootstrap.hit)
        +",\"gui_query_succeeded\":"+flag(query)+",\"foreground_snapshot_stable\":"+flag(stable)+",\"gui_flags\":"+(query?std::to_string(gui.flags):"null")+",\"capture_hwnd\":"+(query?std::to_string(number(gui.hwndCapture)):"null")+",\"menu_owner_hwnd\":"+(query?std::to_string(number(gui.hwndMenuOwner)):"null")+",\"move_size_hwnd\":"+(query?std::to_string(number(gui.hwndMoveSize)):"null")
        +",\"activation_diagnostic_contract\":\"foreground_gui_activation_v1\",\"foreground_pid\":"+(tid&&foreground_pid?std::to_string(foreground_pid):"null")+",\"foreground_tid\":"+(tid?std::to_string(tid):"null")+",\"foreground_query_before_qpc\":"+std::to_string(foreground_before_qpc)+",\"foreground_query_after_qpc\":"+(foreground?std::to_string(foreground_after_qpc):"null")
        +",\"foreground_after\":"+(foreground?std::to_string(number(foreground_after)):"null")+",\"foreground_after_pid\":"+(foreground_after_tid&&foreground_after_pid?std::to_string(foreground_after_pid):"null")+",\"foreground_after_tid\":"+(foreground_after_tid?std::to_string(foreground_after_tid):"null")+",\"foreground_tuple_stable\":"+flag(tuple_stable)
        +",\"gui_query_tid\":"+(tid?std::to_string(tid):"null")+",\"gui_query_attempted\":"+flag(tid!=0)+",\"gui_query_start_qpc\":"+(tid?std::to_string(gui_start):"null")+",\"gui_query_finish_qpc\":"+(tid?std::to_string(gui_finish):"null")+",\"gui_query_error\":"+(tid?std::to_string(gui_error):"null")+",\"gui_failure_subreason\":\""+gui_subreason+"\""
        +",\"foreign_capture_clear\":"+flag(proof.foreign_capture_clear)+",\"modifiers_clear\":"+flag(proof.modifiers_clear)+",\"button_state_matches\":"+flag(proof.button_state_matches)+",\"left_down\":"+flag(pressed(VK_LBUTTON))+",\"input_tag\":"+std::to_string(input_tag));
    return proof;
}
void emit_bootstrap(bool pass,const char* reason){
    if(bootstrap_emitted)return;bootstrap_emitted=true;
    const HWND current=GetForegroundWindow();const bool topmost_now=identity()&&(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    record("foreground_bootstrap",",\"target\":"+std::to_string(number(owned))+",\"set_foreground_attempted\":"+flag(bootstrap.attempted)+",\"set_foreground_success\":"+flag(set_foreground_success)+",\"activation_click_required\":"+flag(bootstrap.click_required)
        +",\"temporary_topmost\":"+flag(bootstrap.temporary_topmost)+",\"activation_point\":"+(bootstrap.click_required?point(activation_point):"null")+",\"window_from_point_root_matches\":"+flag(bootstrap.last_proof.point_root_matches)+",\"hit_test\":"+(bootstrap.click_required?std::to_string(bootstrap.hit):"null")+",\"foreign_capture_clear\":"+flag(bootstrap.last_proof.foreign_capture_clear)
        +",\"sendinput_move_success\":"+flag(bootstrap.move_success)+",\"sendinput_down_success\":"+flag(bootstrap.down_success)+",\"sendinput_up_success\":"+flag(bootstrap.up_success)+",\"wm_activate_seen\":"+flag(bootstrap.click_required?activate_seen.load():direct_activate_seen.load())+",\"wm_setfocus_seen\":"+flag(bootstrap.click_required?focus_seen.load():direct_focus_seen.load())+",\"activation_event_seen\":"+flag(bootstrap.event_seen)
        +",\"foreground\":"+std::to_string(number(current))+",\"topmost_now\":"+flag(topmost_now)+",\"final_foreground_matches\":"+flag(identity()&&current==owned)+",\"topmost_restored\":"+flag(bootstrap.topmost_restored)+",\"result\":\""+(pass?"PASS":"BLOCKED")+"\",\"reason\":\""+reason+"\"");
}
void restore_topmost(){
    if(!topmost_active)return;
    const bool ok=identity()&&SetWindowPos(owned,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;
    bootstrap.topmost_restored=ok&&!(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST);
    if(bootstrap.topmost_restored)topmost_active=false;
    record("activation_visibility",",\"target\":"+std::to_string(number(owned))+",\"enabled\":false,\"success\":"+flag(bootstrap.topmost_restored)+",\"flags\":19,\"topmost_style\":"+flag(identity()&&(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST))+",\"visible\":"+flag(identity()&&IsWindowVisible(owned)));
}
void activation_input(std::string_view phase,DWORD flags){
    require(!stop_requested,"owner_message_wait_failed");
    const auto proof=activation_proof(phase,phase=="up");const char* reason=fg::activation_guard(proof);
    require(std::string_view(reason)=="none",reason);
    auto input=mouse_input(flags,activation_point);const auto start=qpc();SetLastError(0);
    const UINT sent=SendInput(1,&input,sizeof(input));const DWORD error=sent==1?0:GetLastError();const auto returned=qpc();
    const bool success=sent==1;
    if(phase=="move")bootstrap.move_success=success;
    if(phase=="down"){bootstrap.down_success=success;activation_down=success;}
    if(phase=="up"){bootstrap.up_success=success;if(success)activation_down=false;}
    record("activation_input",",\"phase\":\""+std::string(phase)+"\",\"flags\":"+std::to_string(flags)+",\"point\":"+point(activation_point)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned));
    require(success,"BLOCKED_BY_SENDINPUT");
}
void release_activation_button(){
    if(!activation_down)return;
    try {
    // Preserve the visibility/point authority until a verified pending DOWN
    // has been released. This is cleanup, never another activation attempt.
    const auto proof=activation_proof("up",true);const char* reason=fg::activation_guard(proof);
    if(std::string_view(reason)!="none"){
        record("activation_release",",\"attempted\":false,\"sent\":0,\"reason\":\""+std::string(reason)+"\",\"button_release_pending\":true");return;
    }
    auto up=mouse_input(MOUSEEVENTF_LEFTUP,activation_point);const auto start=qpc();SetLastError(0);
    const UINT sent=SendInput(1,&up,sizeof(up));const DWORD error=sent==1?0:GetLastError();const auto returned=qpc();
    if(sent==1)activation_down=false;
    record("activation_release",",\"attempted\":true,\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"point\":"+point(activation_point)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned)+",\"button_release_pending\":"+flag(activation_down));
    }catch(const std::exception&){
        // A failed fresh hit-test/proof must not mask the original blocker or
        // skip the caller's visibility restoration and bootstrap record.
        record("activation_release",",\"attempted\":false,\"sent\":0,\"reason\":\"BLOCKED_BY_ACTIVATION_CLEANUP_PROOF_FAILURE\",\"button_release_pending\":true");
    }
}
void foreground_ready(){
    const bool own=identity(),desktop=desktop_available(),visible=own&&IsWindowVisible(owned);
    const HWND foreground=GetForegroundWindow();DWORD pid{};const DWORD tid=GetWindowThreadProcessId(foreground,&pid);
    GUITHREADINFO gui{sizeof(gui)};const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const bool clean=!pressed(VK_LBUTTON)&&!other_input();
    const bool stable=GetForegroundWindow()==foreground&&identity();
    const bool topmost=own&&(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
    const bool success=own&&desktop&&visible&&foreground==owned&&pid==GetCurrentProcessId()&&tid==ui_thread&&stable
        &&query&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&30)&&clean&&!topmost;
    record("foreground_ready",",\"target\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"own_identity\":"+flag(own)+",\"desktop_ready\":"+flag(desktop)+",\"visible\":"+flag(visible)+",\"foreground\":"+std::to_string(number(foreground))+",\"foreground_pid\":"+std::to_string(pid)+",\"foreground_tid\":"+std::to_string(tid)+",\"foreground_snapshot_stable\":"+flag(stable)
        +",\"gui_query_succeeded\":"+flag(query)+",\"gui_flags\":"+std::to_string(gui.flags)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))
        +",\"buttons_modifiers_clear\":"+flag(clean)+",\"topmost_now\":"+flag(topmost)+",\"source_thread_local_active\":"+std::to_string(number(gui.hwndActive))+",\"source_thread_local_focus\":"+std::to_string(number(gui.hwndFocus))+",\"success\":"+flag(success));
    require(success,"BLOCKED_BY_FOREGROUND_READY_PROOF");
}
namespace ops=panebind::platform::windows::operations;
panebind::core::geometry::Rect core_rect(const RECT& r){return {r.left,r.top,r.right,r.bottom};}
Geometry intended_at(POINT cursor){
    Geometry start;POINT anchor{};
    {std::lock_guard lock(continuation_mutex);start=intent_start;anchor=intent_pointer;}
    require(start.p_ok&&start.v_ok,"intent_start_capture_failed");
    const auto dx=panebind::core::geometry::checked_difference(static_cast<std::int64_t>(cursor.x),static_cast<std::int64_t>(anchor.x));
    const auto dy=panebind::core::geometry::checked_difference(static_cast<std::int64_t>(cursor.y),static_cast<std::int64_t>(anchor.y));
    require(dx&&dy,"intent_delta_overflow");
    const auto adjust=[&](RECT r){
        const auto edge=[](LONG original,std::int64_t delta){
            const auto value=panebind::core::geometry::checked_add(static_cast<std::int64_t>(original),delta);
            require(value&&*value>=LONG_MIN&&*value<=LONG_MAX,"intent_native_range");return static_cast<LONG>(*value);
        };
        if(selected_gesture==1){r.left=edge(r.left,*dx);r.right=edge(r.right,*dx);r.top=edge(r.top,*dy);}
        r.bottom=edge(r.bottom,*dy);return r;
    };
    Geometry target=start;target.p=adjust(start.p);target.v=adjust(start.v);
    const auto bridge=ops::prepare_visible_rect_adjustment(core_rect(start.p),core_rect(start.v),core_rect(target.v));
    require(bridge.status==ops::RectAdjustmentStatus::Succeeded&&bridge.positioning&&*bridge.positioning==core_rect(target.p),"intent_checked_frame_bridge_failed");
    require(contained(target.p),"intent_outside_safe_workarea");return target;
}
std::string named_geometry(const Geometry& g,std::string_view prefix){
    return ",\""+std::string(prefix)+"positioning\":"+(g.p_ok?rect(g.p):"null")+",\""+std::string(prefix)+"visible\":"+(g.v_ok?rect(g.v):"null");
}
std::string delta_fields(POINT cursor){
    POINT anchor{};{std::lock_guard lock(continuation_mutex);anchor=intent_pointer;}
    return ",\"cursor_delta\":["+std::to_string(static_cast<std::int64_t>(cursor.x)-anchor.x)+","+std::to_string(static_cast<std::int64_t>(cursor.y)-anchor.y)+"]";
}
ti::Identity isolation_identity(HWND window){
    ti::Identity id;id.hwnd=number(window);DWORD pid{};id.tid=window?GetWindowThreadProcessId(window,&pid):0;id.pid=pid;
    if(window&&id.pid==GetCurrentProcessId()&&id.tid==ui_thread)id.nonce=static_cast<std::uintptr_t>(GetWindowLongPtrW(window,GWLP_USERDATA));
    return id;
}
ti::Rect isolation_rect(const RECT& r){return {r.left,r.top,r.right,r.bottom};}
std::string isolation_rect_json(const ti::Rect& r){return "["+std::to_string(r[0])+","+std::to_string(r[1])+","+std::to_string(r[2])+","+std::to_string(r[3])+"]";}
struct InputIsolationSnapshot {ti::TestInputIsolationDiagnostic facts;Geometry source;HWND foreground{};DWORD foreground_pid{},foreground_tid{};};
InputIsolationSnapshot capture_input_isolation(POINT destination,bool cursor_available=true){
    InputIsolationSnapshot s;auto& d=s.facts;d.cursor_available=cursor_available;d.destination={destination.x,destination.y};
    d.expected_source={number(owned),GetCurrentProcessId(),ui_thread,run_nonce};d.actual_source=isolation_identity(owned);
    d.expected_shield={number(input_shield),GetCurrentProcessId(),ui_thread,run_nonce};d.actual_shield=isolation_identity(input_shield);
    s.source=capture();d.source_positioning_available=s.source.p_ok;d.source_positioning=isolation_rect(s.source.p);
    s.foreground=GetForegroundWindow();s.foreground_tid=GetWindowThreadProcessId(s.foreground,&s.foreground_pid);
    d.foreground_matches=s.foreground==owned&&s.foreground_pid==GetCurrentProcessId()&&s.foreground_tid==ui_thread&&GetForegroundWindow()==s.foreground;
    d.shield_needed=shield_needed;d.shield_active=shield_active;d.shield_never_activated=!shield_activated;
    if(ti::exact_identity(d.actual_shield,d.expected_shield)){
        RECT shield_rect{};if(GetWindowRect(input_shield,&shield_rect))d.shield_positioning=isolation_rect(shield_rect);
        const auto ex=GetWindowLongPtrW(input_shield,GWL_EXSTYLE);d.shield_noactivate=(ex&WS_EX_NOACTIVATE)!=0;d.shield_topmost=(ex&WS_EX_TOPMOST)!=0;d.shield_visible=IsWindowVisible(input_shield)!=FALSE;
    }
    d.root=cursor_available?number(GetAncestor(WindowFromPoint(destination),GA_ROOT)):0;
    return s;
}
std::string input_isolation_fields(const InputIsolationSnapshot& s){
    const auto& d=s.facts;const bool inside=ti::contains(d.source_positioning,d.destination);
    const bool source_identity=ti::exact_identity(d.actual_source,d.expected_source),shield_identity=ti::exact_identity(d.actual_shield,d.expected_shield);
    const bool root_owned=(d.root==d.expected_source.hwnd&&source_identity)||(d.root==d.expected_shield.hwnd&&shield_identity&&d.shield_active);
    return ",\"run_nonce\":"+std::to_string(run_nonce)+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"actual_source_pid\":"+std::to_string(d.actual_source.pid)+",\"actual_source_tid\":"+std::to_string(d.actual_source.tid)+",\"actual_source_nonce\":"+std::to_string(d.actual_source.nonce)+",\"source_identity\":"+flag(source_identity)
        +",\"shield_hwnd\":"+std::to_string(number(input_shield))+",\"shield_pid\":"+std::to_string(GetCurrentProcessId())+",\"shield_tid\":"+std::to_string(ui_thread)+",\"actual_shield_pid\":"+std::to_string(d.actual_shield.pid)+",\"actual_shield_tid\":"+std::to_string(d.actual_shield.tid)+",\"actual_shield_nonce\":"+std::to_string(d.actual_shield.nonce)+",\"shield_identity\":"+flag(shield_identity)
        +",\"cursor_available\":"+flag(d.cursor_available)+",\"destination\":"+point({static_cast<LONG>(d.destination[0]),static_cast<LONG>(d.destination[1])})+",\"source_positioning_available\":"+flag(d.source_positioning_available)+",\"source_positioning\":"+(d.source_positioning_available?isolation_rect_json(d.source_positioning):"null")
        +",\"shield_positioning\":"+(ti::valid(d.shield_positioning)?isolation_rect_json(d.shield_positioning):"null")+",\"inside_source\":"+flag(inside)+",\"root\":"+std::to_string(d.root)+",\"expected_root\":"+std::to_string(inside?d.expected_source.hwnd:d.expected_shield.hwnd)+",\"root_owned\":"+flag(root_owned)
        +",\"foreground_hwnd\":"+std::to_string(number(s.foreground))+",\"foreground_pid\":"+std::to_string(s.foreground_pid)+",\"foreground_tid\":"+std::to_string(s.foreground_tid)+",\"foreground_matches\":"+flag(d.foreground_matches)
        +",\"shield_needed\":"+flag(d.shield_needed)+",\"shield_active\":"+flag(d.shield_active)+",\"shield_noactivate\":"+flag(d.shield_noactivate)+",\"shield_topmost\":"+flag(d.shield_topmost)+",\"shield_visible\":"+flag(d.shield_visible)+",\"shield_never_activated\":"+flag(d.shield_never_activated)
        +",\"shield_nonoverlapping\":"+flag(!ti::overlaps(d.source_positioning,d.shield_positioning))+",\"point_valid\":"+flag(d.passed()&&shield_healthy)+",\"failure_class\":\""+(d.passed()&&shield_healthy?"None":"TestInputIsolationUnavailable")+"\"";
}
LRESULT CALLBACK shield_procedure(HWND window,UINT message,WPARAM w,LPARAM l) noexcept {
    try{
        if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        if((message==WM_ACTIVATE&&LOWORD(w)!=WA_INACTIVE)||message==WM_SETFOCUS){
            shield_activated=true;shield_healthy=false;
            record("input_shield_activation",",\"shield_hwnd\":"+std::to_string(number(window))+",\"message\":"+std::to_string(message)+",\"run_nonce\":"+std::to_string(run_nonce));
        }
    }catch(...){shield_healthy=false;}
    return DefWindowProcW(window,message,w,l);
}
void position_input_shield(const Geometry& actual,std::uint64_t operation,std::uint64_t postverify){
    if(!isolation_mode||!shield_needed)return;
    require(GetCurrentThreadId()==ui_thread&&actual.p_ok&&identity(),"TestInputIsolationUnavailable");
    Geometry terminal;{std::lock_guard lock(continuation_mutex);terminal=terminal_geometry;}
    const auto plan=ti::position_existing_shield(frozen_shield_plan,isolation_rect(terminal.p),isolation_rect(actual.p));
    const ti::Identity expected{number(input_shield),GetCurrentProcessId(),ui_thread,run_nonce};
    const bool work_area_contained=ti::contained_rect(isolation_rect(work_area),plan.positioning);
    require(plan.valid&&work_area_contained&&ti::exact_identity(isolation_identity(input_shield),expected)&&shield_healthy,"TestInputIsolationUnavailable");
    const auto foreground_before=GetForegroundWindow();require(foreground_before==owned,"TestInputIsolationUnavailable");
    const auto started=qpc();SetLastError(0);
    const bool success=SetWindowPos(input_shield,HWND_TOPMOST,static_cast<int>(plan.positioning[0]),static_cast<int>(plan.positioning[1]),static_cast<int>(plan.positioning[2]-plan.positioning[0]),static_cast<int>(plan.positioning[3]-plan.positioning[1]),SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
    const auto error=success?0:GetLastError();const auto returned=qpc();
    shield_active=success;const auto source_after=capture();RECT shield_rect{};const bool available=GetWindowRect(input_shield,&shield_rect)!=FALSE;
    const bool source_unchanged=source_after.p_ok&&source_after.v_ok&&equal(source_after.p,actual.p)&&equal(source_after.v,actual.v);
    const auto foreground_after=GetForegroundWindow();const bool foreground_unchanged=foreground_after==foreground_before&&foreground_before==owned;
    const auto proof=capture_input_isolation({static_cast<LONG>(frozen_shield_plan.min_x),static_cast<LONG>(frozen_shield_plan.max_y)});
    const auto row=record("input_shield_position",",\"actor\":\"shield\",\"source_operation_id\":"+std::to_string(operation)+",\"source_postverify_sequence\":"+std::to_string(postverify)+",\"target\":"+std::to_string(number(input_shield))+",\"insert_after\":-1,\"flags\":80,\"native_start_qpc\":"+std::to_string(started)+",\"native_return_qpc\":"+std::to_string(returned)+",\"native_success\":"+flag(success)+",\"error\":"+std::to_string(error)
        +",\"requested_positioning\":"+isolation_rect_json(plan.positioning)+",\"actual_positioning\":"+(available?rect(shield_rect):"null")+",\"terminal_source_positioning\":"+rect(terminal.p)+",\"source_positioning_before\":"+rect(actual.p)+",\"source_positioning_after\":"+(source_after.p_ok?rect(source_after.p):"null")+",\"source_visible_before\":"+(actual.v_ok?rect(actual.v):"null")+",\"source_visible_after\":"+(source_after.v_ok?rect(source_after.v):"null")+",\"source_unchanged\":"+flag(source_unchanged)+",\"foreground_before\":"+std::to_string(number(foreground_before))+",\"foreground_after\":"+std::to_string(number(foreground_after))+",\"foreground_unchanged\":"+flag(foreground_unchanged)
        +",\"planned_min_x\":"+std::to_string(frozen_shield_plan.min_x)+",\"planned_max_x\":"+std::to_string(frozen_shield_plan.max_x)+",\"planned_min_y\":"+std::to_string(frozen_shield_plan.min_y)+",\"planned_max_y\":"+std::to_string(frozen_shield_plan.max_y)+",\"margin_px\":50,\"top_margin_clipped\":"+flag(plan.top_margin_clipped)+",\"work_area\":"+rect(work_area)+",\"work_area_contained\":"+flag(work_area_contained&&available&&ti::contained_rect(isolation_rect(work_area),isolation_rect(shield_rect)))+input_isolation_fields(proof));
    require(row&&log_ok&&success&&available&&isolation_rect(shield_rect)==plan.positioning&&source_unchanged&&foreground_unchanged&&proof.facts.passed()&&shield_healthy,"TestInputIsolationUnavailable");
}
void setup_input_isolation(POINT cursor,const Geometry& actual){
    require(GetCurrentThreadId()==ui_thread&&native_end_qpc&&winevent_end_qpc&&product_preflight_sequence&&actual.p_ok&&identity(),"TestInputIsolationUnavailable");
    std::array<ti::Point,ti::planned_point_count> points{};points[0]={cursor.x,cursor.y};
    POINT planned_current;
    {std::lock_guard lock(continuation_mutex);planned_current=planned_current_cursor;for(std::size_t n=0;n<remaining_input_points.size();++n)points[n+1]={remaining_input_points[n].x,remaining_input_points[n].y};}
    Geometry terminal;{std::lock_guard lock(continuation_mutex);terminal=terminal_geometry;}
    frozen_shield_plan=ti::bottom_shield_plan(isolation_rect(terminal.p),isolation_rect(actual.p),points);
    const bool current_matches=ti::matches_planned_cursor(points[0],{planned_current.x,planned_current.y});
    bool points_contained=true;for(const auto& p:points)if(!ti::contains(isolation_rect(work_area),p))points_contained=false;
    const bool plan_contained=frozen_shield_plan.valid&&(!frozen_shield_plan.needed||ti::contained_rect(isolation_rect(work_area),frozen_shield_plan.positioning));
    std::string point_list="[";for(std::size_t n=0;n<points.size();++n){if(n)point_list+=",";point_list+=point({static_cast<LONG>(points[n][0]),static_cast<LONG>(points[n][1])});}point_list+="]";
    record("input_isolation_setup_plan",",\"run_nonce\":"+std::to_string(run_nonce)+",\"product_preflight_sequence\":"+std::to_string(product_preflight_sequence)+",\"planned_current_cursor\":"+point(planned_current)+",\"actual_current_cursor\":"+point(cursor)+",\"current_cursor_matches_plan\":"+flag(current_matches)+",\"planned_points\":"+point_list+",\"work_area\":"+rect(work_area)+",\"points_work_area_contained\":"+flag(points_contained)+",\"shield_plan_valid\":"+flag(frozen_shield_plan.valid)+",\"shield_plan_needed\":"+flag(frozen_shield_plan.needed)+",\"shield_plan_positioning\":"+(frozen_shield_plan.valid&&frozen_shield_plan.needed?isolation_rect_json(frozen_shield_plan.positioning):"null")+",\"shield_plan_work_area_contained\":"+flag(plan_contained)+",\"test_data_ready\":"+flag(current_matches&&points_contained&&plan_contained)+",\"failure_class\":\""+(current_matches&&points_contained&&plan_contained?"None":"TestInputIsolationUnavailable")+"\"");
    require(log_ok&&current_matches&&points_contained&&plan_contained,"TestInputIsolationUnavailable");shield_needed=frozen_shield_plan.needed;
    if(shield_needed){
        require(selected_gesture==2,"TestInputIsolationUnavailable");
        const auto started=qpc();require(started>std::max(native_end_qpc.load(),winevent_end_qpc.load()),"shield_before_END");
        input_shield=CreateWindowExW(WS_EX_NOACTIVATE,L"PaneBindPostEndInputShield",L"PaneBind empty post-END input shield",WS_POPUP,static_cast<int>(frozen_shield_plan.positioning[0]),static_cast<int>(frozen_shield_plan.positioning[1]),static_cast<int>(frozen_shield_plan.positioning[2]-frozen_shield_plan.positioning[0]),static_cast<int>(frozen_shield_plan.positioning[3]-frozen_shield_plan.positioning[1]),nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        const auto returned=qpc();require(input_shield!=nullptr,"TestInputIsolationUnavailable");shield_created=true;
        SetLastError(0);const auto old=SetWindowLongPtrW(input_shield,GWLP_USERDATA,static_cast<LONG_PTR>(run_nonce));const auto nonce_error=old?0:GetLastError();
        const auto actual_identity=isolation_identity(input_shield);
        record("input_shield_created",",\"actor\":\"shield\",\"shield_hwnd\":"+std::to_string(number(input_shield))+",\"shield_pid\":"+std::to_string(GetCurrentProcessId())+",\"shield_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_shield_pid\":"+std::to_string(actual_identity.pid)+",\"actual_shield_tid\":"+std::to_string(actual_identity.tid)+",\"actual_shield_nonce\":"+std::to_string(actual_identity.nonce)+",\"nonce_error\":"+std::to_string(nonce_error)+",\"create_tid\":"+std::to_string(GetCurrentThreadId())+",\"create_start_qpc\":"+std::to_string(started)+",\"create_return_qpc\":"+std::to_string(returned)+",\"create_positioning\":"+isolation_rect_json(frozen_shield_plan.positioning)+",\"style\":2147483648,\"exstyle\":134217728,\"parent\":0,\"product_preflight_sequence\":"+std::to_string(product_preflight_sequence)+",\"winevent_end_qpc\":"+std::to_string(winevent_end_qpc.load()));
        require(!nonce_error&&actual_identity.nonce==run_nonce,"TestInputIsolationUnavailable");position_input_shield(actual,0,0);
    }
    for(std::size_t n=0;n<points.size();++n){
        const auto proof=capture_input_isolation({static_cast<LONG>(points[n][0]),static_cast<LONG>(points[n][1])});
        record("input_isolation_point",",\"phase\":\"setup\",\"index\":"+std::to_string(n+2)+input_isolation_fields(proof));
        require(proof.facts.passed()&&shield_healthy,"TestInputIsolationUnavailable");
    }
    isolation_ready_qpc=qpc();
    isolation_ready_sequence=record("input_isolation_ready",",\"checked_count\":19,\"shield_needed\":"+flag(shield_needed)+",\"shield_state\":\""+(shield_needed?"READY":"NOT_NEEDED")+"\",\"run_nonce\":"+std::to_string(run_nonce)+",\"product_preflight_sequence\":"+std::to_string(product_preflight_sequence)+",\"end_qpc\":"+std::to_string(native_end_qpc.load())+",\"winevent_end_qpc\":"+std::to_string(winevent_end_qpc.load())+",\"isolation_ready_qpc\":"+std::to_string(isolation_ready_qpc.load())+",\"test_input_isolation\":true");
    require(isolation_ready_sequence&&log_ok,"input_isolation_record_failed");
}
bool fresh_synthetic_input_isolation(POINT destination,DWORD flags,const char* scope){
    const auto proof=capture_input_isolation(destination);
    record("synthetic_input_isolation",",\"phase\":\"api_boundary\",\"flags\":"+std::to_string(flags)+",\"input_scope\":\""+scope+"\",\"acceptance_eligible\":"+flag(std::string_view(scope)=="gesture")+",\"input_isolation_ready_sequence\":"+std::to_string(isolation_ready_sequence.load())+input_isolation_fields(proof));
    return proof.facts.passed()&&shield_healthy&&log_ok;
}
void destroy_input_shield(const char* reason,std::uint64_t acceptance,std::int64_t observed_up){
    if(!isolation_mode||!shield_created||shield_destroyed)return;
    require(GetCurrentThreadId()==ui_thread,"shield_destroy_wrong_thread");
    const HWND window=input_shield;const auto actual_identity=isolation_identity(window);
    const bool exact=ti::exact_identity(actual_identity,{number(window),GetCurrentProcessId(),ui_thread,run_nonce});
    const auto started=qpc();bool success=false;if(exact)success=DestroyWindow(window)!=FALSE;
    const auto returned=qpc();const bool absent=!IsWindow(window);shield_destroyed=success&&absent;if(shield_destroyed){shield_active=false;input_shield=nullptr;}
    record("input_shield_destroyed",",\"actor\":\"shield\",\"shield_hwnd\":"+std::to_string(number(window))+",\"shield_pid\":"+std::to_string(GetCurrentProcessId())+",\"shield_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_shield_pid\":"+std::to_string(actual_identity.pid)+",\"actual_shield_tid\":"+std::to_string(actual_identity.tid)+",\"actual_shield_nonce\":"+std::to_string(actual_identity.nonce)+",\"own_identity\":"+flag(exact)+",\"reason\":\""+reason+"\",\"source_acceptance_sequence\":"+std::to_string(acceptance)+",\"raw_up_receiver_qpc\":"+std::to_string(observed_up)+",\"destroy_start_qpc\":"+std::to_string(started)+",\"destroy_return_qpc\":"+std::to_string(returned)+",\"destroy_success\":"+flag(success)+",\"window_absent\":"+flag(absent));
    require(shield_destroyed,"input_shield_destroy_failed");
}
struct OwnerProof {POINT cursor{};std::string fields;bool healthy{},product_authority{};};
OwnerProof owner_proof(bool require_held){
    OwnerProof proof;
    const auto actual_source=isolation_mode?isolation_identity(owned):ti::Identity{};
    const bool own=isolation_mode?ti::exact_identity(actual_source,{number(owned),GetCurrentProcessId(),ui_thread,run_nonce}):identity();
    const bool desktop=desktop_available(),visible=own&&IsWindowVisible(owned);
    const HWND foreground=GetForegroundWindow();DWORD pid{};const DWORD tid=GetWindowThreadProcessId(foreground,&pid);
    GUITHREADINFO gui{sizeof(gui)};const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const bool cursor_ok=GetCursorPos(&proof.cursor)!=FALSE;
    const HWND root=cursor_ok?GetAncestor(WindowFromPoint(proof.cursor),GA_ROOT):nullptr;
    const bool held=pressed(VK_LBUTTON),clean=!other_input();bool up{};
    {std::lock_guard lock(continuation_mutex);up=raw_up_seen;}
    const UINT dpi=own?GetDpiForWindow(owned):0;const auto monitor=MonitorFromWindow(owned,MONITOR_DEFAULTTONULL);
    proof.healthy=own&&desktop&&visible&&foreground==owned&&pid==GetCurrentProcessId()&&tid==ui_thread&&GetForegroundWindow()==foreground
        &&query&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&30)&&cursor_ok&&(isolation_mode||input_root_owned(root))&&clean
        &&(!require_held||(held&&!up))&&receiver_ok&&takeover_healthy&&!foreign_capture_transferred&&dpi==frozen_dpi&&monitor==frozen_monitor;
    if(isolation_mode){
        proof.product_authority=own&&desktop&&visible&&foreground==owned&&pid==GetCurrentProcessId()&&tid==ui_thread&&GetForegroundWindow()==foreground
            &&query&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&30)&&clean&&(!require_held||(held&&!up))
            &&receiver_ok&&takeover_healthy&&!foreign_capture_transferred&&dpi==frozen_dpi&&monitor==frozen_monitor&&winevent_healthy&&log_ok&&native_drag_after_return==0&&native_drag_after_end==0&&native_drag_after_winevent_end==0&&unowned_geometry_changes==0;
        proof.healthy=proof.product_authority&&cursor_ok;
    }
    proof.fields=",\"target\":"+std::to_string(number(owned))+",\"guard\":"+std::to_string(number(guard))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"own_identity\":"+flag(own)+",\"desktop_ready\":"+flag(desktop)+",\"source_visible\":"+flag(visible)+",\"foreground\":"+std::to_string(number(foreground))+",\"foreground_pid\":"+std::to_string(pid)+",\"foreground_tid\":"+std::to_string(tid)
        +",\"gui_query_succeeded\":"+flag(query)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"gui_flags\":"+std::to_string(gui.flags)
        +",\"buttons_modifiers_clear\":"+flag(clean)+",\"left_down\":"+flag(held)+",\"receiver_healthy\":"+flag(receiver_ok)+",\"raw_up_seen\":"+flag(up)+",\"cursor_root\":"+std::to_string(number(root))+",\"dpi\":"+std::to_string(dpi)+",\"monitor\":"+std::to_string(reinterpret_cast<std::uintptr_t>(monitor))
        +(isolation_mode?",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+std::to_string(actual_source.pid)+",\"actual_source_tid\":"+std::to_string(actual_source.tid)+",\"actual_source_nonce\":"+std::to_string(actual_source.nonce)+",\"cursor_root_required_for_product\":false,\"cursor_data_ready\":"+flag(cursor_ok)+",\"product_authority\":"+flag(proof.product_authority):"");
    return proof;
}
std::array<std::int64_t,4> rect_values(const RECT& r){return {r.left,r.top,r.right,r.bottom};}
std::string diagnostic_rect(const std::array<std::int64_t,4>& r,bool available){
    return available?"["+std::to_string(r[0])+","+std::to_string(r[1])+","+std::to_string(r[2])+","+std::to_string(r[3])+"]":"null";
}
std::string handoff_diagnostic_fields(const hd::HandoffPreflightDiagnostic& d,const hd::Failures& f){
    const auto b=[](const char* name,bool value){return ",\""+std::string(name)+"\":"+flag(value);};
    const auto n=[](const char* name,auto value){return ",\""+std::string(name)+"\":"+std::to_string(value);};
    std::string s=n("target",number(owned))+n("guard",number(guard))+n("source_pid",GetCurrentProcessId())+n("source_tid",ui_thread)+n("gesture_id",active_gesture_id.load())
        +b("end_observed",d.end_observed)+n("end_sequence",d.end_sequence)+n("end_qpc",d.end_qpc)
        +b("winevent_end_observed",d.winevent_end_observed)+n("winevent_end_sequence",d.winevent_end_sequence)+n("winevent_end_qpc",d.winevent_end_qpc)
        +b("own_identity",d.own_identity)+n("actual_source_pid",d.actual_source_pid)+n("actual_source_tid",d.actual_source_tid)+b("desktop_ready",d.desktop_ready)+b("source_visible",d.source_visible)
        +n("foreground_hwnd",d.foreground_hwnd)+b("foreground_matches",d.foreground_matches)+b("foreground_snapshot_stable",d.foreground_snapshot_stable)+n("foreground_pid",d.foreground_pid)+n("foreground_tid",d.foreground_tid)
        +b("gui_query_succeeded",d.gui_query_succeeded)+n("gui_error",d.gui_error)+n("capture_hwnd",d.capture_hwnd)+b("capture_clear",d.capture_clear)+n("menu_owner_hwnd",d.menu_owner_hwnd)+b("menu_clear",d.menu_clear)
        +n("move_size_hwnd",d.move_size_hwnd)+b("move_size_clear",d.move_size_clear)+n("gui_flags",d.gui_flags)+b("gui_in_movesize_clear",d.gui_in_movesize_clear)
        +b("cursor_success",d.cursor_success)+n("cursor_error",d.cursor_error)+",\"cursor\":"+(d.cursor_success?"["+std::to_string(d.cursor[0])+","+std::to_string(d.cursor[1])+"]":"null")+n("cursor_root",d.cursor_root)+b("cursor_root_owned_or_guard",d.cursor_root_owned_or_guard)
        +b("buttons_modifiers_clear",d.buttons_modifiers_clear)+b("left_down",d.left_down)+b("raw_up_seen",d.raw_up_seen)+b("receiver_healthy",d.receiver_healthy)+b("takeover_healthy",d.takeover_healthy)+b("foreign_capture_transferred",d.foreign_capture_transferred)
        +n("dpi",d.dpi)+n("frozen_dpi",frozen_dpi)+b("dpi_matches",d.dpi_matches)+n("monitor",d.monitor)+n("frozen_monitor",reinterpret_cast<std::uintptr_t>(frozen_monitor))+b("monitor_matches",d.monitor_matches)
        +b("actual_positioning_available",d.actual_positioning_available)+b("actual_visible_available",d.actual_visible_available)+b("terminal_positioning_available",d.terminal_positioning_available)+b("terminal_visible_available",d.terminal_visible_available)
        +",\"terminal_positioning\":"+diagnostic_rect(d.terminal_positioning,d.terminal_positioning_available)+",\"terminal_visible\":"+diagnostic_rect(d.terminal_visible,d.terminal_visible_available)
        +",\"actual_positioning\":"+diagnostic_rect(d.actual_positioning,d.actual_positioning_available)+",\"actual_visible\":"+diagnostic_rect(d.actual_visible,d.actual_visible_available)
        +n("positioning_error",d.positioning_error)+n("visible_hresult",d.visible_hresult)+b("terminal_positioning_exact",d.terminal_positioning_exact)+b("terminal_visible_exact",d.terminal_visible_exact)
        +n("native_drag_after_cancel_return",d.native_drag_after_cancel_return)+n("native_drag_after_end",d.native_drag_after_end)+n("native_drag_after_winevent_end",d.native_drag_after_winevent_end)+n("unowned_geometry_changes",d.unowned_geometry_changes)
        +b("winevent_healthy",winevent_healthy)+b("log_healthy",log_ok)+",\"failure_class\":\""+std::string(hd::name(f.first()))+"\",\"failed_checks\":[";
    for(std::size_t i=0;i<f.count;++i){if(i)s+=",";s+="\""+std::string(hd::name(f.values[i]))+"\"";}return s+"]";
}
struct HandoffSnapshot {hd::HandoffPreflightDiagnostic diagnostic;OwnerProof proof;Geometry actual;std::uintptr_t source_nonce{};};
HandoffSnapshot capture_handoff_preflight(){
    HandoffSnapshot s;auto& d=s.diagnostic;
    d.end_sequence=native_end_sequence;d.end_qpc=native_end_qpc;d.end_observed=d.end_sequence!=0&&d.end_qpc!=0;
    d.winevent_end_sequence=winevent_end_sequence;d.winevent_end_qpc=winevent_end_qpc;d.winevent_end_observed=d.winevent_end_sequence!=0&&d.winevent_end_qpc!=0;
    DWORD source_pid{};d.actual_source_tid=GetWindowThreadProcessId(owned,&source_pid);d.actual_source_pid=source_pid;s.source_nonce=isolation_mode?static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA)):0;
    d.own_identity=owned&&d.actual_source_pid==GetCurrentProcessId()&&d.actual_source_tid==ui_thread&&(!isolation_mode||s.source_nonce==run_nonce);
    d.desktop_ready=desktop_available();d.source_visible=d.own_identity&&IsWindowVisible(owned);
    DWORD foreground_pid{};const auto foreground=GetForegroundWindow();d.foreground_hwnd=number(foreground);d.foreground_tid=GetWindowThreadProcessId(foreground,&foreground_pid);d.foreground_pid=foreground_pid;
    d.foreground_snapshot_stable=GetForegroundWindow()==foreground;
    d.foreground_matches=d.foreground_snapshot_stable&&foreground==owned&&d.foreground_pid==GetCurrentProcessId()&&d.foreground_tid==ui_thread;
    GUITHREADINFO gui{sizeof(gui)};SetLastError(0);d.gui_query_succeeded=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;d.gui_error=d.gui_query_succeeded?0:GetLastError();
    d.capture_hwnd=number(gui.hwndCapture);d.menu_owner_hwnd=number(gui.hwndMenuOwner);d.move_size_hwnd=number(gui.hwndMoveSize);d.gui_flags=gui.flags;
    d.capture_clear=d.gui_query_succeeded&&!gui.hwndCapture;d.menu_clear=d.gui_query_succeeded&&!gui.hwndMenuOwner&&!(gui.flags&(GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE));
    d.move_size_clear=d.gui_query_succeeded&&!gui.hwndMoveSize;d.gui_in_movesize_clear=d.gui_query_succeeded&&!(gui.flags&GUI_INMOVESIZE);
    SetLastError(0);d.cursor_success=GetCursorPos(&s.proof.cursor)!=FALSE;d.cursor_error=d.cursor_success?0:GetLastError();d.cursor={s.proof.cursor.x,s.proof.cursor.y};
    const auto root=d.cursor_success?GetAncestor(WindowFromPoint(s.proof.cursor),GA_ROOT):nullptr;d.cursor_root=number(root);d.cursor_root_owned_or_guard=d.cursor_success&&input_root_owned(root);
    d.buttons_modifiers_clear=!other_input();d.left_down=pressed(VK_LBUTTON);{std::lock_guard lock(continuation_mutex);d.raw_up_seen=raw_up_seen;}
    if(!winevent_healthy||!log_ok)takeover_healthy=false;
    d.receiver_healthy=receiver_ok;d.takeover_healthy=takeover_healthy;d.foreign_capture_transferred=foreign_capture_transferred;
    d.dpi=d.own_identity?GetDpiForWindow(owned):0;d.dpi_matches=d.dpi==frozen_dpi&&frozen_dpi!=0;
    d.monitor=reinterpret_cast<std::uintptr_t>(MonitorFromWindow(owned,MONITOR_DEFAULTTONULL));d.monitor_matches=d.monitor==reinterpret_cast<std::uintptr_t>(frozen_monitor)&&frozen_monitor!=nullptr;
    Geometry terminal;{std::lock_guard lock(continuation_mutex);terminal=terminal_geometry;}
    s.actual=capture();d.actual_positioning_available=s.actual.p_ok;d.actual_visible_available=s.actual.v_ok;d.terminal_positioning_available=terminal.p_ok;d.terminal_visible_available=terminal.v_ok;
    d.actual_positioning=rect_values(s.actual.p);d.actual_visible=rect_values(s.actual.v);d.terminal_positioning=rect_values(terminal.p);d.terminal_visible=rect_values(terminal.v);
    d.positioning_error=s.actual.error;d.visible_hresult=s.actual.hr;
    d.terminal_positioning_exact=s.actual.p_ok&&terminal.p_ok&&equal(s.actual.p,terminal.p);d.terminal_visible_exact=s.actual.v_ok&&terminal.v_ok&&equal(s.actual.v,terminal.v);
    d.native_drag_after_cancel_return=native_drag_after_return;d.native_drag_after_end=native_drag_after_end;d.native_drag_after_winevent_end=native_drag_after_winevent_end;d.unowned_geometry_changes=unowned_geometry_changes;
    s.proof.healthy=(isolation_mode?ti::classify_product(ti::product_facts(d)):hd::classify(d)).count==0;
    if(isolation_mode)s.proof.product_authority=s.proof.healthy;
    s.proof.fields=",\"target\":"+std::to_string(number(owned))+",\"guard\":"+std::to_string(number(guard))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"own_identity\":"+flag(d.own_identity)+",\"desktop_ready\":"+flag(d.desktop_ready)+",\"source_visible\":"+flag(d.source_visible)+",\"foreground\":"+std::to_string(d.foreground_hwnd)+",\"foreground_pid\":"+std::to_string(d.foreground_pid)+",\"foreground_tid\":"+std::to_string(d.foreground_tid)
        +",\"gui_query_succeeded\":"+flag(d.gui_query_succeeded)+",\"capture_hwnd\":"+std::to_string(d.capture_hwnd)+",\"menu_owner_hwnd\":"+std::to_string(d.menu_owner_hwnd)+",\"move_size_hwnd\":"+std::to_string(d.move_size_hwnd)+",\"gui_flags\":"+std::to_string(d.gui_flags)
        +",\"buttons_modifiers_clear\":"+flag(d.buttons_modifiers_clear)+",\"left_down\":"+flag(d.left_down)+",\"receiver_healthy\":"+flag(d.receiver_healthy)+",\"raw_up_seen\":"+flag(d.raw_up_seen)+",\"cursor_root\":"+std::to_string(d.cursor_root)+",\"dpi\":"+std::to_string(d.dpi)+",\"monitor\":"+std::to_string(d.monitor)
        +(isolation_mode?",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+std::to_string(d.actual_source_pid)+",\"actual_source_tid\":"+std::to_string(d.actual_source_tid)+",\"actual_source_nonce\":"+std::to_string(s.source_nonce)+",\"cursor_root_required_for_product\":false,\"cursor_data_ready\":"+flag(d.cursor_success)+",\"product_authority\":"+flag(s.proof.healthy):"");
    return s;
}
std::uint64_t emit_handoff_preflight(const char* phase,const HandoffSnapshot& s){
    const auto f=isolation_mode?ti::classify_product(ti::product_facts(s.diagnostic)):hd::classify(s.diagnostic);
    const bool scope=takeover_scope.load(),ready=handoff_ready.load(),cleanup=cleanup_observation.load();
    const bool authorized=!isolation_mode&&std::string_view(phase)=="winevent_end"&&f.count==0&&scope&&!ready&&!cleanup;
    std::string product_fields;
    if(isolation_mode){product_fields=",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_nonce\":"+std::to_string(s.source_nonce)+",\"cursor_root_required_for_product\":false,\"product_authority\":"+flag(f.count==0)+",\"product_gate_passed\":"+flag(f.count==0)+",\"product_failure_class\":\""+std::string(hd::name(f.first()))+"\",\"product_failed_checks\":[";
        for(std::size_t n=0;n<f.count;++n){if(n)product_fields+=",";product_fields+="\""+std::string(hd::name(f.values[n]))+"\"";}product_fields+="]";}
    const auto row=record(isolation_mode?"product_handoff_preflight":"handoff_preflight",",\"phase\":\""+std::string(phase)+"\",\"write_authorized\":"+flag(authorized)+",\"takeover_scope\":"+flag(scope)+",\"handoff_ready\":"+flag(ready)+",\"cleanup_observation\":"+flag(cleanup)+handoff_diagnostic_fields(s.diagnostic,f)+product_fields);
    if(s.diagnostic.terminal_positioning_exact&&s.diagnostic.actual_visible_available&&s.diagnostic.terminal_visible_available&&!s.diagnostic.terminal_visible_exact)
        record("handoff_lag_candidate",",\"preflight_sequence\":"+std::to_string(row)+",\"phase\":\""+phase+"\",\"reason\":\"DWM_VISIBLE_TERMINAL_LAG_CANDIDATE\"");
    require(row!=0&&log_ok,"handoff_diagnostic_record_failed");return row;
}
void process_native_preflight(){
    enable_winevent_audit();const auto s=capture_handoff_preflight();emit_handoff_preflight("native_end",s);SetEvent(native_preflight_finished);
}
struct WriteReceipt {std::uint64_t id{};int native_calls{};};
WriteReceipt write_intended(const char* kind,std::uint64_t quantum,const RawPacket& raw,std::uint32_t first,const OwnerProof& proof){
    require(proof.healthy&&native_end_qpc&&native_drag_after_return==0&&native_drag_after_end==0&&unowned_geometry_changes==0,"takeover_authority_failed");
    if(diagnostic_mode)require(takeover_scope&&!cleanup_observation&&winevent_end_qpc&&winevent_healthy&&log_ok&&native_drag_after_winevent_end==0&&final_preflight_sequence,"winevent_takeover_authority_failed");
    if(isolation_mode)require(isolation_ready_sequence&&isolation_ready_qpc&&shield_healthy,"TestInputIsolationUnavailable");
    std::uint64_t id{},writer_isolation_sequence{};
    if(isolation_mode){
        id=++operation_counter;const auto isolation=capture_input_isolation(proof.cursor);
        writer_isolation_sequence=record("writer_input_isolation",",\"operation_id\":"+std::to_string(id)+",\"quantum_id\":"+std::to_string(quantum)+",\"kind\":\""+kind+"\",\"writer_product_authority\":"+flag(proof.product_authority)+",\"product_preflight_sequence\":"+std::to_string(product_preflight_sequence)+",\"input_isolation_ready_sequence\":"+std::to_string(isolation_ready_sequence.load())+input_isolation_fields(isolation));
        require(writer_isolation_sequence&&log_ok&&isolation.facts.passed()&&shield_healthy,"TestInputIsolationUnavailable");
    }
    const auto before=capture(),target=intended_at(proof.cursor);
    require(before.p_ok&&before.v_ok&&equal(before.p,expected_live.p)&&equal(before.v,expected_live.v),"post_end_unattributed_geometry_change");
    const bool mismatch=!equal(before.p,target.p)||!equal(before.v,target.v);
    if(!isolation_mode)id=++operation_counter;
    record("writer_begin",",\"operation_id\":"+std::to_string(id)+",\"quantum_id\":"+std::to_string(quantum)+",\"kind\":\""+kind+"\",\"raw_first_sequence\":"+std::to_string(first)+",\"raw_last_sequence\":"+std::to_string(raw.serial)+",\"raw_trigger_qpc\":"+std::to_string(raw.observed_qpc)
        +",\"cursor\":"+point(proof.cursor)+delta_fields(proof.cursor)+named_geometry(target,"intended_")+named_geometry(before,"before_")+named_geometry(expected_live,"expected_before_")+",\"native_calls\":"+std::to_string(mismatch?1:0)+",\"end_qpc\":"+std::to_string(native_end_qpc.load())+proof.fields
        +(diagnostic_mode?",\"winevent_end_qpc\":"+std::to_string(winevent_end_qpc.load())+",\"winevent_end_sequence\":"+std::to_string(winevent_end_sequence.load())+",\"handoff_preflight_sequence\":"+std::to_string(final_preflight_sequence):"")
        +(isolation_mode?",\"actor\":\"source\",\"product_preflight_sequence\":"+std::to_string(product_preflight_sequence)+",\"input_isolation_ready_sequence\":"+std::to_string(isolation_ready_sequence.load())+",\"input_isolation_ready_qpc\":"+std::to_string(isolation_ready_qpc.load())+",\"writer_input_isolation_sequence\":"+std::to_string(writer_isolation_sequence):""));
    std::int64_t started{},returned{};BOOL succeeded=TRUE;DWORD error{};
    if(mismatch){
        // Serialize the owner commit against publication of terminal UP. The
        // independent receiver QPC remains authoritative: a physically earlier
        // UP cannot be made acceptable by a later parent publication/log row.
        std::lock_guard lock(continuation_mutex);require(!raw_up_seen,"raw_up_before_write");
        require(identity()&&GetForegroundWindow()==owned&&pressed(VK_LBUTTON)&&!other_input()&&takeover_healthy,"write_boundary_context_lost");
        if(diagnostic_mode)require(takeover_scope&&!cleanup_observation&&winevent_healthy&&log_ok,"write_boundary_evidence_lost");
        if(isolation_mode)require(shield_healthy&&isolation_ready_sequence,"TestInputIsolationUnavailable");
        inflight_target=target;active_operation=id;started=qpc();
        if(isolation_mode)require(ti::source_write_after_gates(started,native_end_qpc,winevent_end_qpc,isolation_ready_qpc),"source_write_before_isolation_ready");
        require(started>std::max(native_end_qpc.load(),diagnostic_mode?winevent_end_qpc.load():std::int64_t{0}),"write_before_native_end");SetLastError(0);++takeover_native_calls;
        succeeded=SetWindowPos(owned,nullptr,target.p.left,target.p.top,target.p.right-target.p.left,target.p.bottom-target.p.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
        error=succeeded?0:GetLastError();returned=qpc();expected_live=target;active_operation=0;
    }
    const auto immediate=capture(),full=capture();
    ops::MagnetPostverifyDiagnostic diagnostic;
    diagnostic.native_success=succeeded!=FALSE;diagnostic.win32_error=error;diagnostic.capture_succeeded=immediate.p_ok&&immediate.v_ok;
    diagnostic.requested_positioning=core_rect(target.p);diagnostic.requested_visible=core_rect(target.v);
    if(immediate.p_ok)diagnostic.actual_positioning=core_rect(immediate.p);if(immediate.v_ok)diagnostic.actual_visible=core_rect(immediate.v);
    diagnostic.other_members_exact=true;diagnostic.receipt_health=receiver_ok&&takeover_healthy&&native_drag_after_end==0&&unowned_geometry_changes==0;
    GUITHREADINFO post_gui{sizeof(post_gui)};
    diagnostic.source_context_exact=identity()&&desktop_available()&&IsWindowVisible(owned)&&GetForegroundWindow()==owned&&GetDpiForWindow(owned)==frozen_dpi&&MonitorFromWindow(owned,MONITOR_DEFAULTTONULL)==frozen_monitor
        &&GetGUIThreadInfo(ui_thread,&post_gui)&&!post_gui.hwndCapture&&!post_gui.hwndMenuOwner&&!post_gui.hwndMoveSize&&!(post_gui.flags&30)&&!other_input()&&!foreign_capture_transferred;
    diagnostic.classify();
    const bool exact=diagnostic.failure_class==ops::MagnetPostverifyFailure::None&&full.p_ok&&full.v_ok&&equal(full.p,target.p)&&equal(full.v,target.v);
    const auto postverify_sequence=record("writer_result",",\"operation_id\":"+std::to_string(id)+",\"quantum_id\":"+std::to_string(quantum)+",\"kind\":\""+kind+"\",\"native_calls\":"+std::to_string(mismatch?1:0)+",\"native_success\":"+flag(succeeded!=FALSE)+",\"error\":"+std::to_string(error)
        +",\"native_start_qpc\":"+std::to_string(started)+",\"native_return_qpc\":"+std::to_string(returned)+geometry(immediate)+",\"positioning_exact\":"+flag(diagnostic.positioning_exact)+",\"visible_exact\":"+flag(diagnostic.visible_exact)+",\"postverify_exact\":"+flag(exact)+",\"diagnostic\":"+ops::magnet_postverify_json(diagnostic)+named_geometry(full,"full_")+(isolation_mode?",\"actor\":\"source\"":""));
    require(exact,"takeover_exact_postverify_failed");expected_live=target;
    if(isolation_mode){require(postverify_sequence&&log_ok,"writer_evidence_failed");position_input_shield(full,id,postverify_sequence);}
    return {id,mismatch?1:0};
}
void process_handoff(){
    if(!diagnostic_mode)require(!handoff_ready&&takeover_scope&&native_end_qpc,"handoff_without_end_or_duplicate");
    OwnerProof proof;Geometry actual;
    if(diagnostic_mode){
        enable_winevent_audit();const auto s=capture_handoff_preflight();final_preflight_sequence=emit_handoff_preflight("winevent_end",s);
        require((isolation_mode?ti::classify_product(ti::product_facts(s.diagnostic)):hd::classify(s.diagnostic)).count==0,isolation_mode?"product_handoff_authority_failed":"handoff_preflight_failed");
        require(!handoff_ready&&takeover_scope&&native_end_qpc,"handoff_without_end_or_duplicate");proof=s.proof;actual=s.actual;
        if(isolation_mode){product_preflight_sequence=final_preflight_sequence;
            if(!s.diagnostic.cursor_success){const auto unavailable=capture_input_isolation(proof.cursor,false);record("input_isolation_point",",\"phase\":\"setup\",\"index\":2"+input_isolation_fields(unavailable));require(false,"TestInputIsolationUnavailable");}
            setup_input_isolation(proof.cursor,actual);}
    }else{
        proof=owner_proof(true);actual=capture();Geometry terminal;
        {std::lock_guard lock(continuation_mutex);terminal=terminal_geometry;}
        require(proof.healthy&&actual.p_ok&&actual.v_ok&&equal(actual.p,terminal.p)&&equal(actual.v,terminal.v),"handoff_terminal_stability_failed");
    }
    const auto target=intended_at(proof.cursor);
    record("handoff_begin",",\"end_sequence\":"+std::to_string(native_end_sequence.load())+",\"end_qpc\":"+std::to_string(native_end_qpc.load())+",\"current_cursor\":"+point(proof.cursor)+delta_fields(proof.cursor)+named_geometry(actual,"actual_handoff_")+named_geometry(target,"intended_")+",\"raw_watermark\":"+std::to_string(receiver_watermark.load())+proof.fields);
    const RawPacket no_raw;
    const auto receipt=write_intended("handoff",0,no_raw,0,proof);
    bool notify=false;
    {std::lock_guard lock(continuation_mutex);motion_pending=false;pending_count=0;consumed_motion=pending_motion.serial;
        handoff_ready=true;if(raw_up_seen&&!notice_posted){notice_posted=true;notify=true;}}
    record("handoff_complete",",\"operation_id\":"+std::to_string(receipt.id)+",\"native_calls\":"+std::to_string(receipt.native_calls)+",\"exact\":true,\"end_qpc\":"+std::to_string(native_end_qpc.load()));
    if(notify)require(post_source_work(raw_notice_message),"handoff_terminal_notice_failed");
    SetEvent(handoff_finished);
}
void process_raw_notice(){
    RawPacket motion,up;std::uint32_t first{},count{};bool ending=false,has_motion=false;
    {std::lock_guard lock(continuation_mutex);notice_posted=false;if(!takeover_scope)return;
        ending=raw_up_seen;up=published_up;
        if(!ending&&motion_pending){motion=pending_motion;first=pending_first;count=pending_count;has_motion=true;}
        motion_pending=false;pending_count=0;
    }
    if(ending){
        const auto proof=owner_proof(false);const auto target=intended_at(proof.cursor);const auto actual=capture();
        const bool exact=proof.healthy&&actual.p_ok&&actual.v_ok&&equal(actual.p,target.p)&&equal(actual.v,target.v)&&native_drag_after_end==0&&unowned_geometry_changes==0;
        const auto accepted=record("takeover_end",",\"raw_up_receiver_sequence\":"+std::to_string(up.serial)+",\"raw_up_receiver_qpc\":"+std::to_string(up.observed_qpc)+",\"final_cursor\":"+point(proof.cursor)+named_geometry(target,"intended_")+geometry(actual)+",\"exact\":"+flag(exact)+",\"pending_write\":false,\"pending_motion\":false,\"native_drag_after_end\":"+std::to_string(native_drag_after_end.load())+",\"unowned_geometry_changes\":"+std::to_string(unowned_geometry_changes.load())+proof.fields);
        takeover_scope=false;audit_live=false;require(exact,"takeover_final_geometry_failed");
        if(isolation_mode){require(accepted&&log_ok,"acceptance_evidence_failed");takeover_end_sequence=accepted;}
        SetEvent(takeover_finished);return;
    }
    if(!has_motion||!handoff_ready||motion.serial<=consumed_motion||motion.observed_qpc<=native_end_qpc)return;
    const auto quantum=++quantum_counter;const auto proof=owner_proof(true);
    const auto receipt=write_intended("raw_movement",quantum,motion,first,proof);consumed_motion=motion.serial;
    record("raw_quantum",",\"quantum_id\":"+std::to_string(quantum)+",\"raw_first_sequence\":"+std::to_string(first)+",\"raw_last_sequence\":"+std::to_string(motion.serial)+",\"raw_trigger_qpc\":"+std::to_string(motion.observed_qpc)+",\"coalesced_count\":"+std::to_string(count)+",\"cursor\":"+point(proof.cursor)+",\"operation_id\":"+std::to_string(receipt.id)+",\"native_calls\":"+std::to_string(receipt.native_calls));
    owner_processed_sequence=motion.serial;SetEvent(quantum_finished);
}
void process_final_acceptance(){
    require(!source_acceptance_sequence&&takeover_end_sequence,"duplicate_final_acceptance");
    const auto proof=owner_proof(false);const auto actual=capture(),target=intended_at(proof.cursor);RawPacket up;
    {std::lock_guard lock(continuation_mutex);up=published_up;require(raw_up_seen&&!motion_pending&&!pending_count,"final_pending_input");}
    const bool left=pressed(VK_LBUTTON);const bool exact=proof.healthy&&!left&&actual.p_ok&&actual.v_ok&&equal(actual.p,target.p)&&equal(actual.v,target.v)&&!takeover_scope&&!active_operation&&takeover_end_sequence;
    source_acceptance_sequence=record("source_final_acceptance",",\"takeover_end_sequence\":"+std::to_string(takeover_end_sequence)+",\"raw_up_receiver_sequence\":"+std::to_string(up.serial)+",\"raw_up_receiver_qpc\":"+std::to_string(up.observed_qpc)+",\"final_left_down\":"+flag(left)+",\"final_cursor\":"+point(proof.cursor)+",\"pending_write\":false,\"pending_motion\":false,\"exact\":"+flag(exact)+named_geometry(target,"intended_")+geometry(actual)+proof.fields);
    require(exact&&source_acceptance_sequence&&log_ok,"final_source_acceptance_failed");destroy_input_shield("acceptance",source_acceptance_sequence,up.observed_qpc);SetEvent(final_acceptance_finished);
}
void acquire_foreground(POINT& expected){
    if(set_foreground_success){
        try{foreground_ready();emit_bootstrap(true,"none");return;}
        catch(const std::exception& e){emit_bootstrap(false,e.what());throw;}
    }
    bootstrap.click_required=true;bootstrap.topmost_restored=false;
    try {
        require(identity()&&desktop_available(),"BLOCKED_BY_ACTIVATION_IDENTITY");
        require(!(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST),"BLOCKED_BY_ACTIVATION_VISIBILITY");
        const bool top=SetWindowPos(owned,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
        topmost_active=top;bootstrap.temporary_topmost=top&&(GetWindowLongPtrW(owned,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
        record("activation_visibility",",\"target\":"+std::to_string(number(owned))+",\"enabled\":true,\"success\":"+flag(top)+",\"flags\":83,\"topmost_style\":"+flag(bootstrap.temporary_topmost)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        require(bootstrap.temporary_topmost,"BLOCKED_BY_ACTIVATION_VISIBILITY");
        RECT client{};require(GetClientRect(owned,&client)!=FALSE&&client.right>0&&client.bottom>0,"BLOCKED_BY_ACTIVATION_HIT_TEST");
        activation_point={(client.left+client.right)/2,(client.top+client.bottom)/2};require(ClientToScreen(owned,&activation_point)!=FALSE,"BLOCKED_BY_ACTIVATION_HIT_TEST");
        ResetEvent(activation_event);ResetEvent(pointer_arrived);ResetEvent(activation_down_received);ResetEvent(activation_up_received);activate_seen=false;focus_seen=false;activation_armed=true;
        activation_input("move",MOUSEEVENTF_MOVE);
        require(WaitForSingleObject(pointer_arrived,2000)==WAIT_OBJECT_0,"BLOCKED_BY_ACTIVATION_FAILURE");
        ResetEvent(activation_event);activate_seen=false;focus_seen=false;
        activation_input("down",MOUSEEVENTF_LEFTDOWN);
        require(WaitForSingleObject(activation_down_received,2000)==WAIT_OBJECT_0,"BLOCKED_BY_ACTIVATION_FAILURE");
        activation_input("up",MOUSEEVENTF_LEFTUP);
        require(WaitForSingleObject(activation_up_received,2000)==WAIT_OBJECT_0,"BLOCKED_BY_ACTIVATION_FAILURE");
        // Queue-local activation callbacks are diagnostics, not authority.
        bootstrap.event_seen=WaitForSingleObject(activation_event,0)==WAIT_OBJECT_0;
        activation_armed=false;restore_topmost();
        require(bootstrap.move_success&&bootstrap.down_success&&bootstrap.up_success,"BLOCKED_BY_SENDINPUT");
        require(bootstrap.topmost_restored,"BLOCKED_BY_ACTIVATION_VISIBILITY");
        foreground_ready();
        require(GetCursorPos(&expected)!=FALSE&&std::abs(expected.x-activation_point.x)<=1&&std::abs(expected.y-activation_point.y)<=1,"BLOCKED_BY_INPUT_INTERFERENCE");emit_bootstrap(true,"none");
    }catch(const std::exception& e){if(!diagnostic_mode)release_activation_button();activation_armed=false;restore_topmost();emit_bootstrap(false,e.what());throw;}
}
POINT find_point(const RECT& r,int wanted){
    // Bounded 3 columns x 160 rows; hit-test decides, not an assumed title size.
    for(int offset=1;offset<=160;++offset)for(int part:{2,3,4}){
        POINT p{r.left+(r.right-r.left)*part/6,wanted==HTCAPTION?r.top+offset:r.bottom-offset};
        if(hit_test(p)==wanted&&GetAncestor(WindowFromPoint(p),GA_ROOT)==owned)return p;
    }
    throw std::runtime_error("BLOCKED_BY_HIT_TEST");
}
struct CleanupSnapshot {
    hd::CleanupInputDiagnostic facts;
    GUITHREADINFO gui{sizeof(gui)};POINT cursor{};HWND foreground{},root{};
    DWORD source_pid{},source_tid{},foreground_pid{},foreground_tid{},gui_error{},cursor_error{};
    bool stop_requested_snapshot{},source_retired_snapshot{},fixture_scope_active{true};
};
CleanupSnapshot capture_cleanup_input(){
    CleanupSnapshot s;auto& d=s.facts;
    if(isolation_mode){s.stop_requested_snapshot=stop_requested;s.source_retired_snapshot=source_retired;s.fixture_scope_active=!s.stop_requested_snapshot&&!s.source_retired_snapshot;}
    s.source_tid=GetWindowThreadProcessId(owned,&s.source_pid);d.own_identity=owned&&s.source_pid==GetCurrentProcessId()&&s.source_tid==ui_thread&&(!isolation_mode||isolation_identity(owned).nonce==run_nonce);
    d.desktop_ready=desktop_available();d.source_visible=d.own_identity&&IsWindowVisible(owned);
    s.foreground=GetForegroundWindow();s.foreground_tid=GetWindowThreadProcessId(s.foreground,&s.foreground_pid);
    d.foreground_matches=s.foreground==owned&&s.foreground_pid==GetCurrentProcessId()&&s.foreground_tid==ui_thread&&GetForegroundWindow()==s.foreground;
    SetLastError(0);d.gui_query_succeeded=GetGUIThreadInfo(ui_thread,&s.gui)!=FALSE;s.gui_error=d.gui_query_succeeded?0:GetLastError();
    d.no_foreign_capture=d.gui_query_succeeded&&(!s.gui.hwndCapture||s.gui.hwndCapture==owned);
    d.menu_clear=d.gui_query_succeeded&&!s.gui.hwndMenuOwner&&!(s.gui.flags&28);
    d.move_size_clear=d.gui_query_succeeded&&!s.gui.hwndMoveSize;d.gui_mode_clear=d.gui_query_succeeded&&!(s.gui.flags&30);
    SetLastError(0);d.cursor_success=GetCursorPos(&s.cursor)!=FALSE;s.cursor_error=d.cursor_success?0:GetLastError();
    s.root=d.cursor_success?GetAncestor(WindowFromPoint(s.cursor),GA_ROOT):nullptr;d.cursor_root_owned_or_guard=d.cursor_success&&input_root_owned(s.root);
    d.left_down=pressed(VK_LBUTTON);d.buttons_modifiers_clear=!other_input();d.foreign_capture_transferred=foreign_capture_transferred;
    return s;
}
std::string cleanup_input_fields(const CleanupSnapshot& s,bool test_down_owned){
    const auto& d=s.facts;
    return ",\"target\":"+std::to_string(number(owned))+",\"guard\":"+std::to_string(number(guard))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"actual_source_pid\":"+std::to_string(s.source_pid)+",\"actual_source_tid\":"+std::to_string(s.source_tid)
        +",\"test_down_owned\":"+flag(test_down_owned)+",\"own_identity\":"+flag(d.own_identity)+",\"desktop_ready\":"+flag(d.desktop_ready)+",\"source_visible\":"+flag(d.source_visible)+",\"foreground_hwnd\":"+std::to_string(number(s.foreground))+",\"foreground_matches\":"+flag(d.foreground_matches)+",\"foreground_pid\":"+std::to_string(s.foreground_pid)+",\"foreground_tid\":"+std::to_string(s.foreground_tid)
        +",\"gui_query_succeeded\":"+flag(d.gui_query_succeeded)+",\"gui_error\":"+std::to_string(s.gui_error)+",\"capture_hwnd\":"+std::to_string(number(s.gui.hwndCapture))+",\"no_foreign_capture\":"+flag(d.no_foreign_capture)+",\"menu_owner_hwnd\":"+std::to_string(number(s.gui.hwndMenuOwner))+",\"menu_clear\":"+flag(d.menu_clear)+",\"move_size_hwnd\":"+std::to_string(number(s.gui.hwndMoveSize))+",\"move_size_clear\":"+flag(d.move_size_clear)+",\"gui_flags\":"+std::to_string(s.gui.flags)+",\"gui_mode_clear\":"+flag(d.gui_mode_clear)
        +",\"cursor_success\":"+flag(d.cursor_success)+",\"cursor_error\":"+std::to_string(s.cursor_error)+",\"cursor\":"+(d.cursor_success?point(s.cursor):"null")+",\"cursor_root\":"+std::to_string(number(s.root))+",\"cursor_root_owned_or_guard\":"+flag(d.cursor_root_owned_or_guard)+",\"left_down\":"+flag(d.left_down)+",\"buttons_modifiers_clear\":"+flag(d.buttons_modifiers_clear)+",\"foreign_capture_transferred\":"+flag(d.foreign_capture_transferred)+",\"acceptance_eligible\":false"
        +(isolation_mode?",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_nonce\":"+std::to_string(isolation_identity(owned).nonce)+",\"stop_requested\":"+flag(s.stop_requested_snapshot)+",\"source_retired\":"+flag(s.source_retired_snapshot)+",\"fixture_scope_active\":"+flag(s.fixture_scope_active):"");
}
void perform_cleanup_input(bool test_down_owned){
    // Retire queued acceptance commands first, but retain the receiver until
    // the one cleanup stimulus and its independent observation are complete.
    takeover_scope=false;cleanup_observation=true;
    const auto s=capture_cleanup_input();const bool eligible=test_down_owned&&s.facts.eligible()&&(!isolation_mode||s.fixture_scope_active);
    // A held button with no proven test-owned DOWN is not permission to send
    // another UP, but it is also not a proven clean NOT_NEEDED state.
    const bool needed=s.facts.left_down;
    const auto context_reliable=[](const CleanupSnapshot& snapshot){const auto& d=snapshot.facts;
        return d.own_identity&&d.desktop_ready&&d.source_visible&&d.foreground_matches&&d.gui_query_succeeded&&d.no_foreign_capture&&d.menu_clear&&d.move_size_clear&&d.gui_mode_clear&&!d.foreign_capture_transferred&&(!isolation_mode||snapshot.fixture_scope_active);};
    record("cleanup_input_diagnostic",",\"phase\":\"initial\",\"eligible\":"+flag(eligible)+",\"outcome\":\""+(eligible?"ELIGIBLE":((needed||!context_reliable(s))?"SKIPPED_NO_AUTHORITY":"NOT_NEEDED"))+"\""+cleanup_input_fields(s,test_down_owned));
    bool attempted=false,sent_ok=false,up_observed=false;DWORD sent{},error{};DWORD wait=WAIT_TIMEOUT;
    std::int64_t started{},returned{};const auto watermark=receiver_watermark.load();RawPacket observed_up;
    if(eligible){
        const auto boundary=capture_cleanup_input();const bool same_cursor=boundary.facts.cursor_success&&s.facts.cursor_success&&std::abs(boundary.cursor.x-s.cursor.x)<=1&&std::abs(boundary.cursor.y-s.cursor.y)<=1;
        const bool safe=test_down_owned&&boundary.facts.eligible()&&same_cursor&&(!isolation_mode||(boundary.fixture_scope_active&&fresh_synthetic_input_isolation(boundary.cursor,MOUSEEVENTF_LEFTUP,"cleanup")));
        record("cleanup_input_diagnostic",",\"phase\":\"api_boundary\",\"eligible\":"+flag(safe)+",\"cursor_matches_initial\":"+flag(same_cursor)+",\"outcome\":\""+(safe?"ELIGIBLE":"SKIPPED_NO_AUTHORITY")+"\""+cleanup_input_fields(boundary,test_down_owned));
        if(safe){
            // Bootstrap may have failed before acceptance armed the receiver.
            // This is a cleanup-only observation epoch, never an accepted UP.
            require(receiver_armed&&SetEvent(receiver_armed),"cleanup_receiver_arm_failed");
            ResetEvent(raw_up);INPUT input=mouse_input(MOUSEEVENTF_LEFTUP,boundary.cursor);started=qpc();attempted=true;if(reliability_mode)abort_cleanup_attempted=true;SetLastError(0);sent=SendInput(1,&input,sizeof(input));error=sent==1?0:GetLastError();returned=qpc();sent_ok=sent==1;
            record("cleanup_release",",\"cleanup_up_attempted\":true,\"cleanup_up_sent\":"+flag(sent_ok)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"flags\":4,\"input_tag\":"+std::to_string(input_tag)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"injection_start_qpc\":"+std::to_string(started)+",\"injection_return_qpc\":"+std::to_string(returned)+cleanup_input_fields(boundary,test_down_owned));
            if(sent_ok&&raw_up){wait=WaitForSingleObject(raw_up,2000);if(wait==WAIT_OBJECT_0){std::lock_guard lock(raw_mutex);observed_up=last_up;
                up_observed=receiver_ok&&observed_up.serial>watermark&&observed_up.observed_qpc>=started&&observed_up.tag_matches&&observed_up.input_code==RIM_INPUTSINK&&observed_up.foreground==number(owned)&&observed_up.foreground_pid==GetCurrentProcessId()&&(observed_up.buttons&RI_MOUSE_LEFT_BUTTON_UP);}}
            record("cleanup_raw_up",",\"raw_up_observed\":"+flag(up_observed)+",\"wait_result\":"+std::to_string(wait)+",\"timeout_ms\":2000,\"receiver_sequence\":"+std::to_string(observed_up.serial)+",\"receiver_qpc\":"+std::to_string(observed_up.observed_qpc)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"injection_start_qpc\":"+std::to_string(started)+",\"acceptance_eligible\":false");
        }else record("cleanup_skipped_no_authority",",\"reason\":\"cleanup_boundary_context_changed\",\"acceptance_eligible\":false");
    }else if(needed)record("cleanup_skipped_no_authority",",\"reason\":\"cleanup_initial_authority_failed\",\"acceptance_eligible\":false");
    const auto final=capture_cleanup_input();
    bool final_isolation=true;
    if(isolation_mode){const auto proof=capture_input_isolation(final.cursor,final.facts.cursor_success);final_isolation=proof.facts.passed()&&shield_healthy;
        record("cleanup_final_isolation",",\"phase\":\"final\",\"input_scope\":\"cleanup\",\"acceptance_eligible\":false"+input_isolation_fields(proof));}
    const bool released=sent_ok&&up_observed&&!final.facts.left_down&&final.facts.own_identity&&final.facts.desktop_ready&&final.facts.source_visible&&final.facts.foreground_matches&&final.facts.gui_query_succeeded&&!final.gui.hwndCapture&&final.facts.menu_clear&&final.facts.move_size_clear&&final.facts.gui_mode_clear&&final.facts.cursor_success&&final.facts.cursor_root_owned_or_guard&&final.facts.buttons_modifiers_clear&&!final.facts.foreign_capture_transferred&&final_isolation&&(!isolation_mode||final.fixture_scope_active);
    const bool no_release_needed=context_reliable(s)&&context_reliable(final)&&!s.facts.left_down&&!final.facts.left_down;
    const char* result=attempted?(released?"PASS":"FAILED"):(no_release_needed?"NOT_NEEDED":"SKIPPED_NO_AUTHORITY");
    if(!attempted&&!no_release_needed&&!needed)record("cleanup_skipped_no_authority",",\"reason\":\"cleanup_no_reliable_final_no_button_proof\",\"acceptance_eligible\":false");
    record("cleanup_final",",\"cleanup_input_release\":\""+std::string(result)+"\",\"cleanup_up_attempted\":"+flag(attempted)+",\"cleanup_up_sent\":"+flag(sent_ok)+",\"raw_up_observed\":"+flag(up_observed)+",\"left_button_high_bit\":"+flag(final.facts.left_down)+",\"final_capture_hwnd\":"+(final.facts.gui_query_succeeded?std::to_string(number(final.gui.hwndCapture)):"null")+",\"final_cursor\":"+(final.facts.cursor_success?point(final.cursor):"null")+cleanup_input_fields(final,test_down_owned));
    cleanup_observation=false;ResetEvent(receiver_armed);
}
struct AbortSnapshot {
    ab::AbortAuthority facts;DownLedger ledger;
    std::int64_t start{},finish{};DWORD actual_pid{},actual_tid{},fg_pid{},fg_tid{},gui_error{},cursor_error{};
    std::uintptr_t actual_nonce{};bool nonce_evaluated{},gui_evaluated{},buttons_evaluated{},cursor_evaluated{},swapped{};
    HWND foreground{},root{};GUITHREADINFO gui{sizeof(gui)};POINT cursor{};std::array<bool,10> others{};
};
AbortSnapshot capture_abort_authority(const AbortSnapshot* initial=nullptr){
    AbortSnapshot s;s.start=qpc();auto& d=s.facts;
    d.fixture_scope_active=!stop_requested&&!source_retired;
    s.actual_tid=GetWindowThreadProcessId(owned,&s.actual_pid);
    if(s.actual_pid==GetCurrentProcessId()&&s.actual_tid==ui_thread){s.nonce_evaluated=true;s.actual_nonce=static_cast<std::uintptr_t>(GetWindowLongPtrW(owned,GWLP_USERDATA));}
    d.own_identity=owned&&s.actual_pid==GetCurrentProcessId()&&s.actual_tid==ui_thread&&s.nonce_evaluated&&s.actual_nonce==run_nonce;
    d.desktop_ready=desktop_available();d.source_visible=d.own_identity&&IsWindowVisible(owned);
    s.foreground=GetForegroundWindow();s.fg_tid=GetWindowThreadProcessId(s.foreground,&s.fg_pid);
    d.foreground_matches=s.foreground==owned&&s.fg_pid==GetCurrentProcessId()&&s.fg_tid==ui_thread&&GetForegroundWindow()==s.foreground;
    if(d.desktop_ready&&d.own_identity&&d.foreground_matches){
        s.gui_evaluated=true;SetLastError(0);d.gui_query_succeeded=GetGUIThreadInfo(ui_thread,&s.gui)!=FALSE;s.gui_error=d.gui_query_succeeded?0:GetLastError();
        d.capture_is_source=d.gui_query_succeeded&&s.gui.hwndCapture==owned;d.move_size_is_source=d.gui_query_succeeded&&s.gui.hwndMoveSize==owned;
        d.expected_native_mode=d.gui_query_succeeded&&(s.gui.flags&30)==GUI_INMOVESIZE;
        d.menu_clear=d.gui_query_succeeded&&!s.gui.hwndMenuOwner&&!(s.gui.flags&28);
        s.cursor_evaluated=true;SetLastError(0);d.cursor_available=GetCursorPos(&s.cursor)!=FALSE;s.cursor_error=d.cursor_available?0:GetLastError();
        if(d.cursor_available)s.root=GetAncestor(WindowFromPoint(s.cursor),GA_ROOT);d.root_is_source=d.cursor_available&&s.root==owned;
        if(d.gui_query_succeeded){
            s.buttons_evaluated=true;d.left_down=pressed(VK_LBUTTON);constexpr int keys[]={VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_ESCAPE};
            for(std::size_t i=0;i<s.others.size();++i)s.others[i]=pressed(keys[i]);d.other_input_clear=std::none_of(s.others.begin(),s.others.end(),[](bool v){return v;});
        }
    }
    s.swapped=GetSystemMetrics(SM_SWAPBUTTON)!=0;d.input_mapping_supported=!s.swapped;
    s.ledger=ledger_snapshot();d.ledger=s.ledger.facts;d.receiver_healthy=receiver_ok;d.log_healthy=log_ok;d.foreign_capture_transferred=foreign_capture_transferred;
    d.writer_quiescent=abort_quiescent&&abort_quiescence_sequence!=0;d.acceptance_retired=abort_retiring&&!takeover_scope;
    d.api_boundary_stable=!initial||(d.cursor_available&&initial->facts.cursor_available&&s.cursor.x==initial->cursor.x&&s.cursor.y==initial->cursor.y&&s.ledger.facts.down_input_sequence==initial->ledger.facts.down_input_sequence&&s.foreground==initial->foreground&&s.gui.hwndCapture==initial->gui.hwndCapture&&s.gui.hwndMoveSize==initial->gui.hwndMoveSize&&s.gui.flags==initial->gui.flags);
    s.finish=qpc();return s;
}
std::uint64_t emit_abort_authority(const char* phase,const AbortSnapshot& s,std::uint64_t initial_authority,std::uint64_t initial_ack){
    const auto& d=s.facts;const auto nullable=[](bool known,bool value){return known?flag(value):"null";};const auto handle=[](bool known,HWND h){return known?std::to_string(number(h)):"null";};
    std::string other="{";constexpr const char* names[]={"ctrl","shift","alt","lwin","rwin","rbutton","mbutton","xbutton1","xbutton2","escape"};
    for(std::size_t i=0;i<s.others.size();++i){if(i)other+=",";other+="\""+std::string(names[i])+"\":"+nullable(s.buttons_evaluated,s.others[i]);}other+="}";
    return record("abort_cleanup_authority",",\"phase\":\""+std::string(phase)+"\",\"query_start_qpc\":"+std::to_string(s.start)+",\"query_finish_qpc\":"+std::to_string(s.finish)
        +",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)+",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_pid\":"+std::to_string(s.actual_pid)+",\"actual_source_tid\":"+std::to_string(s.actual_tid)+",\"actual_source_nonce\":"+(s.nonce_evaluated?std::to_string(s.actual_nonce):"null")
        +",\"fixture_scope_active\":"+flag(d.fixture_scope_active)+",\"own_identity\":"+flag(d.own_identity)+",\"desktop_ready\":"+flag(d.desktop_ready)+",\"source_visible\":"+(d.own_identity?flag(d.source_visible):"null")+",\"foreground_hwnd\":"+std::to_string(number(s.foreground))+",\"foreground_pid\":"+std::to_string(s.fg_pid)+",\"foreground_tid\":"+std::to_string(s.fg_tid)+",\"foreground_matches\":"+flag(d.foreground_matches)
        +",\"gui_query_succeeded\":"+nullable(s.gui_evaluated,d.gui_query_succeeded)+",\"gui_error\":"+(s.gui_evaluated?std::to_string(s.gui_error):"null")+",\"capture_hwnd\":"+handle(d.gui_query_succeeded,s.gui.hwndCapture)+",\"menu_owner_hwnd\":"+handle(d.gui_query_succeeded,s.gui.hwndMenuOwner)+",\"move_size_hwnd\":"+handle(d.gui_query_succeeded,s.gui.hwndMoveSize)+",\"gui_flags\":"+(d.gui_query_succeeded?std::to_string(s.gui.flags):"null")
        +",\"expected_native_mode\":"+nullable(d.gui_query_succeeded,d.expected_native_mode)+",\"menu_clear\":"+nullable(d.gui_query_succeeded,d.menu_clear)+",\"cursor_success\":"+nullable(s.cursor_evaluated,d.cursor_available)+",\"cursor_error\":"+(s.cursor_evaluated?std::to_string(s.cursor_error):"null")+",\"cursor\":"+(d.cursor_available?point(s.cursor):"null")+",\"cursor_root\":"+handle(d.cursor_available,s.root)+",\"root_is_source\":"+nullable(d.cursor_available,d.root_is_source)
        +",\"left_down\":"+nullable(s.buttons_evaluated,d.left_down)+",\"other_states\":"+other+",\"other_input_clear\":"+nullable(s.buttons_evaluated,d.other_input_clear)+",\"receiver_healthy\":"+flag(d.receiver_healthy)+",\"log_healthy\":"+flag(d.log_healthy)+",\"foreign_capture_transferred\":"+flag(d.foreign_capture_transferred)+",\"mouse_buttons_swapped\":"+flag(s.swapped)+",\"input_mapping_supported\":"+flag(d.input_mapping_supported)
        +",\"writer_quiescent\":"+flag(d.writer_quiescent)+",\"acceptance_retired\":"+flag(d.acceptance_retired)+",\"quiescence_ack_sequence\":"+std::to_string(abort_quiescence_sequence.load())+",\"initial_ack_sequence\":"+std::to_string(initial_ack)+",\"initial_authority_sequence\":"+std::to_string(initial_authority)+",\"api_boundary_stable\":"+flag(d.api_boundary_stable)+",\"eligible\":"+flag(d.eligible())+",\"acceptance_eligible\":false"+ledger_fields(s.ledger));
}
void perform_reliability_cleanup(bool test_down_owned){
    abort_retiring=true;takeover_scope=false;cleanup_observation=true;driver_phase="abort_cleanup";
    // Preserve the former strict source/shield cleanup path outside the new
    // native-active extension. Do not fake clear Product fields for either.
    const bool initial_quiescent=request_abort_quiescence("initial");const auto initial_ack=abort_quiescence_sequence.load();
    if(!native_enter_sequence||native_end_sequence){
        if(initial_quiescent)perform_cleanup_input(test_down_owned);
        else{abort_cleanup_result="SKIPPED_NO_AUTHORITY";record("abort_cleanup_skipped",",\"reason\":\"legacy_cleanup_missing_owner_quiescence\",\"acceptance_eligible\":false");ResetEvent(receiver_armed);}
        request_abort_quiescence("final");return;
    }
    const auto initial=capture_abort_authority();const auto initial_ref=emit_abort_authority("initial",initial,0,initial_ack);
    bool attempted=false,sent_ok=false,up_observed=false;RawPacket observed;std::uint64_t boundary_ref{};std::int64_t injection_start{};std::uint32_t watermark{};
    if(initial_quiescent&&initial.facts.eligible()&&initial_ref&&log_ok){
        const auto boundary=capture_abort_authority(&initial);boundary_ref=emit_abort_authority("api_boundary",boundary,initial_ref,initial_ack);
        if(boundary.facts.eligible()&&boundary_ref&&log_ok){
            ResetEvent(raw_up);watermark=receiver_watermark;
            const auto intent=begin_ledger_intent(MOUSEEVENTF_LEFTUP,abort_cleanup_tag,watermark);
            INPUT up=mouse_input(MOUSEEVENTF_LEFTUP,boundary.cursor);up.mi.dwExtraInfo=abort_cleanup_tag;
            require(ledger_snapshot().facts.pending_owned()&&receiver_ok&&log_ok&&!stop_requested&&!source_retired&&identity()&&GetForegroundWindow()==owned,"abort_api_boundary_ledger_or_health_changed");
            injection_start=qpc();ledger_api_started(intent,injection_start);attempted=true;abort_cleanup_attempted=true;SetLastError(0);
            const auto sent=SendInput(1,&up,sizeof(up));const auto error=sent==1?0:GetLastError();const auto returned=qpc();sent_ok=sent==1;
            const auto release=record("abort_cleanup_release",",\"flags\":4,\"actual_mouse_flags\":4,\"normalized_dx\":0,\"normalized_dy\":0,\"input_tag\":"+std::to_string(abort_cleanup_tag)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"injection_start_qpc\":"+std::to_string(injection_start)+",\"injection_return_qpc\":"+std::to_string(returned)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"initial_authority_sequence\":"+std::to_string(initial_ref)+",\"boundary_authority_sequence\":"+std::to_string(boundary_ref)+",\"ledger_intent_sequence\":"+std::to_string(intent)+",\"acceptance_eligible\":false");
            ledger_api_committed(intent,release,sent,injection_start,returned);
            const auto wait=sent_ok?WaitForSingleObject(raw_up,2000):WAIT_FAILED;
            if(wait==WAIT_OBJECT_0){std::lock_guard lock(raw_mutex);observed=last_up;up_observed=receiver_ok&&!observed.device_present&&observed.serial>watermark&&observed.observed_qpc>=injection_start&&observed.input_code==RIM_INPUTSINK&&observed.foreground==number(owned)&&observed.foreground_pid==GetCurrentProcessId()&&observed.cleanup_tag_matches&&observed.extra_information==abort_cleanup_tag&&observed.buttons==RI_MOUSE_LEFT_BUTTON_UP;}
            record("abort_cleanup_raw_up",",\"receiver_sequence\":"+std::to_string(observed.serial)+",\"receiver_qpc\":"+std::to_string(observed.observed_qpc)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"injection_start_qpc\":"+std::to_string(injection_start)+",\"input_tag\":"+std::to_string(abort_cleanup_tag)+",\"wait_result\":"+std::to_string(wait)+",\"timeout_ms\":2000,\"raw_up_observed\":"+flag(up_observed)+",\"acceptance_eligible\":false");
        }else record("abort_cleanup_skipped",",\"reason\":\"fresh_api_boundary_authority_failed\",\"acceptance_eligible\":false");
    }else record("abort_cleanup_skipped",",\"reason\":\"initial_authority_or_quiescence_failed\",\"acceptance_eligible\":false");
    const auto native_wait=sent_ok?WaitForSingleObject(exited,2000):WAIT_FAILED;
    const auto win_wait=sent_ok?WaitForSingleObject(winevent_finished,2000):WAIT_FAILED;
    record("abort_cleanup_end_wait",",\"native_wait_result\":"+std::to_string(native_wait)+",\"winevent_wait_result\":"+std::to_string(win_wait)+",\"timeout_ms\":2000,\"native_exit_sequence\":"+std::to_string(native_end_sequence.load())+",\"native_exit_qpc\":"+std::to_string(native_end_qpc.load())+",\"winevent_end_sequence\":"+std::to_string(winevent_end_sequence.load())+",\"winevent_end_qpc\":"+std::to_string(winevent_end_qpc.load())+",\"acceptance_eligible\":false");
    const bool final_quiescent=request_abort_quiescence("final");const auto final=capture_abort_authority();const auto final_ref=emit_abort_authority("final",final,initial_ref,initial_ack);
    const bool context=final.facts.fixture_scope_active&&final.facts.own_identity&&final.facts.desktop_ready&&final.facts.source_visible&&final.facts.foreground_matches&&final.facts.gui_query_succeeded&&final.facts.cursor_available&&final.facts.root_is_source&&final.facts.other_input_clear&&final.facts.receiver_healthy&&final.facts.log_healthy&&!final.facts.foreign_capture_transferred&&final.facts.input_mapping_supported;
    ab::AbortCompletion completion{attempted,sent_ok,up_observed,native_wait==WAIT_OBJECT_0&&native_end_sequence!=0,win_wait==WAIT_OBJECT_0&&winevent_end_sequence!=0,context,
        final.facts.gui_query_succeeded&&!final.gui.hwndCapture,final.facts.gui_query_succeeded&&!final.gui.hwndMoveSize&&!final.gui.hwndMenuOwner&&!(final.gui.flags&30),final.buttons_evaluated&&!final.facts.left_down,
        final.ledger.facts.up_command_sent&&final.ledger.facts.matching_up_seen&&!final.ledger.facts.non_test_button_transitions&&!final.ledger.facts.unmatched_button_transitions,final_quiescent&&final.facts.writer_quiescent,final.facts.acceptance_retired,final_quiescent,true};
    abort_cleanup_result=std::string(completion.result());
    record("abort_cleanup_final",",\"cleanup_result\":\""+abort_cleanup_result+"\",\"cleanup_up_attempted\":"+flag(attempted)+",\"cleanup_up_sent\":"+flag(sent_ok)+",\"raw_up_observed\":"+flag(up_observed)+",\"native_exit_observed\":"+flag(completion.native_exit_observed)+",\"winevent_end_observed\":"+flag(completion.winevent_end_matches)+",\"final_authority_sequence\":"+std::to_string(final_ref)+",\"initial_ack_sequence\":"+std::to_string(initial_ack)+",\"quiescence_ack_sequence\":"+std::to_string(abort_quiescence_sequence.load())
        +",\"final_left_down\":"+(final.buttons_evaluated?flag(final.facts.left_down):"null")+",\"final_capture_clear\":"+flag(completion.final_capture_clear)+",\"final_mode_clear\":"+flag(completion.final_mode_clear)+",\"ledger_settled\":"+flag(completion.ledger_settled)+",\"writer_quiescent\":"+flag(completion.writer_quiescent)+",\"acceptance_retired\":"+flag(completion.acceptance_retired)+",\"pending_work\":"+flag(!final_quiescent)+",\"acceptance_eligible\":false");
    cleanup_observation=false;ResetEvent(receiver_armed);
}
void drive_end_gesture(POINT& expected,bool& down){
    if(reliability_mode){driver_phase="native_start";driver_sample=0;}
    gesture=selected_gesture;cancelled=false;cancel_pending=false;cancel_return_boundary=0;native_drag_after_return=0;
    ResetEvent(entered);ResetEvent(exited);ResetEvent(stepped);ResetEvent(nonclient_down);ResetEvent(raw_up);
    const auto initial=capture();require(initial.p_ok&&initial.v_ok&&contained(initial.p),"setup_geometry_unavailable");
    const int wanted=selected_gesture==1?HTCAPTION:HTBOTTOM;const POINT start=find_point(initial.p,wanted);
    fence(expected,false);inject(MOUSEEVENTF_MOVE,start);expected=start;pace(interval_ms);
    fence(expected,false);require(hit_test(start)==wanted&&GetAncestor(WindowFromPoint(start),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
    const POINT end{start.x+(selected_gesture==1?180:0),start.y+(selected_gesture==2?120:0)};
    record("path",",\"hit_test\":"+std::to_string(wanted)+",\"start\":"+point(start)+",\"end\":"+point(end)+",\"samples\":20,\"cancel_after_sample\":2,\"interval_ms\":30,\"foreground\":"+std::to_string(number(GetForegroundWindow()))+geometry(initial));
    {std::lock_guard lock(continuation_mutex);pending_motion={};published_up={};motion_pending=false;raw_up_seen=false;notice_posted=false;pending_count=0;}
    if(isolation_mode){std::lock_guard lock(continuation_mutex);planned_current_cursor={start.x+(end.x-start.x)*2/samples,start.y+(end.y-start.y)*2/samples};for(int sample=3;sample<=samples;++sample)remaining_input_points[static_cast<std::size_t>(sample-3)]={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};}
    takeover_scope=true;
    if(diagnostic_mode){
        gesture_arm_tick=GetTickCount();gesture_arm_qpc=qpc();active_gesture_id=1;
        record("gesture_armed",",\"gesture_id\":1,\"arm_qpc\":"+std::to_string(gesture_arm_qpc.load())+",\"arm_tick\":"+std::to_string(gesture_arm_tick.load())+",\"source_hwnd\":"+std::to_string(number(owned))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread));
    }
    inject(MOUSEEVENTF_LEFTDOWN,expected);down=true;
    require(WaitForSingleObject(nonclient_down,2000)==WAIT_OBJECT_0,"missing_native_mouse_down");
    for(int sample=1;sample<=2;++sample){
        if(reliability_mode){driver_phase="native_sample_before";driver_sample=sample;}
        pace(interval_ms);fence(expected,true,sample==1&&WaitForSingleObject(entered,0)!=WAIT_OBJECT_0,false);
        ResetEvent(stepped);const int before=callbacks;
        expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
        ResetEvent(raw_motion);const auto receipt=inject(MOUSEEVENTF_MOVE,expected);wait_raw_motion(expected,receipt);
        if(sample==1){require(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"missing_ENTER");if(reliability_mode){const auto ledger=ledger_snapshot();if(ledger.raw_down_recorded&&ledger.facts.native_down_matches&&ledger.facts.native_enter_matches&&ledger.facts.raw_down_matches)record("test_down_ledger",ledger_fields(ledger));}}
        require(WaitForSingleObject(stepped,2000)==WAIT_OBJECT_0&&callbacks>before,"missing_DRAG");
        pace(interval_ms);if(reliability_mode)driver_phase="native_sample_after";fence(expected,true);
        const auto sample_sequence=record("sample",",\"index\":"+std::to_string(sample)+",\"cursor\":"+point(expected)+",\"left_down\":true,\"post_cancel\":false"+geometry(capture()));
        if(reliability_mode&&controlled_abort&&sample==1){
            const auto ledger=ledger_snapshot();const bool clean=ledger.facts.pending_owned()&&!ledger.non_test_movements&&receiver_ok&&log_ok;
            record("controlled_abort_fault",",\"sample_sequence\":"+std::to_string(sample_sequence)+",\"native_enter_sequence\":"+std::to_string(native_enter_sequence.load())+",\"native_down_sequence\":"+std::to_string(native_down_sequence.load())+",\"non_test_movements\":"+std::to_string(ledger.non_test_movements)+",\"non_test_button_transitions\":"+std::to_string(ledger.facts.non_test_button_transitions)+",\"unmatched_button_transitions\":"+std::to_string(ledger.facts.unmatched_button_transitions)+",\"guard_passed\":"+flag(clean)+",\"fault\":\"after_sample1_before_cancel\",\"reason\":\""+(clean?"BLOCKED_BY_TEST_FAULT":"BLOCKED_BY_UNEXPECTED_INPUT_DURING_TEST_FAULT")+"\"");
            require(clean,"BLOCKED_BY_UNEXPECTED_INPUT_DURING_TEST_FAULT");require(false,"BLOCKED_BY_TEST_FAULT");
        }
    }
    Geometry anchored;POINT pointer{};{std::lock_guard lock(continuation_mutex);anchored=intent_start;pointer=intent_pointer;}
    require(anchored.p_ok&&anchored.v_ok&&equal(anchored.p,initial.p)&&equal(anchored.v,initial.v)&&pointer.x==start.x&&pointer.y==start.y,"native_start_anchor_mismatch");
    GUITHREADINFO gui{sizeof(gui)};require(GetGUIThreadInfo(ui_thread,&gui)&&gui.hwndCapture==owned,"cancel_capture_before_failed");fence(expected,true);
    cancelled=true;const auto issued=qpc();
    record("cancel_begin",",\"target\":"+std::to_string(number(owned))+",\"source_tid\":"+std::to_string(ui_thread)+",\"capture_before\":"+std::to_string(number(gui.hwndCapture))+",\"left_down\":true");
    DWORD_PTR recipient{};SetLastError(0);
    const auto sent=SendMessageTimeoutW(owned,WM_CANCELMODE,0,0,SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,1000,&recipient);const auto error=sent?0:GetLastError();const auto returned=qpc();cancel_return_boundary=returned;
    const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    record("cancel_return",",\"transport_success\":"+flag(sent!=0)+",\"error\":"+std::to_string(error)+",\"recipient_result\":"+std::to_string(recipient)+",\"issued_qpc\":"+std::to_string(issued)+",\"returned_qpc\":"+std::to_string(returned)+",\"gui_query_succeeded\":"+flag(query)+",\"capture_after\":"+std::to_string(number(gui.hwndCapture))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(capture()));
    require(sent!=0,"cancel_transport_failed");require(query&&!gui.hwndCapture,"cancel_capture_not_released");
    const auto wait_start=qpc();const auto wait=WaitForSingleObject(exited,2000);const auto wait_end=qpc();
    record("end_wait",",\"started_qpc\":"+std::to_string(wait_start)+",\"finished_qpc\":"+std::to_string(wait_end)+",\"timeout_ms\":2000,\"wait_result\":"+std::to_string(wait));
    if(!diagnostic_mode){require(wait==WAIT_OBJECT_0,"missing_native_END");require(native_drag_after_return==0&&native_drag_after_end==0,"native_drag_after_cancel_or_end");}
    if(diagnostic_mode){
        require(post_source_work(native_preflight_message),"native_preflight_post_failed");
        require(WaitForSingleObject(native_preflight_finished,2000)==WAIT_OBJECT_0,"native_preflight_observation_failed");
        const auto begin=qpc();const auto win_wait=WaitForSingleObject(winevent_finished,3000);const auto finish=qpc();
        record("winevent_end_wait",",\"started_qpc\":"+std::to_string(begin)+",\"finished_qpc\":"+std::to_string(finish)+",\"timeout_ms\":3000,\"wait_result\":"+std::to_string(win_wait)+",\"winevent_end_observed\":"+flag(winevent_end_qpc!=0)+",\"winevent_healthy\":"+flag(winevent_healthy));
        // Even a missing witness receives one complete final diagnostic; no
        // timer retry and no write can follow its MissingWinEventEnd class.
    }
    require(post_source_work(handoff_message),"handoff_post_failed");
    require(WaitForSingleObject(handoff_finished,2000)==WAIT_OBJECT_0&&handoff_ready&&takeover_healthy,"handoff_completion_failed");
    for(int sample=3;sample<=samples;++sample){
        if(reliability_mode){driver_phase="takeover_sample";driver_sample=sample;}
        pace(interval_ms);fence(expected,true,false,true);require(takeover_healthy,"takeover_observer_failure");
        expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
        ResetEvent(raw_motion);ResetEvent(quantum_finished);const auto receipt=inject(MOUSEEVENTF_MOVE,expected);
        wait_raw_motion(expected,receipt);
        require(WaitForSingleObject(quantum_finished,2000)==WAIT_OBJECT_0&&takeover_healthy,"raw_quantum_completion_failed");
        require(owner_processed_sequence>receipt.watermark,"raw_quantum_receipt_mismatch");
        pace(interval_ms);fence(expected,true,false,true);POINT current{};require(GetCursorPos(&current),"sample_cursor_failed");
        const auto intended=intended_at(current),actual=capture();const bool exact=actual.p_ok&&actual.v_ok&&equal(actual.p,intended.p)&&equal(actual.v,intended.v);
        record("sample",",\"index\":"+std::to_string(sample)+",\"cursor\":"+point(current)+",\"left_down\":true,\"post_cancel\":true,\"intended_positioning\":"+rect(intended.p)+",\"intended_visible\":"+rect(intended.v)+",\"exact\":"+flag(exact)+geometry(actual));
        require(exact,"takeover_cursor_authority_lag_or_geometry_mismatch");
    }
    fence(expected,true,false,true);const auto up=inject(MOUSEEVENTF_LEFTUP,expected);down=false;wait_raw_up(up);
    require(WaitForSingleObject(takeover_finished,2000)==WAIT_OBJECT_0&&takeover_healthy,"takeover_end_failed");
    pace(interval_ms);fence(expected,false,false,true);record("path_complete",geometry(capture())+(isolation_mode?",\"final_left_down\":"+flag(pressed(VK_LBUTTON)):""));
    if(isolation_mode){require(post_source_work(final_acceptance_message),"final_acceptance_post_failed");require(WaitForSingleObject(final_acceptance_finished,2000)==WAIT_OBJECT_0&&takeover_healthy&&shield_healthy,"final_acceptance_failed");}
}
void drive(){
    bool down=false;POINT expected=saved_cursor;
    try {
        acquire_foreground(expected);if(reliability_mode)driver_phase="raw_preflight";fence(expected,false);
        SetEvent(receiver_armed);
        // Prove real background mouse movement and UP before native cancel.
        RECT client{};require(GetClientRect(owned,&client),"preflight_client_failed");
        POINT centre{client.right/2,client.bottom/2};require(ClientToScreen(owned,&centre),"preflight_client_failed");
        if(std::abs(centre.x-expected.x)<=1&&std::abs(centre.y-expected.y)<=1)centre.x+=12;
        require(hit_test(centre)==HTCLIENT&&GetAncestor(WindowFromPoint(centre),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
        record("raw_preflight_begin",",\"point\":"+point(centre));
        ResetEvent(raw_motion);const auto preflight_move=inject(MOUSEEVENTF_MOVE,centre);expected=centre;
        wait_raw_motion(expected,preflight_move);pace(interval_ms);fence(expected,false);
        ResetEvent(raw_up);inject(MOUSEEVENTF_LEFTDOWN,expected);down=true;pace(interval_ms);fence(expected,true);
        POINT next{centre.x+12,centre.y};require(hit_test(next)==HTCLIENT&&GetAncestor(WindowFromPoint(next),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
        ResetEvent(raw_motion);const auto held_request=inject(MOUSEEVENTF_MOVE,next);expected=next;
        wait_raw_motion(expected,held_request);pace(interval_ms);fence(expected,true);
        const auto preflight_up=inject(MOUSEEVENTF_LEFTUP,expected);down=false;
        const auto up_started=qpc();const auto up_wait=WaitForSingleObject(raw_up,2000);const auto up_finished=qpc();
        if(up_wait==WAIT_OBJECT_0)wait_raw_up(preflight_up);
        // A completed actual UP stimulus needs an independent final fence even
        // when the raw witness is absent. Do not invent a packet or raw END.
        pace(interval_ms);fence(expected,false);
        record("raw_preflight_outcome",",\"attempted\":true,\"actual_move_verified\":true,\"actual_held_move_verified\":true,\"actual_up_verified\":true,\"input_delivery_verified\":true,\"observation_completed\":true,\"receiver_healthy\":"+flag(receiver_ok)
            +",\"up_wait_result\":"+std::to_string(up_wait)+",\"up_wait_started_qpc\":"+std::to_string(up_started)+",\"up_wait_finished_qpc\":"+std::to_string(up_finished)+",\"up_timeout_ms\":2000,\"raw_up_observed\":"+flag(up_wait==WAIT_OBJECT_0));
        require(receiver_ok,"BLOCKED_BY_RAW_RECEIVER_ERROR");require(up_wait==WAIT_OBJECT_0,"RAW_PREFLIGHT_MISSING_UP");
        record("raw_preflight_complete",",\"movement_count\":"+std::to_string(raw_movements.load())+",\"up_count\":"+std::to_string(raw_ups.load()));
        if(end_mode)drive_end_gesture(expected,down);
        else for(int kind=1;kind<=2;++kind){
            gesture=kind;cancelled=false;cancel_pending=false;cancel_return_boundary=0;native_drag_after_return=0;callbacks=0;ResetEvent(entered);ResetEvent(exited);ResetEvent(stepped);ResetEvent(nonclient_down);ResetEvent(raw_up);
            const auto initial=capture();require(initial.p_ok&&initial.v_ok&&contained(initial.p),"setup_geometry_unavailable");
            Geometry cancel_geometry;
            const int wanted=kind==1?HTCAPTION:HTBOTTOM;const POINT start=find_point(initial.p,wanted);
            fence(expected,false);inject(MOUSEEVENTF_MOVE,start);expected=start;pace(interval_ms);
            fence(expected,false);require(hit_test(start)==wanted&&GetAncestor(WindowFromPoint(start),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
            POINT end{start.x+(kind==1?180:0),start.y+(kind==2?120:0)};
            record("path",",\"hit_test\":"+std::to_string(wanted)+",\"start\":"+point(start)+",\"end\":"+point(end)+",\"samples\":20,\"cancel_after_sample\":2,\"interval_ms\":30,\"foreground\":"+std::to_string(number(GetForegroundWindow()))+geometry(initial));
            inject(MOUSEEVENTF_LEFTDOWN,expected);down=true;require(WaitForSingleObject(nonclient_down,2000)==WAIT_OBJECT_0,"missing_native_mouse_down");
            for(int sample=1;sample<=samples;++sample){
                pace(interval_ms);fence(expected,true,sample==1&&WaitForSingleObject(entered,0)!=WAIT_OBJECT_0,cancelled);
                ResetEvent(stepped);const int before=callbacks;
                expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
                ResetEvent(raw_motion);const auto move_request=inject(MOUSEEVENTF_MOVE,expected);
                wait_raw_motion(expected,move_request);
                if(sample<=2){
                    if(sample==1)require(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"missing_ENTER");
                    require(WaitForSingleObject(stepped,2000)==WAIT_OBJECT_0&&callbacks>before,"missing_DRAG");
                }
                pace(interval_ms);
                if(sample==3){
                    // Exactly the next existing trajectory sample, no extra
                    // cancel/write/retry, avoids starving queued input at exit.
                    const auto wait_started=qpc();const auto exit_wait=WaitForSingleObject(exited,2000);const auto wait_finished=qpc();
                    GUITHREADINFO after_wait{sizeof(after_wait)};const bool after_query=GetGUIThreadInfo(ui_thread,&after_wait)!=FALSE;
                    POINT wait_cursor{};const bool cursor_ok=GetCursorPos(&wait_cursor)!=FALSE;
                    record("cancel_exit_wait",",\"started_qpc\":"+std::to_string(wait_started)+",\"finished_qpc\":"+std::to_string(wait_finished)+",\"timeout_ms\":2000,\"observation_wakeup_sample\":3,\"wait_result\":"+std::to_string(exit_wait)+",\"gui_query_succeeded\":"+flag(after_query)+",\"target\":"+std::to_string(number(owned))+",\"source_tid\":"+std::to_string(ui_thread)+",\"source_identity\":"+flag(identity())+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"capture_hwnd\":"+std::to_string(number(after_wait.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(after_wait.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(after_wait.hwndMoveSize))+",\"gui_flags\":"+std::to_string(after_wait.flags)+",\"left_down\":"+flag(pressed(VK_LBUTTON))
                        +",\"desktop_ready\":"+flag(desktop_available())+",\"visible\":"+flag(identity()&&IsWindowVisible(owned))+",\"buttons_modifiers_clear\":"+flag(!other_input())+",\"cursor_success\":"+flag(cursor_ok)+",\"cursor\":"+point(wait_cursor)+",\"expected_cursor\":"+point(expected)+",\"cursor_matches_expected\":"+flag(cursor_ok&&std::abs(wait_cursor.x-expected.x)<=1&&std::abs(wait_cursor.y-expected.y)<=1)+",\"receiver_healthy\":"+flag(receiver_ok));
                    require(native_drag_after_return==0,"native_drag_after_cancel");
                    require(exit_wait==WAIT_OBJECT_0,"cancel_missing_EXIT");
                    require(after_query&&!after_wait.hwndCapture,"cancel_capture_not_released");
                    cancel_pending=false;fence(expected,true,false,true);
                    const auto confirmed_geometry=capture();record("cancel_confirmed",geometry(confirmed_geometry));
                    require(confirmed_geometry.p_ok&&equal(confirmed_geometry.p,cancel_geometry.p),"native_geometry_after_cancel");
                }
                require(native_drag_after_return==0,"native_drag_after_cancel");
                fence(expected,true,false,cancelled);
                POINT current{};require(GetCursorPos(&current),"sample_cursor_failed");
                const auto observed=capture();
                record("sample",",\"index\":"+std::to_string(sample)+",\"cursor\":"+point(current)+",\"left_down\":"+flag(pressed(VK_LBUTTON))+",\"post_cancel\":"+flag(cancelled)+geometry(observed));
                if(cancelled)require(observed.p_ok&&equal(observed.p,cancel_geometry.p),"native_geometry_after_cancel");
                if(sample==2){
                    GUITHREADINFO gui{sizeof(gui)};require(GetGUIThreadInfo(ui_thread,&gui)&&gui.hwndCapture==owned,"cancel_capture_before_failed");fence(expected,true);
                    cancelled=true;const auto issued=qpc();
                    record("cancel_begin",",\"target\":"+std::to_string(number(owned))+",\"source_tid\":"+std::to_string(ui_thread)+",\"capture_before\":"+std::to_string(number(gui.hwndCapture))+",\"left_down\":"+flag(pressed(VK_LBUTTON)));
                    DWORD_PTR result{};SetLastError(0);
                    const auto sent=SendMessageTimeoutW(owned,WM_CANCELMODE,0,0,SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT,1000,&result);const auto error=sent?0:GetLastError();const auto returned=qpc();cancel_return_boundary=returned;
                    const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
                    cancel_geometry=capture();
                    record("cancel_return",",\"transport_success\":"+flag(sent!=0)+",\"error\":"+std::to_string(error)+",\"recipient_result\":"+std::to_string(result)+",\"issued_qpc\":"+std::to_string(issued)+",\"returned_qpc\":"+std::to_string(returned)+",\"gui_query_succeeded\":"+flag(query)+",\"capture_after\":"+std::to_string(number(gui.hwndCapture))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(cancel_geometry));
                    require(sent!=0,"cancel_transport_failed");
                    require(query&&!gui.hwndCapture,"cancel_capture_not_released");
                    require(cancel_geometry.p_ok&&cancel_geometry.v_ok,"cancel_geometry_unavailable");
                    cancel_pending=true;
                }
            }
            fence(expected,true,false,true);const auto up_request=inject(MOUSEEVENTF_LEFTUP,expected);down=false;
            wait_raw_up(up_request);
            pace(interval_ms);fence(expected,false,false,true);record("path_complete",geometry(capture()));
        }
        // End input observation before restoring cursor outside the guard.
        ResetEvent(receiver_armed);
        if(isolation_mode){record("cursor_restore_skipped",",\"reason\":\"strict_test_input_isolation\",\"point\":"+point(saved_cursor)+",\"cursor_restored\":false,\"synthetic_input_sent\":false");driver_pass=true;}
        else{record("cursor_restore_begin",",\"point\":"+point(saved_cursor)+",\"target\":"+std::to_string(number(owned)));
            fence(expected,false,false,true);inject(MOUSEEVENTF_MOVE,saved_cursor,true);expected=saved_cursor;pace(interval_ms);fence(expected,false,false,true);restored=true;driver_pass=true;}
    }catch(const std::exception& e){
        failure=e.what();record("blocked",",\"reason\":\""+failure+"\"");
        takeover_scope=false;
        if(diagnostic_mode){
            if(reliability_mode){
                try{perform_reliability_cleanup(down||activation_down);}
                catch(const std::exception& cleanup_error){abort_cleanup_result=abort_cleanup_attempted?"UNCONFIRMED":"SKIPPED_NO_AUTHORITY";record("abort_cleanup_skipped",",\"reason\":\""+std::string(cleanup_error.what())+"\",\"cleanup_up_attempted\":"+flag(abort_cleanup_attempted)+",\"cleanup_result\":\""+abort_cleanup_result+"\",\"acceptance_eligible\":false");cleanup_observation=false;ResetEvent(receiver_armed);}
                catch(...){abort_cleanup_result=abort_cleanup_attempted?"UNCONFIRMED":"SKIPPED_NO_AUTHORITY";record("abort_cleanup_skipped",",\"reason\":\"unknown_abort_cleanup_exception\",\"cleanup_up_attempted\":"+flag(abort_cleanup_attempted)+",\"cleanup_result\":\""+abort_cleanup_result+"\",\"acceptance_eligible\":false");cleanup_observation=false;ResetEvent(receiver_armed);}
            }else{try{perform_cleanup_input(down||activation_down);}catch(...){record("cleanup_skipped_no_authority",",\"reason\":\"cleanup_diagnostic_exception\",\"acceptance_eligible\":false");ResetEvent(receiver_armed);}}
        }
        else {
        ResetEvent(receiver_armed); // cleanup packets cannot extend acceptance
        GUITHREADINFO gui{sizeof(gui)};POINT current{};
        if(down&&!foreign_capture_transferred&&identity()&&desktop_available()&&GetForegroundWindow()==owned&&GetGUIThreadInfo(ui_thread,&gui)&&GetCursorPos(&current)){
            const HWND root=GetAncestor(WindowFromPoint(current),GA_ROOT);
            const bool capture_owned=gui.hwndCapture==owned;
            if((capture_owned||(!gui.hwndCapture&&input_root_owned(root)))&&!gui.hwndMenuOwner&&(!gui.hwndMoveSize||gui.hwndMoveSize==owned)&&!other_input()&&pressed(VK_LBUTTON)){
                record("cleanup_fence",",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"own_identity\":true,\"desktop_ready\":true,\"visible\":"+flag(IsWindowVisible(owned))+",\"cursor\":"+point(current)+",\"left_down\":true,\"modifiers_clear\":true,\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"root\":"+std::to_string(number(root))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"gui_flags\":"+std::to_string(gui.flags));
                INPUT up=mouse_input(MOUSEEVENTF_LEFTUP,current);const auto start=qpc();SetLastError(0);const auto sent=SendInput(1,&up,sizeof(up));const auto error=sent==1?0:GetLastError();const auto returned=qpc();
                record("cleanup_release",",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned)+",\"target\":"+std::to_string(number(owned))+",\"cursor\":"+point(current)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"root\":"+std::to_string(number(root)));
            }
        }
        }
        // No second WM_CANCELMODE. Destroy only our empty test windows.
    }
    if(!post_source_work(finish_message)){driver_pass=false;record("blocked",",\"reason\":\"finish_message_failed\"");if(!isolation_mode){DWORD_PTR ignored{};SendMessageTimeoutW(owned,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,1000,&ignored);}}
    SetEvent(driver_finished);
}

}
int receiver_main(int argc,wchar_t** argv);
int wmain(int argc,wchar_t** argv){
    if(argc==5&&std::wstring_view(argv[1])==L"--raw-receiver")return receiver_main(argc,argv);
    reliability_mode=argc==8&&std::wstring_view(argv[1])==L"--run-owned-input-reliability-test"&&std::wstring_view(argv[2])==L"--gesture"&&std::wstring_view(argv[4])==L"--mode"&&std::wstring_view(argv[6])==L"--evidence-log";
    if(reliability_mode){
        if(std::wstring_view(argv[5])==L"controlled-abort")controlled_abort=true;
        else if(std::wstring_view(argv[5])!=L"normal")return 2;
    }
    isolation_mode=reliability_mode||(argc==6&&std::wstring_view(argv[1])==L"--run-owned-input-isolation-test"&&std::wstring_view(argv[2])==L"--gesture"&&std::wstring_view(argv[4])==L"--evidence-log");
    diagnostic_mode=isolation_mode||(argc==6&&std::wstring_view(argv[1])==L"--run-owned-end-diagnostics-test"&&std::wstring_view(argv[2])==L"--gesture"&&std::wstring_view(argv[4])==L"--evidence-log");
    end_mode=diagnostic_mode||(argc==6&&std::wstring_view(argv[1])==L"--run-owned-takeover-test"&&std::wstring_view(argv[2])==L"--gesture"&&std::wstring_view(argv[4])==L"--evidence-log");
    if(end_mode){
        if(std::wstring_view(argv[3])==L"move")selected_gesture=1;
        else if(std::wstring_view(argv[3])==L"bottom-resize")selected_gesture=2;
        else return 2;
    }else if(argc!=4||std::wstring_view(argv[1])!=L"--run-owned-cancel-test"||std::wstring_view(argv[2])!=L"--evidence-log"){
        std::cout<<"Explicit test only: --run-owned-cancel-test --evidence-log NEW_FILE\n"
                 <<"Or --run-owned-takeover-test --gesture move|bottom-resize --evidence-log NEW_FILE\n"
                 <<"Or --run-owned-end-diagnostics-test --gesture move|bottom-resize --evidence-log NEW_FILE\n"
                 <<"Or --run-owned-input-isolation-test --gesture move|bottom-resize --evidence-log NEW_FILE\n"
                 <<"Or --run-owned-input-reliability-test --gesture move|bottom-resize --mode controlled-abort|normal --evidence-log NEW_FILE\n";return 2;
    }
    log_file=CreateFileW(argv[reliability_mode?7:(end_mode?5:3)],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log_file==INVALID_HANDLE_VALUE)return 2;
    timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);ui_thread=GetCurrentThreadId();
    if(isolation_mode){run_nonce=static_cast<std::uintptr_t>(static_cast<std::uint64_t>(qpc())^(static_cast<std::uint64_t>(GetCurrentProcessId())<<32));if(!run_nonce)run_nonce=1;}
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    record("startup",",\"evidence_kind\":\""+std::string(end_mode?"automated_owned_end_handoff":"automated_owned_cancel")+"\",\"human_input\":false,\"real_explorer\":false,\"sendinput_in_probe\":true,\"mode\":\""+(end_mode?"free_takeover":"cancel_only")+"\",\"input_correlation\":\"actual_absolute_receipt_v1\",\"takeover_geometry_writes\":0,\"foreground_contract\":\"verified_global_foreground_v2\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"ui_tid\":"+std::to_string(ui_thread)+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart)
        +(end_mode?",\"handoff_contract\":\""+std::string(diagnostic_mode?"winevent_end_barrier_v1":"end_barrier_v1")+"\",\"operation\":\""+std::string(selected_gesture==1?"Move":"BottomResize")+"\"":"")+(diagnostic_mode?",\"diagnostic_contract\":\""+std::string(isolation_mode?"separated_authority_v1":"structured_handoff_v1")+"\",\"gesture_id\":1":"")
        +(isolation_mode?",\"authority_contract\":\"product_gesture_v1\",\"input_isolation_contract\":\"post_end_shield_v1\",\"run_nonce\":"+std::to_string(run_nonce)+",\"cursor_root_required_for_product\":false,\"shield_test_only\":true":"")
        +(reliability_mode?",\"test_mode\":\""+std::string(controlled_abort?"controlled_abort":"normal")+"\",\"fence_contract\":\"ordered_failure_snapshot_v1\",\"cleanup_contract\":\"owned_native_abort_v1\",\"abort_cleanup_tag\":"+std::to_string(abort_cleanup_tag):""));
    int result=2;
    try {
        require(timer&&desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
        record("desktop_gate",",\"active_unlocked\":true,\"input_desktop_matches\":true");
        std::cout<<"PaneBind Pivot 1 background-input / native-cancel research.\nOnly empty test-owned windows are used.\nDo not touch mouse or keyboard.\nStarting in 3 seconds... Press Esc to cancel.\n"<<std::flush;
        for(int n=3;n>0;--n){std::cout<<n<<'\n'<<std::flush;pace(1000);require(!pressed(VK_ESCAPE),"BLOCKED_BY_INPUT_INTERFERENCE");}
        require(!pressed(VK_LBUTTON)&&!other_input()&&GetCursorPos(&saved_cursor),"BLOCKED_BY_INPUT_INTERFERENCE");
        MONITORINFO monitor{sizeof(monitor)};
        require(GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&monitor)!=FALSE,"BLOCKED_BY_INTERACTIVE_DESKTOP");work_area=monitor.rcWork;
        virtual_area={GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};
        virtual_area.right=virtual_area.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);virtual_area.bottom=virtual_area.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
        require(work_area.right-work_area.left>=1120&&work_area.bottom-work_area.top>=850,"BLOCKED_BY_INTERACTIVE_DESKTOP");
        const int setup_x=work_area.left+(work_area.right-work_area.left-640-180)/2;
        const int setup_y=work_area.top+(work_area.bottom-work_area.top-440-120)/2;
        WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"PaneBindC4BPivot1Source";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        require(RegisterClassW(&cls)!=0,"owned_window_creation_failed");
        WNDCLASSW guard_class{};guard_class.lpfnWndProc=DefWindowProcW;guard_class.hInstance=cls.hInstance;guard_class.lpszClassName=L"PaneBindTakeoverInputGuard";guard_class.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        require(RegisterClassW(&guard_class)!=0,"guard_window_creation_failed");
        if(isolation_mode){WNDCLASSW shield_class{};shield_class.lpfnWndProc=shield_procedure;shield_class.hInstance=cls.hInstance;shield_class.lpszClassName=L"PaneBindPostEndInputShield";shield_class.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);require(RegisterClassW(&shield_class)!=0,"shield_class_registration_failed");}
        // Fix C's sole CursorOutsideOwnedInputGuard branch authorizes only
        // this test-owned geometry correction, not a stacking/authority change.
        const int guard_margin=diagnostic_mode?50:30;
        const int guard_width=diagnostic_mode?640+180+100:880;
        const int guard_height=diagnostic_mode?440+120+100:650;
        guard=CreateWindowExW(WS_EX_NOACTIVATE,guard_class.lpszClassName,L"PaneBind empty test input guard",WS_POPUP,setup_x-guard_margin,setup_y-guard_margin,guard_width,guard_height,nullptr,nullptr,cls.hInstance,nullptr);
        require(guard!=nullptr,"guard_window_creation_failed");ShowWindow(guard,SW_SHOWNOACTIVATE);
        if(!IsWindowVisible(guard))ShowWindow(guard,SW_SHOWNOACTIVATE);
        require(IsWindowVisible(guard),"guard_window_visibility_failed");
        RECT guard_rect{};SetLastError(0);const bool guard_rect_ok=diagnostic_mode&&GetWindowRect(guard,&guard_rect)!=FALSE;const auto guard_rect_error=guard_rect_ok?0:GetLastError();
        const RECT move_plan{setup_x,setup_y,setup_x+640+180,setup_y+440};
        const RECT resize_plan{setup_x,setup_y,setup_x+640,setup_y+440+120};
        const RECT combined_plan{setup_x,setup_y,setup_x+640+180,setup_y+440+120};
        const auto covered_with_margin=[&](const RECT& plan){return guard_rect_ok&&guard_rect.left<=plan.left-50&&guard_rect.top<=plan.top-50&&guard_rect.right>=plan.right+50&&guard_rect.bottom>=plan.bottom+50;};
        record("guard",",\"hwnd\":"+std::to_string(number(guard))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)
            +(diagnostic_mode?",\"positioning_available\":"+flag(guard_rect_ok)+",\"positioning\":"+(guard_rect_ok?rect(guard_rect):"null")+",\"positioning_error\":"+std::to_string(guard_rect_error)+",\"noactivate\":"+flag((GetWindowLongPtrW(guard,GWL_EXSTYLE)&WS_EX_NOACTIVATE)!=0)
                +",\"margin_px\":50,\"planned_move_bounds\":"+rect(move_plan)+",\"planned_bottom_resize_bounds\":"+rect(resize_plan)+",\"planned_bounds\":"+rect(combined_plan)+",\"move_trajectory_with_margin_covered\":"+flag(covered_with_margin(move_plan))+",\"bottom_resize_trajectory_with_margin_covered\":"+flag(covered_with_margin(resize_plan)):""));
        owned=CreateWindowExW(0,cls.lpszClassName,L"PaneBind Pivot 1 owned research - do not touch input",WS_OVERLAPPEDWINDOW,
            setup_x,setup_y,640,440,nullptr,nullptr,cls.hInstance,nullptr);
        require(owned!=nullptr,"owned_window_creation_failed");
        if(isolation_mode){SetLastError(0);const auto previous=SetWindowLongPtrW(owned,GWLP_USERDATA,static_cast<LONG_PTR>(run_nonce));const auto error=previous?0:GetLastError();require(!error&&identity(),"source_nonce_binding_failed");}
        // Test setup only, before any native interactive gesture.
        require(SetWindowPos(owned,nullptr,setup_x,setup_y,640,440,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"setup_failed");
        entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);exited=CreateEventW(nullptr,TRUE,FALSE,nullptr);stepped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);pointer_arrived=CreateEventW(nullptr,TRUE,FALSE,nullptr);nonclient_down=CreateEventW(nullptr,TRUE,FALSE,nullptr);correction_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);driver_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_down_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);activation_up_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        handoff_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);quantum_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);takeover_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(diagnostic_mode){winevent_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);native_preflight_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(winevent_finished&&native_preflight_finished,"winevent_observation_event_creation_failed");}
        if(isolation_mode){final_acceptance_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(final_acceptance_finished!=nullptr,"final_acceptance_event_creation_failed");}
        if(reliability_mode){abort_quiescence_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(abort_quiescence_finished!=nullptr,"abort_quiescence_event_creation_failed");}
        require(entered&&exited&&stepped&&activation_event&&pointer_arrived&&nonclient_down&&correction_finished&&driver_finished&&activation_down_received&&activation_up_received,"event_creation_failed");
        require(handoff_finished&&quantum_finished&&takeover_finished,"continuation_event_creation_failed");
        start_receiver();
        STARTUPINFOW startup{sizeof(startup)};GetStartupInfoW(&startup);
        ShowWindow(owned,SW_SHOWNOACTIVATE);
        record("show_window",",\"startup_flags\":"+std::to_string(startup.dwFlags)+",\"startup_show\":"+std::to_string(startup.wShowWindow)+",\"requested_show\":4,\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        // The first ShowWindow may honor the shell parent's hidden STARTUPINFO
        // instead of the requested mode. Do not create local activation before
        // the ordinary foreground attempt / verified activation click.
        if(!IsWindowVisible(owned))ShowWindow(owned,SW_SHOWNOACTIVATE);
        bootstrap.attempted=true;const BOOL activated=SetForegroundWindow(owned);set_foreground_success=activated!=FALSE;UpdateWindow(owned);
        record("foreground_attempt",",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"source_thread_local_active\":"+std::to_string(number(GetActiveWindow()))+",\"source_thread_local_focus\":"+std::to_string(number(GetFocus()))+",\"set_foreground_success\":"+flag(activated!=FALSE)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        frozen_dpi=GetDpiForWindow(owned);frozen_monitor=MonitorFromWindow(owned,MONITOR_DEFAULTTONULL);
        record("owned",",\"hwnd\":"+std::to_string(number(owned))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)+",\"saved_cursor\":"+point(saved_cursor)+",\"work_area\":"+rect(work_area)+",\"dpi\":"+std::to_string(frozen_dpi)+geometry(capture())+",\"virtual_screen\":"+rect(virtual_area)+(end_mode?",\"monitor\":"+std::to_string(reinterpret_cast<std::uintptr_t>(frozen_monitor)):"")+(isolation_mode?",\"run_nonce\":"+std::to_string(run_nonce)+",\"actual_source_nonce\":"+std::to_string(isolation_identity(owned).nonce):""));
        if(diagnostic_mode)install_movesize_hook();
        std::thread driver{drive};MSG message{};bool owner_ok=true;
        bool quit=false;while(!quit){
            const DWORD wait=MsgWaitForMultipleObjects(1,&driver_finished,FALSE,15000,QS_ALLINPUT);
            if(wait==WAIT_TIMEOUT||wait==WAIT_FAILED){owner_ok=false;stop_requested=true;retire_source_fixture();record("blocked",",\"reason\":\"owner_message_wait_failed\"");if(identity())DestroyWindow(owned);break;}
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){quit=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}
            if(wait==WAIT_OBJECT_0){retire_source_fixture();if(identity())DestroyWindow(owned);quit=true;}
        }
        driver.join();result=owner_ok&&driver_pass&&log_ok?0:2;
    }catch(const std::exception& e){failure=e.what();retire_source_fixture();emit_bootstrap(false,e.what());record("blocked",",\"reason\":\""+failure+"\"");if(identity())DestroyWindow(owned);}
    if(isolation_mode){try{destroy_input_shield("failure",0,0);}catch(...){shield_healthy=false;}}
    remove_movesize_hook();stop_receiver();if(guard)DestroyWindow(guard);
    if(!receiver_ok||!receiver_removed||!receiver_destroyed)result=2;
    if(diagnostic_mode&&(!winevent_healthy||movesize_hook))result=2;
    if(isolation_mode&&(!shield_healthy||(shield_created&&!shield_destroyed)))result=2;
    record("shutdown",",\"result\":\""+std::string(result==0?"CAPTURED_NOT_ACCEPTED":"BLOCKED")+"\",\"cursor_restored\":"+flag(restored)+",\"owned_window_destroyed\":"+flag(!IsWindow(owned))+",\"guard_window_destroyed\":"+flag(!IsWindow(guard))+",\"receiver_stopped\":"+flag(receiver_ok&&receiver_removed&&receiver_destroyed)+",\"external_windows_touched\":false"+(end_mode?",\"takeover_geometry_writes\":"+std::to_string(takeover_native_calls.load()):"")
        +(diagnostic_mode?",\"winevent_hook_removed\":"+flag(movesize_hook_removal_attempted&&movesize_hook==nullptr&&winevent_healthy)+",\"native_drag_after_winevent_end\":"+std::to_string(native_drag_after_winevent_end.load()):"")+(isolation_mode?",\"input_shield_created\":"+flag(shield_created)+",\"input_shield_destroyed\":"+flag(!shield_created||shield_destroyed)+",\"input_shield_activated\":"+flag(shield_activated)+",\"source_acceptance_sequence\":"+std::to_string(source_acceptance_sequence)+",\"run_nonce\":"+std::to_string(run_nonce):"")
        +(reliability_mode?",\"gesture_result\":\""+std::string(driver_pass?"CAPTURED_NOT_ACCEPTED":failure)+"\",\"cleanup_result\":\""+abort_cleanup_result+"\",\"fixture_result\":\""+std::string(controlled_abort&&failure=="BLOCKED_BY_TEST_FAULT"&&abort_cleanup_result=="PASS"?"PASS_EXPECTED_ABORT":(driver_pass?"CAPTURED_NOT_ACCEPTED":"BLOCKED"))+"\"":""));
    for(HANDLE handle:{entered,exited,stepped,timer,activation_event,pointer_arrived,nonclient_down,correction_finished,driver_finished,activation_down_received,activation_up_received,handoff_finished,quantum_finished,takeover_finished,winevent_finished,native_preflight_finished,final_acceptance_finished,abort_quiescence_finished})if(handle)CloseHandle(handle);
    const bool evidence_ok=log_ok;CloseHandle(log_file);return evidence_ok?result:2;
}
int receiver_main(int,wchar_t** argv){
    try{
        child_pipe=reinterpret_cast<HANDLE>(std::stoull(argv[2]));child_stop=reinterpret_cast<HANDLE>(std::stoull(argv[3]));child_armed=reinterpret_cast<HANDLE>(std::stoull(argv[4]));
        if(GetFileType(child_pipe)!=FILE_TYPE_PIPE)return 2;
        WNDCLASSW cls{};cls.lpfnWndProc=raw_procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"PaneBindTakeoverRawReceiver";
        if(!RegisterClassW(&cls))return 2;
        const HWND window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
        if(!window)return 2;
        RAWINPUTDEVICE device{1,2,RIDEV_INPUTSINK,window};
        const bool registered=RegisterRawInputDevices(&device,1,sizeof(device))!=FALSE;
        RAWINPUTDEVICE actual[4]{};UINT count=4;
        const auto found=GetRegisteredRawInputDevices(actual,&count,sizeof(RAWINPUTDEVICE));
        const bool verified=registered&&found==1&&actual[0].usUsagePage==1&&actual[0].usUsage==2&&actual[0].dwFlags==RIDEV_INPUTSINK&&actual[0].hwndTarget==window;
        RawPacket ready;ready.kind=1;ready.hwnd=number(window);ready.registration_ok=verified;
        if(!send_packet(ready)||!verified){DestroyWindow(window);return 2;}
        bool okay=true;
        for(;;){
            const auto wait=MsgWaitForMultipleObjectsEx(1,&child_stop,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(wait==WAIT_OBJECT_0)break;
            if(wait==WAIT_FAILED){okay=false;break;}
            MSG message{};unsigned drained{};
            while(drained++<64&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        }
        device.dwFlags=RIDEV_REMOVE;device.hwndTarget=nullptr;
        const bool removed=RegisterRawInputDevices(&device,1,sizeof(device))!=FALSE;
        DestroyWindow(window);RawPacket final;final.kind=4;final.hwnd=number(window);final.removed=removed;final.destroyed=!IsWindow(window);
        const bool written=send_packet(final);CloseHandle(child_pipe);CloseHandle(child_stop);CloseHandle(child_armed);
        return okay&&removed&&written?0:2;
    }catch(...){return 2;}
}
