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
#include <wtsapi32.h>
#include <wincrypt.h>
#include <objbase.h>
#include <tlhelp32.h>
#include "platform/windows/product/window_catalog.h"
#include "platform/windows/product/guest_path_scope.h"
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
std::string quote(std::string_view text){std::string out="\"";for(char c:text){if(c=='\\'||c=='"')out+='\\';out+=c<32?' ':c;}return out+'"';}
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
    std::atomic<bool> armed{},entered{},finished{},left_down{};
    std::atomic<std::uint64_t> before_qpc{},after_qpc{},before_tick{};
    std::atomic<UINT> flags{};std::atomic<int> x{},y{},width{},height{};
};
struct Frame{HWND hwnd{};std::atomic<unsigned> down{},up{},starts{},ends{};LONG min_width{120},min_height{100},grid{1},max_width{},max_height{};WriterStall stall;};
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
    std::string kind;fs::path executable,directory;std::wstring command;
    std::vector<HWND> baseline;std::vector<ApplicationProcess> processes;
    std::optional<product::Snapshot> identity;bool accepted{},cleanup_confirmed{};
};
struct State{
    HINSTANCE instance{};std::wstring run,class_name;fs::path output,input_product,product,product_log;
    std::string configuration;std::array<Frame,3> frames;Log log;std::thread driver;std::atomic<int> result{1};
    HANDLE process{};DWORD product_pid{},owner{};bool left{},control{};
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
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM wp,LPARAM lp){
    auto* frame=reinterpret_cast<Frame*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE){frame=static_cast<Frame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(frame));}
    if(frame){
        if(message==WM_GETMINMAXINFO){auto* mm=reinterpret_cast<MINMAXINFO*>(lp);mm->ptMinTrackSize={frame->min_width,frame->min_height};
            const auto dpi=GetDpiForWindow(h);
            mm->ptMaxTrackSize={frame->max_width>0?frame->max_width:GetSystemMetricsForDpi(SM_CXMAXTRACK,dpi),
                frame->max_height>0?frame->max_height:GetSystemMetricsForDpi(SM_CYMAXTRACK,dpi)};return 0;}
        if(message==WM_WINDOWPOSCHANGING&&frame->stall.armed.load()&&(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&frame->stall.armed.exchange(false)){
            // TEST ONLY: this exact empty fixture owns this window/thread.
            // No source parameters are modified; the product's already-issued
            // SetWindowPos may finish after UP. Never suspend another process.
            const auto* position=reinterpret_cast<const WINDOWPOS*>(lp);LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);
            frame->stall.flags=position->flags;frame->stall.x=position->x;frame->stall.y=position->y;
            frame->stall.width=position->cx;frame->stall.height=position->cy;frame->stall.before_qpc=static_cast<std::uint64_t>(qpc.QuadPart);
            frame->stall.before_tick=GetTickCount64();frame->stall.left_down=true;frame->stall.entered=true;
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
        s.log.emit("test_cursor_observation","\"requested\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"before\":"+
            (before_read?"["+std::to_string(before.x)+","+std::to_string(before.y)+"]":"null")+",\"before_GetCursorPos_error\":"+std::to_string(before_error)+
            ",\"actual\":"+(position_read?"["+std::to_string(actual.x)+","+std::to_string(actual.y)+"]":"null")+
            ",\"GetCursorPos_succeeded\":"+(position_read?"true":"false")+",\"GetCursorPos_error\":"+std::to_string(position_error)+
            ",\"virtual_screen\":["+std::to_string(x)+","+std::to_string(y)+","+std::to_string(w)+","+std::to_string(h)+
            "],\"normalized_absolute\":["+std::to_string(input.mi.dx)+","+std::to_string(input.mi.dy)+"],\"mouse_flags\":"+std::to_string(input.mi.dwFlags)+
            ",\"SendInput_called\":"+(delivered==UINT_MAX?"false":"true")+",\"SendInput_return\":"+(delivered==UINT_MAX?"null":std::to_string(delivered))+
            ",\"SendInput_error_on_failure\":"+std::to_string(injection_error)+",\"bounded_observations\":"+std::to_string(observations)+
            ",\"exact_system_cursor\":"+(exact?"true":"false")+",\"product_packet_not_used_as_cursor_proof\":true");
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
void button(State& s,bool pressed){INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;inject(packet);s.left=pressed;}
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
std::string placement(State& s,std::size_t offset,std::uint64_t generation,Rect expected,bool native){
    const auto text=read(s.product_log);if(offset>=text.size())return {};std::istringstream stream(text.substr(offset));std::string line,out;
    while(std::getline(stream,line))if(line.ends_with('}')&&line.find("\"type\":\"placement\"")!=std::string::npos&&
        number(line,"\"generation\":")==generation&&line.find("\"exact\":true")!=std::string::npos&&
        line.find("\"actual\":"+rect(expected))!=std::string::npos&&(!native||line.find("\"native_attempted\":true")!=std::string::npos))out=line;
    return out;
}
struct Gesture{POINT anchor{};std::uint64_t generation{};std::size_t offset{};};
Gesture down(State& s,std::size_t source,bool ctrl=false,int region=HTCAPTION){
    const auto h=source_window(s,source);foreground(s,h);const auto point=hit_point(h,region);
    s.log.check("actual_nonclient_hit",point.has_value(),"\"source\":"+std::to_string(source)+",\"hit\":"+std::to_string(region));
    cursor(s,*point);Sleep(100);if(ctrl)key(s,VK_CONTROL,true);Gesture g{*point,0,read(s.product_log).size()};button(s,true);
    const bool observed=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;});
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
        const auto fresh=s.target_catalog->get(h,{});
        if(!fresh||!product::Catalog::same(*target->identity,*fresh)||
            std::find(target->baseline.begin(),target->baseline.end(),h)!=target->baseline.end())return false;
        const auto process=std::find_if(target->processes.begin(),target->processes.end(),[&](const auto& p){return p.pid==fresh->pid&&p.born==fresh->born;});
        return process!=target->processes.end()&&process->handle&&WaitForSingleObject(process->handle->value,0)==WAIT_TIMEOUT&&
            equal(fresh->exe,target->executable.wstring());
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
bool provision_explorer(State& s,std::size_t index,bool separate){
    auto target=std::make_unique<ExplorerTarget>();target->separate=separate;
    target->directory=s.output/(s.run+L"-explorer-"+std::wstring(s.configuration.begin(),s.configuration.end())+L"-"+
        std::wstring(1,static_cast<wchar_t>(L'A'+index))+(separate?L"-separate":L""));
    if(fs::exists(target->directory)||!fs::create_directory(target->directory))throw std::runtime_error("exact_new_explorer_directory_required");
    target->location=shell::filesystem_location_identity(target->directory);if(!target->location.filesystem())throw std::runtime_error("new_directory_file_identity_unavailable");
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
    const bool accepted=target->identity.has_value()&&target->observation&&exact_shell_location(*target);
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
    std::vector<HWND> out;EnumWindows([](HWND h,LPARAM context)->BOOL{
        auto& roots=*reinterpret_cast<std::vector<HWND>*>(context);if(roots.size()>=1024)return FALSE;
        if(GetAncestor(h,GA_ROOT)==h)roots.push_back(h);return TRUE;},reinterpret_cast<LPARAM>(&out));return out;
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
            target.processes.push_back({candidate.th32ProcessID,candidate.th32ParentProcessID,born,std::move(handle)});added=true;if(target.processes.size()>=64)break;
        }
        if(!added)break;
    }
}
bool application_fresh(State& s,const ApplicationTarget& target,HWND h,std::optional<product::Snapshot>* actual=nullptr){
    if(!h||std::find(target.baseline.begin(),target.baseline.end(),h)!=target.baseline.end()||GetAncestor(h,GA_ROOT)!=h||!s.target_catalog)return false;
    auto fresh=s.target_catalog->get(h,{});if(!fresh||!equal(fresh->exe,target.executable.wstring()))return false;
    const auto process=std::find_if(target.processes.begin(),target.processes.end(),[&](const auto& p){return p.pid==fresh->pid&&p.born==fresh->born;});
    if(process==target.processes.end()||!process->handle||WaitForSingleObject(process->handle->value,0)!=WAIT_TIMEOUT||!medium(process->handle->value))return false;
    if(target.kind=="Terminal"&&!equal(fresh->window_class,L"ConsoleWindowClass"))return false;
    if(actual)*actual=std::move(fresh);return true;
}
std::shared_ptr<ApplicationTarget> provision_application(State& s,std::string kind){
    auto target=std::make_shared<ApplicationTarget>();target->kind=std::move(kind);
    const fs::path base=fs::path(L"C:\\PaneBindV1\\Applications")/s.run;
    if(!plain_directory(base)&&!fs::create_directory(base))throw std::runtime_error("application_RunId_directory_not_available");
    target->directory=base/(std::wstring(s.configuration.begin(),s.configuration.end())+L"-"+std::wstring(target->kind.begin(),target->kind.end()));
    if(fs::exists(target->directory)||!fs::create_directory(target->directory)||!plain_directory(target->directory))throw std::runtime_error("fresh_application_directory_required");
    std::array<wchar_t,MAX_PATH> system{};if(!GetSystemDirectoryW(system.data(),static_cast<UINT>(system.size())))throw std::runtime_error("official_guest_System_directory_unavailable");
    const fs::path blank=target->directory/L"PaneBind-empty.txt";
    if(target->kind=="Notepad"){target->executable=fs::path(system.data())/L"notepad.exe";std::ofstream(blank,std::ios::binary).close();target->command=L"\""+target->executable.wstring()+L"\" \""+blank.wstring()+L"\"";}
    else if(target->kind=="Edge"){
        for(const fs::path candidate:{fs::path(L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"),fs::path(L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe")})
            if(fs::is_regular_file(candidate)){target->executable=candidate;break;}
        target->command=L"\""+target->executable.wstring()+L"\" --user-data-dir=\""+(target->directory/L"Profile").wstring()+L"\" --no-first-run --no-default-browser-check --disable-background-mode --new-window about:blank";
    }else if(target->kind=="VSCode"){
        target->executable=base/L"VSCode"/L"Code.exe";std::ofstream(blank,std::ios::binary).close();
        target->command=L"\""+target->executable.wstring()+L"\" --user-data-dir \""+(target->directory/L"UserData").wstring()+L"\" --extensions-dir \""+(target->directory/L"Extensions").wstring()+L"\" --disable-extensions --new-window --skip-welcome --skip-release-notes \""+blank.wstring()+L"\"";
    }else if(target->kind=="Terminal"){
        target->executable=fs::path(system.data())/L"conhost.exe";
        target->command=L"\""+target->executable.wstring()+L"\" \""+(fs::path(system.data())/L"cmd.exe").wstring()+L"\" /d /q /k title PaneBind-"+s.run+L"-Terminal";
    }else throw std::runtime_error("unknown_application_class");
    s.created_applications.push_back(target);target->baseline=root_baseline();
    if(target->executable.empty()||!fs::is_regular_file(target->executable)){
        s.log.emit("application_binding","\"application\":"+quote(target->kind)+",\"result\":\"BLOCKED\",\"reason\":\"explicit_official_executable_not_available\",\"input_sent\":false");return target;
    }
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION info{};std::wstring line=target->command;
    const bool launched=CreateProcessW(target->executable.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,target->directory.c_str(),&startup,&info)!=FALSE;
    const DWORD error=launched?0:GetLastError();int rid=-1;bool elevated{},normal=false;
    if(launched){CloseHandle(info.hThread);normal=medium(info.hProcess,&rid,&elevated);target->processes.push_back({info.dwProcessId,GetCurrentProcessId(),process_birth(info.hProcess),std::make_unique<Handle>(info.hProcess)});}
    s.log.emit("application_launch","\"application\":"+quote(target->kind)+",\"command\":"+quote(utf8(line))+",\"executable\":"+quote(utf8(target->executable.wstring()))+
        ",\"file_version\":"+quote(application_version(target->executable))+",\"binary_sha256\":"+quote(hash(target->executable))+
        ",\"api_succeeded\":"+(launched?"true":"false")+",\"win32_error\":"+std::to_string(error)+",\"launch_PID\":"+std::to_string(launched?info.dwProcessId:0)+
        ",\"launch_integrity_rid\":"+(rid<0?"null":std::to_string(rid))+",\"launch_elevated\":"+(rid<0?"null":(elevated?"true":"false"))+",\"launch_is_not_binding_proof\":true");
    std::optional<product::Snapshot> candidate;bool bound=false;
    if(normal&&target->processes.front().born)bound=wait([&]{
        discover_application_children(*target);candidate.reset();unsigned matches{};
        for(HWND h:root_baseline()){std::optional<product::Snapshot> fresh;if(application_fresh(s,*target,h,&fresh)){++matches;candidate=std::move(fresh);}}
        return matches==1;
    },12000);
    if(bound){target->identity=candidate;target->accepted=true;}
    s.log.emit("application_binding","\"application\":"+quote(target->kind)+",\"result\":"+quote(bound?"BOUND":"BLOCKED")+
        ",\"reason\":"+quote(bound?"baseline_excluded_new_root_exact_launch_tree_actual_Catalog_Medium_same_SID_session_desktop":"no_unique_eligible_root_in_held_birth_verified_launch_tree")+
        ",\"hwnd\":"+std::to_string(bound?reinterpret_cast<std::uintptr_t>(candidate->hwnd):0)+",\"pid\":"+std::to_string(bound?candidate->pid:0)+
        ",\"tid\":"+std::to_string(bound?candidate->tid:0)+",\"birth\":"+std::to_string(bound?candidate->born:0)+",\"key\":"+std::to_string(bound?candidate->key:0)+
        ",\"window_class\":"+quote(bound?utf8(candidate->window_class):"UNKNOWN")+",\"retained_process_facts\":"+std::to_string(target->processes.size())+
        ",\"input_sent\":false,\"product_permission_override\":false");return target;
}
bool close_application(State& s,ApplicationTarget& target){
    bool attempted=false,gone=false;
    if(target.accepted&&target.identity){std::optional<product::Snapshot> fresh;
        if(application_fresh(s,target,target.identity->hwnd,&fresh)&&product::Catalog::same(*target.identity,*fresh)){
            attempted=PostMessageW(fresh->hwnd,WM_CLOSE,0,0)!=FALSE;gone=attempted&&wait([&]{return !IsWindow(fresh->hwnd);},3000);
        }else gone=!IsWindow(target.identity->hwnd);
        if(!gone)s.log.failed=true;
    }
    target.cleanup_confirmed=gone;
    s.log.emit("application_cleanup","\"application\":"+quote(target.kind)+",\"root_confirmed\":"+(target.accepted?"true":"false")+
        ",\"exact_root_close_attempted\":"+(attempted?"true":"false")+",\"actual_root_gone\":"+(gone?"true":"false")+
        ",\"unknown_windows_touched\":false,\"processes_force_terminated\":false,\"guest_local_data_retained\":true");return gone||!target.accepted;
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
    s.log.check("L2_native_call_observed_in_owned_wndproc",wait([&]{return stall.entered.load();},800)&&stall.left_down.load()&&
        (GetAsyncKeyState(VK_LBUTTON)&0x8000),"\"generation\":"+std::to_string(g.generation)+",\"message\":\"WM_WINDOWPOSCHANGING\",\"bounded_sleep_ms\":1000");
    s.log.emit("L2_native_call_entered","\"generation\":"+std::to_string(g.generation)+",\"actual_owned_hwnd\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(s.frames[source].hwnd))+",\"native_entry_qpc\":"+std::to_string(stall.before_qpc)+
        ",\"actual_flags\":"+std::to_string(stall.flags)+",\"actual_positioning\":["+std::to_string(stall.x)+","+std::to_string(stall.y)+","+
        std::to_string(stall.width)+","+std::to_string(stall.height)+"],\"left_down_at_entry\":true");
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
        if(row.find("\"native_attempted\":true")!=std::string::npos){++issued;if(number(row,"\"native_before_qpc\":")>static_cast<std::uint64_t>(sent.QuadPart))++late_issued;}
        timeout_unknown=timeout_unknown||row.find("\"native_completion_known\":false")!=std::string::npos;
    }
    s.log.check("L2_no_new_native_attempt_after_terminator",issued==1&&late_issued==0&&timeout_unknown,
        "\"issued_receipts\":"+std::to_string(issued)+",\"issued_after_terminator\":"+std::to_string(late_issued)+
        ",\"old_native_call_may_finish_late\":true,\"source_return_qpc\":"+std::to_string(stall.after_qpc));
}
void cleanup(State& s){
    if(s.left){try{button(s,false);s.log.emit("test_cleanup","\"synthetic_release_sent\":true,\"not_product_cleanup_pass\":true");}catch(...){}}
    if(s.control){try{key(s,VK_CONTROL,false);}catch(...){}}
    if(s.process){const bool command=launch(s,L"--quit",true);const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;
        const auto shutdown=record(s,0,"shutdown");DWORD exit_code=STILL_ACTIVE;if(stopped)GetExitCodeProcess(s.process,&exit_code);
        s.log.emit("product_stop","\"quit_command_completed\":"+std::string(command?"true":"false")+",\"process_exit_observed\":"+(stopped?"true":"false")+
            ",\"product_exit_code\":"+std::to_string(exit_code)+",\"shutdown_report\":"+quote(shutdown));
        if(!command||!stopped||exit_code!=0||shutdown.find("\"native_worker_exited\":true")==std::string::npos||
            shutdown.find("\"issued_native_completion_unknown\":false")==std::string::npos)s.log.failed=true;
        if(!stopped){TerminateProcess(s.process,90);WaitForSingleObject(s.process,2000);s.log.failed=true;s.log.emit("test_abort","\"own_product_forced_termination\":true,\"product_cleanup_pass\":false");}
        CloseHandle(s.process);s.process=nullptr;}
    // A successful normal stop is also checked using real delivery to a
    // remaining exact empty fixture, not merely the child exit status. Forced
    // termination remains a failure regardless of this test-only observation.
    bool delivered=false;const bool check_delivery=!s.log.failed;
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
    if(!gui_identity(s,gui,control)||(hit!=control&&!IsChild(control,hit)))throw std::runtime_error("GUI_actual_hit_is_not_target_control");
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
    s.log.check("GUI_exact_product_settings_window",found,"\"pid\":"+std::to_string(s.product_pid)+
        ",\"class\":\"PaneBindV1SettingsWindow\",\"owner_and_child_identity_verified\":true");return gui;
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
void drive(State& s){
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try{
        if(FAILED(apartment))throw std::runtime_error("fixture_driver_COM_STA_initialization_failed");
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
        settings_gui_flow(s);
        explorer_flow(s);
        cleanup_explorers(s);
        cleanup(s);
        if(!s.log.failed)s.log.emit("functional_summary","\"owned_I02\":\"PASS\",\"production_commands\":\"PASS\",\"normal_stop_and_client_delivery\":\"PASS\",\"Explorer_I02\":\"PASS\",\"human_uat\":\"NOT_RUN\"");
        s.result=s.log.failed?1:0;
    }catch(const std::exception& error){
        try{s.log.emit("failure","\"reason\":"+quote(error.what()));cleanup_explorers(s);cleanup(s);}catch(...){}s.result=1;
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
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    State s;s.instance=instance;s.owner=GetCurrentThreadId();int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 64;
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
            ",\"product_process_closed\":"+std::string(!s.process?"true":"false")+",\"not_a_medium_application_matrix\":true,\"human_uat\":\"NOT_RUN\"");
        return s.result==0&&!s.log.failed&&destroyed?0:1;
    }catch(...){if(s.driver.joinable())s.driver.join();for(auto& f:s.frames)if(own(s,f.hwnd))DestroyWindow(f.hwnd);return 1;}
}
