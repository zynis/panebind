// Test executable only: never linked into the product / Explorer runtime.
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
constexpr UINT correction_message=WM_APP+51, finish_message=WM_APP+52;
constexpr int samples=20, interval_ms=30, pulse=7;
constexpr ULONG_PTR input_tag=0x50424D41;
HWND owned{};
DWORD ui_thread{};
HANDLE log_file=INVALID_HANDLE_VALUE,entered{},exited{},stepped{},timer{};
std::mutex log_mutex;
std::uint64_t sequence{};
std::atomic<bool> log_ok{true};
std::atomic<int> gesture{},callbacks{};
bool active{},queued{},corrected{},t2_seen{}; // UI thread only
RECT proposed{},target{},target_visible{},work_area{};
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
    if(sequence>=2048){log_ok=false;return;}
    const auto line="{\"schema\":\"r1c4b-auto-owned-modal/v1\",\"sequence\":"+std::to_string(++sequence)+",\"type\":\""+std::string(type)+"\",\"gesture\":"+std::to_string(gesture.load())+",\"qpc\":"+std::to_string(qpc())+fields+"}\n";
    DWORD written{};log_ok=WriteFile(log_file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
}
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
bool identity(){DWORD pid{};return owned&&GetWindowThreadProcessId(owned,&pid)==ui_thread&&pid==GetCurrentProcessId();}
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
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l){
    if(message==WM_ENTERSIZEMOVE){active=true;queued=corrected=t2_seen=false;record("ENTER",geometry(capture()));SetEvent(entered);return 0;}
    if(active&&(message==WM_MOVING||message==WM_SIZING)){
        proposed=*reinterpret_cast<RECT*>(l);POINT cursor{};GetCursorPos(&cursor);
        record(corrected&&!t2_seen?"T2":"DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"cursor\":"+point(cursor)+",\"proposed\":"+rect(proposed)+geometry(capture()));
        if(corrected)t2_seen=true;
        if(!queued&&((gesture==1&&message==WM_MOVING)||(gesture==2&&message==WM_SIZING&&w==WMSZ_BOTTOM))){
            queued=true;if(!PostMessageW(h,correction_message,static_cast<WPARAM>(gesture.load()),0))log_ok=false;
        }
        ++callbacks;SetEvent(stepped);
        return DefWindowProcW(h,message,w,l); // Never modify the drag RECT.
    }
    if(message==correction_message){
        if(!identity()||!active||corrected||w!=static_cast<WPARAM>(gesture.load()))return 0;
        const auto before=capture();
        if(!before.p_ok||!before.v_ok){log_ok=false;return 0;}
        target=before.p;target_visible=before.v;
        if(gesture==1){target.top+=pulse;target.bottom+=pulse;target_visible.top+=pulse;target_visible.bottom+=pulse;}
        else {target.bottom+=pulse;target_visible.bottom+=pulse;}
        if(!contained(target)){log_ok=false;return 0;}
        record("T0",",\"drag_active\":true,\"proposed\":"+rect(proposed)+",\"target_positioning\":"+rect(target)+",\"target_visible\":"+rect(target_visible)+geometry(before));
        const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|(gesture==1?SWP_NOSIZE:0);
        const auto start=qpc();SetLastError(0);
        const BOOL ok=SetWindowPos(h,nullptr,target.left,target.top,target.right-target.left,target.bottom-target.top,flags);
        const DWORD error=ok?0:GetLastError();const auto returned=qpc();const auto actual=capture();corrected=true;
        record("T1",",\"native_success\":"+flag(ok!=FALSE)+",\"error\":"+std::to_string(error)+",\"flags\":"+std::to_string(flags)+",\"native_calls\":1,\"native_start_qpc\":"+std::to_string(start)+",\"native_return_qpc\":"+std::to_string(returned)+",\"positioning_exact\":"+flag(actual.p_ok&&equal(actual.p,target))+",\"visible_exact\":"+flag(actual.v_ok&&equal(actual.v,target_visible))+geometry(actual));
        return 0;
    }
    if(message==WM_WINDOWPOSCHANGED&&active&&corrected)record("POSITION_CHANGED",geometry(capture()));
    if(message==WM_EXITSIZEMOVE){record("T3",",\"corrected\":"+flag(corrected)+",\"next_drag_seen\":"+flag(t2_seen)+geometry(capture()));active=false;SetEvent(exited);return 0;}
    if(message==finish_message){
        if(active)SendMessageW(h,WM_CANCELMODE,0,0); // own empty test window only
        DestroyWindow(h);return 0;
    }
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,message,w,l);
}
void fence(POINT expected,bool down){
    require(log_ok,"evidence_capture_failed");require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    POINT actual{};require(GetCursorPos(&actual)!=FALSE,"BLOCKED_BY_INPUT_INTERFERENCE");
    require(std::abs(actual.x-expected.x)<=1&&std::abs(actual.y-expected.y)<=1&&pressed(VK_LBUTTON)==down&&!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    if(down){GUITHREADINFO gui{sizeof(gui)};require(GetGUIThreadInfo(ui_thread,&gui)&&gui.hwndCapture==owned,"BLOCKED_BY_INPUT_INTERFERENCE");}
    record("input_fence",",\"cursor\":"+point(actual)+",\"expected_cursor\":"+point(expected)+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"target\":"+std::to_string(number(owned))+",\"left_down\":"+flag(down));
}
void inject(DWORD flags,POINT p={}){
    // Recheck immediately at the API boundary as well as at the recorded
    // path fence. Never use input to acquire foreground authority.
    require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&IsWindowVisible(owned)&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    require(!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;input.mi.dwExtraInfo=input_tag;
    if(flags&MOUSEEVENTF_MOVE){
        const auto x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        require(width>1&&height>1&&p.x>=x&&p.y>=y&&p.x<x+width&&p.y<y+height,"BLOCKED_BY_INTERACTIVE_DESKTOP");
        input.mi.dwFlags|=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
        input.mi.dx=static_cast<LONG>((static_cast<std::int64_t>(p.x-x)*65535+(width-1)/2)/(width-1));
        input.mi.dy=static_cast<LONG>((static_cast<std::int64_t>(p.y-y)*65535+(height-1)/2)/(height-1));
    }
    SetLastError(0);const UINT sent=SendInput(1,&input,sizeof(input));const DWORD error=sent==1?0:GetLastError();
    record("input",",\"flags\":"+std::to_string(flags)+",\"point\":"+point(p)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error));
    require(sent==1,"BLOCKED_BY_SENDINPUT");
}
LRESULT hit_test(POINT p){
    require(p.x>=-32768&&p.x<=32767&&p.y>=-32768&&p.y<=32767,"BLOCKED_BY_HIT_TEST");
    DWORD_PTR result{};
    require(SendMessageTimeoutW(owned,WM_NCHITTEST,0,MAKELPARAM(static_cast<SHORT>(p.x),static_cast<SHORT>(p.y)),SMTO_ABORTIFHUNG,500,&result)!=0,"BLOCKED_BY_HIT_TEST");
    return static_cast<LRESULT>(result);
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
        fence(expected,false);
        for(int kind=1;kind<=2;++kind){
            gesture=kind;ResetEvent(entered);ResetEvent(exited);ResetEvent(stepped);
            const auto initial=capture();require(initial.p_ok&&initial.v_ok&&contained(initial.p),"setup_geometry_unavailable");
            const int wanted=kind==1?HTCAPTION:HTBOTTOM;const POINT start=find_point(initial.p,wanted);
            fence(expected,false);inject(MOUSEEVENTF_MOVE,start);expected=start;pace(interval_ms);
            fence(expected,false);require(hit_test(start)==wanted&&GetAncestor(WindowFromPoint(start),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
            POINT end{start.x+(kind==1?180:0),start.y+(kind==2?120:0)};
            record("path",",\"hit_test\":"+std::to_string(wanted)+",\"start\":"+point(start)+",\"end\":"+point(end)+",\"samples\":20,\"interval_ms\":30,\"planned_duration_ms\":600,\"foreground\":"+std::to_string(number(GetForegroundWindow()))+geometry(initial));
            inject(MOUSEEVENTF_LEFTDOWN);down=true;
            require(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"missing_ENTER");
            for(int sample=1;sample<=samples;++sample){
                pace(interval_ms);fence(expected,true);ResetEvent(stepped);const int before=callbacks;
                expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
                inject(MOUSEEVENTF_MOVE,expected);
                require(WaitForSingleObject(stepped,2000)==WAIT_OBJECT_0&&callbacks>before,"missing_DRAG");
            }
            pace(interval_ms);fence(expected,true);inject(MOUSEEVENTF_LEFTUP);down=false;
            require(WaitForSingleObject(exited,2000)==WAIT_OBJECT_0,"missing_EXIT");
            pace(interval_ms);fence(expected,false);record("path_complete",geometry(capture()));
        }
        fence(expected,false);inject(MOUSEEVENTF_MOVE,saved_cursor);expected=saved_cursor;pace(interval_ms);fence(expected,false);
        restored=true;driver_pass=true;
    }catch(const std::exception& e){
        failure=e.what();record("blocked",",\"reason\":\""+failure+"\"");
        // Do not send anything into an unknown desktop/foreground. A verified
        // own held button gets one release; never restore cursor on failure.
        if(down&&identity()&&desktop_available()&&GetForegroundWindow()==owned){
            INPUT up{};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_LEFTUP;up.mi.dwExtraInfo=input_tag;
            const auto released=SendInput(1,&up,sizeof(up));record("cleanup_release",",\"sent\":"+std::to_string(released));
        }
    }
    PostMessageW(owned,finish_message,0,0);
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=4||std::wstring_view(argv[1])!=L"--run-owned-input-test"||std::wstring_view(argv[2])!=L"--evidence-log"){
        std::cout<<"Explicit test only: --run-owned-input-test --evidence-log NEW_FILE\n";return 2;
    }
    log_file=CreateFileW(argv[3],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log_file==INVALID_HANDLE_VALUE)return 2;
    timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);ui_thread=GetCurrentThreadId();
    LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    record("startup",",\"evidence_kind\":\"automated_owned_modal\",\"human_input\":false,\"real_explorer\":false,\"sendinput_in_probe\":true,\"pid\":"+std::to_string(GetCurrentProcessId())+",\"ui_tid\":"+std::to_string(ui_thread)+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart));
    int result=2;
    try {
        require(timer&&desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
        record("desktop_gate",",\"active_unlocked\":true,\"input_desktop_matches\":true");
        std::cout<<"PaneBind automated owned-window modal-loop test.\nMouse input will be controlled for a few seconds.\nDo not touch mouse or keyboard.\nStarting in 3 seconds... Press Esc to cancel.\n"<<std::flush;
        for(int n=3;n>0;--n){std::cout<<n<<'\n'<<std::flush;pace(1000);require(!pressed(VK_ESCAPE),"BLOCKED_BY_INPUT_INTERFERENCE");}
        require(!pressed(VK_LBUTTON)&&!other_input()&&GetCursorPos(&saved_cursor),"BLOCKED_BY_INPUT_INTERFERENCE");
        MONITORINFO monitor{sizeof(monitor)};
        require(GetMonitorInfoW(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&monitor)!=FALSE,"BLOCKED_BY_INTERACTIVE_DESKTOP");work_area=monitor.rcWork;
        require(work_area.right-work_area.left>=1120&&work_area.bottom-work_area.top>=850,"BLOCKED_BY_INTERACTIVE_DESKTOP");
        const int setup_x=work_area.left+(work_area.right-work_area.left-640-180)/2;
        const int setup_y=work_area.top+(work_area.bottom-work_area.top-440-120)/2;
        WNDCLASSW cls{};cls.lpfnWndProc=procedure;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"PaneBindC4BAutoModalProbe";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        require(RegisterClassW(&cls)!=0,"owned_window_creation_failed");
        owned=CreateWindowExW(0,cls.lpszClassName,L"PaneBind AUTOMATED owned modal probe - do not touch input",WS_OVERLAPPEDWINDOW,
            setup_x,setup_y,640,440,nullptr,nullptr,cls.hInstance,nullptr);
        require(owned!=nullptr,"owned_window_creation_failed");
        // Test setup only, before any native interactive gesture.
        require(SetWindowPos(owned,nullptr,setup_x,setup_y,640,440,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"setup_failed");
        STARTUPINFOW startup{sizeof(startup)};GetStartupInfoW(&startup);
        ShowWindow(owned,SW_SHOW);
        record("show_window",",\"startup_flags\":"+std::to_string(startup.dwFlags)+",\"startup_show\":"+std::to_string(startup.wShowWindow)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        // The first ShowWindow may honor the shell parent's hidden STARTUPINFO
        // instead of SW_SHOW. The explicitly requested test UI must be visible.
        if(!IsWindowVisible(owned))ShowWindow(owned,SW_SHOWNORMAL);
        const BOOL activated=SetForegroundWindow(owned);SetFocus(owned);UpdateWindow(owned);
        record("foreground_attempt",",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"set_foreground_success\":"+flag(activated!=FALSE)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        require(GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
        record("owned",",\"hwnd\":"+std::to_string(number(owned))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)+",\"saved_cursor\":"+point(saved_cursor)+",\"work_area\":"+rect(work_area)+",\"dpi\":"+std::to_string(GetDpiForWindow(owned))+geometry(capture()));
        entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);exited=CreateEventW(nullptr,TRUE,FALSE,nullptr);stepped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        require(entered&&exited&&stepped,"event_creation_failed");
        std::thread driver{drive};MSG message{};BOOL state{};
        while((state=GetMessageW(&message,nullptr,0,0))>0){TranslateMessage(&message);DispatchMessageW(&message);}
        driver.join();result=state==0&&driver_pass&&log_ok?0:2;
    }catch(const std::exception& e){failure=e.what();record("blocked",",\"reason\":\""+failure+"\"");if(identity())DestroyWindow(owned);}
    record("shutdown",",\"result\":\""+std::string(result==0?"CAPTURED_NOT_ACCEPTED":"BLOCKED")+"\",\"cursor_restored\":"+flag(restored)+",\"owned_window_destroyed\":"+flag(!IsWindow(owned))+",\"external_windows_touched\":false");
    for(HANDLE handle:{entered,exited,stepped,timer})if(handle)CloseHandle(handle);
    const bool evidence_ok=log_ok;CloseHandle(log_file);return evidence_ok?result:2;
}
