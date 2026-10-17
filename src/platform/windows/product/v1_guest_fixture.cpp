// Disposable-Sandbox driver, not product input. No core solver supplies any
// expected geometry. All native windows and child processes are owned here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <aclapi.h>
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
#include "platform/windows/product/v1_guest_up_evidence.h"
#include "platform/windows/product/v1_guest_actor_capture.h"
#include "platform/windows/product/v1_guest_product_log.h"
#include "platform/windows/product/v1_guest_physical_log.h"
#include "platform/windows/product/v1_guest_idle_measurement.h"
#include "core/v1/caption_hit_guard.h"
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
#include <limits>
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
bool input_resource_stop_observed(std::string_view shutdown){
    // Old binaries did not use this candidate resource. A new-route process
    // must independently confirm real shield/receiver/capture/ATI cleanup;
    // its exit code and preview-window destruction are not that evidence.
    return shutdown.find("\"guest_capture_route_used\":true")==shutdown.npos||
        (shutdown.find("\"own_input_resources_stopped\":true")!=shutdown.npos&&
         shutdown.find("\"own_capture_released\":true")!=shutdown.npos&&
         shutdown.find("\"own_input_association_detached\":true")!=shutdown.npos);
}
namespace fs=std::filesystem;
namespace product=panebind::platform::windows::product;
namespace shell=panebind::platform::windows::explorer;
namespace v1=panebind::core::v1;
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
    [[nodiscard]] std::uint64_t mark()const{
        std::ifstream stream(path,std::ios::binary);return product::guest::driver_log_mark(stream);
    }
    [[nodiscard]] std::string since(std::uint64_t mark)const{
        std::ifstream stream(path,std::ios::binary);return product::guest::driver_log_suffix(stream,mark);
    }
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
// Read-only native facts. The query thread can differ from the window's
// receiver; neither awareness equality nor a synthetic hit result is assumed.
struct NativeCaptionObservation {
    POINT point{};RECT window{},buttons{};HRESULT hresult{E_PENDING};
    DPI_AWARENESS_CONTEXT query_context{},window_context{};UINT dpi{};
    bool window_known{},visible{},standard_buttons_possible{};
    [[nodiscard]] v1::CaptionHit result()const{
        return v1::caption_hit({{point.x,point.y},{window.left,window.top,window.right,window.bottom},
            {buttons.left,buttons.top,buttons.right,buttons.bottom},window_known,SUCCEEDED(hresult),
            query_context&&window_context&&dpi&&AreDpiAwarenessContextsEqual(query_context,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
            visible,standard_buttons_possible});
    }
};
NativeCaptionObservation native_caption_observation(HWND h,POINT point){
    NativeCaptionObservation fact;fact.point=point;fact.query_context=GetThreadDpiAwarenessContext();
    fact.window_context=GetWindowDpiAwarenessContext(h);fact.dpi=GetDpiForWindow(h);
    fact.window_known=GetWindowRect(h,&fact.window)!=FALSE;fact.visible=IsWindowVisible(h)&&!IsIconic(h);
    fact.hresult=DwmGetWindowAttribute(h,DWMWA_CAPTION_BUTTON_BOUNDS,&fact.buttons,sizeof(fact.buttons));
    SetLastError(0);const auto style=GetWindowLongPtrW(h,GWL_STYLE);const bool style_known=style!=0||GetLastError()==0;
    fact.standard_buttons_possible=!style_known||((style&WS_CAPTION)==WS_CAPTION&&(style&(WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX))!=0);
    return fact;
}
std::string native_caption_json(const NativeCaptionObservation& fact){
    const auto raw=[](const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+
        std::to_string(r.right)+","+std::to_string(r.bottom)+"]";};
    return "{\"point\":["+std::to_string(fact.point.x)+","+std::to_string(fact.point.y)+"]"+
        ",\"query_thread_DPI_context\":"+(fact.query_context?std::to_string(reinterpret_cast<std::intptr_t>(fact.query_context)):"null")+
        ",\"query_thread_PMV2\":"+(fact.query_context&&AreDpiAwarenessContextsEqual(fact.query_context,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)?"true":"false")+
        ",\"window_DPI_context\":"+(fact.window_context?std::to_string(reinterpret_cast<std::intptr_t>(fact.window_context)):"null")+
        ",\"window_DPI\":"+(fact.dpi?std::to_string(fact.dpi):"null")+
        ",\"DwmGetWindowAttribute_hresult\":"+std::to_string(fact.hresult)+
        ",\"caption_buttons_window_relative\":"+(SUCCEEDED(fact.hresult)?raw(fact.buttons):"null")+
        ",\"positioning\":"+(fact.window_known?raw(fact.window):"null")+
        ",\"visible_not_iconic\":"+(fact.visible?"true":"false")+
        ",\"standard_caption_buttons_possible\":"+(fact.standard_buttons_possible?"true":"false")+
        ",\"caption_button_guard\":"+quote(v1::caption_hit_name(fact.result()))+"}";
}
struct NativeMoveBoundary {
    std::atomic<std::uint64_t> version{};
    std::array<std::atomic<std::uint64_t>,2> count{};
    std::array<std::atomic<WPARAM>,2> wparam{};
    std::array<std::atomic<LONG>,2> message_time{};
    std::array<std::atomic<SHORT>,2> key_state{};
    std::array<std::atomic<LPARAM>,2> lparam{};
    std::array<std::atomic<std::intptr_t>,2> receiver_context{},window_context{};
    std::array<std::atomic<UINT>,2> dpi{};std::array<std::atomic<HRESULT>,2> dwm_hresult{};
    std::array<std::atomic<bool>,2> positioning_known{},visible{},standard_buttons_possible{};
    std::array<std::array<std::atomic<LONG>,4>,2> positioning{},buttons{};
};
struct Frame{HWND hwnd{};std::atomic<unsigned> down{},up{},starts{},ends{};std::atomic<std::uint64_t> native_start_qpc{},native_end_qpc{};
    std::array<std::atomic<unsigned>,5> client_down{},client_up{};
    LONG min_width{120},min_height{100},grid{1},max_width{},max_height{};WriterStall stall;
    std::atomic<std::shared_ptr<NativeBatchLoss>> native_batch_loss;NativeMoveBoundary native_move_boundary;};
std::string native_move_boundary_json(const Frame& frame){
    const auto& fact=frame.native_move_boundary;
    for(unsigned attempt=0;attempt<3;++attempt){const auto version=fact.version.load();if(version&1u)continue;
        std::array<std::uint64_t,2> count{},wp{};std::array<LONG,2> time{};std::array<SHORT,2> key{};
        std::array<LPARAM,2> lp{};std::array<NativeCaptionObservation,2> caption{};
        for(std::size_t n=0;n<2;++n){count[n]=fact.count[n].load();wp[n]=fact.wparam[n].load();
            time[n]=fact.message_time[n].load();key[n]=fact.key_state[n].load();lp[n]=fact.lparam[n].load();
            auto& actual=caption[n];actual.point={static_cast<short>(LOWORD(lp[n])),static_cast<short>(HIWORD(lp[n]))};
            actual.query_context=reinterpret_cast<DPI_AWARENESS_CONTEXT>(fact.receiver_context[n].load());
            actual.window_context=reinterpret_cast<DPI_AWARENESS_CONTEXT>(fact.window_context[n].load());actual.dpi=fact.dpi[n].load();
            actual.hresult=fact.dwm_hresult[n].load();actual.window_known=fact.positioning_known[n].load();
            actual.visible=fact.visible[n].load();actual.standard_buttons_possible=fact.standard_buttons_possible[n].load();
            actual.window={fact.positioning[n][0].load(),fact.positioning[n][1].load(),fact.positioning[n][2].load(),fact.positioning[n][3].load()};
            actual.buttons={fact.buttons[n][0].load(),fact.buttons[n][1].load(),fact.buttons[n][2].load(),fact.buttons[n][3].load()};}
        if(version!=fact.version.load())continue;
        const auto row=[&](std::size_t n){return "{\"count\":"+std::to_string(count[n])+",\"last_wparam\":"+
            (count[n]?std::to_string(wp[n]):"null")+",\"last_GetMessageTime_return\":"+(count[n]?std::to_string(time[n]):"null")+
            ",\"last_GetKeyState_LMB_return\":"+(count[n]?std::to_string(key[n]):"null")+
            ",\"last_lParam\":"+(count[n]?std::to_string(lp[n]):"null")+
            ",\"last_message_signed_screen_point\":"+(count[n]?"["+std::to_string(caption[n].point.x)+","+std::to_string(caption[n].point.y)+"]":"null")+
            ",\"receiver_DPI_and_caption_at_message\":"+(count[n]?native_caption_json(caption[n]):"null")+"}";};
        return "{\"snapshot_known\":true,\"WM_NCLBUTTONDOWN\":"+row(0)+",\"WM_SYSCOMMAND_SC_MOVE\":"+row(1)+
            ",\"snapshot_query_thread_DPI_context\":"+std::to_string(reinterpret_cast<std::intptr_t>(GetThreadDpiAwarenessContext()))+
            ",\"snapshot_query_context_is_not_sender_context\":true,\"button_delivery_or_physical_state_not_inferred_from_key_state\":true}";
    }
    return "{\"snapshot_known\":false,\"reason\":\"concurrent_window_message_observation\"}";
}
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
    std::optional<product::Snapshot> identity;bool accepted{},cleanup_confirmed{},matrix_passed{},close_failure_facts_recorded{},terminal_client_rejection_recorded{};
    std::string last_close_reason;
};
struct State{
    HINSTANCE instance{};std::wstring run,class_name;fs::path output,input_product,product,product_log;
    std::string configuration,suite{"Full"},product_source;std::array<Frame,3> frames;Log log;std::thread driver;std::atomic<int> result{1};
    HANDLE process{};DWORD product_pid{},owner{};
    product::guest::ProductLogEvidence product_evidence;fs::path product_evidence_path;
    // Set only while the exact guest-local diagnostic failure copy is active.
    // Not an argument or a general product-path/permission override.
    bool diagnostic_export_failure_local{};
    // Driver send ledger only: successful insertion is not native delivery or
    // physical button state. A separate product hook receipt proves receipt.
    bool left{},control{},left_up_attempted{};std::uint64_t left_down_send_qpc{},left_up_send_qpc{};
    std::array<std::unique_ptr<ExplorerTarget>,3> explorer;
    std::vector<std::unique_ptr<ExplorerTarget>> retired_explorers;
    std::unique_ptr<product::Catalog> target_catalog;
    std::array<std::shared_ptr<ApplicationTarget>,3> applications;
    std::vector<std::shared_ptr<ApplicationTarget>> created_applications;
};
std::uint64_t process_birth(HANDLE);
const std::string& read_product(State& s){
    // Only product evidence uses this bounded logical stream. Driver logs,
    // markers, settings and every other file keep their original read().
    if(!s.product_evidence.bound()||s.product_evidence_path!=s.product_log)return s.product_evidence.text();
    if(s.product_evidence.pid()!=s.product_pid||(s.process&&
       (GetProcessId(s.process)!=s.product_pid||process_birth(s.process)!=s.product_evidence.birth())))
        throw std::runtime_error("product_log_actual_process_identity_changed");
    unsigned pair_attempt{};
    const auto physical=[&](const fs::path& path)->std::optional<std::string>{
        product::guest::ProductLogPhysicalFile file(path);
        auto actual=file.snapshot();
        if(actual.state==product::guest::ProductLogPhysicalState::Missing)return std::string{};
        if(actual.state!=product::guest::ProductLogPhysicalState::Ready){
            s.log.emit("product_log_physical_read_error","\"file\":"+quote(utf8(path.filename().wstring()))+
                ",\"stage\":"+quote(actual.stage)+",\"win32_error\":"+std::to_string(actual.win32_error)+
                ",\"actual_size_known\":"+(actual.actual_size_known?"true":"false")+
                ",\"actual_size\":"+(actual.actual_size_known?std::to_string(actual.actual_size):"null")+
                ",\"pair_attempt\":"+std::to_string(pair_attempt+1)+",\"single_pair_recollection_allowed\":false"+
                ",\"rotation_cause_confirmed\":false");
            if(std::string_view(actual.stage)=="non_plain_handle_attributes")throw std::runtime_error("product_log_physical_file_not_plain");
            if(std::string_view(actual.stage)=="size"||std::string_view(actual.stage)=="size_limit"||
               std::string_view(actual.stage)=="negative_size")throw std::runtime_error("product_log_physical_size_unavailable_or_unbounded");
            throw std::runtime_error("product_log_physical_snapshot_failed");
        }
        return std::move(actual.bytes);
    };
    auto previous=s.product_log;previous+=L".1";std::string why;
    for(unsigned attempt=0;attempt<2;++attempt){
        pair_attempt=attempt;
        const auto old=physical(previous);if(!old)continue;
        const auto current=physical(s.product_log);if(!current)continue;
        if(s.product_evidence.observe(*old,*current,why))return s.product_evidence.text();
        if(s.product_evidence.rejected())break;
    }
    s.product_evidence.reject();throw std::runtime_error("product_log_evidence_rejected_"+why);
}
std::string product_log_since(State& s,std::size_t mark){
    (void)read_product(s);
    if(s.product_evidence_path!=s.product_log)return {};
    const auto result=s.product_evidence.since(mark);
    if(!result)throw std::runtime_error("product_log_mark_not_in_current_launch");
    return std::string(*result);
}
std::string product_log_current(State& s){return product_log_since(s,0);}
bool own(State&,HWND);
// IndependentEntry TEST observation only. One existing fixture UI STA writes
// fixed atomics; the driver reads at most three times. No product authority,
// normalized UP, HWND input attribution, callback disk IO or ring is created.
struct GuestRawMouseObserver {
    std::atomic<HWND> target{};std::atomic<bool> registered{};
    std::atomic<std::uint64_t> version{},packets{},buttons{},qpc{},device{},extra{},errors{};
    std::atomic<DWORD> stamp{};std::atomic<USHORT> flags{};
    std::array<std::atomic<std::uint64_t>,5> downs{},ups{};
} guest_raw_mouse;
bool raw_mouse_observer_registration(std::optional<RAWINPUTDEVICE>& mouse){
    std::array<RAWINPUTDEVICE,32> devices{};UINT capacity=static_cast<UINT>(devices.size());
    const auto count=GetRegisteredRawInputDevices(devices.data(),&capacity,sizeof(RAWINPUTDEVICE));
    if(count==static_cast<UINT>(-1)||count>devices.size())return false;
    for(UINT n=0;n<count;++n)if(devices[n].usUsagePage==1&&devices[n].usUsage==2){if(mouse)return false;mouse=devices[n];}
    return true;
}
bool raw_mouse_observer_start(State& s,HWND h){
    std::optional<RAWINPUTDEVICE> previous;SetLastError(0);
    const bool exact=desktop()&&medium(GetCurrentProcess())&&own(s,h)&&!guest_raw_mouse.target.load();
    const bool inventory_known=exact&&raw_mouse_observer_registration(previous);
    bool attempted=false,returned=false,verified=false;DWORD error{};
    if(inventory_known&&!previous){RAWINPUTDEVICE mouse{1,2,RIDEV_INPUTSINK,h};guest_raw_mouse.target=h;attempted=true;
        SetLastError(0);returned=RegisterRawInputDevices(&mouse,1,sizeof(mouse))!=FALSE;error=returned?0:GetLastError();
        if(returned){std::optional<RAWINPUTDEVICE> current;verified=raw_mouse_observer_registration(current)&&current&&
            current->hwndTarget==h&&current->dwFlags==RIDEV_INPUTSINK;guest_raw_mouse.registered=true;}
        else guest_raw_mouse.target=nullptr;
    }
    s.log.emit("guest_raw_mouse_observer_registration","\"result\":"+quote(verified?"REGISTERED_OBSERVATION_ONLY":"UNKNOWN")+
        ",\"actual_target\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"exact_test_owned\":"+(exact?"true":"false")+
        ",\"previous_registration_inventory_known\":"+(inventory_known?"true":"false")+",\"previous_mouse_registration_present\":"+
        (inventory_known?(previous?"true":"false"):"null")+",\"RegisterRawInputDevices_attempted\":"+(attempted?"true":"false")+
        ",\"RegisterRawInputDevices_return\":"+(attempted?(returned?"true":"false"):"null")+",\"error\":"+std::to_string(error)+
        ",\"actual_registration_verified\":"+(verified?"true":"false")+",\"input_delivery_or_product_PASS_not_inferred\":true");return verified;
}
bool raw_mouse_observer_stop(State& s){
    const HWND target=guest_raw_mouse.target.load();std::optional<RAWINPUTDEVICE> before;
    const bool known=raw_mouse_observer_registration(before);bool attempted=false,returned=false;DWORD error{};
    const bool ours=known&&before&&target&&before->hwndTarget==target&&before->dwFlags==RIDEV_INPUTSINK;
    if(ours){RAWINPUTDEVICE mouse{1,2,RIDEV_REMOVE,nullptr};attempted=true;SetLastError(0);
        returned=RegisterRawInputDevices(&mouse,1,sizeof(mouse))!=FALSE;error=returned?0:GetLastError();}
    std::optional<RAWINPUTDEVICE> after;const bool after_known=raw_mouse_observer_registration(after);
    const bool absent=after_known&&!after;const bool stopped=absent&&(!target||!before||returned);
    if(stopped){guest_raw_mouse.registered=false;guest_raw_mouse.target=nullptr;}
    s.log.emit("guest_raw_mouse_observer_removal","\"registration_before_known\":"+std::string(known?"true":"false")+
        ",\"only_own_exact_target_removed\":"+(ours?"true":"false")+",\"RIDEV_REMOVE_attempted\":"+(attempted?"true":"false")+
        ",\"RegisterRawInputDevices_return\":"+(attempted?(returned?"true":"false"):"null")+",\"error\":"+std::to_string(error)+
        ",\"registration_after_known\":"+(after_known?"true":"false")+",\"mouse_registration_actually_absent\":"+
        (after_known?(absent?"true":"false"):"null")+",\"observer_stopped\":"+(stopped?"true":"false")+
        ",\"product_cleanup_PASS_not_inferred\":true");return stopped;
}
void raw_mouse_observer_message(HWND h,UINT message,LPARAM lp){
    if(message!=WM_INPUT||guest_raw_mouse.target.load()!=h)return;
    RAWINPUT raw{};UINT bytes=sizeof(raw);const auto read_bytes=GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&raw,&bytes,sizeof(RAWINPUTHEADER));
    if(read_bytes==static_cast<UINT>(-1)||read_bytes<sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE)||raw.header.dwType!=RIM_TYPEMOUSE||raw.header.dwSize!=read_bytes){
        ++guest_raw_mouse.errors;return;
    }
    LARGE_INTEGER observed{};QueryPerformanceCounter(&observed);++guest_raw_mouse.version;++guest_raw_mouse.packets;
    const auto buttons=raw.data.mouse.usButtonFlags;
    if(buttons&0x3ffu){++guest_raw_mouse.buttons;guest_raw_mouse.flags=buttons;guest_raw_mouse.stamp=static_cast<DWORD>(GetMessageTime());
        guest_raw_mouse.qpc=observed.QuadPart>0?static_cast<std::uint64_t>(observed.QuadPart):0;
        guest_raw_mouse.device=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(raw.header.hDevice));guest_raw_mouse.extra=raw.data.mouse.ulExtraInformation;
        constexpr std::array<USHORT,5> down{RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_5_DOWN};
        constexpr std::array<USHORT,5> up{RI_MOUSE_LEFT_BUTTON_UP,RI_MOUSE_RIGHT_BUTTON_UP,RI_MOUSE_MIDDLE_BUTTON_UP,RI_MOUSE_BUTTON_4_UP,RI_MOUSE_BUTTON_5_UP};
        for(std::size_t n=0;n<5;++n){if(buttons&down[n])++guest_raw_mouse.downs[n];if(buttons&up[n])++guest_raw_mouse.ups[n];}
    }
    ++guest_raw_mouse.version;
}
std::string raw_mouse_observer_json(){
    for(unsigned attempt=0;attempt<3;++attempt){const auto before=guest_raw_mouse.version.load();if(before&1u)continue;
        const auto packets=guest_raw_mouse.packets.load(),buttons=guest_raw_mouse.buttons.load(),qpc=guest_raw_mouse.qpc.load(),device=guest_raw_mouse.device.load(),extra=guest_raw_mouse.extra.load();
        const auto stamp=guest_raw_mouse.stamp.load();const auto flags=guest_raw_mouse.flags.load();std::array<std::array<std::uint64_t,2>,5> counts{};
        for(std::size_t n=0;n<5;++n)counts[n]={guest_raw_mouse.downs[n].load(),guest_raw_mouse.ups[n].load()};
        if(before!=guest_raw_mouse.version.load())continue;std::string rows="[";
        for(std::size_t n=0;n<5;++n){if(n)rows+=',';rows+='['+std::to_string(counts[n][0])+','+std::to_string(counts[n][1])+']';}rows+=']';
        return "{\"snapshot_known\":true,\"registered\":"+std::string(guest_raw_mouse.registered.load()?"true":"false")+
            ",\"actual_mouse_packets\":"+std::to_string(packets)+",\"actual_button_packets\":"+std::to_string(buttons)+",\"actual_button_DOWN_UP_counts\":"+rows+
            ",\"last_button_flags\":"+(buttons?std::to_string(flags):"null")+",\"last_button_message_time\":"+(buttons?std::to_string(stamp):"null")+
            ",\"last_button_receiver_qpc\":"+(buttons?std::to_string(qpc):"null")+",\"last_button_hDevice\":"+(buttons?std::to_string(device):"null")+
            ",\"last_button_extra_information\":"+(buttons?std::to_string(extra):"null")+",\"raw_read_errors\":"+std::to_string(guest_raw_mouse.errors.load())+
            ",\"generation_authority_or_NormalUp_not_inferred\":true}";
    }
    return "{\"snapshot_known\":false,\"reason\":\"concurrent_native_observation\",\"generation_authority_or_NormalUp_not_inferred\":true}";
}
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
    // Preserve DefWindowProc below, including required WM_INPUT cleanup.
    raw_mouse_observer_message(h,message,lp);
    auto* frame=reinterpret_cast<Frame*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE){frame=static_cast<Frame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(frame));}
    if(frame){
        if(message==WM_NCLBUTTONDOWN||(message==WM_SYSCOMMAND&&(wp&0xfff0u)==SC_MOVE)){
            // One owning UI thread publishes fixed facts only. Preserve the
            // original message and the DefWindowProc call below, including
            // nested default processing; never write evidence on this thread.
            auto& fact=frame->native_move_boundary;const std::size_t index=message==WM_NCLBUTTONDOWN?0u:1u;
            const auto caption=native_caption_observation(h,{static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))});
            ++fact.version;++fact.count[index];fact.wparam[index]=wp;
            fact.message_time[index]=GetMessageTime();fact.key_state[index]=GetKeyState(VK_LBUTTON);fact.lparam[index]=lp;
            fact.receiver_context[index]=reinterpret_cast<std::intptr_t>(caption.query_context);
            fact.window_context[index]=reinterpret_cast<std::intptr_t>(caption.window_context);fact.dpi[index]=caption.dpi;
            fact.dwm_hresult[index]=caption.hresult;fact.positioning_known[index]=caption.window_known;
            fact.visible[index]=caption.visible;fact.standard_buttons_possible[index]=caption.standard_buttons_possible;
            const std::array<LONG,4> positioning{caption.window.left,caption.window.top,caption.window.right,caption.window.bottom};
            const std::array<LONG,4> buttons{caption.buttons.left,caption.buttons.top,caption.buttons.right,caption.buttons.bottom};
            for(std::size_t n=0;n<4;++n){fact.positioning[index][n]=positioning[n];fact.buttons[index][n]=buttons[n];}++fact.version;
        }
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
        // Actual CLIENT messages only. Nonclient messages and test insertion
        // returns are deliberately not counted as client delivery.
        unsigned client_button{};bool client_pressed{};
        if(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP){client_button=1;client_pressed=message==WM_LBUTTONDOWN;}
        else if(message==WM_RBUTTONDOWN||message==WM_RBUTTONUP){client_button=2;client_pressed=message==WM_RBUTTONDOWN;}
        else if(message==WM_MBUTTONDOWN||message==WM_MBUTTONUP){client_button=3;client_pressed=message==WM_MBUTTONDOWN;}
        else if(message==WM_XBUTTONDOWN||message==WM_XBUTTONUP){const auto x=GET_XBUTTON_WPARAM(wp);
            client_button=x==XBUTTON1?4u:x==XBUTTON2?5u:0u;client_pressed=message==WM_XBUTTONDOWN;}
        if(client_button){if(client_pressed)++frame->client_down[client_button-1];else ++frame->client_up[client_button-1];}
        if(message==WM_LBUTTONDOWN)++frame->down;if(message==WM_LBUTTONUP)++frame->up;
        if(message==WM_ENTERSIZEMOVE||message==WM_EXITSIZEMOVE){LARGE_INTEGER observed{};QueryPerformanceCounter(&observed);
            const auto qpc=observed.QuadPart>0?static_cast<std::uint64_t>(observed.QuadPart):0;
            if(message==WM_ENTERSIZEMOVE){frame->native_start_qpc=qpc;++frame->starts;}else{frame->native_end_qpc=qpc;++frame->ends;}}
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
    const bool desktop_before=desktop();const auto foreground_before=GetForegroundWindow();
    const SHORT before=GetAsyncKeyState(code);SHORT first=before,last=before;unsigned observations{};
    LARGE_INTEGER send_before{},send_after{};QueryPerformanceCounter(&send_before);SetLastError(0);
    const UINT inserted=desktop_before?SendInput(1,&packet,sizeof(packet)):0;
    const DWORD error=inserted==1?0:GetLastError();QueryPerformanceCounter(&send_after);
    if(inserted==1&&code==VK_CONTROL)s.control=pressed;
    HWND changed_foreground{};DWORD changed_pid{},changed_tid{};std::array<wchar_t,128> changed_class{};bool class_known=false,foreground_changed=false;
    const bool observed=inserted==1&&wait([&]{
        ++observations;last=GetAsyncKeyState(code);if(observations==1)first=last;const auto foreground_now=GetForegroundWindow();
        if(!foreground_changed&&foreground_now!=foreground_before){
            foreground_changed=true;
            changed_foreground=foreground_now;changed_tid=GetWindowThreadProcessId(changed_foreground,&changed_pid);
            class_known=GetClassNameW(changed_foreground,changed_class.data(),static_cast<int>(changed_class.size()))>0;
        }
        return ((last&0x8000)!=0)==pressed;
    },500);
    // Only the fixed key this guest TEST driver just attempted is observed.
    // No extra packet/retry, keyboard hook or product key-content collection.
    if(code==VK_CONTROL||!observed)s.log.emit("test_key_observation",
        "\"test_virtual_key\":"+std::to_string(code)+",\"requested_down\":"+(pressed?"true":"false")+
        ",\"input_desktop_before\":"+(desktop_before?"true":"false")+",\"SendInput_called\":"+(desktop_before?"true":"false")+
        ",\"SendInput_return\":"+std::to_string(inserted)+",\"SendInput_error_on_failure\":"+std::to_string(error)+
        ",\"send_before_qpc\":"+std::to_string(send_before.QuadPart)+",\"send_after_qpc\":"+std::to_string(send_after.QuadPart)+
        ",\"async_before_raw_SHORT\":"+std::to_string(before)+",\"async_first_raw_SHORT\":"+std::to_string(first)+",\"async_last_raw_SHORT\":"+std::to_string(last)+
        ",\"bounded_observations\":"+std::to_string(observations)+",\"requested_state_observed\":"+(observed?"true":"false")+
        ",\"foreground_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_before))+
        ",\"foreground_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
        ",\"foreground_change_observed\":"+(foreground_changed?"true":"false")+
        ",\"first_changed_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(changed_foreground))+
        ",\"first_changed_foreground_pid\":"+std::to_string(changed_pid)+",\"first_changed_foreground_tid\":"+std::to_string(changed_tid)+
        ",\"first_changed_foreground_class\":"+(class_known?quote(utf8(changed_class.data())):"null")+
        ",\"single_packet_no_retransmission\":true,\"actual_key_delivery_or_UIPI_cause_not_inferred\":true");
    if(inserted!=1)throw std::runtime_error("test_key_input_failed");
    if(!observed)throw std::runtime_error("test_key_state_not_observed");
}
bool authorized_source(State&,HWND);
std::uint64_t number(const std::string&,std::string_view);
std::uint64_t process_birth(HANDLE);
void normal_release_with_final_cursor(State&,HWND,std::uint64_t generation=0);
void button(State& s,bool pressed){
    // A failed insertion remains UNKNOWN and keeps the send ledger pending.
    // Do not let an outer abort attempt a second UP for that same test DOWN.
    if(!pressed&&s.left&&s.left_up_attempted)throw std::runtime_error("same_test_DOWN_UP_already_attempted_no_retransmission");
    if(!pressed&&s.left){
        POINT point{};
        if(GetCursorPos(&point)){
            const HWND root=GetAncestor(WindowFromPoint(point),GA_ROOT);
            // Use the actually observed single-release variant consistently
            // for exact foreground fixture/application roots, including abort
            // receipt checks. Native/UI/unknown-context cleanup keeps its old
            // standalone release; never invent a target or move the cursor.
            if(root&&GetForegroundWindow()==root&&authorized_source(s,root)){
                normal_release_with_final_cursor(s,root);return;
            }
        }
    }
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
    try{inject(packet,&delivered,&error);}catch(...){if(!pressed&&delivered!=UINT_MAX)s.left_up_attempted=true;receipt();throw;}
    s.left=pressed;
    s.left_up_attempted=!pressed;
    const auto qpc=before.QuadPart>0?static_cast<std::uint64_t>(before.QuadPart):0;
    if(pressed)s.left_down_send_qpc=qpc;else s.left_up_send_qpc=qpc;
    receipt();
}
product::GuiContext fixture_source_gui_context(State&,std::size_t,std::string&);
product::GuiContext release_source_gui_context(State& s,HWND h,std::string& why){
    for(std::size_t index=0;index<s.frames.size();++index)if(source_window(s,index)==h)
        return fixture_source_gui_context(s,index,why);
    // LayoutFixtures have their own exact owns() boundary at their caller,
    // not one of State's three source slots. Preserve ONLY the previous raw
    // owner query there; never grant a Console/foreground fallback by class.
    product::GuiContext result;const auto tid=h?GetWindowThreadProcessId(h,nullptr):0;
    if(tid){SetLastError(ERROR_SUCCESS);result.owner_succeeded=GetGUIThreadInfo(tid,&result.actual)!=FALSE;
        result.owner_error=result.owner_succeeded?ERROR_SUCCESS:GetLastError();
        if(result.owner_succeeded)result.mode=product::GuiContextMode::OwnerThread;}
    why="original_non_slot_owner_thread_query_no_foreground_fallback";return result;
}
product::guest::CapturedReleaseFacts release_capture_facts(State& s,HWND h,HWND hit,std::uint64_t expected,
    product::GuiContext* observed_source=nullptr,std::string* source_reason=nullptr){
    product::guest::CapturedReleaseFacts facts;facts.expected_generation=expected;
    facts.source=reinterpret_cast<std::uintptr_t>(h);DWORD source_pid{};
    facts.source_tid=GetWindowThreadProcessId(h,&source_pid);facts.source_pid=source_pid;facts.hit_root=reinterpret_cast<std::uintptr_t>(hit);
    facts.product_pid=s.product_pid;facts.product_identity=s.process&&GetProcessId(s.process)==s.product_pid&&
        WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&process_birth(s.process)!=0;
    std::istringstream lines(product_log_current(s));std::string row;
    while(std::getline(lines,row)){
        if(!row.ends_with('}')||number(row,"\"process_id\":")!=s.product_pid||
           row.find("\"source_sha\":"+quote(s.product_source))==row.npos)continue;
        if(row.find("\"type\":\"gesture_input_resource\"")==row.npos)continue;
        const auto generation=number(row,"\"generation\":");
        if(row.find("\"fact\":\"capture_ready\"")!=row.npos){
            // Any later READY supersedes the earlier generation, even when
            // its source differs. Do not resurrect an old matching window.
            facts.generation=generation;facts.ready_source=number(row,"\"source\":");
            facts.ready_source_pid=number(row,"\"source_pid\":");facts.ready_source_tid=number(row,"\"source_tid\":");
            facts.overlay=number(row,"\"overlay\":");facts.ready_actual_capture=number(row,"\"capture_actual_after\":");
            facts.ready_identity=number(row,"\"resource_observed_qpc\":")>=s.left_down_send_qpc&&s.left_down_send_qpc;
            facts.ready_attempt_succeeded=row.find("\"capture_attempted\":true")!=row.npos&&
                row.find("\"capture_failure\":0,")!=row.npos&&row.find("\"attach_succeeded\":true")!=row.npos;
            facts.not_retired=true;
        }else if(generation&&generation==facts.generation&&
            (row.find("\"fact\":\"isolation_gone\"")!=row.npos||row.find("\"fact\":\"capture_lost\"")!=row.npos||
             row.find("\"fact\":\"capture_failure\"")!=row.npos||row.find("\"fact\":\"context_lost\"")!=row.npos||
             row.find("\"fact\":\"resource_failure\"")!=row.npos||row.find("\"fact\":\"deadline\"")!=row.npos||
             row.find("\"fact\":\"legacy_left_up\"")!=row.npos||row.find("\"fact\":\"capture_released\"")!=row.npos||
             row.find("\"fact\":\"thread_detached\"")!=row.npos||
             (row.find("\"fact\":\"raw_mouse\"")!=row.npos&&(number(row,"\"raw_button_flags\":")&RI_MOUSE_LEFT_BUTTON_UP))))facts.not_retired=false;
    }
    const HWND overlay=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(facts.overlay));DWORD overlay_pid{};
    facts.overlay_tid=overlay?GetWindowThreadProcessId(overlay,&overlay_pid):0;facts.overlay_pid=overlay_pid;
    facts.product_identity=facts.product_identity&&IsWindow(overlay)&&GetAncestor(overlay,GA_ROOT)==overlay;
    std::string why;const auto context=release_source_gui_context(s,h,why);
    if(observed_source)*observed_source=context;if(source_reason)*source_reason=why;
    const auto& source=context.actual;GUITHREADINFO owner{sizeof(owner)};
    facts.source_gui=context.known();
    facts.owner_gui=facts.overlay_tid&&GetGUIThreadInfo(static_cast<DWORD>(facts.overlay_tid),&owner)!=FALSE;
    facts.source_capture=reinterpret_cast<std::uintptr_t>(source.hwndCapture);facts.owner_capture=reinterpret_cast<std::uintptr_t>(owner.hwndCapture);
    facts.gui_idle_except_capture=!source.hwndMoveSize&&!source.hwndMenuOwner&&!owner.hwndMoveSize&&!owner.hwndMenuOwner&&
        !(source.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE))&&
        !(owner.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));
    return facts;
}
void normal_release_with_final_cursor(State& s,HWND h,std::uint64_t generation){
    // TEST DRIVER ONLY. Compare one serial final-MOVE/UP insertion with the
    // preserved standalone-UP failures. There is exactly ONE UP, no retry and
    // no product termination fact follows from SendInput's insertion count.
    if(!s.left||s.left_up_attempted)throw std::runtime_error("normal_release_pair_missing_or_UP_already_attempted");
    POINT point{},after{};const int x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN),
        width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const bool point_known=GetCursorPos(&point)!=FALSE;const HWND hit=point_known?GetAncestor(WindowFromPoint(point),GA_ROOT):nullptr;
    product::GuiContext source_context;std::string source_context_reason;
    const auto capture=release_capture_facts(s,h,hit,generation,&source_context,&source_context_reason);const bool shield_hit=product::guest::captured_release_hit_allowed(capture);
    const bool captured_source_hit=hit==h&&product::guest::captured_release_route_allowed(capture);
    const bool captured_route=shield_hit||captured_source_hit;
    const bool source_hit=hit==h&&capture.source_gui&&(!capture.source_capture||capture.source_capture==reinterpret_cast<std::uintptr_t>(h));
    s.log.emit("normal_release_route_observation","\"expected_generation\":"+std::to_string(generation)+",\"ready_generation\":"+std::to_string(capture.generation)+
        ",\"source\":"+std::to_string(capture.source)+",\"actual_hit_root\":"+std::to_string(capture.hit_root)+
        ",\"ready_overlay\":"+std::to_string(capture.overlay)+",\"actual_overlay_pid\":"+std::to_string(capture.overlay_pid)+
        ",\"actual_overlay_tid\":"+std::to_string(capture.overlay_tid)+",\"source_capture\":"+std::to_string(capture.source_capture)+
        ",\"overlay_owner_capture\":"+std::to_string(capture.owner_capture)+",\"ready_not_retired\":"+(capture.not_retired?"true":"false")+
        ",\"source_GUI_known\":"+(source_context.known()?"true":"false")+",\"source_GUI_mode\":"+quote(product::gui_context_mode_name(source_context.mode))+
        ",\"source_GUI_identity_reason\":"+quote(source_context_reason)+",\"source_GUI_owner_succeeded\":"+(source_context.owner_succeeded?"true":"false")+
        ",\"source_GUI_owner_error\":"+std::to_string(source_context.owner_error)+",\"source_GUI_foreground_succeeded\":"+(source_context.foreground_succeeded?"true":"false")+
        ",\"source_GUI_foreground_error\":"+std::to_string(source_context.foreground_error)+",\"source_GUI_foreground_before\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(source_context.foreground_before))+",\"source_GUI_foreground_after\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(source_context.foreground_after))+
        ",\"exact_current_product_shield_hit\":"+(shield_hit?"true":"false")+",\"direct_source_hit\":"+(source_hit?"true":"false")+
        ",\"exact_source_spatial_hit_with_current_product_capture\":"+(captured_source_hit?"true":"false")+
        ",\"UP_SendInput_not_yet_called\":true");
    if(!desktop()||!authorized_source(s,h)||GetForegroundWindow()!=h||!point_known||(!source_hit&&!captured_route)||width<=0||height<=0||
        point.x<x||point.y<y||static_cast<long long>(point.x)>=static_cast<long long>(x)+width||
        static_cast<long long>(point.y)>=static_cast<long long>(y)+height)
        throw std::runtime_error("normal_release_actual_cursor_or_context_unavailable");
    std::array<INPUT,2> packets{};for(auto& packet:packets){packet.type=INPUT_MOUSE;packet.mi.dwExtraInfo=0x50425631;}
    packets[0].mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    packets[0].mi.dx=static_cast<LONG>(((static_cast<long long>(point.x-x)*2+1)*65536)/(static_cast<long long>(width)*2));
    packets[0].mi.dy=static_cast<LONG>(((static_cast<long long>(point.y-y)*2+1)*65536)/(static_cast<long long>(height)*2));
    packets[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    // The route-observation log is disk IO, not a lease on its earlier facts.
    // Recheck actual hit/capture/identity once immediately before insertion.
    POINT fresh_point{};GUITHREADINFO fresh_owner{sizeof(fresh_owner)};DWORD fresh_pid{};
    const HWND expected_hit=hit;const HWND expected_capture=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(capture.overlay));
    std::string fresh_context_reason;
    const auto fresh_context=release_source_gui_context(s,h,fresh_context_reason);const auto& fresh_source=fresh_context.actual;
    const bool current_source=GetWindowThreadProcessId(h,&fresh_pid)==capture.source_tid&&fresh_pid==capture.source_pid&&fresh_context.known();
    const bool current_capture=captured_route?current_source&&GetWindowThreadProcessId(expected_capture,&fresh_pid)==capture.overlay_tid&&
        fresh_pid==s.product_pid&&GetGUIThreadInfo(static_cast<DWORD>(capture.overlay_tid),&fresh_owner)&&
        fresh_source.hwndCapture==expected_capture&&fresh_owner.hwndCapture==expected_capture&&
        !fresh_source.hwndMoveSize&&!fresh_source.hwndMenuOwner&&!fresh_owner.hwndMoveSize&&!fresh_owner.hwndMenuOwner:
        current_source&&(!fresh_source.hwndCapture||fresh_source.hwndCapture==h);
    if(!current_capture||!authorized_source(s,h)||GetForegroundWindow()!=h||!GetCursorPos(&fresh_point)||
       fresh_point.x!=point.x||fresh_point.y!=point.y||GetAncestor(WindowFromPoint(fresh_point),GA_ROOT)!=expected_hit)
        throw std::runtime_error("normal_release_route_changed_after_observation_before_UP");
    LARGE_INTEGER before{},finished{};QueryPerformanceCounter(&before);
    // Even a partial insertion must never result in an automatic second UP.
    s.left_up_attempted=true;s.left_up_send_qpc=before.QuadPart>0?before.QuadPart:0;
    SetLastError(0);const UINT inserted=SendInput(static_cast<UINT>(packets.size()),packets.data(),sizeof(INPUT));
    const DWORD error=inserted==packets.size()?0:GetLastError();QueryPerformanceCounter(&finished);
    if(inserted==packets.size())s.left=false;
    const bool after_known=GetCursorPos(&after)!=FALSE;
    s.log.emit("test_button_send","\"requested\":\"UP\",\"variant\":\"one_serial_final_absolute_MOVE_then_single_UP\",\"UP_packet_count\":1,\"prior_standalone_UP_not_retried\":true,"+
        std::string("\"send_before_qpc\":")+std::to_string(s.left_up_send_qpc)+",\"send_after_qpc\":"+std::to_string(finished.QuadPart>0?finished.QuadPart:0)+
        ",\"SendInput_called\":true,\"SendInput_requested\":2,\"SendInput_return\":"+std::to_string(inserted)+",\"SendInput_error_on_failure\":"+std::to_string(error)+
        ",\"mouse_flags\":["+std::to_string(packets[0].mi.dwFlags)+","+std::to_string(packets[1].mi.dwFlags)+"],\"actual_cursor_before\":["+
        std::to_string(point.x)+","+std::to_string(point.y)+"],\"actual_cursor_after\":"+(after_known?"["+std::to_string(after.x)+","+std::to_string(after.y)+"]":"null")+
        ",\"driver_left_outstanding_before\":true,\"driver_left_outstanding_after\":"+(s.left?"true":"false")+",\"delivery_not_inferred\":true,\"physical_button_held_not_inferred\":true");
    if(inserted!=packets.size())throw std::runtime_error("normal_release_serial_insertion_partial_delivery_UNKNOWN_no_UP_retry");
}
std::optional<POINT> hit_point(HWND h,int required,std::string* failure_facts=nullptr,bool terminal_drag_bar=false){
    const auto started=GetTickCount64();RECT p{};DWORD root_pid{};
    SetLastError(0);const bool positioned=GetWindowRect(h,&p)!=FALSE;const DWORD positioning_error=positioned?0:GetLastError();
    SetLastError(0);const DWORD root_tid=GetWindowThreadProcessId(h,&root_pid),identity_error=root_tid?0:GetLastError();
    if(!positioned||!root_tid){
        if(failure_facts)*failure_facts="{\"positioning_known\":"+std::string(positioned?"true":"false")+
            ",\"positioning_error\":"+std::to_string(positioning_error)+",\"source_tid\":"+std::to_string(root_tid)+
            ",\"source_identity_error\":"+std::to_string(identity_error)+",\"samples\":[],\"input_sent\":false}";
        return {};
    }
    auto caption=required==HTCAPTION?native_caption_observation(h,{p.left,p.top}):NativeCaptionObservation{};
    std::array<POINT,24> candidates{};std::size_t candidate_count{};std::string candidate_rows="[";
    const auto candidate=[&](std::int64_t x,std::int64_t y){
        if(x<p.left||x>=p.right||y<p.top||y>=p.bottom||candidate_count>=candidates.size())return;
        const POINT point{static_cast<LONG>(x),static_cast<LONG>(y)};
        for(std::size_t n=0;n<candidate_count;++n)if(candidates[n].x==point.x&&candidates[n].y==point.y)return;
        candidates[candidate_count++]=point;
        if(candidate_count>1)candidate_rows+=',';candidate_rows+="["+std::to_string(point.x)+","+std::to_string(point.y)+"]";
    };
    std::string candidate_mode;bool actual_button_band=false;
    if(required==HTBOTTOM){
        candidate_mode="existing_bottom_eight_pixels";
        for(std::int64_t y=static_cast<std::int64_t>(p.bottom)-8;y<p.bottom;++y)candidate((static_cast<std::int64_t>(p.left)+p.right)/2,y);
    }else if(required==HTRIGHT){
        // The existing mixed-application shared-edge case also uses this
        // helper. Keep its actual right-edge query, not a caption substitute.
        candidate_mode="right_edge_eight_pixels";
        for(std::int64_t x=static_cast<std::int64_t>(p.right)-8;x<p.right;++x)candidate(x,(static_cast<std::int64_t>(p.top)+p.bottom)/2);
    }else if(required==HTCAPTION){
        const std::int64_t width=static_cast<std::int64_t>(p.right)-p.left,height=static_cast<std::int64_t>(p.bottom)-p.top;
        const auto& buttons=caption.buttons;
        actual_button_band=SUCCEEDED(caption.hresult)&&buttons.left>=0&&buttons.top>=0&&buttons.left<buttons.right&&buttons.top<buttons.bottom&&
            buttons.right<=width&&buttons.bottom<=height;
        const UINT dpi=caption.dpi;
        const std::int64_t border_x=dpi?GetSystemMetricsForDpi(SM_CXSIZEFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi):0;
        const std::int64_t border_y=dpi?GetSystemMetricsForDpi(SM_CYSIZEFRAME,dpi)+GetSystemMetricsForDpi(SM_CXPADDEDBORDER,dpi):0;
        const std::int64_t left=static_cast<std::int64_t>(p.left)+std::max<std::int64_t>(1,border_x),
            right=actual_button_band?static_cast<std::int64_t>(p.left)+buttons.left:static_cast<std::int64_t>(p.right)-std::max<std::int64_t>(1,border_x);
        const std::int64_t top=static_cast<std::int64_t>(p.top)+(actual_button_band?buttons.top:border_y),
            bottom=actual_button_band?static_cast<std::int64_t>(p.top)+buttons.bottom:top+(dpi?GetSystemMetricsForDpi(SM_CYCAPTION,dpi):0);
        candidate_mode=actual_button_band?"DWM_button_bounds_plus_DPI_border_edge_candidates_8x3":"DPI_frame_caption_edge_candidates_8x3";
        if(terminal_drag_bar){
            // Run1acb's real pixels/child facts show that the former first
            // point was over XAML/tab content, not Terminal's actual drag bar.
            // Its root's external HTCAPTION is not proof of native dragging.
            // Prefer ONE actual, unique child band; all hit/identity/DWM gates
            // below and the real native START gate remain required.
            candidate_mode="actual_unique_Terminal_drag_bar_child_center_before_DWM_buttons";
            const auto bar=FindWindowExW(h,nullptr,L"DRAG_BAR_WINDOW_CLASS",nullptr);
            DWORD bar_pid{};const auto bar_tid=bar?GetWindowThreadProcessId(bar,&bar_pid):0;RECT band{};
            if(bar&&bar_tid==root_tid&&bar_pid==root_pid&&GetAncestor(bar,GA_ROOT)==h&&IsWindowVisible(bar)&&
                !FindWindowExW(h,bar,L"DRAG_BAR_WINDOW_CLASS",nullptr)&&GetWindowRect(bar,&band)&&
                band.left>=p.left&&band.top>=p.top&&band.right<=p.right&&band.bottom<=p.bottom&&
                band.left<band.right&&band.top<band.bottom){
                const auto safe_right=std::min<std::int64_t>(band.right,right);
                if(band.left<safe_right)candidate((static_cast<std::int64_t>(band.left)+safe_right)/2,
                    (static_cast<std::int64_t>(band.top)+band.bottom)/2);
            }
        }else{
        // A single small candidate set, not a raster or a retry. These are
        // proposed points only: the SAME actual child hit and DWM exclusion
        // gates below still have to succeed before any input is sent.
        if(left<right&&top<bottom){
            // The earlier all-fraction grid omitted both narrow horizontal
            // caption gaps and the top border's half-height. Include those
            // actual metric/boundary points without increasing the 24 budget.
            const std::int64_t span=right-left;
            const std::array<std::int64_t,8> xs{left+1,right-1,left+span/2,left+span*3/4,
                left+span/4,left+span*7/8,left+span/8,left+span*5/8};
            const std::array<std::int64_t,3> ys{static_cast<std::int64_t>(p.top)+std::max<std::int64_t>(1,border_y/2),
                top+(bottom-top)/2,top+(bottom-top)*3/4};
            for(const auto y:ys)for(const auto x:xs)candidate(x,y);
        }
        }
    }else candidate_mode="unsupported_nonclient_region_no_candidate";
    candidate_rows+=']';
    const auto caption_at_start=caption;std::uint64_t points{},foreign_hits{},coordinate_rejections{},guard_rejections{},queries{},query_failures{},wrong_hits{},fresh_guard_rejections{};
    std::array<std::string,8> sample_kinds{};std::size_t samples{};std::string sample_rows="[";
    const auto sample=[&](const char* reason,POINT point,HWND actual,HWND root,DWORD pid,DWORD tid,bool attempted,bool read,DWORD_PTR hit,DWORD error,const NativeCaptionObservation& facts){
        if(!failure_facts||samples>=sample_kinds.size())return;
        const std::string kind=std::string(reason)+":"+(read?std::to_string(static_cast<LRESULT>(hit)):"unknown");
        for(std::size_t index=0;index<samples;++index)if(sample_kinds[index]==kind)return;
        sample_kinds[samples]=kind;if(samples++)sample_rows+=',';
        sample_rows+="{\"reason\":"+quote(reason)+",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+
            "],\"hit_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(actual))+",\"hit_root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+
            ",\"hit_pid\":"+(tid?std::to_string(pid):"null")+",\"hit_tid\":"+(tid?std::to_string(tid):"null")+
            ",\"WM_NCHITTEST_attempted\":"+(attempted?"true":"false")+",\"WM_NCHITTEST_returned\":"+(attempted?(read?"true":"false"):"null")+
            ",\"actual_hit\":"+(read?std::to_string(static_cast<LRESULT>(hit)):"null")+",\"query_error\":"+(attempted?std::to_string(error):"null")+
            ",\"caption_facts\":"+(required==HTCAPTION?native_caption_json(facts):"null")+"}";
    };
    for(std::size_t candidate_index=0;candidate_index<candidate_count;++candidate_index){++points;DWORD_PTR hit{};const POINT point=candidates[candidate_index];const LONG x=point.x,y=point.y;const HWND actual=WindowFromPoint(point),actual_root=actual?GetAncestor(actual,GA_ROOT):nullptr;DWORD actual_pid{};
        const DWORD actual_tid=actual?GetWindowThreadProcessId(actual,&actual_pid):0;caption.point=point;
        if(!actual||actual_root!=h||!actual_tid||actual_pid!=root_pid){++foreign_hits;sample("hit_not_same_source_root_process",point,actual,actual_root,actual_pid,actual_tid,false,false,0,0,caption);continue;}
        // The production adapter queries the actual hit child, not an idealized
        // root. Chromium/custom title bars can legitimately return HTCAPTION.
        if(x<std::numeric_limits<short>::min()||x>std::numeric_limits<short>::max()||
           y<std::numeric_limits<short>::min()||y>std::numeric_limits<short>::max()){++coordinate_rejections;sample("signed_message_coordinates_unavailable",point,actual,actual_root,actual_pid,actual_tid,false,false,0,0,caption);continue;}
        if(required==HTCAPTION&&caption.result()!=v1::CaptionHit::Caption){++guard_rejections;sample("initial_caption_guard_rejected",point,actual,actual_root,actual_pid,actual_tid,false,false,0,0,caption);continue;}
        ++queries;SetLastError(0);const bool read=SendMessageTimeoutW(actual,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(x),static_cast<short>(y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&hit)!=0;
        const DWORD error=read?0:GetLastError();
        if(!read){++query_failures;sample("actual_hit_query_failed",point,actual,actual_root,actual_pid,actual_tid,true,false,hit,error,caption);continue;}
        if(static_cast<int>(hit)!=required){++wrong_hits;sample("actual_hit_different",point,actual,actual_root,actual_pid,actual_tid,true,true,hit,0,caption);continue;}
        if(required==HTCAPTION){const auto fresh=native_caption_observation(h,point);if(fresh.result()!=v1::CaptionHit::Caption){++fresh_guard_rejections;sample("fresh_caption_guard_rejected",point,actual,actual_root,actual_pid,actual_tid,true,true,hit,0,fresh);continue;}}
        if(terminal_drag_bar&&failure_facts)*failure_facts="{\"candidate_mode\":"+quote(candidate_mode)+
            ",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"],\"hit_hwnd\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(actual))+",\"actual_hit\":"+std::to_string(static_cast<LRESULT>(hit))+
            ",\"native_START_or_product_authority_inferred\":false}";
        return point;
    }
    if(failure_facts){
        std::array<wchar_t,256> title{};const int title_chars=GetWindowTextW(h,title.data(),static_cast<int>(title.size()));
        *failure_facts="{\"source_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+",\"source_pid\":"+std::to_string(root_pid)+",\"source_tid\":"+std::to_string(root_tid)+
            ",\"positioning\":"+rect({p.left,p.top,p.right,p.bottom})+",\"required_hit\":"+std::to_string(required)+",\"candidate_mode\":"+quote(candidate_mode)+
            ",\"candidate_screen_points\":"+candidate_rows+",\"candidate_count\":"+std::to_string(candidate_count)+",\"actual_query_budget\":"+
            std::to_string(required==HTCAPTION?24:required==HTBOTTOM||required==HTRIGHT?8:0)+",\"actual_DWM_button_band_used\":"+(actual_button_band?"true":"false")+",\"points_examined\":"+std::to_string(points)+
            ",\"foreign_or_unknown_hit_count\":"+std::to_string(foreign_hits)+",\"coordinate_rejections\":"+std::to_string(coordinate_rejections)+",\"initial_guard_rejections\":"+std::to_string(guard_rejections)+
            ",\"actual_hit_query_count\":"+std::to_string(queries)+",\"query_failures\":"+std::to_string(query_failures)+",\"different_hit_results\":"+std::to_string(wrong_hits)+
            ",\"fresh_guard_rejections\":"+std::to_string(fresh_guard_rejections)+",\"elapsed_ms\":"+std::to_string(GetTickCount64()-started)+
            ",\"source_title\":"+(title_chars>0?quote(utf8(std::wstring(title.data(),title_chars))):"null")+",\"title_empty_or_unknown\":"+(title_chars>0?"false":"true")+
            ",\"caption_at_search_start\":"+(required==HTCAPTION?native_caption_json(caption_at_start):"null")+",\"samples\":"+sample_rows+
            "],\"sample_limit\":8,\"samples_are_distinct_existing_search_outcomes\":true,\"caption_facts_cached_unless_fresh_guard_sample\":true,\"input_sent\":false,\"permission_override\":false}";
    }
    return {};
}
bool own(State&,HWND);
bool authorized_source(State&,HWND);
bool launch(State&,std::wstring,bool,DWORD*);
std::string record(State&,std::size_t,std::string_view,std::uint64_t);
bool activation_layout_idle();
bool owned_client_activation_setup(State& s,HWND h){
    // One TEST-only activation click on a blank root created by this fixture.
    // Never attach queues, retry SetForegroundWindow, or target real apps.
    bool attempted=false,up_attempted=false,delivered=false;std::string why="not_started",status;
    std::string raw_before="null";
    POINT point{};HWND prior_foreground{},hit{};DWORD status_exit=STILL_ACTIVE;
    unsigned before_down{},before_up{},after_down{},after_up{};Frame* frame{};
    const auto modifiers_up=[] {return !(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_RBUTTON)&0x8000)&&
        !(GetAsyncKeyState(VK_MBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_XBUTTON1)&0x8000)&&!(GetAsyncKeyState(VK_XBUTTON2)&0x8000)&&
        !(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&
        !((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000);};
    const auto idle_gui=[](DWORD tid){GUITHREADINFO gui{sizeof(gui)};return tid&&GetGUIThreadInfo(tid,&gui)&&
        !gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner&&
        !(gui.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));};
    const auto fresh_native=[&]{
        if(!desktop()||!own(s,h)||!IsWindowVisible(h)||IsIconic(h)||s.left||s.control||!activation_layout_idle()||!modifiers_up())return false;
        DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);if(pid!=GetCurrentProcessId()||!idle_gui(tid))return false;
        const auto foreground=GetForegroundWindow();const auto foreground_tid=foreground?GetWindowThreadProcessId(foreground,nullptr):0;
        return foreground&&idle_gui(foreground_tid)&&GetForegroundWindow()==foreground;
    };
    try{
        if(!fresh_native())throw std::runtime_error("owned_activation_not_exact_idle_guest_context");
        const auto found=std::find_if(s.frames.begin(),s.frames.end(),[&](const auto& candidate){return candidate.hwnd==h;});
        if(found==s.frames.end())throw std::runtime_error("owned_activation_blank_frame_not_registered");frame=&*found;
        if(!s.process||GetProcessId(s.process)!=s.product_pid||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT||!process_birth(s.process))
            throw std::runtime_error("owned_activation_product_identity_not_live");
        const auto mark=read_product(s).size();const bool requested=launch(s,L"--status",true,&status_exit);
        if(!requested||status_exit||!wait([&]{status=record(s,mark,"status",0);return !status.empty();},1000)||
           !product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0}))
            throw std::runtime_error("owned_activation_product_not_fresh_idle");
        RECT client{};if(!GetClientRect(h,&client)||client.left>=client.right||client.top>=client.bottom)
            throw std::runtime_error("owned_activation_blank_client_unknown");
        point={(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(h,&point)||WindowFromPoint(point)!=h)throw std::runtime_error("owned_activation_client_covered_or_foreign");
        cursor(s,point);hit=WindowFromPoint(point);DWORD_PTR actual_region{};
        if(!fresh_native()||hit!=h||!SendMessageTimeoutW(h,WM_NCHITTEST,0,
            MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&actual_region)||actual_region!=HTCLIENT)
            throw std::runtime_error("owned_activation_fresh_client_hit_not_confirmed");
        prior_foreground=GetForegroundWindow();before_down=frame->client_down[0];before_up=frame->client_up[0];
        // A single real DOWN/UP activates through the target's native client
        // behavior. These are setup packets, not a product gesture or UAT.
        raw_before=raw_mouse_observer_json();
        attempted=true;button(s,true);up_attempted=true;button(s,false);
        delivered=wait([&]{return own(s,h)&&GetForegroundWindow()==h&&frame->client_down[0]==before_down+1&&frame->client_up[0]==before_up+1;},1000);
        why=delivered?"actual_blank_client_click_delivered_and_activated":"owned_activation_click_delivery_or_foreground_not_confirmed";
    }catch(const std::exception& error){why=error.what();
        // Only the UP belonging to a successful setup DOWN may be attempted,
        // once. A partial/failed insertion never licenses retransmission.
        if(attempted&&s.left&&!up_attempted&&!s.left_up_attempted){try{up_attempted=true;button(s,false);}catch(...){}}
    }
    if(frame){after_down=frame->client_down[0];after_up=frame->client_up[0];}
    s.log.emit("owned_foreground_client_activation_setup","\"target\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"scope\":\"exact_fixture_blank_client_only\",\"setup_not_product_gesture\":true,\"attempt_limit\":1"+
        ",\"DOWN_attempted\":"+(attempted?"true":"false")+",\"UP_attempted\":"+(up_attempted?"true":"false")+
        ",\"actual_client_DOWN_UP_delivered\":"+(delivered?"true":"false")+
        ",\"foreground_before_click\":"+std::to_string(reinterpret_cast<std::uintptr_t>(prior_foreground))+
        ",\"foreground_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
        ",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
        ",\"client_screen_point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"]"+
        ",\"client_counts_before\":["+std::to_string(before_down)+","+std::to_string(before_up)+"]"+
        ",\"client_counts_after\":["+std::to_string(after_down)+","+std::to_string(after_up)+"]"+
        ",\"existing_Raw_before_single_setup_click\":"+raw_before+
        ",\"existing_Raw_after_setup_wait\":"+raw_mouse_observer_json()+
        ",\"Raw_does_not_prove_source_or_UP_delivery\":true"+
        ",\"public_status_receipt\":"+quote(status)+",\"reason\":"+quote(why)+",\"AttachThreadInput_used\":false,\"SetForegroundWindow_retry\":false");
    return delivered&&!s.left&&!s.control&&GetForegroundWindow()==h;
}
void foreground(State& s,HWND h){
    if(!authorized_source(s,h))throw std::runtime_error("not_exact_test_created_source");
    const bool result=SetForegroundWindow(h)!=FALSE;
    bool observed=wait([&]{return GetForegroundWindow()==h;},1000);bool click_setup=false;
    if(!observed&&own(s,h)){click_setup=owned_client_activation_setup(s,h);observed=click_setup&&GetForegroundWindow()==h;}
    s.log.check("foreground_exact_owned_source",observed&&authorized_source(s,h),"\"api_return\":"+std::string(result?"true":"false")+
        ",\"actual_foreground_matches\":"+(observed?"true":"false")+",\"SetForegroundWindow_calls\":1,\"automatic_retry\":false"+
        ",\"one_exact_blank_client_activation_setup\":"+(click_setup?"true":"false"));
}
bool launch(State& s,std::wstring args,bool command,DWORD* exit=nullptr){
    std::wstring line=L"\""+s.product.wstring()+L"\""+(args.empty()?L"":L" "+args);STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    const auto launch_before_tick=GetTickCount64();
    if(!CreateProcessW(s.product.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,s.product.parent_path().c_str(),&si,&pi))return false;
    CloseHandle(pi.hThread);if(!command){s.process=pi.hProcess;s.product_pid=pi.dwProcessId;
        s.product_evidence.begin(s.product_pid,process_birth(pi.hProcess),launch_before_tick,s.product_source);s.product_evidence_path=s.product_log;
        return medium(pi.hProcess);}
    const DWORD waited=WaitForSingleObject(pi.hProcess,5000);DWORD code=STILL_ACTIVE;if(waited==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hProcess);if(exit)*exit=code;return waited==WAIT_OBJECT_0&&code==0;
}
std::uint64_t number(const std::string& line,std::string_view field){
    auto at=line.find(field);if(at==std::string::npos)return 0;at+=field.size();std::uint64_t value{};
    for(;at<line.size()&&line[at]>='0'&&line[at]<='9';++at)value=value*10+static_cast<unsigned>(line[at]-'0');return value;
}
std::string record(State& s,std::size_t offset,std::string_view type,std::uint64_t generation=0){
    const auto text=product_log_since(s,offset);std::istringstream stream(text);std::string line,out;
    while(std::getline(stream,line))if(line.ends_with('}')&&line.find("\"type\":"+quote(type))!=std::string::npos&&(!generation||number(line,"\"generation\":")==generation))out=line;return out;
}
std::string actual_up(State& s,std::size_t offset,std::uint64_t generation,int pressed_button=1){
    if(!generation||pressed_button<1||pressed_button>5)return {};
    const auto text=product_log_since(s,offset);
    std::istringstream rows(text);std::string line,observed;
    while(std::getline(rows,line))if(line.ends_with('}')&&line.find("\"type\":\"input_button_observed\"")!=line.npos&&
        line.find("\"kind\":\"UP\"")!=line.npos&&
        number(line,"\"generation\":")==generation&&number(line,"\"button\":")==static_cast<unsigned>(pressed_button)&&
        ((line.find("\"source\":\"WH_MOUSE_LL\"")!=line.npos&&number(line,"\"hook_observed_qpc\":"))||
         (pressed_button==1&&line.find("\"source\":\"shield_queued_legacy\"")!=line.npos&&
          number(line,"\"native_message_time\":")&&number(line,"\"resource_observed_qpc\":"))))observed=line;
    return observed;
}
struct AbortCleanupUpReceipt {
    bool matched{},status_requested{},status_observed{};DWORD status_exit{STILL_ACTIVE};
    std::string status,ended,gone,released;
    std::string json()const{
        return "\"evidence_scope\":\"actual_LL_cleanup_UP_after_abort_not_NormalUp\",\"matching_abort_cleanup_UP\":"+
            std::string(matched?"true":"false")+",\"single_public_status_requested\":"+(status_requested?"true":"false")+
            ",\"public_status_exit\":"+std::to_string(status_exit)+",\"fresh_public_status_observed\":"+(status_observed?"true":"false")+
            ",\"actual_status_receipt\":"+quote(status)+",\"actual_non_NormalUp_end\":"+quote(ended)+
            ",\"actual_abort_resource_cleanup\":"+quote(gone)+",\"actual_capture_cleanup_observation\":"+quote(released)+
            ",\"target_UP_delivery_claimed\":false,\"inflight_completion_inferred\":false";
    }
};
AbortCleanupUpReceipt abort_cleanup_up_receipt(State& s,std::size_t offset,std::uint64_t generation){
    AbortCleanupUpReceipt out;
    if(!generation||s.left||!s.left_up_attempted||!s.left_up_send_qpc||!s.process||
       GetProcessId(s.process)!=s.product_pid||WaitForSingleObject(s.process,0)!=WAIT_TIMEOUT||!process_birth(s.process))return out;
    const product::guest::UpEvidenceIdentity identity{s.product_pid,s.product_source,generation,s.left_up_send_qpc};
    const auto status_mark=read_product(s).size();
    // Exactly one public status command. The bounded wait only reads its
    // actual receipt and resource facts; it never launches a polling process.
    out.status_requested=launch(s,L"--status",true,&out.status_exit);
    out.matched=wait([&]{
        out.status=record(s,status_mark,"status");out.status_observed=!out.status.empty();
        out.ended=record(s,offset,"gesture_end",generation);out.gone.clear();out.released.clear();
        const auto text=product_log_since(s,offset);std::istringstream rows(text);std::string row;
        while(std::getline(rows,row))if(row.find("\"type\":\"gesture_input_resource\"")!=row.npos&&number(row,"\"generation\":")==generation){
            if(row.find("\"fact\":\"isolation_gone\"")!=row.npos)out.gone=row;
            if(row.find("\"fact\":\"capture_released\"")!=row.npos)out.released=row;
        }
        return out.status_requested&&out.status_exit==0&&out.status_observed&&
            product::guest::matching_abort_cleanup_up(out.status,out.ended,out.gone,identity,out.released);
    },1000);
    return out;
}
std::string placement(State& s,std::size_t offset,std::uint64_t generation,Rect expected,bool native){
    const auto text=product_log_since(s,offset);std::istringstream stream(text);std::string line,out;
    while(std::getline(stream,line))if(line.ends_with('}')&&line.find("\"type\":\"placement\"")!=std::string::npos&&
        number(line,"\"generation\":")==generation&&line.find("\"exact\":true")!=std::string::npos&&
        line.find("\"actual\":"+rect(expected))!=std::string::npos&&(!native||line.find("\"native_attempted\":true")!=std::string::npos))out=line;
    return out;
}
struct Gesture{POINT anchor{};std::uint64_t generation{};std::size_t offset{};};
bool application_down_context(State&,HWND,POINT,int,std::string_view);
void application_prepare_barrier(State&,HWND,POINT,int);
product::GuiContext fixture_source_gui_context(State&,std::size_t,std::string&);
// TEST-only: one physical threshold displacement may be needed before a
// passed-through native caption DOWN enters Windows' modal Move loop. It
// never creates a START/END receipt or changes the original intent anchor.
void guest_native_pending_threshold_move(State& s,HWND source,POINT anchor,std::size_t offset,
    bool& attempted,std::uint64_t& pending_generation,const std::function<bool()>& exact,
    const std::function<product::GuiContext()>& observe){
    if(attempted)return;
    const auto pending=record(s,offset,"native_move_pending");if(pending.empty())return;
    const auto generation=number(pending,"\"generation\":");DWORD pid{};const DWORD tid=GetWindowThreadProcessId(source,&pid);
    const auto point="\"down_point\":["+std::to_string(anchor.x)+","+std::to_string(anchor.y)+"]";
    if(!generation||(pending_generation&&pending_generation!=generation)||!exact()||
        number(pending,"\"source\":")!=reinterpret_cast<std::uintptr_t>(source)||
        number(pending,"\"source_pid\":")!=pid||number(pending,"\"source_tid\":")!=tid||!pid||!tid||
        !number(pending,"\"source_key\":")||!s.left_down_send_qpc||
        number(pending,"\"down_hook_qpc\":")<s.left_down_send_qpc||pending.find(point)==pending.npos||
        pending.find("\"down_consumed\":false")==pending.npos||pending.find("\"write_authority\":false")==pending.npos)
        throw std::runtime_error("native_pending_threshold_requires_actual_matching_DOWN_source_receipt");
    pending_generation=generation;
    const auto progressed=[&]{const auto rows=product_log_since(s,offset);std::istringstream lines(rows);std::string row;
        while(std::getline(lines,row))if(row.ends_with('}')&&number(row,"\"generation\":")==generation&&
            (row.find("\"type\":\"gesture_start\"")!=row.npos||row.find("\"fact\":\"capture_ready\"")!=row.npos||
             row.find("\"type\":\"native_move_start\"")!=row.npos))return true;return false;};
    if(progressed())return;
    if(!record(s,offset,"native_move_passthrough_release",generation).empty()||!record(s,offset,"gesture_end",generation).empty())
        throw std::runtime_error("native_pending_threshold_generation_already_released");
    const auto own_native_queue=[&](const product::GuiContext& gui){const auto& actual=gui.actual;return gui.known()&&
        actual.hwndMoveSize==source&&
        (!actual.hwndCapture||actual.hwndCapture==source)&&!actual.hwndMenuOwner;};
    const auto gui=observe();
    // Only an actual MoveSize owner establishes entry into the native loop.
    // Source capture alone can precede the threshold and is not a START.
    if(own_native_queue(gui))return;
    POINT cursor_before{};const bool cursor_known=GetCursorPos(&cursor_before)!=FALSE;
    const auto threshold_context=[&](const product::GuiContext& context){const auto& fact=context.actual;return context.known()&&
        (!fact.hwndCapture||fact.hwndCapture==source)&&!fact.hwndMoveSize&&!fact.hwndMenuOwner&&
        !(fact.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));};
    const bool idle=threshold_context(gui);
    if(!desktop()||!exact()||!s.left||s.left_up_attempted||GetForegroundWindow()!=source||
        !idle||!cursor_known||cursor_before.x!=anchor.x||cursor_before.y!=anchor.y){
        if(progressed())return;
        throw std::runtime_error("native_pending_threshold_source_or_idle_context_changed");
    }
    const LONG drag_x=GetSystemMetrics(SM_CXDRAG),drag_y=GetSystemMetrics(SM_CYDRAG);
    const long long left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN),
        right=left+GetSystemMetrics(SM_CXVIRTUALSCREEN),bottom=top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(drag_x<0||drag_y<0||drag_x>128||drag_y>128||anchor.y<top||anchor.y>=bottom)
        throw std::runtime_error("native_pending_threshold_system_drag_metrics_or_desktop_out_of_bounds");
    const LONG delta=drag_x+1;const long long x=static_cast<long long>(anchor.x)+delta<right?
        static_cast<long long>(anchor.x)+delta:static_cast<long long>(anchor.x)-delta;
    if(x<left||x>=right||x<std::numeric_limits<LONG>::min()||x>std::numeric_limits<LONG>::max())
        throw std::runtime_error("native_pending_threshold_destination_outside_guest_desktop");
    const POINT destination{static_cast<LONG>(x),anchor.y};
    if(progressed())return;
    const auto before_send=observe();if(own_native_queue(before_send))return;
    if(!exact()||GetForegroundWindow()!=source||!s.left||s.left_up_attempted)
        throw std::runtime_error("native_pending_threshold_source_revoked_before_single_MOVE");
    if(!threshold_context(before_send))
        throw std::runtime_error("native_pending_threshold_GUI_changed_before_single_MOVE");
    attempted=true;cursor(s,destination);
    s.log.emit("native_pending_test_threshold_MOVE","\"generation\":"+std::to_string(generation)+
        ",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(source))+",\"source_pid\":"+std::to_string(pid)+
        ",\"source_tid\":"+std::to_string(tid)+",\"original_DOWN_anchor\":["+std::to_string(anchor.x)+","+std::to_string(anchor.y)+
        "],\"destination\":["+std::to_string(destination.x)+","+std::to_string(destination.y)+"],\"system_drag_metrics\":["+
        std::to_string(drag_x)+","+std::to_string(drag_y)+"],\"actual_native_pending\":"+quote(pending)+
        ",\"actual_source_capture_before_MOVE\":"+std::to_string(reinterpret_cast<std::uintptr_t>(before_send.actual.hwndCapture))+
        ",\"actual_source_move_size_before_MOVE\":"+std::to_string(reinterpret_cast<std::uintptr_t>(before_send.actual.hwndMoveSize))+
        ",\"maximum_extra_MOVE_per_DOWN\":1,\"test_driver_only\":true,\"START_or_cancel_or_write_authority_inferred\":false");
}
Gesture down(State& s,std::size_t source,bool ctrl=false,int region=HTCAPTION){
    const auto h=source_window(s,source);foreground(s,h);std::string hit_failure;
    const auto& application=s.applications[source];
    const bool terminal_bar=region==HTCAPTION&&application&&application->kind=="Terminal"&&application->identity&&
        equal(application->identity->window_class,L"CASCADIA_HOSTING_WINDOW_CLASS");
    const auto point=hit_point(h,region,&hit_failure,terminal_bar);
    if(point&&terminal_bar)s.log.emit("application_nonclient_search_choice","\"application\":\"Terminal\",\"facts\":"+hit_failure);
    if(!point)s.log.emit("application_nonclient_search_failure","\"application\":"+quote(s.applications[source]?s.applications[source]->kind:own(s,h)?"OwnedFixture":"Explorer")+",\"facts\":"+hit_failure);
    s.log.check("actual_nonclient_hit",point.has_value(),"\"source\":"+std::to_string(source)+",\"hit\":"+std::to_string(region));
    cursor(s,*point);
    if(s.applications[source])application_prepare_barrier(s,h,*point,region);else Sleep(100);
    if(ctrl)key(s,VK_CONTROL,true);Gesture g{*point,0,read_product(s).size()};button(s,true);
    bool threshold_attempted=false;std::uint64_t pending_generation{};
    const bool observed=wait([&]{g.generation=number(record(s,g.offset,"gesture_start"),"\"generation\":");
        if(g.generation)return !pending_generation||g.generation==pending_generation;
        if(region==HTCAPTION)guest_native_pending_threshold_move(s,h,g.anchor,g.offset,threshold_attempted,pending_generation,
            [&]{return source_window(s,source)==h&&authorized_source(s,h);},[&]{std::string why;return fixture_source_gui_context(s,source,why);});
        return false;});
    if(!observed&&s.applications[source])application_down_context(s,h,*point,region,"after_unobserved_DOWN");
    s.log.check("production_real_down_start",observed,
        "\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation)+
        ",\"native_pending_generation\":"+std::to_string(pending_generation)+",\"single_native_threshold_MOVE_attempted\":"+(threshold_attempted?"true":"false"));return g;
}
void phase(State& s,std::size_t source,const Gesture& g,LONG dx,LONG dy,Rect expected,std::string name,bool native){
    // Deliberately paced input trajectory below the configured speed cutoff;
    // this is not a product retry, cursor sampler or authority workaround.
    if(!authorized_source(s,source_window(s,source)))throw std::runtime_error("test_source_identity_revoked");
    Sleep(60);const auto mark=read_product(s).size();cursor(s,{g.anchor.x+dx,g.anchor.y+dy});std::string receipt;
    const bool ok=wait([&]{receipt=placement(s,mark,g.generation,expected,native);return visible(source_window(s,source))==expected&&!receipt.empty();});
    const auto actual=visible(source_window(s,source));
    s.log.check(std::move(name),ok,"\"source\":"+std::to_string(source)+",\"generation\":"+std::to_string(g.generation)+
        ",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+",\"trajectory_pause_ms\":60,\"production_receipt\":"+quote(receipt));
}
product::GuiContext fixture_source_gui_context(State&,std::size_t,std::string&);
#include "platform/windows/product/v1_guest_hook_observer.h"
void up(State& s,std::size_t source,const Gesture& g,Rect expected,
    product::guest::UpEvidenceMode evidence_mode=product::guest::UpEvidenceMode::DiagnosticEvent){
    const auto mark=read_product(s).size();const auto raw_before=raw_mouse_observer_json();
    // Install after this product instance, so this passive observer is ahead
    // of it for the single planned UP. A missing product callback can now be
    // distinguished from a missing guest hook-chain observation. It is never
    // used as product NormalUp or as permission to send a second UP.
    GuestUpHookObservation hook_observer;
    const bool hook_ready=hook_observer.start();
    bool attempted=false,returned=false,raw_logged=false;std::string end;
    try{
        s.log.check("guest_passive_UP_observer_ready",hook_ready,"\"generation\":"+std::to_string(g.generation)+
            ",\"observation\":"+hook_observer.json());
        normal_release_with_final_cursor(s,source_window(s,source),g.generation);attempted=s.left_up_attempted;returned=true;if(s.control)key(s,VK_CONTROL,false);
        const bool observed=wait([&]{end=record(s,mark,"gesture_end",g.generation);return end.find("\"normal_up\":true")!=std::string::npos;});
        std::string status;bool status_requested=false,status_observed=false;DWORD status_exit=STILL_ACTIVE;
        if(observed&&evidence_mode==product::guest::UpEvidenceMode::StatusSnapshot){
            // A new public command AFTER this same-generation NormalUp, not a
            // cached shutdown receipt and not an implicit diagnostic fallback.
            const auto status_mark=read_product(s).size();status_requested=launch(s,L"--status",true,&status_exit);
            status_observed=status_requested&&wait([&]{status=record(s,status_mark,"status");return !status.empty();},1000);
        }
        Sleep(150);std::string context_reason;const auto context=fixture_source_gui_context(s,source,context_reason);
        const auto& gui=context.actual;const bool gui_read=context.known();const auto actual=visible(source_window(s,source));
        const auto raw_after=raw_mouse_observer_json();const auto actual_LL_UP=actual_up(s,mark,g.generation);
        const product::guest::UpEvidenceIdentity up_identity{s.product_pid,s.product_source,g.generation,s.left_up_send_qpc};
        std::string captured_legacy,captured_gone;
        {const auto text=product_log_since(s,mark);std::istringstream rows(text);std::string row;
            while(std::getline(rows,row))if(row.find("\"type\":\"gesture_input_resource\"")!=row.npos&&number(row,"\"generation\":")==g.generation){
                if(row.find("\"fact\":\"legacy_left_up\"")!=row.npos)captured_legacy=row;
                if(row.find("\"fact\":\"isolation_gone\"")!=row.npos)captured_gone=row;
            }}
        const bool matching_product_UP=product::guest::accepted_up_evidence(evidence_mode,!actual_LL_UP.empty(),
            status_requested&&status_observed&&status_exit==0?std::string_view(status):std::string_view{},up_identity,captured_legacy,captured_gone);
        const std::string up_evidence_fields="\"UP_evidence_mode\":"+quote(evidence_mode==product::guest::UpEvidenceMode::StatusSnapshot?"public_status_snapshot":"diagnostic_event")+
            ",\"public_status_requested\":"+(status_requested?"true":"false")+",\"public_status_command_exit\":"+std::to_string(status_exit)+
            ",\"public_status_observed\":"+(status_observed?"true":"false")+",\"single_UP_send_before_qpc\":"+std::to_string(s.left_up_send_qpc)+
            ",\"matching_product_UP_evidence\":"+(matching_product_UP?"true":"false")+",\"public_status_receipt\":"+(status.empty()?"null":quote(status))+
            ",\"actual_captured_legacy_UP_receipt\":"+(captured_legacy.empty()?"null":quote(captured_legacy))+
            ",\"actual_capture_cleanup_receipt\":"+(captured_gone.empty()?"null":quote(captured_gone));
        const bool hook_stopped=hook_observer.stop();
        s.log.check("guest_passive_UP_observer_removed",hook_stopped,"\"generation\":"+std::to_string(g.generation)+
            ",\"observation\":"+hook_observer.json());
        // Emit before the existing throwing assertion, including when its
        // normal-UP observation failed. Raw remains a fixture-side interval
        // observation, never a source/generation or termination substitute.
        s.log.emit("ordinary_gesture_UP_Raw_observation","\"generation\":"+std::to_string(g.generation)+",\"source_member\":"+std::to_string(source)+
            ",\"phase\":\"after_existing_normal_wait\",\"single_test_UP_attempted\":true,\"single_test_UP_call_returned\":true,\"normal_end_observed\":"+
            (observed?"true":"false")+",\"actual_matching_LL_UP\":"+(actual_LL_UP.empty()?"null":quote(actual_LL_UP))+
            ",\"production_end\":"+(end.empty()?"null":quote(end))+",\"fixture_Raw_before_single_UP\":"+raw_before+",\"fixture_Raw_after_normal_wait\":"+raw_after+
            ",\"passive_head_hook\":"+hook_observer.json()+","+up_evidence_fields+
            ",\"Raw_source_generation_attribution\":\"UNKNOWN\",\"Raw_does_not_change_original_assertion_or_authority\":true");raw_logged=true;
        s.log.check("normal_up_and_no_late_geometry",observed&&matching_product_UP&&actual==expected&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&gui_read&&!gui.hwndCapture,
            "\"generation\":"+std::to_string(g.generation)+",\"expected\":"+rect(expected)+",\"actual\":"+(actual?rect(*actual):"null")+
            ",\"source_thread_capture\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
            ",\"source_GUI_known\":"+(gui_read?"true":"false")+",\"source_GUI_mode\":"+quote(product::gui_context_mode_name(context.mode))+
            ",\"source_GUI_identity_reason\":"+quote(context_reason)+",\"source_GUI_owner_succeeded\":"+(context.owner_succeeded?"true":"false")+
            ",\"source_GUI_owner_error\":"+std::to_string(context.owner_error)+",\"source_GUI_foreground_succeeded\":"+(context.foreground_succeeded?"true":"false")+
            ",\"source_GUI_foreground_error\":"+std::to_string(context.foreground_error)+",\"source_GUI_foreground_before\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(context.foreground_before))+",\"source_GUI_foreground_after\":"+
            std::to_string(reinterpret_cast<std::uintptr_t>(context.foreground_after))+",\"production_end\":"+quote(end)+","+up_evidence_fields);
    }catch(...){
        attempted=s.left_up_attempted;
        const bool hook_stopped=hook_observer.stop();
        if(!raw_logged)try{s.log.emit("ordinary_gesture_UP_Raw_observation","\"generation\":"+std::to_string(g.generation)+",\"source_member\":"+std::to_string(source)+
            ",\"phase\":\"exception_during_existing_UP_observation\",\"single_test_UP_attempted\":"+(attempted?"true":"false")+
            ",\"single_test_UP_call_returned\":"+(returned?"true":"false")+",\"fixture_Raw_before_single_UP\":"+raw_before+
            ",\"fixture_Raw_at_exception\":"+raw_mouse_observer_json()+",\"passive_head_hook\":"+hook_observer.json()+
            ",\"passive_hook_cleanup_confirmed\":"+(hook_stopped?"true":"false")+
            ",\"Raw_source_generation_attribution\":\"UNKNOWN\",\"original_exception_rethrown\":true");}catch(...){}
        throw;
    }
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
    s.retired_explorers.clear();
    // Explorer and application adapters share this read-only Catalog. An app
    // whose normal WM_CLOSE has not completed still needs fresh eligibility
    // and exact birth/identity checks during the final cleanup scope. Dropping
    // the Catalog here must not manufacture a later identity rejection or
    // justify closing that app without its existing admission checks.
    const bool application_cleanup_pending=std::any_of(s.created_applications.begin(),s.created_applications.end(),
        [](const auto& target){return target&&target->accepted&&!target->cleanup_confirmed;});
    if(!application_cleanup_pending)s.target_catalog.reset();
    else s.log.emit("application_cleanup_catalog_retained","\"unconfirmed_exact_application_root_remains\":true,\"identity_checks_unchanged\":true,\"cleanup_PASS_inferred\":false");
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
    if(target.kind=="Terminal"){
        if(equal(target.executable.filename().wstring(),L"WindowsTerminal.exe")){
            if(!equal(fresh->window_class,L"CASCADIA_HOSTING_WINDOW_CLASS"))return reject("terminal_not_exact_modern_Terminal_root_class");
        }else if(!equal(fresh->window_class,L"ConsoleWindowClass")||target.processes.empty()||process->parent!=target.processes.front().pid)
            return reject("terminal_not_exact_ConsoleWindowClass_of_explicit_cmd_child");
    }
    if(actual)*actual=std::move(fresh);if(why)*why="exact_candidate_eligible";return true;
}
product::GuiContext fixture_source_gui_context(State& s,std::size_t source,std::string& why){
    product::GuiContext result;const HWND h=source_window(s,source);const auto& target=s.applications.at(source);
    if(target){
        std::optional<product::Snapshot> fresh;
        if(!target->accepted||!target->identity||!authorized_source(s,h)){
            why="application_not_current_exact_authorized_root";return result;}
        if(!application_fresh(s,*target,h,&fresh,&why)||!fresh)return result;
        if(!product::Catalog::same(*target->identity,*fresh)){why="fresh_application_identity_changed";return result;}
        // Use precisely the production adapter's owner-thread-first rule.
        // Its Console-only foreground fallback still requires real GTI(0),
        // exact active HWND and matching foreground observations on both sides.
        return product::observe_gui_context(*fresh);
    }
    // Preserve the original owned/Explorer test path; no general foreground
    // fallback or eligibility relaxation is introduced for other targets.
    SetLastError(ERROR_SUCCESS);result.owner_succeeded=GetGUIThreadInfo(source_thread(s,source),&result.actual)!=FALSE;
    result.owner_error=result.owner_succeeded?ERROR_SUCCESS:GetLastError();
    if(result.owner_succeeded)result.mode=product::GuiContextMode::OwnerThread;
    why="original_owner_thread_query";return result;
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
bool vscode_own_directory_probe(State& s,const fs::path& directory,std::string_view purpose){
    // Actual Medium fixture writes only a CREATE_NEW file belonging to this
    // RunId. Never repair ACLs, change TEMP, or delete an existing file.
    const bool directory_plain=plain_directory(directory);int rid=-1;bool elevated{};DWORD token_error{};
    const bool actual_medium=medium(GetCurrentProcess(),&rid,&elevated,&token_error);
    const auto file=directory/(L"PaneBind-"+s.run+L"-"+std::wstring(s.configuration.begin(),s.configuration.end())+
        L"-"+std::wstring(purpose.begin(),purpose.end())+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".tmp");
    Handle owned(directory_plain&&actual_medium?CreateFileW(file.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,nullptr):INVALID_HANDLE_VALUE);
    const bool create_attempted=directory_plain&&actual_medium;
    const bool created=owned.value!=INVALID_HANDLE_VALUE;const DWORD create_error=created?0:create_attempted?GetLastError():0;
    const std::string payload="PaneBind exact guest write/read probe "+utf8(s.run);DWORD written{},received{};
    bool wrote=false,flushed=false,rewound=false,read_back=false,closed=false,removed=false;
    DWORD write_error{},flush_error{},seek_error{},read_error{},close_error{},delete_error{};
    if(created){
        wrote=WriteFile(owned.value,payload.data(),static_cast<DWORD>(payload.size()),&written,nullptr)!=FALSE;
        write_error=wrote?0:GetLastError();flushed=FlushFileBuffers(owned.value)!=FALSE;flush_error=flushed?0:GetLastError();
        LARGE_INTEGER origin{};rewound=SetFilePointerEx(owned.value,origin,nullptr,FILE_BEGIN)!=FALSE;seek_error=rewound?0:GetLastError();
        std::array<char,128> actual{};
        read_back=rewound&&ReadFile(owned.value,actual.data(),static_cast<DWORD>(actual.size()),&received,nullptr)!=FALSE;
        read_error=read_back?0:rewound?GetLastError():ERROR_INVALID_FUNCTION;
        read_back=read_back&&received==payload.size()&&std::string_view(actual.data(),received)==payload;
        closed=CloseHandle(owned.value)!=FALSE;close_error=closed?0:GetLastError();if(closed)owned.value=nullptr;
        if(closed){removed=DeleteFileW(file.c_str())!=FALSE;delete_error=removed?0:GetLastError();}
    }
    const bool passed=directory_plain&&actual_medium&&created&&wrote&&written==payload.size()&&flushed&&rewound&&read_back&&closed&&removed;
    s.log.emit("VSCode_Medium_directory_probe","\"purpose\":"+quote(purpose)+",\"directory\":"+quote(utf8(directory.wstring()))+
        ",\"owned_probe_file\":"+quote(utf8(file.wstring()))+",\"result\":"+quote(passed?"PASS":"FAIL")+
        ",\"integrity_rid\":"+std::to_string(rid)+",\"elevated\":"+(elevated?"true":"false")+",\"token_query_error\":"+std::to_string(token_error)+
        ",\"plain_directory\":"+(directory_plain?"true":"false")+",\"create_attempted\":"+(create_attempted?"true":"false")+
        ",\"create_new\":"+(created?"true":"false")+",\"create_error\":"+(create_attempted?std::to_string(create_error):"null")+
        ",\"write\":"+(wrote?"true":"false")+",\"write_error\":"+std::to_string(write_error)+",\"flush\":"+(flushed?"true":"false")+
        ",\"flush_error\":"+std::to_string(flush_error)+",\"rewind\":"+(rewound?"true":"false")+",\"seek_error\":"+std::to_string(seek_error)+
        ",\"exact_read_back\":"+(read_back?"true":"false")+",\"read_error\":"+std::to_string(read_error)+",\"closed\":"+(closed?"true":"false")+
        ",\"close_error\":"+std::to_string(close_error)+",\"owned_file_deleted\":"+(removed?"true":"false")+",\"delete_error\":"+std::to_string(delete_error)+
        ",\"unknown_files_deleted\":false,\"ACL_token_environment_changed\":false,\"application_function_PASS_inferred\":false");
    return passed;
}
void vscode_own_log_facts(State& s,const ApplicationTarget& target){
    if(target.kind!="VSCode")return;
    const fs::path root=target.directory/L"UserData"/L"logs";
    // Read only this exact empty test profile. Bounded names/depth/bytes avoid
    // collecting arbitrary profiles or growing a new tracing framework.
    std::vector<std::pair<fs::path,unsigned>> pending{{root,0}};unsigned visited{},files{},directories{};std::size_t bytes{};bool limited=false;
    const auto read_one=[&](const fs::path& path){
        if(files>=16||bytes>=32768){limited=true;return;}
        const auto attributes=GetFileAttributesW(path.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return;
        ++files;const auto cap=std::min<std::size_t>(8192,32768-bytes);
        Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        const bool opened=file.value!=INVALID_HANDLE_VALUE;const DWORD open_error=opened?0:GetLastError();
        LARGE_INTEGER length{};const bool size_read=opened&&GetFileSizeEx(file.value,&length)!=FALSE;
        std::vector<char> text(cap);DWORD actual{};const bool read_back=opened&&ReadFile(file.value,text.data(),static_cast<DWORD>(cap),&actual,nullptr)!=FALSE;
        const DWORD read_error=read_back?0:opened?GetLastError():open_error;bytes+=actual;
        std::string safe;for(DWORD n=0;n<actual;++n){const unsigned char c=static_cast<unsigned char>(text[n]);safe+=c>=32&&c<127?static_cast<char>(c):' ';}
        s.log.emit("VSCode_own_log_excerpt","\"relative_path\":"+quote(utf8(path.lexically_relative(target.directory).wstring()))+
            ",\"opened\":"+(opened?"true":"false")+",\"open_error\":"+std::to_string(open_error)+",\"read_succeeded\":"+(read_back?"true":"false")+
            ",\"read_error\":"+std::to_string(read_error)+",\"file_size\":"+(size_read?std::to_string(length.QuadPart):"null")+
            ",\"bytes_read\":"+std::to_string(actual)+",\"truncated\":"+(size_read&&length.QuadPart>actual?"true":"false")+
            ",\"ascii_excerpt_non_ascii_replaced\":"+quote(safe)+",\"read_only\":true");
    };
    // Electron documents file logging for Windows child processes. Inspect
    // precisely the new launch's requested file, inside its already-created
    // and Medium-write-tested profile; no environment or sandbox change.
    const auto startup_log=target.directory/L"UserData"/L"electron-debug.log";
    const auto startup_attributes=GetFileAttributesW(startup_log.c_str());
    s.log.emit("VSCode_requested_startup_log","\"relative_path\":\"UserData/electron-debug.log\",\"actual_file_present\":"+
        std::string(startup_attributes!=INVALID_FILE_ATTRIBUTES?"true":"false")+",\"absence_does_not_identify_cause\":true");
    if(plain_directory(target.directory)&&plain_directory(startup_log.parent_path()))read_one(startup_log);
    while(!pending.empty()&&visited<64&&files<16&&directories<16&&bytes<32768){
        const auto [directory,depth]=pending.back();pending.pop_back();
        if(!plain_directory(directory))continue;++directories;std::error_code error;
        fs::directory_iterator entries(directory,error);
        if(error){s.log.emit("VSCode_own_log_read","\"directory\":"+quote(utf8(directory.wstring()))+",\"filesystem_error\":"+std::to_string(error.value()));continue;}
        for(fs::directory_iterator end;entries!=end;entries.increment(error)){
            if(error)break;const auto entry=*entries;
            if(++visited>64||files>=16||bytes>=32768){limited=true;break;}
            const auto attributes=GetFileAttributesW(entry.path().c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
            if(attributes&FILE_ATTRIBUTE_DIRECTORY){if(depth<2&&pending.size()<16)pending.emplace_back(entry.path(),depth+1);else limited=true;continue;}
            if(entry.path().extension()!=L".log")continue;
            read_one(entry.path());
        }
        if(error)s.log.emit("VSCode_own_log_read","\"directory\":"+quote(utf8(directory.wstring()))+",\"filesystem_error\":"+std::to_string(error.value()));
    }
    s.log.emit("VSCode_own_log_summary","\"exact_profile_logs\":"+quote(utf8(root.wstring()))+",\"plain_root_available\":"+(plain_directory(root)?"true":"false")+
        ",\"directories_read\":"+std::to_string(directories)+",\"files_read\":"+std::to_string(files)+",\"total_excerpt_bytes\":"+std::to_string(bytes)+
        ",\"bounded_scan_truncated\":"+(limited||!pending.empty()?"true":"false")+",\"stdout_stderr_redirected\":false,\"inherited_handles\":0,\"no_log_means_cause_UNKNOWN\":true");
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
    const auto mark=read_product(s).size();const bool sent=launch(s,L"--status",true);std::string status;
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
        bool profile_ready=true;
        for(const auto& name:std::array{L"UserData",L"Extensions"}){
            const auto directory=target->directory/name;const bool created=CreateDirectoryW(directory.c_str(),nullptr)!=FALSE;
            const DWORD create_error=created?0:GetLastError();
            s.log.emit("VSCode_explicit_profile_directory","\"path\":"+quote(utf8(directory.wstring()))+",\"created_new\":"+(created?"true":"false")+
                ",\"win32_error\":"+std::to_string(create_error)+",\"existing_directory_not_reused\":true");
            profile_ready=created&&vscode_own_directory_probe(s,directory,equal(name,L"UserData")?"UserData":"Extensions")&&profile_ready;
        }
        std::array<wchar_t,32768> temporary{};const DWORD temporary_length=GetTempPathW(static_cast<DWORD>(temporary.size()),temporary.data());
        const DWORD temp_error=temporary_length&&temporary_length<temporary.size()?0:GetLastError();
        const fs::path temp=temporary_length&&temporary_length<temporary.size()?fs::path(temporary.data()).lexically_normal():fs::path{};
        const bool temp_scope=product::guest::same_exact_directory(temp,L"C:\\Users\\WDAGUtilityAccount\\AppData\\Local\\Temp")||
            product::guest::same_exact_directory(temp,L"C:\\Windows\\Temp");
        s.log.emit("VSCode_actual_TEMP","\"GetTempPath_length\":"+std::to_string(temporary_length)+",\"win32_error\":"+std::to_string(temp_error)+
            ",\"path\":"+quote(utf8(temp.wstring()))+",\"exact_guest_local_TEMP_scope\":"+(temp_scope?"true":"false")+",\"environment_changed\":false");
        profile_ready=temp_scope&&vscode_own_directory_probe(s,temp,"TEMP")&&profile_ready;
        if(!profile_ready){s.log.emit("application_binding","\"application\":\"VSCode\",\"result\":\"BLOCKED\",\"reason\":\"actual_Medium_profile_or_TEMP_write_read_cleanup_failed\",\"input_sent\":false");return target;}
        std::ofstream empty(blank,std::ios::binary);empty.close();if(!empty)throw std::runtime_error("VSCode_exact_empty_document_create_failed");
        const auto startup_log=target->directory/L"UserData"/L"electron-debug.log";
        if(fs::exists(startup_log))throw std::runtime_error("VSCode_new_startup_log_required");
        target->command=L"\""+target->executable.wstring()+L"\" --user-data-dir \""+(target->directory/L"UserData").wstring()+L"\" --extensions-dir \""+(target->directory/L"Extensions").wstring()+L"\" --disable-extensions --new-window --skip-welcome --skip-release-notes \""+blank.wstring()+L"\" --disable-gpu --enable-logging=file --log-file=\""+startup_log.wstring()+L"\"";
    }else if(target->kind=="Terminal"){
        const bool portable_requested=fs::exists(fs::path(L"C:\\PaneBindV1\\Input")/L"Terminal-test.zip");
        if(portable_requested){
            const fs::path root=fs::path(L"C:\\PaneBindV1\\Terminal")/s.run;
            const fs::path configuration=root/std::wstring(s.configuration.begin(),s.configuration.end());
            const fs::path program=configuration/L"terminal-1.25.2733.0";
            target->executable=program/L"WindowsTerminal.exe";
            const DWORD attributes=GetFileAttributesW(target->executable.c_str());
            const auto flag=program/L".portable";const DWORD flag_attributes=GetFileAttributesW(flag.c_str());
            const bool prepared=plain_directory(root.parent_path())&&plain_directory(root)&&plain_directory(configuration)&&plain_directory(program)&&
                marker(program/L"panebind-v1-test-application.marker",s.run)&&
                attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
                flag_attributes!=INVALID_FILE_ATTRIBUTES&&!(flag_attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
                fs::file_size(flag)==0&&hash(target->executable)=="1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1";
            s.log.emit("Terminal_test_asset","\"distribution\":\"official_modern_portable_ZIP\",\"version\":\"1.25.2733.0\",\"prepared\":"+
                std::string(prepared?"true":"false")+",\"exact_guest_program\":"+quote(utf8(target->executable.wstring()))+
                ",\"per_configuration_portable_settings\":true,\"application_binding_not_inferred\":true,\"fallback_to_ConsoleWindowClass\":false");
            if(!prepared){s.log.emit("application_binding","\"application\":\"Terminal\",\"result\":\"BLOCKED\",\"reason\":\"requested_official_portable_Terminal_not_prepared\",\"input_sent\":false");return target;}
            // --window new forces a new candidate rather than adopting an
            // existing Terminal. Exact root/birth/Medium/launch-tree checks
            // below still decide whether the resulting HWND is authorized.
            target->command=L"\""+target->executable.wstring()+L"\" --window new new-tab --title \"PaneBind-"+s.run+
                L"-Terminal\" --suppressApplicationTitle \""+(fs::path(system.data())/L"cmd.exe").wstring()+L"\" /d /q /k";
        }else{
            target->executable=fs::path(system.data())/L"conhost.exe";
            // Historical evidence is explicitly Windows Console, not modern
            // Terminal. Preserve its exact cmd-child and parent/birth checks.
            target->root_executable=fs::path(system.data())/L"cmd.exe";
            target->command=L"\""+target->executable.wstring()+L"\" \""+target->root_executable.wstring()+L"\" /d /q /k title PaneBind-"+s.run+L"-Terminal";
            s.log.emit("Terminal_test_asset","\"distribution\":\"Windows_Console_host\",\"modern_Terminal_tested\":false");
        }
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
        ",\"launch_integrity_rid\":"+(rid<0?"null":std::to_string(rid))+",\"launch_elevated\":"+(rid<0?"null":(elevated?"true":"false"))+",\"launch_is_not_binding_proof\":true"+
        (target->kind=="VSCode"?",\"actual_VSCode_launch_flag\":\"--disable-gpu\",\"software_rendering_candidate\":true,\"previous_startup_failure_cause\":\"UNKNOWN\",\"GPU_fix_inferred\":false,\"strict_single_variable_causality_claimed\":false":""));
    std::optional<product::Snapshot> candidate;bool bound=false;
    if(normal&&target->processes.front().born)bound=wait([&]{
        discover_application_children(*target);candidate.reset();unsigned matches{};
        for(HWND h:root_baseline()){std::optional<product::Snapshot> fresh;if(application_fresh(s,*target,h,&fresh)){++matches;candidate=std::move(fresh);}}
        return matches==1;
    },12000);
    if(bound){target->identity=candidate;bound=test_root_stamp(s,*candidate,true);target->accepted=bound;}
    if(!bound&&launched)application_binding_facts(s,*target);
    if(target->kind=="VSCode"){
        DWORD exit{};const bool exit_read=launched&&GetExitCodeProcess(target->processes.front().handle->value,&exit)!=FALSE;
        s.log.emit("VSCode_launch_completion","\"launch_process_exit_code\":"+(exit_read?std::to_string(exit):"null")+
            ",\"launch_process_exited\":"+(exit_read?(exit!=STILL_ACTIVE?"true":"false"):"null")+
            ",\"root_bound\":"+(bound?"true":"false")+",\"launcher_exit_is_not_root_binding_or_function_PASS\":true");
        vscode_own_log_facts(s,*target);
    }
    s.log.emit("application_binding","\"application\":"+quote(target->kind)+",\"result\":"+quote(bound?"BOUND":"BLOCKED")+
        ",\"reason\":"+quote(bound?"baseline_excluded_new_root_exact_launch_tree_actual_Catalog_Medium_same_SID_session_desktop":"no_unique_eligible_root_in_held_birth_verified_launch_tree")+
        ",\"hwnd\":"+std::to_string(bound?reinterpret_cast<std::uintptr_t>(candidate->hwnd):0)+",\"pid\":"+std::to_string(bound?candidate->pid:0)+
        ",\"tid\":"+std::to_string(bound?candidate->tid:0)+",\"birth\":"+std::to_string(bound?candidate->born:0)+",\"key\":"+std::to_string(bound?candidate->key:0)+
        ",\"window_class\":"+quote(bound?utf8(candidate->window_class):"UNKNOWN")+",\"retained_process_facts\":"+std::to_string(target->processes.size())+
        ",\"input_sent\":false,\"product_permission_override\":false");return target;
}
bool close_application(State& s,ApplicationTarget& target){
    if(target.cleanup_confirmed)return true;
    bool attempted=false,gone=false;DWORD close_error{};std::string why="unconfirmed_root_has_no_close_authority";
    if(target.accepted&&target.identity){std::optional<product::Snapshot> fresh;
        const bool eligible=application_fresh(s,target,target.identity->hwnd,&fresh,&why);
        const bool same=eligible&&fresh&&product::Catalog::same(*target.identity,*fresh),stamp=same&&test_root_stamp(s,*target.identity);
        if(eligible&&!same)why="fresh_root_identity_differs_from_bound_identity";
        else if(same&&!stamp)why="exact_run_root_stamp_mismatch";
        if(stamp){SetLastError(0);attempted=PostMessageW(fresh->hwnd,WM_CLOSE,0,0)!=FALSE;close_error=attempted?0:GetLastError();
            gone=attempted&&wait([&]{return !IsWindow(fresh->hwnd);},3000);
            why=gone?"exact_root_close_observed":attempted?"exact_root_close_not_observed_in_bound":"WM_CLOSE_post_failed";
        }else{gone=!IsWindow(target.identity->hwnd);if(gone)why="bound_root_already_gone";}
        if(!gone)s.log.failed=true;
    }
    if(target.accepted&&target.identity&&!gone&&!target.close_failure_facts_recorded){
        target.close_failure_facts_recorded=true;
        // One read-only failure observation of this bound, birth-verified
        // root. Posting WM_CLOSE is not proof of processing or destruction.
        // A popup is never activated, closed, messaged or adopted here.
        const HWND root=target.identity->hwnd;std::optional<product::Snapshot> current;std::string identity_reason;
        const bool exact=application_fresh(s,target,root,&current,&identity_reason)&&current&&
            product::Catalog::same(*target.identity,*current)&&test_root_stamp(s,*target.identity);
        DWORD_PTR response{};bool null_read=false;DWORD null_error{};std::array<wchar_t,256> title{},popup_title{};
        std::array<wchar_t,128> popup_class{};int title_chars{},popup_title_chars{},popup_class_chars{};
        HWND popup{},popup_owner{};DWORD popup_pid{},popup_tid{};bool popup_tree=false;
        const bool root_alive=IsWindow(root)!=FALSE;const auto foreground=GetForegroundWindow();
        if(exact){
            title_chars=GetWindowTextW(root,title.data(),static_cast<int>(title.size()));
            popup=GetLastActivePopup(root);
            if(popup&&popup!=root){
                popup_tid=GetWindowThreadProcessId(popup,&popup_pid);popup_owner=GetWindow(popup,GW_OWNER);
                const auto process=std::find_if(target.processes.begin(),target.processes.end(),[&](const auto& item){return item.pid==popup_pid;});
                popup_tree=popup_tid&&GetAncestor(popup,GA_ROOT)==popup&&GetAncestor(popup,GA_ROOTOWNER)==root&&
                    std::find(target.baseline.begin(),target.baseline.end(),popup)==target.baseline.end()&&
                    process!=target.processes.end()&&process->handle&&WaitForSingleObject(process->handle->value,0)==WAIT_TIMEOUT&&
                    process_birth(process->handle->value)==process->born&&medium(process->handle->value);
                if(popup_tree){popup_title_chars=GetWindowTextW(popup,popup_title.data(),static_cast<int>(popup_title.size()));
                    popup_class_chars=GetClassNameW(popup,popup_class.data(),static_cast<int>(popup_class.size()));}
            }
            SetLastError(0);null_read=SendMessageTimeoutW(root,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&response)!=0;
            null_error=null_read?0:GetLastError();
        }
        s.log.emit("application_close_unconfirmed_facts","\"application\":"+quote(target.kind)+",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+
            ",\"same_exact_bound_root\":"+(exact?"true":"false")+",\"identity_reason\":"+quote(identity_reason)+",\"root_alive\":"+(root_alive?"true":"false")+
            ",\"root_enabled\":"+(exact?(IsWindowEnabled(root)?"true":"false"):"null")+",\"root_visible\":"+(exact?(IsWindowVisible(root)?"true":"false"):"null")+
            ",\"root_title\":"+(exact&&title_chars>0?quote(utf8(std::wstring(title.data(),title_chars))):"null")+
            ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+",\"WM_NULL_attempted\":"+(exact?"true":"false")+
            ",\"WM_NULL_returned\":"+(exact?(null_read?"true":"false"):"null")+",\"WM_NULL_result\":"+(null_read?std::to_string(response):"null")+
            ",\"WM_NULL_error\":"+(exact?std::to_string(null_error):"null")+",\"WM_NULL_timeout_ms\":100,\"last_active_popup\":"+
            (exact?std::to_string(reinterpret_cast<std::uintptr_t>(popup)):"null")+",\"popup_owner\":"+(popup&&popup!=root?std::to_string(reinterpret_cast<std::uintptr_t>(popup_owner)):"null")+
            ",\"popup_pid\":"+(popup_tid?std::to_string(popup_pid):"null")+",\"popup_tid\":"+(popup_tid?std::to_string(popup_tid):"null")+
            ",\"popup_is_new_owned_Medium_launch_tree_root\":"+(popup_tree?"true":"false")+
            ",\"popup_class\":"+(popup_tree&&popup_class_chars>0?quote(utf8(std::wstring(popup_class.data(),popup_class_chars))):"null")+
            ",\"popup_title\":"+(popup_tree&&popup_title_chars>0?quote(utf8(std::wstring(popup_title.data(),popup_title_chars))):"null")+
            ",\"popup_activated_messaged_or_closed\":false,\"single_post_failure_observation\":true,\"normal_close_or_cleanup_PASS_inferred\":false");
    }
    target.last_close_reason=why;
    target.cleanup_confirmed=gone;
    s.log.emit("application_cleanup","\"application\":"+quote(target.kind)+",\"root_confirmed\":"+(target.accepted?"true":"false")+
        ",\"exact_root_close_attempted\":"+(attempted?"true":"false")+",\"actual_root_gone\":"+(gone?"true":"false")+
        ",\"close_reason\":"+quote(why)+",\"WM_CLOSE_post_error\":"+std::to_string(close_error)+
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
        foreground(s,h);const auto before=visible(h);const auto mark=read_product(s).size();const auto limits=facts[0].limits;
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
void edge_paused_native_client_baseline(State& s){
    // One discriminating baseline for the retained Edge hang. Only this
    // exact new offline page is clicked, with product enhancements really
    // paused. This is not proof that hooks or all input observers are absent.
    const auto target=s.applications[0];
    if(!target||target->kind!="Edge"||!target->accepted||!target->identity||s.left||s.control)
        throw std::runtime_error("Edge_native_baseline_exact_idle_binding_required");
    const HWND h=target->identity->hwnd;const auto before=visible(h);
    bool pause_attempted=false,paused=false,click_attempted=false,dom_observed=false,resumed=false;
    bool hit_read=false;HWND hit{};DWORD_PTR region{};DWORD hit_error{};POINT point{};
    std::string failure,status,resume_status;const auto pause_mark=read_product(s).size();
    try{
        foreground(s,h);pause_attempted=true;
        if(!launch(s,L"--pause",true)||!launch(s,L"--status",true)||!wait([&]{status=record(s,pause_mark,"status");return !status.empty();},1000)||
           !product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0})||status.find("\"enabled\":false")==status.npos)
            throw std::runtime_error("Edge_native_baseline_product_pause_not_confirmed");
        paused=true;std::optional<product::Snapshot> fresh;
        if(!before||!application_fresh(s,*target,h,&fresh)||!fresh||GetForegroundWindow()!=h)
            throw std::runtime_error("Edge_native_baseline_identity_or_foreground_changed");
        const auto gui=product::observe_gui_context(*fresh);
        RECT client{};
        if(!gui.idle()||!GetClientRect(h,&client)||client.right<=client.left||client.bottom<=client.top)
            throw std::runtime_error("Edge_native_baseline_client_or_GUI_unknown");
        point={(client.left+client.right)/2,(client.top+client.bottom)/2};
        if(!ClientToScreen(h,&point))throw std::runtime_error("Edge_native_baseline_client_screen_unknown");
        cursor(s,point);hit=WindowFromPoint(point);const bool exact_child=matrix_client_child(s,*fresh,hit);
        SetLastError(ERROR_SUCCESS);
        hit_read=exact_child&&SendMessageTimeoutW(hit,WM_NCHITTEST,0,MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),
            SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
        hit_error=hit_read?ERROR_SUCCESS:exact_child?GetLastError():ERROR_SUCCESS;
        if(!hit_read||region!=HTCLIENT||!application_fresh(s,*target,h)||GetForegroundWindow()!=h||
           !product::observe_gui_context(*fresh).idle()||WindowFromPoint(point)!=hit)
            throw std::runtime_error("Edge_native_baseline_fresh_exact_client_not_confirmed");
        click_attempted=true;button(s,true);button(s,false);
        const auto expected=L"PaneBind-Mouse-"+s.run;std::array<wchar_t,1024> title{};
        dom_observed=wait([&]{return application_fresh(s,*target,h)&&GetWindowTextW(h,title.data(),static_cast<int>(title.size()))>0&&
            std::wstring(title.data()).find(expected)!=std::wstring::npos&&visible(h)==before;},2000);
        if(!dom_observed)failure="Edge_paused_native_client_DOM_delivery_not_observed";
    }catch(const std::exception& error){failure=error.what();}
    if(s.left&&!s.left_up_attempted)try{button(s,false);}catch(const std::exception& error){failure+=';';failure+=error.what();}
    if(pause_attempted){
        const auto mark=read_product(s).size();
        const bool known=launch(s,L"--status",true)&&wait([&]{resume_status=record(s,mark,"status");return !resume_status.empty();},1000)&&
            product::guest::matching_idle_status(resume_status,{s.product_pid,s.product_source,0,0});
        if(known&&resume_status.find("\"enabled\":true")!=resume_status.npos)resumed=true;
        else if(known&&resume_status.find("\"enabled\":false")!=resume_status.npos){
            // Pause is the existing public toggle. An unsupported --resume
            // would open Settings instead; never toggle an unknown state.
            const auto toggle_mark=read_product(s).size();
            resumed=launch(s,L"--command Pause",true)&&launch(s,L"--status",true)&&wait([&]{resume_status=record(s,toggle_mark,"status");return !resume_status.empty();},1000)&&
                product::guest::matching_idle_status(resume_status,{s.product_pid,s.product_source,0,0})&&resume_status.find("\"enabled\":true")!=resume_status.npos;
        }
    }
    s.log.emit("Edge_paused_native_client_baseline","\"scope\":\"one_exact_new_offline_page_enhancements_paused_not_hooks_absent\",\"attempt_limit\":1"+
        std::string(",\"product_pause_confirmed\":")+(paused?"true":"false")+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"WM_NCHITTEST_returned\":"+(hit_read?"true":"false")+
        ",\"WM_NCHITTEST\":"+(hit_read?std::to_string(region):"null")+",\"WM_NCHITTEST_error\":"+std::to_string(hit_error)+
        ",\"single_client_click_attempted\":"+(click_attempted?"true":"false")+",\"actual_DOM_click_effect\":"+(dom_observed?"true":"false")+
        ",\"product_resume_confirmed\":"+(resumed?"true":"false")+",\"failure\":"+quote(failure)+
        ",\"pause_status\":"+quote(status)+",\"resume_status\":"+quote(resume_status)+",\"enhanced_Move_or_architecture_PASS_inferred\":false");
    if(!failure.empty()||!paused||!dom_observed||!resumed||s.left||s.control)
        throw std::runtime_error("Edge_paused_native_client_baseline_failed_enhancement_not_attempted");
}
#include "platform/windows/product/v1_guest_terminal_capture.h"
void matrix_escape_and_client(State& s,const std::string& label){
    const HWND h=source_window(s,0);const auto original=visible(h);if(!original)throw std::runtime_error("matrix_escape_geometry_unknown");
    const auto facts=matrix_sources(s);const auto config=matrix_settings(s);const LONG travel=matrix_pixels(std::max(config.release_dip+24,40),facts[0].dpi);
    for(std::size_t n=1;n<3;++n)if(!authorized_source(s,source_window(s,n))||!matrix_show_source(s,n,false))throw std::runtime_error("matrix_Escape_target_hide_identity_failed");
    const auto g=down(s,0);phase(s,0,g,-travel,travel,translated(*original,-travel,travel),label+".Escape_before",true);
    const auto mark=read_product(s).size();key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);std::string end;
    const bool escaped=wait([&]{end=record(s,mark,"gesture_end",g.generation);return !end.empty()&&visible(h)==original;});
    button(s,false); // Test cleanup after Escape, deliberately not NormalUp.
    s.log.check(label+".Escape_restore",escaped&&end.find("\"normal_up\":false")!=std::string::npos,
        "\"production_end\":"+quote(end)+",\"actual\":"+(visible(h)?rect(*visible(h)):"null"));
    foreground(s,h);RECT client{};if(!GetClientRect(h,&client))throw std::runtime_error("matrix_client_geometry_unknown");
    POINT point{(client.right-client.left)/2,(client.bottom-client.top)/2};if(!ClientToScreen(h,&point))throw std::runtime_error("matrix_client_screen_unknown");cursor(s,point);
    const HWND hit=WindowFromPoint(point);DWORD_PTR region{};DWORD hit_pid{};
    const DWORD hit_tid=hit?GetWindowThreadProcessId(hit,&hit_pid):0;const HWND hit_root=hit?GetAncestor(hit,GA_ROOT):nullptr;
    const bool exact_child=matrix_client_child(s,facts[0],hit);SetLastError(ERROR_SUCCESS);
    const bool hit_read=exact_child&&SendMessageTimeoutW(hit,WM_NCHITTEST,0,
        MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)!=FALSE;
    const DWORD hit_error=hit_read?ERROR_SUCCESS:exact_child?GetLastError():ERROR_SUCCESS;
    s.log.emit("application_actual_client_hit","\"application\":"+quote(label)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(h))+
        ",\"source_pid\":"+std::to_string(facts[0].pid)+",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"]"+
        ",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"actual_hit_pid\":"+std::to_string(hit_pid)+
        ",\"actual_hit_tid\":"+std::to_string(hit_tid)+",\"actual_hit_root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit_root))+
        ",\"exact_bound_root_and_process_tree\":"+(exact_child?"true":"false")+",\"WM_NCHITTEST_attempted\":"+(exact_child?"true":"false")+
        ",\"WM_NCHITTEST_returned\":"+(hit_read?"true":"false")+",\"WM_NCHITTEST\":"+(hit_read?std::to_string(region):"null")+
        ",\"WM_NCHITTEST_error\":"+(exact_child?std::to_string(hit_error):"null")+",\"input_not_yet_sent\":true");
    const bool actual=exact_child&&hit_read&&region==HTCLIENT;
    if(!actual){
        const auto target=s.applications[0];
        if(target&&target->kind=="Terminal"&&!target->terminal_client_rejection_recorded){
            target->terminal_client_rejection_recorded=true;
            try{guest_observe_terminal_client_rejection(s,*target,facts[0],point);}
            catch(const std::exception& error){s.log.emit("Terminal_client_rejection_observation_failed",
                "\"reason\":"+quote(error.what())+",\"original_HTCLIENT_rejection_preserved\":true,\"client_input_sent\":false,\"visual_review\":\"NOT_RUN\"");}
        }
        throw std::runtime_error("matrix_actual_same_process_client_hit_unavailable");
    }
    const auto click_mark=read_product(s).size();button(s,true);button(s,false);Sleep(150);
    std::string context_reason;const auto context=fixture_source_gui_context(s,0,context_reason);const auto& gui=context.actual;
    const bool focused=context.known()&&gui.hwndFocus&&IsWindow(gui.hwndFocus)&&GetAncestor(gui.hwndFocus,GA_ROOT)==h;
    s.log.check(label+".client_click_not_routed_to_Move",record(s,click_mark,"gesture_start").empty()&&visible(h)==original&&focused,
        "\"actual_hit_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+",\"focus_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndFocus))+
        ",\"source_GUI_known\":"+(context.known()?"true":"false")+",\"source_GUI_mode\":"+quote(product::gui_context_mode_name(context.mode))+
        ",\"source_GUI_identity_reason\":"+quote(context_reason)+",\"source_GUI_owner_succeeded\":"+(context.owner_succeeded?"true":"false")+
        ",\"source_GUI_owner_error\":"+std::to_string(context.owner_error)+",\"source_GUI_foreground_succeeded\":"+(context.foreground_succeeded?"true":"false")+
        ",\"source_GUI_foreground_error\":"+std::to_string(context.foreground_error)+",\"source_GUI_foreground_before\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(context.foreground_before))+",\"source_GUI_foreground_after\":"+
        std::to_string(reinterpret_cast<std::uintptr_t>(context.foreground_after))+
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
        const bool saved=wait(content_matches);const auto saved_hash=hash(blank);
        s.log.check(label+".actual_client_text_and_native_save",saved&&!saved_hash.empty(),"\"guest_new_file_only\":true,\"expected_text_bytes\":"+std::to_string(plain.size())+
            ",\"actual_saved_sha256\":"+quote(saved_hash)+",\"clipboard_or_private_API_used\":false");
        matrix_chord(s,'A');key(s,VK_BACK,true);key(s,VK_BACK,false);matrix_chord(s,'S');
        s.log.check(label+".blank_document_restored_before_close",wait([&]{if(!fs::is_regular_file(blank))return false;const auto bytes=read(blank,4096);return
            (bytes.empty()&&fs::file_size(blank)==0)||bytes==std::string("\xef\xbb\xbf")||bytes==std::string("\xff\xfe");}));
    }else if(target->kind=="Terminal"){
        const auto receipt=target->directory/L"client-input-receipt.txt";if(fs::exists(receipt))throw std::runtime_error("matrix_terminal_receipt_must_be_new");
        key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);matrix_text(s,L"echo "+nonce+L">\""+receipt.wstring()+L"\"");key(s,VK_RETURN,true);key(s,VK_RETURN,false);
        const bool delivered=wait([&]{auto text=read(receipt,4096);while(!text.empty()&&(text.back()=='\r'||text.back()=='\n'||text.back()==' '))text.pop_back();return text==utf8(nonce);});
        const auto receipt_hash=hash(receipt);
        const bool modern=equal(target->executable.filename().wstring(),L"WindowsTerminal.exe");
        s.log.check(label+".actual_terminal_client_input",delivered&&!receipt_hash.empty(),
            "\"terminal_distribution\":"+quote(modern?"official_modern_portable_ZIP":"Windows_Console_host")+
            ",\"known_cmd_child_in_new_conhost_only\":"+(modern?"false":"true")+
            ",\"requested_shell\":\"exact_guest_system_cmd\",\"guest_new_output_file_only\":true,\"actual_receipt_sha256\":"+quote(receipt_hash));
    }
    s.log.check(label+".client_actions_did_not_start_geometry_gesture",record(s,click_mark,"gesture_start").empty()&&visible(h)==original);
    for(std::size_t n=1;n<3;++n)if(!matrix_show_source(s,n,true))throw std::runtime_error("matrix_Escape_hidden_target_identity_changed");
}
void matrix_input_idle(State& s){
    if(s.left){button(s,false);s.log.emit("test_cleanup","\"synthetic_release_sent\":true,\"not_product_NormalUp\":true");}
    if(s.control)key(s,VK_CONTROL,false);
    const auto mark=read_product(s).size();const bool sent=launch(s,L"--status",true);std::string status;
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
    std::vector<std::shared_ptr<ApplicationTarget>> unconfirmed;
    for(const auto& target:s.created_applications)if(target&&target->accepted&&!target->cleanup_confirmed){s.applications[0]=target;
        bool closed=false;std::string why;
        try{
            if(target->identity&&IsWindow(target->identity->hwnd)&&!matrix_show_source(s,0,true))why="exact_root_show_not_confirmed";
            closed=close_application(s,*target)&&target->cleanup_confirmed;
            if(!closed&&why.empty())why=target->last_close_reason;
        }catch(const std::exception& error){why=error.what();}
        if(!closed){s.log.failed=true;unconfirmed.push_back(target);}
        s.log.emit("application_cleanup_result","\"application\":"+quote(target->kind)+",\"result\":"+quote(closed?"PASS":"FAIL")+
            ",\"actual_root_gone\":"+(target->cleanup_confirmed?"true":"false")+",\"registration_retained\":"+(closed?"false":"true")+
            ",\"reason\":"+quote(why)+",\"identity_invalid_windows_not_controlled\":true");
        s.applications[0].reset();}
    s.created_applications=std::move(unconfirmed);
}
void independent_entry_ready(State&,std::string_view,bool);
void application_matrix_flow(State& s){
    if(!s.target_catalog)s.target_catalog=std::make_unique<product::Catalog>();
    for(auto& frame:s.frames)if(own(s,frame.hwnd))ShowWindow(frame.hwnd,SW_HIDE);
    std::shared_ptr<ApplicationTarget> browser,editor;
    const std::array<std::string,4> categories=(s.suite=="IndependentEntry"||s.suite=="CoreAndApplications"||s.suite=="ApplicationsOnly")?
        std::array<std::string,4>{"Terminal","Notepad","VSCode","Edge"}:std::array<std::string,4>{"Notepad","Edge","VSCode","Terminal"};
    for(const auto& kind:categories){
        std::shared_ptr<ApplicationTarget> target;bool passed=false;std::string reason;
        const auto down_before=s.left_down_send_qpc;const bool control_before=s.control;
        try{
            matrix_input_idle(s);target=provision_application(s,kind);
            if(!target->accepted){reason="no_exact_eligible_new_root";s.log.failed=true;}
            else{
                if(!matrix_show_source(s,0,false))throw std::runtime_error("matrix_unused_exact_source_hide_failed");
                s.applications[0]=target;matrix_layout(s,kind+"_independent_actual_constraints_setup");
                if(kind=="Edge")edge_paused_native_client_baseline(s);
                matrix_five(s,0,kind+".Move");
                const auto before=visible(source_window(s,0));if(!before)throw std::runtime_error("matrix_Resize_before_geometry_unknown");
                const auto identity=s.target_catalog->get(source_window(s,0),{},nullptr,false,false);if(!identity)throw std::runtime_error("matrix_Resize_actual_identity_changed");
                const LONG amount=matrix_pixels(28,identity->dpi);const auto g=down(s,0,false,HTBOTTOM);auto after=*before;after.b+=amount;
                phase(s,0,g,0,amount,after,kind+".Resize",true);up(s,0,g,after);
                matrix_commands(s,kind);matrix_escape_and_client(s,kind);target->matrix_passed=true;passed=true;
            }
        }catch(const std::exception& error){reason=error.what();s.log.failed=true;}
        if(!passed&&(s.suite=="IndependentEntry"||s.suite=="CoreAndApplications"||s.suite=="ApplicationsOnly")){
            s.log.emit("application_category_failure_preserved","\"application\":"+quote(kind)+",\"reason\":"+quote(reason)+
                ",\"result\":"+quote(target&&target->accepted?"FAIL":"BLOCKED")+",\"UP_retransmission\":false");
            const bool pending_left=s.left,pending_control=s.control;bool cleanup_up=false,cleanup_control=false;
            if(s.left){
                if(!s.left_down_send_qpc||s.left_down_send_qpc==down_before)throw std::runtime_error("application_pending_DOWN_not_this_category");
                button(s,false);cleanup_up=true;
            }
            if(s.control){if(control_before)throw std::runtime_error("application_pending_Ctrl_not_this_category");key(s,VK_CONTROL,false);cleanup_control=true;}
            s.log.emit("application_category_test_input_cleanup","\"application\":"+quote(kind)+
                ",\"category_left_was_outstanding\":"+(pending_left?"true":"false")+",\"category_Ctrl_was_outstanding\":"+(pending_control?"true":"false")+
                ",\"single_cleanup_UP_inserted\":"+(cleanup_up?"true":"false")+",\"owned_Ctrl_release_sent\":"+(cleanup_control?"true":"false")+
                ",\"delivery_NormalUp_or_failed_function_PASS_not_inferred\":true,\"failed_gesture_not_retried\":true");
            // The next category must not inherit this one's active gesture.
            // Observe the exact source's real GUI state before any settings
            // restoration can change foreground (notably for consoles).
            if(target&&target->accepted){
                std::optional<product::Snapshot> fresh;std::string rejected;
                const bool exact=target->identity&&application_fresh(s,*target,target->identity->hwnd,&fresh,&rejected)&&fresh&&
                    product::Catalog::same(*target->identity,*fresh);
                const auto gui=exact?product::observe_gui_context(*fresh):product::GuiContext{};
                s.log.check("application_failed_category_source_GUI_idle",exact&&gui.idle(),
                    "\"application\":"+quote(kind)+",\"identity_rejection\":"+quote(rejected)+
                    ",\"GUI_context_mode\":"+quote(product::gui_context_mode_name(gui.mode))+",\"failed_function_PASS_inferred\":false");
                // The retained Terminal failure can occur BEFORE the old
                // client-rejection capture. Observe the real UI only after
                // the existing single cleanup UP; never re-activate a lost
                // foreground or infer a genuine caption from an external HT.
                if(exact&&(kind=="Terminal"||kind=="Edge")&&s.left_down_send_qpc&&s.left_down_send_qpc!=down_before&&
                    s.left_up_attempted&&s.left_up_send_qpc>=s.left_down_send_qpc)try{
                    POINT actual_cursor{};
                    if(!GetCursorPos(&actual_cursor))throw std::runtime_error("application_failure_cursor_unknown");
                    guest_observe_application_input_rejection(s,*target,*fresh,actual_cursor,reason);
                }catch(const std::exception& error){s.log.emit("application_input_rejection_observation_failed",
                    "\"application\":"+quote(kind)+",\"failed_stage\":"+quote(reason)+",\"reason\":"+quote(error.what())+
                    ",\"original_function_failure_preserved\":true,\"additional_input_sent\":false");}
            }
            // Single pending test-input cleanup above precedes the empty-ledger gate.
            // This gate requires an empty send ledger before it can issue
            // the existing explicit pause, observes actual abort/idle facts,
            // and restores this run's defaults through the real product GUI.
            // It never repeats an UP or retries the failed application Move.
            independent_entry_ready(s,"application."+kind+".after_failure",true);
        }
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
    const auto mark=read_product(s).size();LARGE_INTEGER sent{};QueryPerformanceCounter(&sent);
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
        shutdown.find("\"own_hooks_stopped\":true")!=std::string::npos&&shutdown.find("\"own_overlay_destroyed\":true")!=std::string::npos&&input_resource_stop_observed(shutdown),
        "\"quit_exit\":"+std::to_string(quit_exit)+",\"source_still_blocked_at_exit\":"+(stopped_before_source_resumed?"true":"false")+",\"shutdown\":"+quote(shutdown));
    s.log.emit("L2_native_completion_at_process_exit","\"native_worker_exited_reported\":"+
        std::string(shutdown.find("\"native_worker_exited\":true")!=std::string::npos?"true":"false")+
        ",\"issued_native_completion_unknown\":"+(shutdown.find("\"issued_native_completion_unknown\":true")!=std::string::npos?"true":"false")+
        ",\"issued_call_cancelled_claimed\":false,\"product_cleanup_pass_inferred\":false");
    CloseHandle(s.process);s.process=nullptr;
    s.log.check("L2_owned_source_bounded_resume",wait([&]{return stall.finished.load();},1500));
    if(s.left)button(s,false); // TEST cleanup after the measured product exit.
    std::istringstream rows(product_log_since(s,g.offset));std::string row;unsigned issued{},late_issued{};bool timeout_unknown=false;
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
    if(s.process){std::size_t mark{};bool evidence_available=true;std::string evidence_error;
        try{mark=read_product(s).size();}catch(const std::exception& error){evidence_available=false;evidence_error=error.what();s.log.failed=true;}
        const auto stopped_pid=s.product_pid;
        const bool command=launch(s,L"--quit",true);const bool stopped=WaitForSingleObject(s.process,5000)==WAIT_OBJECT_0;
        std::string shutdown;if(evidence_available)try{shutdown=record(s,mark,"shutdown");}
            catch(const std::exception& error){evidence_available=false;evidence_error=error.what();s.log.failed=true;}
        DWORD exit_code=STILL_ACTIVE;if(stopped)GetExitCodeProcess(s.process,&exit_code);
        s.log.emit("product_stop","\"quit_command_completed\":"+std::string(command?"true":"false")+",\"process_exit_observed\":"+(stopped?"true":"false")+
            ",\"product_exit_code\":"+std::to_string(exit_code)+",\"shutdown_report\":"+quote(shutdown)+
            ",\"log_evidence_available\":"+(evidence_available?"true":"false")+",\"log_evidence_error\":"+quote(evidence_error));
        current_stop_confirmed=evidence_available&&command&&stopped&&exit_code==0&&number(shutdown,"\"process_id\":")==stopped_pid&&
            shutdown.find("\"own_hooks_stopped\":true")!=std::string::npos&&shutdown.find("\"own_overlay_destroyed\":true")!=std::string::npos&&
            shutdown.find("\"native_worker_exited\":true")!=std::string::npos&&shutdown.find("\"issued_native_completion_unknown\":false")!=std::string::npos&&
            shutdown.find("\"visual_recovery_stopped\":true")!=std::string::npos&&shutdown.find("\"visual_restore_confirmed\":true")!=std::string::npos&&input_resource_stop_observed(shutdown);
        if(!current_stop_confirmed)s.log.failed=true;
        if(!stopped){TerminateProcess(s.process,90);WaitForSingleObject(s.process,2000);s.log.failed=true;s.log.emit("test_abort","\"own_product_forced_termination\":true,\"product_cleanup_pass\":false");}
        CloseHandle(s.process);s.process=nullptr;}
    // A successful normal stop is also checked using real delivery to a
    // remaining exact empty fixture, not merely the child exit status. Forced
    // termination remains a failure regardless of this test-only observation.
    // An earlier independent application failure remains a failure, but does
    // not prevent observation of this instance's actual input-resource exit.
    bool delivered=false;const bool check_delivery=current_stop_confirmed;Frame* selected{};std::string candidates="[",why=check_delivery?"no_exact_owned_candidate":"current_product_stop_not_confirmed";
    for(std::size_t n=0;n<s.frames.size();++n){auto& f=s.frames[n];const bool exact=own(s,f.hwnd),shown=exact&&IsWindowVisible(f.hwnd)!=FALSE;
        if(n)candidates+=',';candidates+="{\"member\":"+std::to_string(n)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(f.hwnd))+
            ",\"exact_owned\":"+(exact?"true":"false")+",\"visible\":"+(exact?(shown?"true":"false"):"null")+"}";
        if(exact&&(!selected||(shown&&!IsWindowVisible(selected->hwnd))))selected=&f;
    }candidates+=']';
    bool show_requested=false,show_previous=false,visible_after=false,front_called=false,front_return=false,front_observed=false;
    bool client_read=false,converted=false,hit_read=false,down_invoked=false,up_invoked=false,counts_known=false;
    RECT client{};POINT point{};HWND actual_foreground{},hit{};unsigned downs_before{},ups_before{},downs_after{},ups_after{};
    const auto click_offset=s.log.mark();const auto prior_down_send=s.left_down_send_qpc;
    if(check_delivery&&selected){auto& f=*selected;
        try{
            // Only a window CREATED by this exact fixture may be shown. This
            // is test setup, not proof that product cleanup succeeded.
            if(!own(s,f.hwnd))throw std::runtime_error("selected_owned_identity_changed");
            if(!IsWindowVisible(f.hwnd)){show_requested=true;show_previous=ShowWindow(f.hwnd,SW_SHOWNOACTIVATE)!=FALSE;}
            visible_after=wait([&]{return own(s,f.hwnd)&&IsWindowVisible(f.hwnd);},1000);
            if(!visible_after)throw std::runtime_error("selected_owned_window_not_visible");
            front_called=true;front_return=SetForegroundWindow(f.hwnd)!=FALSE;
            front_observed=wait([&]{return own(s,f.hwnd)&&GetForegroundWindow()==f.hwnd;},1000);actual_foreground=GetForegroundWindow();
            if(!front_observed)throw std::runtime_error("actual_foreground_not_selected_owned_window");
            client_read=GetClientRect(f.hwnd,&client)!=FALSE;
            if(!client_read||client.left>=client.right||client.top>=client.bottom)throw std::runtime_error("selected_owned_client_rect_unavailable_or_empty");
            point={(client.left+client.right)/2,(client.top+client.bottom)/2};converted=ClientToScreen(f.hwnd,&point)!=FALSE;
            if(!converted)throw std::runtime_error("selected_owned_client_to_screen_failed");
            cursor(s,point);hit=WindowFromPoint(point);hit_read=true;
            if(!own(s,f.hwnd)||GetForegroundWindow()!=f.hwnd||hit!=f.hwnd)throw std::runtime_error("actual_client_hit_not_selected_owned_window");
            downs_before=f.down.load();ups_before=f.up.load();counts_known=true;
            down_invoked=true;button(s,true);up_invoked=true;button(s,false);
            delivered=wait([&]{return f.down.load()==downs_before+1&&f.up.load()==ups_before+1;},1000);
            why=delivered?"actual_client_DOWN_UP_delivered":"actual_client_DOWN_UP_not_observed";
        }catch(const std::exception& error){why=error.what();
            if(s.left&&!up_invoked){try{up_invoked=true;button(s,false);}catch(...){};}}
        if(counts_known){downs_after=f.down.load();ups_after=f.up.load();}
    }
    // button() logs whether SendInput was actually reached, including failure.
    // Entering the branch or a successful product shutdown is NOT execution.
    bool down_record=false,up_record=false,down_called=false,up_called=false;
    const auto click_bytes=s.log.since(click_offset);if(!click_bytes.empty()){
        std::istringstream rows(click_bytes);std::string line;
        while(std::getline(rows,line))if(line.ends_with('}')&&line.find("\"type\":\"test_button_send\"")!=std::string::npos){
            if(line.find("\"requested\":\"DOWN\"")!=std::string::npos){down_record=true;down_called=line.find("\"SendInput_called\":true")!=std::string::npos;}
            if(line.find("\"requested\":\"UP\"")!=std::string::npos){up_record=true;up_called=line.find("\"SendInput_called\":true")!=std::string::npos;}
        }
    }
    const bool successful_down_send=s.left_down_send_qpc&&s.left_down_send_qpc!=prior_down_send;
    const std::string executed=down_called||successful_down_send?"true":(!down_invoked||down_record?"false":"null");
    s.log.emit("post_stop_actual_client_click","\"check_requested\":"+std::string(check_delivery?"true":"false")+",\"executed\":"+executed+
        ",\"actual_down_up_delivered\":"+(delivered?"true":"false")+",\"candidates\":"+candidates+
        ",\"selected_target\":"+(selected?std::to_string(reinterpret_cast<std::uintptr_t>(selected->hwnd)):"null")+
        ",\"test_setup_ShowWindow_requested\":"+(show_requested?"true":"false")+",\"ShowWindow_previous_visible\":"+(show_requested?(show_previous?"true":"false"):"null")+
        ",\"visible_after_test_setup\":"+(selected&&check_delivery?(visible_after?"true":"false"):"null")+
        ",\"SetForegroundWindow_called\":"+(front_called?"true":"false")+",\"SetForegroundWindow_return\":"+(front_called?(front_return?"true":"false"):"null")+
        ",\"actual_foreground_matches\":"+(front_called?(front_observed?"true":"false"):"null")+",\"actual_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(actual_foreground))+
        ",\"GetClientRect_succeeded\":"+(client_read?"true":"false")+",\"client_rect\":"+(client_read?rect({client.left,client.top,client.right,client.bottom}):"null")+
        ",\"ClientToScreen_succeeded\":"+(converted?"true":"false")+",\"client_screen_point\":"+(converted?"["+std::to_string(point.x)+","+std::to_string(point.y)+"]":"null")+
        ",\"actual_hit\":"+(hit_read?std::to_string(reinterpret_cast<std::uintptr_t>(hit)):"null")+
        ",\"DOWN_SendInput_called\":"+(down_record?(down_called?"true":"false"):successful_down_send?"true":!down_invoked?"false":"null")+
        ",\"UP_SendInput_called\":"+(up_record?(up_called?"true":"false"):!up_invoked?"false":"null")+
        ",\"counts_before\":"+(counts_known?"["+std::to_string(downs_before)+","+std::to_string(ups_before)+"]":"null")+
        ",\"counts_after\":"+(counts_known?"["+std::to_string(downs_after)+","+std::to_string(ups_after)+"]":"null")+
        ",\"reason\":"+quote(why)+",\"show_is_test_setup_not_product_cleanup_PASS\":true,\"only_test_owned_window\":true,\"not_guest_destruction\":true");
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
bool gui_activate_exact_settings(State& s,const SettingsGui& gui){
    // Test-only normal activation of this exact product UI, not a product
    // foreground repair and never a click on a foreign/unknown window.
    bool down_attempted=false,up_attempted=false,activated=false;POINT point{};HWND hit{};
    std::string reason="not_started",fresh_rejection="not_checked";DWORD_PTR region{};RECT bounds{},client{};
    bool status_attempted=false,status_launch_succeeded=false,status_received=false,status_idle_matched=false;
    DWORD status_exit=STILL_ACTIVE;
    const auto idle=[](DWORD tid){GUITHREADINFO info{sizeof(info)};return tid&&GetGUIThreadInfo(tid,&info)&&
        !info.hwndCapture&&!info.hwndMoveSize&&!info.hwndMenuOwner&&
        !(info.flags&(GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE));};
    const auto fresh=[&]{
        fresh_rejection="none";
        if(!desktop()){fresh_rejection="input_desktop_unknown";return false;}
        if(s.left||s.control){fresh_rejection="test_input_ledger_outstanding";return false;}
        if(!gui_identity(s,gui,gui.tab)){fresh_rejection="settings_identity_changed";return false;}
        if(!IsWindowEnabled(gui.root)||IsIconic(gui.root)){fresh_rejection="settings_disabled_or_iconic";return false;}
        if(!idle(gui.tid)){fresh_rejection="settings_GUI_not_idle_or_unknown";return false;}
        for(const int vk:{VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})
            if(GetAsyncKeyState(vk)&0x8000){fresh_rejection="async_high_bit_VK_"+std::to_string(vk);return false;}
        const auto front=GetForegroundWindow();
        if(!front||!idle(GetWindowThreadProcessId(front,nullptr))){fresh_rejection="foreground_GUI_not_idle_or_unknown";return false;}
        return true;
    };
    try{
        const auto mark=read_product(s).size();std::string status;
        if(!fresh())throw std::runtime_error("settings_activation_initial_"+fresh_rejection);
        status_attempted=true;status_launch_succeeded=launch(s,L"--status",true,&status_exit);
        if(!status_launch_succeeded||status_exit)throw std::runtime_error("settings_activation_status_command_failed");
        status_received=wait([&]{status=record(s,mark,"status",0);return !status.empty();},1000);
        if(!status_received)throw std::runtime_error("settings_activation_current_status_not_observed");
        status_idle_matched=product::guest::matching_idle_status(status,{s.product_pid,s.product_source,0,0});
        if(!status_idle_matched)throw std::runtime_error("settings_activation_current_status_not_exact_idle");
        if(!GetWindowRect(gui.root,&bounds)||!GetClientRect(gui.root,&client))throw std::runtime_error("settings_activation_geometry_unknown");
        MapWindowPoints(gui.root,nullptr,reinterpret_cast<POINT*>(&client),2);
        if(client.top<=bounds.top)throw std::runtime_error("settings_activation_no_native_caption");
        point={bounds.left+(bounds.right-bounds.left)/2,bounds.top+(client.top-bounds.top)/2};cursor(s,point);hit=WindowFromPoint(point);
        RECT current{};POINT actual{};
        if(!fresh()||hit!=gui.root||!GetWindowRect(gui.root,&current)||!EqualRect(&current,&bounds)||!GetCursorPos(&actual)||
           actual.x!=point.x||actual.y!=point.y||!SendMessageTimeoutW(gui.root,WM_NCHITTEST,0,
               MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y)),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&region)||region!=HTCAPTION||
           !fresh()||WindowFromPoint(point)!=gui.root||!GetWindowRect(gui.root,&current)||!EqualRect(&current,&bounds))
            throw std::runtime_error("settings_activation_actual_caption_hit_not_confirmed");
        down_attempted=true;button(s,true);up_attempted=true;button(s,false);
        activated=wait([&]{RECT after{};return !s.left&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&
            gui_identity(s,gui,gui.tab)&&GetForegroundWindow()==gui.root&&gui_identity(s,gui,gui_focus(gui))&&
            idle(gui.tid)&&GetWindowRect(gui.root,&after)&&EqualRect(&after,&bounds);},1000);
        reason=activated?"single_actual_caption_click_activated":"caption_click_foreground_or_focus_not_confirmed";
    }catch(const std::exception& error){reason=error.what();
        if(down_attempted&&s.left&&!up_attempted&&!s.left_up_attempted){try{up_attempted=true;button(s,false);}catch(...){}}
    }
    const HWND observed_foreground=GetForegroundWindow();DWORD observed_foreground_pid{};
    const DWORD observed_foreground_tid=GetWindowThreadProcessId(observed_foreground,&observed_foreground_pid);
    GUITHREADINFO observed_gui{sizeof(observed_gui)};
    const bool observed_gui_known=observed_foreground_tid&&GetGUIThreadInfo(observed_foreground_tid,&observed_gui)!=FALSE;
    s.log.emit("settings_foreground_activation_setup","\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
        ",\"product_PID\":"+std::to_string(s.product_pid)+",\"actual_hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
        ",\"actual_region\":"+std::to_string(region)+",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"]"+
        ",\"DOWN_attempted\":"+(down_attempted?"true":"false")+",\"UP_attempted\":"+(up_attempted?"true":"false")+
        ",\"actual_foreground_and_focus\":"+(activated?"true":"false")+",\"reason\":"+quote(reason)+
        ",\"last_actual_fresh_rejection\":"+quote(fresh_rejection)+",\"status_command_attempted\":"+(status_attempted?"true":"false")+
        ",\"status_command_launch_succeeded\":"+(status_attempted?(status_launch_succeeded?"true":"false"):"null")+
        ",\"status_command_exit\":"+(status_attempted?std::to_string(status_exit):"null")+
        ",\"status_receipt_observed\":"+(status_received?"true":"false")+",\"status_exact_idle_matched\":"+(status_idle_matched?"true":"false")+
        ",\"observed_foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(observed_foreground))+
        ",\"observed_foreground_pid\":"+std::to_string(observed_foreground_pid)+",\"observed_foreground_tid\":"+std::to_string(observed_foreground_tid)+
        ",\"observed_foreground_class\":"+(observed_foreground?quote(utf8(gui_class(observed_foreground))):"null")+
        ",\"observed_foreground_GUI_known\":"+(observed_gui_known?"true":"false")+
        ",\"observed_foreground_capture\":"+(observed_gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(observed_gui.hwndCapture)):"null")+
        ",\"observed_foreground_move_size\":"+(observed_gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(observed_gui.hwndMoveSize)):"null")+
        ",\"observed_foreground_menu_owner\":"+(observed_gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(observed_gui.hwndMenuOwner)):"null")+
        ",\"observed_foreground_GUI_flags\":"+(observed_gui_known?std::to_string(observed_gui.flags):"null")+
        ",\"setup_not_product_gesture\":true,\"attempt_limit\":1,\"SetForegroundWindow_used\":false,\"AttachThreadInput_used\":false");
    return activated;
}
SettingsGui gui_open(State& s){
    const auto mark=read_product(s).size();
    s.log.check("GUI_production_settings_command",launch(s,L"--settings",true));SettingsGui gui;std::string ready_receipt;bool ready_observed=false;
    const bool created=wait([&]{
        // A visible root can be observed while show() is still animating and
        // before it finishes layout/focus. Secondary-command exit only proves
        // PostMessage delivery. Wait for this actual product show() receipt.
        ready_receipt=record(s,mark,"settings_open");
        ready_observed=number(ready_receipt,"\"process_id\":")==s.product_pid&&
            ready_receipt.find("\"source_sha\":"+quote(s.product_source))!=ready_receipt.npos&&
            ready_receipt.find("\"created\":true")!=ready_receipt.npos;
        if(!ready_observed)return false;
        struct Search {DWORD pid{};HWND root{};DWORD tid{};} search{s.product_pid};
        EnumWindows([](HWND h,LPARAM data)->BOOL{auto& query=*reinterpret_cast<Search*>(data);DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);
            if(pid==query.pid&&gui_class(h)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(h)){query.root=h;query.tid=tid;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&search));
        gui={search.root,search.root?GetDlgItem(search.root,100):nullptr,search.tid};return gui.root&&gui.tab&&
            gui_class(gui.tab)==WC_TABCONTROLW&&gui_identity(s,gui,gui.tab);
    });
    s.log.check("GUI_actual_settings_ready_receipt",ready_observed,"\"actual_product_receipt\":"+quote(ready_receipt)+
        ",\"same_PID_source_after_command\":"+(ready_observed?"true":"false")+",\"secondary_exit_not_GUI_readiness\":true");
    // Completion of show() and current activation are separate observations.
    // Windows may legitimately deny its foreground request; a historical
    // false receipt must not make this exact visible UI permanently unusable.
    bool found=created&&GetForegroundWindow()==gui.root&&gui_identity(s,gui,gui_focus(gui));
    if(created&&!found)found=gui_activate_exact_settings(s,gui);
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
    const bool edited=wait([&]{return gui_text(s,gui,edit)==wanted;});
    const auto actual=gui_text(s,gui,edit);
    s.log.check("GUI_actual_edit_keys",edited&&actual==wanted,"\"actual_text\":"+quote(utf8(actual)));
    const auto before=gui_focus(gui);gui_press(s,gui,VK_TAB);
    s.log.check("GUI_actual_Tab_focus",wait([&]{const auto after=gui_focus(gui);return after&&after!=before&&gui_identity(s,gui,after);}));
}
std::optional<bool> gui_public_action_legend(State& s,const SettingsGui& gui,HWND control){
    if(!gui_identity(s,gui,control)||gui_class(control)!=L"Edit")return {};
    std::array<wchar_t,25> prefix{};DWORD_PTR copied{};
    if(!SendMessageTimeoutW(control,WM_GETTEXT,prefix.size(),reinterpret_cast<LPARAM>(prefix.data()),SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&copied)||
       copied>=prefix.size())return {};
    return std::wstring_view(prefix.data(),copied).starts_with(L"动作标识 → 中文行为：");
}
void gui_save_enter(State& s,const SettingsGui& gui){
    const auto save=GetDlgItem(gui.root,IDOK);
    for(unsigned n=0;n<64&&gui_focus(gui)!=save;++n){
        if(!gui_identity(s,gui,save))throw std::runtime_error("GUI_save_identity_changed");
        const auto before=gui_focus(gui);gui_press(s,gui,VK_TAB);
        if(!wait([&]{return gui_focus(gui)&&gui_focus(gui)!=before;})){
            const auto after=gui_focus(gui);DWORD focus_pid{},foreground_pid{};const auto focus_tid=GetWindowThreadProcessId(after,&focus_pid);
            const auto foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&foreground_pid);
            const auto public_legend=gui_public_action_legend(s,gui,after);SetLastError(0);const auto style=GetWindowLongPtrW(after,GWL_STYLE);
            const auto style_error=GetLastError();const bool style_known=gui_identity(s,gui,after)&&(style||!style_error);
            const auto query=[&](WPARAM code){DWORD_PTR value{};MSG keyboard{};keyboard.hwnd=after;keyboard.message=WM_KEYDOWN;keyboard.wParam=code;
                const bool known=gui_identity(s,gui,after)&&SendMessageTimeoutW(after,WM_GETDLGCODE,code,reinterpret_cast<LPARAM>(&keyboard),
                    SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&value)!=FALSE;
                return known?std::to_string(value):std::string("null");};
            s.log.emit("GUI_actual_Tab_navigation_failure","\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(gui.root))+
                ",\"focus_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(before))+
                ",\"focus_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(after))+",\"focus_class\":"+quote(utf8(gui_class(after)))+
                ",\"focus_pid\":"+std::to_string(focus_pid)+",\"focus_tid\":"+std::to_string(focus_tid)+
                ",\"focus_ES_READONLY\":"+(style_known?((style&ES_READONLY)?"true":"false"):"null")+
                ",\"focus_ES_MULTILINE\":"+(style_known?((style&ES_MULTILINE)?"true":"false"):"null")+
                ",\"fixed_public_action_legend_prefix\":"+(public_legend?(*public_legend?"true":"false"):"null")+
                ",\"custom_control_text_recorded\":false"+
                ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+",\"foreground_pid\":"+std::to_string(foreground_pid)+
                ",\"readonly_WM_GETDLGCODE_with_WM_KEYDOWN_MSG\":{\"VK_TAB\":"+query(VK_TAB)+",\"VK_ESCAPE\":"+query(VK_ESCAPE)+
                "},\"actual_Tab_input_preserved\":true,\"query_is_not_historical_query_arguments_or_key_delivery_receipt\":true");
            throw std::runtime_error("GUI_save_Tab_not_processed");
        }
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
    gui_page_select(s,gui,3);HWND mouse_edit{};
    for(HWND child=GetWindow(gui_page(s,gui),GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        if(gui_class(child)==L"Edit"&&gui_text(s,gui,child).starts_with(L"mouse_count=")){mouse_edit=child;break;}
    if(!mouse_edit)throw std::runtime_error("GUI_mouse_edit_for_legend_navigation_missing");
    gui_click(s,gui,mouse_edit);gui_press(s,gui,VK_TAB);
    s.log.check("GUI_mouse_Tab_reaches_readonly_action_legend",wait([&]{const auto focus=gui_focus(gui);
        const auto legend=gui_public_action_legend(s,gui,focus);const auto style=GetWindowLongPtrW(focus,GWL_STYLE);
        return focus!=mouse_edit&&legend&&*legend&&(style&ES_READONLY)&&(style&ES_MULTILINE);}),
        "\"actual_Tab_from_mouse_EDIT\":true,\"only_fixed_public_legend_prefix_compared\":true");
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
    cleanup(s);if(s.log.failed)throw std::runtime_error("GUI_restart_normal_stop_failed");const auto restart_mark=read_product(s).size();
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
    const bool observed=wait([&]{return own(s,h)&&native_visual_is(h,alpha,top);});
    const auto actual=own(s,h)?native_visual(h):std::optional<NativeVisual>{};
    const bool exact=actual&&actual->layered&&actual->known&&actual->alpha==alpha&&actual->color==0&&actual->flags==LWA_ALPHA&&actual->topmost==top;
    s.log.check(std::move(name),observed&&exact,
        "\"expected_native_layered_alpha\":"+std::to_string(alpha)+",\"expected_topmost\":"+(top?"true":"false")+
        ",\"actual_native_layered_alpha\":"+(actual&&actual->layered&&actual->known?std::to_string(actual->alpha):"null")+
        ",\"actual_topmost\":"+(actual?(actual->topmost?"true":"false"):"null")+
        ",\"actual_layered\":"+(actual?(actual->layered?"true":"false"):"null")+
        ",\"actual_layered_tuple_known\":"+(actual&&actual->layered&&actual->known?"true":"false")+
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
    const auto resume=[&]{const auto mark=read_product(s).size();s.log.check("visual_resume_command",launch(s,L"--command Pause",true));
        s.log.check("visual_resume_actual_enabled",wait([&]{return record(s,mark,"status").find("\"enabled\":true")!=std::string::npos;}));};
    s.log.check("visual_pause_command",launch(s,L"--pause",true));visual_exact(s,h,173,true,"visual_pause_restores_actual_original_tuple");resume();
    visual_command(s,h,L"OpacityDown",141,true);visual_toggle_hotkey(s,h,141,false);
    const auto stop_mark=read_product(s).size();const auto driver_stop_mark=s.log.mark();
    const auto stopped_pid=s.product_pid;cleanup(s);
    // Earlier independent application failures remain in the global verdict,
    // but do not turn this fresh, actually observed stop into a false failure.
    // Conversely, a forced termination or an older instance's shutdown cannot
    // satisfy this visual-restoration check.
    std::string stop_receipt;const auto driver_text=s.log.since(driver_stop_mark);
    if(!driver_text.empty()){
        std::istringstream rows(driver_text);std::string row;
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
        stopped.find("\"visual_restore_confirmed\":true")!=std::string::npos&&input_resource_stop_observed(stopped),
        "\"product_stop\":"+quote(stop_receipt)+",\"shutdown\":"+quote(stopped)+",\"previous_global_failures_preserved\":true");
    visual_exact(s,h,173,true,"visual_normal_exit_restores_actual_original_tuple");
    s.log.check("visual_exit_helper_confirmed",record(s,stop_mark,"shutdown").find("\"visual_recovery_stopped\":true")!=std::string::npos);
    const auto restart=read_product(s).size();s.log.check("visual_restart_same_product",launch(s,L"--diagnostic",false));
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
    button(s,false);const auto abort_up=abort_cleanup_up_receipt(s,g.offset,g.generation);
    s.log.check("visual_abort_matching_real_UP",abort_up.matched,
        "\"actual_product_UP\":"+quote(actual_up(s,g.offset,g.generation))+
        ",\"async_left_state\":"+std::to_string(GetAsyncKeyState(VK_LBUTTON))+","+abort_up.json());
    own_visual_setup(s,h,173,false);desired.drag_transparency=false;desired.shake=true;visual_gui_save(s,desired);
    const UINT dpi=GetDpiForWindow(h);if(!dpi)throw std::runtime_error("visual_shake_DPI_unknown");
    const LONG step=std::max<LONG>(48,MulDiv(24,static_cast<int>(dpi),96)*2);
    const auto count_mutations=[&](std::size_t offset){std::istringstream rows(product_log_since(s,offset));std::string row;unsigned count{};
        while(std::getline(rows,row))if(row.find("\"type\":\"visual_mutation\"")!=std::string::npos)++count;return count;};
    for(unsigned gesture=0;gesture<2;++gesture){
        start=*visible(h);g=down(s,0);LONG dx{};
        for(unsigned point=0;point<5;++point){dx=point%2?-step:step;phase(s,0,g,dx,0,translated(start,dx,0),"visual_real_shake_"+std::to_string(gesture)+"_"+std::to_string(point),true);}
        const BYTE alpha=gesture?173:190;const bool top=gesture==0;
        visual_exact(s,h,alpha,top,gesture?"visual_second_shake_restores_tuple":"visual_first_shake_sets_tuple");
        s.log.check("visual_shake_actual_mutation_receipts",wait([&]{return count_mutations(g.offset)>=2;}));
        const auto after_trigger=read_product(s).size();
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
bool activation_layout_idle(){return !layout_ui_pending.load(std::memory_order_acquire);}
// A continuation gate for independent cases, not a replacement for a failed
// gesture's actual UP or cleanup. It neither injects a release nor resets a
// window's geometry. All input below targets this exact product's own UI.
void independent_entry_ready(State& s,std::string_view stage,bool restore_defaults){
    if(s.suite!="IndependentEntry"&&s.suite!="CoreAndApplications"&&s.suite!="ApplicationsOnly"&&s.suite!="EntryAndSettings"&&s.suite!="OwnedComponentsOnly")throw std::runtime_error("independent_gate_wrong_suite");
    if(stage=="application_matrix.after"||stage.starts_with("portable.")){
        const auto pending=std::count_if(s.created_applications.begin(),s.created_applications.end(),[](const auto& target){
            return target&&target->accepted&&!target->cleanup_confirmed;});
        s.log.check("independent_application_cleanup_confirmed_before_next_scope",pending==0,
            "\"stage\":"+quote(stage)+",\"unconfirmed_registered_roots\":"+std::to_string(pending)+",\"unknown_identity_not_closed_to_force_PASS\":true");
    }
    const bool originals=std::all_of(s.frames.begin(),s.frames.end(),[&](const auto& f){return own(s,f.hwnd);});
    struct Roots {State* state;unsigned visited{},matching{};bool unknown{},unexpected{};std::string actual{"["};} roots{&s};
    const bool enumerated=EnumThreadWindows(s.owner,[](HWND h,LPARAM value)->BOOL{auto& r=*reinterpret_cast<Roots*>(value);
        if(++r.visited>32){r.unknown=true;return FALSE;}
        std::array<wchar_t,256> name{};DWORD pid{};const auto tid=GetWindowThreadProcessId(h,&pid);
        if(tid!=r.state->owner||pid!=GetCurrentProcessId()||!GetClassNameW(h,name.data(),static_cast<int>(name.size()))){r.unknown=true;return FALSE;}
        if(!equal(name.data(),r.state->class_name)||GetAncestor(h,GA_ROOT)!=h)return TRUE;
        if(r.matching++)r.actual+=',';r.actual+=std::to_string(reinterpret_cast<std::uintptr_t>(h));
        if(std::none_of(r.state->frames.begin(),r.state->frames.end(),[&](const auto& f){return f.hwnd==h&&own(*r.state,h);}))r.unexpected=true;
        return TRUE;
    },reinterpret_cast<LPARAM>(&roots))!=FALSE;roots.actual+=']';
    s.log.check("independent_actual_fixture_roots_only_original_three",enumerated&&!roots.unknown&&!roots.unexpected&&roots.matching==3&&originals&&
        !layout_ui_pending.load(std::memory_order_acquire),"\"stage\":"+quote(stage)+",\"enumeration_completed\":"+(enumerated?"true":"false")+
        ",\"unknown_window_facts\":"+(roots.unknown?"true":"false")+",\"unexpected_fixture_root\":"+(roots.unexpected?"true":"false")+
        ",\"actual_fixture_class_roots\":"+roots.actual+",\"RAII_return_not_used_as_destruction_proof\":true");
    GUITHREADINFO source{sizeof(source)};
    const bool source_idle=GetGUIThreadInfo(s.owner,&source)&&!source.hwndCapture&&!source.hwndMoveSize&&!source.hwndMenuOwner;
    const auto app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD app_pid{};
    const auto app_tid=app?GetWindowThreadProcessId(app,&app_pid):0;
    const auto birth=s.process?process_birth(s.process):0;
    const auto actual_hash=hash(s.product),source_hash=hash(s.input_product);
    const bool alive=s.process&&s.product_pid&&birth&&app_tid&&app_pid==s.product_pid&&
        GetProcessId(s.process)==s.product_pid&&WaitForSingleObject(s.process,0)==WAIT_TIMEOUT&&medium(s.process)&&
        !actual_hash.empty()&&actual_hash==source_hash;
    s.log.check("independent_entry_initial_resource_identity",!s.left&&!s.control&&originals&&source_idle&&alive&&
        !layout_ui_pending.load(std::memory_order_acquire),"\"stage\":"+quote(stage)+
        ",\"driver_outstanding_is_not_delivery\":true,\"no_synthetic_UP_sent_by_gate\":true");
    GUITHREADINFO product_gui{sizeof(product_gui)};
    if(!GetGUIThreadInfo(app_tid,&product_gui)||product_gui.hwndCapture||product_gui.hwndMoveSize)
        throw std::runtime_error("independent_product_GUI_context_unknown_or_busy");
    if(product_gui.hwndActive&&product_gui.hwndActive!=app){
        SettingsGui candidate{product_gui.hwndActive,GetDlgItem(product_gui.hwndActive,100),app_tid};
        if(!gui_identity(s,candidate,candidate.tab))throw std::runtime_error("independent_unknown_active_product_modal_not_controlled");
    }
    if(product_gui.hwndMenuOwner){
        DWORD menu_pid{};GetWindowThreadProcessId(product_gui.hwndMenuOwner,&menu_pid);
        if(product_gui.hwndMenuOwner!=app||menu_pid!=s.product_pid||GetForegroundWindow()!=app)
            throw std::runtime_error("independent_menu_not_exact_product_owner");
        key(s,VK_ESCAPE,true);key(s,VK_ESCAPE,false);
        s.log.check("independent_actual_product_menu_Escape",wait([&]{GUITHREADINFO g{sizeof(g)};
            return GetGUIThreadInfo(app_tid,&g)&&!g.hwndMenuOwner&&!g.hwndCapture&&!g.hwndMoveSize;}));
    }
    struct Settings {State* s;HWND app;DWORD tid;SettingsGui gui;unsigned count{};} query{&s,app,app_tid};
    EnumThreadWindows(app_tid,[](HWND h,LPARAM p)->BOOL{auto& q=*reinterpret_cast<Settings*>(p);
        if(gui_class(h)==L"PaneBindV1SettingsWindow"&&IsWindowVisible(h)){
            SettingsGui candidate{h,GetDlgItem(h,100),q.tid};if(GetWindow(h,GW_OWNER)==q.app&&gui_identity(*q.s,candidate,candidate.tab)){q.gui=candidate;++q.count;}}
        return TRUE;},reinterpret_cast<LPARAM>(&query));
    if(query.count>1)throw std::runtime_error("independent_multiple_settings_windows");
    if(query.count){
        // A failed modeless-settings live-policy case can legitimately leave
        // its exact test-owned source foreground. Revisit our exact settings
        // with the existing one-click/actual-idle guard, never activate an
        // unknown foreground or treat this cleanup as the failed case PASS.
        if(GetForegroundWindow()!=query.gui.root){
            const auto front=GetForegroundWindow();
            if(!own(s,front)||!gui_activate_exact_settings(s,query.gui))
                throw std::runtime_error("independent_settings_has_unknown_modal_or_foreground");
            s.log.emit("independent_modeless_settings_cleanup_revisit",
                "\"stage\":"+quote(stage)+",\"previous_foreground_exact_test_owned_source\":true,\"failed_case_PASS_inferred\":false");
        }
        gui_press(s,query.gui,VK_ESCAPE);
        s.log.check("independent_actual_settings_Escape",wait([&]{return !IsWindow(query.gui.root);}));
    }
    const auto status=[&]{std::string row;const auto mark=read_product(s).size();DWORD code=STILL_ACTIVE;
        if(!launch(s,L"--status",true,&code)||code||!wait([&]{row=record(s,mark,"status");return !row.empty();},1000)||
           number(row,"\"process_id\":")!=s.product_pid)throw std::runtime_error("independent_fresh_status_unavailable");return row;};
    auto facts=status();
    if(facts.find("\"gesture_active\":true")!=facts.npos){
        const auto mark=read_product(s).size();
        s.log.check("independent_explicit_abort_pause_command",launch(s,L"--pause",true));
        s.log.check("independent_abort_observed_not_NormalUp",wait([&]{const auto end=record(s,mark,"gesture_end");
            return !end.empty()&&number(end,"\"end\":")==static_cast<unsigned>(panebind::core::v1::End::Paused)&&
                end.find("\"normal_up\":false")!=end.npos;}));facts=status();
    }
    const auto idle=[](const std::string& row){return row.find("\"gesture_active\":false")!=row.npos&&
        row.find("\"native_operation_pending\":false")!=row.npos&&row.find("\"native_completion_unknown\":false")!=row.npos&&
        row.find("\"visual_restore_pending\":false")!=row.npos;};
    s.log.check("independent_entry_actual_idle_before_continuation",idle(facts)&&GetGUIThreadInfo(app_tid,&product_gui)&&
        !product_gui.hwndCapture&&!product_gui.hwndMoveSize&&!product_gui.hwndMenuOwner,
        "\"stage\":"+quote(stage)+",\"status\":"+quote(facts)+",\"failed_case_PASS_inferred\":false");
    if(!restore_defaults&&facts.find("\"enabled\":false")!=facts.npos){
        // Ordinary portable-entry checks keep their current saved attraction
        // value; only resume after a separately observed abort/idle gate.
        const auto before=matrix_settings(s);const auto gui=gui_open(s);gui_page_select(s,gui,0);
        const auto enabled=gui_find(s,gui,L"Button",L"启用 PaneBind");
        if(gui_read(s,gui,enabled,BM_GETCHECK)!=BST_CHECKED)gui_click(s,gui,enabled);
        s.log.check("independent_portable_resume_actual_checkbox",wait([&]{return gui_read(s,gui,enabled,BM_GETCHECK)==BST_CHECKED;}));
        gui_save_enter(s,gui);const auto restored=matrix_settings(s);auto expected=before;
        expected.enabled=true;expected.generation=restored.generation;facts=status();
        s.log.check("independent_portable_resume_preserves_other_settings",restored==expected&&idle(facts)&&
            facts.find("\"enabled\":true")!=facts.npos,"\"not_recovery_of_old_generation\":true");
    }
    if(restore_defaults){
        namespace v=panebind::core::v1;const auto actual=matrix_settings(s);auto expected=v::default_settings();expected.generation=actual.generation;
        const auto run=settings_guest_run_value();const auto own_run=L"\""+s.product.wstring()+L"\" --startup";
        if(run.exists&&run.text!=own_run)throw std::runtime_error("independent_unknown_startup_value_not_modified");
        if(actual!=expected||run.exists||!fs::exists(s.product.parent_path()/L"settings.ini")||facts.find("\"enabled\":true")==facts.npos){
            const auto gui=gui_open(s);settings_guest_activate(s,gui,GetDlgItem(gui.root,111));
            const auto modal=settings_guest_modal(s,gui,L"PaneBind");const auto yes=GetDlgItem(modal.root,IDYES);RECT r{};
            DWORD yes_pid{};if(!yes||GetWindowThreadProcessId(yes,&yes_pid)!=gui.tid||yes_pid!=s.product_pid||
                !IsChild(modal.root,yes)||!GetWindowRect(yes,&r))throw std::runtime_error("independent_defaults_confirmation_unknown");
            const POINT point{(r.left+r.right)/2,(r.top+r.bottom)/2};cursor(s,point);
            if(!settings_guest_modal_identity(s,gui,modal)||GetForegroundWindow()!=modal.root||WindowFromPoint(point)!=yes)
                throw std::runtime_error("independent_defaults_actual_yes_hit_changed");
            button(s,true);button(s,false);
            s.log.check("independent_defaults_real_confirmation",wait([&]{return !IsWindow(modal.root)&&GetForegroundWindow()==gui.root;}));
            gui_save_enter(s,gui);
        }
        const auto restored=settings_guest_configuration(s);expected.generation=restored.generation;facts=status();
        s.log.check("independent_defaults_actual_saved_and_readback",restored==expected&&!settings_guest_run_value().exists&&idle(facts)&&
            facts.find("\"enabled\":true")!=facts.npos,"\"stage\":"+quote(stage)+",\"test_setup_only_not_failed_case_PASS\":true");
    }
    // Enabled/idle is not evidence that the service still admits input. Do
    // not send a later scenario's DOWN after a known resource-health failure.
    // This is the service's report, not a claim to query silent LL-hook loss.
    s.log.check("independent_input_service_reported_healthy",facts.find("\"hook_owner_running\":true")!=facts.npos,
        "\"stage\":"+quote(stage)+",\"enabled_and_idle_not_health_proof\":true,\"silent_hook_health_query_claimed\":false");
}
#include "platform/windows/product/v1_guest_terminal_case.h"
void independent_entry_flow(State& s,bool core_and_applications=false){
    const bool owned_components_only=s.suite=="OwnedComponentsOnly";
    const auto phase=[&](const char* name,void(*run)(State&)){
        independent_entry_ready(s,std::string(name)+".before",true);const auto mark=s.log.mark();bool exception=false;
        const auto down_before=s.left_down_send_qpc;const bool control_before=s.control;
        try{run(s);}catch(const std::exception& error){exception=true;s.log.failed=true;
            s.log.emit("independent_entry_phase_failure","\"phase\":"+quote(name)+",\"reason\":"+quote(error.what())+",\"original_failure_preserved\":true");}
        std::string rows,evidence_error;
        try{rows=s.log.since(mark);}catch(const std::exception& error){exception=true;s.log.failed=true;evidence_error=error.what();}
        const bool failed=exception||rows.find("\"result\":\"FAIL\"")!=rows.npos;
        const bool blocked=rows.find("\"result\":\"BLOCKED")!=rows.npos||rows.find("\"result\":\"ENVIRONMENT_BLOCKED\"")!=rows.npos;
        const bool not_run=rows.find("\"result\":\"NOT_RUN\"")!=rows.npos;
        if(failed||blocked||not_run)s.log.failed=true;
        if(failed||blocked||not_run){
            const bool pending_left=s.left,up_already_attempted=s.left_up_attempted,pending_control=s.control;
            bool up_sent=false,control_sent=false;std::string cleanup_error;
            if(s.left&&!s.left_up_attempted){
                if(!s.left_down_send_qpc||s.left_down_send_qpc==down_before)cleanup_error="pending_DOWN_not_created_by_this_phase";
                else try{button(s,false);up_sent=true;}catch(const std::exception& error){cleanup_error=error.what();}
            }
            if(s.control){
                if(control_before)cleanup_error+=";pending_Ctrl_not_created_by_this_phase";
                else try{key(s,VK_CONTROL,false);control_sent=true;}catch(const std::exception& error){cleanup_error+=';';cleanup_error+=error.what();}
            }
            s.log.emit("independent_phase_test_input_cleanup","\"phase\":"+quote(name)+",\"phase_left_was_outstanding\":"+(pending_left?"true":"false")+
                ",\"UP_already_attempted_before_cleanup\":"+(up_already_attempted?"true":"false")+",\"single_pending_UP_inserted\":"+(up_sent?"true":"false")+
                ",\"phase_Ctrl_was_outstanding\":"+(pending_control?"true":"false")+",\"own_Ctrl_release_sent\":"+(control_sent?"true":"false")+
                ",\"cleanup_error\":"+quote(cleanup_error)+",\"not_NormalUp_or_delivery_PASS\":true,\"failed_scenario_not_retried\":true");
        }
        if(!evidence_error.empty())s.log.emit("independent_phase_evidence_unavailable","\"phase\":"+quote(name)+
            ",\"reason\":"+quote(evidence_error)+",\"failed_scenario_preserved\":true,\"existing_input_cleanup_attempted_first\":true,\"PASS_inferred\":false");
        independent_entry_ready(s,std::string(name)+".after",true);
        s.log.emit("independent_entry_phase_result","\"phase\":"+quote(name)+",\"result\":"+
            quote(failed?"FAIL":blocked?"BLOCKED":not_run?"NOT_RUN":"PASS")+
            ",\"independent_only\":true,\"component_result_not_full_I02_I11_or_version_PASS\":true");
    };
    if(!core_and_applications){
    phase("settings_GUI",settings_gui_flow);
    phase("settings_GUI_diagnostic",owned_settings_diagnostic_case);
    phase("rapid_terminal_motion",owned_rapid_terminal_move);
    phase("settings_startup",settings_guest_startup);
    phase("five_button_mapping",owned_mouse_mapping_cases);
    phase("settings_import_export",settings_guest_import_export);
    phase("settings_save_failure",settings_guest_save_failure);
    phase("settings_diagnostic_export_failure",settings_guest_diagnostic_export_failure);
    phase("settings_corrupt_restart",settings_guest_corrupt_restart);
    phase("tray",owned_tray_entry_cases);
    // These are independent entry cases, not the Full historical chain.
    // Finish the non-drag GUI/command paths before the exclusion case's
    // actual native Move; an input failure must not hide unrelated results.
    phase("settings_exclusion",settings_guest_exclusion);
    if(s.configuration=="Release"){
        independent_entry_ready(s,"portable.before",true);
        const auto original_pid=s.product_pid;const auto original_birth=process_birth(s.process);const auto original_product=s.product,original_log=s.product_log;
        const auto portable_mark=s.log.mark();owned_portable_zip_entry_case(s);
        // A returned portable case has passed its own normal-stop assertion,
        // even when a separately retained Move failure makes I12 FAIL. Verify
        // that exact current-scope receipt before starting another instance.
        std::string stopped;std::istringstream rows(s.log.since(portable_mark));std::string row;
        while(std::getline(rows,row))if(row.ends_with('}')&&row.find("\"case\":\"portable_normal_exit_and_actual_input_recovery\"")!=row.npos&&
            row.find("\"stage\":\"normal_no_argument_ZIP_instance\"")!=row.npos)stopped=row;
        const auto portable_pid=number(stopped,"\"exact_product_pid\":"),portable_birth=number(stopped,"\"exact_product_birth\":");GUITHREADINFO gui{sizeof(gui)};
        const bool native_idle=GetGUIThreadInfo(s.owner,&gui)&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
        const bool originals=std::all_of(s.frames.begin(),s.frames.end(),[&](const auto& frame){return own(s,frame.hwnd);});
        const auto binary=hash(original_product);
        s.log.check("independent_portable_normal_exit_before_run_copy_restart",!s.process&&!s.left&&!s.control&&native_idle&&originals&&
            !layout_ui_pending.load(std::memory_order_acquire)&&!FindWindowW(L"PaneBind.V1.Application",L"PaneBind")&&portable_pid&&portable_birth&&
            stopped.find("\"result\":\"PASS\"")!=stopped.npos&&s.product==original_product&&s.product_log==original_log&&
            !binary.empty()&&binary==hash(s.input_product),"\"actual_portable_stop_receipt\":"+quote(stopped)+
            ",\"old_chain_PASS_not_inferred\":true,\"restart_is_test_setup\":true");
        const auto startup_mark=read_product(s).size();s.log.check("independent_restart_original_run_copy",launch(s,L"--diagnostic",false));
        const auto new_birth=process_birth(s.process);HWND owner{};DWORD owner_pid{};
        const bool started=wait([&]{const auto startup=record(s,startup_mark,"startup");owner=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");
            owner_pid=0;if(owner)GetWindowThreadProcessId(owner,&owner_pid);
            return number(startup,"\"process_id\":")==s.product_pid&&owner&&owner_pid==s.product_pid;},8000);
        s.log.check("independent_restart_actual_new_identity_and_startup",started&&s.process&&new_birth&&
            (s.product_pid!=original_pid||new_birth!=original_birth)&&(s.product_pid!=portable_pid||new_birth!=portable_birth)&&
            GetProcessId(s.process)==s.product_pid&&medium(s.process)&&hash(s.product)==binary,
            "\"previous_run_PID\":"+std::to_string(original_pid)+",\"portable_PID\":"+std::to_string(portable_pid)+
            ",\"new_run_PID\":"+std::to_string(s.product_pid)+",\"new_birth\":"+std::to_string(new_birth)+
            ",\"test_setup_not_old_chain_PASS\":true,\"failed_portable_Move_not_retried\":true");
        independent_entry_ready(s,"original_run_copy_restart.after",true);
    }else s.log.emit("independent_entry_phase_result","\"phase\":\"portable_ordinary_entry\",\"result\":\"NOT_RUN\",\"reason\":\"Release_only_actual_package_required\"");
    }else s.log.emit("scoped_prior_components_not_repeated","\"suite\":"+quote(s.suite)+",\"settings_portable_rapid_cases\":\"NOT_RUN_IN_THIS_BATCH\",\"earlier_evidence_not_rebound\":true");
    if(s.suite=="EntryAndSettings"){
        cleanup(s);
        s.log.emit("entry_settings_summary","\"suite\":\"EntryAndSettings\",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
            ",\"scope\":\"existing_entry_settings_and_Release_portable_only\",\"owned_Core_I11\":\"NOT_RUN\",\"application_matrix\":\"NOT_RUN\",\"Full_suite\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
        return;
    }
    if(owned_components_only){
        s.log.emit("owned_components_skipped_scopes","\"suite\":\"OwnedComponentsOnly\",\"settings\":\"NOT_RUN\",\"portable\":\"NOT_RUN\",\"rapid_terminal_motion\":\"NOT_RUN\",\"I11\":\"NOT_RUN\",\"Release_idle60\":\"NOT_RUN\",\"Explorer\":\"NOT_RUN\",\"applications\":\"NOT_RUN\",\"prior_evidence_not_rebound\":true");
    }
    // Execute existing components with their own actual setup, not the blocked
    // Full eight-window history or a newly invented successful continuation.
    phase("owned_dock_stretch_component",owned_dock_stretch_chains);
    phase("owned_interruption_component",owned_interruption_cases);
    if(s.configuration=="Release"&&!owned_components_only){
        phase("owned_finite_I11_component",owned_finite_I11_chain);
        const auto idle=[](State& state){
            const auto measured=product::guest_test::measure_idle_60s(state.process,state.product_pid,process_birth(state.process));
            state.log.emit("Release_actual_idle_measurement",measured.json_fields());
            state.log.check("independent_Release_idle60_actual_budget",measured.completed&&measured.one_core_cpu_percent<1.0&&
                measured.after.working_bytes<=80ULL*1024*1024,"\"independent_idle_only_not_I11_or_version_PASS\":true");
        };
        phase("Release_idle60_independent",idle);
    }
    // Native failure injection must not hide unrelated real user functions.
    phase("owned_visual_mapping_component",visual_mapping_flow);
    phase("owned_temporary_visual_inflight_component",owned_temporary_visual_inflight_case);
    phase("owned_native_grid_constraint_component",owned_native_grid_constraint_case);
    phase("owned_second_member_native_failure_component",owned_second_member_native_failure_case);
    if(owned_components_only){
        cleanup(s);
        s.log.emit("owned_components_summary","\"suite\":\"OwnedComponentsOnly\",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
            ",\"scope\":\"existing_owned_dock_stretch_interruption_visual_inflight_grid_batch_components_only\",\"owned_I02_acceptance\":\"NOT_GRANTED\",\"I11_acceptance\":\"NOT_GRANTED\",\"settings\":\"NOT_RUN\",\"portable\":\"NOT_RUN\",\"Release_idle60\":\"NOT_RUN\",\"Explorer\":\"NOT_RUN\",\"application_matrix\":\"NOT_RUN\",\"Full_suite\":\"NOT_RUN\",\"whole_version_acceptance\":\"NOT_GRANTED\",\"human_uat\":\"NOT_RUN\"");
        return;
    }
    // Successful exact Explorer roots remain available for the existing mixed
    // application group. The applications scope cleans them after that group;
    // an unsuccessful Explorer chain still cleans its own registered roots.
    const auto explorer=[](State& state){try{explorer_flow(state);}catch(...){cleanup_explorers(state);throw;}};
    phase("Explorer_actual_chain",explorer);
    const auto applications=[](State& state){try{application_matrix_flow(state);}catch(...){cleanup_applications(state);cleanup_explorers(state);throw;}
        cleanup_applications(state);cleanup_explorers(state);};
    phase("application_matrix",applications);
    cleanup(s);
    s.log.emit("independent_entry_summary","\"suite\":"+quote(s.suite)+",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
        ",\"owned_I02_acceptance\":\"NOT_GRANTED\",\"I11_acceptance\":\"NOT_GRANTED\",\"Full_suite\":\"NOT_RUN\","+
        "\"component_results_do_not_infer_whole_chain_PASS\":true,\"Explorer_result_requires_own_phase_evidence\":true,\"human_uat\":\"NOT_RUN\"");
}
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
        const auto route=record(s,0,"input_route");
        if(!route.empty())s.log.check("production_exact_guest_capture_route_selected",
            route.find("\"guest_consumed_down_capture\":true")!=route.npos,
            "\"actual_product_route_receipt\":"+quote(route)+",\"older_route_not_substituted_for_new_resource_validation\":true");
        const HWND app=FindWindowW(L"PaneBind.V1.Application",L"PaneBind");DWORD pid{};if(app)GetWindowThreadProcessId(app,&pid);
        s.log.check("single_exact_product_owner",app&&pid==s.product_pid);
        const auto mark=read_product(s).size();DWORD second{};
        const bool second_ok=launch(s,L"--status",true,&second)&&
            FindWindowW(L"PaneBind.V1.Application",L"PaneBind")==app&&wait([&]{return !record(s,mark,"status").empty();});
        s.log.check("second_instance_status",second_ok,
            "\"secondary_exit\":"+std::to_string(second));
        s.log.emit("actual_suite","\"suite\":"+quote(s.suite)+",\"Full_is_default\":true,\"partial_suite_is_not_Full_PASS\":true");
        if(s.suite=="IndependentEntry"||s.suite=="CoreAndApplications"||s.suite=="EntryAndSettings"||s.suite=="OwnedComponentsOnly"){
            independent_entry_flow(s,s.suite=="CoreAndApplications"||s.suite=="OwnedComponentsOnly");s.result=s.log.failed?1:0;
        }else if(s.suite=="InterruptionOnly"){
            bool completed=false;
            try{owned_interruption_cases(s);completed=true;}
            catch(const std::exception& error){s.log.failed=true;
                s.log.emit("interruption_only_failure","\"reason\":"+quote(error.what())+",\"original_failure_preserved\":true");}
            cleanup(s);s.result=s.log.failed?1:0;
            s.log.emit("interruption_only_summary","\"suite\":\"InterruptionOnly\",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
                ",\"existing_I08_flow_returned\":"+(completed?"true":"false")+
                ",\"I11\":\"NOT_RUN\",\"Release_idle60\":\"NOT_RUN\",\"Explorer\":\"NOT_RUN\",\"applications\":\"NOT_RUN\",\"Full_suite\":\"NOT_RUN\",\"whole_version_acceptance\":\"NOT_GRANTED\",\"human_uat\":\"NOT_RUN\"");
        }else if(s.suite=="ApplicationsOnly"){
            try{
                independent_entry_ready(s,"application_matrix.before",true);
                application_matrix_flow(s);
                independent_entry_ready(s,"application_matrix.after",false);
            }catch(const std::exception& error){s.log.failed=true;s.log.emit("applications_only_failure","\"reason\":"+quote(error.what())+",\"original_failure_preserved\":true");}
            const auto finish=[&](const char* stage,void(*run)(State&)){
                try{run(s);}catch(const std::exception& error){s.log.failed=true;s.log.emit("applications_only_cleanup_failure","\"stage\":"+quote(stage)+",\"reason\":"+quote(error.what()));}
            };
            finish("exact_application_roots",cleanup_applications);finish("exact_Explorer_roots",cleanup_explorers);finish("product_stop_and_owned_input_recovery",cleanup);
            s.result=s.log.failed?1:0;
            s.log.emit("applications_only_summary","\"suite\":\"ApplicationsOnly\",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
                ",\"scope\":\"existing_real_application_matrix_only\",\"missing_application_or_mixed_group_is_not_skipped\":true,"+
                "\"Explorer_provisioning\":\"NOT_RUN\",\"Explorer_chain\":\"NOT_RUN\",\"previous_Explorer_environment_facts_not_rebound\":true,"+
                "\"owned_Core_I11\":\"NOT_RUN\",\"Full_suite\":\"NOT_RUN\",\"whole_version_acceptance\":\"NOT_GRANTED\",\"human_uat\":\"NOT_RUN\"");
        }else if(s.suite=="SettingsDiagnostic"){
            owned_settings_diagnostic_case(s);
            if(s.configuration=="Release")owned_portable_zip_entry_case(s);
            cleanup(s);s.result=s.log.failed?1:0;
            s.log.emit("settings_diagnostic_summary","\"suite\":\"SettingsDiagnostic\",\"result\":"+quote(s.log.failed?"FAIL":"PASS")+
                ",\"scope\":\"settings_diagnostic_and_Release_portable_only\",\"actual_entry\":\"settings_footer\",\"scoped_only\":true,"+
                "\"tray_acceptance\":\"NOT_GRANTED\",\"whole_I01_or_I12_acceptance\":\"NOT_GRANTED\",\"Full_suite\":\"NOT_RUN\",\"human_uat\":\"NOT_RUN\"");
        }else{
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
        owned_rapid_terminal_move(s);
        owned_settings_screenshots(s);
        settings_gui_flow(s);
        owned_settings_diagnostic_case(s);
        // Complete independent owned DEV before retrying application-specific
        // environment gaps. A failed application admission is not permission
        // to bypass it, nor a reason to leave unrelated product paths untested.
        owned_layout_cases(s);
        owned_settings_lifecycle_cases(s);
        owned_tray_entry_cases(s);
        owned_mouse_mapping_cases(s);
        owned_dock_stretch_chains(s);
        owned_interruption_cases(s);
        if(s.configuration=="Release"){
            owned_finite_I11_chain(s);
            const auto idle=product::guest_test::measure_idle_60s(s.process,s.product_pid,process_birth(s.process));
            s.log.emit("Release_actual_idle_measurement",idle.json_fields());
            if(!idle.completed||idle.one_core_cpu_percent>=1.0||idle.after.working_bytes>80ULL*1024*1024)s.log.failed=true;
        }
        visual_mapping_flow(s);
        // Keep native failure-injection probes after independent owned
        // functions. A failure is retained, not turned into a successful
        // chain; only observed cleanup permits the next independent scenario.
        const auto independent_native_case=[&](const char* name,void(*scenario)(State&)){
            const HANDLE original_process=s.process;const DWORD original_pid=s.product_pid;
            const auto original_birth=original_process?process_birth(original_process):0;
            bool scenario_failed=false;
            try{scenario(s);}
            catch(const std::exception& error){scenario_failed=true;s.log.failed=true;
                s.log.emit("independent_native_scenario_failure","\"scenario\":"+quote(name)+",\"reason\":"+quote(error.what())+
                    ",\"original_failure_preserved\":true,\"functional_PASS\":false");}
            const bool driver_released=!s.left&&!s.control;
            const bool layout_idle=!layout_ui_pending.load(std::memory_order_acquire);
            bool originals_exact=true;for(const auto& frame:s.frames)originals_exact=own(s,frame.hwnd)&&originals_exact;
            GUITHREADINFO gui{sizeof(gui)};SetLastError(0);const bool gui_known=GetGUIThreadInfo(s.owner,&gui)!=FALSE;
            const DWORD gui_error=gui_known?0:GetLastError();
            const bool gui_idle=gui_known&&!gui.hwndCapture&&!gui.hwndMoveSize&&!gui.hwndMenuOwner;
            const bool product_current=original_process&&s.process==original_process&&s.product_pid==original_pid&&original_pid&&original_birth&&
                GetProcessId(original_process)==original_pid&&process_birth(original_process)==original_birth&&
                WaitForSingleObject(original_process,0)==WAIT_TIMEOUT;
            std::string status;bool status_requested=false,status_observed=false;DWORD status_exit=STILL_ACTIVE;
            if(driver_released&&layout_idle&&originals_exact&&gui_idle&&product_current){
                const auto status_mark=read_product(s).size();status_requested=launch(s,L"--status",true,&status_exit);
                if(status_requested)status_observed=wait([&]{status=record(s,status_mark,"status");return !status.empty();},1000);
            }
            const bool product_idle=status_observed&&status_exit==0&&number(status,"\"process_id\":")==original_pid&&
                status.find("\"gesture_active\":false")!=std::string::npos&&
                status.find("\"native_operation_pending\":false")!=std::string::npos&&
                status.find("\"native_completion_unknown\":false")!=std::string::npos&&
                status.find("\"visual_restore_pending\":false")!=std::string::npos;
            s.log.check("independent_native_resources_safe_to_continue",driver_released&&layout_idle&&originals_exact&&gui_idle&&product_current&&
                status_requested&&product_idle,"\"scenario\":"+quote(name)+",\"scenario_exception_preserved\":"+(scenario_failed?"true":"false")+
                ",\"driver_buttons_released_ledger\":"+(driver_released?"true":"false")+",\"layout_UI_pending_empty\":"+(layout_idle?"true":"false")+
                ",\"original_three_exact_owned\":"+(originals_exact?"true":"false")+",\"source_UI_GUI_known\":"+(gui_known?"true":"false")+
                ",\"source_UI_GUI_error\":"+std::to_string(gui_error)+",\"source_UI_capture\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
                ",\"source_UI_move_size\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
                ",\"source_UI_menu_owner\":"+(gui_known?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMenuOwner)):"null")+
                ",\"exact_product_process_alive\":"+(product_current?"true":"false")+",\"fresh_status_requested\":"+(status_requested?"true":"false")+
                ",\"fresh_status_observed\":"+(status_observed?"true":"false")+",\"fresh_status_exit\":"+std::to_string(status_exit)+
                ",\"fresh_status\":"+(status.empty()?"null":quote(status))+",\"independent_scenario_continuation_only\":true,\"failed_chain_PASS_inferred\":false");
        };
        independent_native_case("temporary_visual_inflight",owned_temporary_visual_inflight_case);
        independent_native_case("native_grid_constraint",owned_native_grid_constraint_case);
        independent_native_case("second_member_native_failure",owned_second_member_native_failure_case);
        try{explorer_flow(s);}
        catch(const std::exception& error){s.log.failed=true;s.log.emit("Explorer_core_result","\"result\":\"BLOCKED_OR_FAIL\",\"reason\":"+quote(error.what())+",\"other_application_classes_continue_after_exact_cleanup\":true");cleanup_explorers(s);}
        application_matrix_flow(s);
        cleanup_explorers(s);
        s.log.check("Full_application_registered_roots_closed_before_portable",std::none_of(s.created_applications.begin(),s.created_applications.end(),
            [](const auto& target){return target&&target->accepted&&!target->cleanup_confirmed;}),
            "\"unknown_identity_windows_not_closed_to_force_PASS\":true");
        if(s.configuration=="Release")owned_portable_zip_entry_case(s);
        cleanup(s);
        if(!s.log.failed)s.log.emit("functional_summary","\"owned_I02\":\"PASS\",\"production_commands\":\"PASS\",\"normal_stop_and_client_delivery\":\"PASS\",\"Explorer_I02\":\"PASS\",\"human_uat\":\"NOT_RUN\"");
        s.result=s.log.failed?1:0;
        }
    }catch(const std::exception& error){
        try{s.log.emit("failure","\"reason\":"+quote(error.what()));cleanup(s);cleanup_applications(s);cleanup_explorers(s);}catch(...){}s.result=1;
    }
    // This read-only Catalog owns a WinEvent hook and a COM interface created
    // on THIS driver STA. An unconfirmed application close may retain its
    // registration until here, but cannot transfer destruction to State on
    // the non-COM fixture UI thread after join(). Preserve the close failure;
    // releasing our observer is not authority to close that application.
    if(s.target_catalog){
        try{s.log.emit("driver_catalog_owner_release_begin","\"actual_thread_id\":"+std::to_string(GetCurrentThreadId())+
            ",\"before_driver_CoUninitialize\":true,\"application_close_failure_not_erased\":true");}
        catch(...){s.log.failed=true;s.result=1;}
        s.target_catalog.reset();
        try{s.log.emit("driver_catalog_owner_release_complete","\"actual_thread_id\":"+std::to_string(GetCurrentThreadId())+
            ",\"actual_catalog_empty\":true,\"before_driver_CoUninitialize\":true,\"application_close_PASS_inferred\":false");}
        catch(...){s.log.failed=true;s.result=1;}
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
    const auto package=read(input/L"manifest.json",256*1024);
    if((s.suite!="Full"&&s.suite!="IndependentEntry"&&s.suite!="SettingsDiagnostic"&&s.suite!="CoreAndApplications"&&s.suite!="ApplicationsOnly"&&s.suite!="EntryAndSettings"&&s.suite!="OwnedComponentsOnly"&&s.suite!="InterruptionOnly")||portable_field(package,"guest_suite")!=s.suite)
        return guard_reject(s,104,"fixture_suite_manifest_mismatch");
    s.log.emit("guard_suite","\"requested_suite\":"+quote(s.suite)+",\"readonly_manifest_exact_match\":true,\"input_sent\":false");
    // The bootstrap verifies immutable per-file hashes before creating this
    // Medium process. Bind this driver's own baked identity and both actual
    // executable bytes independently; never relabel product evidence with the
    // newer driver commit. The prefix contains only unique flat identity fields.
    const auto file_inventory=package.find("\"files\"");
    if(file_inventory==std::string::npos)return guard_reject(s,105,"split_binary_identity_missing");
    const std::string_view package_identity(package.data(),file_inventory);
    const auto product_source=portable_field(package_identity,"product_source_commit");
    const auto driver_source=portable_field(package_identity,"driver_source_commit");
    if(!portable_hex(product_source,40,false)||driver_source!=PANEBIND_BUILD_SHA||
       portable_field(package_identity,"source_commit")!=product_source||
       portable_field(package_identity,"guest_contract")!="panebind-v1-guest-contract/2"||
       portable_field(package_identity,"product_interface")!="panebind-v1-product-guest-interface/1")
        return guard_reject(s,105,"split_binary_identity_or_interface_mismatch");
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
    std::array<wchar_t,32768> driver_path{};
    const DWORD driver_length=GetModuleFileNameW(nullptr,driver_path.data(),static_cast<DWORD>(driver_path.size()));
    const fs::path expected_driver=debug?input/L"Debug"/L"panebind-v1-guest-fixture.exe":input/L"panebind-v1-guest-fixture.exe";
    const auto driver_hash=driver_length&&driver_length<driver_path.size()?hash(driver_path.data()):std::string{};
    const auto identity_suffix=debug?"debug_sha256":"release_sha256";
    if(!driver_length||driver_length>=driver_path.size()||!equal(driver_path.data(),expected_driver.wstring())||
       source_hash!=portable_field(package_identity,std::string("product_")+identity_suffix)||
       driver_hash.empty()||driver_hash!=portable_field(package_identity,std::string("driver_")+identity_suffix))
        return guard_reject(s,105,"split_binary_actual_hash_mismatch");
    s.log.emit("guard_binary_identity","\"product_source_commit\":"+quote(product_source)+",\"product_sha256\":"+quote(source_hash)+
        ",\"driver_source_commit\":"+quote(driver_source)+",\"driver_sha256\":"+quote(driver_hash)+
        ",\"packaging_head\":"+quote(portable_field(package_identity,"packaging_head"))+
        ",\"guest_contract\":\"panebind-v1-guest-contract/2\",\"input_sent\":false");
    if(!read(actual.parent_path()/L"portable.flag",128).starts_with("PaneBind V1 portable test candidate"))return guard_reject(s,98,"portable_run_marker_missing");
    if(FindWindowW(L"PaneBind.V1.Application",L"PaneBind"))return guard_reject(s,99,"preexisting_product_instance_present");
    s.product_source=product_source;s.input_product=source;s.product=actual;s.output=output;return 0;
}
#include "platform/windows/product/v1_guest_input_actor.h"
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    State s;s.instance=instance;s.owner=GetCurrentThreadId();int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 64;
    if(count>=2&&std::wstring_view(args[1])==L"--input-actor-once"){
        int result=78;try{result=input_actor_child(count,args);}catch(...){}LocalFree(args);return result;
    }
    if(count==2&&std::wstring_view(args[1])==L"--build-identity"){
        constexpr char identity[]="{\"product\":\"PaneBind V1 guest fixture\",\"source_commit\":\"" PANEBIND_BUILD_SHA "\",\"host_synthetic_input\":\"forbidden\",\"requires_medium_guest\":true,\"guest_contract\":\"panebind-v1-guest-contract/2\",\"product_interface\":\"panebind-v1-product-guest-interface/1\"}\n";
        DWORD written{};const auto output=GetStdHandle(STD_OUTPUT_HANDLE);
        if(output&&output!=INVALID_HANDLE_VALUE)WriteFile(output,identity,static_cast<DWORD>(sizeof(identity)-1),&written,nullptr);
        LocalFree(args);return 0;
    }
    bool parsed=count==7||count==9;unsigned seen{};
    if(parsed)for(int n=1;n<count;n+=2){const std::wstring name=args[n];unsigned field{};
        if(name==L"--run-id"){field=1;s.run=args[n+1];}else if(name==L"--output"){field=2;s.output=args[n+1];}
        else if(name==L"--product"){field=4;s.input_product=args[n+1];}else if(name==L"--suite"){field=8;
            const std::wstring_view value(args[n+1]);if(value==L"Full")s.suite="Full";else if(value==L"IndependentEntry")s.suite="IndependentEntry";
            else if(value==L"SettingsDiagnostic")s.suite="SettingsDiagnostic";
            else if(value==L"CoreAndApplications")s.suite="CoreAndApplications";else if(value==L"ApplicationsOnly")s.suite="ApplicationsOnly";else if(value==L"EntryAndSettings")s.suite="EntryAndSettings";else if(value==L"OwnedComponentsOnly")s.suite="OwnedComponentsOnly";else if(value==L"InterruptionOnly")s.suite="InterruptionOnly";else parsed=false;}
        else parsed=false;if(field&&(seen&field))parsed=false;seen|=field;}
    parsed=parsed&&(seen&7)==7&&(count==7?seen==7:seen==15);
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
        s.log.emit("guard","\"run_id\":"+quote(utf8(s.run))+",\"configuration\":"+quote(s.configuration)+",\"suite\":"+quote(s.suite)+",\"actual_integrity\":\"Medium\",\"elevated\":false,\"driver_has_console\":"+
            std::string(GetConsoleWindow()?"true":"false")+",\"input_executable\":"+quote(utf8(s.input_product.wstring()))+",\"actual_executable\":"+quote(utf8(s.product.wstring()))+
            ",\"binary_sha256\":"+quote(source)+",\"human_uat\":\"NOT_RUN\"");
        s.class_name=L"PaneBind.V1.OwnedFixture."+s.run;WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=procedure;wc.lpszClassName=s.class_name.c_str();wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
        if(!RegisterClassW(&wc))return 1;
        for(std::size_t n=0;n<3;++n){const auto title=L"PaneBind V1 — "+std::wstring(1,static_cast<wchar_t>(L'A'+n));
            s.frames[n].hwnd=CreateWindowExW(0,s.class_name.c_str(),title.c_str(),WS_OVERLAPPEDWINDOW,100+static_cast<int>(n)*250,80,260,260,nullptr,nullptr,instance,&s.frames[n]);
            if(!s.frames[n].hwnd)throw std::runtime_error("owned_window_creation_failed");ShowWindow(s.frames[n].hwnd,SW_SHOWNOACTIVATE);UpdateWindow(s.frames[n].hwnd);}
        // Every suite passes the same exact disposable-guest guards.
        // Observe the existing Full gestures too, so an actual missing hook
        // UP can be compared with Raw delivery without changing termination.
        raw_mouse_observer_start(s,s.frames[0].hwnd);
        MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);s.driver=std::thread([&]{drive(s);});
        while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}if(s.driver.joinable())s.driver.join();
        if(guest_raw_mouse.target.load()&&!raw_mouse_observer_stop(s))s.log.failed=true;
        bool destroyed=true;for(auto& f:s.frames)if(f.hwnd&&IsWindow(f.hwnd)){if(!own(s,f.hwnd)){destroyed=false;continue;}destroyed=DestroyWindow(f.hwnd)&&destroyed;}UnregisterClassW(s.class_name.c_str(),instance);
        s.log.emit("driver_shutdown","\"result\":"+quote(s.result==0&&!s.log.failed&&destroyed?"PASS":"FAIL")+",\"suite\":"+quote(s.suite)+",\"owned_windows_destroyed\":"+std::string(destroyed?"true":"false")+
            ",\"product_process_closed\":"+std::string(!s.process?"true":"false")+",\"scope\":\"exact_guest_targets_not_full_acceptance\",\"human_uat\":\"NOT_RUN\"");
        return s.result==0&&!s.log.failed&&destroyed?0:1;
    }catch(...){if(s.driver.joinable())s.driver.join();
        if(guest_raw_mouse.target.load())try{raw_mouse_observer_stop(s);}catch(...){}
        for(auto& f:s.frames)if(own(s,f.hwnd))DestroyWindow(f.hwnd);return 1;}
}
