// Test executable only: never linked into the product / Explorer runtime.
#include "platform/windows/operations/test_foreground_bootstrap_model.h"
#include "platform/windows/operations/window_rect_adjustment.h"
#include "platform/windows/operations/magnet_postverify_diagnostic.h"
#include <windows.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <algorithm>
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
constexpr int samples=20, interval_ms=30;
constexpr ULONG_PTR input_tag=0x50424D41;
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
bool end_mode{};int selected_gesture=1;
std::atomic<std::int64_t> native_end_qpc{},native_enter_qpc{};
std::atomic<std::uint64_t> native_end_sequence{},native_enter_sequence{},native_down_sequence{};
std::atomic<int> native_drag_after_end{},unowned_geometry_changes{};
std::atomic<bool> takeover_scope{},handoff_ready{},takeover_healthy{true};
HANDLE handoff_finished{},quantum_finished{},takeover_finished{};
HMONITOR frozen_monitor{};UINT frozen_dpi{};

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
    const auto line="{\"schema\":\""+std::string(end_mode?"r1c4b-takeover-owned/v2":"r1c4b-takeover-owned/v1")+"\",\"sequence\":"+std::to_string(assigned)+",\"type\":\""+std::string(type)+"\",\"gesture\":"+std::to_string(gesture.load())+",\"qpc\":"+std::to_string(qpc())+fields+(end_mode&&type=="EXIT"?",\"end_sequence\":"+std::to_string(assigned):"")+"}\n";
    DWORD written{};log_ok=WriteFile(log_file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
    return sequence;
}
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
bool identity(){DWORD pid{};return owned&&GetWindowThreadProcessId(owned,&pid)==ui_thread&&pid==GetCurrentProcessId();}
bool input_root_owned(HWND root){DWORD pid{};return (root==owned||root==guard)&&GetWindowThreadProcessId(root,&pid)==ui_thread&&pid==GetCurrentProcessId();}
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
void process_handoff();void process_raw_notice();
Geometry capture(){
    Geometry g;if(!identity())return g;SetLastError(0);g.p_ok=GetWindowRect(owned,&g.p)!=FALSE;g.error=g.p_ok?0:GetLastError();
    g.hr=DwmGetWindowAttribute(owned,DWMWA_EXTENDED_FRAME_BOUNDS,&g.v,sizeof(g.v));g.v_ok=SUCCEEDED(g.hr);return g;
}
std::string geometry(const Geometry& g){return ",\"positioning\":"+(g.p_ok?rect(g.p):"null")+",\"visible\":"+(g.v_ok?rect(g.v):"null")+",\"positioning_error\":"+std::to_string(g.error)+",\"visible_hresult\":"+std::to_string(g.hr);}
bool contained(const RECT& r){return r.left>=work_area.left+100&&r.top>=work_area.top+100&&r.right<=work_area.right-100&&r.bottom<=work_area.bottom-100;}
// A separate empty child process makes RIM_INPUTSINK empirically meaningful.
// It can observe mouse packets; it has no target or geometry-writing authority.
struct RawPacket {
    std::uint32_t kind{},serial{},pid{},tid{},input_code{},flags{},buttons{},error{},cursor_error{};
    std::int32_t dx{},dy{};
    std::uintptr_t hwnd{},foreground{};
    DWORD foreground_pid{};
    std::int64_t observed_qpc{};
    POINT cursor{};
    bool cursor_sampled{},cursor_ok{},left_down{},device_present{},tag_matches{},registration_ok{},removed{},destroyed{};
};
RawPacket last_motion,last_up;std::mutex raw_mutex;
RawPacket pending_motion,published_up;
std::uint32_t pending_first{},pending_count{};
bool motion_pending{},raw_up_seen{},notice_posted{}; // continuation_mutex
std::atomic<std::uint32_t> owner_processed_sequence{};
std::atomic<int> takeover_native_calls{};
struct InputReceipt {INPUT input;std::int64_t start{};std::uint32_t watermark{};};
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
                packet.left_down=pressed(VK_LBUTTON);packet.device_present=input.header.hDevice!=nullptr;packet.tag_matches=mouse.ulExtraInformation==input_tag;
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
            bool notify=false;
            if(end_mode&&takeover_scope){
                // Publish terminal lifecycle before any potentially delayed
                // logging. A pending motion can never override a received UP.
                std::lock_guard lock(continuation_mutex);
                if(p.buttons&RI_MOUSE_LEFT_BUTTON_UP){published_up=p;raw_up_seen=true;motion_pending=false;pending_count=0;}
                else if(p.cursor_sampled&&!raw_up_seen){
                    if(!motion_pending){pending_first=p.serial;pending_count=0;}
                    pending_motion=p;motion_pending=true;
                    if(++pending_count>128)receiver_ok=false;
                }
                if(handoff_ready&&!notice_posted){notice_posted=true;notify=true;}
            }
            ++raw_packets;
            record("raw_input",common+",\"input_code\":"+std::to_string(p.input_code)+",\"raw_flags\":"+std::to_string(p.flags)+",\"dx\":"+std::to_string(p.dx)+",\"dy\":"+std::to_string(p.dy)+",\"button_flags\":"+std::to_string(p.buttons)+",\"cursor_sampled\":"+flag(p.cursor_sampled)+",\"cursor_success\":"+flag(p.cursor_ok)+",\"cursor\":"+(p.cursor_sampled&&p.cursor_ok?point(p.cursor):"null")+",\"left_down\":"+flag(p.left_down)+",\"foreground_hwnd\":"+std::to_string(p.foreground)+",\"foreground_pid\":"+std::to_string(p.foreground_pid)+",\"device_handle_present\":"+flag(p.device_present)+",\"test_tag_matches\":"+flag(p.tag_matches));
            record("raw_cursor_status",common+",\"cursor_error\":"+std::to_string(p.cursor_error));
            if(notify&&!PostMessageW(owned,raw_notice_message,0,0)){receiver_ok=false;takeover_healthy=false;record("receiver_error",",\"reason\":\"owner_notice_post_failed\"");}
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
        POINT down_cursor{};const bool observed=GetCursorPos(&down_cursor)!=FALSE;
        if(end_mode){std::lock_guard lock(continuation_mutex);intent_pointer=down_cursor;if(!observed)takeover_healthy=false;}
        native_down_sequence=record("native_button_down",",\"target\":"+std::to_string(number(h))+",\"hit_test\":"+std::to_string(w)+(end_mode?",\"cursor\":"+point(down_cursor)+",\"cursor_success\":"+flag(observed):""));SetEvent(nonclient_down);
    }
    if(message==WM_ENTERSIZEMOVE){
        active=true;const auto receipt=qpc();const auto g=capture();
        native_enter_qpc=receipt;
        native_enter_sequence=record("ENTER",",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(g)+(end_mode?",\"receipt_qpc\":"+std::to_string(receipt):""));
        if(end_mode){
            POINT down_cursor{};{std::lock_guard lock(continuation_mutex);intent_start=g;down_cursor=intent_pointer;}
            record("intent_anchor",",\"native_enter_sequence\":"+std::to_string(native_enter_sequence.load())+",\"native_enter_qpc\":"+std::to_string(receipt)+",\"native_down_sequence\":"+std::to_string(native_down_sequence.load())+",\"pointer_down\":"+point(down_cursor)+",\"start_positioning\":"+rect(g.p)+",\"start_visible\":"+rect(g.v)+",\"operation\":\""+(selected_gesture==1?"Move":"BottomResize")+"\"");
        }
        SetEvent(entered);return 0;
    }
    if(message==WM_MOVING||message==WM_SIZING){
        if(cancel_return_boundary&&qpc()>cancel_return_boundary)++native_drag_after_return;
        if(end_mode&&native_end_qpc){++native_drag_after_end;takeover_healthy=false;}
        POINT cursor{};GetCursorPos(&cursor);
        record("DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"cursor\":"+point(cursor)+",\"proposed\":"+rect(*reinterpret_cast<RECT*>(l))+",\"after_cancel\":"+flag(cancelled)+geometry(capture()));
        ++callbacks;SetEvent(stepped);return DefWindowProcW(h,message,w,l);
    }
    if(message==WM_CANCELMODE)record("cancel_message",",\"target\":"+std::to_string(number(h))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(capture()));
    if(message==WM_CAPTURECHANGED){
        const auto next=reinterpret_cast<HWND>(l);if(next&&!input_root_owned(next))foreign_capture_transferred=true;
        record("CAPTURE_CHANGED",",\"new_capture\":"+std::to_string(number(next))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(capture()));
    }
    if(message==WM_WINDOWPOSCHANGED&&gesture>0){
        const auto g=capture();bool unexpected=false;
        if(end_mode&&audit_live){
            unexpected=!g.p_ok||!g.v_ok||(active_operation?!equal(g.p,inflight_target.p):(!equal(g.p,expected_live.p)||!equal(g.v,expected_live.v)));
            if(unexpected){++unowned_geometry_changes;takeover_healthy=false;}
        }
        record("POSITION_CHANGED",",\"after_cancel\":"+flag(cancelled)+geometry(g)+(end_mode?",\"operation_id\":"+std::to_string(active_operation)+",\"after_end\":"+flag(native_end_qpc!=0)+",\"unexpected_change\":"+flag(unexpected):""));
    }
    if(message==WM_EXITSIZEMOVE){
        const auto receipt=qpc();const auto g=capture();
        native_end_qpc=receipt;
        if(end_mode){std::lock_guard lock(continuation_mutex);terminal_geometry=g;expected_live=g;audit_live=true;}
        native_end_sequence=record("EXIT",",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(g)+(end_mode?",\"receipt_qpc\":"+std::to_string(receipt):""));
        active=false;SetEvent(exited);return 0;
    }
    if(message==finish_message){if(end_mode){audit_live=false;takeover_scope=false;}DestroyWindow(h);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,message,w,l);
}
void fence(POINT expected,bool down,bool priming=false,bool post_cancel=false){
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
        if(down)require(input_root_owned(root),"BLOCKED_BY_INPUT_HIT_AUTHORITY");
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
        require(input_root_owned(GetAncestor(WindowFromPoint(p),GA_ROOT)),"BLOCKED_BY_INPUT_HIT_AUTHORITY");
        GUITHREADINFO gui{sizeof(gui)};
        require(GetGUIThreadInfo(ui_thread,&gui)&&cancellation_gui_safe(gui),"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
    }
    if(!restoring_cursor&&(gesture==0||cancelled)&&(flags&MOUSEEVENTF_MOVE)){
        const HWND root=GetAncestor(WindowFromPoint(p),GA_ROOT);
        require(input_root_owned(root),"BLOCKED_BY_INPUT_HIT_AUTHORITY");
        GUITHREADINFO gui{sizeof(gui)};
        require(GetGUIThreadInfo(ui_thread,&gui)&&cancellation_gui_safe(gui),"BLOCKED_BY_FOREIGN_INPUT_CAPTURE");
        record("destination_fence",",\"point\":"+point(p)+",\"root\":"+std::to_string(number(root))+",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"capture_hwnd\":0,\"menu_owner_hwnd\":0,\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"gui_flags\":"+std::to_string(gui.flags)+",\"cancel_pending\":"+flag(cancel_pending)+",\"left_down\":"+flag(pressed(VK_LBUTTON)));
    }
    auto input=mouse_input(flags,p);
    const auto watermark=receiver_watermark.load();const auto start=qpc();SetLastError(0);const UINT sent=SendInput(1,&input,sizeof(input));const DWORD error=sent==1?0:GetLastError();const auto returned=qpc();
    record("input",",\"flags\":"+std::to_string(flags)+",\"point\":"+point(p)+",\"restoring_cursor\":"+flag(restoring_cursor)+",\"actual_mouse_flags\":"+std::to_string(input.mi.dwFlags)+",\"normalized_dx\":"+std::to_string(input.mi.dx)+",\"normalized_dy\":"+std::to_string(input.mi.dy)+",\"receiver_watermark\":"+std::to_string(watermark)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned)+",\"virtual_screen\":"+rect(virtual_area));
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
    const HWND foreground=GetForegroundWindow();const DWORD tid=GetWindowThreadProcessId(foreground,nullptr);
    GUITHREADINFO gui{sizeof(gui)};const bool query=tid&&GetGUIThreadInfo(tid,&gui)!=FALSE;
    const bool stable=foreground&&GetForegroundWindow()==foreground;
    constexpr DWORD modal_flags=GUI_INMOVESIZE|GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE;
    proof.foreign_capture_clear=query&&stable&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&modal_flags);
    proof.modifiers_clear=!pressed(VK_CONTROL)&&!pressed(VK_SHIFT)&&!pressed(VK_MENU)&&!pressed(VK_LWIN)&&!pressed(VK_RWIN)&&!pressed(VK_ESCAPE);
    proof.button_state_matches=pressed(VK_LBUTTON)==held&&!pressed(VK_RBUTTON)&&!pressed(VK_MBUTTON)&&!pressed(VK_XBUTTON1)&&!pressed(VK_XBUTTON2);
    POINT cursor{};const bool cursor_ok=GetCursorPos(&cursor)!=FALSE;
    if(phase!="move"&&(!cursor_ok||std::abs(cursor.x-activation_point.x)>1||std::abs(cursor.y-activation_point.y)>1))proof.button_state_matches=false;
    bootstrap.last_proof=proof;
    record("activation_fence",",\"phase\":\""+std::string(phase)+"\",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(foreground))+",\"cursor\":"+point(cursor)+",\"activation_point\":"+point(activation_point)
        +",\"own_identity\":"+flag(proof.own_identity)+",\"desktop_ready\":"+flag(proof.desktop_ready)+",\"same_integrity\":"+flag(proof.same_integrity)+",\"visible\":"+flag(proof.visible)+",\"temporary_topmost\":"+flag(proof.temporary_topmost)
        +",\"window_from_point_root\":"+std::to_string(number(root))+",\"window_from_point_root_matches\":"+flag(proof.point_root_matches)+",\"hit_test\":"+std::to_string(bootstrap.hit)
        +",\"gui_query_succeeded\":"+flag(query)+",\"foreground_snapshot_stable\":"+flag(stable)+",\"gui_flags\":"+std::to_string(gui.flags)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))
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
struct OwnerProof {POINT cursor{};std::string fields;bool healthy{};};
OwnerProof owner_proof(bool require_held){
    OwnerProof proof;
    const bool own=identity(),desktop=desktop_available(),visible=own&&IsWindowVisible(owned);
    const HWND foreground=GetForegroundWindow();DWORD pid{};const DWORD tid=GetWindowThreadProcessId(foreground,&pid);
    GUITHREADINFO gui{sizeof(gui)};const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const bool cursor_ok=GetCursorPos(&proof.cursor)!=FALSE;
    const HWND root=cursor_ok?GetAncestor(WindowFromPoint(proof.cursor),GA_ROOT):nullptr;
    const bool held=pressed(VK_LBUTTON),clean=!other_input();bool up{};
    {std::lock_guard lock(continuation_mutex);up=raw_up_seen;}
    const UINT dpi=own?GetDpiForWindow(owned):0;const auto monitor=MonitorFromWindow(owned,MONITOR_DEFAULTTONULL);
    proof.healthy=own&&desktop&&visible&&foreground==owned&&pid==GetCurrentProcessId()&&tid==ui_thread&&GetForegroundWindow()==foreground
        &&query&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&30)&&cursor_ok&&input_root_owned(root)&&clean
        &&(!require_held||(held&&!up))&&receiver_ok&&takeover_healthy&&!foreign_capture_transferred&&dpi==frozen_dpi&&monitor==frozen_monitor;
    proof.fields=",\"target\":"+std::to_string(number(owned))+",\"guard\":"+std::to_string(number(guard))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(ui_thread)
        +",\"own_identity\":"+flag(own)+",\"desktop_ready\":"+flag(desktop)+",\"source_visible\":"+flag(visible)+",\"foreground\":"+std::to_string(number(foreground))+",\"foreground_pid\":"+std::to_string(pid)+",\"foreground_tid\":"+std::to_string(tid)
        +",\"gui_query_succeeded\":"+flag(query)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))+",\"gui_flags\":"+std::to_string(gui.flags)
        +",\"buttons_modifiers_clear\":"+flag(clean)+",\"left_down\":"+flag(held)+",\"receiver_healthy\":"+flag(receiver_ok)+",\"raw_up_seen\":"+flag(up)+",\"cursor_root\":"+std::to_string(number(root))+",\"dpi\":"+std::to_string(dpi)+",\"monitor\":"+std::to_string(reinterpret_cast<std::uintptr_t>(monitor));
    return proof;
}
struct WriteReceipt {std::uint64_t id{};int native_calls{};};
WriteReceipt write_intended(const char* kind,std::uint64_t quantum,const RawPacket& raw,std::uint32_t first,const OwnerProof& proof){
    require(proof.healthy&&native_end_qpc&&native_drag_after_return==0&&native_drag_after_end==0&&unowned_geometry_changes==0,"takeover_authority_failed");
    const auto before=capture(),target=intended_at(proof.cursor);
    require(before.p_ok&&before.v_ok&&equal(before.p,expected_live.p)&&equal(before.v,expected_live.v),"post_end_unattributed_geometry_change");
    const bool mismatch=!equal(before.p,target.p)||!equal(before.v,target.v);
    const auto id=++operation_counter;
    record("writer_begin",",\"operation_id\":"+std::to_string(id)+",\"quantum_id\":"+std::to_string(quantum)+",\"kind\":\""+kind+"\",\"raw_first_sequence\":"+std::to_string(first)+",\"raw_last_sequence\":"+std::to_string(raw.serial)+",\"raw_trigger_qpc\":"+std::to_string(raw.observed_qpc)
        +",\"cursor\":"+point(proof.cursor)+delta_fields(proof.cursor)+named_geometry(target,"intended_")+named_geometry(before,"before_")+named_geometry(expected_live,"expected_before_")+",\"native_calls\":"+std::to_string(mismatch?1:0)+",\"end_qpc\":"+std::to_string(native_end_qpc.load())+proof.fields);
    std::int64_t started{},returned{};BOOL succeeded=TRUE;DWORD error{};
    if(mismatch){
        // Serialize the owner commit against publication of terminal UP. The
        // independent receiver QPC remains authoritative: a physically earlier
        // UP cannot be made acceptable by a later parent publication/log row.
        std::lock_guard lock(continuation_mutex);require(!raw_up_seen,"raw_up_before_write");
        require(identity()&&GetForegroundWindow()==owned&&pressed(VK_LBUTTON)&&!other_input()&&takeover_healthy,"write_boundary_context_lost");
        inflight_target=target;active_operation=id;started=qpc();
        require(started>native_end_qpc,"write_before_native_end");SetLastError(0);++takeover_native_calls;
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
    record("writer_result",",\"operation_id\":"+std::to_string(id)+",\"quantum_id\":"+std::to_string(quantum)+",\"kind\":\""+kind+"\",\"native_calls\":"+std::to_string(mismatch?1:0)+",\"native_success\":"+flag(succeeded!=FALSE)+",\"error\":"+std::to_string(error)
        +",\"native_start_qpc\":"+std::to_string(started)+",\"native_return_qpc\":"+std::to_string(returned)+geometry(immediate)+",\"positioning_exact\":"+flag(diagnostic.positioning_exact)+",\"visible_exact\":"+flag(diagnostic.visible_exact)+",\"postverify_exact\":"+flag(exact)+",\"diagnostic\":"+ops::magnet_postverify_json(diagnostic)+named_geometry(full,"full_"));
    require(exact,"takeover_exact_postverify_failed");expected_live=target;return {id,mismatch?1:0};
}
void process_handoff(){
    require(!handoff_ready&&takeover_scope&&native_end_qpc,"handoff_without_end_or_duplicate");
    const auto proof=owner_proof(true);const auto actual=capture();Geometry terminal;
    {std::lock_guard lock(continuation_mutex);terminal=terminal_geometry;}
    require(proof.healthy&&actual.p_ok&&actual.v_ok&&equal(actual.p,terminal.p)&&equal(actual.v,terminal.v),"handoff_terminal_stability_failed");
    const auto target=intended_at(proof.cursor);
    record("handoff_begin",",\"end_sequence\":"+std::to_string(native_end_sequence.load())+",\"end_qpc\":"+std::to_string(native_end_qpc.load())+",\"current_cursor\":"+point(proof.cursor)+delta_fields(proof.cursor)+named_geometry(actual,"actual_handoff_")+named_geometry(target,"intended_")+",\"raw_watermark\":"+std::to_string(receiver_watermark.load())+proof.fields);
    const RawPacket no_raw;
    const auto receipt=write_intended("handoff",0,no_raw,0,proof);
    bool notify=false;
    {std::lock_guard lock(continuation_mutex);motion_pending=false;pending_count=0;consumed_motion=pending_motion.serial;
        handoff_ready=true;if(raw_up_seen&&!notice_posted){notice_posted=true;notify=true;}}
    record("handoff_complete",",\"operation_id\":"+std::to_string(receipt.id)+",\"native_calls\":"+std::to_string(receipt.native_calls)+",\"exact\":true,\"end_qpc\":"+std::to_string(native_end_qpc.load()));
    if(notify)require(PostMessageW(owned,raw_notice_message,0,0)!=FALSE,"handoff_terminal_notice_failed");
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
        record("takeover_end",",\"raw_up_receiver_sequence\":"+std::to_string(up.serial)+",\"raw_up_receiver_qpc\":"+std::to_string(up.observed_qpc)+",\"final_cursor\":"+point(proof.cursor)+named_geometry(target,"intended_")+geometry(actual)+",\"exact\":"+flag(exact)+",\"pending_write\":false,\"pending_motion\":false,\"native_drag_after_end\":"+std::to_string(native_drag_after_end.load())+",\"unowned_geometry_changes\":"+std::to_string(unowned_geometry_changes.load())+proof.fields);
        takeover_scope=false;audit_live=false;require(exact,"takeover_final_geometry_failed");SetEvent(takeover_finished);return;
    }
    if(!has_motion||!handoff_ready||motion.serial<=consumed_motion||motion.observed_qpc<=native_end_qpc)return;
    const auto quantum=++quantum_counter;const auto proof=owner_proof(true);
    const auto receipt=write_intended("raw_movement",quantum,motion,first,proof);consumed_motion=motion.serial;
    record("raw_quantum",",\"quantum_id\":"+std::to_string(quantum)+",\"raw_first_sequence\":"+std::to_string(first)+",\"raw_last_sequence\":"+std::to_string(motion.serial)+",\"raw_trigger_qpc\":"+std::to_string(motion.observed_qpc)+",\"coalesced_count\":"+std::to_string(count)+",\"cursor\":"+point(proof.cursor)+",\"operation_id\":"+std::to_string(receipt.id)+",\"native_calls\":"+std::to_string(receipt.native_calls));
    owner_processed_sequence=motion.serial;SetEvent(quantum_finished);
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
    }catch(const std::exception& e){release_activation_button();activation_armed=false;restore_topmost();emit_bootstrap(false,e.what());throw;}
}
POINT find_point(const RECT& r,int wanted){
    // Bounded 3 columns x 160 rows; hit-test decides, not an assumed title size.
    for(int offset=1;offset<=160;++offset)for(int part:{2,3,4}){
        POINT p{r.left+(r.right-r.left)*part/6,wanted==HTCAPTION?r.top+offset:r.bottom-offset};
        if(hit_test(p)==wanted&&GetAncestor(WindowFromPoint(p),GA_ROOT)==owned)return p;
    }
    throw std::runtime_error("BLOCKED_BY_HIT_TEST");
}
void drive_end_gesture(POINT& expected,bool& down){
    gesture=selected_gesture;cancelled=false;cancel_pending=false;cancel_return_boundary=0;native_drag_after_return=0;
    ResetEvent(entered);ResetEvent(exited);ResetEvent(stepped);ResetEvent(nonclient_down);ResetEvent(raw_up);
    const auto initial=capture();require(initial.p_ok&&initial.v_ok&&contained(initial.p),"setup_geometry_unavailable");
    const int wanted=selected_gesture==1?HTCAPTION:HTBOTTOM;const POINT start=find_point(initial.p,wanted);
    fence(expected,false);inject(MOUSEEVENTF_MOVE,start);expected=start;pace(interval_ms);
    fence(expected,false);require(hit_test(start)==wanted&&GetAncestor(WindowFromPoint(start),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
    const POINT end{start.x+(selected_gesture==1?180:0),start.y+(selected_gesture==2?120:0)};
    record("path",",\"hit_test\":"+std::to_string(wanted)+",\"start\":"+point(start)+",\"end\":"+point(end)+",\"samples\":20,\"cancel_after_sample\":2,\"interval_ms\":30,\"foreground\":"+std::to_string(number(GetForegroundWindow()))+geometry(initial));
    {std::lock_guard lock(continuation_mutex);pending_motion={};published_up={};motion_pending=false;raw_up_seen=false;notice_posted=false;pending_count=0;}
    takeover_scope=true;
    inject(MOUSEEVENTF_LEFTDOWN,expected);down=true;
    require(WaitForSingleObject(nonclient_down,2000)==WAIT_OBJECT_0,"missing_native_mouse_down");
    for(int sample=1;sample<=2;++sample){
        pace(interval_ms);fence(expected,true,sample==1&&WaitForSingleObject(entered,0)!=WAIT_OBJECT_0,false);
        ResetEvent(stepped);const int before=callbacks;
        expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
        ResetEvent(raw_motion);const auto receipt=inject(MOUSEEVENTF_MOVE,expected);wait_raw_motion(expected,receipt);
        if(sample==1)require(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"missing_ENTER");
        require(WaitForSingleObject(stepped,2000)==WAIT_OBJECT_0&&callbacks>before,"missing_DRAG");
        pace(interval_ms);fence(expected,true);
        record("sample",",\"index\":"+std::to_string(sample)+",\"cursor\":"+point(expected)+",\"left_down\":true,\"post_cancel\":false"+geometry(capture()));
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
    require(wait==WAIT_OBJECT_0,"missing_native_END");require(native_drag_after_return==0&&native_drag_after_end==0,"native_drag_after_cancel_or_end");
    require(PostMessageW(owned,handoff_message,0,0),"handoff_post_failed");
    require(WaitForSingleObject(handoff_finished,2000)==WAIT_OBJECT_0&&handoff_ready&&takeover_healthy,"handoff_completion_failed");
    for(int sample=3;sample<=samples;++sample){
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
    pace(interval_ms);fence(expected,false,false,true);record("path_complete",geometry(capture()));
}
void drive(){
    bool down=false;POINT expected=saved_cursor;
    try {
        acquire_foreground(expected);fence(expected,false);
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
        record("cursor_restore_begin",",\"point\":"+point(saved_cursor)+",\"target\":"+std::to_string(number(owned)));
        fence(expected,false,false,true);inject(MOUSEEVENTF_MOVE,saved_cursor,true);expected=saved_cursor;pace(interval_ms);fence(expected,false,false,true);
        restored=true;driver_pass=true;
    }catch(const std::exception& e){
        failure=e.what();record("blocked",",\"reason\":\""+failure+"\"");
        takeover_scope=false;
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
        // No second WM_CANCELMODE. Destroy only our empty test windows.
    }
    if(!PostMessageW(owned,finish_message,0,0)){driver_pass=false;record("blocked",",\"reason\":\"finish_message_failed\"");DWORD_PTR ignored{};SendMessageTimeoutW(owned,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,1000,&ignored);}
    SetEvent(driver_finished);
}

}
int receiver_main(int argc,wchar_t** argv);
int wmain(int argc,wchar_t** argv){
    if(argc==5&&std::wstring_view(argv[1])==L"--raw-receiver")return receiver_main(argc,argv);
    end_mode=argc==6&&std::wstring_view(argv[1])==L"--run-owned-takeover-test"&&std::wstring_view(argv[2])==L"--gesture"&&std::wstring_view(argv[4])==L"--evidence-log";
    if(end_mode){
        if(std::wstring_view(argv[3])==L"move")selected_gesture=1;
        else if(std::wstring_view(argv[3])==L"bottom-resize")selected_gesture=2;
        else return 2;
    }else if(argc!=4||std::wstring_view(argv[1])!=L"--run-owned-cancel-test"||std::wstring_view(argv[2])!=L"--evidence-log"){
        std::cout<<"Explicit test only: --run-owned-cancel-test --evidence-log NEW_FILE\n"
                 <<"Or --run-owned-takeover-test --gesture move|bottom-resize --evidence-log NEW_FILE\n";return 2;
    }
    log_file=CreateFileW(argv[end_mode?5:3],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log_file==INVALID_HANDLE_VALUE)return 2;
    timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);ui_thread=GetCurrentThreadId();
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    record("startup",",\"evidence_kind\":\""+std::string(end_mode?"automated_owned_end_handoff":"automated_owned_cancel")+"\",\"human_input\":false,\"real_explorer\":false,\"sendinput_in_probe\":true,\"mode\":\""+(end_mode?"free_takeover":"cancel_only")+"\",\"input_correlation\":\"actual_absolute_receipt_v1\",\"takeover_geometry_writes\":0,\"foreground_contract\":\"verified_global_foreground_v2\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"ui_tid\":"+std::to_string(ui_thread)+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart)+(end_mode?",\"handoff_contract\":\"end_barrier_v1\",\"operation\":\""+std::string(selected_gesture==1?"Move":"BottomResize")+"\"":""));
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
        guard=CreateWindowExW(WS_EX_NOACTIVATE,guard_class.lpszClassName,L"PaneBind empty test input guard",WS_POPUP,setup_x-30,setup_y-30,880,650,nullptr,nullptr,cls.hInstance,nullptr);
        require(guard!=nullptr,"guard_window_creation_failed");ShowWindow(guard,SW_SHOWNOACTIVATE);
        if(!IsWindowVisible(guard))ShowWindow(guard,SW_SHOWNOACTIVATE);
        require(IsWindowVisible(guard),"guard_window_visibility_failed");
        record("guard",",\"hwnd\":"+std::to_string(number(guard))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread));
        owned=CreateWindowExW(0,cls.lpszClassName,L"PaneBind Pivot 1 owned research - do not touch input",WS_OVERLAPPEDWINDOW,
            setup_x,setup_y,640,440,nullptr,nullptr,cls.hInstance,nullptr);
        require(owned!=nullptr,"owned_window_creation_failed");
        // Test setup only, before any native interactive gesture.
        require(SetWindowPos(owned,nullptr,setup_x,setup_y,640,440,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"setup_failed");
        entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);exited=CreateEventW(nullptr,TRUE,FALSE,nullptr);stepped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);pointer_arrived=CreateEventW(nullptr,TRUE,FALSE,nullptr);nonclient_down=CreateEventW(nullptr,TRUE,FALSE,nullptr);correction_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);driver_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_down_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);activation_up_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        handoff_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);quantum_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);takeover_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
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
        record("owned",",\"hwnd\":"+std::to_string(number(owned))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)+",\"saved_cursor\":"+point(saved_cursor)+",\"work_area\":"+rect(work_area)+",\"dpi\":"+std::to_string(frozen_dpi)+geometry(capture())+",\"virtual_screen\":"+rect(virtual_area)+(end_mode?",\"monitor\":"+std::to_string(reinterpret_cast<std::uintptr_t>(frozen_monitor)):""));
        std::thread driver{drive};MSG message{};bool owner_ok=true;
        bool quit=false;while(!quit){
            const DWORD wait=MsgWaitForMultipleObjects(1,&driver_finished,FALSE,15000,QS_ALLINPUT);
            if(wait==WAIT_TIMEOUT||wait==WAIT_FAILED){owner_ok=false;stop_requested=true;record("blocked",",\"reason\":\"owner_message_wait_failed\"");if(identity())DestroyWindow(owned);break;}
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){quit=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}
            if(wait==WAIT_OBJECT_0){if(identity())DestroyWindow(owned);quit=true;}
        }
        driver.join();result=owner_ok&&driver_pass&&log_ok?0:2;
    }catch(const std::exception& e){failure=e.what();emit_bootstrap(false,e.what());record("blocked",",\"reason\":\""+failure+"\"");if(identity())DestroyWindow(owned);}
    stop_receiver();if(guard)DestroyWindow(guard);
    if(!receiver_ok||!receiver_removed||!receiver_destroyed)result=2;
    record("shutdown",",\"result\":\""+std::string(result==0?"CAPTURED_NOT_ACCEPTED":"BLOCKED")+"\",\"cursor_restored\":"+flag(restored)+",\"owned_window_destroyed\":"+flag(!IsWindow(owned))+",\"guard_window_destroyed\":"+flag(!IsWindow(guard))+",\"receiver_stopped\":"+flag(receiver_ok&&receiver_removed&&receiver_destroyed)+",\"external_windows_touched\":false"+(end_mode?",\"takeover_geometry_writes\":"+std::to_string(takeover_native_calls.load()):""));
    for(HANDLE handle:{entered,exited,stepped,timer,activation_event,pointer_arrived,nonclient_down,correction_finished,driver_finished,activation_down_received,activation_up_received,handoff_finished,quantum_finished,takeover_finished})if(handle)CloseHandle(handle);
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
