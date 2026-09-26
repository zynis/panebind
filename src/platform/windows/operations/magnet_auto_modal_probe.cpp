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
constexpr UINT correction_message=WM_APP+51, finish_message=WM_APP+52;
constexpr int samples=20, interval_ms=30, pulse=7;
constexpr ULONG_PTR input_tag=0x50424D41;
HWND owned{};
DWORD ui_thread{};
HANDLE log_file=INVALID_HANDLE_VALUE,entered{},exited{},stepped{},timer{};
HANDLE activation_event{},pointer_arrived{},nonclient_down{},correction_finished{},driver_finished{};
HANDLE activation_down_received{},activation_up_received{};
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
    if(h==owned&&message==WM_MOUSEMOVE&&activation_armed){SetEvent(pointer_arrived);}
    if(h==owned&&activation_armed&&(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP)){
        record("activation_button",",\"message\":\""+std::string(message==WM_LBUTTONDOWN?"WM_LBUTTONDOWN":"WM_LBUTTONUP")+"\",\"target\":"+std::to_string(number(h)));
        SetEvent(message==WM_LBUTTONDOWN?activation_down_received:activation_up_received);
    }
    if(h==owned&&message==WM_NCLBUTTONDOWN&&(w==HTCAPTION||w==HTBOTTOM)){
        record("native_button_down",",\"target\":"+std::to_string(number(h))+",\"hit_test\":"+std::to_string(w));SetEvent(nonclient_down);
    }
    if(message==WM_ENTERSIZEMOVE){active=true;queued=corrected=t2_seen=false;record("ENTER",geometry(capture()));SetEvent(entered);return 0;}
    if(active&&(message==WM_MOVING||message==WM_SIZING)){
        proposed=*reinterpret_cast<RECT*>(l);POINT cursor{};GetCursorPos(&cursor);
        record(corrected&&!t2_seen?"T2":"DRAG",",\"event\":\""+std::string(message==WM_MOVING?"WM_MOVING":"WM_SIZING")+"\",\"edge\":"+std::to_string(w)+",\"cursor\":"+point(cursor)+",\"proposed\":"+rect(proposed)+geometry(capture()));
        if(corrected)t2_seen=true;
        if(!queued&&((gesture==1&&message==WM_MOVING)||(gesture==2&&message==WM_SIZING&&w==WMSZ_BOTTOM))){
            queued=true;if(!PostMessageW(h,correction_message,static_cast<WPARAM>(gesture.load()),0)){probe_failure="correction_dispatch_failed";record("probe_failure",",\"reason\":\"correction_dispatch_failed\"");SetEvent(correction_finished);}
        }
        ++callbacks;SetEvent(stepped);
        return DefWindowProcW(h,message,w,l); // Never modify the drag RECT.
    }
    if(message==correction_message){
        if(!identity()||!active||corrected||w!=static_cast<WPARAM>(gesture.load()))return 0;
        const auto before=capture();
        if(!before.p_ok||!before.v_ok){probe_failure="correction_capture_failed";record("probe_failure",",\"reason\":\"correction_capture_failed\"");SetEvent(correction_finished);return 0;}
        target=before.p;target_visible=before.v;
        if(gesture==1){target.top+=pulse;target.bottom+=pulse;target_visible.top+=pulse;target_visible.bottom+=pulse;}
        else {target.bottom+=pulse;target_visible.bottom+=pulse;}
        if(!contained(target)){probe_failure="correction_work_area_failed";record("probe_failure",",\"reason\":\"correction_work_area_failed\"");SetEvent(correction_finished);return 0;}
        record("T0",",\"drag_active\":true,\"proposed\":"+rect(proposed)+",\"target_positioning\":"+rect(target)+",\"target_visible\":"+rect(target_visible)+geometry(before));
        const UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|(gesture==1?SWP_NOSIZE:0);
        const auto start=qpc();SetLastError(0);
        const BOOL ok=SetWindowPos(h,nullptr,target.left,target.top,target.right-target.left,target.bottom-target.top,flags);
        const DWORD error=ok?0:GetLastError();const auto returned=qpc();const auto actual=capture();corrected=true;
        record("T1",",\"native_success\":"+flag(ok!=FALSE)+",\"error\":"+std::to_string(error)+",\"flags\":"+std::to_string(flags)+",\"native_calls\":1,\"native_start_qpc\":"+std::to_string(start)+",\"native_return_qpc\":"+std::to_string(returned)+",\"positioning_exact\":"+flag(actual.p_ok&&equal(actual.p,target))+",\"visible_exact\":"+flag(actual.v_ok&&equal(actual.v,target_visible))+geometry(actual));
        SetEvent(correction_finished);
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
void fence(POINT expected,bool down,bool priming=false){
    require(!stop_requested,"owner_message_wait_failed");
    require(log_ok,"evidence_capture_failed");require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    POINT actual{};require(GetCursorPos(&actual)!=FALSE,"BLOCKED_BY_INPUT_INTERFERENCE");
    require(std::abs(actual.x-expected.x)<=1&&std::abs(actual.y-expected.y)<=1&&pressed(VK_LBUTTON)==down&&!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    GUITHREADINFO gui{sizeof(gui)};const bool queried=GetGUIThreadInfo(ui_thread,&gui)!=FALSE;
    const HWND root=GetAncestor(WindowFromPoint(actual),GA_ROOT);
    if(down){require(queried&&
        (gui.hwndCapture==owned||(priming&&!gui.hwndCapture&&GetAncestor(WindowFromPoint(actual),GA_ROOT)==owned)),"BLOCKED_BY_INPUT_INTERFERENCE");}
    record("input_fence",",\"cursor\":"+point(actual)+",\"expected_cursor\":"+point(expected)+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"target\":"+std::to_string(number(owned))+",\"left_down\":"+flag(down)+",\"priming\":"+flag(priming)+",\"gui_query_succeeded\":"+flag(queried)+",\"capture_hwnd\":"+std::to_string(number(gui.hwndCapture))+",\"window_from_point_root\":"+std::to_string(number(root)));
}
INPUT mouse_input(DWORD flags,POINT p){
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;input.mi.dwExtraInfo=input_tag;
    if(flags&MOUSEEVENTF_MOVE){
        const auto x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        require(width>1&&height>1&&p.x>=x&&p.y>=y&&p.x<x+width&&p.y<y+height,"BLOCKED_BY_INTERACTIVE_DESKTOP");
        input.mi.dwFlags|=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
        // Address the pixel cell centre rather than its normalized boundary.
        input.mi.dx=static_cast<LONG>((2LL*(p.x-x)+1)*65536/(2LL*width));
        input.mi.dy=static_cast<LONG>((2LL*(p.y-y)+1)*65536/(2LL*height));
    }
    return input;
}
void inject(DWORD flags,POINT p={}){
    require(!stop_requested,"owner_message_wait_failed");
    // Recheck immediately at the API boundary as well as at the recorded
    // path fence. The activation exception lives in its separate helper.
    require(desktop_available(),"BLOCKED_BY_INTERACTIVE_DESKTOP");
    require(identity()&&IsWindowVisible(owned)&&GetForegroundWindow()==owned,"BLOCKED_BY_FOREGROUND");
    require(!other_input(),"BLOCKED_BY_INPUT_INTERFERENCE");
    auto input=mouse_input(flags,p);
    const auto start=qpc();SetLastError(0);const UINT sent=SendInput(1,&input,sizeof(input));const DWORD error=sent==1?0:GetLastError();const auto returned=qpc();
    record("input",",\"flags\":"+std::to_string(flags)+",\"point\":"+point(p)+",\"sent\":"+std::to_string(sent)+",\"error\":"+std::to_string(error)+",\"input_tag\":"+std::to_string(input_tag)+",\"injection_start_qpc\":"+std::to_string(start)+",\"injection_return_qpc\":"+std::to_string(returned));
    require(sent==1,"BLOCKED_BY_SENDINPUT");
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
        acquire_foreground(expected);
        fence(expected,false);
        for(int kind=1;kind<=2;++kind){
            gesture=kind;ResetEvent(entered);ResetEvent(exited);ResetEvent(stepped);ResetEvent(nonclient_down);ResetEvent(correction_finished);
            const auto initial=capture();require(initial.p_ok&&initial.v_ok&&contained(initial.p),"setup_geometry_unavailable");
            const int wanted=kind==1?HTCAPTION:HTBOTTOM;const POINT start=find_point(initial.p,wanted);
            fence(expected,false);inject(MOUSEEVENTF_MOVE,start);expected=start;pace(interval_ms);
            fence(expected,false);require(hit_test(start)==wanted&&GetAncestor(WindowFromPoint(start),GA_ROOT)==owned,"BLOCKED_BY_HIT_TEST");
            POINT end{start.x+(kind==1?180:0),start.y+(kind==2?120:0)};
            record("path",",\"hit_test\":"+std::to_string(wanted)+",\"start\":"+point(start)+",\"end\":"+point(end)+",\"samples\":20,\"interval_ms\":30,\"planned_duration_ms\":600,\"foreground\":"+std::to_string(number(GetForegroundWindow()))+geometry(initial));
            inject(MOUSEEVENTF_LEFTDOWN);down=true;
            require(WaitForSingleObject(nonclient_down,2000)==WAIT_OBJECT_0,"missing_native_mouse_down");
            for(int sample=1;sample<=samples;++sample){
                pace(interval_ms);fence(expected,true,sample==1&&WaitForSingleObject(entered,0)!=WAIT_OBJECT_0);ResetEvent(stepped);const int before=callbacks;
                expected={start.x+(end.x-start.x)*sample/samples,start.y+(end.y-start.y)*sample/samples};
                inject(MOUSEEVENTF_MOVE,expected);
                if(sample==1)require(WaitForSingleObject(entered,2000)==WAIT_OBJECT_0,"missing_ENTER");
                require(WaitForSingleObject(stepped,2000)==WAIT_OBJECT_0&&callbacks>before,"missing_DRAG");
                if(sample==1){require(WaitForSingleObject(correction_finished,2000)==WAIT_OBJECT_0,"correction_message_not_dispatched");require(std::string_view(probe_failure.load())=="none",probe_failure.load());}
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
        GUITHREADINFO gui{sizeof(gui)};
        // Activation cleanup has already made its one proven attempt before
        // retiring TOPMOST authority; never retry it through this normal path.
        if(down&&identity()&&desktop_available()&&GetForegroundWindow()==owned&&GetGUIThreadInfo(ui_thread,&gui)&&(!gui.hwndCapture||gui.hwndCapture==owned)){
            INPUT up{};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_LEFTUP;up.mi.dwExtraInfo=input_tag;
            const auto released=SendInput(1,&up,sizeof(up));record("cleanup_release",",\"sent\":"+std::to_string(released));
        }
        if(identity()){DWORD_PTR ignored{};SendMessageTimeoutW(owned,WM_CANCELMODE,0,0,SMTO_ABORTIFHUNG,1000,&ignored);}
    }
    if(!PostMessageW(owned,finish_message,0,0)){
        driver_pass=false;record("blocked",",\"reason\":\"finish_message_failed\"");
        DWORD_PTR ignored{};SendMessageTimeoutW(owned,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,1000,&ignored);
    }
    SetEvent(driver_finished);
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
    record("startup",",\"evidence_kind\":\"automated_owned_modal\",\"human_input\":false,\"real_explorer\":false,\"sendinput_in_probe\":true,\"foreground_contract\":\"verified_activation_v1\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"ui_tid\":"+std::to_string(ui_thread)+",\"qpc_frequency\":"+std::to_string(frequency.QuadPart));
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
        entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);exited=CreateEventW(nullptr,TRUE,FALSE,nullptr);stepped=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);pointer_arrived=CreateEventW(nullptr,TRUE,FALSE,nullptr);nonclient_down=CreateEventW(nullptr,TRUE,FALSE,nullptr);correction_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);driver_finished=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        activation_down_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);activation_up_received=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        require(entered&&exited&&stepped&&activation_event&&pointer_arrived&&nonclient_down&&correction_finished&&driver_finished&&activation_down_received&&activation_up_received,"event_creation_failed");
        STARTUPINFOW startup{sizeof(startup)};GetStartupInfoW(&startup);
        ShowWindow(owned,SW_SHOW);
        record("show_window",",\"startup_flags\":"+std::to_string(startup.dwFlags)+",\"startup_show\":"+std::to_string(startup.wShowWindow)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        // The first ShowWindow may honor the shell parent's hidden STARTUPINFO
        // instead of SW_SHOW. The explicitly requested test UI must be visible.
        if(!IsWindowVisible(owned))ShowWindow(owned,SW_SHOWNORMAL);
        bootstrap.attempted=true;const BOOL activated=SetForegroundWindow(owned);set_foreground_success=activated!=FALSE;UpdateWindow(owned);
        record("foreground_attempt",",\"target\":"+std::to_string(number(owned))+",\"foreground\":"+std::to_string(number(GetForegroundWindow()))+",\"set_foreground_success\":"+flag(activated!=FALSE)+",\"visible\":"+flag(IsWindowVisible(owned)!=FALSE));
        record("owned",",\"hwnd\":"+std::to_string(number(owned))+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(ui_thread)+",\"saved_cursor\":"+point(saved_cursor)+",\"work_area\":"+rect(work_area)+",\"dpi\":"+std::to_string(GetDpiForWindow(owned))+geometry(capture()));
        std::thread driver{drive};MSG message{};bool owner_ok=true;
        bool quit=false;while(!quit){
            const DWORD wait=MsgWaitForMultipleObjects(1,&driver_finished,FALSE,15000,QS_ALLINPUT);
            if(wait==WAIT_TIMEOUT||wait==WAIT_FAILED){owner_ok=false;stop_requested=true;record("blocked",",\"reason\":\"owner_message_wait_failed\"");if(identity())DestroyWindow(owned);break;}
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){quit=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}
            if(wait==WAIT_OBJECT_0){if(identity())DestroyWindow(owned);quit=true;}
        }
        driver.join();result=owner_ok&&driver_pass&&log_ok?0:2;
    }catch(const std::exception& e){failure=e.what();emit_bootstrap(false,e.what());record("blocked",",\"reason\":\""+failure+"\"");if(identity())DestroyWindow(owned);}
    record("shutdown",",\"result\":\""+std::string(result==0?"CAPTURED_NOT_ACCEPTED":"BLOCKED")+"\",\"cursor_restored\":"+flag(restored)+",\"owned_window_destroyed\":"+flag(!IsWindow(owned))+",\"external_windows_touched\":false");
    for(HANDLE handle:{entered,exited,stepped,timer,activation_event,pointer_arrived,nonclient_down,correction_finished,driver_finished,activation_down_received,activation_up_received})if(handle)CloseHandle(handle);
    const bool evidence_ok=log_ok;CloseHandle(log_file);return evidence_ok?result:2;
}
