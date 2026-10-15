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
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
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
bool medium(HANDLE process){
    HANDLE raw{};if(!OpenProcessToken(process,TOKEN_QUERY,&raw))return false;Handle token(raw);
    alignas(TOKEN_MANDATORY_LABEL) std::array<unsigned char,512> bytes{};DWORD size{};TOKEN_ELEVATION elevation{};
    if(!GetTokenInformation(token.value,TokenIntegrityLevel,bytes.data(),static_cast<DWORD>(bytes.size()),&size)||
        !GetTokenInformation(token.value,TokenElevation,&elevation,sizeof(elevation),&size))return false;
    const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(bytes.data())->Label.Sid;
    return IsValidSid(sid)&&*GetSidSubAuthorityCount(sid)>0&&
        *GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1U)==SECURITY_MANDATORY_MEDIUM_RID&&!elevation.TokenIsElevated;
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
    const auto end=GetTickCount64()+duration;do{if(predicate())return true;Sleep(20);}while(GetTickCount64()<end);return predicate();
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
struct Frame{HWND hwnd{};std::atomic<unsigned> down{},up{},starts{},ends{};LONG min_width{120},min_height{100},grid{1};};
struct State{
    HINSTANCE instance{};std::wstring run,class_name;fs::path output,input_product,product,product_log;
    std::string configuration;std::array<Frame,3> frames;Log log;std::thread driver;std::atomic<int> result{1};
    HANDLE process{};DWORD product_pid{},owner{};bool left{},control{};
};
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM wp,LPARAM lp){
    auto* frame=reinterpret_cast<Frame*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE){frame=static_cast<Frame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(frame));}
    if(frame){
        if(message==WM_GETMINMAXINFO){auto* mm=reinterpret_cast<MINMAXINFO*>(lp);mm->ptMinTrackSize={frame->min_width,frame->min_height};mm->ptMaxTrackSize={1800,1400};return 0;}
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
    const LONG x=required==HTBOTTOM?(p.left+p.right)/2:p.left+80;
    const LONG begin=required==HTBOTTOM?p.bottom-8:p.top+3,end=required==HTBOTTOM?p.bottom:p.top+55;
    for(LONG y=begin;y<end;++y){DWORD_PTR hit{};const POINT point{x,y};if(WindowFromPoint(point)!=h)continue;
        if(SendMessageTimeoutW(h,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG,100,&hit)&&static_cast<int>(hit)==required)return point;}
    return {};
}
bool own(State&,HWND);
void foreground(State& s,HWND h){
    if(!own(s,h))throw std::runtime_error("not_exact_owned_source");
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
    const auto h=s.frames[source].hwnd;foreground(s,h);const auto point=hit_point(h,region);
    s.log.check("actual_nonclient_hit",point.has_value(),"\"source\":"+std::to_string(source)+",\"hit\":"+std::to_string(region));
    cursor(*point);Sleep(100);if(ctrl)key(s,VK_CONTROL,true);Gesture g{*point,0,read(s.product_log).size()};button(s,true);
    const bool observed=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");return g.generation!=0;});
    s.log.check("production_real_down_start",observed,
        "\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation));return g;
}
void phase(State& s,std::size_t source,const Gesture& g,LONG dx,LONG dy,Rect expected,std::string name,bool native){
    // Deliberately paced input trajectory below the configured speed cutoff;
    // this is not a product retry, cursor sampler or authority workaround.
    Sleep(60);const auto mark=read(s.product_log).size();cursor({g.anchor.x+dx,g.anchor.y+dy});std::string receipt;
    const bool ok=wait([&]{receipt=placement(s,mark,g.generation,expected,native);return visible(s.frames[source].hwnd)==expected&&!receipt.empty();});
    const auto actual=visible(s.frames[source].hwnd);
    s.log.check(std::move(name),ok,"\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation)+
        ",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+",\"trajectory_pause_ms\":60,\"production_receipt\":"+quote(receipt));
}
void up(State& s,std::size_t source,const Gesture& g,Rect expected){
    const auto mark=read(s.product_log).size();button(s,false);if(s.control)key(s,VK_CONTROL,false);std::string end;
    const bool observed=wait([&]{end=record(s,mark,"gesture_end",g.generation);return end.find("\"normal_up\":true")!=std::string::npos;});
    Sleep(150);GUITHREADINFO gui{sizeof(gui)};const bool gui_read=GetGUIThreadInfo(s.owner,&gui)!=FALSE;const auto actual=visible(s.frames[source].hwnd);
    s.log.check("normal_up_and_no_late_geometry",observed&&actual==expected&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&gui_read&&!gui.hwndCapture,
        "\"generation\":"+std::to_string(g.generation)+",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+
        ",\"source_thread_capture\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture))+",\"production_end\":"+quote(end));
}
void five(State& s,std::size_t source,std::string label){
    const auto original=visible(s.frames[source].hwnd);if(!original)throw std::runtime_error("owned_geometry_missing");
    const auto g=down(s,source);const LONG dx=source==0?-40:50,dy=source==1?-35:40;
    phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".free",true);
    phase(s,source,g,-6,3,*original,label+".xy_snap",true);phase(s,source,g,-8,5,*original,label+".hold",false);
    phase(s,source,g,dx,dy,translated(*original,dx,dy),label+".detach",true);phase(s,source,g,-5,4,*original,label+".resnap",true);up(s,source,g,*original);
}
void ctrl_move(State& s,std::size_t source,LONG dx,LONG dy,std::string label){
    std::array<Rect,3> before{};for(std::size_t n=0;n<3;++n){const auto r=visible(s.frames[n].hwnd);if(!r)throw std::runtime_error("group_geometry_missing");before[n]=*r;}
    const auto g=down(s,source,true);phase(s,source,g,dx,dy,translated(before[source],dx,dy),label+".source",true);
    for(std::size_t n=0;n<3;++n){const auto target=translated(before[n],dx,dy);s.log.check(label+".member_"+std::to_string(n),
        wait([&]{return visible(s.frames[n].hwnd)==target;}),"\"expected\":"+rect(target));}
    up(s,source,g,translated(before[source],dx,dy));
}
bool own(State& s,HWND window){
    DWORD pid{};std::array<wchar_t,256> name{};
    if(!window||!IsWindow(window)||GetWindowThreadProcessId(window,&pid)!=s.owner||pid!=GetCurrentProcessId()||
        GetClassNameW(window,name.data(),static_cast<int>(name.size()))<=0||!equal(name.data(),s.class_name))return false;
    const auto frame=std::find_if(s.frames.begin(),s.frames.end(),[&](const auto& f){return f.hwnd==window;});
    return frame!=s.frames.end()&&GetWindowLongPtrW(window,GWLP_USERDATA)==reinterpret_cast<LONG_PTR>(&*frame);
}
void cleanup(State& s){
    if(s.left){try{button(s,false);s.log.emit("test_cleanup","\"synthetic_release_sent\":true,\"not_product_cleanup_pass\":true");}catch(...){}}
    if(s.control){try{key(s,VK_CONTROL,false);}catch(...){}}
    if(s.process){const bool command=launch(s,L"--quit",true);const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;
        const auto shutdown=record(s,0,"shutdown");DWORD exit_code=STILL_ACTIVE;if(stopped)GetExitCodeProcess(s.process,&exit_code);
        s.log.emit("product_stop","\"quit_command_completed\":"+std::string(command?"true":"false")+",\"process_exit_observed\":"+(stopped?"true":"false")+
            ",\"product_exit_code\":"+std::to_string(exit_code)+",\"shutdown_report\":"+quote(shutdown));
        if(!command||!stopped||exit_code!=0||shutdown.empty())s.log.failed=true;
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
    try{
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
        cleanup(s);
        if(!s.log.failed)s.log.emit("functional_summary","\"owned_I02\":\"PASS\",\"production_commands\":\"PASS\",\"normal_stop_and_client_delivery\":\"PASS\",\"Explorer\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
        s.result=s.log.failed?1:0;
    }catch(const std::exception& error){
        try{s.log.emit("failure","\"reason\":"+quote(error.what()));cleanup(s);}catch(...){}s.result=1;
    }
    PostThreadMessageW(s.owner,WM_QUIT,0,0);
}
bool guard(State& s){
    if(s.run.size()!=32||!std::all_of(s.run.begin(),s.run.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');}))return false;
    std::array<wchar_t,128> username{};DWORD length=static_cast<DWORD>(username.size()),session{};
    if(!GetUserNameW(username.data(),&length)||!equal(username.data(),L"WDAGUtilityAccount")||
        !ProcessIdToSessionId(GetCurrentProcessId(),&session)||!session||!medium(GetCurrentProcess())||!desktop())return false;
    LPWSTR value{};DWORD bytes{};const bool queried=WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSConnectState,&value,&bytes)!=FALSE;
    const bool active=queried&&bytes>=sizeof(WTS_CONNECTSTATE_CLASS)&&*reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(value)==WTSActive;if(value)WTSFreeMemory(value);if(!active)return false;
    const fs::path input=L"C:\\PaneBindV1\\Input",output=L"C:\\PaneBindV1\\Output";std::error_code error;
    if(!plain_directory(input)||!plain_directory(output)||!equal(fs::weakly_canonical(s.output,error).wstring(),output.wstring())||error||
        !marker(input/L"panebind-v1-input.marker",s.run)||!marker(output/L"panebind-v1-output.marker",s.run))return false;
    const auto actual=fs::canonical(s.input_product,error);if(error)return false;
    const fs::path debug_dir=output/(s.run+L"-product-Debug"),release_dir=output/(s.run+L"-product-Release");
    const bool debug=equal(actual.wstring(),(debug_dir/L"panebind.exe").wstring());
    if(!debug&&!equal(actual.wstring(),(release_dir/L"panebind.exe").wstring()))return false;
    if(!plain_directory(actual.parent_path())||(debug&&!plain_directory(input/L"Debug")))return false;
    if(GetFileAttributesW(actual.c_str())&FILE_ATTRIBUTE_REPARSE_POINT)return false;
    const fs::path source=debug?input/L"Debug"/L"panebind.exe":input/L"panebind.exe";
    if(!fs::is_regular_file(source,error)||error)return false;
    const auto source_hash=hash(source),actual_hash=hash(actual);
    if(source_hash.empty()||source_hash!=actual_hash)return false;
    if(!read(actual.parent_path()/L"portable.flag",128).starts_with("PaneBind V1 portable test candidate"))return false;
    s.input_product=source;s.product=actual;s.output=output;s.configuration=debug?"Debug":"Release";
    return !FindWindowW(L"PaneBind.V1.Application",L"PaneBind");
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
    LocalFree(args);if(!parsed||!guard(s))return 78;
    if(!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)&&!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))return 78;
    try{
        const std::wstring suffix=s.configuration=="Debug"?L"debug":L"release";s.product_log=s.product.parent_path()/L"panebind.log.jsonl";
        if(fs::exists(s.product_log)||fs::exists(s.product.parent_path()/L"settings.ini"))return 78;
        const auto source=hash(s.input_product),copy=hash(s.product);if(source.empty()||source!=copy)return 1;
        s.log.path=s.output/(s.run+L"-"+suffix+L"-fixture.jsonl");
        {Handle file(CreateFileW(s.log.path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(file.value==INVALID_HANDLE_VALUE)return 78;}
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
