// Disposable-Sandbox driver, not product input. No core solver supplies any
// expected geometry. All native windows and child processes are owned here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <wincrypt.h>
#include <objbase.h>
#include "platform/windows/product/window_catalog.h"
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
struct Frame{HWND hwnd{};std::atomic<unsigned> down{},up{},starts{},ends{};LONG min_width{120},min_height{100},grid{1};WriterStall stall;};
struct ExplorerTarget {
    fs::path directory;shell::ShellLocationFact location;std::unique_ptr<shell::ExplorerProvisioningLease> lease;
    std::unique_ptr<shell::ExplorerConsentTargetObservation> observation;
    std::optional<product::Snapshot> identity;HWND hwnd{};DWORD pid{},tid{};std::uint64_t born{};
    std::vector<HWND> baseline;bool separate{},navigated{};
};
struct State{
    HINSTANCE instance{};std::wstring run,class_name;fs::path output,input_product,product,product_log;
    std::string configuration;std::array<Frame,3> frames;Log log;std::thread driver;std::atomic<int> result{1};
    HANDLE process{};DWORD product_pid{},owner{};bool left{},control{};
    std::array<std::unique_ptr<ExplorerTarget>,3> explorer;
    std::vector<std::unique_ptr<ExplorerTarget>> retired_explorers;
    std::unique_ptr<product::Catalog> target_catalog;
};
HWND source_window(State& s,std::size_t source){return s.explorer.at(source)?s.explorer[source]->hwnd:s.frames.at(source).hwnd;}
DWORD source_thread(State& s,std::size_t source){return s.explorer.at(source)?s.explorer[source]->tid:s.owner;}
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM wp,LPARAM lp){
    auto* frame=reinterpret_cast<Frame*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE){frame=static_cast<Frame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(frame));}
    if(frame){
        if(message==WM_GETMINMAXINFO){auto* mm=reinterpret_cast<MINMAXINFO*>(lp);mm->ptMinTrackSize={frame->min_width,frame->min_height};mm->ptMaxTrackSize={1800,1400};return 0;}
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
void inject(INPUT packet){
    if(!desktop())throw std::runtime_error("guest_input_desktop_became_inactive");packet.mi.dwExtraInfo=0x50425631;
    if(SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("test_SendInput_failed");
}
void cursor(POINT point){
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),w=GetSystemMetrics(SM_CXVIRTUALSCREEN),h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(w<2||h<2||point.x<x||point.y<y||point.x>=x+w||point.y>=y+h)throw std::runtime_error("test_cursor_outside_virtual_screen");
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx=static_cast<LONG>(((static_cast<long long>(point.x-x)*2+1)*65536)/(static_cast<long long>(w)*2));
    input.mi.dy=static_cast<LONG>(((static_cast<long long>(point.y-y)*2+1)*65536)/(static_cast<long long>(h)*2));inject(input);
    if(!wait([&]{POINT p{};return GetCursorPos(&p)&&p.x==point.x&&p.y==point.y;},500))throw std::runtime_error("test_cursor_not_exact");
}
void key(State& s,WORD code,bool pressed){
    INPUT packet{};packet.type=INPUT_KEYBOARD;packet.ki.wVk=code;packet.ki.dwFlags=pressed?0:KEYEVENTF_KEYUP;packet.ki.dwExtraInfo=0x50425631;
    if(!desktop()||SendInput(1,&packet,sizeof(packet))!=1)throw std::runtime_error("test_key_input_failed");
    if(code==VK_CONTROL)s.control=pressed;
    if(!wait([&]{return ((GetAsyncKeyState(code)&0x8000)!=0)==pressed;},500))throw std::runtime_error("test_key_state_not_observed");
}
void button(State& s,bool pressed){INPUT packet{};packet.type=INPUT_MOUSE;packet.mi.dwFlags=pressed?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;inject(packet);s.left=pressed;}
std::optional<POINT> hit_point(HWND h,int required){
    RECT p{};if(!GetWindowRect(h,&p))return {};
    const LONG begin=required==HTBOTTOM?p.bottom-8:p.top+3,end=required==HTBOTTOM?p.bottom:p.top+55;
    const LONG left=required==HTBOTTOM?(p.left+p.right)/2:p.left+40,right=required==HTBOTTOM?left+1:p.right-40;
    for(LONG y=begin;y<end;++y)for(LONG x=left;x<right;x+=8){DWORD_PTR hit{};const POINT point{x,y};if(WindowFromPoint(point)!=h)continue;
        if(SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG,100,&hit)&&static_cast<int>(hit)==required)return point;}
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
    cursor(*point);Sleep(100);if(ctrl)key(s,VK_CONTROL,true);Gesture g{*point,0,read(s.product_log).size()};button(s,true);
    const bool observed=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;});
    s.log.check("production_real_down_start",observed,
        "\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation));return g;
}
void phase(State& s,std::size_t source,const Gesture& g,LONG dx,LONG dy,Rect expected,std::string name,bool native){
    // Deliberately paced input trajectory below the configured speed cutoff;
    // this is not a product retry, cursor sampler or authority workaround.
    if(!authorized_source(s,source_window(s,source)))throw std::runtime_error("test_source_identity_revoked");
    Sleep(60);const auto mark=read(s.product_log).size();cursor({g.anchor.x+dx,g.anchor.y+dy});std::string receipt;
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
enum class BlockedWriterExit { RealUp, EmergencyHotkey };
// Deliberately NOT invoked by drive(): first establish the normal production
// chain. This case terminates that product instance and belongs at the end of
// an explicitly selected L2 run, never in the ordinary application path.
[[maybe_unused]] void blocked_writer_case(State& s,std::size_t source,BlockedWriterExit exit){
    if(source>=s.frames.size()||!own(s,s.frames[source].hwnd)||!s.process)throw std::runtime_error("L2_exact_owned_source_required");
    auto& stall=s.frames[source].stall;
    s.log.check("L2_no_previous_stall",!stall.armed&&!stall.entered&&!stall.finished);
    const auto g=down(s,source);stall.armed=true;Sleep(60);cursor({g.anchor.x-42,g.anchor.y+35});
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
    cursor({g.anchor.x-58,g.anchor.y+45});const bool quit_started_while_blocked=!stall.finished;
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
        try{cursor(point);if(WindowFromPoint(point)!=f.hwnd)break;const auto downs=f.down.load(),ups=f.up.load();
            button(s,true);button(s,false);delivered=wait([&]{return f.down.load()==downs+1&&f.up.load()==ups+1;},1000);
        }catch(...){if(s.left){try{button(s,false);}catch(...){}}}
        break;
    }
    s.log.emit("post_stop_actual_client_click","\"executed\":"+std::string(check_delivery?"true":"false")+",\"actual_down_up_delivered\":"+std::string(delivered?"true":"false")+
        ",\"only_test_owned_window\":true,\"not_guest_destruction\":true");
    if(check_delivery&&!delivered)s.log.failed=true;
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
        if(!hit)throw std::runtime_error("command_target_titlebar_unavailable");cursor(*hit);Sleep(100);
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
    if(!s.output.is_absolute()||!equal(s.output.lexically_normal().wstring(),output.wstring()))return 82;
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
    if(error||!equal(actual_output.wstring(),output.wstring()))return guard_reject(s,91,"output_canonical_path_not_exact");
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
