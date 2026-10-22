#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace panebind::platform::windows::operations {

// A narrowly bound alternative to the historical Default-desktop route. This
// is resource/context evidence, NEVER target, gesture or placement authority.
// Only the product's exact manifest/path/hash guest factory constructs it.
struct RunDesktopFacts {
    bool bound{},same_session{},medium{},session_active_unlocked{},station_matches{},owner_matches{},owner_active{},input_matches{},input_active{},held_matches{};
};
[[nodiscard]] constexpr bool run_desktop_active(const RunDesktopFacts& f) noexcept {
    return f.bound&&f.same_session&&f.medium&&f.session_active_unlocked&&f.station_matches&&f.owner_matches&&f.owner_active&&f.input_matches&&f.input_active&&f.held_matches;
}
struct RunDesktopSourceFacts {
    bool active{},identity_stable{},first_complete{},second_complete{},first_member{},second_member{};
    bool source_name_query{},source_name_matches{},source_io_query{},source_io_active{};
    DWORD thread_desktop_error{},source_name_error{},source_io_error{};
};
[[nodiscard]] constexpr bool run_desktop_source_allowed(const RunDesktopSourceFacts& f) noexcept {
    // Unknown foreign GetThreadDesktop/UOI facts stay unknown. Two complete
    // enumerations are an explicitly DIFFERENT evidence source; a known
    // contradictory name or IO read cannot be overridden by membership.
    return f.active&&f.identity_stable&&f.first_complete&&f.second_complete&&f.first_member&&f.second_member&&
        (!f.source_name_query||f.source_name_matches)&&(!f.source_io_query||f.source_io_active);
}
[[nodiscard]] constexpr bool current_virtual_desktop_allowed(HRESULT result,BOOL on_current) noexcept {
    return result==S_OK&&on_current!=FALSE;
}

class InteractiveDesktopContext final {
    const std::wstring name_;
    const std::string run_;
    const DWORD session_;
    const HDESK desktop_;
    InteractiveDesktopContext(std::string run,DWORD session,HDESK desktop)
        :name_(L"PaneBindV1_"+std::wstring(run.begin(),run.end())),run_(std::move(run)),session_(session),desktop_(desktop){}
    static bool named(HANDLE object,std::wstring_view expected) noexcept {
        std::array<wchar_t,128> actual{};DWORD length{};
        return object&&GetUserObjectInformationW(object,UOI_NAME,actual.data(),sizeof(actual),&length)&&
            CompareStringOrdinal(actual.data(),-1,expected.data(),static_cast<int>(expected.size()),TRUE)==CSTR_EQUAL;
    }
    static bool member(HDESK desktop,HWND source,bool& complete) noexcept {
        struct Sample {HWND source;unsigned roots{},matches{};bool overflow{};} sample{source};
        complete=EnumDesktopWindows(desktop,[](HWND h,LPARAM value)->BOOL{
            auto& s=*reinterpret_cast<Sample*>(value);
            if(++s.roots>1024){s.overflow=true;return FALSE;}
            if(h==s.source)++s.matches;return TRUE;
        },reinterpret_cast<LPARAM>(&sample))!=FALSE&&!sample.overflow;
        return complete&&sample.matches==1;
    }
    static std::uint64_t birth(HANDLE process) noexcept {
        FILETIME created{},exited{},kernel{},user{};
        if(!process||WaitForSingleObject(process,0)!=WAIT_TIMEOUT||!GetProcessTimes(process,&created,&exited,&kernel,&user))return 0;
        return (static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime;
    }
public:
    InteractiveDesktopContext(const InteractiveDesktopContext&)=delete;
    ~InteractiveDesktopContext(){if(desktop_)CloseDesktop(desktop_);}
    [[nodiscard]] const std::wstring& name()const noexcept{return name_;}
    [[nodiscard]] const std::string& run()const noexcept{return run_;}
    [[nodiscard]] HDESK desktop()const noexcept{return desktop_;} // borrowed read-only, lifetime held by Ref
    // This factory does no disk IO, desktop switch, ACL change or permission
    // repair. The caller MUST have established the exact immutable guest run.
    [[nodiscard]] static std::shared_ptr<const InteractiveDesktopContext> open_verified_guest(std::string_view run){
        if(run.size()!=32)return {};
        for(char c:run)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return {};
        DWORD session{};if(!ProcessIdToSessionId(GetCurrentProcessId(),&session)||!session)return {};
        const std::wstring name=L"PaneBindV1_"+std::wstring(run.begin(),run.end());
        const auto held=OpenDesktopW(name.c_str(),0,FALSE,DESKTOP_READOBJECTS);
        if(!held)return {};
        auto result=std::shared_ptr<const InteractiveDesktopContext>(new InteractiveDesktopContext(std::string(run),session,held));
        if(!result->active())return {};return result;
    }
    [[nodiscard]] RunDesktopFacts facts()const noexcept {
        RunDesktopFacts f;f.bound=desktop_&&!run_.empty();DWORD current_session{};
        f.same_session=ProcessIdToSessionId(GetCurrentProcessId(),&current_session)&&current_session==session_&&session_;
        HANDLE token{};
        if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){
            alignas(TOKEN_MANDATORY_LABEL) std::array<BYTE,512> label{};DWORD bytes{},ui{},app{};TOKEN_ELEVATION elevation{};
            const bool read=GetTokenInformation(token,TokenIntegrityLevel,label.data(),static_cast<DWORD>(label.size()),&bytes)&&
                GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&bytes)&&
                GetTokenInformation(token,TokenUIAccess,&ui,sizeof(ui),&bytes)&&
                GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&bytes);
            const auto sid=reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
            f.medium=read&&IsValidSid(sid)&&*GetSidSubAuthorityCount(sid)&&!elevation.TokenIsElevated&&!ui&&!app&&
                *GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1)==SECURITY_MANDATORY_MEDIUM_RID;
            CloseHandle(token);
        }
        LPWSTR state{};DWORD bytes{};
        if(WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session_,WTSSessionInfoEx,&state,&bytes)&&state&&bytes>=sizeof(WTSINFOEXW)){
            const auto& info=*reinterpret_cast<const WTSINFOEXW*>(state);
            f.session_active_unlocked=info.Level==1&&info.Data.WTSInfoExLevel1.SessionId==session_&&
                info.Data.WTSInfoExLevel1.SessionState==WTSActive&&info.Data.WTSInfoExLevel1.SessionFlags==WTS_SESSIONSTATE_UNLOCK;
        }
        if(state)WTSFreeMemory(state);
        f.station_matches=named(GetProcessWindowStation(),L"WinSta0");
        const auto owner=GetThreadDesktop(GetCurrentThreadId());f.owner_matches=named(owner,name_);
        BOOL owner_io{};DWORD size{};
        f.owner_active=owner&&GetUserObjectInformationW(owner,UOI_IO,&owner_io,sizeof(owner_io),&size)&&owner_io;
        f.held_matches=named(desktop_,name_);
        const auto input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
        if(input){BOOL io{};DWORD length{};f.input_matches=named(input,name_);
            f.input_active=GetUserObjectInformationW(input,UOI_IO,&io,sizeof(io),&length)&&io;
            if(!CloseDesktop(input))f.input_active=false;}
        return f;
    }
    [[nodiscard]] bool active()const noexcept{return run_desktop_active(facts());}
    // Bounded read-only adapter preflight, NOT a hook callback. Native GUI,
    // foreground, Medium/SID, VDM and per-generation checks remain separate.
    [[nodiscard]] RunDesktopSourceFacts observe(HWND source,DWORD pid,DWORD tid,std::uint64_t expected_birth=0)const noexcept {
        RunDesktopSourceFacts f;f.active=active();if(!f.active||!source||!pid||!tid)return f;
        DWORD actual_pid{};if(GetWindowThreadProcessId(source,&actual_pid)!=tid||actual_pid!=pid||
            !IsWindow(source)||GetAncestor(source,GA_ROOT)!=source)return f;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);if(!process)return f;
        const auto before=birth(process);DWORD source_session{};
        bool stable=before&&(!expected_birth||before==expected_birth)&&ProcessIdToSessionId(pid,&source_session)&&source_session==session_;
        SetLastError(ERROR_SUCCESS);const auto foreign=GetThreadDesktop(tid);f.thread_desktop_error=foreign?0:GetLastError();
        if(foreign){std::array<wchar_t,128> actual{};DWORD length{};BOOL io{};
            SetLastError(ERROR_SUCCESS);f.source_name_query=GetUserObjectInformationW(foreign,UOI_NAME,actual.data(),sizeof(actual),&length)!=FALSE;
            f.source_name_error=f.source_name_query?0:GetLastError();
            f.source_name_matches=f.source_name_query&&CompareStringOrdinal(actual.data(),-1,name_.c_str(),-1,TRUE)==CSTR_EQUAL;
            SetLastError(ERROR_SUCCESS);f.source_io_query=GetUserObjectInformationW(foreign,UOI_IO,&io,sizeof(io),&length)!=FALSE;
            f.source_io_error=f.source_io_query?0:GetLastError();f.source_io_active=f.source_io_query&&io;}
        f.first_member=member(desktop_,source,f.first_complete);
        f.second_member=member(desktop_,source,f.second_complete);
        f.identity_stable=stable&&birth(process)==before&&IsWindow(source)&&GetAncestor(source,GA_ROOT)==source&&
            GetWindowThreadProcessId(source,&actual_pid)==tid&&actual_pid==pid&&active();
        CloseHandle(process);return f;
    }
};
using InteractiveDesktopContextRef=std::shared_ptr<const InteractiveDesktopContext>;

} // namespace panebind::platform::windows::operations
