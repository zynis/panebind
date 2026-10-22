#include "visual_recovery.h"
#include "guest_capture_scope.h"

#include <sddl.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <thread>
#ifdef PANEBIND_VISUAL_RECOVERY_MODEL_TEST
#include <iostream>
#endif

namespace panebind::platform::windows::product {
namespace {
namespace v=core::v1;
constexpr DWORD wire_magic=0x31565250,io_ms=750,launch_ms=2500,barrier_ms=1800,stop_io_ms=2750;
enum class Command:DWORD { Remember=1,Forget=2,Stop=3,Commit=4,CompletionBarrier=5 };
struct Wire {
    DWORD magic{wire_magic},size{sizeof(Wire)},command{};
    std::uint64_t serial{},key{},hwnd{},born{},completion_handle{};
    DWORD pid{},tid{};
    v::VisualOwnership ownership;
    std::array<wchar_t,1024> exe{};
    std::array<wchar_t,512> window_class{};
};
struct Reply {DWORD magic{wire_magic},status{};std::uint64_t serial{};DWORD records{};};
struct Handle {
    HANDLE value{};
    explicit Handle(HANDLE h=nullptr):value(h){}
    ~Handle(){close();}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    void close(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
};
struct Facts {std::vector<unsigned char> sid;std::wstring logon_sid,exe;DWORD session{},rid{};std::uint64_t born{};bool valid{};};
std::uint64_t ticks(FILETIME t){return(static_cast<std::uint64_t>(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
bool equal(std::wstring_view a,std::wstring_view b){return CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;}
Facts facts(HANDLE process){
    Facts f;HANDLE raw{};if(!OpenProcessToken(process,TOKEN_QUERY,&raw))return f;Handle token(raw);
    auto info=[&](TOKEN_INFORMATION_CLASS type){DWORD n{};GetTokenInformation(raw,type,nullptr,0,&n);
        std::vector<unsigned char> bytes;if(n&&n<=65536){bytes.resize(n);if(!GetTokenInformation(raw,type,bytes.data(),n,&n))bytes.clear();}return bytes;};
    auto user=info(TokenUser),label=info(TokenIntegrityLevel),groups=info(TokenGroups);
    if(user.empty()||label.empty()||groups.empty())return f;
    PSID sid=reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid;
    if(!IsValidSid(sid))return f;f.sid.resize(GetLengthSid(sid));if(!CopySid(static_cast<DWORD>(f.sid.size()),f.sid.data(),sid))return f;
    sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
    if(!IsValidSid(sid)||!*GetSidSubAuthorityCount(sid))return f;
    f.rid=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1);
    TOKEN_ELEVATION elevation{};DWORD ui{},app{},n{};
    if(!GetTokenInformation(raw,TokenElevation,&elevation,sizeof(elevation),&n)||
       !GetTokenInformation(raw,TokenSessionId,&f.session,sizeof(f.session),&n)||
       !GetTokenInformation(raw,TokenUIAccess,&ui,sizeof(ui),&n)||
       !GetTokenInformation(raw,TokenIsAppContainer,&app,sizeof(app),&n)||
       f.rid!=SECURITY_MANDATORY_MEDIUM_RID||elevation.TokenIsElevated||ui||app||!f.session)return f;
    const auto* list=reinterpret_cast<TOKEN_GROUPS*>(groups.data());
    for(DWORD i=0;i<list->GroupCount;++i)if((list->Groups[i].Attributes&SE_GROUP_LOGON_ID)==SE_GROUP_LOGON_ID){
        LPWSTR value{};if(ConvertSidToStringSidW(list->Groups[i].Sid,&value)){f.logon_sid=value;LocalFree(value);}break;}
    FILETIME created{},exited{},kernel{},cpu{};
    if(!GetProcessTimes(process,&created,&exited,&kernel,&cpu)||!(f.born=ticks(created)))return f;
    std::array<wchar_t,32768> image{};n=static_cast<DWORD>(image.size());
    if(!QueryFullProcessImageNameW(process,0,image.data(),&n)||!n)return f;f.exe.assign(image.data(),n);
    f.valid=!f.logon_sid.empty();return f;
}
bool peers(const Facts& a,const Facts& b){return a.valid&&b.valid&&a.session==b.session&&a.rid==b.rid&&
    EqualSid(const_cast<unsigned char*>(a.sid.data()),const_cast<unsigned char*>(b.sid.data()))&&equal(a.exe,b.exe)&&a.logon_sid==b.logon_sid;}
std::wstring pipe_name(DWORD pid,std::uint64_t born,std::wstring_view nonce){return L"\\\\.\\pipe\\PaneBindV1.VisualRecovery-"+std::to_wstring(pid)+L"-"+std::to_wstring(born)+L"-"+std::wstring(nonce);}
bool nonce_valid(std::wstring_view value){return value.size()==32&&std::all_of(value.begin(),value.end(),[](wchar_t c){return(c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f');});}
std::optional<std::uint64_t> unsigned_number(std::wstring_view value){std::uint64_t n{};if(value.empty()||value.size()>20)return {};
    for(auto c:value){if(c<L'0'||c>L'9'||n>(std::numeric_limits<std::uint64_t>::max()-static_cast<unsigned>(c-L'0'))/10)return {};n=n*10+static_cast<unsigned>(c-L'0');}return n;}
void pump(){MSG message{};for(unsigned n=0;n<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++n){if(message.message==WM_QUIT)continue;TranslateMessage(&message);DispatchMessageW(&message);}}
bool same_kernel_object(HANDLE a,HANDLE b){
    using Compare=BOOL(WINAPI*)(HANDLE,HANDLE);
    const auto module=GetModuleHandleW(L"kernelbase.dll");
    const auto compare=module?reinterpret_cast<Compare>(GetProcAddress(module,"CompareObjectHandles")):nullptr;
    return compare&&compare(a,b)!=FALSE;
}
struct CompletionBarrier {
    Handle event;
    // Ownership transfers on a valid received command, even if its ACK is
    // lost. All rejection paths close the incoming duplicate exactly once.
    bool adopt(HANDLE incoming){
        if(!incoming||incoming==INVALID_HANDLE_VALUE)return false;
        if(incoming==event.value)return true;
        Handle owned(incoming);DWORD flags{};
        if(!GetHandleInformation(incoming,&flags))return false;
        if(event.value){
            if(same_kernel_object(event.value,incoming))return true;
            if(WaitForSingleObject(event.value,0)!=WAIT_OBJECT_0)return false;
        }
        const auto state=WaitForSingleObject(incoming,0);
        if(state!=WAIT_OBJECT_0&&state!=WAIT_TIMEOUT)return false;
        event.close();event.value=owned.value;owned.value=nullptr;return true;
    }
    bool ready(DWORD timeout){
        if(!event.value)return false;const auto start=GetTickCount64();
        for(;;){
            const auto elapsed=GetTickCount64()-start;const DWORD remaining=elapsed>=timeout?0:timeout-static_cast<DWORD>(elapsed);
            const auto result=MsgWaitForMultipleObjectsEx(1,&event.value,remaining,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            if(result==WAIT_OBJECT_0)return true;
            if(result==WAIT_OBJECT_0+1){pump();if(GetTickCount64()-start<timeout)continue;
                return WaitForSingleObject(event.value,0)==WAIT_OBJECT_0;}
            return false;
        }
    }
};
struct Io {
    OVERLAPPED ov{};std::array<unsigned char,sizeof(Wire)> bytes{};
    Io(){ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);}
    ~Io(){if(ov.hEvent)CloseHandle(ov.hEvent);}
};
enum class IoResult { Complete,PeerExit,Failed };
// Each pending buffer/OVERLAPPED is owned until cancellation completion. A
// kernel cancellation that cannot be confirmed retains this bounded storage
// for process lifetime rather than returning stack memory to a live request.
void cancel(HANDLE pipe,std::unique_ptr<Io>& op){
    if(!op)return;CancelIoEx(pipe,&op->ov);DWORD n{};
    if(WaitForSingleObject(op->ov.hEvent,200)!=WAIT_OBJECT_0||
       (!GetOverlappedResult(pipe,&op->ov,&n,FALSE)&&GetLastError()==ERROR_IO_INCOMPLETE))op.release();
}
IoResult transfer(HANDLE pipe,HANDLE peer,bool write,void* data,DWORD length,DWORD timeout,bool messages,bool peer_first=false){
    if(length>sizeof(Wire))return IoResult::Failed;
    auto op=std::make_unique<Io>();if(!op->ov.hEvent)return IoResult::Failed;
    if(write)std::memcpy(op->bytes.data(),data,length);DWORD bytes{};
    const BOOL done=write?WriteFile(pipe,op->bytes.data(),length,&bytes,&op->ov):ReadFile(pipe,op->bytes.data(),length,&bytes,&op->ov);
    if(!done&&GetLastError()!=ERROR_IO_PENDING)return IoResult::Failed;
    const auto start=GetTickCount64();
    if(!done)for(;;){
        if(peer_first&&WaitForSingleObject(peer,0)==WAIT_OBJECT_0){cancel(pipe,op);return IoResult::PeerExit;}
        const auto elapsed=GetTickCount64()-start;
        if(timeout!=INFINITE&&elapsed>=timeout){cancel(pipe,op);return IoResult::Failed;}
        const DWORD left=timeout==INFINITE?INFINITE:timeout-static_cast<DWORD>(elapsed);
        const std::array<HANDLE,2> handles{op->ov.hEvent,peer};
        const DWORD result=messages?MsgWaitForMultipleObjectsEx(2,handles.data(),left,QS_ALLINPUT,MWMO_INPUTAVAILABLE):
            WaitForMultipleObjects(2,handles.data(),FALSE,left);
        if(result==WAIT_OBJECT_0){if(!GetOverlappedResult(pipe,&op->ov,&bytes,FALSE))return IoResult::Failed;break;}
        if(result==WAIT_OBJECT_0+1){cancel(pipe,op);return IoResult::PeerExit;}
        if(messages&&result==WAIT_OBJECT_0+2){pump();continue;}
        cancel(pipe,op);return IoResult::Failed;
    }
    if(peer_first&&WaitForSingleObject(peer,0)==WAIT_OBJECT_0)return IoResult::PeerExit;
    if(bytes!=length)return IoResult::Failed;
    if(!write)std::memcpy(data,op->bytes.data(),length);return IoResult::Complete;
}
std::optional<v::VisualState> visual(HWND hwnd){SetLastError(0);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    if(!ex&&GetLastError())return {};v::VisualState value;value.topmost=(ex&WS_EX_TOPMOST)!=0;value.layered=(ex&WS_EX_LAYERED)!=0;
    if(value.layered){COLORREF color{};BYTE alpha{};DWORD flags{};value.attributes_known=GetLayeredWindowAttributes(hwnd,&color,&alpha,&flags)!=FALSE;
        if(value.attributes_known){value.color=color;value.alpha=alpha;value.flags=flags;}}return value;}
bool identity(const Wire& wire,const Snapshot& source){return reinterpret_cast<std::uintptr_t>(source.hwnd)==wire.hwnd&&source.pid==wire.pid&&
    source.tid==wire.tid&&source.born==wire.born&&equal(source.exe,wire.exe.data())&&source.window_class==wire.window_class.data();}
struct Record {Snapshot identity;v::VisualOwnership ownership;std::optional<v::VisualOwnership> previous;bool pending{};};
bool ready_to_restore(const std::map<std::uint64_t,Record>& records,CompletionBarrier& barrier,DWORD timeout=barrier_ms){
    if(barrier.event.value)return barrier.ready(timeout);
    // A pre-native/ACK-lost crash without a recorded event is NOT evidence that
    // a prepared transition completed. Fail closed, keeping the journal until
    // this failed helper exits; never clear it or claim cleanup PASS.
    return std::none_of(records.begin(),records.end(),[](const auto& item){return item.second.pending;});
}
const v::VisualOwnership* alpha_owner(const Record& record,const v::VisualState& current){
    if(v::owns_alpha(record.ownership,current))return &record.ownership;
    if(record.pending&&record.previous&&v::owns_alpha(*record.previous,current))return &*record.previous;
    return nullptr;
}
const v::VisualOwnership* top_owner(const Record& record,const v::VisualState& current){
    if(v::owns_top(record.ownership,current))return &record.ownership;
    if(record.pending&&record.previous&&v::owns_top(*record.previous,current))return &*record.previous;
    return nullptr;
}
bool alpha_plan_known(const v::VisualOwnership& ownership){
    return !ownership.changed_alpha||
        ((!ownership.original.layered||ownership.original.attributes_known)&&
         (!ownership.applied.layered||ownership.applied.attributes_known));
}
bool commit_matches(const Record& record,const v::VisualOwnership& completed,const v::VisualState& current){
    const auto alpha=alpha_owner(record,current);
    if(completed.changed_alpha){
        if(!alpha||!v::same_alpha(completed.applied,current)||!v::same_alpha(completed.original,alpha->original))return false;
    }else if(alpha&&!v::same_alpha(current,alpha->original))return false;
    const auto top=top_owner(record,current);
    if(completed.changed_top){
        if(!top||completed.applied.topmost!=current.topmost||completed.original.topmost!=top->original.topmost)return false;
    }else if(top&&current.topmost!=top->original.topmost)return false;
    return true;
}
bool unknown_owned_alpha(const Record& record,const v::VisualState& current){
    return current.layered&&!current.attributes_known&&
        (record.ownership.changed_alpha||(record.pending&&record.previous&&record.previous->changed_alpha));
}
bool forget_allowed(const Record& record,const v::VisualState& current){
    return !unknown_owned_alpha(record,current)&&!alpha_owner(record,current)&&!top_owner(record,current);
}
class Watchdog {
    Handle stop_{CreateEventW(nullptr,TRUE,FALSE,nullptr)},timer_{CreateWaitableTimerW(nullptr,TRUE,nullptr)};std::thread thread_;
public:
    Watchdog(){if(stop_.value&&timer_.value)thread_=std::thread([this]{const std::array<HANDLE,2> wait{stop_.value,timer_.value};
        if(WaitForMultipleObjects(2,wait.data(),FALSE,INFINITE)==WAIT_OBJECT_0+1)TerminateProcess(GetCurrentProcess(),79);});}
    ~Watchdog(){if(thread_.joinable()){SetEvent(stop_.value);thread_.join();}}
    bool valid()const{return thread_.joinable();}
    bool arm(){LARGE_INTEGER due{};due.QuadPart=-30000000;return SetWaitableTimer(timer_.value,&due,0,nullptr,nullptr,FALSE)!=FALSE;}
    void idle(){CancelWaitableTimer(timer_.value);}
};
bool restore(Catalog& catalog,Record& record){
    const auto settings=v::default_settings();
    const auto initial=catalog.get(record.identity.hwnd,settings,nullptr,true,false);
    if(!initial)return !IsWindow(record.identity.hwnd);
    if(!Catalog::same(record.identity,*initial))return true; // old native lifetime ended; never touch its replacement
    auto valid=[&]{const auto now=catalog.get(record.identity.hwnd,settings,nullptr,true,false);return now&&Catalog::same(record.identity,*now);};
    if(!valid())return false;auto current=visual(record.identity.hwnd);if(!current)return false;
    // Unknown alpha cannot be restored or awarded success. It must not prevent
    // an independently exact-owned topmost property from being cleaned up.
    const bool exact=!unknown_owned_alpha(record,*current);
    if(const auto owned=alpha_owner(record,*current)){
        const auto original=owned->original;
        if(original.layered&&!original.attributes_known)return false;
        if(!valid())return false;
        if(original.layered){
            if(!SetLayeredWindowAttributes(record.identity.hwnd,original.color,original.alpha,original.flags))return false;
        }else{SetLastError(0);const auto ex=GetWindowLongPtrW(record.identity.hwnd,GWL_EXSTYLE);if(!ex&&GetLastError())return false;
            if(!valid())return false;SetLastError(0);const auto previous=SetWindowLongPtrW(record.identity.hwnd,GWL_EXSTYLE,ex&~WS_EX_LAYERED);
            if(!previous&&GetLastError())return false;}
        current=visual(record.identity.hwnd);if(!current||!valid()||!v::same_alpha(*current,original))return false;
    }
    current=visual(record.identity.hwnd);if(!current)return false;
    if(const auto owned=top_owner(record,*current)){
        const bool top=owned->original.topmost;
        if(!valid()||!SetWindowPos(record.identity.hwnd,top?HWND_TOPMOST:HWND_NOTOPMOST,
            0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))return false;
        current=visual(record.identity.hwnd);if(!current||!valid()||current->topmost!=top)return false;
    }return exact;
}
bool remember(Catalog& catalog,std::map<std::uint64_t,Record>& records,const Wire& wire){
    if(!wire.key||!wire.hwnd||!wire.born||!wire.pid||!wire.tid||!v::visual_changes(wire.ownership))return false;
    if(wire.exe.back()!=L'\0'||wire.window_class.back()!=L'\0'||wire.exe.front()==L'\0'||wire.window_class.front()==L'\0')return false;
    if(!alpha_plan_known(wire.ownership))return false;
    const auto fresh=catalog.get(reinterpret_cast<HWND>(static_cast<std::uintptr_t>(wire.hwnd)),v::default_settings(),nullptr,true,false);
    if(!fresh||!identity(wire,*fresh))return false;
    const auto previous=records.find(wire.key);
    if(previous!=records.end()&&!Catalog::same(previous->second.identity,*fresh))return false;
    if(previous!=records.end()&&previous->second.pending)return false;
    if(previous==records.end()&&records.size()>=64)return false;
    const auto current=visual(fresh->hwnd);if(!current)return false;
    if(wire.ownership.changed_alpha&&!v::same_alpha(*current,wire.ownership.original)&&
       (previous==records.end()||!v::owns_alpha(previous->second.ownership,*current)))return false;
    if(wire.ownership.changed_top&&current->topmost!=wire.ownership.original.topmost&&
       (previous==records.end()||!v::owns_top(previous->second.ownership,*current)))return false;
    const auto stable=previous==records.end()?std::optional<v::VisualOwnership>{}:previous->second.ownership;
    records[wire.key]={*fresh,wire.ownership,stable,true};return true;
}
bool commit(Catalog& catalog,std::map<std::uint64_t,Record>& records,const Wire& wire){
    const auto entry=records.find(wire.key);if(entry==records.end()||!entry->second.pending)return false;
    auto& record=entry->second;
    const auto fresh=catalog.get(record.identity.hwnd,v::default_settings(),nullptr,true,false);
    if(!fresh||!Catalog::same(record.identity,*fresh)||!identity(wire,*fresh))return false;
    const auto current=visual(fresh->hwnd);if(!current)return false;
    if(!commit_matches(record,wire.ownership,*current))return false;
    if(!v::visual_changes(wire.ownership)){records.erase(entry);return true;}
    record.ownership=wire.ownership;record.previous.reset();record.pending=false;return true;
}
bool forget(Catalog& catalog,std::map<std::uint64_t,Record>& records,std::uint64_t key){
    const auto entry=records.find(key);if(entry==records.end())return true;
    const auto fresh=catalog.get(entry->second.identity.hwnd,v::default_settings(),nullptr,true,false);
    if(!fresh){if(!IsWindow(entry->second.identity.hwnd)){records.erase(entry);return true;}return false;}
    if(!Catalog::same(entry->second.identity,*fresh)){records.erase(entry);return true;}
    const auto current=visual(fresh->hwnd);if(!current||!forget_allowed(entry->second,*current))return false;
    records.erase(entry);return true;
}
int run_helper(DWORD parent_pid,std::uint64_t parent_born,std::wstring_view nonce,std::string_view source_sha){
    Watchdog watchdog;if(!watchdog.valid()||!watchdog.arm())return 78;
    const Facts own=facts(GetCurrentProcess());Handle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,parent_pid));
    const Facts source=parent.value?facts(parent.value):Facts{};
    if(!peers(own,source)||source.born!=parent_born||WaitForSingleObject(parent.value,0)!=WAIT_TIMEOUT)return 78;
    const auto name=pipe_name(parent_pid,parent_born,nonce);
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|FILE_WRITE_DATA|FILE_WRITE_ATTRIBUTES|SYNCHRONIZE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
    if(pipe.value==INVALID_HANDLE_VALUE)return 78;ULONG server{};
    if(!GetNamedPipeServerProcessId(pipe.value,&server)||server!=parent_pid)return 78;
    DWORD mode=PIPE_READMODE_MESSAGE;if(!SetNamedPipeHandleState(pipe.value,&mode,nullptr,nullptr))return 78;
    const HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(apartment))return 78;
    struct Com {~Com(){CoUninitialize();}} com;
    if(!AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))return 78;
    // The same executable independently revalidates its exact manifest/bytes
    // and actual desktop. The parent cannot send an arbitrary allowed name.
    const auto route=guest::detect_resource_route(source_sha);if(!route.enabled())return 78;
    Catalog catalog(route.desktop_context);std::map<std::uint64_t,Record> records;CompletionBarrier barrier;Reply hello;hello.status=1;
    if(transfer(pipe.value,parent.value,true,&hello,sizeof(hello),1000,true,true)!=IoResult::Complete)return 78;
    bool requested_stop=false;std::uint64_t previous_serial{};
    for(;;){Wire wire;watchdog.idle();const auto read=transfer(pipe.value,parent.value,false,&wire,sizeof(wire),INFINITE,true,true);if(!watchdog.arm())return 79;
        if(read!=IoResult::Complete)break;
        if(wire.magic!=wire_magic||wire.size!=sizeof(wire)||!wire.serial||wire.serial<=previous_serial)break;
        previous_serial=wire.serial;bool accepted{};
        const bool settled=!barrier.event.value||barrier.ready(0);
        if(wire.command==static_cast<DWORD>(Command::Remember))accepted=settled&&remember(catalog,records,wire);
        else if(wire.command==static_cast<DWORD>(Command::Forget))accepted=settled&&forget(catalog,records,wire.key);
        else if(wire.command==static_cast<DWORD>(Command::Commit))accepted=settled&&commit(catalog,records,wire);
        else if(wire.command==static_cast<DWORD>(Command::CompletionBarrier)){
            if(!records.empty())accepted=barrier.adopt(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(wire.completion_handle)));
            else{Handle unused(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(wire.completion_handle)));}
        }
        else if(wire.command==static_cast<DWORD>(Command::Stop)){requested_stop=true;accepted=true;}
        else break;
        if(requested_stop){bool exact=ready_to_restore(records,barrier);if(exact){pump();for(auto& item:records)exact=restore(catalog,item.second)&&exact;}
            if(exact)records.clear();accepted=exact;}
        Reply reply;reply.serial=wire.serial;reply.status=accepted?1u:0u;reply.records=static_cast<DWORD>(records.size());
        if(transfer(pipe.value,parent.value,true,&reply,sizeof(reply),1000,true,true)!=IoResult::Complete)break;
        if(requested_stop)return accepted?0:4;
        if(accepted&&(wire.command==static_cast<DWORD>(Command::Forget)||wire.command==static_cast<DWORD>(Command::Commit))&&records.empty())return 0;
    }
    // A broken channel withdraws visual permission just as parent exit does.
    // Restore only exact owned tuples; never reconstruct a missing authority.
    bool exact=ready_to_restore(records,barrier);if(exact){pump();for(auto& item:records)exact=restore(catalog,item.second)&&exact;}
    return exact?0:4;
}
} // namespace

struct VisualRecovery::Impl {
    operations::InteractiveDesktopContextRef desktop_context;
    explicit Impl(operations::InteractiveDesktopContextRef context):desktop_context(std::move(context)){}
    DWORD owner{GetCurrentThreadId()},pid{};Handle pipe,process;std::uint64_t serial{},helper_born{};std::set<std::uint64_t> keys;std::string failure;
    bool start(){
        if(desktop_context&&!desktop_context->active()){failure="recovery_exact_Run_not_active";return false;}
        if(!process.value&&!keys.empty()){failure="visual_recovery_cleanup_unconfirmed";return false;}
        if(process.value){if(WaitForSingleObject(process.value,0)==WAIT_TIMEOUT&&pipe.value)return true;
            if(!keys.empty()){failure="visual_recovery_helper_exited_with_owned_records";return false;}
            if(!finish_process())return false;serial=0;}
        const auto own=facts(GetCurrentProcess());if(!own.valid){failure="visual_recovery_requires_medium";return false;}
        GUID id{};std::array<wchar_t,64> guid{};if(FAILED(CoCreateGuid(&id))||!StringFromGUID2(id,guid.data(),static_cast<int>(guid.size()))){failure="recovery_nonce_failed";return false;}
        std::wstring nonce;for(auto c:guid){if(c>=L'A'&&c<=L'F')c=static_cast<wchar_t>(c-L'A'+L'a');if((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'f'))nonce+=c;}
        if(!nonce_valid(nonce))return false;
        PSECURITY_DESCRIPTOR descriptor{};const auto sddl=L"D:P(A;;GA;;;"+own.logon_sid+L")";
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr)){failure="recovery_pipe_acl_failed";return false;}
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};const auto name=pipe_name(GetCurrentProcessId(),own.born,nonce);
        pipe.value=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,sizeof(Wire),sizeof(Wire),1000,&attributes);LocalFree(descriptor);
        if(pipe.value==INVALID_HANDLE_VALUE){pipe.value=nullptr;failure="recovery_pipe_create_failed";return false;}
        auto connect=std::make_unique<Io>();if(!connect->ov.hEvent){pipe.close();return false;}
        const bool connected=ConnectNamedPipe(pipe.value,&connect->ov)!=FALSE;const DWORD connection_error=connected?0:GetLastError();
        if(!connected&&connection_error!=ERROR_IO_PENDING&&connection_error!=ERROR_PIPE_CONNECTED){pipe.close();failure="recovery_connect_failed";return false;}
        std::wstring command=L"\""+own.exe+L"\" --visual-recovery-helper "+std::to_wstring(GetCurrentProcessId())+L" "+std::to_wstring(own.born)+L" "+nonce;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
        std::wstring exact_desktop;
        if(desktop_context){exact_desktop=L"WinSta0\\"+desktop_context->name();startup.lpDesktop=exact_desktop.data();}
        if(!CreateProcessW(own.exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)){
            cancel(pipe.value,connect);pipe.close();failure="recovery_helper_create_failed";return false;}
        process.value=child.hProcess;pid=child.dwProcessId;CloseHandle(child.hThread);
        const auto child_facts=facts(process.value);
        if(!peers(own,child_facts)||child_facts.born<own.born){cancel(pipe.value,connect);pipe.close();failure="recovery_helper_identity_rejected";return false;}
        helper_born=child_facts.born;
        if(!connected&&connection_error!=ERROR_PIPE_CONNECTED){const std::array<HANDLE,2> waits{connect->ov.hEvent,process.value};
            if(WaitForMultipleObjects(2,waits.data(),FALSE,launch_ms)!=WAIT_OBJECT_0){cancel(pipe.value,connect);pipe.close();failure="recovery_helper_connect_timeout";return false;}
            DWORD bytes{};if(!GetOverlappedResult(pipe.value,&connect->ov,&bytes,FALSE)){pipe.close();failure="recovery_connect_result_failed";return false;}}
        ULONG observed{};if(!GetNamedPipeClientProcessId(pipe.value,&observed)||observed!=pid){pipe.close();failure="recovery_exact_client_rejected";return false;}
        Reply hello;if(transfer(pipe.value,process.value,false,&hello,sizeof(hello),io_ms,false)!=IoResult::Complete||hello.magic!=wire_magic||hello.status!=1||hello.serial){
            pipe.close();failure="recovery_handshake_failed";return false;}return true;
    }
    bool request(Wire& wire,Reply& reply,DWORD timeout=io_ms){
        if(owner!=GetCurrentThreadId()||!pipe.value||!process.value||serial==std::numeric_limits<std::uint64_t>::max()){failure="recovery_client_unavailable";return false;}
        wire.serial=++serial;
        if(transfer(pipe.value,process.value,true,&wire,sizeof(wire),io_ms,false)!=IoResult::Complete||
           transfer(pipe.value,process.value,false,&reply,sizeof(reply),timeout,false)!=IoResult::Complete||reply.magic!=wire_magic||reply.serial!=wire.serial||reply.status!=1){
            pipe.close();failure="recovery_record_not_acknowledged";return false;}return true;
    }
    bool finish_process(){pipe.close();if(!process.value)return true;
        const bool ended=WaitForSingleObject(process.value,1200)==WAIT_OBJECT_0;DWORD result{};
        const bool exact=ended&&GetExitCodeProcess(process.value,&result)&&result==0;
        process.close();pid=0;helper_born=0;if(!exact)failure="recovery_helper_cleanup_unconfirmed";return exact;}
};

VisualRecovery::VisualRecovery(operations::InteractiveDesktopContextRef context):impl_(std::make_unique<Impl>(std::move(context))){}
VisualRecovery::~VisualRecovery(){stop();}
bool VisualRecovery::remember(const Snapshot& identity,const v::VisualOwnership& ownership){
    if(!identity.hwnd||!identity.key||identity.exe.empty()||identity.exe.size()>=1024||identity.window_class.empty()||identity.window_class.size()>=512||!v::visual_changes(ownership))return false;
    if(impl_->owner!=GetCurrentThreadId()||!impl_->start())return false;
    Wire wire;wire.command=static_cast<DWORD>(Command::Remember);wire.key=identity.key;wire.hwnd=reinterpret_cast<std::uintptr_t>(identity.hwnd);
    wire.pid=identity.pid;wire.tid=identity.tid;wire.born=identity.born;wire.ownership=ownership;
    std::copy(identity.exe.begin(),identity.exe.end(),wire.exe.begin());std::copy(identity.window_class.begin(),identity.window_class.end(),wire.window_class.begin());
    Reply reply;if(!impl_->request(wire,reply))return false;impl_->keys.insert(identity.key);return true;
}
bool VisualRecovery::commit(const Snapshot& identity,const v::VisualOwnership& ownership){
    if(!identity.hwnd||!identity.key||identity.exe.empty()||identity.exe.size()>=1024||identity.window_class.empty()||identity.window_class.size()>=512||!impl_->keys.contains(identity.key))return false;
    Wire wire;wire.command=static_cast<DWORD>(Command::Commit);wire.key=identity.key;wire.hwnd=reinterpret_cast<std::uintptr_t>(identity.hwnd);
    wire.pid=identity.pid;wire.tid=identity.tid;wire.born=identity.born;wire.ownership=ownership;
    std::copy(identity.exe.begin(),identity.exe.end(),wire.exe.begin());std::copy(identity.window_class.begin(),identity.window_class.end(),wire.window_class.begin());
    Reply reply;if(!impl_->request(wire,reply))return false;
    if(!v::visual_changes(ownership)){impl_->keys.erase(identity.key);if(impl_->keys.empty())return impl_->finish_process();}
    return true;
}
bool VisualRecovery::defer_until_native_completion(HANDLE borrowed_event){
    if(impl_->owner!=GetCurrentThreadId()||!borrowed_event||borrowed_event==INVALID_HANDLE_VALUE||impl_->keys.empty()||!impl_->pipe.value||!impl_->process.value){
        impl_->failure="recovery_completion_barrier_client_unavailable";return false;}
    ULONG client{};const auto own=facts(GetCurrentProcess()),helper=facts(impl_->process.value);
    if(WaitForSingleObject(impl_->process.value,0)!=WAIT_TIMEOUT||GetProcessId(impl_->process.value)!=impl_->pid||
        !GetNamedPipeClientProcessId(impl_->pipe.value,&client)||client!=impl_->pid||!peers(own,helper)||helper.born!=impl_->helper_born){
        impl_->failure="recovery_completion_barrier_exact_helper_rejected";return false;}
    HANDLE remote{};
    if(!DuplicateHandle(GetCurrentProcess(),borrowed_event,impl_->process.value,&remote,SYNCHRONIZE,FALSE,0)){
        impl_->failure="recovery_completion_barrier_duplicate_failed";return false;}
    Wire wire;wire.command=static_cast<DWORD>(Command::CompletionBarrier);wire.completion_handle=reinterpret_cast<std::uintptr_t>(remote);Reply reply;
    // The numeric remote handle is NEVER closed in this process. Once the
    // command can have reached the peer, reclaiming it by remote numeric value
    // would race handle reuse/double-close. The verified helper adopts/closes
    // it; on an uncertain transfer, its bounded process exit closes any unread
    // duplicate. The caller must mark the unissued Work done even on ACK loss.
    if(!impl_->request(wire,reply)){impl_->failure="recovery_completion_barrier_not_acknowledged";return false;}
    return true;
}
bool VisualRecovery::forget(std::uint64_t key){
    if(!impl_->keys.contains(key))return true;Wire wire;wire.command=static_cast<DWORD>(Command::Forget);wire.key=key;Reply reply;
    if(!impl_->request(wire,reply))return false;impl_->keys.erase(key);
    if(impl_->keys.empty())return impl_->finish_process();return true;
}
bool VisualRecovery::stop(){
    if(!impl_->process.value)return impl_->keys.empty();bool exact=true;
    if(impl_->pipe.value){Wire wire;wire.command=static_cast<DWORD>(Command::Stop);Reply reply;exact=impl_->request(wire,reply,stop_io_ms);if(exact)impl_->keys.clear();}
    else exact=impl_->keys.empty();const bool ended=impl_->finish_process();return ended&&exact;
}
std::string VisualRecovery::error()const{return impl_->failure;}
DWORD VisualRecovery::helper_pid()const{return impl_->pid;}
std::optional<int> VisualRecovery::helper_entry(const std::vector<std::wstring>& argv,std::string_view source_sha){
    if(argv.empty()||argv[0]!=L"--visual-recovery-helper")return {};
    if(argv.size()!=4||!nonce_valid(argv[3]))return 78;
    const auto pid=unsigned_number(argv[1]),birth=unsigned_number(argv[2]);
    if(!pid||!*pid||*pid>MAXDWORD||!birth||!*birth)return 78;
    try{return run_helper(static_cast<DWORD>(*pid),*birth,argv[3],source_sha);}catch(...){return 78;}
}

#ifdef PANEBIND_VISUAL_RECOVERY_MODEL_TEST
// Same predicates called by production restore/commit; no helper construction,
// process launch, native window query, token lookup or host input in this test.
int recovery_model_tests(){
    int checks{},failures{};auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}};
    v::VisualOwnership previous;previous.original={false,false,true,255,0,0};
    previous.applied={false,true,true,190,0,LWA_ALPHA};previous.changed_alpha=true;
    auto planned=previous;planned.applied.alpha=158;
    Record record;record.ownership=planned;record.previous=previous;record.pending=true;
    const auto* old=alpha_owner(record,previous.applied);
    check(old&&v::same_alpha(old->original,previous.original),"pre-native crash after second prepare still owns old applied tuple");
    const auto* next=alpha_owner(record,planned.applied);
    check(next&&v::same_alpha(next->original,previous.original),"post-native pre-commit crash owns planned tuple");
    record.pending=false;check(!alpha_owner(record,previous.applied),"commit retires previous tuple rather than allowing history indefinitely");
    check(alpha_owner(record,planned.applied)!=nullptr,"committed tuple remains exact recovery authority");
    auto foreign=planned.applied;foreign.alpha=166;
    check(!alpha_owner(record,foreign),"external alpha replacement is not cleanup authority");
    foreign=planned.applied;foreign.color=RGB(11,12,13);foreign.flags=LWA_ALPHA|LWA_COLORKEY;
    check(!alpha_owner(record,foreign),"different colorkey and flags cannot be overwritten");
    foreign=planned.applied;foreign.attributes_known=false;
    check(!alpha_owner(record,foreign),"unreadable layered tuple is not invented ownership");
    record.pending=true;record.ownership.changed_top=false;record.previous->changed_top=true;
    record.previous->original.topmost=false;record.previous->applied.topmost=true;
    auto top=previous.applied;top.topmost=true;
    check(top_owner(record,top)==&*record.previous,"pending restore intent retains old topmost ownership");
    record.pending=false;check(!top_owner(record,top),"old topmost tuple not accepted after commit");
    record.ownership.changed_top=true;record.ownership.applied.topmost=true;
    check(top_owner(record,top)==&record.ownership,"new topmost expected tuple independently owned");
    Record restoring;restoring.previous=previous;restoring.ownership=previous;
    restoring.ownership.applied=previous.original;restoring.pending=true;
    auto cleared=restoring.ownership;cleared.changed_alpha=false;
    check(commit_matches(restoring,cleared,previous.original),"pending exact alpha restore may clear ownership after native verification");
    check(!commit_matches(restoring,cleared,previous.applied),"clear alpha ownership rejected while previous owned tuple is still applied");
    restoring.ownership.changed_top=true;restoring.ownership.original.topmost=false;
    restoring.ownership.applied.topmost=false;restoring.previous->changed_top=true;
    restoring.previous->original.topmost=false;restoring.previous->applied.topmost=true;
    cleared=restoring.ownership;cleared.changed_top=false;cleared.changed_alpha=false;
    auto restored=previous.original;restored.topmost=false;
    check(commit_matches(restoring,cleared,restored),"pending exact topmost restore may clear ownership");
    restored.topmost=true;check(!commit_matches(restoring,cleared,restored),"clear top ownership rejected while old owned topmost remains");
    auto top_only=restoring.ownership;top_only.changed_alpha=false;
    top_only.original.layered=true;top_only.original.attributes_known=false;
    top_only.applied.layered=true;top_only.applied.attributes_known=false;
    check(alpha_plan_known(top_only),"top-only plan does not invent or require unknown alpha tuple");
    top_only.changed_alpha=true;check(!alpha_plan_known(top_only),"alpha mutation still requires readable layered tuples");
    Record unknown;unknown.ownership=previous;unknown.ownership.changed_top=true;
    unknown.ownership.original.topmost=false;unknown.ownership.applied.topmost=true;
    auto unreadable=previous.applied;unreadable.attributes_known=false;unreadable.topmost=true;
    check(unknown_owned_alpha(unknown,unreadable)&&!alpha_owner(unknown,unreadable),"unknown owned alpha remains failed and cannot be restored speculatively");
    check(top_owner(unknown,unreadable)==&unknown.ownership,"unknown alpha does not suppress independent exact top cleanup");
    auto alpha_only=unknown;alpha_only.ownership.changed_top=false;
    check(!forget_allowed(alpha_only,unreadable),"unknown alpha must not erase recovery journal merely because alpha_owner is null");
    auto replaced=previous.applied;replaced.alpha=166;
    check(forget_allowed(alpha_only,replaced),"known external alpha replacement may retire owned journal without rewriting it");
    CompletionBarrier barrier;std::map<std::uint64_t,Record> pending{{1,record}};pending.begin()->second.pending=true;
    check(!ready_to_restore(pending,barrier,0),"pending visual transition without acknowledged barrier remains UNKNOWN");
    Handle first(CreateEventW(nullptr,TRUE,FALSE,nullptr));HANDLE first_copy{};
    const bool duplicated=first.value&&DuplicateHandle(GetCurrentProcess(),first.value,GetCurrentProcess(),&first_copy,SYNCHRONIZE,FALSE,0);
    check(duplicated&&barrier.adopt(first_copy),"actual SYNCHRONIZE duplicate adopted before native mutation");
    check(!ready_to_restore(pending,barrier,0),"unsignaled completion prevents pre-native original tuple from being treated as cleanup success");
    if(first.value)SetEvent(first.value);
    check(ready_to_restore(pending,barrier,0),"actual matching native completion permits independent owned restore");
    HANDLE same_copy{};const bool same_duplicate=first.value&&DuplicateHandle(GetCurrentProcess(),first.value,GetCurrentProcess(),&same_copy,SYNCHRONIZE,FALSE,0);
    check(same_duplicate&&barrier.adopt(same_copy),"duplicate same-call registration is idempotent without double close");
    Handle next_event(CreateEventW(nullptr,TRUE,FALSE,nullptr));HANDLE next_copy{};
    const bool next_duplicate=next_event.value&&DuplicateHandle(GetCurrentProcess(),next_event.value,GetCurrentProcess(),&next_copy,SYNCHRONIZE,FALSE,0);
    check(next_duplicate&&barrier.adopt(next_copy),"next exact call replaces only a completed barrier");
    check(!ready_to_restore(pending,barrier,0),"previous signaled event never completes a different native call");
    Handle third(CreateEventW(nullptr,TRUE,TRUE,nullptr));HANDLE third_copy{};
    const bool third_duplicate=third.value&&DuplicateHandle(GetCurrentProcess(),third.value,GetCurrentProcess(),&third_copy,SYNCHRONIZE,FALSE,0);
    check(third_duplicate&&!barrier.adopt(third_copy)&&!ready_to_restore(pending,barrier,0),"a different call cannot replace the unfinished barrier");
    check(!barrier.adopt(nullptr),"invalid completion handle fails closed");
    if(next_event.value)SetEvent(next_event.value);next_event.close();
    check(ready_to_restore(pending,barrier,0),"borrowed source close does not invalidate the helper-owned event duplicate");
    pending.begin()->second.pending=false;CompletionBarrier absent;
    check(ready_to_restore(pending,absent,0),"already committed journal without a native pending call retains ordinary recovery path");
    Wire wire;wire.hwnd=1;wire.key=900;wire.pid=10;wire.tid=20;wire.born=30;
    const std::wstring exe=L"C:\\Fixture\\app.exe",cls=L"Fixture.Window";
    std::copy(exe.begin(),exe.end(),wire.exe.begin());std::copy(cls.begin(),cls.end(),wire.window_class.begin());
    Snapshot target;target.hwnd=reinterpret_cast<HWND>(1);target.key=42;target.pid=10;target.tid=20;target.born=30;target.exe=exe;target.window_class=cls;
    check(identity(wire,target),"parent opaque key is not confused with helper Catalog key");
    auto other=target;other.born=31;check(!identity(wire,other),"process reuse birth differs");
    other=target;other.tid=21;check(!identity(wire,other),"source thread differs");
    other=target;other.window_class=L"Another";check(!identity(wire,other),"reused source class differs");
    other=target;other.exe=L"C:\\Other\\app.exe";check(!identity(wire,other),"same basename different executable rejected");
    check(nonce_valid(L"0123456789abcdef0123456789abcdef")&&!nonce_valid(L"0123456789abcdef0123456789abcdeF"),"exact lowercase nonce contract");
    check(unsigned_number(L"18446744073709551615")==std::numeric_limits<std::uint64_t>::max()&&!unsigned_number(L"18446744073709551616"),"bounded parent birth parse handles overflow");
    check(!unsigned_number(L"+1")&&!unsigned_number(L" 1")&&!unsigned_number(L""),"internal PID parsing rejects ambiguous values");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
#endif

} // namespace panebind::platform::windows::product

#ifdef PANEBIND_VISUAL_RECOVERY_MODEL_TEST
int main(){return panebind::platform::windows::product::recovery_model_tests();}
#endif
