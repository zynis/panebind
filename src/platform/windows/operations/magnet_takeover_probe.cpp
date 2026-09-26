// Test executable only: never linked into the product / Explorer runtime.
#include "platform/windows/operations/test_foreground_bootstrap_model.h"
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

std::int64_t qpc(){LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart;}
std::string flag(bool b){return b?"true":"false";}
std::string rect(const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+std::to_string(r.right)+","+std::to_string(r.bottom)+"]";}
std::string point(POINT p){return "["+std::to_string(p.x)+","+std::to_string(p.y)+"]";}
std::uintptr_t number(HWND h){return reinterpret_cast<std::uintptr_t>(h);}
bool equal(const RECT& a,const RECT& b){return EqualRect(&a,&b)!=FALSE;}
void record(std::string_view type,const std::string& fields={}){
    std::lock_guard lock{log_mutex};
    if(!log_ok)return;
    if(sequence>=4096){log_ok=false;return;}
    const auto line="{\"schema\":\"r1c4b-takeover-owned/v1\",\"sequence\":"+std::to_string(++sequence)+",\"type\":\""+std::string(type)+"\",\"gesture\":"+std::to_string(gesture.load())+",\"qpc\":"+std::to_string(qpc())+fields+"}\n";
    DWORD written{};log_ok=WriteFile(log_file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
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
            ++raw_packets;
            record("raw_input",common+",\"input_code\":"+std::to_string(p.input_code)+",\"raw_flags\":"+std::to_string(p.flags)+",\"dx\":"+std::to_string(p.dx)+",\"dy\":"+std::to_string(p.dy)+",\"button_flags\":"+std::to_string(p.buttons)+",\"cursor_sampled\":"+flag(p.cursor_sampled)+",\"cursor_success\":"+flag(p.cursor_ok)+",\"cursor\":"+(p.cursor_sampled&&p.cursor_ok?point(p.cursor):"null")+",\"left_down\":"+flag(p.left_down)+",\"foreground_hwnd\":"+std::to_string(p.foreground)+",\"foreground_pid\":"+std::to_string(p.foreground_pid)+",\"device_handle_present\":"+flag(p.device_present)+",\"test_tag_matches\":"+flag(p.tag_matches));
            record("raw_cursor_status",common+",\"cursor_error\":"+std::to_string(p.cursor_error));
            if(p.input_code!=RIM_INPUTSINK||p.foreground_pid==p.pid||p.foreground_pid!=GetCurrentProcessId())receiver_ok=false;
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
        record("native_button_down",",\"target\":"+std::to_string(number(h))+",\"hit_test\":"+std::to_string(w));SetEvent(nonclient_down);
    }
    if(message==WM_ENTERSIZEMOVE){active=true;record("ENTER",",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(capture()));SetEvent(entered);return 0;}
    if(message==WM_MOVING||message==WM_SIZING){
        if(cancel_return_boundary&&qpc()>cancel_return_boundary)++native_drag_after_return;
        POINT cursor{};GetCursorPos(&cursor);
        record("DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"cursor\":"+point(cursor)+",\"proposed\":"+rect(*reinterpret_cast<RECT*>(l))+",\"after_cancel\":"+flag(cancelled)+geometry(capture()));
        ++callbacks;SetEvent(stepped);return DefWindowProcW(h,message,w,l);
    }
    if(message==WM_CANCELMODE)record("cancel_message",",\"target\":"+std::to_string(number(h))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(capture()));
    if(message==WM_CAPTURECHANGED){
        const auto next=reinterpret_cast<HWND>(l);if(next&&!input_root_owned(next))foreign_capture_transferred=true;
        record("CAPTURE_CHANGED",",\"new_capture\":"+std::to_string(number(next))+",\"owner_capture\":"+std::to_string(number(GetCapture()))+geometry(capture()));
    }
    if(message==WM_WINDOWPOSCHANGED&&gesture>0)record("POSITION_CHANGED",",\"after_cancel\":"+flag(cancelled)+geometry(capture()));
    if(message==WM_EXITSIZEMOVE){record("EXIT",",\"owner_capture\":"+std::to_string(number(GetCapture()))+",\"left_down\":"+flag(pressed(VK_LBUTTON))+geometry(capture()));active=false;SetEvent(exited);return 0;}
    if(message==finish_message){DestroyWindow(h);return 0;}
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
void acquire_foreground(POINT& expected){
    fg::ActivationProof direct;direct.own_identity=identity();direct.same_integrity=direct.own_identity;direct.desktop_ready=desktop_available();direct.visible=identity()&&IsWindowVisible(owned);
    if(fg::direct_ready(set_foreground_success,GetForegroundWindow()==owned,direct)){emit_bootstrap(true,"none");return;}
    if(set_foreground_success){emit_bootstrap(false,"BLOCKED_BY_ACTIVATION_FAILURE");throw std::runtime_error("BLOCKED_BY_ACTIVATION_FAILURE");}
    bootstrap.click_required=true;bootstrap.topmost_restored=false;
    try {
        require(identity()&&direct.desktop_ready,"BLOCKED_BY_ACTIVATION_IDENTITY");
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
        bootstrap.event_seen=WaitForSingleObject(activation_event,2000)==WAIT_OBJECT_0;
        activation_armed=false;restore_topmost();
        const fg::ActivationCompletion completion{bootstrap.move_success,bootstrap.down_success,bootstrap.up_success,bootstrap.event_seen,identity()&&IsWindowVisible(owned)&&GetForegroundWindow()==owned,bootstrap.topmost_restored};
        const char* reason=fg::completion_guard(completion);require(std::string_view(reason)=="none",reason);
        require(GetCursorPos(&expected)!=FALSE,"BLOCKED_BY_INPUT_INTERFERENCE");emit_bootstrap(true,"none");
    }catch(const std::exception& e){release_activation_button();activation_armed=false;restore_topmost();emit_bootstrap(false,e.what());throw;}
}
void prepare_local_activation(){
    // Owner UI thread only. Clear only this empty test application's local
    // background state; never assign focus/activation or alter foreground policy.
    if(set_foreground_success)return;
    const HWND foreground=GetForegroundWindow(),before_active=GetActiveWindow(),before_focus=GetFocus();
    GUITHREADINFO gui{sizeof(gui)};
    const bool query=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const bool own_before=identity(),desktop_ready=desktop_available();
    const bool clear_input=!pressed(VK_LBUTTON)&&!other_input();
    const bool authorized=GetCurrentThreadId()==ui_thread&&own_before&&desktop_ready&&foreground&&foreground!=owned&&foreground!=guard
        &&(!before_active||input_root_owned(before_active))&&(!before_focus||input_root_owned(before_focus))
        &&query&&!gui.hwndCapture&&!gui.hwndMenuOwner&&!gui.hwndMoveSize&&!(gui.flags&30)&&clear_input
        &&identity()&&GetForegroundWindow()==foreground&&!pressed(VK_LBUTTON)&&!other_input();
    const bool attempted=authorized&&(before_active||before_focus);
    HWND focus_return{},active_return{};DWORD focus_error{},active_error{};bool active_called{};
    if(attempted){
        SetLastError(0);focus_return=SetFocus(nullptr);focus_error=GetLastError();
        GUITHREADINFO middle{sizeof(middle)};
        if(identity()&&GetGUIThreadInfo(ui_thread,&middle)&&!middle.hwndCapture&&!middle.hwndMenuOwner&&!middle.hwndMoveSize&&!(middle.flags&30)
            &&GetForegroundWindow()==foreground&&!pressed(VK_LBUTTON)&&!other_input()){
            active_called=true;SetLastError(0);active_return=SetActiveWindow(nullptr);active_error=GetLastError();
        }
    }
    const HWND after_active=GetActiveWindow(),after_focus=GetFocus(),after_foreground=GetForegroundWindow();
    const bool own_after=identity();
    const bool unchanged=foreground==after_foreground,cleared=!after_active&&!after_focus;
    const bool success=authorized&&own_after&&unchanged&&cleared&&(!attempted||active_called);
    record("local_activation_preparation",",\"target\":"+std::to_string(number(owned))+",\"guard\":"+std::to_string(number(guard))+",\"source_pid\":"+std::to_string(GetCurrentProcessId())+",\"source_tid\":"+std::to_string(GetCurrentThreadId())
        +",\"attempted\":"+flag(attempted)+",\"authorized\":"+flag(authorized)+",\"before_active\":"+std::to_string(number(before_active))+",\"before_focus\":"+std::to_string(number(before_focus))+",\"after_active\":"+std::to_string(number(after_active))+",\"after_focus\":"+std::to_string(number(after_focus))
        +",\"own_identity_before\":"+flag(own_before)+",\"desktop_ready\":"+flag(desktop_ready)+",\"own_identity_after\":"+flag(own_after)
        +",\"focus_return\":"+std::to_string(number(focus_return))+",\"focus_error\":"+std::to_string(focus_error)+",\"active_called\":"+flag(active_called)+",\"active_return\":"+std::to_string(number(active_return))+",\"active_error\":"+std::to_string(active_error)
        +",\"foreground_before\":"+std::to_string(number(foreground))+",\"foreground_after\":"+std::to_string(number(after_foreground))+",\"gui_query_succeeded\":"+flag(query)+",\"gui_flags\":"+std::to_string(gui.flags)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"menu_owner_hwnd\":"+std::to_string(number(gui.hwndMenuOwner))+",\"move_size_hwnd\":"+std::to_string(number(gui.hwndMoveSize))
        +",\"buttons_modifiers_clear\":"+flag(clear_input)+",\"global_unchanged\":"+flag(unchanged)+",\"local_cleared\":"+flag(cleared)+",\"success\":"+flag(success));
    require(success,"BLOCKED_BY_LOCAL_ACTIVATION_PREPARATION");
}
POINT find_point(const RECT& r,int wanted){
    // Bounded 3 columns x 160 rows; hit-test decides, not an assumed title size.
    for(int offset=1;offset<=160;++offset)for(int part:{2,3,4}){
        POINT p{r.left+(r.right-r.left)*part/6,wanted==HTCAPTION?r.top+offset:r.bottom-offset};
        if(hit_test(p)==wanted&&GetAncestor(WindowFromPoint(p),GA_ROOT)==owned)return p;
    }
    throw std::runtime_error("BLOCKED_BY_HIT_TEST");
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
        wait_raw_up(preflight_up);
        pace(interval_ms);fence(expected,false);record("raw_preflight_complete",",\"movement_count\":"+std::to_string(raw_movements.load())+",\"up_count\":"+std::to_string(raw_ups.load()));
        for(int kind=1;kind<=2;++kind){
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
                    record("cancel_exit_wait",",\"started_qpc\":"+std::to_string(wait_started)+",\"finished_qpc\":"+std::to_string(wait_finished)+",\"timeout_ms\":2000,\"observation_wakeup_sample\":3,\"wait_result\":"+std::to_string(exit_wait)+",\"gui_query_succeeded\":"+flag(after_query)+",\"target\":"+std::to_string(number(owned))+",\"source_tid\":"+std::to_string(ui_thread)+",\"source_identity\":"+flag(identity())+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"capture_hwnd\":"+std::to_string(number(after_wait.hwndCapture))+",\"move_size_hwnd\":"+std::to_string(number(after_wait.hwndMoveSize))+",\"gui_flags\":"+std::to_string(after_wait.flags)+",\"left_down\":"+flag(pressed(VK_LBUTTON)));
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
    if(argc!=4||std::wstring_view(argv[1])!=L"--run-owned-cancel-test"||std::wstring_view(argv[2])!=L"--evidence-log"){
        std::cout<<"Explicit test only: --run-owned-cancel-test --evidence-log NEW_FILE\n";return 2;
    }
    log_file=CreateFileW(argv[3],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log_file==INVALID_HANDLE_VALUE)return 2;
    timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);ui_thread=GetCurrentThreadId();
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    record("startup",",\"evidence_kind\":\"automated_owned_cancel\",\"human_input\":false,\"real_explorer\":false,\"sendinput_in_probe\":true,\"mode\":\"cancel_only\",\"input_correlation\":\"actual_absolute_receipt_v1\",\"local_activation_policy\":\"own_background_reset_v1\",\"takeover_geometry_writes\":0,\"foreground_contract\":\"verified_activation_v1\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"ui_tid\":"+std::to_string(ui_thread)+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart));
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
        require(entered&&exited&&stepped&&activation_event&&pointer_arrived&&nonclient_down&&correction_finished&&driver_finished&&activation_down_received&&activation_up_received,"event_creation_failed");
        start_receiver();
        STARTUPINFOW startup{sizeof(startup)};GetStartupInfoW(&startup);
        ShowWindow(owned,SW_SHOWNOACTIVATE);
        record("show_window",",\"startup_flags\":"+std::to_string(startup.dwFlags)+",\"startup_show\":"+std::to_string(startup.wShowWindow)+",\"requested_show\":4,\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        // The first ShowWindow may honor the shell parent's hidden STARTUPINFO
        // instead of the requested mode. Do not create local activation before
        // the ordinary foreground attempt / verified activation click.
        if(!IsWindowVisible(owned))ShowWindow(owned,SW_SHOWNOACTIVATE);
        bootstrap.attempted=true;const BOOL activated=SetForegroundWindow(owned);set_foreground_success=activated!=FALSE;UpdateWindow(owned);
        record("foreground_attempt",",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"local_active\":"+std::to_string(number(GetActiveWindow()))+",\"local_focus\":"+std::to_string(number(GetFocus()))+",\"set_foreground_success\":"+flag(activated!=FALSE)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        record("owned",",\"hwnd\":"+std::to_string(number(owned))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)+",\"saved_cursor\":"+point(saved_cursor)+",\"work_area\":"+rect(work_area)+",\"dpi\":"+std::to_string(GetDpiForWindow(owned))+geometry(capture())+",\"virtual_screen\":"+rect(virtual_area));
        prepare_local_activation();
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
    record("shutdown",",\"result\":\""+std::string(result==0?"CAPTURED_NOT_ACCEPTED":"BLOCKED")+"\",\"cursor_restored\":"+flag(restored)+",\"owned_window_destroyed\":"+flag(!IsWindow(owned))+",\"guard_window_destroyed\":"+flag(!IsWindow(guard))+",\"receiver_stopped\":"+flag(receiver_ok&&receiver_removed&&receiver_destroyed)+",\"external_windows_touched\":false");
    for(HANDLE handle:{entered,exited,stepped,timer,activation_event,pointer_arrived,nonclient_down,correction_finished,driver_finished,activation_down_received,activation_up_received})if(handle)CloseHandle(handle);
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
