// Explicit read-only host check. No HWND creation, hook, Raw Input, input
// injection, cursor movement, desktop switching or window manipulation.
#include <windows.h>
#include <wtsapi32.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::int64_t ticks() { LARGE_INTEGER n{};return QueryPerformanceCounter(&n)?n.QuadPart:0; }
std::string boolean(bool value) { return value?"true":"false"; }
std::string quote(std::wstring_view value) {
    std::string out="\"";
    for(const wchar_t c:value){
        if(c==L'"'||c==L'\\'){out+='\\';out+=static_cast<char>(c);}
        else if(c>=32&&c<127)out+=static_cast<char>(c);
        else {constexpr char hex[]="0123456789abcdef";out+="\\u";
            for(int shift=12;shift>=0;shift-=4)out+=hex[(static_cast<unsigned>(c)>>shift)&15];}
    }
    return out+'"';
}
struct Context {
    std::int64_t start{},finish{};std::wstring input_name,thread_name,station_name;
    DWORD input_error{},thread_error{},station_error{},session_error{},session_id{};
    std::uintptr_t foreground{};DWORD foreground_pid{},foreground_tid{};
    DWORD caller_integrity{},foreground_integrity{},caller_integrity_error{},foreground_integrity_error{};
    bool caller_integrity_known{},foreground_integrity_known{},hook_access{},foreground_known{},complete{};
    DWORD session_level{},session_flags{};int session_state{-1};
    bool input_read{},thread_read{},station_read{},session_read{},valid{};
};
bool integrity(HANDLE process,DWORD& level,DWORD& error) {
    HANDLE token{};SetLastError(0);
    if(!process){error=ERROR_INVALID_HANDLE;return false;}
    if(!OpenProcessToken(process,TOKEN_QUERY,&token)){error=GetLastError();return false;}
    DWORD bytes{};GetTokenInformation(token,TokenIntegrityLevel,nullptr,0,&bytes);
    if(!bytes){error=GetLastError();CloseHandle(token);return false;}
    std::vector<unsigned char> buffer(bytes);SetLastError(0);
    const bool ok=GetTokenInformation(token,TokenIntegrityLevel,buffer.data(),bytes,&bytes)!=FALSE;
    error=ok?0:GetLastError();bool known=false;
    if(ok){const auto& label=*reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
        if(IsValidSid(label.Label.Sid)){
            const auto count=*GetSidSubAuthorityCount(label.Label.Sid);
            if(count){level=*GetSidSubAuthority(label.Label.Sid,count-1);known=true;}
        }
    }
    if(ok&&!known)error=ERROR_INVALID_SID;
    CloseHandle(token);return known;
}
bool object_name(HANDLE handle,std::wstring& name,DWORD& error) {
    wchar_t buffer[256]{};DWORD needed{};SetLastError(0);
    if(!handle||!GetUserObjectInformationW(handle,UOI_NAME,buffer,sizeof(buffer),&needed)){
        error=GetLastError();return false;
    }
    name=buffer;return true;
}
Context context() {
    Context s;s.start=ticks();SetLastError(0);
    // Requesting read/hook-access on a handle does not install a hook. A mere
    // READOBJECTS success would not prove GetAsyncKeyState's documented access.
    const auto input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS|DESKTOP_HOOKCONTROL);
    s.input_error=input?0:GetLastError();
    s.hook_access=input!=nullptr;
    if(input){s.input_read=object_name(input,s.input_name,s.input_error);CloseDesktop(input);}
    s.thread_read=object_name(GetThreadDesktop(GetCurrentThreadId()),s.thread_name,s.thread_error);
    s.station_read=object_name(GetProcessWindowStation(),s.station_name,s.station_error);
    LPWSTR memory{};DWORD bytes{};SetLastError(0);
    const bool queried=WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,WTS_CURRENT_SESSION,WTSSessionInfoEx,&memory,&bytes)!=FALSE;
    s.session_error=queried?0:GetLastError();
    if(queried&&memory&&bytes>=sizeof(WTSINFOEXW)){
        const auto& info=*reinterpret_cast<const WTSINFOEXW*>(memory);s.session_level=info.Level;
        if(info.Level==1){const auto& data=info.Data.WTSInfoExLevel1;
            // Never inspect or emit the username/domain fields of this buffer.
            s.session_id=data.SessionId;s.session_state=static_cast<int>(data.SessionState);
            s.session_flags=static_cast<DWORD>(data.SessionFlags);s.session_read=true;}
    }
    if(memory)WTSFreeMemory(memory);
    s.foreground=reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
    s.foreground_tid=GetWindowThreadProcessId(reinterpret_cast<HWND>(s.foreground),&s.foreground_pid);
    s.foreground_known=s.foreground!=0&&s.foreground_pid!=0&&s.foreground_tid!=0;
    SetLastError(0);
    const auto foreground_process=s.foreground_pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,s.foreground_pid):nullptr;
    s.foreground_integrity_error=foreground_process?0:(s.foreground_pid?GetLastError():ERROR_INVALID_PARAMETER);
    s.caller_integrity_known=integrity(GetCurrentProcess(),s.caller_integrity,s.caller_integrity_error);
    if(foreground_process)s.foreground_integrity_known=integrity(foreground_process,s.foreground_integrity,s.foreground_integrity_error);
    if(foreground_process)CloseHandle(foreground_process);
    s.finish=ticks();
    s.complete=s.input_read&&s.thread_read&&s.station_read&&s.session_read&&s.foreground_known&&s.hook_access&&s.caller_integrity_known&&s.foreground_integrity_known;
    s.valid=s.start>0&&s.finish>=s.start&&s.input_read&&s.thread_read&&s.station_read&&
        s.input_name==L"Default"&&s.thread_name==s.input_name&&s.station_name==L"WinSta0"&&
        s.session_read&&s.session_state==WTSActive&&s.session_flags==WTS_SESSIONSTATE_UNLOCK&&s.foreground!=0&&
        s.hook_access&&s.caller_integrity_known&&s.foreground_integrity_known&&s.caller_integrity>=s.foreground_integrity;
    return s;
}
std::string fields(const Context& s) {
    return "{\"start_qpc\":"+std::to_string(s.start)+",\"finish_qpc\":"+std::to_string(s.finish)+
        ",\"input_name\":"+(s.input_read?quote(s.input_name):"null")+",\"thread_name\":"+(s.thread_read?quote(s.thread_name):"null")+",\"station_name\":"+(s.station_read?quote(s.station_name):"null")+
        ",\"input_query_succeeded\":"+boolean(s.input_read)+",\"thread_query_succeeded\":"+boolean(s.thread_read)+",\"station_query_succeeded\":"+boolean(s.station_read)+
        ",\"input_error\":"+std::to_string(s.input_error)+",\"thread_error\":"+std::to_string(s.thread_error)+",\"station_error\":"+std::to_string(s.station_error)+
        ",\"session_query_succeeded\":"+boolean(s.session_read)+",\"session_error\":"+std::to_string(s.session_error)+",\"session_level\":"+(s.session_read?std::to_string(s.session_level):"null")+
        ",\"session_id\":"+(s.session_read?std::to_string(s.session_id):"null")+",\"session_state\":"+(s.session_read?std::to_string(s.session_state):"null")+",\"session_flags\":"+(s.session_read?std::to_string(s.session_flags):"null")+
        ",\"foreground_hwnd\":"+std::to_string(s.foreground)+",\"foreground_pid\":"+(s.foreground_known?std::to_string(s.foreground_pid):"null")+",\"foreground_tid\":"+(s.foreground_known?std::to_string(s.foreground_tid):"null")+",\"foreground_identity_query_succeeded\":"+boolean(s.foreground_known)+
        ",\"caller_integrity_known\":"+boolean(s.caller_integrity_known)+",\"caller_integrity_error\":"+std::to_string(s.caller_integrity_error)+",\"foreground_integrity_known\":"+boolean(s.foreground_integrity_known)+",\"foreground_integrity_error\":"+std::to_string(s.foreground_integrity_error)+
        ",\"caller_integrity\":"+(s.caller_integrity_known?std::to_string(s.caller_integrity):"null")+",\"foreground_integrity\":"+(s.foreground_integrity_known?std::to_string(s.foreground_integrity):"null")+
        ",\"desktop_hook_access_verified\":"+boolean(s.hook_access)+",\"context_complete\":"+boolean(s.complete)+",\"valid_context\":"+boolean(s.valid)+"}";
}
struct Key {const char* name;int code;std::int64_t start{},finish{};bool attempted{},down{};};
struct Foreground {
    std::int64_t start{},finish{};std::uintptr_t hwnd{};DWORD pid{},tid{},error{};
    bool attempted{},known{};
};
Foreground foreground_tuple(){
    Foreground f;f.start=ticks();f.hwnd=reinterpret_cast<std::uintptr_t>(GetForegroundWindow());
    if(f.hwnd){f.attempted=true;SetLastError(0);f.tid=GetWindowThreadProcessId(reinterpret_cast<HWND>(f.hwnd),&f.pid);f.error=f.tid?0:GetLastError();}
    f.known=f.hwnd!=0&&f.pid!=0&&f.tid!=0;f.finish=ticks();return f;
}
std::string foreground_fields(const Foreground& f){
    return "{\"start_qpc\":"+std::to_string(f.start)+",\"finish_qpc\":"+std::to_string(f.finish)+",\"hwnd\":"+std::to_string(f.hwnd)
        +",\"pid\":"+(f.known?std::to_string(f.pid):"null")+",\"tid\":"+(f.known?std::to_string(f.tid):"null")+",\"identity_query_attempted\":"+boolean(f.attempted)+",\"identity_query_succeeded\":"+boolean(f.known)+",\"error\":"+(f.attempted?std::to_string(f.error):"null")+"}";
}
struct ForegroundGui {
    Foreground before,after;GUITHREADINFO gui{sizeof(gui)};
    std::int64_t start{},finish{};DWORD error{};bool attempted{},succeeded{},stable{};
};
ForegroundGui foreground_gui(){
    ForegroundGui g;g.before=foreground_tuple();
    if(g.before.known){g.attempted=true;g.start=ticks();SetLastError(0);g.succeeded=GetGUIThreadInfo(g.before.tid,&g.gui)!=FALSE;g.error=g.succeeded?0:GetLastError();g.finish=ticks();}
    g.after=foreground_tuple();
    g.stable=g.before.known&&g.after.known&&g.before.hwnd==g.after.hwnd&&g.before.pid==g.after.pid&&g.before.tid==g.after.tid;
    return g;
}
std::string gui_fields(const ForegroundGui& g){
    return "{\"foreground_before\":"+foreground_fields(g.before)+",\"query_tid\":"+(g.before.known?std::to_string(g.before.tid):"null")+",\"query_attempted\":"+boolean(g.attempted)+",\"query_start_qpc\":"+(g.attempted?std::to_string(g.start):"null")+",\"query_finish_qpc\":"+(g.attempted?std::to_string(g.finish):"null")
        +",\"query_succeeded\":"+boolean(g.succeeded)+",\"error\":"+(g.attempted?std::to_string(g.error):"null")+",\"foreground_after\":"+foreground_fields(g.after)+",\"foreground_tuple_stable\":"+boolean(g.stable)
        +",\"capture_hwnd\":"+(g.succeeded?std::to_string(reinterpret_cast<std::uintptr_t>(g.gui.hwndCapture)):"null")+",\"menu_owner_hwnd\":"+(g.succeeded?std::to_string(reinterpret_cast<std::uintptr_t>(g.gui.hwndMenuOwner)):"null")+",\"move_size_hwnd\":"+(g.succeeded?std::to_string(reinterpret_cast<std::uintptr_t>(g.gui.hwndMoveSize)):"null")+",\"gui_flags\":"+(g.succeeded?std::to_string(g.gui.flags):"null")+"}";
}
const char* predicate(bool known,bool value){return known?(value?"PASS":"FAIL"):"UNKNOWN";}
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=4||std::wstring_view(argv[1])!=L"--check-input-state"||std::wstring_view(argv[2])!=L"--evidence-log")return 2;
    const auto file=CreateFileW(argv[3],GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 1;
    LARGE_INTEGER frequency{};const bool frequency_ok=QueryPerformanceFrequency(&frequency)&&frequency.QuadPart>0;
    const auto before=context();
    const bool buttons_swapped=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    std::array keys{Key{"left",VK_LBUTTON},Key{"right",VK_RBUTTON},Key{"middle",VK_MBUTTON},Key{"x1",VK_XBUTTON1},Key{"x2",VK_XBUTTON2},Key{"ctrl",VK_CONTROL},Key{"shift",VK_SHIFT},Key{"alt",VK_MENU},Key{"lwin",VK_LWIN},Key{"rwin",VK_RWIN},Key{"escape",VK_ESCAPE}};
    if(before.valid&&frequency_ok){for(auto& key:keys){key.start=ticks();key.attempted=true;key.down=(GetAsyncKeyState(key.code)&0x8000)!=0;key.finish=ticks();}}
    const auto gui=foreground_gui();
    const auto after=context();
    // Ordered observations, not an atomic snapshot. Any observed context
    // change invalidates the intervening key samples rather than implying UP.
    const bool gui_bound=gui.stable&&gui.before.hwnd==before.foreground&&gui.before.pid==before.foreground_pid&&gui.before.tid==before.foreground_tid&&gui.after.hwnd==after.foreground&&gui.after.pid==after.foreground_pid&&gui.after.tid==after.foreground_tid;
    const bool clock_ok=frequency_ok&&before.start>0&&before.finish>=before.start&&gui.before.start>=before.finish&&gui.before.finish>=gui.before.start&&gui.after.start>=gui.before.finish&&gui.after.finish>=gui.after.start&&after.start>=gui.after.finish&&after.finish>=after.start&&(!gui.attempted||(gui.start>=gui.before.finish&&gui.finish>=gui.start&&gui.after.start>=gui.finish));
    const bool stable=clock_ok&&before.valid&&after.valid&&before.input_name==after.input_name&&before.thread_name==after.thread_name&&before.station_name==after.station_name&&before.session_id==after.session_id&&
        before.foreground==after.foreground&&before.foreground_pid==after.foreground_pid&&before.foreground_tid==after.foreground_tid&&
        before.caller_integrity==after.caller_integrity&&before.foreground_integrity==after.foreground_integrity&&gui_bound;
    bool all_up=stable,keys_reliable=stable,any_down=false;std::string key_fields="{";std::int64_t key_clock=before.finish;
    for(std::size_t i=0;i<keys.size();++i){const auto& key=keys[i];const bool reliable=stable&&key.attempted&&key.start>0&&key.finish>=key.start;
        const bool ordered=reliable&&key.start>=key_clock&&key.finish<=gui.before.start;
        keys_reliable=keys_reliable&&ordered;all_up=all_up&&ordered&&!key.down;any_down=any_down||(ordered&&key.down);if(key.attempted)key_clock=key.finish;
        if(i)key_fields+=',';
        key_fields+='"'+std::string(key.name)+"\":{\"query_attempted\":"+boolean(key.attempted)+",\"start_qpc\":"+(key.attempted?std::to_string(key.start):"null")+",\"finish_qpc\":"+(key.attempted?std::to_string(key.finish):"null")+
            ",\"state\":\""+(ordered?(key.down?std::string("DOWN"):std::string("UP")):(key.attempted?std::string("UNKNOWN"):std::string("NOT_EVALUATED")))+"\",\"high_bit_down\":"+(ordered?boolean(key.down):"null")+"}";
    }
    key_fields+='}';
    const bool tuple_known=before.foreground_known&&after.foreground_known&&gui.before.known&&gui.after.known;
    const bool gui_reliable=gui.succeeded&&gui_bound;
    struct Predicate{const char* name;const char* value;};
    const std::array readiness{
        Predicate{"QPC_VALID",predicate(clock_ok,true)},Predicate{"BEFORE_CONTEXT_VALID",predicate(before.complete,before.valid)},Predicate{"AFTER_CONTEXT_VALID",predicate(after.complete,after.valid)},
        Predicate{"OBSERVATION_CONTEXT_STABLE",predicate(clock_ok&&before.complete&&after.complete&&tuple_known,stable)},Predicate{"BUTTONS_OBSERVED_UP",predicate(keys_reliable,all_up)},Predicate{"INPUT_MAPPING_SUPPORTED",predicate(true,!buttons_swapped)},
        Predicate{"FOREGROUND_GUI_QUERY",gui.attempted?predicate(true,gui.succeeded):"NOT_EVALUATED"},Predicate{"FOREGROUND_GUI_TUPLE_STABLE",predicate(tuple_known,gui_bound)},
        Predicate{"CAPTURE_CLEAR",predicate(gui_reliable,!gui.gui.hwndCapture)},Predicate{"MENU_CLEAR",predicate(gui_reliable,!gui.gui.hwndMenuOwner)},Predicate{"MOVE_SIZE_CLEAR",predicate(gui_reliable,!gui.gui.hwndMoveSize)},Predicate{"DISALLOWED_GUI_FLAGS_CLEAR",predicate(gui_reliable,!(gui.gui.flags&30))}
    };
    bool ready=true,known_failure=false;const char* first="NONE";std::string predicates="{",failed="[";bool failed_first=true;
    for(std::size_t i=0;i<readiness.size();++i){const auto& p=readiness[i];if(i)predicates+=',';predicates+='"'+std::string(p.name)+"\":\""+p.value+'"';
        if(std::string_view(p.value)!="PASS"){ready=false;if(std::string_view(first)=="NONE")first=p.name;}
        if(std::string_view(p.value)=="FAIL"){known_failure=true;if(!failed_first)failed+=',';failed+='"'+std::string(p.name)+'"';failed_first=false;}}
    predicates+='}';failed+=']';const char* result=ready?"READY":(known_failure?"BLOCKED":"UNKNOWN");
    const char* button_observation=keys_reliable?(any_down?"OBSERVED_DOWN":"OBSERVED_UP"):"UNKNOWN";
    const auto line="{\"schema\":\"r1c4b-readonly-input-environment/v2\",\"startup_contract\":\"foreground_gui_readiness_v1\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(GetCurrentThreadId())+
        ",\"qpc_frequency\":"+std::to_string(frequency.QuadPart)+",\"read_only\":true,\"atomic_snapshot\":false,\"before\":"+fields(before)+",\"keys\":"+key_fields+",\"foreground_gui\":"+gui_fields(gui)+",\"after\":"+fields(after)+
        ",\"context_reliable\":"+boolean(stable)+",\"mouse_buttons_swapped\":"+boolean(buttons_swapped)+",\"all_required_inputs_up\":"+boolean(all_up)+",\"buttons_observed_up\":"+boolean(all_up)+",\"buttons_observation\":\""+button_observation+"\",\"startup_ready\":"+boolean(ready)+",\"startup_readiness\":\""+result+"\",\"readiness_predicates\":"+predicates+",\"first_failed_predicate\":\""+first+"\",\"failed_predicates\":"+failed+",\"result\":\""+result+"\"}\n";
    DWORD written{};const bool logged=WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();CloseHandle(file);
    std::cout<<(logged?line:"input environment evidence write failed\n");
    return logged?(ready?0:2):1;
}
