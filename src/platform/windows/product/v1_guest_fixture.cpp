// Disposable-Sandbox driver, not product input. No core solver supplies any
// expected geometry. All native windows and child processes are owned here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>
#include <wincrypt.h>
#include <objbase.h>
#include <tlhelp32.h>
#include "platform/windows/product/window_catalog.h"
#include "platform/windows/product/guest_path_scope.h"
#include "platform/windows/product/v1_guest_idle_measurement.h"
#include "platform/windows/explorer/explorer_shell_inventory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif
namespace {
namespace fs=std::filesystem;
namespace product=panebind::platform::windows::product;
namespace shell=panebind::platform::windows::explorer;
struct Rect {
    LONG l{},t{},r{},b{};
    bool operator==(const Rect&)const=default;
    LONG width()const{return r-l;}LONG height()const{return b-t;}
};
Rect translated(Rect r,LONG x,LONG y){return {r.l+x,r.t+y,r.r+x,r.b+y};}
std::string rect(Rect r){return "["+std::to_string(r.l)+","+std::to_string(r.t)+","+std::to_string(r.r)+","+std::to_string(r.b)+"]";}
std::string quote(std::string_view text){std::string out="\"";for(unsigned char c:text){if(c=='\\'||c=='"')out+='\\';out+=c<32?' ':static_cast<char>(c);}return out+'"';}
std::string utf8(const std::wstring& text){
    if(text.empty())return {};const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(size<=0)throw std::runtime_error("invalid_wide_text");std::string out(static_cast<std::size_t>(size),' ');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),out.data(),size,nullptr,nullptr);return out;
}
bool equal(const std::wstring& a,const std::wstring& b){return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;}
struct Handle{
    HANDLE value{};explicit Handle(HANDLE h=nullptr):value(h){}
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
bool desktop(){
    const HDESK h=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!h)return false;
    std::array<wchar_t,128> a{},b{};DWORD bytes{};BOOL receiving_input{};
    const bool ok=GetUserObjectInformationW(h,UOI_NAME,a.data(),static_cast<DWORD>(sizeof(a)),&bytes)&&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),UOI_NAME,b.data(),static_cast<DWORD>(sizeof(b)),&bytes)&&
        GetUserObjectInformationW(h,UOI_IO,&receiving_input,sizeof(receiving_input),&bytes)&&receiving_input!=FALSE;
    const bool closed=CloseDesktop(h)!=FALSE;return ok&&closed&&equal(a.data(),L"Default")&&equal(a.data(),b.data());
}
bool medium(HANDLE process,int* integrity=nullptr,bool* elevated=nullptr,DWORD* error=nullptr){
    if(error)*error=0;HANDLE raw{};if(!OpenProcessToken(process,TOKEN_QUERY,&raw)){if(error)*error=GetLastError();return false;}Handle token(raw);
    alignas(TOKEN_MANDATORY_LABEL) std::array<unsigned char,512> bytes{};DWORD size{};TOKEN_ELEVATION elevation{};
    if(!GetTokenInformation(token.value,TokenIntegrityLevel,bytes.data(),static_cast<DWORD>(bytes.size()),&size)){if(error)*error=GetLastError();return false;}
    if(!GetTokenInformation(token.value,TokenElevation,&elevation,sizeof(elevation),&size)){if(error)*error=GetLastError();return false;}
    const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(bytes.data())->Label.Sid;
    if(!IsValidSid(sid)||!*GetSidSubAuthorityCount(sid))return false;
    const auto rid=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1U);
    if(integrity)*integrity=static_cast<int>(rid);if(elevated)*elevated=elevation.TokenIsElevated!=0;
    return rid==SECURITY_MANDATORY_MEDIUM_RID&&!elevation.TokenIsElevated;
}
std::string read(const fs::path& path,std::size_t cap=2*1024*1024){
    std::error_code error;if(!fs::is_regular_file(path,error)||fs::file_size(path,error)>cap)return {};
    std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};
}
bool marker(const fs::path& path,const std::wstring& run){
    auto value=read(path,128);if(value.ends_with("\r\n"))value.resize(value.size()-2);else if(value.ends_with('\n'))value.pop_back();return value==utf8(run);
}
bool plain_directory(const fs::path& path){
    const auto a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT);
}
std::string hash(const fs::path& path){
    HCRYPTPROV provider{};HCRYPTHASH h{};
    if(!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT|CRYPT_SILENT))return {};
    if(!CryptCreateHash(provider,CALG_SHA_256,0,0,&h)){CryptReleaseContext(provider,0);return {};}
    std::ifstream file(path,std::ios::binary);std::array<char,65536> block{};bool ok=static_cast<bool>(file);
    while(ok&&file){file.read(block.data(),block.size());if(file.gcount()>0)ok=CryptHashData(h,reinterpret_cast<const BYTE*>(block.data()),static_cast<DWORD>(file.gcount()),0)!=FALSE;}
    std::array<BYTE,32> digest{};DWORD length=static_cast<DWORD>(digest.size());
    ok=ok&&file.eof()&&CryptGetHashParam(h,HP_HASHVAL,digest.data(),&length,0)&&length==digest.size();
    CryptDestroyHash(h);CryptReleaseContext(provider,0);if(!ok)return {};
    constexpr char digits[]="0123456789ABCDEF";std::string out;for(auto b:digest){out+=digits[b>>4];out+=digits[b&15];}return out;
}
bool wait(const std::function<bool()>& predicate,DWORD duration=3500){
    const auto end=GetTickCount64()+duration;do{
        // This driver STA must also receive real Shell COM callbacks. The
        // empty fixture windows themselves belong to the independent UI STA.
        MSG message{};unsigned budget=512;while(budget--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            if(message.message==WM_QUIT)throw std::runtime_error("test_driver_quit");
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(predicate())return true;Sleep(20);
    }while(GetTickCount64()<end);return predicate();
} // Bounded TEST observation, never a product event source.
std::optional<Rect> visible(HWND h){
    RECT r{};if(!IsWindow(h)||FAILED(DwmGetWindowAttribute(h,DWMWA_EXTENDED_FRAME_BOUNDS,&r,sizeof(r)))||r.left>=r.right||r.top>=r.bottom)return {};
    return Rect{r.left,r.top,r.right,r.bottom};
}
bool fixture_rect(HWND h,Rect target){
    RECT p{};const auto v=visible(h);if(!v||!GetWindowRect(h,&p))return false;
    const LONG l=target.l-(v->l-p.left),t=target.t-(v->t-p.top),r=target.r+(p.right-v->r),b=target.b+(p.bottom-v->b);
    return SetWindowPos(h,nullptr,l,t,r-l,b-t,SWP_NOACTIVATE|SWP_NOZORDER)&&wait([&]{return visible(h)==target;},1000);
}
struct Log{
    fs::path path;std::uint64_t sequence{};bool failed{};
    void emit(std::string_view type,std::string fields){
        std::ofstream out(path,std::ios::app|std::ios::binary);
        out<<"{\"schema\":\"panebind-v1-owned-driver/1\",\"source_sha\":\"" PANEBIND_BUILD_SHA "\",\"sequence\":"<<++sequence
            <<",\"tick\":"<<GetTickCount64()<<",\"automated_guest_driver\":true,\"type\":"<<quote(type)<<','<<fields<<"}\n";
        out.flush();if(!out)throw std::runtime_error("driver_evidence_write_failed");
    }
    void check(std::string name,bool passed,std::string fields={}){
        emit("case","\"case\":"+quote(name)+",\"result\":"+quote(passed?"PASS":"FAIL")+(fields.empty()?"":","+fields));
        if(!passed){failed=true;throw std::runtime_error(name);}
    }
};
struct WriterStall {
    std::atomic<bool> armed{},entered{},finished{};std::atomic<SHORT> async_left_state{};
    std::atomic<std::uint64_t> before_qpc{},after_qpc{},before_tick{};
    std::atomic<UINT> flags{};std::atomic<int> x{},y{},width{},height{};
};
struct NativeBatchLossMember {
    std::atomic<unsigned> order{};std::atomic<UINT> flags{};std::atomic<int> x{},y{},width{},height{};
};
struct NativeBatchLoss {
    std::array<HWND,3> exact{};std::array<LONG_PTR,3> userdata{};DWORD owner{};
    std::array<NativeBatchLossMember,3> member;std::atomic<unsigned> arrivals{};
    std::atomic<bool> destroy_called{},destroy_return{},destroy_gone{},completed{};std::atomic<SHORT> async_left_state{};
    std::atomic<DWORD> destroy_error{};std::atomic<std::uint64_t> destroy_qpc{};
};
struct Frame{HWND hwnd{};std::atomic<unsigned> down{},up{},starts{},ends{};LONG min_width{120},min_height{100},grid{1},max_width{},max_height{};WriterStall stall;
    std::atomic<std::shared_ptr<NativeBatchLoss>> native_batch_loss;};
struct ExplorerTarget {
    fs::path directory;shell::ShellLocationFact location;std::unique_ptr<shell::ExplorerProvisioningLease> lease;
    std::unique_ptr<shell::ExplorerConsentTargetObservation> observation;
    std::optional<product::Snapshot> identity;HWND hwnd{};DWORD pid{},tid{};std::uint64_t born{};
    std::vector<HWND> baseline;bool separate{},navigated{};
};
struct ApplicationProcess {
    DWORD pid{},parent{};std::uint64_t born{};std::unique_ptr<Handle> handle;
};
struct ApplicationTarget {
    std::string kind;fs::path executable,root_executable,directory;std::wstring command;
    std::vector<HWND> baseline;std::vector<ApplicationProcess> processes;
    std::optional<product::Snapshot> identity;bool accepted{},cleanup_confirmed{},matrix_passed{};
};
struct State{
    HINSTANCE instance{};std::wstring run,class_name;fs::path output,input_product,product,product_log;
    std::string configuration;std::array<Frame,3> frames;Log log;std::thread driver;std::atomic<int> result{1};
    HANDLE process{};DWORD product_pid{},owner{};
    // Driver send ledger only: successful insertion is not native delivery or
    // physical button state. A separate product hook receipt proves receipt.
    bool left{},control{};std::uint64_t left_down_send_qpc{},left_up_send_qpc{};
    std::array<std::unique_ptr<ExplorerTarget>,3> explorer;
    std::vector<std::unique_ptr<ExplorerTarget>> retired_explorers;
    std::unique_ptr<product::Catalog> target_catalog;
    std::array<std::shared_ptr<ApplicationTarget>,3> applications;
    std::vector<std::shared_ptr<ApplicationTarget>> created_applications;
};
HWND source_window(State& s,std::size_t source){return s.applications.at(source)&&s.applications[source]->identity?s.applications[source]->identity->hwnd:
    (s.explorer.at(source)?s.explorer[source]->hwnd:s.frames.at(source).hwnd);}
DWORD source_thread(State& s,std::size_t source){return s.applications.at(source)&&s.applications[source]->identity?s.applications[source]->identity->tid:
    (s.explorer.at(source)?s.explorer[source]->tid:s.owner);}
std::wstring test_root_property(const State& s){return L"PaneBind.V1.ExactGuestRoot."+s.run;}
bool test_root_stamp(State& s,const product::Snapshot& identity,bool install=false){
    if(!identity.hwnd||!identity.key)return false;const auto name=test_root_property(s);const auto value=reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(identity.key));
    const auto existing=GetPropW(identity.hwnd,name.c_str());
    if(install&&existing&&existing!=value)return false;
    if(install&&(!SetPropW(identity.hwnd,name.c_str(),value)))return false;
    return GetPropW(identity.hwnd,name.c_str())==value;
}
bool layout_ui_message(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM wp,LPARAM lp){
    if(layout_ui_message(h,message,wp,lp))return 0;
    auto* frame=reinterpret_cast<Frame*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE){frame=static_cast<Frame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(frame));}
    if(frame){
        if(message==WM_GETMINMAXINFO){auto* mm=reinterpret_cast<MINMAXINFO*>(lp);mm->ptMinTrackSize={frame->min_width,frame->min_height};
            const auto dpi=GetDpiForWindow(h);
            mm->ptMaxTrackSize={frame->max_width>0?frame->max_width:GetSystemMetricsForDpi(SM_CXMAXTRACK,dpi),
                frame->max_height>0?frame->max_height:GetSystemMetricsForDpi(SM_CYMAXTRACK,dpi)};return 0;}
        if(message==WM_WINDOWPOSCHANGING){
            const auto loss=frame->native_batch_loss.load();const auto* position=reinterpret_cast<const WINDOWPOS*>(lp);
            if(loss&&position&&!(position->flags&SWP_NOSIZE)&&GetCurrentThreadId()==loss->owner){
                for(std::size_t n=0;n<loss->exact.size();++n)if(loss->exact[n]==h&&loss->userdata[n]==reinterpret_cast<LONG_PTR>(frame)&&
                    GetWindowLongPtrW(h,GWLP_USERDATA)==loss->userdata[n]&&GetAncestor(h,GA_ROOT)==h){
                    // Guest TEST application behavior, never a product fault
                    // hook. Only an actually received sizing call counts.
                    frame->native_batch_loss.store({});auto& observed=loss->member[n];
                    observed.flags=position->flags;observed.x=position->x;observed.y=position->y;observed.width=position->cx;observed.height=position->cy;
                    const auto order=loss->arrivals.fetch_add(1)+1;observed.order=order;
                    if(order==2){LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);loss->destroy_qpc=static_cast<std::uint64_t>(qpc.QuadPart);
                        loss->async_left_state=GetAsyncKeyState(VK_LBUTTON);loss->destroy_called=true;SetLastError(0);
                        const bool result=DestroyWindow(h)!=FALSE;loss->destroy_error=result?0:GetLastError();loss->destroy_return=result;
                        loss->destroy_gone=!IsWindow(h);loss->completed.store(true,std::memory_order_release);
                        // The native outer SetWindowPos result is deliberately
                        // NOT inferred from this DestroyWindow result.
                        if(!IsWindow(h))return 0;
                    }
                    break;
                }
            }
        }
        if(message==WM_WINDOWPOSCHANGING&&frame->stall.armed.load()&&!(reinterpret_cast<const WINDOWPOS*>(lp)->flags&SWP_NOSIZE)&&frame->stall.armed.exchange(false)){
            // TEST ONLY: this exact empty fixture owns this window/thread.
            // No source parameters are modified; the product's already-issued
            // SetWindowPos may finish after UP. Never suspend another process.
            const auto* position=reinterpret_cast<const WINDOWPOS*>(lp);LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);
            frame->stall.flags=position->flags;frame->stall.x=position->x;frame->stall.y=position->y;
            frame->stall.width=position->cx;frame->stall.height=position->cy;frame->stall.before_qpc=static_cast<std::uint64_t>(qpc.QuadPart);
            frame->stall.before_tick=GetTickCount64();frame->stall.async_left_state=GetAsyncKeyState(VK_LBUTTON);frame->stall.entered=true;
            Sleep(1000);QueryPerformanceCounter(&qpc);frame->stall.after_qpc=static_cast<std::uint64_t>(qpc.QuadPart);frame->stall.finished=true;
        }
        if(message==WM_WINDOWPOSCHANGING&&frame->grid>1){auto* p=reinterpret_cast<WINDOWPOS*>(lp);if(!(p->flags&SWP_NOSIZE)){
            p->cx=std::max(frame->min_width,(p->cx/frame->grid)*frame->grid);p->cy=std::max(frame->min_height,(p->cy/frame->grid)*frame->grid);}}
        if(message==WM_LBUTTONDOWN)++frame->down;if(message==WM_LBUTTONUP)++frame->up;
        if(message==WM_ENTERSIZEMOVE)++frame->starts;if(message==WM_EXITSIZEMOVE)++frame->ends;
    }
    if(message==WM_PAINT){PAINTSTRUCT p{};HDC dc=BeginPaint(h,&p);RECT r{};GetClientRect(h,&r);FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW));
        DrawTextW(dc,L"PaneBind V1 — 自建空白测试窗口",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);EndPaint(h,&p);return 0;}
    return DefWindowProcW(h,message,wp,lp);
}
void inject(INPUT packet,UINT* delivered=nullptr,DWORD* failure_error=nullptr){
    if(!desktop())throw std::runtime_error("guest_input_desktop_became_inactive");packet.mi.dwExtraInfo=0x50425631;
    const UINT count=SendInput(1,&packet,sizeof(packet));const DWORD error=count==1?0:GetLastError();
    if(delivered)*delivered=count;if(failure_error)*failure_error=error;
    if(count!=1)throw std::runtime_error("test_SendInput_failed");
}
void cursor(State& s,POINT point){
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(w<2||h<2||point.x<x||point.y<y||point.x>=x+w||point.y>=y+h)throw std::runtime_error("test_cursor_outside_virtual_screen");
    POINT before{},actual{};const bool before_read=GetCursorPos(&before)!=FALSE;const DWORD before_error=before_read?0:GetLastError();
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx=static_cast<LONG>(((static_cast<long long>(point.x-x)*2+1)*65536)/(static_cast<long long>(w)*2));
    input.mi.dy=static_cast<LONG>(((static_cast<long long>(point.y-y)*2+1)*65536)/(static_cast<long long>(h)*2));
    UINT delivered=UINT_MAX;DWORD injection_error{},position_error{};bool position_read=false;unsigned observations{};
    const auto evidence=[&](bool exact){
        std::string failure_context;
        if(!exact){
            // One failure-time native observation, not a retry/permission or
            // an attempt to reclaim focus/capture. Packet coordinates alone
            // did not prove the actual system cursor in the failing run.
            const HWND foreground_window=GetForegroundWindow();DWORD foreground_pid{};
            const DWORD foreground_tid=GetWindowThreadProcessId(foreground_window,&foreground_pid);
            std::array<wchar_t,128> foreground_class{};
            const bool class_read=foreground_window&&GetClassNameW(foreground_window,foreground_class.data(),static_cast<int>(foreground_class.size()))>0;
            GUITHREADINFO gui{sizeof(gui)};const bool gui_read=foreground_tid&&GetGUIThreadInfo(foreground_tid,&gui)!=FALSE;
            RECT clipping{};const bool clip_read=GetClipCursor(&clipping)!=FALSE;
            failure_context=",\"failure_actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_window))+
                ",\"failure_foreground_pid\":"+std::to_string(foreground_pid)+",\"failure_foreground_tid\":"+std::to_string(foreground_tid)+
                ",\"failure_foreground_class\":"+(class_read?quote(utf8(foreground_class.data())):"null")+
                ",\"failure_GUI_read\":"+(gui_read?"true":"false")+
                ",\"failure_actual_capture\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
                ",\"failure_actual_move_size\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
                ",\"failure_clip_rect\":"+(clip_read?rect({clipping.left,clipping.top,clipping.right,clipping.bottom}):"null");
        }
        s.log.emit("test_cursor_observation","\"requested\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"before\":"+
            (before_read?"["+std::to_string(before.x)+","+std::to_string(before.y)+"]":"null")+",\"before_GetCursorPos_error\":"+std::to_string(before_error)+
            ",\"actual\":"+(position_read?"["+std::to_string(actual.x)+","+std::to_string(actual.y)+"]":"null")+
            ",\"GetCursorPos_succeeded\":"+(position_read?"true":"false")+",\"GetCursorPos_error\":"+std::to_string(position_error)+
            ",\"virtual_screen\":["+std::to_string(x)+","+std::to_string(y)+","+std::to_string(w)+","+std::to_string(h)+
            "],\"normalized_absolute\":["+std::to_string(input.mi.dx)+","+std::to_string(input.mi.dy)+"],\"mouse_flags\":"+std::to_string(input.mi.dwFlags)+
            ",\"SendInput_called\":"+(delivered==UINT_MAX?"false":"true")+",\"SendInput_return\":"+(delivered==UINT_MAX?"null":std::to_string(delivered))+
            ",\"SendInput_error_on_failure\":"+std::to_string(injection_error)+",\"bounded_observations\":"+std::to_string(observations)+
            ",\"exact_system_cursor\":"+(exact?"true":"false")+",\"product_packet_not_used_as_cursor_proof\":true"+failure_context);
    };
    try{inject(input,&delivered,&injection_error);}catch(...){
        position_read=GetCursorPos(&actual)!=FALSE;position_error=position_read?0:GetLastError();evidence(false);throw;
    }
    const bool exact=wait([&]{++observations;position_read=GetCursorPos(&actual)!=FALSE;position_error=position_read?0:GetLastError();
        return position_read&&actual.x==point.x&&actual.y==point.y;},500);
    evidence(exact);if(!exact)throw std::runtime_error("test_cursor_not_exact");
}
void key(State& s,WORD code,bool pressed){
    INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wVk=code;packet.ki.dwFlags=pressed?0:KEYEVENTF_KEYUP;packet.ki.dwExtraInfo=0x50425631;
    if(!desktop()||SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("test_key_input_failed");
    if(code==VK_CONTROL)s.control=pressed;
    if(!wait([&]{return ((GetAsyncKeyState(code)&0x8000)!=0)==pressed;},500))throw std::runtime_error("test_key_state_not_observed");
}
void button(State& s,bool pressed){
    INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;
    const bool outstanding_before=s.left;const SHORT async_before=GetAsyncKeyState(VK_LBUTTON);
    LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);UINT delivered=UINT_MAX;DWORD error{};
    const auto receipt=[&]{
        QueryPerformanceCounter(&after);
        s.log.emit("test_button_send","\"requested\":"+quote(pressed?"DOWN":"UP")+
            ",\"send_before_qpc\":"+std::to_string(before.QuadPart>0?before.QuadPart:0)+
            ",\"send_after_qpc\":"+std::to_string(after.QuadPart>0?after.QuadPart:0)+
            ",\"mouse_flags\":"+std::to_string(packet.mi.dwFlags)+
            ",\"SendInput_called\":"+(delivered==UINT_MAX?"false":"true")+
            ",\"SendInput_return\":"+(delivered==UINT_MAX?"null":std::to_string(delivered))+
            ",\"SendInput_error_on_failure\":"+std::to_string(error)+
            ",\"driver_left_outstanding_before\":"+(outstanding_before?"true":"false")+
            ",\"driver_left_outstanding_after\":"+(s.left?"true":"false")+
            ",\"async_left_before\":"+std::to_string(async_before)+
            ",\"async_left_after\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+
            ",\"delivery_not_inferred\":true,\"physical_button_held_not_inferred\":true");
    };
    try{inject(packet,&delivered,&error);}catch(...){receipt();throw;}
    s.left=pressed;
    const auto qpc=before.QuadPart>0?static_cast<std::uint64_t>(before.QuadPart):0;
    if(pressed)s.left_down_send_qpc=qpc;else s.left_up_send_qpc=qpc;
    receipt();
}
std::optional<POINT> hit_point(HWND h,int required){
    RECT p{};DWORD root_pid{};if(!GetWindowRect(h,&p)||!GetWindowThreadProcessId(h,&root_pid))return {};
    const LONG begin=required==HTBOTTOM?p.bottom-8:p.top+3,end=required==HTBOTTOM?p.bottom:p.top+55;
    const LONG left=required==HTBOTTOM?(p.left+p.right)/2:p.left+40,right=required==HTBOTTOM?left+1:p.right-40;
    for(LONG y=begin;y<end;++y)for(LONG x=left;x<right;x+=8){DWORD_PTR hit{};const POINT point{x,y};const HWND actual=WindowFromPoint(point);DWORD actual_pid{};
        if(!actual||GetAncestor(actual,GA_ROOT)!=h||!GetWindowThreadProcessId(actual,&actual_pid)||actual_pid!=root_pid)continue;
        // The production adapter queries the actual hit child, not an idealized
        // root. Chromium/custom title bars can legitimately return HTCAPTION.
        if(SendMessageTimeoutW(actual,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)&&static_cast<int>(hit)==required)return point;}
    return {};
}
bool own(State&,HWND);
bool authorized_source(State&,HWND);
void foreground(State& s,HWND h){
    if(!authorized_source(s,h))throw std::runtime_error("not_exact_test_created_source");
    const bool result=SetForegroundWindow(h)!=FALSE;
    s.log.check("foreground_exact_owned_source",result&&wait([&]{return GetForegroundWindow()==h;},1000),"\"api_return\":"+std::string(result?"true":"false"));
}
bool launch(State& s,std::wstring args,bool command,DWORD* exit=nullptr){
    std::wstring line=L"\""+s.product.wstring()+L"\""+(args.empty()?L"":L" "+args);STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(s.product.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,s.product.parent_path().c_str(),&si,&pi))return false;
    CloseHandle(pi.hThread);if(!command){s.process=pi.hProcess;s.product_pid=pi.dwProcessId;return medium(pi.hProcess);}
    const DWORD waited=WaitForSingleObject(pi.hProcess,5000);DWORD code=STILL_ACTIVE;if(waited==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hProcess);if(exit)*exit=code;return waited==WAIT_OBJECT_0&&code==0;
}
std::uint64_t number(const std::string& line,std::string_view field){
    auto at=line.find(field);if(at==std::string::npos)return 0;at+=field.size();std::uint64_t value{};
    for(;at<line.size()&&line[at]>='0'&&line[at]<='9';++at)value=value*10+static_cast<unsigned>(line[at]-'0');return value;
}
std::string record(State& s,std::size_t offset,std::string_view type,std::uint64_t generation=0){
    const auto text=read(s.product_log);if(offset>=text.size())return {};std::istringstream stream(text.substr(offset));std::string line,out;
    while(std::getline(stream,line))if(line.ends_with('}')&&line.find("\"type\":"+quote(type))!=std::string::npos&&(!generation||number(line,"\"generation\":")==generation))out=line;return out;
}
std::string actual_up(State& s,std::size_t offset,std::uint64_t generation,int pressed_button=1){
    if(!generation||pressed_button<1||pressed_button>5)return {};
    const auto text=read(s.product_log);if(offset>=text.size())return {};
    std::istringstream rows(text.substr(offset));std::string line,observed;
    while(std::getline(rows,line))if(line.ends_with('}')&&line.find("\"type\":\"input_button_observed\"")!=line.npos&&
        line.find("\"source\":\"WH_MOUSE_LL\"")!=line.npos&&line.find("\"kind\":\"UP\"")!=line.npos&&
        number(line,"\"generation\":")==generation&&number(line,"\"button\":")==static_cast<unsigned>(pressed_button)&&
        number(line,"\"hook_observed_qpc\":"))observed=line;
    return observed;
}
std::string placement(State& s,std::size_t offset,std::uint64_t generation,Rect expected,bool native){
    const auto text=read(s.product_log);if(offset>=text.size())return {};std::istringstream stream(text.substr(offset));std::string line,out;
    while(std::getline(stream,line))if(line.ends_with('}')&&line.find("\"type\":\"placement\"")!=std::string::npos&&
        number(line,"\"generation\":")==generation&&line.find("\"exact\":true")!=std::string::npos&&
        line.find("\"actual\":"+rect(expected))!=std::string::npos&&(!native||line.find("\"native_attempted\":true")!=std::string::npos))out=line;
    return out;
}
struct Gesture{POINT anchor{};std::uint64_t generation{};std::size_t offset{};};
bool application_down_context(State&,HWND,POINT,int,std::string_view);
void application_prepare_barrier(State&,HWND,POINT,int);
Gesture down(State& s,std::size_t source,bool ctrl=false,int region=HTCAPTION){
    const auto h=source_window(s,source);foreground(s,h);const auto point=hit_point(h,region);
    s.log.check("actual_nonclient_hit",point.has_value(),"\"source\":"+std::to_string(source)+",\"hit\":"+std::to_string(region));
    cursor(s,*point);
    if(s.applications[source])application_prepare_barrier(s,h,*point,region);else Sleep(100);
    if(ctrl)key(s,VK_CONTROL,true);Gesture g{*point,0,read(s.product_log).size()};button(s,true);
    const bool observed=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;});
    if(!observed&&s.applications[source])application_down_context(s,h,*point,region,"after_unobserved_DOWN");
    s.log.check("production_real_down_start",observed,
        "\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation));return g;
}
void phase(State& s,std::size_t source,const Gesture& g,LONG dx,LONG dy,Rect expected,std::string name,bool native){
    // Deliberately paced input trajectory below the configured speed cutoff;
    // this is not a product retry, cursor sampler or authority workaround.
    if(!authorized_source(s,source_window(s,source)))throw std::runtime_error("test_source_identity_revoked");
    Sleep(60);const auto mark=read(s.product_log).size();cursor(s,{g.anchor.x+dx,g.anchor.y+dy});std::string receipt;
    const bool ok=wait([&]{receipt=placement(s,mark,g.generation,expected,native);return visible(source_window(s,source))==expected&&!receipt.empty();});
    const auto actual=visible(source_window(s,source));
    s.log.check(std::move(name),ok,"\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation)+
        ",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+",\"trajectory_pause_ms\":60,\"production_receipt\":"+quote(receipt));
}
void up(State& s,std::size_t source,const Gesture& g,Rect expected){
    const auto mark=read(s.product_log).size();button(s,false);if(s.control)key(s,VK_CONTROL,false);std::string end;
    const bool observed=wait([&]{end=record(s,mark,"gesture_end",g.generation);return end.find("\"normal_up\":true")!=std::string::npos;});
    Sleep(150);GUITHREADINFO gui{sizeof(gui)};const bool gui_read=GetGUIThreadInfo(source_thread(s,source),&gui)!=FALSE;const auto actual=visible(source_window(s,source));
    s.log.check("normal_up_and_no_late_geometry",observed&&actual==expected&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&gui_read&&!gui.hwndCapture,
        "\"generation\":"+std::to_string(g.generation)+",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+
        ",\"source_thread_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture))+",\"production_end\":"+quote(end));
}
void five(State& s,std::size_t source,std::string label){
    const auto original=visible(source_window(s,source));if(!original)throw std::runtime_error("test_source_geometry_missing");
    const auto g=down(s,source);const LONG dx=source==0?-40:50,dy=source==1?-35:40;
    phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".free",true);
    phase(s,source,g,-6,3,*original,label+".xy_snap",true);phase(s,source,g,-8,5,*original,label+".hold",false);
    phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".detach",true);phase(s,source,g,-5,4,*original,label+".resnap",true);up(s,source,g,*original);
}
void ctrl_move(State& s,std::size_t source,LONG dx,LONG dy,std::string label){
    std::array<Rect,3> before{};for(std::size_t n=0;n<3;++n){const auto r=visible(source_window(s,n));if(!r)throw std::runtime_error("group_geometry_missing");before[n]=*r;}
    const auto g=down(s,source,true);phase(s,source,g,dx,dy,translated(before[source],dx,dy),label+".source",true);
    for(std::size_t n=0;n<3;++n){const auto target=translated(before[n],dx,dy);s.log.check(label+".member_"+std::to_string(n),
        wait([&]{return visible(source_window(s,n))==target;}),"\"expected\":"+rect(target));}
    up(s,source,g,translated(before[source],dx,dy));
}
bool own(State& s,HWND window){
    DWORD pid{};std::array<wchar_t,256> name{};
    if(!window||!IsWindow(window)||GetWindowThreadProcessId(window,&pid)!=s.owner||pid!=GetCurrentProcessId()||
        GetClassNameW(window,name.data(),static_cast<int>(name.size()))<=0||!equal(name.data(),s.class_name))return false;
    const auto frame=std::find_if(s.frames.begin(),s.frames.end(),[&](const auto& f){return f.hwnd==window;});
    return frame!=s.frames.end()&&GetWindowLongPtrW(window,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(&*frame);
}
bool native_explorer_identity(ExplorerTarget& target,HWND h,bool initialize=false){
    DWORD pid{};const DWORD tid=GetWindowThreadProcessId(h,&pid);std::array<wchar_t,128> name{};
    if(!h||!tid||!pid||!IsWindow(h)||GetAncestor(h,GA_ROOT)!=h||
        GetClassNameW(h,name.data(),static_cast<int>(name.size()))<=0||!equal(name.data(),L"CabinetWClass")||
        std::find(target.baseline.begin(),target.baseline.end(),h)!=target.baseline.end())return false;
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));FILETIME birth{},ended{},kernel{},user{};
    if(!process.value||WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT||!GetProcessTimes(process.value,&birth,&ended,&kernel,&user))return false;
    std::array<wchar_t,32768> image{},windows{};DWORD image_size=static_cast<DWORD>(image.size());
    if(!QueryFullProcessImageNameW(process.value,0,image.data(),&image_size)||!GetWindowsDirectoryW(windows.data(),static_cast<UINT>(windows.size()))||
        !equal(image.data(),(fs::path(windows.data())/L"explorer.exe").wstring()))return false;
    const std::uint64_t born=(static_cast<std::uint64_t>(birth.dwHighDateTime)<<32)|birth.dwLowDateTime;
    if(initialize){target.hwnd=h;target.pid=pid;target.tid=tid;target.born=born;return true;}
    return h==target.hwnd&&pid==target.pid&&tid==target.tid&&born==target.born;
}
bool exact_shell_location(ExplorerTarget& target){
    const auto directory=shell::filesystem_location_identity(target.directory);
    if(!shell::same_filesystem_location(directory,target.location))return false;
    const auto inventory=shell::capture_shell_window_inventory();const auto* entry=inventory.find(target.hwnd);
    if(!inventory.complete||!entry||entry->shell_entry_count!=1||entry->locations.size()!=1||
        !shell::same_filesystem_location(entry->locations.front(),target.location))return false;
    unsigned matches{};for(const auto& window:inventory.windows)for(const auto& location:window.locations)
        if(shell::same_filesystem_location(location,target.location))++matches;
    if(matches!=1)return false;
    if(target.lease){
        const auto h=target.lease->current_hwnd();if(!h.succeeded()||h.window!=target.hwnd||
            !shell::same_filesystem_location(target.lease->current_location(),target.location))return false;
    }
    if(target.observation){
        const auto h=target.observation->current_window_key();if(!h.succeeded()||h.window_key!=reinterpret_cast<std::uintptr_t>(target.hwnd)||
            !shell::same_filesystem_location(target.observation->current_location(),target.location))return false;
        const auto facts=target.observation->facts();if(!facts.canonical_identity_matches||facts.navigation_changed_since_binding())return false;
    }
    return true;
}
bool authorized_source(State& s,HWND h){
    if(own(s,h))return true;
    for(const auto& target:s.applications)if(target&&target->accepted&&target->identity&&target->identity->hwnd==h&&s.target_catalog){
        // Identity rechecks do not require a new cross-process constraint
        // message. Actual layout planning/writing separately queries limits.
        const auto fresh=s.target_catalog->get(h,{},nullptr,false,false);
        if(!fresh||!product::Catalog::same(*target->identity,*fresh)||
            std::find(target->baseline.begin(),target->baseline.end(),h)!=target->baseline.end())return false;
        const auto process=std::find_if(target->processes.begin(),target->processes.end(),[&](const auto& p){return p.pid==fresh->pid&&p.born==fresh->born;});
        return process!=target->processes.end()&&process->handle&&WaitForSingleObject(process->handle->value,0)==WAIT_TIMEOUT&&
            equal(fresh->exe,target->root_executable.wstring());
    }
    for(auto& target:s.explorer)if(target&&target->hwnd==h&&target->identity&&s.target_catalog){
        const auto fresh=s.target_catalog->get(h,{});
        return fresh&&product::Catalog::same(*target->identity,*fresh)&&native_explorer_identity(*target,h)&&exact_shell_location(*target);
    }
    return false;
}
void close_explorer_target(State& s,ExplorerTarget& target){
    if(target.observation){(void)target.observation->retire();target.observation.reset();}
    bool attempted=false;HRESULT result=E_ACCESSDENIED;
    // A failed eligibility check never permits geometry/input. Test cleanup
    // may close only the exact newly-created automation object/root, never a
    // pre-existing frame. Unknown or reused identities are simply preserved.
    if(native_explorer_identity(target,target.hwnd)&&(!target.navigated||exact_shell_location(target))){
        if(target.lease){const auto h=target.lease->current_hwnd();if(h.succeeded()&&h.window==target.hwnd){attempted=true;result=target.lease->quit().hresult;}}
        else if(target.identity&&s.target_catalog){
            const auto actual=s.target_catalog->get(target.hwnd,{});
            if(actual&&product::Catalog::same(*target.identity,*actual)){attempted=true;result=PostMessageW(target.hwnd,WM_CLOSE,0,0)?S_OK:HRESULT_FROM_WIN32(GetLastError());}
        }
    }
    if(target.lease){(void)target.lease->close_browser_events();target.lease.reset();}
    const bool gone=target.hwnd&&(!IsWindow(target.hwnd)||(attempted&&result==S_OK&&wait([&]{return !IsWindow(target.hwnd);},3000)));
    if(target.identity&&!gone)s.log.failed=true;
    s.log.emit("exact_explorer_cleanup","\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target.hwnd))+
        ",\"attempted\":"+(attempted?"true":"false")+",\"hresult\":"+std::to_string(result)+",\"actual_root_gone\":"+(gone?"true":"false")+
        ",\"directory_retained\":true,\"unknown_objects_touched\":false,\"product_cleanup_pass_inferred\":false");
}
fs::path new_guest_explorer_directory(State& s,std::size_t index,bool separate){
    // The disposable guest's local disk is deliberately separate from the
    // mapped evidence output. This is a new test route, not proof of which
    // identity API failed on the previous mapped-directory run.
    if(index>=3||(s.configuration!="Debug"&&s.configuration!="Release"))throw std::runtime_error("Explorer_directory_scope_invalid");
    const fs::path guest_root=L"C:\\PaneBindV1",base=guest_root/L"Explorer",run=base/s.run;
    if(!plain_directory(guest_root))throw std::runtime_error("Explorer_guest_local_root_not_plain");
    if(!fs::exists(base)){if(!fs::create_directory(base))throw std::runtime_error("Explorer_guest_local_base_creation_failed");}
    if(!plain_directory(base))throw std::runtime_error("Explorer_guest_local_base_not_plain");
    const auto ownership=run/L"panebind-v1-explorer-run.marker";
    const std::string expected="panebind-v1-explorer-run/1\nrun_id="+utf8(s.run)+"\nsource_sha=" PANEBIND_BUILD_SHA "\n";
    if(!fs::exists(run)){
        if(!fs::create_directory(run))throw std::runtime_error("Explorer_guest_local_run_creation_failed");
        Handle file(CreateFileW(ownership.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Explorer_guest_local_marker_creation_failed");
        DWORD written{};const bool saved=WriteFile(file.value,expected.data(),static_cast<DWORD>(expected.size()),&written,nullptr)&&
            written==expected.size()&&FlushFileBuffers(file.value);
        const bool closed=CloseHandle(file.value)!=FALSE;file.value=nullptr;
        if(!saved||!closed)throw std::runtime_error("Explorer_guest_local_marker_write_failed");
    }
    // Debug and Release in the same run may share this recognized parent,
    // but never an already-existing leaf or an unknown ownership marker.
    if(!plain_directory(run)||read(ownership,256)!=expected)throw std::runtime_error("Explorer_guest_local_run_ownership_unknown");
    const auto leaf=std::wstring(s.configuration.begin(),s.configuration.end())+L"-"+
        std::wstring(1,static_cast<wchar_t>(L'A'+index))+(separate?L"-separate":L"");
    const auto target=run/leaf;
    if(fs::exists(target)||!fs::create_directory(target)||!plain_directory(target))throw std::runtime_error("exact_new_explorer_directory_required");
    std::error_code error;const auto actual=fs::weakly_canonical(target,error);
    if(error||!product::guest::same_exact_directory(actual,target))throw std::runtime_error("Explorer_guest_local_canonical_scope_changed");
    s.log.emit("explorer_test_directory","\"source\":"+std::to_string(index)+",\"directory\":"+quote(utf8(target.wstring()))+
        ",\"route\":\"guest_local_C_not_mapped_output\",\"marker\":"+quote(utf8(ownership.wstring()))+
        ",\"marker_sha256\":"+quote(hash(ownership))+",\"leaf_new_and_empty\":"+(fs::is_empty(target)?"true":"false")+
        ",\"old_mapped_failure_root_cause_not_inferred\":true,\"unknown_content_overwritten\":false");
    if(!fs::is_empty(target))throw std::runtime_error("Explorer_guest_local_leaf_not_empty");return target;
}
bool provision_explorer(State& s,std::size_t index,bool separate){
    auto target=std::make_unique<ExplorerTarget>();target->separate=separate;
    target->directory=new_guest_explorer_directory(s,index,separate);
    target->location=shell::filesystem_location_identity(target->directory);
    std::string file_id;constexpr char hex[]="0123456789abcdef";
    if(target->location.identity)for(const auto value:target->location.identity->file_id){const auto byte=std::to_integer<unsigned>(value);file_id+=hex[byte>>4];file_id+=hex[byte&15];}
    s.log.emit("explorer_directory_identity","\"source\":"+std::to_string(index)+",\"directory\":"+quote(utf8(target->directory.wstring()))+
        ",\"filesystem\":"+(target->location.filesystem()?"true":"false")+",\"status\":"+std::to_string(static_cast<unsigned>(target->location.status))+
        ",\"hresult\":"+std::to_string(target->location.diagnostic)+",\"volume_serial\":"+
        (target->location.identity?std::to_string(target->location.identity->volume_serial_number):"null")+",\"file_id_128\":"+
        (file_id.empty()?"null":quote(file_id))+",\"identity_failure_does_not_authorize_Explorer\":true");
    if(!target->location.filesystem())throw std::runtime_error("new_directory_file_identity_unavailable");
    const auto baseline=shell::capture_shell_window_inventory();if(!baseline.complete)throw std::runtime_error("complete_shell_baseline_required");
    for(const auto& entry:baseline.windows)target->baseline.push_back(entry.window);
    std::string why;bool created=false;
    if(!separate){
        auto result=shell::create_explorer_provisioning_lease(GetTickCount64()+1,index+1,target->directory);
        s.log.emit("explorer_create","\"method\":\"single_owned_COM_lease\",\"source\":"+std::to_string(index)+
            ",\"succeeded\":"+(result.succeeded()?"true":"false")+",\"hresult\":"+std::to_string(result.diagnostic?result.diagnostic->hresult:S_OK));
        target->lease=std::move(result.lease);
        if(result.succeeded()||target->lease){
            // Never navigate an object whose root has not first been proven
            // new. A successful CoCreate call alone is not ownership.
            HWND candidate{};const bool fresh=wait([&]{const auto h=target->lease->current_hwnd();if(h.succeeded())candidate=h.window;return native_explorer_identity(*target,candidate,true);},5000);
            Handle process(fresh?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,target->pid):nullptr);int rid=-1;bool elevated=false;
            const bool normal=process.value&&medium(process.value,&rid,&elevated);
            s.log.emit("explorer_actual_process","\"source\":"+std::to_string(index)+",\"new_root_confirmed\":"+(fresh?"true":"false")+
                ",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(candidate))+",\"pid\":"+std::to_string(target->pid)+
                ",\"tid\":"+std::to_string(target->tid)+",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+
                ",\"elevated\":"+(rid<0?"null":(elevated?"true":"false"))+",\"eligible_medium\":"+(normal?"true":"false"));
            if(fresh&&normal){const auto navigation=target->lease->navigate_to_target_and_show();target->navigated=navigation.succeeded();
                created=target->navigated&&wait([&]{return exact_shell_location(*target)&&IsWindowVisible(target->hwnd);},5000);why=navigation.succeeded()?"shell_location_not_ready":"owned_navigation_failed";}
            else why=fresh?"actual_target_not_medium_non_elevated":"new_root_unproven_before_navigation";
        }else why="owned_COM_creation_failed";
    }else{
        std::array<wchar_t,MAX_PATH> windows{};if(!GetWindowsDirectoryW(windows.data(),static_cast<UINT>(windows.size())))throw std::runtime_error("guest_Windows_directory_unavailable");
        const fs::path executable=fs::path(windows.data())/L"explorer.exe";
        std::wstring line=L"\""+executable.wstring()+L"\" /separate /n,\""+target->directory.wstring()+L"\"";
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};
        const bool launched=CreateProcessW(executable.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,s.output.c_str(),&startup,&info)!=FALSE;
        const DWORD error=launched?0:GetLastError();const bool medium_launch=launched&&medium(info.hProcess);
        s.log.emit("explorer_create","\"method\":\"one_separate_process_contrast\",\"source\":"+std::to_string(index)+
            ",\"api_succeeded\":"+(launched?"true":"false")+",\"win32_error\":"+std::to_string(error)+",\"launch_process_actual_medium\":"+(medium_launch?"true":"false")+
            ",\"command\":"+quote(utf8(line))+",\"new_root_or_eligibility_inferred_from_launch\":false");
        if(launched){CloseHandle(info.hThread);CloseHandle(info.hProcess);}
        if(medium_launch)created=wait([&]{
            const auto inventory=shell::capture_shell_window_inventory();if(!inventory.complete)return false;
            HWND candidate{};unsigned matches{};for(const auto& entry:inventory.windows)for(const auto& location:entry.locations)
                if(shell::same_filesystem_location(location,target->location)){++matches;candidate=entry.window;}
            if(matches!=1||!native_explorer_identity(*target,candidate,true))return false;
            target->navigated=true;return exact_shell_location(*target)&&IsWindowVisible(candidate);
        },8000);
        why=launched?"no_unique_new_medium_root":"separate_process_creation_failed";
    }
    if(created){
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,target->pid));int rid=-1;bool elevated=false;
        const bool actual_medium=process.value&&medium(process.value,&rid,&elevated);
        s.log.emit("explorer_actual_root_token","\"source\":"+std::to_string(index)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target->hwnd))+
            ",\"pid\":"+std::to_string(target->pid)+",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+
            ",\"elevated\":"+(rid<0?"null":(elevated?"true":"false"))+",\"medium\":"+(actual_medium?"true":"false")+
            ",\"process_launch_was_not_binding_proof\":true");
        target->identity=s.target_catalog->get(target->hwnd,{},&why);
        if(target->identity){
            auto binding=shell::bind_explorer_consent_target(reinterpret_cast<std::uintptr_t>(target->hwnd),*target->location.identity);
            if(binding.succeeded())target->observation=std::move(binding.observation);else{target->identity.reset();why="canonical_shell_observation_binding_failed";}
        }
    }
    const bool accepted=target->identity.has_value()&&target->observation&&exact_shell_location(*target)&&test_root_stamp(s,*target->identity,true);
    s.log.emit("explorer_binding","\"source\":"+std::to_string(index)+",\"method\":"+quote(separate?"separate_process":"owned_COM_lease")+
        ",\"directory\":"+quote(utf8(target->directory.wstring()))+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(target->hwnd))+
        ",\"pid\":"+std::to_string(target->pid)+",\"tid\":"+std::to_string(target->tid)+",\"process_birth\":"+std::to_string(target->born)+
        ",\"accepted\":"+(accepted?"true":"false")+",\"reason\":"+quote(accepted?"actual_medium_same_user_session_new_root_exact_Shell_path":why)+
        ",\"product_permission_override\":false,\"input_sent\":false");
    if(accepted)s.explorer[index]=std::move(target);else s.retired_explorers.push_back(std::move(target));
    return accepted;
}
void explorer_flow(State& s){
    s.target_catalog=std::make_unique<product::Catalog>();
    // Hide only this fixture's exact windows after preserving the full owned
    // historical sequence; do not rearrange the old state or any other app.
    for(auto& frame:s.frames)if(own(s,frame.hwnd))ShowWindow(frame.hwnd,SW_HIDE);
    for(std::size_t n=0;n<3;++n){
        const bool com=provision_explorer(s,n,false);
        s.log.check("exact_Medium_Explorer_created",com||provision_explorer(s,n,true),"\"source\":"+std::to_string(n));
    }
    MONITORINFO monitor{sizeof(monitor)};if(!GetMonitorInfoW(MonitorFromWindow(source_window(s,0),MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("Explorer_monitor_unavailable");
    LONG width=380,height=240;for(const auto& target:s.explorer){
        width=std::max(width,static_cast<LONG>(target->identity->limits.min_width));
        height=std::max(height,static_cast<LONG>(target->identity->limits.min_height));
    }
    const LONG x=monitor.rcWork.left+60,y=monitor.rcWork.top+55;
    s.log.check("Explorer_three_window_space_available",2*width+120<=monitor.rcWork.right-monitor.rcWork.left&&2*height+110<=monitor.rcWork.bottom-monitor.rcWork.top,
        "\"chosen_visible_width\":"+std::to_string(width)+",\"chosen_half_height\":"+std::to_string(height));
    const std::array<Rect,3> layout{{{x,y,x+width,y+2*height},{x+width,y,x+2*width,y+height},{x+width,y+height,x+2*width,y+2*height}}};
    for(std::size_t n=0;n<3;++n){
        if(!authorized_source(s,source_window(s,n)))throw std::runtime_error("Explorer_identity_lost_before_fixture_layout");
        s.log.check("exact_Explorer_fixture_layout",fixture_rect(source_window(s,n),layout[n]),"\"source\":"+std::to_string(n)+",\"actual\":"+rect(layout[n]));
    }
    five(s,0,"Explorer_A_initial");
    s.log.emit("Explorer_first_real_Move","\"result\":\"PASS\",\"normal_up_checked\":true,\"High_exception_used\":false");
    ctrl_move(s,0,34,0,"Explorer_Ctrl_A");five(s,1,"Explorer_B");five(s,2,"Explorer_C");
    const auto before=visible(source_window(s,1));if(!before)throw std::runtime_error("Explorer_B_resize_missing");
    const auto g=down(s,1,false,HTBOTTOM);auto after=*before;after.b+=28;phase(s,1,g,0,28,after,"Explorer_B_resize",true);up(s,1,g,after);
    five(s,0,"Explorer_A_post_resize");ctrl_move(s,2,22,18,"Explorer_Ctrl_C");
    s.log.emit("Explorer_I02","\"result\":\"PASS\",\"actual_Medium_new_roots\":true,\"human_uat\":\"NOT_RUN\"");
}
void cleanup_explorers(State& s){
    for(auto& target:s.explorer)if(target){try{close_explorer_target(s,*target);}catch(...){s.log.failed=true;}target.reset();}
    for(auto& target:s.retired_explorers)if(target){try{close_explorer_target(s,*target);}catch(...){s.log.failed=true;}}
    s.retired_explorers.clear();s.target_catalog.reset();
    for(auto& frame:s.frames)if(own(s,frame.hwnd)&&!IsWindowVisible(frame.hwnd))ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);
}
// Application-matrix automation stays in this guarded test executable. Process
// names, a returned PID, or an attractive title never authorize input.
std::uint64_t process_birth(HANDLE process){
    FILETIME born{},ended{},kernel{},user{};if(!GetProcessTimes(process,&born,&ended,&kernel,&user))return 0;
    return (static_cast<std::uint64_t>(born.dwHighDateTime)<<32)|born.dwLowDateTime;
}
std::vector<HWND> root_baseline(){
    std::vector<HWND> out;const bool complete=EnumWindows([](HWND h,LPARAM context)->BOOL{
        auto& roots=*reinterpret_cast<std::vector<HWND>*>(context);if(roots.size()>=1024)return FALSE;
        if(GetAncestor(h,GA_ROOT)==h)roots.push_back(h);return TRUE;},reinterpret_cast<LPARAM>(&out))!=FALSE;
    if(!complete)throw std::runtime_error("bounded_root_baseline_incomplete");return out;
}
std::string application_version(const fs::path& image){
    const HMODULE library=LoadLibraryExW(L"version.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)return "UNKNOWN";
    using Size=DWORD(WINAPI*)(LPCWSTR,LPDWORD);using Get=BOOL(WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID);using Query=BOOL(WINAPI*)(LPCVOID,LPCWSTR,LPVOID*,PUINT);
    const auto size=reinterpret_cast<Size>(GetProcAddress(library,"GetFileVersionInfoSizeW"));
    const auto get=reinterpret_cast<Get>(GetProcAddress(library,"GetFileVersionInfoW"));const auto query=reinterpret_cast<Query>(GetProcAddress(library,"VerQueryValueW"));
    DWORD ignored{};const DWORD length=size?size(image.c_str(),&ignored):0;std::string out="UNKNOWN";
    if(length&&length<=1024*1024&&get&&query){std::vector<BYTE> bytes(length);VS_FIXEDFILEINFO* fact{};UINT count{};
        if(get(image.c_str(),0,length,bytes.data())&&query(bytes.data(),L"\\",reinterpret_cast<void**>(&fact),&count)&&count>=sizeof(*fact)&&fact->dwSignature==0xfeef04bd)
            out=std::to_string(HIWORD(fact->dwFileVersionMS))+"."+std::to_string(LOWORD(fact->dwFileVersionMS))+"."+
                std::to_string(HIWORD(fact->dwFileVersionLS))+"."+std::to_string(LOWORD(fact->dwFileVersionLS));
    }
    FreeLibrary(library);return out;
}
void discover_application_children(ApplicationTarget& target){
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));if(snapshot.value==INVALID_HANDLE_VALUE)return;
    std::vector<PROCESSENTRY32W> facts;PROCESSENTRY32W item{sizeof(item)};
    if(Process32FirstW(snapshot.value,&item))do{if(facts.size()>=4096)break;facts.push_back(item);}while(Process32NextW(snapshot.value,&item));
    for(unsigned pass=0;pass<8&&target.processes.size()<64;++pass){bool added=false;
        for(const auto& candidate:facts){
            if(candidate.th32ProcessID==GetCurrentProcessId()||std::any_of(target.processes.begin(),target.processes.end(),[&](const auto& p){return p.pid==candidate.th32ProcessID;}))continue;
            const auto parent=std::find_if(target.processes.begin(),target.processes.end(),[&](const auto& p){return p.pid==candidate.th32ParentProcessID;});
            // Retain handles: a recycled PID cannot create a false ancestry
            // edge, including after the known launcher has already exited.
            if(parent==target.processes.end()||!parent->handle||process_birth(parent->handle->value)!=parent->born)continue;
            auto handle=std::make_unique<Handle>(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,candidate.th32ProcessID));
            if(!handle->value)continue;const auto born=process_birth(handle->value);
            if(!born||born<parent->born||born<target.processes.front().born)continue;
            FILETIME pb{},pe{},pk{},pu{};if(!GetProcessTimes(parent->handle->value,&pb,&pe,&pk,&pu))continue;
            const auto ended=(static_cast<std::uint64_t>(pe.dwHighDateTime)<<32)|pe.dwLowDateTime;
            if(ended&&born>ended)continue; // A child cannot be born after this exact parent died.
            target.processes.push_back({candidate.th32ProcessID,candidate.th32ParentProcessID,born,std::move(handle)});added=true;if(target.processes.size()>=64)break;
        }
        if(!added)break;
    }
}
bool application_fresh(State& s,const ApplicationTarget& target,HWND h,std::optional<product::Snapshot>* actual=nullptr,std::string* why=nullptr){
    const auto reject=[&](std::string reason){if(why)*why=std::move(reason);return false;};
    if(!h||std::find(target.baseline.begin(),target.baseline.end(),h)!=target.baseline.end()||GetAncestor(h,GA_ROOT)!=h||!s.target_catalog)return reject("not_new_root_or_catalog_unavailable");
    std::string admission;auto fresh=s.target_catalog->get(h,{},&admission,false,false);
    if(!fresh)return reject("Catalog:"+admission);
    if(!equal(fresh->exe,target.root_executable.wstring()))return reject("root_image_does_not_equal_explicit_requested_root_image");
    const auto process=std::find_if(target.processes.begin(),target.processes.end(),[&](const auto& p){return p.pid==fresh->pid&&p.born==fresh->born;});
    if(process==target.processes.end()||!process->handle||WaitForSingleObject(process->handle->value,0)!=WAIT_TIMEOUT||!medium(process->handle->value))return reject("root_not_live_Medium_in_held_birth_verified_launch_tree");
    if(target.kind=="Terminal"&&(!equal(fresh->window_class,L"ConsoleWindowClass")||
        target.processes.empty()||process->parent!=target.processes.front().pid))
        return reject("terminal_not_exact_ConsoleWindowClass_of_explicit_cmd_child");
    if(actual)*actual=std::move(fresh);if(why)*why="exact_candidate_eligible";return true;
}
void application_binding_facts(State& s,ApplicationTarget& target){
    // One bounded post-failure observation, restricted to retained launch-tree
    // process handles. No unknown window is adopted or operated by diagnostics.
    discover_application_children(target);
    for(const auto& process:target.processes){
        DWORD exit{},image_size=32768;std::wstring image(image_size,L' ');int rid=-1;bool elevated{};DWORD token_error{};
        const bool identity=process.handle&&process_birth(process.handle->value)==process.born;
        const bool exit_read=identity&&GetExitCodeProcess(process.handle->value,&exit)!=FALSE;
        const bool image_read=identity&&QueryFullProcessImageNameW(process.handle->value,0,image.data(),&image_size)!=FALSE;
        if(image_read)image.resize(image_size);const bool normal=identity&&medium(process.handle->value,&rid,&elevated,&token_error);
        s.log.emit("application_held_process","\"application\":"+quote(target.kind)+",\"pid\":"+std::to_string(process.pid)+
            ",\"parent_pid\":"+std::to_string(process.parent)+",\"birth\":"+std::to_string(process.born)+
            ",\"birth_matches_retained_handle\":"+(identity?"true":"false")+",\"exit_code\":"+(exit_read?std::to_string(exit):"null")+
            ",\"actual_image\":"+(image_read?quote(utf8(image)):"null")+",\"integrity_rid\":"+(rid<0?"null":std::to_string(rid))+
            ",\"elevated\":"+(rid<0?"null":elevated?"true":"false")+",\"actual_Medium\":"+(normal?"true":"false")+
            ",\"token_query_error\":"+std::to_string(token_error));
    }
    unsigned observed{};
    for(const HWND h:root_baseline()){
        DWORD pid{};const DWORD tid=GetWindowThreadProcessId(h,&pid);
        const auto process=std::find_if(target.processes.begin(),target.processes.end(),[&](const auto& item){return item.pid==pid;});
        if(process==target.processes.end()||!process->handle||process_birth(process->handle->value)!=process->born||
            std::find(target.baseline.begin(),target.baseline.end(),h)!=target.baseline.end())continue;
        ++observed;std::string reason;std::optional<product::Snapshot> candidate;
        const bool eligible=application_fresh(s,target,h,&candidate,&reason);std::array<wchar_t,256> cls{};GetClassNameW(h,cls.data(),static_cast<int>(cls.size()));
        s.log.emit("application_candidate_root","\"application\":"+quote(target.kind)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
            ",\"pid\":"+std::to_string(pid)+",\"tid\":"+std::to_string(tid)+",\"window_class\":"+quote(utf8(cls.data()))+
            ",\"visible\":"+(IsWindowVisible(h)?"true":"false")+",\"candidate_eligible\":"+(eligible?"true":"false")+
            ",\"reason\":"+quote(reason)+",\"not_binding_or_input_authority\":true");
    }
    s.log.emit("application_binding_observation","\"application\":"+quote(target.kind)+",\"held_tree_new_roots\":"+std::to_string(observed)+
        ",\"unknown_process_roots_not_queried\":true,\"single_post_failure_observation\":true");
}
bool application_down_context(State& s,HWND h,POINT point,int expected_hit,std::string_view stage){
    DWORD pid{},hit_pid{};const DWORD tid=GetWindowThreadProcessId(h,&pid);const HWND hit=WindowFromPoint(point);
    GetWindowThreadProcessId(hit,&hit_pid);DWORD_PTR nonclient{};SetLastError(0);
    const bool hit_read=hit&&SendMessageTimeoutW(hit,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
        SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&nonclient)!=FALSE;const DWORD hit_error=hit_read?0:GetLastError();
    GUITHREADINFO gui{sizeof(gui)};const bool gui_read=tid&&GetGUIThreadInfo(tid,&gui)!=FALSE;
    RECT positioning{};const bool positioned=GetWindowRect(h,&positioning)!=FALSE;const HRGN rgn=CreateRectRgn(0,0,0,0);
    const int region_type=rgn?GetWindowRgn(h,rgn):ERROR;
    const bool region_known=positioned&&region_type!=ERROR;
    const bool region_contains=region_known&&PtInRegion(rgn,point.x-positioning.left,point.y-positioning.top)!=FALSE;
    if(rgn)DeleteObject(rgn);
    s.log.emit("application_DOWN_context","\"stage\":"+quote(stage)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"pid\":"+std::to_string(pid)+",\"tid\":"+std::to_string(tid)+",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"]"+
        ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
        ",\"hit_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"hit_pid\":"+std::to_string(hit_pid)+
        ",\"hit_root_matches\":"+(hit&&GetAncestor(hit,GA_ROOT)==h?"true":"false")+
        ",\"WM_NCHITTEST\":"+(hit_read?std::to_string(static_cast<LRESULT>(nonclient)):"null")+",\"hit_query_error\":"+std::to_string(hit_error)+
        ",\"expected_nonclient\":"+std::to_string(expected_hit)+",\"GUI_query_succeeded\":"+(gui_read?"true":"false")+
        ",\"capture\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
        ",\"move_size\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
        ",\"menu_owner\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMenuOwner)):"null")+
        ",\"window_region_type\":"+std::to_string(region_type)+",\"region_contains_point\":"+(region_known?(region_contains?"true":"false"):"null")+
        ",\"actual_left_down\":"+((GetAsyncKeyState(VK_LBUTTON)&0x8000)?"true":"false")+",\"product_prepared_not_inferred\":true");
    // The earlier hit search is not perpetual permission. 3e observed a
    // caption initially, then ERROR_TIMEOUT before DOWN; do not inject based
    // on that stale search. Unknown GUI state is still not called "no capture"
    // and this receipt never substitutes for the product preparation gate.
    return hit_read&&static_cast<LRESULT>(nonclient)==expected_hit&&pid&&hit_pid==pid&&
        hit&&GetAncestor(hit,GA_ROOT)==h&&GetForegroundWindow()==h;
}
void application_prepare_barrier(State& s,HWND h,POINT point,int expected_hit){
    const auto mark=read(s.product_log).size();const bool sent=launch(s,L"--status",true);std::string status;
    const bool observed=sent&&wait([&]{status=record(s,mark,"status");return !status.empty();},2000);
    s.log.check("application_actual_STA_prepare_barrier",observed&&status.find("\"enabled\":true")!=std::string::npos&&
        status.find("\"hook_owner_running\":true")!=std::string::npos&&status.find("\"gesture_active\":false")!=std::string::npos,
        "\"production_status\":"+quote(status)+",\"actual_prepare_permission_not_inferred\":true");
    if(!application_down_context(s,h,point,expected_hit,"before_real_DOWN_after_STA_barrier"))
        throw std::runtime_error("application_actual_hit_unavailable_before_DOWN");
    if(!authorized_source(s,h)||GetForegroundWindow()!=h)throw std::runtime_error("application_identity_or_foreground_changed_before_DOWN");
}
std::shared_ptr<ApplicationTarget> provision_application(State& s,std::string kind){
    auto target=std::make_shared<ApplicationTarget>();target->kind=std::move(kind);
    const fs::path base=fs::path(L"C:\\PaneBindV1\\Applications")/s.run;
    if(!plain_directory(base.parent_path())&&!fs::create_directory(base.parent_path()))throw std::runtime_error("guest_application_root_not_available");
    if(!plain_directory(base)&&!fs::create_directory(base))throw std::runtime_error("application_RunId_directory_not_available");
    target->directory=base/(std::wstring(s.configuration.begin(),s.configuration.end())+L"-"+std::wstring(target->kind.begin(),target->kind.end()));
    if(fs::exists(target->directory)||!fs::create_directory(target->directory)||!plain_directory(target->directory))throw std::runtime_error("fresh_application_directory_required");
    std::array<wchar_t,MAX_PATH> system{};if(!GetSystemDirectoryW(system.data(),static_cast<UINT>(system.size())))throw std::runtime_error("official_guest_System_directory_unavailable");
    const fs::path blank=target->directory/L"PaneBind-empty.txt";
    if(target->kind=="Notepad"){target->executable=fs::path(system.data())/L"notepad.exe";std::ofstream(blank,std::ios::binary).close();target->command=L"\""+target->executable.wstring()+L"\" \""+blank.wstring()+L"\"";}
    else if(target->kind=="Edge"){
        for(const fs::path candidate:{fs::path(L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"),fs::path(L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe")})
            if(fs::is_regular_file(candidate)){target->executable=candidate;break;}
        const auto html=target->directory/L"PaneBind-empty.html";
        std::ofstream page(html,std::ios::binary);page<<"<!doctype html><meta charset=utf-8><title>PaneBind blank</title><body style='margin:0;height:100vh' onclick=\"document.title='PaneBind-Mouse-"<<utf8(s.run)<<"'\"></body>";page.close();
        if(!page)throw std::runtime_error("guest_offline_browser_fixture_write_failed");
        auto url=html.wstring();std::replace(url.begin(),url.end(),L'\\',L'/');
        target->command=L"\""+target->executable.wstring()+L"\" --user-data-dir=\""+(target->directory/L"Profile").wstring()+L"\" --no-first-run --no-default-browser-check --disable-background-mode --new-window \"file:///"+url+L"\"";
    }else if(target->kind=="VSCode"){
        target->executable=base/L"VSCode"/L"Code.exe";
        if(fs::exists(base/L"VSCode"/L"data"))throw std::runtime_error("VSCode_portable_data_would_override_exact_test_profile");
        std::ofstream(blank,std::ios::binary).close();
        target->command=L"\""+target->executable.wstring()+L"\" --user-data-dir \""+(target->directory/L"UserData").wstring()+L"\" --extensions-dir \""+(target->directory/L"Extensions").wstring()+L"\" --disable-extensions --new-window --skip-welcome --skip-release-notes \""+blank.wstring()+L"\"";
    }else if(target->kind=="Terminal"){
        target->executable=fs::path(system.data())/L"conhost.exe";
        // Actual 8cc evidence reports ConsoleWindowClass on the exact cmd
        // child, not the conhost launcher. Bind only this explicitly requested
        // image AND its retained direct-parent/birth relationship.
        target->root_executable=fs::path(system.data())/L"cmd.exe";
        target->command=L"\""+target->executable.wstring()+L"\" \""+target->root_executable.wstring()+L"\" /d /q /k title PaneBind-"+s.run+L"-Terminal";
    }else throw std::runtime_error("unknown_application_class");
    if(target->root_executable.empty())target->root_executable=target->executable;
    s.created_applications.push_back(target);target->baseline=root_baseline();
    if(target->executable.empty()||!fs::is_regular_file(target->executable)){
        s.log.emit("application_binding","\"application\":"+quote(target->kind)+",\"result\":\"BLOCKED\",\"reason\":\"explicit_official_executable_not_available\",\"input_sent\":false");return target;
    }
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};std::wstring line=target->command;
    const bool launched=CreateProcessW(target->executable.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,target->directory.c_str(),&startup,&info)!=FALSE;
    const DWORD error=launched?0:GetLastError();int rid=-1;bool elevated{},normal=false;
    if(launched){CloseHandle(info.hThread);normal=medium(info.hProcess,&rid,&elevated);target->processes.push_back({info.dwProcessId,GetCurrentProcessId(),process_birth(info.hProcess),std::make_unique<Handle>(info.hProcess)});}
    s.log.emit("application_launch","\"application\":"+quote(target->kind)+",\"command\":"+quote(utf8(line))+",\"executable\":"+quote(utf8(target->executable.wstring()))+
        ",\"explicit_expected_root_image\":"+quote(utf8(target->root_executable.wstring()))+
        ",\"file_version\":"+quote(application_version(target->executable))+",\"binary_sha256\":"+quote(hash(target->executable))+
        ",\"api_succeeded\":"+(launched?"true":"false")+",\"win32_error\":"+std::to_string(error)+",\"launch_PID\":"+std::to_string(launched?info.dwProcessId:0)+
        ",\"launch_integrity_rid\":"+(rid<0?"null":std::to_string(rid))+",\"launch_elevated\":"+(rid<0?"null":(elevated?"true":"false"))+",\"launch_is_not_binding_proof\":true");
    std::optional<product::Snapshot> candidate;bool bound=false;
    if(normal&&target->processes.front().born)bound=wait([&]{
        discover_application_children(*target);candidate.reset();unsigned matches{};
        for(HWND h:root_baseline()){std::optional<product::Snapshot> fresh;if(application_fresh(s,*target,h,&fresh)){++matches;candidate=std::move(fresh);}}
        return matches==1;
    },12000);
    if(bound){target->identity=candidate;bound=test_root_stamp(s,*candidate,true);target->accepted=bound;}
    if(!bound&&launched)application_binding_facts(s,*target);
    s.log.emit("application_binding","\"application\":"+quote(target->kind)+",\"result\":"+quote(bound?"BOUND":"BLOCKED")+
        ",\"reason\":"+quote(bound?"baseline_excluded_new_root_exact_launch_tree_actual_Catalog_Medium_same_SID_session_desktop":"no_unique_eligible_root_in_held_birth_verified_launch_tree")+
        ",\"hwnd\":"+std::to_string(bound?reinterpret_cast<std::uintptr_t>(candidate->hwnd):0)+",\"pid\":"+std::to_string(bound?candidate->pid:0)+
        ",\"tid\":"+std::to_string(bound?candidate->tid:0)+",\"birth\":"+std::to_string(bound?candidate->born:0)+",\"key\":"+std::to_string(bound?candidate->key:0)+
        ",\"window_class\":"+quote(bound?utf8(candidate->window_class):"UNKNOWN")+",\"retained_process_facts\":"+std::to_string(target->processes.size())+
        ",\"input_sent\":false,\"product_permission_override\":false");return target;
}
bool close_application(State& s,ApplicationTarget& target){
    if(target.cleanup_confirmed)return true;
    bool attempted=false,gone=false;
    if(target.accepted&&target.identity){std::optional<product::Snapshot> fresh;
        if(application_fresh(s,target,target.identity->hwnd,&fresh)&&product::Catalog::same(*target.identity,*fresh)&&test_root_stamp(s,*target.identity)){
            attempted=PostMessageW(fresh->hwnd,WM_CLOSE,0,0)!=FALSE;gone=attempted&&wait([&]{return !IsWindow(fresh->hwnd);},3000);
        }else gone=!IsWindow(target.identity->hwnd);
        if(!gone)s.log.failed=true;
    }
    target.cleanup_confirmed=gone;
    s.log.emit("application_cleanup","\"application\":"+quote(target.kind)+",\"root_confirmed\":"+(target.accepted?"true":"false")+
        ",\"exact_root_close_attempted\":"+(attempted?"true":"false")+",\"actual_root_gone\":"+(gone?"true":"false")+
        ",\"unknown_windows_touched\":false,\"processes_force_terminated\":false,\"guest_local_data_retained\":true");return gone||!target.accepted;
}
panebind::core::v1::Settings matrix_settings(State& s){
    const auto path=s.product.parent_path()/L"settings.ini";if(!fs::exists(path))return panebind::core::v1::default_settings();
    auto parsed=panebind::core::v1::parse(read(path));if(!parsed.value)throw std::runtime_error("matrix_actual_product_configuration_invalid");return *parsed.value;
}
bool matrix_show_source(State& s,std::size_t source,bool show){
    const HWND h=source_window(s,source);if(own(s,h)){ShowWindow(h,show?SW_SHOWNOACTIVATE:SW_HIDE);return true;}
    const product::Snapshot* identity=s.applications[source]&&s.applications[source]->identity?&*s.applications[source]->identity:
        s.explorer[source]&&s.explorer[source]->identity?&*s.explorer[source]->identity:nullptr;
    DWORD pid{};std::array<wchar_t,256> cls{};const DWORD tid=GetWindowThreadProcessId(h,&pid);
    if(!identity||h!=identity->hwnd||pid!=identity->pid||tid!=identity->tid||!test_root_stamp(s,*identity)||GetClassNameW(h,cls.data(),static_cast<int>(cls.size()))<=0||!equal(cls.data(),identity->window_class))return false;
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));std::array<wchar_t,32768> path{};DWORD length=static_cast<DWORD>(path.size());
    if(!process.value||WaitForSingleObject(process.value,0)!=WAIT_TIMEOUT||process_birth(process.value)!=identity->born||!medium(process.value)||
        !QueryFullProcessImageNameW(process.value,0,path.data(),&length)||!equal(path.data(),identity->exe))return false;
    ShowWindow(h,show?SW_SHOWNOACTIVATE:SW_HIDE);return (IsWindowVisible(h)!=FALSE)==show;
}
LONG matrix_pixels(int dip,UINT dpi){
    if(dip<0||dip>20000||dpi<48||dpi>960)throw std::runtime_error("matrix_DPI_or_parameter_out_of_range");
    return static_cast<LONG>((static_cast<std::int64_t>(dip)*dpi+48)/96);
}
std::array<product::Snapshot,3> matrix_sources(State& s){
    std::array<product::Snapshot,3> out{};
    for(std::size_t n=0;n<out.size();++n){const HWND h=source_window(s,n);if(!authorized_source(s,h))throw std::runtime_error("matrix_source_identity_no_longer_authorized");
        if(own(s,h)){
            // A Catalog correctly excludes its caller's own UI. This fixture
            // reads only its exact owned HWND independently; these facts are
            // NOT an admission receipt for the separate product process.
            const auto actual=visible(h);RECT positioning{};MONITORINFO monitor{sizeof(monitor)};MINMAXINFO mm{};DWORD_PTR answered{};
            const auto display=MonitorFromWindow(h,MONITOR_DEFAULTTONULL);const auto dpi=GetDpiForWindow(h);
            if(!actual||!GetWindowRect(h,&positioning)||!display||!dpi||!GetMonitorInfoW(display,&monitor)||!medium(GetCurrentProcess())||
                !SendMessageTimeoutW(h,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&mm),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&answered))throw std::runtime_error("matrix_owned_independent_native_geometry_unavailable");
            auto& fact=out[n];fact.hwnd=h;fact.pid=GetCurrentProcessId();fact.tid=s.owner;fact.born=process_birth(GetCurrentProcess());fact.dpi=dpi;fact.monitor=display;fact.can_resize=true;
            fact.visible=panebind::core::v1::Rect{actual->l,actual->t,actual->r,actual->b};fact.positioning=panebind::core::v1::Rect{positioning.left,positioning.top,positioning.right,positioning.bottom};
            fact.work=panebind::core::v1::Rect{monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right,monitor.rcWork.bottom};
            const LONG horizontal=positioning.right-positioning.left-actual->width(),vertical=positioning.bottom-positioning.top-actual->height();
            fact.limits={std::max<LONG>(1,mm.ptMinTrackSize.x-horizontal),std::max<LONG>(1,mm.ptMinTrackSize.y-vertical),
                std::max<LONG>(1,mm.ptMaxTrackSize.x-horizontal),std::max<LONG>(1,mm.ptMaxTrackSize.y-vertical),1,1};
            fact.maximized=IsZoomed(h)!=FALSE;fact.minimized=IsIconic(h)!=FALSE;
        }else{auto fresh=s.target_catalog->get(h,{});if(!fresh)throw std::runtime_error("matrix_source_not_current_Medium_Catalog_eligible");out[n]=*fresh;}}
    return out;
}
void matrix_layout(State& s,std::string_view label){
    for(std::size_t n=0;n<3;++n)if(!matrix_show_source(s,n,true))throw std::runtime_error("matrix_real_source_show_identity_failed");
    auto facts=matrix_sources(s);
    for(const auto& fact:facts)if(fact.maximized||fact.minimized){
        const bool requested=ShowWindowAsync(fact.hwnd,SW_RESTORE)!=FALSE;
        s.log.check("matrix_setup_actual_native_restore",requested&&wait([&]{return !IsIconic(fact.hwnd)&&!IsZoomed(fact.hwnd);}),
            "\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(fact.hwnd))+",\"setup_not_product_restore_test\":true");}
    facts=matrix_sources(s);const auto& work=facts[0].work;const auto config=matrix_settings(s);
    LONG width=matrix_pixels(500,facts[0].dpi),height=matrix_pixels(220,facts[0].dpi);
    const LONG margin=matrix_pixels(std::max(100,config.release_dip*4),facts[0].dpi);
    for(std::size_t n=0;n<facts.size();++n){if(facts[n].monitor!=facts[0].monitor||facts[n].dpi!=facts[0].dpi)throw std::runtime_error("matrix_requires_one_actual_DPI_monitor");
        width=std::max(width,static_cast<LONG>(facts[n].limits.min_width));
        height=std::max(height,static_cast<LONG>(n?(facts[n].limits.min_height):((facts[n].limits.min_height+1)/2)));}
    width+=matrix_pixels(100,facts[0].dpi); // Real min-size slack for shared Resize.
    const LONG x=static_cast<LONG>(work.left())+margin,y=static_cast<LONG>(work.top())+margin;
    if(2*width+2*margin>work.width()||2*height+2*margin>work.height())throw std::runtime_error("matrix_real_work_area_too_small_for_declared_application_constraints");
    const std::array<Rect,3> layout{{{x,y,x+width,y+2*height},{x+width,y,x+2*width,y+height},{x+width,y+height,x+2*width,y+2*height}}};
    for(std::size_t n=0;n<facts.size();++n){if(layout[n].width()>facts[n].limits.max_width||layout[n].height()>facts[n].limits.max_height)throw std::runtime_error("matrix_layout_exceeds_actual_application_maximum");
        if(!authorized_source(s,facts[n].hwnd))throw std::runtime_error("matrix_identity_changed_before_layout");
        ShowWindow(facts[n].hwnd,SW_SHOWNOACTIVATE);
        s.log.check("matrix_real_setup_layout",fixture_rect(facts[n].hwnd,layout[n]),"\"stage\":"+quote(label)+",\"source\":"+std::to_string(n)+
            ",\"expected\":"+rect(layout[n])+",\"min_visible_size\":["+std::to_string(facts[n].limits.min_width)+","+std::to_string(facts[n].limits.min_height)+
            "],\"dpi\":"+std::to_string(facts[n].dpi)+",\"work_area\":["+std::to_string(work.left())+","+std::to_string(work.top())+","+std::to_string(work.right())+","+std::to_string(work.bottom())+
            "],\"fixture_setup_not_product_placement\":true");}
}
void matrix_five(State& s,std::size_t source,const std::string& label){
    const auto facts=matrix_sources(s);const auto config=matrix_settings(s);const auto original=visible(source_window(s,source));
    if(!original||config.attraction_dip<4)throw std::runtime_error("matrix_five_requires_actual_nonzero_attraction_configuration");
    const UINT dpi=facts[source].dpi;const LONG travel=matrix_pixels(std::max(config.release_dip+24,40),dpi);
    const LONG near_delta=std::max<LONG>(1,matrix_pixels(std::max(1,config.attraction_dip/2),dpi));
    const LONG dy=source==1?-travel:travel,dx=source==0?-travel:travel;
    const auto g=down(s,source);phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".free",true);
    phase(s,source,g,-near_delta,std::max<LONG>(1,near_delta/2),*original,label+".xy_snap",true);
    phase(s,source,g,-near_delta-1,std::max<LONG>(1,near_delta/2)+1,*original,label+".hold",false);
    phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".detach",true);
    phase(s,source,g,-near_delta+1,std::max<LONG>(1,near_delta/2),*original,label+".resnap",true);up(s,source,g,*original);
}
void matrix_commands(State& s,const std::string& label){
    const HWND h=source_window(s,0);const auto facts=matrix_sources(s);const auto original=visible(h);if(!original)throw std::runtime_error("matrix_command_geometry_unknown");
    // Hide only the two exact test-created targets so smart stretch has the
    // independently known work-area boundary, not an accidental neighbour.
    for(std::size_t n=1;n<3;++n)if(!authorized_source(s,source_window(s,n))||!matrix_show_source(s,n,false))throw std::runtime_error("matrix_hide_target_identity_changed");
    const auto& w=facts[0].work;Rect center{static_cast<LONG>(w.left()+(w.width()-original->width())/2),static_cast<LONG>(w.top()+(w.height()-original->height())/2),0,0};center.r=center.l+original->width();center.b=center.t+original->height();
    const auto command=[&](std::wstring action,Rect expected){
        foreground(s,h);const auto before=visible(h);const auto mark=read(s.product_log).size();const auto limits=facts[0].limits;
        const bool legal=expected.width()>=limits.min_width&&expected.width()<=limits.max_width&&expected.height()>=limits.min_height&&expected.height()<=limits.max_height;
        const bool sent=launch(s,L"--command "+action,true);bool observed=false;std::string evidence;
        if(legal)observed=sent&&wait([&]{return visible(h)==expected;});
        else observed=sent&&wait([&]{evidence=record(s,mark,"placement");return visible(h)==before&&evidence.find("\"native_attempted\":false")!=std::string::npos&&evidence.find("target_violates_size_constraints")!=std::string::npos;});
        s.log.check(label+".command_"+utf8(action),observed,"\"requested\":"+rect(expected)+",\"actual\":"+(visible(h)?rect(*visible(h)):"null")+
            ",\"expected_outcome\":"+quote(legal?"exact":"declared_application_constraint_safe_rejection")+",\"rejection_receipt\":"+quote(evidence));
        return legal;
    };
    command(L"Center",center);
    const LONG split=static_cast<LONG>(w.left()+w.width()/2);const Rect half{static_cast<LONG>(w.left()),static_cast<LONG>(w.top()),split,static_cast<LONG>(w.bottom())};
    if(command(L"DockLeft",half))command(L"Undo",center);
    if(command(L"StretchHorizontal",{static_cast<LONG>(w.left()),center.t,static_cast<LONG>(w.right()),center.b}))command(L"Undo",center);
    for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,true))throw std::runtime_error("matrix_hidden_target_identity_changed");
}
void matrix_text(State& s,const std::wstring& text){
    for(const wchar_t c:text){if(!authorized_source(s,source_window(s,0))||GetForegroundWindow()!=source_window(s,0))throw std::runtime_error("matrix_text_target_not_exact_foreground");
        for(const bool release:{false,true}){INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wScan=c;
            packet.ki.dwFlags=KEYEVENTF_UNICODE|(release?KEYEVENTF_KEYUP:0);packet.ki.dwExtraInfo=0x50425631;
            if(!desktop()||SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("matrix_client_text_SendInput_failed");}}
}
void matrix_chord(State& s,WORD key_code){key(s,VK_CONTROL,true);key(s,key_code,true);key(s,key_code,false);key(s,VK_CONTROL,false);}
bool matrix_client_child(State& s,const product::Snapshot& source,HWND child){
    DWORD pid{};if(!child||GetAncestor(child,GA_ROOT)!=source.hwnd||!GetWindowThreadProcessId(child,&pid))return false;
    if(pid==source.pid)return true;
    const auto& target=s.applications[0];if(!target)return false;discover_application_children(*target);
    const auto process=std::find_if(target->processes.begin(),target->processes.end(),[&](const auto& p){return p.pid==pid;});
    return process!=target->processes.end()&&process->handle&&WaitForSingleObject(process->handle->value,0)==WAIT_TIMEOUT&&process_birth(process->handle->value)==process->born;
}
void matrix_escape_and_client(State& s,const std::string& label){
    const HWND h=source_window(s,0);const auto original=visible(h);if(!original)throw std::runtime_error("matrix_escape_geometry_unknown");
    const auto facts=matrix_sources(s);const auto config=matrix_settings(s);const LONG travel=matrix_pixels(std::max(config.release_dip+24,40),facts[0].dpi);
    for(std::size_t n=1;n<3;++n)if(!authorized_source(s,source_window(s,n))||!matrix_show_source(s,n,false))throw std::runtime_error("matrix_Escape_target_hide_identity_failed");
    const auto g=down(s,0);phase(s,0,g,-travel,travel,translated(*original,-travel,travel),label+".Escape_before",true);
    const auto mark=read(s.product_log).size();key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);std::string end;
    const bool escaped=wait([&]{end=record(s,mark,"gesture_end",g.generation);return !end.empty()&&visible(h)==original;});
    button(s,false); // Test cleanup after Escape, deliberately not NormalUp.
    s.log.check(label+".Escape_restore",escaped&&end.find("\"normal_up\":false")!=std::string::npos,
        "\"production_end\":"+quote(end)+",\"actual\":"+(visible(h)?rect(*visible(h)):"null"));
    foreground(s,h);RECT client{};if(!GetClientRect(h,&client))throw std::runtime_error("matrix_client_geometry_unknown");
    POINT point{(client.right-client.left)/2,(client.bottom-client.top)/2};if(!ClientToScreen(h,&point))throw std::runtime_error("matrix_client_screen_unknown");cursor(s,point);
    const HWND hit=WindowFromPoint(point);DWORD_PTR region{};
    const bool actual=matrix_client_child(s,facts[0],hit)&&
        SendMessageTimeoutW(hit,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)&&region==HTCLIENT;
    if(!actual)throw std::runtime_error("matrix_actual_same_process_client_hit_unavailable");
    const auto click_mark=read(s.product_log).size();button(s,true);button(s,false);Sleep(150);GUITHREADINFO gui{sizeof(gui)};
    const bool focused=GetGUIThreadInfo(source_thread(s,0),&gui)&&gui.hwndFocus&&GetAncestor(gui.hwndFocus,GA_ROOT)==h;
    s.log.check(label+".client_click_not_routed_to_Move",record(s,click_mark,"gesture_start").empty()&&visible(h)==original&&focused,
        "\"actual_hit_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"focus_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndFocus))+
        ",\"document_mutation\":false,\"semantic_text_delivery_not_proven_by_focus\":true");
    const auto target=s.applications[0];if(!target)throw std::runtime_error("matrix_client_application_binding_missing");
    const auto nonce=L"PaneBind-Client-"+s.run;
    if(target->kind=="Edge"){
        const auto expected=L"PaneBind-Mouse-"+s.run;std::array<wchar_t,1024> title{};
        const bool delivered=wait([&]{return GetWindowTextW(h,title.data(),static_cast<int>(title.size()))>0&&std::wstring(title.data()).find(expected)!=std::wstring::npos;});
        s.log.check(label+".actual_offline_page_client_click",delivered,"\"DOM_click_effect_observed_in_native_title\":"+std::string(delivered?"true":"false")+",\"offline_test_page_only\":true");
    }else if(target->kind=="Notepad"||target->kind=="VSCode"){
        const auto blank=target->directory/L"PaneBind-empty.txt";
        if(!read(blank,4096).empty()||fs::file_size(blank)!=0)throw std::runtime_error("matrix_editor_blank_input_changed_before_test");
        matrix_chord(s,VK_END);matrix_text(s,nonce);matrix_chord(s,'S');
        const auto plain=utf8(nonce);std::string utf16;
        for(const wchar_t c:nonce){utf16.push_back(static_cast<char>(c&255));utf16.push_back(static_cast<char>((c>>8)&255));}
        const auto content_matches=[&]{const auto bytes=read(blank,4096);return bytes==plain||bytes==std::string("\xef\xbb\xbf")+plain||bytes==std::string("\xff\xfe")+utf16;};
        s.log.check(label+".actual_client_text_and_native_save",wait(content_matches),"\"guest_new_file_only\":true,\"expected_text_bytes\":"+std::to_string(plain.size())+
            ",\"actual_saved_sha256\":"+quote(hash(blank))+",\"clipboard_or_private_API_used\":false");
        matrix_chord(s,'A');key(s,VK_BACK,true);key(s,VK_BACK,false);matrix_chord(s,'S');
        s.log.check(label+".blank_document_restored_before_close",wait([&]{if(!fs::is_regular_file(blank))return false;const auto bytes=read(blank,4096);return
            (bytes.empty()&&fs::file_size(blank)==0)||bytes==std::string("\xef\xbb\xbf")||bytes==std::string("\xff\xfe");}));
    }else if(target->kind=="Terminal"){
        const auto receipt=target->directory/L"client-input-receipt.txt";if(fs::exists(receipt))throw std::runtime_error("matrix_terminal_receipt_must_be_new");
        key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);matrix_text(s,L"echo "+nonce+L">\""+receipt.wstring()+L"\"");key(s,VK_RETURN,true);key(s,VK_RETURN,false);
        s.log.check(label+".actual_terminal_client_input",wait([&]{auto text=read(receipt,4096);while(!text.empty()&&(text.back()=='\r'||text.back()=='\n'||text.back()==' '))text.pop_back();return text==utf8(nonce);}),
            "\"known_cmd_child_in_new_conhost_only\":true,\"guest_new_output_file_only\":true,\"actual_receipt_sha256\":"+quote(hash(receipt)));
    }
    s.log.check(label+".client_actions_did_not_start_geometry_gesture",record(s,click_mark,"gesture_start").empty()&&visible(h)==original);
    for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,true))throw std::runtime_error("matrix_Escape_hidden_target_identity_changed");
}
void matrix_input_idle(State& s){
    if(s.left){button(s,false);s.log.emit("test_cleanup","\"synthetic_release_sent\":true,\"not_product_NormalUp\":true");}
    if(s.control)key(s,VK_CONTROL,false);
    const auto mark=read(s.product_log).size();const bool sent=launch(s,L"--status",true);std::string status;
    const bool idle=sent&&wait([&]{status=record(s,mark,"status");return status.find("\"gesture_active\":false")!=std::string::npos;});
    s.log.check("matrix_between_categories_actual_idle",idle&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000),
        "\"production_status\":"+quote(status)+",\"no_new_input_if_unknown\":true");
}
void matrix_mixed_group(State& s,const std::shared_ptr<ApplicationTarget>& browser,const std::shared_ptr<ApplicationTarget>& editor){
    if(!browser||!editor||browser==editor||!browser->matrix_passed||!editor->matrix_passed||!s.explorer[0]){
        s.log.failed=true;s.log.emit("mixed_application_group","\"result\":\"BLOCKED\",\"reason\":\"exact_Medium_Explorer_browser_and_editor_preconditions_not_all_passed\"");return;
    }
    for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,false))throw std::runtime_error("mixed_unused_Explorer_hide_identity_failed");
    s.applications[1]=browser;s.applications[2]=editor;
    for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,true))throw std::runtime_error("mixed_confirmed_application_restore_failed");
    matrix_layout(s,"fresh_real_Explorer_browser_editor_T_not_replacement_for_prior_I02");
    const auto facts=matrix_sources(s);const LONG dx=matrix_pixels(34,facts[0].dpi),dy=matrix_pixels(18,facts[0].dpi);
    ctrl_move(s,0,dx,0,"Mixed_Explorer_Ctrl");matrix_five(s,1,"Mixed_browser_Move");
    std::array<Rect,3> before{};for(std::size_t n=0;n<3;++n){const auto actual=visible(source_window(s,n));if(!actual)throw std::runtime_error("mixed_shared_Resize_geometry_unknown");before[n]=*actual;}
    const auto fresh=matrix_sources(s);const LONG resize=matrix_pixels(24,fresh[0].dpi);
    if(before[1].width()-resize<fresh[1].limits.min_width||before[2].width()-resize<fresh[2].limits.min_width)throw std::runtime_error("mixed_shared_Resize_no_real_constraint_slack");
    auto after=before;after[0].r+=resize;after[1].l+=resize;after[2].l+=resize;
    const auto g=down(s,0,true,HTRIGHT);phase(s,0,g,resize,0,after[0],"Mixed_Ctrl_shared_vertical_Resize.source",true);
    for(std::size_t n=1;n<3;++n)s.log.check("Mixed_Ctrl_shared_vertical_Resize.member_"+std::to_string(n),wait([&]{return visible(source_window(s,n))==after[n];}),
        "\"expected\":"+rect(after[n])+",\"actual\":"+(visible(source_window(s,n))?rect(*visible(source_window(s,n))):"null"));
    up(s,0,g,after[0]);matrix_five(s,0,"Mixed_Explorer_post_Resize");ctrl_move(s,2,matrix_pixels(22,fresh[2].dpi),dy,"Mixed_editor_Ctrl_after_Resize");
    s.log.emit("mixed_application_group","\"result\":\"PASS\",\"members\":[\"Explorer\","+quote(browser->kind)+","+quote(editor->kind)+
        "],\"real_Medium_roots\":true,\"Ctrl_Move_shared_Resize_post_Resize_Move\":true,\"not_full_I03_or_human_UAT\":true");
}
void cleanup_applications(State& s){
    s.applications={};
    for(const auto& target:s.created_applications)if(target&&target->accepted&&!target->cleanup_confirmed){s.applications[0]=target;
        if(!matrix_show_source(s,0,true))s.log.failed=true;close_application(s,*target);s.applications[0].reset();}
    s.created_applications.clear();
}
void application_matrix_flow(State& s){
    if(!s.target_catalog)s.target_catalog=std::make_unique<product::Catalog>();
    for(auto& frame:s.frames)if(own(s,frame.hwnd))ShowWindow(frame.hwnd,SW_HIDE);
    std::shared_ptr<ApplicationTarget> browser,editor;
    for(const std::string kind:{"Notepad","Edge","VSCode","Terminal"}){
        std::shared_ptr<ApplicationTarget> target;bool passed=false;std::string reason;
        try{
            matrix_input_idle(s);target=provision_application(s,kind);
            if(!target->accepted){reason="no_exact_eligible_new_root";s.log.failed=true;}
            else{
                if(!matrix_show_source(s,0,false))throw std::runtime_error("matrix_unused_exact_source_hide_failed");
                s.applications[0]=target;matrix_layout(s,kind+"_independent_actual_constraints_setup");matrix_five(s,0,kind+".Move");
                const auto before=visible(source_window(s,0));if(!before)throw std::runtime_error("matrix_Resize_before_geometry_unknown");
                const auto identity=s.target_catalog->get(source_window(s,0),{},nullptr,false,false);if(!identity)throw std::runtime_error("matrix_Resize_actual_identity_changed");
                const LONG amount=matrix_pixels(28,identity->dpi);const auto g=down(s,0,false,HTBOTTOM);auto after=*before;after.b+=amount;
                phase(s,0,g,0,amount,after,kind+".Resize",true);up(s,0,g,after);
                matrix_commands(s,kind);matrix_escape_and_client(s,kind);target->matrix_passed=true;passed=true;
            }
        }catch(const std::exception& error){reason=error.what();s.log.failed=true;}
        // Always recover input before attempting another category. This is
        // separately reported test cleanup, never manufactured product UP.
        matrix_input_idle(s);
        for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,true)){s.log.failed=true;throw std::runtime_error("matrix_exact_Explorer_restore_failed_after_category");}
        if(passed&&(kind=="Edge"||kind=="Notepad"||kind=="VSCode")){
            if(kind=="Edge")browser=target;
            if(kind=="Notepad"||(!editor&&kind=="VSCode"))editor=target;
            if(!matrix_show_source(s,0,false))throw std::runtime_error("matrix_confirmed_application_hide_failed");
        }else if(target)close_application(s,*target);
        s.applications[0].reset();
        s.log.emit("application_matrix_result","\"application\":"+quote(kind)+",\"result\":"+quote(passed?"PASS":target&&target->accepted?"FAIL":"BLOCKED")+
            ",\"reason\":"+quote(reason)+",\"scope\":\"actual_Medium_Move_Resize_commands_Escape_client\",\"human_uat\":\"NOT_RUN\"");
    }
    try{matrix_mixed_group(s,browser,editor);}
    catch(const std::exception& error){s.log.failed=true;s.log.emit("mixed_application_group","\"result\":\"FAIL\",\"reason\":"+quote(error.what()));matrix_input_idle(s);}
    cleanup_applications(s);
    for(std::size_t n=0;n<3;++n)if(!matrix_show_source(s,n,true))s.log.failed=true;
    for(auto& frame:s.frames)if(own(s,frame.hwnd))ShowWindow(frame.hwnd,SW_SHOWNOACTIVATE);
}
enum class BlockedWriterExit { RealUp, EmergencyHotkey };
// Deliberately NOT invoked by drive(): first establish the normal production
// chain. This case terminates that product instance and belongs at the end of
// an explicitly selected L2 run, never in the ordinary application path.
[[maybe_unused]] void blocked_writer_case(State& s,std::size_t source,BlockedWriterExit exit){
    if(source>=s.frames.size()||!own(s,s.frames[source].hwnd)||!s.process)throw std::runtime_error("L2_exact_owned_source_required");
    auto& stall=s.frames[source].stall;
    s.log.check("L2_no_previous_stall",!stall.armed&&!stall.entered&&!stall.finished);
    const auto g=down(s,source);stall.armed=true;Sleep(60);cursor(s,{g.anchor.x-42,g.anchor.y+35});
    s.log.check("L2_native_call_observed_in_owned_wndproc",wait([&]{return stall.entered.load();},800)&&s.left&&g.generation&&
        actual_up(s,g.offset,g.generation).empty(),"\"generation\":"+std::to_string(g.generation)+
        ",\"message\":\"WM_WINDOWPOSCHANGING\",\"bounded_sleep_ms\":1000,\"driver_DOWN_outstanding\":"+(s.left?"true":"false")+
        ",\"async_left_state\":"+std::to_string(stall.async_left_state.load()));
    s.log.emit("L2_native_call_entered","\"generation\":"+std::to_string(g.generation)+",\"actual_owned_hwnd\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(s.frames[source].hwnd))+",\"native_entry_qpc\":"+std::to_string(stall.before_qpc)+
        ",\"actual_flags\":"+std::to_string(stall.flags)+",\"actual_positioning\":["+std::to_string(stall.x)+","+std::to_string(stall.y)+","+
        std::to_string(stall.width)+","+std::to_string(stall.height)+"],\"async_left_state_at_entry\":"+
        std::to_string(stall.async_left_state.load())+",\"async_sample_not_delivery_proof\":true");
    const auto mark=read(s.product_log).size();LARGE_INTEGER sent{};QueryPerformanceCounter(&sent);
    if(exit==BlockedWriterExit::RealUp)button(s,false);
    else{
        key(s,VK_CONTROL,true);key(s,VK_SHIFT,true);key(s,VK_F11,true);key(s,VK_F11,false);key(s,VK_SHIFT,false);key(s,VK_CONTROL,false);
    }
    s.log.emit("L2_terminator_sent","\"generation\":"+std::to_string(g.generation)+",\"kind\":"+
        quote(exit==BlockedWriterExit::RealUp?"single_test_real_up":"actual_CtrlShiftF11")+",\"send_before_qpc\":"+
        std::to_string(sent.QuadPart)+",\"source_still_blocked_after_send\":"+(stall.finished?"false":"true"));
    const auto status_begin=GetTickCount64();DWORD status_exit{};const bool status_command=launch(s,L"--status",true,&status_exit);
    const auto status=record(s,mark,"status");const auto status_ms=GetTickCount64()-status_begin;
    s.log.check("L2_GUI_status_independent_of_issued_call",status_command&&!status.empty()&&!stall.finished,
        "\"actual_elapsed_ms\":"+std::to_string(status_ms)+",\"command_exit\":"+std::to_string(status_exit)+",\"status\":"+quote(status));
    if(exit==BlockedWriterExit::EmergencyHotkey){
        const auto hotkey=record(s,mark,"emergency_hotkey");
        s.log.check("L2_actual_WM_HOTKEY",hotkey.find("\"native_message\":\"WM_HOTKEY\"")!=std::string::npos&&
            hotkey.find("\"registration_id\":910")!=std::string::npos,"\"production_event\":"+quote(hotkey));
    }
    const auto end=record(s,g.offset,"gesture_end",g.generation);
    s.log.check("L2_failed_native_call_not_NormalUp",!end.empty()&&end.find("\"normal_up\":false")!=std::string::npos,"\"production_end\":"+quote(end));
    // Once the actual terminator was observed and the generation ended, extra
    // cursor events may not start new calls. This does not cancel the old one.
    cursor(s,{g.anchor.x-58,g.anchor.y+45});const bool quit_started_while_blocked=!stall.finished;
    DWORD quit_exit{};const bool quit_command=launch(s,L"--quit",true,&quit_exit);
    const bool stopped=WaitForSingleObject(s.process,1500)==WAIT_OBJECT_0;DWORD code=STILL_ACTIVE;if(stopped)GetExitCodeProcess(s.process,&code);
    const auto shutdown=record(s,mark,"shutdown");const bool stopped_before_source_resumed=!stall.finished;
    s.log.check("L2_own_input_resources_exit_independently",quit_command&&stopped&&code==0&&quit_started_while_blocked&&stopped_before_source_resumed&&
        shutdown.find("\"own_hooks_stopped\":true")!=std::string::npos&&shutdown.find("\"own_overlay_destroyed\":true")!=std::string::npos,
        "\"quit_exit\":"+std::to_string(quit_exit)+",\"source_still_blocked_at_exit\":"+(stopped_before_source_resumed?"true":"false")+",\"shutdown\":"+quote(shutdown));
    s.log.emit("L2_native_completion_at_process_exit","\"native_worker_exited_reported\":"+
        std::string(shutdown.find("\"native_worker_exited\":true")!=std::string::npos?"true":"false")+
        ",\"issued_native_completion_unknown\":"+(shutdown.find("\"issued_native_completion_unknown\":true")!=std::string::npos?"true":"false")+
        ",\"issued_call_cancelled_claimed\":false,\"product_cleanup_pass_inferred\":false");
    CloseHandle(s.process);s.process=nullptr;
    s.log.check("L2_owned_source_bounded_resume",wait([&]{return stall.finished.load();},1500));
    if(s.left)button(s,false); // TEST cleanup after the measured product exit.
    std::istringstream rows(read(s.product_log).substr(g.offset));std::string row;unsigned issued{},late_issued{};bool timeout_unknown=false;
    while(std::getline(rows,row))if(row.find("\"type\":\"placement\"")!=std::string::npos&&number(row,"\"generation\":")==g.generation){
        // A timed-out issued call is a real callsite claim, not a fabricated
        // successful API return. The target's actual WindowProc entry above
        // independently proves that this call reached the native window.
        if(row.find("\"native_callsite_claimed\":true")!=std::string::npos){++issued;if(number(row,"\"native_before_qpc\":")>static_cast<std::uint64_t>(sent.QuadPart))++late_issued;}
        timeout_unknown=timeout_unknown||row.find("\"native_completion_known\":false")!=std::string::npos;
    }
    s.log.check("L2_no_new_native_attempt_after_terminator",issued==1&&late_issued==0&&timeout_unknown,
        "\"issued_receipts\":"+std::to_string(issued)+",\"issued_after_terminator\":"+std::to_string(late_issued)+
        ",\"old_native_call_may_finish_late\":true,\"source_return_qpc\":"+std::to_string(stall.after_qpc));
}
void cleanup(State& s){
    bool current_stop_confirmed=false;
    if(s.left){try{button(s,false);s.log.emit("test_cleanup","\"synthetic_release_sent\":true,\"not_product_cleanup_pass\":true");}catch(...){}}
    if(s.control){try{key(s,VK_CONTROL,false);}catch(...){}}
    if(s.process){const auto mark=read(s.product_log).size();const auto stopped_pid=s.product_pid;
        const bool command=launch(s,L"--quit",true);const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;
        const auto shutdown=record(s,mark,"shutdown");DWORD exit_code=STILL_ACTIVE;if(stopped)GetExitCodeProcess(s.process,&exit_code);
        s.log.emit("product_stop","\"quit_command_completed\":"+std::string(command?"true":"false")+",\"process_exit_observed\":"+(stopped?"true":"false")+
            ",\"product_exit_code\":"+std::to_string(exit_code)+",\"shutdown_report\":"+quote(shutdown));
        current_stop_confirmed=command&&stopped&&exit_code==0&&number(shutdown,"\"process_id\":")==stopped_pid&&
            shutdown.find("\"own_hooks_stopped\":true")!=std::string::npos&&shutdown.find("\"own_overlay_destroyed\":true")!=std::string::npos&&
            shutdown.find("\"native_worker_exited\":true")!=std::string::npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=std::string::npos&&
            shutdown.find("\"visual_recovery_stopped\":true")!=std::string::npos&&shutdown.find("\"visual_restore_confirmed\":true")!=std::string::npos;
        if(!current_stop_confirmed)s.log.failed=true;
        if(!stopped){TerminateProcess(s.process,90);WaitForSingleObject(s.process,2000);s.log.failed=true;s.log.emit("test_abort","\"own_product_forced_termination\":true,\"product_cleanup_pass\":false");}
        CloseHandle(s.process);s.process=nullptr;}
    // A successful normal stop is also checked using real delivery to a
    // remaining exact empty fixture, not merely the child exit status. Forced
    // termination remains a failure regardless of this test-only observation.
    // An earlier independent application failure remains a failure, but does
    // not prevent observation of this instance's actual input-resource exit.
    bool delivered=false;const bool check_delivery=current_stop_confirmed;
    for(auto& f:s.frames)if(check_delivery&&own(s,f.hwnd)&&IsWindowVisible(f.hwnd)){
        const bool front=SetForegroundWindow(f.hwnd)!=FALSE;RECT client{};POINT point{};
        if(!front||GetForegroundWindow()!=f.hwnd||!GetClientRect(f.hwnd,&client))break;
        point={(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(f.hwnd,&point))break;
        try{cursor(s,point);if(WindowFromPoint(point)!=f.hwnd)break;const auto downs=f.down.load(),ups=f.up.load();
            button(s,true);button(s,false);delivered=wait([&]{return f.down.load()==downs+1&&f.up.load()==ups+1;},1000);
        }catch(...){if(s.left){try{button(s,false);}catch(...){}}}
        break;
    }
    s.log.emit("post_stop_actual_client_click","\"executed\":"+std::string(check_delivery?"true":"false")+",\"actual_down_up_delivered\":"+std::string(delivered?"true":"false")+
        ",\"only_test_owned_window\":true,\"not_guest_destruction\":true");
    if(check_delivery&&!delivered)s.log.failed=true;
}
// Real GUI exercise in the already-guarded guest, using the SAME executable
// and production settings handlers. Native messages below only READ controls;
// every interaction is an actual SendInput mouse/key event, not BM_CLICK,
// WM_COMMAND, WM_SETTEXT or a private settings API.
struct SettingsGui {HWND root{},tab{};DWORD tid{};};
std::wstring gui_class(HWND h){std::array<wchar_t,128> name{};const int n=GetClassNameW(h,name.data(),static_cast<int>(name.size()));return n>0?std::wstring(name.data(),n):L"";}
bool gui_identity(State& s,const SettingsGui& gui,HWND control){
    DWORD root_pid{},control_pid{},app_pid{};const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");
    return s.process&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&app&&
        GetWindowThreadProcessId(app,&app_pid)&&app_pid==s.product_pid&&
        GetWindowThreadProcessId(gui.root,&root_pid)==gui.tid&&root_pid==s.product_pid&&
        gui_class(gui.root)==L"PaneBindV1SettingsWindow"&&GetWindow(gui.root,GW_OWNER)==app&&
        IsWindowVisible(gui.root)&&GetWindowThreadProcessId(control,&control_pid)==gui.tid&&control_pid==s.product_pid&&
        (control==gui.root||GetAncestor(control,GA_ROOT)==gui.root);
}
DWORD_PTR gui_read(State& s,const SettingsGui& gui,HWND control,UINT message,WPARAM w=0,LPARAM l=0){
    DWORD_PTR answer{};
    if(!gui_identity(s,gui,control)||!SendMessageTimeoutW(control,message,w,l,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&answer))
        throw std::runtime_error("GUI_exact_owned_control_read_failed");
    return answer;
}
std::wstring gui_text(State& s,const SettingsGui& gui,HWND control){
    std::array<wchar_t,512> value{};
    // WM_GETTEXT is a system-marshalled message. Never pass a local pointer
    // through cross-process TCM_GETITEMRECT or another WM_USER structure.
    const auto copied=gui_read(s,gui,control,WM_GETTEXT,value.size(),reinterpret_cast<LPARAM>(value.data()));
    if(copied>=value.size())throw std::runtime_error("GUI_control_text_unbounded");return {value.data(),copied};
}
HWND gui_focus(const SettingsGui& gui){GUITHREADINFO info{sizeof(info)};return GetGUIThreadInfo(gui.tid,&info)?info.hwndFocus:nullptr;}
HWND gui_page(State& s,const SettingsGui& gui){
    HWND page{};unsigned count{};
    for(HWND h=GetWindow(gui.root,GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT))if(gui_class(h)==L"PaneBindV1SettingsPage"&&IsWindowVisible(h)){
        if(!gui_identity(s,gui,h))throw std::runtime_error("GUI_page_identity_changed");page=h;++count;
    }
    if(count!=1)throw std::runtime_error("GUI_exactly_one_visible_page_required");return page;
}
HWND gui_find(State& s,const SettingsGui& gui,std::wstring_view cls,std::wstring_view prefix){
    for(HWND h=GetWindow(gui_page(s,gui),GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT))
        if(gui_class(h)==cls&&gui_text(s,gui,h).starts_with(prefix))return h;
    throw std::runtime_error("GUI_named_production_control_missing");
}
HWND gui_number(State& s,const SettingsGui& gui,std::wstring_view label){
    const auto caption=gui_find(s,gui,L"Static",label);const auto edit=GetWindow(caption,GW_HWNDNEXT);
    if(gui_class(edit)!=L"Edit"||GetParent(edit)!=GetParent(caption)||!gui_identity(s,gui,edit))
        throw std::runtime_error("GUI_label_and_actual_edit_not_adjacent");return edit;
}
void gui_click(State& s,const SettingsGui& gui,HWND control){
    if(!gui_identity(s,gui,control)||!IsWindowVisible(control)||!IsWindowEnabled(control)||s.left)
        throw std::runtime_error("GUI_click_target_not_exact_visible_enabled");
    RECT actual{},page{},client{};
    if(!GetWindowRect(control,&actual)||!GetWindowRect(GetParent(control),&page)||!GetClientRect(gui.root,&client))
        throw std::runtime_error("GUI_click_geometry_unavailable");
    MapWindowPoints(gui.root,nullptr,reinterpret_cast<POINT*>(&client),2);
    RECT clipped{},visible_part{};
    if(!IntersectRect(&clipped,&actual,&page)||!IntersectRect(&visible_part,&clipped,&client))throw std::runtime_error("GUI_control_not_reachable");
    const POINT point{visible_part.left+(visible_part.right-visible_part.left)/2,visible_part.top+(visible_part.bottom-visible_part.top)/2};
    cursor(s,point);const auto hit=WindowFromPoint(point);
    DWORD actual_hit_pid{};GetWindowThreadProcessId(hit,&actual_hit_pid);
    s.log.check("GUI_actual_hit_is_target_control",gui_identity(s,gui,control)&&(hit==control||IsChild(control,hit)),
        "\"control\":"+std::to_string(reinterpret_cast<std::uintptr_t>(control))+",\"control_class\":"+quote(utf8(gui_class(control)))+
        ",\"control_text\":"+quote(utf8(gui_text(s,gui,control)))+",\"rect\":"+rect({actual.left,actual.top,actual.right,actual.bottom})+
        ",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
        ",\"hit_class\":"+quote(utf8(gui_class(hit)))+",\"hit_pid\":"+std::to_string(actual_hit_pid));
    button(s,true);button(s,false);
    s.log.check("GUI_actual_mouse_click",wait([&]{return GetForegroundWindow()==gui.root&&gui_focus(gui)==control;}),
        "\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+",\"control\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(control))+",\"pid\":"+std::to_string(s.product_pid)+
        ",\"hit_confirmed\":true,\"input_kind\":\"SendInput_LEFTDOWN_LEFTUP\"");
}
void gui_require_input(State& s,const SettingsGui& gui){
    if(!gui_identity(s,gui,gui.tab)||GetForegroundWindow()!=gui.root||!gui_identity(s,gui,gui_focus(gui)))
        throw std::runtime_error("GUI_keyboard_target_no_longer_exact_product");
}
void gui_press(State& s,const SettingsGui& gui,WORD vk){gui_require_input(s,gui);key(s,vk,true);key(s,vk,false);}
void gui_page_select(State& s,const SettingsGui& gui,unsigned wanted){
    if(wanted>=6||gui_read(s,gui,gui.tab,TCM_GETITEMCOUNT)!=6)throw std::runtime_error("GUI_six_production_pages_required");
    bool changed=false;
    for(unsigned n=0;n<6&&gui_read(s,gui,gui.tab,TCM_GETCURSEL)!=wanted;++n){
        const auto before=gui_read(s,gui,gui.tab,TCM_GETCURSEL);
        gui_require_input(s,gui);key(s,VK_CONTROL,true);gui_press(s,gui,VK_TAB);key(s,VK_CONTROL,false);changed=true;
        if(!wait([&]{return gui_read(s,gui,gui.tab,TCM_GETCURSEL)==(before+1)%6;}))throw std::runtime_error("GUI_actual_CtrlTab_did_not_select_next_page");
    }
    s.log.check("GUI_page_"+std::to_string(wanted),gui_read(s,gui,gui.tab,TCM_GETCURSEL)==wanted&&gui_page(s,gui),
        "\"selection_via\":"+quote(changed?"actual_CtrlTab":"already_selected_actual_page")+",\"production_page\":"+std::to_string(wanted));
}
SettingsGui gui_open(State& s){
    s.log.check("GUI_production_settings_command",launch(s,L"--settings",true));SettingsGui gui;
    const bool found=wait([&]{
        struct Search {DWORD pid{};HWND root{};DWORD tid{};} search{s.product_pid};
        EnumWindows([](HWND h,LPARAM data)->BOOL{auto& query=*reinterpret_cast<Search*>(data);DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);
            if(pid==query.pid&&gui_class(h)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(h)){query.root=h;query.tid=tid;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));
        gui={search.root,search.root?GetDlgItem(search.root,100):nullptr,search.tid};return gui.root&&gui.tab&&
            gui_class(gui.tab)==WC_TABCONTROLW&&gui_identity(s,gui,gui.tab)&&GetForegroundWindow()==gui.root&&gui_identity(s,gui,gui_focus(gui));
    });
    DWORD foreground_pid{},root_pid{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground_pid);
    GetWindowThreadProcessId(gui.root,&root_pid);
    s.log.check("GUI_exact_product_settings_window",found,"\"pid\":"+std::to_string(s.product_pid)+
        ",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
        ",\"root_pid\":"+std::to_string(root_pid)+",\"root_class\":"+quote(utf8(gui_class(gui.root)))+
        ",\"tab\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.tab))+
        ",\"tab_class\":"+quote(utf8(gui_class(gui.tab)))+
        ",\"owner\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetWindow(gui.root,GW_OWNER)))+
        ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
        ",\"foreground_pid\":"+std::to_string(foreground_pid)+
        ",\"focus\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui_focus(gui)))+
        ",\"exact_identity\":"+(gui.root&&gui.tab&&gui_identity(s,gui,gui.tab)?"true":"false"));return gui;
}
void gui_edit_number(State& s,const SettingsGui& gui,HWND edit,unsigned number_value){
    gui_click(s,gui,edit);gui_require_input(s,gui);key(s,VK_CONTROL,true);gui_press(s,gui,'A');key(s,VK_CONTROL,false);
    const auto wanted=std::to_wstring(number_value);for(const auto digit:wanted)gui_press(s,gui,static_cast<WORD>(digit));
    s.log.check("GUI_actual_edit_keys",wait([&]{return gui_text(s,gui,edit)==wanted;}),"\"actual_text\":"+quote(utf8(gui_text(s,gui,edit))));
    const auto before=gui_focus(gui);gui_press(s,gui,VK_TAB);
    s.log.check("GUI_actual_Tab_focus",wait([&]{const auto after=gui_focus(gui);return after&&after!=before&&gui_identity(s,gui,after);}));
}
void gui_save_enter(State& s,const SettingsGui& gui){
    const auto save=GetDlgItem(gui.root,IDOK);
    for(unsigned n=0;n<64&&gui_focus(gui)!=save;++n){
        if(!gui_identity(s,gui,save))throw std::runtime_error("GUI_save_identity_changed");
        const auto before=gui_focus(gui);gui_press(s,gui,VK_TAB);
        if(!wait([&]{return gui_focus(gui)&&gui_focus(gui)!=before;}))throw std::runtime_error("GUI_save_Tab_not_processed");
    }
    s.log.check("GUI_Tab_reached_save",gui_focus(gui)==save);
    gui_press(s,gui,VK_RETURN);
    s.log.check("GUI_actual_Enter_saved_and_closed",wait([&]{return !IsWindow(gui.root);}),"\"input_kind\":\"actual_Enter_on_focused_IDOK\"");
}
void settings_gui_flow(State& s){
    namespace settings=panebind::core::v1;
    const auto path=s.product.parent_path()/L"settings.ini";const bool existed=fs::exists(path);const auto before=read(path);
    const auto parsed=existed?settings::parse(before):settings::ParseResult{settings::default_settings(),{}};
    if(!parsed.value)throw std::runtime_error("GUI_existing_configuration_invalid");const auto original=*parsed.value;
    auto gui=gui_open(s);
    // Visit all six real pages. Toggle reversible draft-only checkboxes on
    // general/magnet/docking/visual pages, focus the shortcut editor, and
    // toggle the rule's class filter without creating or excluding an app.
    constexpr std::array<const wchar_t*,6> labels{L"启用 PaneBind",L"移动时磁吸",L"启用边角停靠",L"键盘微移",L"仅限制为该窗口类名",L"显示实际吸附"};
    for(unsigned page=0;page<6;++page){gui_page_select(s,gui,page);
        if(page==3){const auto edit=gui_number(s,gui,labels[page]);gui_click(s,gui,edit);const auto old=gui_focus(gui);gui_press(s,gui,VK_TAB);
            s.log.check("GUI_shortcuts_actual_Tab",wait([&]{return gui_focus(gui)&&gui_focus(gui)!=old;}));}
        else{const auto check=gui_find(s,gui,L"Button",labels[page]);const auto old=gui_read(s,gui,check,BM_GETCHECK);
            gui_click(s,gui,check);s.log.check("GUI_page_checkbox_real_toggle",wait([&]{return gui_read(s,gui,check,BM_GETCHECK)!=old;}),"\"page\":"+std::to_string(page));
            gui_click(s,gui,check);s.log.check("GUI_page_checkbox_real_restore",wait([&]{return gui_read(s,gui,check,BM_GETCHECK)==old;}),"\"page\":"+std::to_string(page));}
    }
    gui_page_select(s,gui,1);gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),original.attraction_dip==7?8:7);
    gui_press(s,gui,VK_ESCAPE);
    s.log.check("GUI_actual_Esc_cancel_unchanged",wait([&]{return !IsWindow(gui.root);})&&fs::exists(path)==existed&&read(path)==before,
        "\"file_existed_before\":"+std::string(existed?"true":"false")+",\"config_bytes_unchanged\":true");
    const auto save_attraction=[&](unsigned value){auto current=gui_open(s);gui_page_select(s,current,1);
        gui_edit_number(s,current,gui_number(s,current,L"吸引距离"),value);gui_save_enter(s,current);
        const auto actual=settings::parse(read(path));if(!actual.value)throw std::runtime_error("GUI_saved_file_not_valid_configuration");
        auto expected=original;expected.attraction_dip=static_cast<int>(value);expected.generation=actual.value->generation;
        s.log.check("GUI_save_actual_configuration",*actual.value==expected&&actual.value->generation>original.generation,
            "\"attraction_dip\":"+std::to_string(actual.value->attraction_dip)+",\"generation\":"+std::to_string(actual.value->generation));return *actual.value;};
    const auto saved=save_attraction(0);
    const auto no_snap=[&](std::string_view stage){
        const std::array<Rect,3> independent{{{120,80,380,440},{380,80,640,260},{380,260,640,440}}};
        for(std::size_t n=0;n<3;++n)s.log.check("GUI_effect_fixture_setup",own(s,s.frames[n].hwnd)&&fixture_rect(s.frames[n].hwnd,independent[n]),
            "\"stage\":"+quote(stage)+",\"not_a_replacement_for_prior_I02\":true");
        const auto g=down(s,0);const auto expected=translated(independent[0],-6,3);
        phase(s,0,g,-6,3,expected,"GUI_attraction_zero_"+std::string(stage),true);up(s,0,g,expected);
    };
    no_snap("next_gesture");
    cleanup(s);if(s.log.failed)throw std::runtime_error("GUI_restart_normal_stop_failed");const auto restart_mark=read(s.product_log).size();
    s.log.check("GUI_restart_same_product",launch(s,L"--diagnostic",false));
    s.log.check("GUI_restart_new_startup",wait([&]{const auto row=record(s,restart_mark,"startup");return number(row,"\"process_id\":")==s.product_pid;}));
    const auto retained=settings::parse(read(path));s.log.check("GUI_restart_persisted_configuration",retained.value&&*retained.value==saved);
    gui=gui_open(s);gui_page_select(s,gui,1);
    s.log.check("GUI_restart_actual_edit_value",gui_text(s,gui,gui_number(s,gui,L"吸引距离"))==L"0");gui_press(s,gui,VK_ESCAPE);
    s.log.check("GUI_restart_Esc_closed",wait([&]{return !IsWindow(gui.root);}));no_snap("after_restart");
    // Restore via the SAME real GUI so the following Explorer 10/16 trajectory
    // keeps its original preconditions. No direct configuration file mutation.
    const auto restored=save_attraction(static_cast<unsigned>(original.attraction_dip));
    s.log.check("GUI_original_settings_restored_before_Explorer",restored.attraction_dip==original.attraction_dip&&restored.generation>saved.generation);
    s.log.emit("settings_GUI_sequence","\"six_pages\":true,\"actual_mouse_and_Tab_Enter_Esc\":true,\"cancel_unchanged\":true,\"save_changes_next_Move\":true,\"restart_persists_and_changes_Move\":true,\"original_attraction_restored\":true,\"human_uat\":\"NOT_RUN\"");
}
// Only invoked by the parent-selected L2 sequence after earlier evidence is
// reviewed. These are actual test-owned window attributes, never core verdicts.
struct NativeVisual {
    bool layered{},topmost{},known{true};BYTE alpha{255};COLORREF color{};DWORD flags{};
    bool operator==(const NativeVisual&)const=default;
};
std::optional<NativeVisual> native_visual(HWND h){
    SetLastError(0);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!ex&&GetLastError())return {};
    NativeVisual value;value.layered=(ex&WS_EX_LAYERED)!=0;value.topmost=(ex&WS_EX_TOPMOST)!=0;
    if(value.layered)value.known=GetLayeredWindowAttributes(h,&value.color,&value.alpha,&value.flags)!=FALSE;
    return value;
}
bool native_visual_is(HWND h,BYTE alpha,bool top){
    const auto now=native_visual(h);return now&&now->layered&&now->known&&now->alpha==alpha&&now->color==0&&now->flags==LWA_ALPHA&&now->topmost==top;
}
void own_visual_setup(State& s,HWND h,BYTE alpha,bool top){
    if(!own(s,h))throw std::runtime_error("visual_setup_not_exact_owned_fixture");
    SetLastError(0);const auto ex=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!ex&&GetLastError())throw std::runtime_error("visual_setup_style_read_failed");
    SetLastError(0);const auto previous=SetWindowLongPtrW(h,GWL_EXSTYLE,ex|WS_EX_LAYERED);
    if((!previous&&GetLastError())||!SetLayeredWindowAttributes(h,0,alpha,LWA_ALPHA)||
        !SetWindowPos(h,top?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("visual_setup_native_failed");
    s.log.check("visual_fixture_app_setup",wait([&]{return native_visual_is(h,alpha,top);}),
        "\"actor\":\"fixture_app_not_product\",\"alpha\":"+std::to_string(alpha)+",\"topmost\":"+(top?"true":"false"));
}
void visual_exact(State& s,HWND h,BYTE alpha,bool top,std::string name){
    s.log.check(std::move(name),wait([&]{return own(s,h)&&native_visual_is(h,alpha,top);}),
        "\"actual_native_layered_alpha\":"+std::to_string(alpha)+",\"actual_topmost\":"+(top?"true":"false")+
        ",\"readback\":\"GetLayeredWindowAttributes_and_GWL_EXSTYLE\"");
}
void visual_command(State& s,HWND h,std::wstring_view name,BYTE alpha,bool top){
    foreground(s,h);const auto point=hit_point(h,HTCAPTION);if(!point)throw std::runtime_error("visual_command_caption_missing");
    cursor(s,*point);Sleep(100);s.log.check("visual_production_command",launch(s,L"--command "+std::wstring(name),true),"\"action\":"+quote(utf8(std::wstring(name))));
    visual_exact(s,h,alpha,top,"visual_command_actual_"+utf8(std::wstring(name)));
}
void visual_toggle_hotkey(State& s,HWND h,BYTE alpha,bool top){
    foreground(s,h);
    try{key(s,VK_CONTROL,true);key(s,VK_MENU,true);key(s,'T',true);key(s,'T',false);key(s,VK_MENU,false);key(s,VK_CONTROL,false);}
    catch(...){try{key(s,'T',false);key(s,VK_MENU,false);key(s,VK_CONTROL,false);}catch(...){}throw;}
    visual_exact(s,h,alpha,top,"visual_actual_registered_CtrlAltT");
}
panebind::core::v1::Settings visual_gui_save(State& s,const panebind::core::v1::Settings& desired){
    const auto gui=gui_open(s);gui_page_select(s,gui,1);
    gui_edit_number(s,gui,gui_number(s,gui,L"吸引距离"),static_cast<unsigned>(desired.attraction_dip));
    gui_page_select(s,gui,5);
    for(const auto& item:std::array<std::pair<const wchar_t*,bool>,2>{{{L"拖动期间临时透明",desired.drag_transparency},{L"Move 中摇动",desired.shake}}}){
        const auto control=gui_find(s,gui,L"Button",item.first);
        if((gui_read(s,gui,control,BM_GETCHECK)==BST_CHECKED)!=item.second)gui_click(s,gui,control);
        // Focus can arrive on DOWN before the actual queued UP toggles the
        // checkbox. Observe its real state, not SendInput/focus completion.
        s.log.check("visual_GUI_real_checkbox",wait([&]{return (gui_read(s,gui,control,BM_GETCHECK)==BST_CHECKED)==item.second;}),
            "\"label\":"+quote(utf8(item.first))+",\"wanted\":"+(item.second?"true":"false"));
    }
    const std::array<std::pair<const wchar_t*,int>,5> numbers{{
        {L"拖动不透明度",desired.drag_alpha},{L"摇动不透明度",desired.shake_alpha},
        {L"摇动幅度",desired.shake_amplitude_dip},{L"摇动反向次数",desired.shake_reversals},{L"摇动时间窗口",desired.shake_window_ms}}};
    for(const auto& item:numbers)gui_edit_number(s,gui,gui_number(s,gui,item.first),static_cast<unsigned>(item.second));
    gui_save_enter(s,gui);
    const auto observed=panebind::core::v1::parse(read(s.product.parent_path()/L"settings.ini"));
    auto expected=desired;if(observed.value)expected.generation=observed.value->generation;
    s.log.check("visual_GUI_saved_parameters",observed.value&&*observed.value==expected);
    return *observed.value;
}
[[maybe_unused]] void visual_mapping_flow(State& s){
    namespace settings=panebind::core::v1;
    const HWND h=s.frames[0].hwnd;
    if(!own(s,h)||source_window(s,0)!=h)throw std::runtime_error("visual_flow_exact_owned_mode_required");
    const auto original_visual=native_visual(h);if(!original_visual||!original_visual->known)throw std::runtime_error("visual_original_tuple_unknown");
    const auto parsed=settings::parse(read(s.product.parent_path()/L"settings.ini"));
    if(!parsed.value)throw std::runtime_error("visual_original_configuration_missing");
    const auto original=*parsed.value;
    if(std::none_of(original.hotkeys.begin(),original.hotkeys.end(),[](const auto& key){return key.modifiers==3&&key.key=='T'&&key.action==settings::Action::ToggleTopmost;}))
        throw std::runtime_error("visual_default_CtrlAltT_mapping_precondition_missing");
    std::array<Rect,3> old_rects{};std::array<bool,3> shown{};
    for(std::size_t n=0;n<3;++n){const auto r=visible(s.frames[n].hwnd);if(!own(s,s.frames[n].hwnd)||!r)throw std::runtime_error("visual_fixture_original_missing");
        old_rects[n]=*r;shown[n]=IsWindowVisible(s.frames[n].hwnd)!=FALSE;if(n)ShowWindow(s.frames[n].hwnd,SW_HIDE);}
    MONITORINFO monitor{sizeof(monitor)};
    if(!GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONULL),&monitor))throw std::runtime_error("visual_work_area_unknown");
    const Rect base{monitor.rcWork.left+200,monitor.rcWork.top+160,monitor.rcWork.left+600,monitor.rcWork.top+460};
    s.log.check("visual_independent_fixture_layout",fixture_rect(h,base),"\"not_substitute_for_I02\":true");
    auto desired=original;desired.attraction_dip=0;desired.drag_transparency=false;desired.shake=false;
    own_visual_setup(s,h,173,false);visual_gui_save(s,desired);own_visual_setup(s,h,173,true);
    visual_command(s,h,L"OpacityDown",141,true);visual_toggle_hotkey(s,h,141,false);
    visual_command(s,h,L"OpacityDown",109,false);visual_command(s,h,L"OpacityUp",141,false);
    const auto resume=[&]{const auto mark=read(s.product_log).size();s.log.check("visual_resume_command",launch(s,L"--command Pause",true));
        s.log.check("visual_resume_actual_enabled",wait([&]{return record(s,mark,"status").find("\"enabled\":true")!=std::string::npos;}));};
    s.log.check("visual_pause_command",launch(s,L"--pause",true));visual_exact(s,h,173,true,"visual_pause_restores_actual_original_tuple");resume();
    visual_command(s,h,L"OpacityDown",141,true);visual_toggle_hotkey(s,h,141,false);
    const auto stop_mark=read(s.product_log).size(),driver_stop_mark=read(s.log.path).size();
    const auto stopped_pid=s.product_pid;cleanup(s);
    // Earlier independent application failures remain in the global verdict,
    // but do not turn this fresh, actually observed stop into a false failure.
    // Conversely, a forced termination or an older instance's shutdown cannot
    // satisfy this visual-restoration check.
    std::string stop_receipt;const auto driver_text=read(s.log.path);
    if(driver_stop_mark<driver_text.size()){
        std::istringstream rows(driver_text.substr(driver_stop_mark));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"type\":\"product_stop\"")!=std::string::npos)stop_receipt=row;
    }
    const auto stopped=record(s,stop_mark,"shutdown");
    s.log.check("visual_normal_exit_actual_receipts",!s.process&&
        stop_receipt.find("\"quit_command_completed\":true")!=std::string::npos&&
        stop_receipt.find("\"process_exit_observed\":true")!=std::string::npos&&
        stop_receipt.find("\"product_exit_code\":0,")!=std::string::npos&&
        number(stopped,"\"process_id\":")==stopped_pid&&
        stopped.find("\"own_hooks_stopped\":true")!=std::string::npos&&
        stopped.find("\"own_overlay_destroyed\":true")!=std::string::npos&&
        stopped.find("\"native_worker_exited\":true")!=std::string::npos&&
        stopped.find("\"issued_native_completion_unknown\":false")!=std::string::npos&&
        stopped.find("\"visual_recovery_stopped\":true")!=std::string::npos&&
        stopped.find("\"visual_restore_confirmed\":true")!=std::string::npos,
        "\"product_stop\":"+quote(stop_receipt)+",\"shutdown\":"+quote(stopped)+",\"previous_global_failures_preserved\":true");
    visual_exact(s,h,173,true,"visual_normal_exit_restores_actual_original_tuple");
    s.log.check("visual_exit_helper_confirmed",record(s,stop_mark,"shutdown").find("\"visual_recovery_stopped\":true")!=std::string::npos);
    const auto restart=read(s.product_log).size();s.log.check("visual_restart_same_product",launch(s,L"--diagnostic",false));
    s.log.check("visual_restart_actual_startup",wait([&]{return number(record(s,restart,"startup"),"\"process_id\":")==s.product_pid;}));
    visual_command(s,h,L"OpacityDown",141,true);
    s.log.check("visual_app_changes_alpha_199",SetLayeredWindowAttributes(h,0,199,LWA_ALPHA)!=FALSE);visual_exact(s,h,199,true,"visual_app_actual_alpha_199");
    s.log.check("visual_pause_after_app_change",launch(s,L"--pause",true));visual_exact(s,h,199,true,"visual_cleanup_does_not_overwrite_app_alpha_199");resume();
    own_visual_setup(s,h,173,false);
    desired.drag_transparency=true;desired.shake=false;desired.drag_alpha=190;desired.shake_alpha=190;
    desired.shake_amplitude_dip=24;desired.shake_reversals=3;desired.shake_window_ms=2000;
    visual_gui_save(s,desired);own_visual_setup(s,h,173,true);
    auto start=*visible(h);auto g=down(s,0);visual_exact(s,h,190,true,"visual_drag_actual_alpha_while_pressed");
    phase(s,0,g,-40,35,translated(start,-40,35),"visual_drag_actual_Move",true);up(s,0,g,translated(start,-40,35));
    visual_exact(s,h,173,true,"visual_NormalUp_restores_original_alpha");
    start=*visible(h);g=down(s,0);visual_exact(s,h,190,true,"visual_abort_drag_actual_alpha_while_pressed");
    phase(s,0,g,40,35,translated(start,40,35),"visual_abort_actual_Move",true);
    key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);
    s.log.check("visual_actual_Escape_end",wait([&]{const auto end=record(s,g.offset,"gesture_end",g.generation);return !end.empty()&&number(end,"\"end\":")==1&&end.find("\"normal_up\":false")!=std::string::npos;}));
    s.log.check("visual_Escape_original_geometry",wait([&]{return visible(h)==start;}));visual_exact(s,h,173,true,"visual_Escape_restores_original_alpha");
    button(s,false);s.log.check("visual_abort_matching_real_UP",wait([&]{return !actual_up(s,g.offset,g.generation).empty();}),
        "\"actual_product_UP\":"+quote(actual_up(s,g.offset,g.generation))+
        ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON)));
    own_visual_setup(s,h,173,false);desired.drag_transparency=false;desired.shake=true;visual_gui_save(s,desired);
    const UINT dpi=GetDpiForWindow(h);if(!dpi)throw std::runtime_error("visual_shake_DPI_unknown");
    const LONG step=std::max<LONG>(48,MulDiv(24,static_cast<int>(dpi),96)*2);
    const auto count_mutations=[&](std::size_t offset){std::istringstream rows(read(s.product_log).substr(offset));std::string row;unsigned count{};
        while(std::getline(rows,row))if(row.find("\"type\":\"visual_mutation\"")!=std::string::npos)++count;return count;};
    for(unsigned gesture=0;gesture<2;++gesture){
        start=*visible(h);g=down(s,0);LONG dx{};
        for(unsigned point=0;point<5;++point){dx=point%2?-step:step;phase(s,0,g,dx,0,translated(start,dx,0),"visual_real_shake_"+std::to_string(gesture)+"_"+std::to_string(point),true);}
        const BYTE alpha=gesture?173:190;const bool top=gesture==0;
        visual_exact(s,h,alpha,top,gesture?"visual_second_shake_restores_tuple":"visual_first_shake_sets_tuple");
        s.log.check("visual_shake_actual_mutation_receipts",wait([&]{return count_mutations(g.offset)>=2;}));
        const auto after_trigger=read(s.product_log).size();
        for(unsigned point=0;point<4;++point){dx=point%2?step:-step;phase(s,0,g,dx,0,translated(start,dx,0),"visual_same_gesture_extra_reverse",true);}
        up(s,0,g,translated(start,dx,0));visual_exact(s,h,alpha,top,"visual_shake_tuple_after_real_UP");
        s.log.check("visual_at_most_one_toggle_per_real_gesture",count_mutations(after_trigger)==0);
    }
    // Current tuple is 173/not-topmost and no permanent shake ownership remains.
    visual_gui_save(s,original);
    s.log.check("visual_original_configuration_restored",!settings::parse(read(s.product.parent_path()/L"settings.ini")).error.size());
    SetLastError(0);const auto current=GetWindowLongPtrW(h,GWL_EXSTYLE);if(!current&&GetLastError())throw std::runtime_error("visual_final_style_unknown");
    SetLastError(0);const auto previous=SetWindowLongPtrW(h,GWL_EXSTYLE,original_visual->layered?current|WS_EX_LAYERED:current&~WS_EX_LAYERED);
    if((!previous&&GetLastError())||(original_visual->layered&&!SetLayeredWindowAttributes(h,original_visual->color,original_visual->alpha,original_visual->flags))||
        !SetWindowPos(h,original_visual->topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
        throw std::runtime_error("visual_fixture_original_restore_failed");
    s.log.check("visual_fixture_original_tuple_restored",wait([&]{const auto actual=native_visual(h);return actual&&*actual==*original_visual;}));
    for(std::size_t n=0;n<3;++n){s.log.check("visual_fixture_layout_restored",fixture_rect(s.frames[n].hwnd,old_rects[n]));if(shown[n])ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);}
    s.log.emit("visual_mapping_sequence","\"real_property_readbacks\":true,\"actual_registered_hotkey\":true,\"real_GUI_configuration\":true,\"NormalUp_and_Escape\":true,\"two_real_shake_gestures\":true,\"human_uat\":\"NOT_RUN\"");
}
#include "platform/windows/product/v1_guest_settings_capture.h"
#include "platform/windows/product/v1_guest_layout_cases.h"
#include "platform/windows/product/v1_guest_settings_cases.h"
#include "platform/windows/product/v1_guest_product_chains.h"
#include "platform/windows/product/v1_guest_interruptions.h"
#include "platform/windows/product/v1_guest_constraint_cases.h"
#include "platform/windows/product/v1_guest_batch_failure_case.h"
#include "platform/windows/product/v1_guest_portable_cases.h"
void record_guest_environment(State& s){
    std::array<wchar_t,64> build{};DWORD size=static_cast<DWORD>(build.size()*sizeof(wchar_t)),ubr{},ubr_size=sizeof(ubr);
    const bool build_known=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"CurrentBuildNumber",RRF_RT_REG_SZ,nullptr,build.data(),&size)==ERROR_SUCCESS;
    const bool ubr_known=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"UBR",RRF_RT_REG_DWORD,nullptr,&ubr,&ubr_size)==ERROR_SUCCESS;
    std::array<wchar_t,KL_NAMELENGTH> keyboard{};const bool keyboard_known=GetKeyboardLayoutNameW(keyboard.data())!=FALSE;
    struct Monitors {std::vector<std::string> rows;bool complete{true};} monitors;
    const bool enumerated=EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR h,HDC,LPRECT,LPARAM data)->BOOL{
        auto& out=*reinterpret_cast<Monitors*>(data);MONITORINFO info{sizeof(info)};
        if(out.rows.size()>=16||!GetMonitorInfoW(h,&info)){out.complete=false;return FALSE;}
        out.rows.push_back("{\"bounds\":"+rect({info.rcMonitor.left,info.rcMonitor.top,info.rcMonitor.right,info.rcMonitor.bottom})+
            ",\"work\":"+rect({info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom})+
            ",\"primary\":"+((info.dwFlags&MONITORINFOF_PRIMARY)?"true":"false")+"}");return TRUE;
    },reinterpret_cast<LPARAM>(&monitors))!=FALSE;
    std::string rows="[";for(const auto& row:monitors.rows){if(rows.size()>1)rows+=',';rows+=row;}rows+=']';
    s.log.emit("actual_guest_environment","\"os_build\":"+(build_known?quote(utf8(build.data())):"null")+
        ",\"ubr\":"+(ubr_known?std::to_string(ubr):"null")+",\"system_dpi\":"+std::to_string(GetDpiForSystem())+
        ",\"source_window_dpi\":"+std::to_string(GetDpiForWindow(s.frames[0].hwnd))+
        ",\"keyboard_layout\":"+(keyboard_known?quote(utf8(keyboard.data())):"null")+
        ",\"monitors_complete\":"+(enumerated&&monitors.complete?"true":"false")+",\"monitors\":"+rows+
        ",\"synthetic_display_configuration_used\":false");
}
void drive(State& s){
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try{
        if(FAILED(apartment))throw std::runtime_error("fixture_driver_COM_STA_initialization_failed");
        record_guest_environment(s);
        s.log.check("guest_input_baseline_clear",!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&
            !(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&!(GetAsyncKeyState(VK_LWIN)&0x8000)&&!(GetAsyncKeyState(VK_RWIN)&0x8000));
        const std::array<Rect,3> layout{{{120,80,380,440},{380,80,640,260},{380,260,640,440}}};
        for(std::size_t n=0;n<3;++n)s.log.check("initial_owned_visible_layout",fixture_rect(s.frames[n].hwnd,layout[n]),
            "\"member\":"+std::to_string(n)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(s.frames[n].hwnd))+
            ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(s.owner)+",\"expected\":"+rect(layout[n]));
        s.log.check("production_create_no_window",launch(s,L"--diagnostic",false),
            "\"creation_flags\":134217728,\"actual_product\":"+quote(utf8(s.product.wstring())));
        s.log.check("production_gui_startup",wait([&]{return !record(s,0,"startup").empty();},8000));
        const HWND app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD pid{};if(app)GetWindowThreadProcessId(app,&pid);
        s.log.check("single_exact_product_owner",app&&pid==s.product_pid);
        const auto mark=read(s.product_log).size();DWORD second{};
        const bool second_ok=launch(s,L"--status",true,&second)&&
            FindWindowW(L"PaneBind.V1.Application",L"PaneBind")==app&&wait([&]{return !record(s,mark,"status").empty();});
        s.log.check("second_instance_status",second_ok,
            "\"secondary_exit\":"+std::to_string(second));
        // Preserve the complete historical action order. Initial layout is
        // fixture setup only; no geometry reset occurs between these gestures.
        five(s,0,"A_initial");
        ctrl_move(s,0,34,0,"Ctrl_A");
        five(s,1,"B");
        five(s,2,"C");
        const auto before=visible(s.frames[1].hwnd);if(!before)throw std::runtime_error("B_resize_before_missing");
        const auto gesture=down(s,1,false,HTBOTTOM);Rect after=*before;after.b+=28;
        phase(s,1,gesture,0,28,after,"B_real_frame_resize",true);up(s,1,gesture,after);
        s.log.emit("resize_actual","\"before\":"+rect(*before)+",\"after\":"+rect(after)+
            ",\"source_native_modal_starts\":"+std::to_string(s.frames[1].starts.load())+
            ",\"note\":\"real_frame_input; V1 may enhance resize without a native modal loop\"");
        five(s,0,"A_post_resize");
        ctrl_move(s,2,22,18,"Ctrl_C");
        // Ordinary second-instance commands use the same production handler,
        // not a direct test writer or a window message invented as a result.
        foreground(s,s.frames[0].hwnd);const auto hit=hit_point(s.frames[0].hwnd,HTCAPTION);
        if(!hit)throw std::runtime_error("command_target_titlebar_unavailable");cursor(s,*hit);Sleep(100);
        const auto initial=visible(s.frames[0].hwnd);MONITORINFO monitor{sizeof(monitor)};
        if(!initial||!GetMonitorInfoW(MonitorFromWindow(s.frames[0].hwnd,MONITOR_DEFAULTTONULL),&monitor))
            throw std::runtime_error("command_monitor_unavailable");
        Rect center{monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-initial->width())/2,
            monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-initial->height())/2,0,0};
        center.r=center.l+initial->width();center.b=center.t+initial->height();
        const bool center_ok=launch(s,L"--command Center",true)&&wait([&]{return visible(s.frames[0].hwnd)==center;});
        s.log.check("production_center_command",center_ok,
            "\"expected\":"+rect(center)+",\"actual\":"+(visible(s.frames[0].hwnd)?rect(*visible(s.frames[0].hwnd)):"null"));
        const LONG split=monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left)/2;
        const Rect half{monitor.rcWork.left,monitor.rcWork.top,split,monitor.rcWork.bottom};
        const bool half_ok=launch(s,L"--command DockLeft",true)&&wait([&]{return visible(s.frames[0].hwnd)==half;});
        s.log.check("production_half_screen_command",half_ok,
            "\"expected\":"+rect(half)+",\"actual\":"+(visible(s.frames[0].hwnd)?rect(*visible(s.frames[0].hwnd)):"null"));
        s.log.emit("owned_core_sequence","\"I02\":\"PASS\",\"commands\":\"PASS\",\"next\":\"real_Medium_Explorer_new_root_adapter\"");
        owned_settings_screenshots(s);
        settings_gui_flow(s);
        // Complete independent owned DEV before retrying application-specific
        // environment gaps. A failed application admission is not permission
        // to bypass it, nor a reason to leave unrelated product paths untested.
        owned_layout_cases(s);
        owned_native_grid_constraint_case(s);
        owned_second_member_native_failure_case(s);
        owned_temporary_visual_inflight_case(s);
        owned_settings_lifecycle_cases(s);
        owned_dock_stretch_chains(s);
        owned_interruption_cases(s);
        if(s.configuration=="Release"){
            owned_finite_I11_chain(s);
            const auto idle=product::guest_test::measure_idle_60s(s.process,s.product_pid,process_birth(s.process));
            s.log.emit("Release_actual_idle_measurement",idle.json_fields());
            if(!idle.completed||idle.one_core_cpu_percent>=1.0||idle.after.working_bytes>80ULL*1024*1024)s.log.failed=true;
        }
        visual_mapping_flow(s);
        try{explorer_flow(s);}
        catch(const std::exception& error){s.log.failed=true;s.log.emit("Explorer_core_result","\"result\":\"BLOCKED_OR_FAIL\",\"reason\":"+quote(error.what())+",\"other_application_classes_continue_after_exact_cleanup\":true");cleanup_explorers(s);}
        application_matrix_flow(s);
        cleanup_explorers(s);
        if(s.configuration=="Release")owned_portable_zip_entry_case(s);
        cleanup(s);
        if(!s.log.failed)s.log.emit("functional_summary","\"owned_I02\":\"PASS\",\"production_commands\":\"PASS\",\"normal_stop_and_client_delivery\":\"PASS\",\"Explorer_I02\":\"PASS\",\"human_uat\":\"NOT_RUN\"");
        s.result=s.log.failed?1:0;
    }catch(const std::exception& error){
        try{s.log.emit("failure","\"reason\":"+quote(error.what()));cleanup(s);cleanup_applications(s);cleanup_explorers(s);}catch(...){}s.result=1;
    }
    if(SUCCEEDED(apartment))CoUninitialize();
    PostThreadMessageW(s.owner,WM_QUIT,0,0);
}
int guard_reject(State& s,int code,std::string_view reason){
    if(!s.log.path.empty())s.log.emit("guard_rejected","\"exit_code\":"+std::to_string(code)+",\"reason\":"+quote(reason)+",\"input_sent\":false");
    return code;
}
bool guard_desktop(State& s){
    const HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);const DWORD open_error=input?0:GetLastError();
    const HDESK owner=GetThreadDesktop(GetCurrentThreadId());const HWINSTA station=GetProcessWindowStation();
    std::array<wchar_t,128> input_name{},owner_name{},station_name{};DWORD needed{};BOOL input_io{},owner_io{};
    const bool input_named=input&&GetUserObjectInformationW(input,UOI_NAME,input_name.data(),static_cast<DWORD>(sizeof(input_name)),&needed);
    const DWORD input_name_error=input_named?0:input?GetLastError():open_error;
    const bool owner_named=owner&&GetUserObjectInformationW(owner,UOI_NAME,owner_name.data(),static_cast<DWORD>(sizeof(owner_name)),&needed);
    const DWORD owner_name_error=owner_named?0:GetLastError();
    const bool station_named=station&&GetUserObjectInformationW(station,UOI_NAME,station_name.data(),static_cast<DWORD>(sizeof(station_name)),&needed);
    const DWORD station_error=station_named?0:GetLastError();
    const bool input_queried=input&&GetUserObjectInformationW(input,UOI_IO,&input_io,sizeof(input_io),&needed);
    const DWORD input_io_error=input_queried?0:input?GetLastError():open_error;
    const bool owner_queried=owner&&GetUserObjectInformationW(owner,UOI_IO,&owner_io,sizeof(owner_io),&needed);
    const DWORD owner_io_error=owner_queried?0:GetLastError();
    const bool closed=!input||CloseDesktop(input)!=FALSE;const DWORD close_error=closed?0:GetLastError();
    s.log.emit("guard_desktop","\"open_input_succeeded\":"+std::string(input?"true":"false")+",\"open_input_error\":"+std::to_string(open_error)+
        ",\"input_name\":"+quote(utf8(input_name.data()))+",\"input_name_error\":"+std::to_string(input_name_error)+
        ",\"owner_name\":"+quote(utf8(owner_name.data()))+",\"owner_name_error\":"+std::to_string(owner_name_error)+
        ",\"window_station\":"+quote(utf8(station_name.data()))+",\"station_name_error\":"+std::to_string(station_error)+
        ",\"input_uoi_io\":"+(input_queried?(input_io?"true":"false"):"null")+",\"input_uoi_error\":"+std::to_string(input_io_error)+
        ",\"owner_uoi_io\":"+(owner_queried?(owner_io?"true":"false"):"null")+",\"owner_uoi_error\":"+std::to_string(owner_io_error)+
        ",\"close_input_succeeded\":"+(closed?"true":"false")+",\"close_input_error\":"+std::to_string(close_error));
    return input&&input_named&&owner_named&&station_named&&input_queried&&owner_queried&&input_io&&owner_io&&closed&&
        equal(input_name.data(),L"Default")&&equal(owner_name.data(),L"Default")&&equal(station_name.data(),L"WinSta0");
}
int guard(State& s){
    // Before the exact guest + lexical scope + markers, never write evidence
    // or resolve arbitrary caller-supplied filesystem paths.
    if(s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');}))return 78;
    std::array<wchar_t,128> username{};DWORD length=static_cast<DWORD>(username.size()),session{};
    if(!GetUserNameW(username.data(),&length)||!equal(username.data(),L"WDAGUtilityAccount"))return 78;
    const fs::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";
    if(!product::guest::same_exact_directory(s.output.lexically_normal(),output))return 82;
    const auto requested=s.input_product.lexically_normal();
    const fs::path debug_dir=output/(s.run+L"-product-Debug"),release_dir=output/(s.run+L"-product-Release");
    const bool debug=equal(requested.wstring(),(debug_dir/L"panebind.exe").wstring());
    if(!requested.is_absolute()||(!debug&&!equal(requested.wstring(),(release_dir/L"panebind.exe").wstring())))return 83;
    if(!marker(input/L"panebind-v1-input.marker",s.run))return 80;
    if(!marker(output/L"panebind-v1-output.marker",s.run))return 81;
    const DWORD output_attrs=GetFileAttributesW(output.c_str());const DWORD output_error=output_attrs==INVALID_FILE_ATTRIBUTES?GetLastError():0;
    if(output_attrs==INVALID_FILE_ATTRIBUTES||!(output_attrs&FILE_ATTRIBUTE_DIRECTORY)||(output_attrs&FILE_ATTRIBUTE_REPARSE_POINT))return 84;
    s.configuration=debug?"Debug":"Release";const std::wstring suffix=debug?L"debug":L"release";
    s.log.path=output/(s.run+L"-"+suffix+L"-fixture.jsonl");
    {Handle file(CreateFileW(s.log.path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(file.value==INVALID_HANDLE_VALUE){s.log.path.clear();return 85;}}
    s.log.emit("guard_scope","\"run_id\":"+quote(utf8(s.run))+",\"configuration\":"+quote(s.configuration)+
        ",\"exact_guest_account\":true,\"exact_run_markers\":true,\"requested_output\":"+quote(utf8(s.output.wstring()))+
        ",\"requested_product\":"+quote(utf8(requested.wstring()))+",\"output_attributes\":"+std::to_string(output_attrs)+",\"output_attributes_error\":"+std::to_string(output_error)+
        ",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(GetCurrentThreadId())+",\"input_sent\":false");
    const DWORD input_attrs=GetFileAttributesW(input.c_str());const DWORD input_error=input_attrs==INVALID_FILE_ATTRIBUTES?GetLastError():0;
    s.log.emit("guard_input_directory","\"path\":"+quote(utf8(input.wstring()))+",\"attributes\":"+std::to_string(input_attrs)+",\"win32_error\":"+std::to_string(input_error));
    if(input_attrs==INVALID_FILE_ATTRIBUTES||!(input_attrs&FILE_ATTRIBUTE_DIRECTORY)||(input_attrs&FILE_ATTRIBUTE_REPARSE_POINT))return guard_reject(s,86,"input_directory_not_plain");
    int rid=-1;bool elevated{};DWORD token_error{};const bool normal=medium(GetCurrentProcess(),&rid,&elevated,&token_error);
    s.log.emit("guard_token","\"integrity_rid\":"+(rid<0?std::string("null"):std::to_string(rid))+",\"elevated\":"+(rid<0?"null":(elevated?"true":"false"))+
        ",\"medium_non_elevated\":"+(normal?"true":"false")+",\"win32_error\":"+std::to_string(token_error));
    if(!normal)return guard_reject(s,87,"own_token_not_verified_Medium_non_elevated");
    const bool session_read=ProcessIdToSessionId(GetCurrentProcessId(),&session)!=FALSE;const DWORD session_error=session_read?0:GetLastError();
    s.log.emit("guard_session","\"query_succeeded\":"+std::string(session_read?"true":"false")+",\"session_id\":"+std::to_string(session)+",\"win32_error\":"+std::to_string(session_error));
    if(!session_read||!session)return guard_reject(s,88,"own_interactive_session_unavailable");
    if(!guard_desktop(s))return guard_reject(s,89,"own_Default_input_desktop_not_verified");
    LPWSTR value{};DWORD bytes{};const bool queried=WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSConnectState,&value,&bytes)!=FALSE;
    const DWORD wts_error=queried?0:GetLastError();const int state=queried&&bytes>=sizeof(WTS_CONNECTSTATE_CLASS)?*reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(value):-1;
    if(value)WTSFreeMemory(value);
    s.log.emit("guard_WTS","\"query_succeeded\":"+std::string(queried?"true":"false")+",\"bytes\":"+std::to_string(bytes)+",\"state\":"+std::to_string(state)+",\"win32_error\":"+std::to_string(wts_error));
    if(state!=WTSActive)return guard_reject(s,90,"own_WTS_session_not_active");
    std::error_code error;const auto actual_output=fs::weakly_canonical(output,error);
    s.log.emit("guard_output_canonical","\"path\":"+quote(utf8(actual_output.wstring()))+",\"error_code\":"+std::to_string(error.value()));
    if(error||!product::guest::same_exact_directory(actual_output,output))return guard_reject(s,91,"output_canonical_path_not_exact");
    if(!plain_directory(requested.parent_path()))return guard_reject(s,92,"product_directory_not_plain");
    if(debug&&!plain_directory(input/L"Debug"))return guard_reject(s,93,"Debug_input_directory_not_plain");
    const fs::path source=debug?input/L"Debug"/L"panebind.exe":input/L"panebind.exe";
    for(const auto& path:std::array{requested,source}){
        const DWORD attrs=GetFileAttributesW(path.c_str());const DWORD e=attrs==INVALID_FILE_ATTRIBUTES?GetLastError():0;
        s.log.emit("guard_executable_attributes","\"path\":"+quote(utf8(path.wstring()))+",\"attributes\":"+std::to_string(attrs)+",\"win32_error\":"+std::to_string(e));
        if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
            return guard_reject(s,path==requested?94:95,"executable_not_plain_regular_file");
    }
    const auto actual=fs::canonical(requested,error);
    s.log.emit("guard_product_canonical","\"path\":"+quote(utf8(actual.wstring()))+",\"error_code\":"+std::to_string(error.value()));
    if(error||!equal(actual.wstring(),requested.wstring()))return guard_reject(s,96,"product_canonical_path_not_exact");
    const auto source_hash=hash(source),actual_hash=hash(actual);
    s.log.emit("guard_executable_hashes","\"input_sha256\":"+quote(source_hash)+",\"actual_sha256\":"+quote(actual_hash));
    if(source_hash.empty()||source_hash!=actual_hash)return guard_reject(s,97,"product_source_hash_mismatch_or_unreadable");
    if(!read(actual.parent_path()/L"portable.flag",128).starts_with("PaneBind V1 portable test candidate"))return guard_reject(s,98,"portable_run_marker_missing");
    if(FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))return guard_reject(s,99,"preexisting_product_instance_present");
    s.input_product=source;s.product=actual;s.output=output;return 0;
}
#include "platform/windows/product/v1_guest_input_actor.h"
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    State s;s.instance=instance;s.owner=GetCurrentThreadId();int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 64;
    if(count>=2&&std::wstring_view(args[1])==L"--input-actor-once"){
        int result=78;try{result=input_actor_child(count,args);}catch(...){}LocalFree(args);return result;
    }
    if(count==2&&std::wstring_view(args[1])==L"--build-identity"){
        constexpr char identity[]="{\"product\":\"PaneBind V1 guest fixture\",\"source_commit\":\"" PANEBIND_BUILD_SHA "\",\"host_synthetic_input\":\"forbidden\",\"requires_medium_guest\":true}\n";
        DWORD written{};const auto output=GetStdHandle(STD_OUTPUT_HANDLE);
        if(output&&output!=INVALID_HANDLE_VALUE)WriteFile(output,identity,static_cast<DWORD>(sizeof(identity)-1),&written,nullptr);
        LocalFree(args);return 0;
    }
    bool parsed=count==7;if(parsed)for(int n=1;n<count;n+=2){const std::wstring name=args[n];if(name==L"--run-id")s.run=args[n+1];else if(name==L"--output")s.output=args[n+1];else if(name==L"--product")s.input_product=args[n+1];else parsed=false;}
    LocalFree(args);if(!parsed)return 78;
    try{const int result=guard(s);if(result)return result;}catch(...){try{return guard_reject(s,103,"guard_observation_or_evidence_exception");}catch(...){return 103;}}
    const bool dpi_set=SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE;const DWORD dpi_error=dpi_set?0:GetLastError();
    const bool pmv2=AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)!=FALSE;
    s.log.emit("guard_DPI","\"set_succeeded\":"+std::string(dpi_set?"true":"false")+",\"win32_error\":"+std::to_string(dpi_error)+",\"actual_thread_PMV2\":"+(pmv2?"true":"false"));
    if(!dpi_set&&!pmv2)return guard_reject(s,100,"PMV2_context_not_available");
    try{
        s.product_log=s.product.parent_path()/L"panebind.log.jsonl";
        if(fs::exists(s.product_log)||fs::exists(s.product.parent_path()/L"settings.ini"))return guard_reject(s,101,"product_runtime_directory_is_not_fresh");
        const auto source=hash(s.input_product),copy=hash(s.product);if(source.empty()||source!=copy)return guard_reject(s,102,"product_changed_after_guard");
        s.log.emit("guard","\"run_id\":"+quote(utf8(s.run))+",\"configuration\":"+quote(s.configuration)+",\"actual_integrity\":\"Medium\",\"elevated\":false,\"driver_has_console\":"+
            std::string(GetConsoleWindow()?"true":"false")+",\"input_executable\":"+quote(utf8(s.input_product.wstring()))+",\"actual_executable\":"+quote(utf8(s.product.wstring()))+
            ",\"binary_sha256\":"+quote(source)+",\"human_uat\":\"NOT_RUN\"");
        s.class_name=L"PaneBind.V1.OwnedFixture."+s.run;WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=s.class_name.c_str();wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
        if(!RegisterClassW(&wc))return 1;
        for(std::size_t n=0;n<3;++n){const auto title=L"PaneBind V1 — "+std::wstring(1,static_cast<wchar_t>(L'A'+n));
            s.frames[n].hwnd=CreateWindowExW(0,s.class_name.c_str(),title.c_str(),WS_OVERLAPPEDWINDOW,100+static_cast<int>(n)*250,80,260,260,nullptr,nullptr,instance,&s.frames[n]);
            if(!s.frames[n].hwnd)throw std::runtime_error("owned_window_creation_failed");ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);UpdateWindow(s.frames[n].hwnd);}
        MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);s.driver=std::thread([&]{drive(s);});
        while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}if(s.driver.joinable())s.driver.join();
        bool destroyed=true;for(auto& f:s.frames)if(f.hwnd&&IsWindow(f.hwnd)){if(!own(s,f.hwnd)){destroyed=false;continue;}destroyed=DestroyWindow(f.hwnd)&&destroyed;}UnregisterClassW(s.class_name.c_str(),instance);
        s.log.emit("driver_shutdown","\"result\":"+quote(s.result==0&&!s.log.failed&&destroyed?"PASS":"FAIL")+",\"owned_windows_destroyed\":"+std::string(destroyed?"true":"false")+
            ",\"product_process_closed\":"+std::string(!s.process?"true":"false")+",\"scope\":\"exact_guest_targets_not_full_acceptance\",\"human_uat\":\"NOT_RUN\"");
        return s.result==0&&!s.log.failed&&destroyed?0:1;
    }catch(...){if(s.driver.joinable())s.driver.join();for(auto& f:s.frames)if(own(s,f.hwnd))DestroyWindow(f.hwnd);return 1;}
}
