#include "platform/windows/product/window_writer.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/operations/window_rect_adjustment.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <dwmapi.h>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace panebind::platform::windows::product {
namespace {

using core::v1::Limits;
using core::v1::Rect;
namespace v=core::v1;

std::optional<v::VisualState> read_visual(HWND hwnd){
    SetLastError(ERROR_SUCCESS);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    if(!ex&&GetLastError())return {};
    v::VisualState result;result.topmost=(ex&WS_EX_TOPMOST)!=0;result.layered=(ex&WS_EX_LAYERED)!=0;
    if(result.layered){COLORREF color{};BYTE alpha{};DWORD flags{};
        result.attributes_known=GetLayeredWindowAttributes(hwnd,&color,&alpha,&flags)!=FALSE;
        if(result.attributes_known){result.color=color;result.alpha=alpha;result.flags=flags;}}
    return result;
}
bool same_changed_visual(const v::VisualState& a,const v::VisualState& b,bool alpha,bool top){
    return(!alpha||v::same_alpha(a,b))&&(!top||a.topmost==b.topmost);
}
bool native_visual_identity(const Snapshot& expected,GuiContextMode* observed_mode=nullptr,
    const GestureCaptureLease& capture={},GeometryContextPhase phase=GeometryContextPhase::BeforeNativeCall,std::string* why=nullptr){
    DWORD pid{};if(!expected.hwnd||!expected.born||GetWindowThreadProcessId(expected.hwnd,&pid)!=expected.tid||pid!=expected.pid||pid==GetCurrentProcessId())return false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);if(!process)return false;
    FILETIME birth{},exited{},kernel{},user{};HANDLE token{},own_token{};
    bool valid=WaitForSingleObject(process,0)==WAIT_TIMEOUT&&GetProcessTimes(process,&birth,&exited,&kernel,&user)&&
        ((static_cast<std::uint64_t>(birth.dwHighDateTime)<<32)|birth.dwLowDateTime)==expected.born;
    std::array<wchar_t,32768> path{};DWORD length=static_cast<DWORD>(path.size());std::array<wchar_t,512> cls{};
    valid=valid&&QueryFullProcessImageNameW(process,0,path.data(),&length)&&
        CompareStringOrdinal(path.data(),static_cast<int>(length),expected.exe.data(),static_cast<int>(expected.exe.size()),TRUE)==CSTR_EQUAL&&
        GetClassNameW(expected.hwnd,cls.data(),static_cast<int>(cls.size()))>0&&expected.window_class==cls.data();
    valid=valid&&OpenProcessToken(process,TOKEN_QUERY,&token)&&OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&own_token);
    if(valid){
        alignas(TOKEN_MANDATORY_LABEL) std::array<BYTE,512> label{},target_sid{},own_sid{};
        DWORD size{},session{},own_session{},ui{},app{};TOKEN_ELEVATION elevation{};
        valid=GetTokenInformation(token,TokenIntegrityLevel,label.data(),static_cast<DWORD>(label.size()),&size)&&
            GetTokenInformation(token,TokenUser,target_sid.data(),static_cast<DWORD>(target_sid.size()),&size)&&
            GetTokenInformation(own_token,TokenUser,own_sid.data(),static_cast<DWORD>(own_sid.size()),&size)&&
            GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)&&!elevation.TokenIsElevated&&
            GetTokenInformation(token,TokenSessionId,&session,sizeof(session),&size)&&session&&
            GetTokenInformation(own_token,TokenSessionId,&own_session,sizeof(own_session),&size)&&session==own_session&&
            GetTokenInformation(token,TokenUIAccess,&ui,sizeof(ui),&size)&&!ui&&
            GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&size)&&!app;
        if(valid){const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
            const auto a=reinterpret_cast<TOKEN_USER*>(target_sid.data())->User.Sid,b=reinterpret_cast<TOKEN_USER*>(own_sid.data())->User.Sid;
            valid=IsValidSid(sid)&&*GetSidSubAuthorityCount(sid)&&
                *GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1)==SECURITY_MANDATORY_MEDIUM_RID&&IsValidSid(a)&&IsValidSid(b)&&EqualSid(a,b);}
    }
    if(token)CloseHandle(token);if(own_token)CloseHandle(own_token);CloseHandle(process);
    if(!valid)return false;const auto gui=observe_gui_context(expected);if(observed_mode)*observed_mode=gui.mode;
    if(!geometry_gui_context_allowed(expected,gui,capture,phase,why))return false;
    const auto desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!desktop)return false;
    BOOL receiving{};DWORD bytes{};valid=GetUserObjectInformationW(desktop,UOI_IO,&receiving,sizeof(receiving),&bytes)&&receiving;
    CloseDesktop(desktop);return valid;
}

std::uint64_t now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart > 0 ?
        static_cast<std::uint64_t>(value.QuadPart) : 0;
}

HWND read_z_neighbor(HWND window,UINT relation,bool& known)noexcept{
    SetLastError(ERROR_SUCCESS);const HWND value=GetWindow(window,relation);
    if(!value&&GetLastError()!=ERROR_SUCCESS)known=false;
    return value;
}

WindowWriter::LowerZOrderSample observe_lower_z_order(const Snapshot& expected)noexcept{
    WindowWriter::LowerZOrderSample sample;sample.source=expected.hwnd;sample.shell=GetShellWindow();
    sample.source_tid=GetWindowThreadProcessId(sample.source,&sample.source_pid);
    sample.known=sample.source&&IsWindow(sample.source)&&GetAncestor(sample.source,GA_ROOT)==sample.source&&
        sample.source_tid==expected.tid&&sample.source_pid==expected.pid&&expected.tid&&expected.pid;
    if(!sample.known)return sample;
    sample.previous=read_z_neighbor(sample.source,GW_HWNDPREV,sample.known);
    sample.next=read_z_neighbor(sample.source,GW_HWNDNEXT,sample.known);
    if(sample.shell&&sample.shell!=sample.source&&sample.next==sample.shell){
        sample.shell_tid=GetWindowThreadProcessId(sample.shell,&sample.shell_pid);
        sample.shell_known=sample.shell_pid&&sample.shell_tid&&IsWindow(sample.shell)&&
            GetAncestor(sample.shell,GA_ROOT)==sample.shell;
        sample.shell_previous=read_z_neighbor(sample.shell,GW_HWNDPREV,sample.shell_known);
        sample.shell_next=read_z_neighbor(sample.shell,GW_HWNDNEXT,sample.shell_known);
        DWORD pid{};const DWORD tid=GetWindowThreadProcessId(sample.shell,&pid);
        const HWND previous=read_z_neighbor(sample.shell,GW_HWNDPREV,sample.shell_known);
        const HWND next=read_z_neighbor(sample.shell,GW_HWNDNEXT,sample.shell_known);
        sample.shell_known=sample.shell_known&&IsWindow(sample.shell)&&GetAncestor(sample.shell,GA_ROOT)==sample.shell&&
            pid==sample.shell_pid&&tid==sample.shell_tid&&
            previous==sample.shell_previous&&next==sample.shell_next;
    }
    DWORD pid{};const DWORD tid=GetWindowThreadProcessId(sample.source,&pid);
    const HWND previous=read_z_neighbor(sample.source,GW_HWNDPREV,sample.known);
    const HWND next=read_z_neighbor(sample.source,GW_HWNDNEXT,sample.known);
    sample.known=sample.known&&IsWindow(sample.source)&&GetAncestor(sample.source,GA_ROOT)==sample.source&&
        pid==sample.source_pid&&tid==sample.source_tid&&
        previous==sample.previous&&next==sample.next&&GetShellWindow()==sample.shell;
    return sample;
}

bool native_rect(const Rect& value) noexcept {
    using core::geometry::checked_difference;
    const auto width = checked_difference(value.right(), value.left());
    const auto height = checked_difference(value.bottom(), value.top());
    if (!width || !height || *width <= 0 || *height <= 0) return false;
    constexpr auto lo = std::numeric_limits<std::int32_t>::min();
    constexpr auto hi = std::numeric_limits<std::int32_t>::max();
    for (const auto coordinate : std::array{value.left(), value.top(), value.right(),
        value.bottom(), *width, *height}) {
        if (coordinate < lo || coordinate > hi) return false;
    }
    return true;
}

bool legal_size(const Rect& value, const Limits& limits) noexcept {
    if (limits.min_width <= 0 || limits.min_height <= 0 ||
        limits.max_width < limits.min_width || limits.max_height < limits.min_height ||
        limits.grid_width <= 0 || limits.grid_height <= 0) return false;
    return value.width() >= limits.min_width && value.width() <= limits.max_width &&
        value.height() >= limits.min_height && value.height() <= limits.max_height &&
        value.width() % limits.grid_width == 0 && value.height() % limits.grid_height == 0;
}

bool consistent(const Snapshot& expected, const Snapshot& current, std::string& reason) {
    if (!Catalog::same(expected, current)) { reason = "window_identity_or_generation_changed"; return false; }
    if (expected.visible != current.visible || expected.positioning != current.positioning) {
        reason = "unattributed_geometry_change"; return false;
    }
    if (expected.dpi != current.dpi || expected.monitor != current.monitor ||
        expected.work != current.work || expected.maximized != current.maximized ||
        expected.can_resize != current.can_resize || expected.minimized != current.minimized) {
        reason = "geometry_context_changed"; return false;
    }
    return true;
}

bool target_allowed(const Snapshot& expected, const Snapshot& current,
    const Rect& desired, std::string& reason) {
    if (!native_rect(desired)) { reason = "invalid_or_out_of_native_range_target"; return false; }
    const bool resizing = desired.size() != current.visible.size();
    if(current.minimized){reason="restore_minimized_window_before_geometry_command";return false;}
    if (resizing && (!expected.can_resize || !current.can_resize)) {
        reason = "window_does_not_support_resize"; return false;
    }
    if (!legal_size(desired, expected.limits) || !legal_size(desired, current.limits)) {
        reason = "target_violates_size_constraints"; return false;
    }
    if (current.maximized && desired != current.visible) {
        reason = "restore_maximized_window_before_geometry_command"; return false;
    }
    return true;
}

bool allowed_by_guard(const WindowWriter::Guard& guard, std::string& reason) {
    if (!guard) { reason = "missing_operation_guard"; return false; }
    try {
        if (guard()) return true;
        reason = "operation_revoked_before_native_attempt";
    } catch (...) { reason = "operation_guard_failed"; }
    return false;
}

bool same_display(const Display& a,const Display& b,bool dpi=true){
    return a.monitor==b.monitor&&a.bounds==b.bounds&&a.work==b.work&&(!dpi||a.dpi==b.dpi);
}
bool same_topology(std::span<const Display> expected,std::span<const Display> actual,bool dpi=true){
    if(expected.empty()||expected.size()>32||expected.size()!=actual.size())return false;
    for(const auto& display:expected){
        if(!display.monitor||std::count_if(expected.begin(),expected.end(),[&](const auto& d){return d.monitor==display.monitor;})!=1)return false;
        const auto found=std::find_if(actual.begin(),actual.end(),[&](const auto& d){return d.monitor==display.monitor;});
        if(found==actual.end()||!same_display(display,*found,dpi))return false;
    }return true;
}
HMONITOR planned_monitor(const Rect& native){
    if(!native_rect(native))return nullptr;
    const RECT rect{static_cast<LONG>(native.left()),static_cast<LONG>(native.top()),static_cast<LONG>(native.right()),static_cast<LONG>(native.bottom())};
    return MonitorFromRect(&rect,MONITOR_DEFAULTTONULL);
}
// No foreign messages, Catalog/COM, DPI-probe creation or application lock on
// the native worker. GUI pre/post checks additionally verify actual DPI.
bool native_destination_unchanged(const WindowWriter::DestinationDisplay& proof){
    if(planned_monitor(proof.positioning)!=proof.target.monitor)return false;
    struct Facts{std::vector<Display> displays;bool complete{true};} facts;facts.displays.reserve(32);
    const auto enumerate=[](HMONITOR monitor,HDC,LPRECT,LPARAM context)->BOOL{
        auto& value=*reinterpret_cast<Facts*>(context);MONITORINFO info{sizeof(info)};
        if(value.displays.size()>=32||!GetMonitorInfoW(monitor,&info)){value.complete=false;return FALSE;}
        value.displays.push_back({monitor,{info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom},
            {info.rcMonitor.left,info.rcMonitor.top,info.rcMonitor.right,info.rcMonitor.bottom},0});return TRUE;};
    return EnumDisplayMonitors(nullptr,nullptr,enumerate,reinterpret_cast<LPARAM>(&facts))&&facts.complete&&
        same_topology(proof.topology,facts.displays,false);
}
WindowWriter::DestinationPreparation observe_destination(Catalog& catalog,const Snapshot& before,const Rect& native){
    const auto monitor=planned_monitor(native);
    if(monitor==before.monitor)return WindowWriter::prepare_destination(before,native,monitor,DPI_AWARENESS_INVALID,{});
    const auto awareness=GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(before.hwnd));
    return WindowWriter::prepare_destination(before,native,monitor,awareness,catalog.displays());
}

bool observe_capture_completion(WindowWriter::Receipt& receipt,const GestureCaptureLease& capture){
    if(!capture)return true;
    receipt.post_context_checked=true;
    if(!receipt.after){receipt.post_context_reason="post_native_identity_unavailable";return false;}
    if(receipt.before.dpi!=receipt.after->dpi||receipt.before.monitor!=receipt.after->monitor||
       receipt.before.work!=receipt.after->work||receipt.before.maximized!=receipt.after->maximized||
       receipt.before.minimized!=receipt.after->minimized){receipt.post_context_reason="post_native_geometry_context_changed";return false;}
    receipt.post_context_valid=geometry_gui_context_allowed(*receipt.after,observe_gui_context(*receipt.after),capture,
        receipt.native_attempted?GeometryContextPhase::IssuedCompletion:GeometryContextPhase::BeforeNativeCall,&receipt.post_context_reason);
    return receipt.post_context_valid;
}

bool observe_placement_completion(WindowWriter::Receipt& receipt,Catalog& catalog,const GestureCaptureLease& capture){
    // Keep the pre-existing no-lease same-display completion contract. A new
    // GUI interaction after an already exact discrete call is not evidence
    // that call failed; this patch only adds explicit display-transition proof.
    if(!capture&&!receipt.destination)return true;
    receipt.post_context_checked=true;
    if(!receipt.after){receipt.post_context_reason="post_native_identity_unavailable";return false;}
    const auto displays=receipt.destination?catalog.displays():std::vector<Display>{};
    const auto awareness=receipt.destination?GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(receipt.after->hwnd)):DPI_AWARENESS_INVALID;
    if(!WindowWriter::placement_context_matches(receipt.before,*receipt.after,receipt.destination,displays,awareness,receipt.post_context_reason))return false;
    receipt.post_context_valid=geometry_gui_context_allowed(*receipt.after,observe_gui_context(*receipt.after),capture,
        receipt.native_attempted?GeometryContextPhase::IssuedCompletion:GeometryContextPhase::BeforeNativeCall,&receipt.post_context_reason);
    return receipt.post_context_valid;
}

WindowWriter::Receipt rejected(const Snapshot& expected, const Rect& desired, std::string reason) {
    WindowWriter::Receipt result;
    result.before = expected;
    result.requested = desired;
    result.reason = std::move(reason);
    return result;
}

} // namespace

// Exactly one native worker. It owns no hook, input queue, capture, Catalog or
// application object. A hung foreign WindowProc cannot trap the GUI/input
// resource owner. After a timeout no subsequent operation overtakes the issued
// call; its eventual result remains distinct from cancellation of NEW calls.
struct WindowWriter::NativeExecutor {
    struct Work {
        Snapshot expected;Rect native,requested;Guard guard;std::uint64_t id{};
        GestureCaptureLease capture;
        std::optional<DestinationDisplay> destination;
        Operation operation{Operation::Placement};v::VisualState visual_before,visual_target;
        bool alpha_change{},top_change{};std::optional<v::VisualOwnership> cleanup_owned;VisualNativeFacts visual_native;
        HANDLE done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::mutex mutex;bool cancelled{},attempted{},succeeded{},known{},timed_out{};
        DWORD error{};UINT position_flags{};std::uint64_t before{},after{},preflight_started{};std::string reason;
        ~Work(){if(done)CloseHandle(done);}
    };
    struct State {
        operations::InteractiveDesktopContextRef desktop_context;
        HANDLE wake{CreateEventW(nullptr,FALSE,FALSE,nullptr)},exited{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::atomic<bool> stopping{};std::atomic<HWND> notify{};
        std::mutex mutex;std::shared_ptr<Work> pending;std::vector<std::shared_ptr<Work>> late;
        std::uint64_t next_id{};
        ~State(){if(wake)CloseHandle(wake);if(exited)CloseHandle(exited);}
    };
    std::shared_ptr<State> state{std::make_shared<State>()};std::thread thread;
    static bool first_attempt(const std::shared_ptr<State>& shared,const std::shared_ptr<Work>& work){
        // No hook/UP lock is held across these bounded read-only queries.
        // Identity and GUI/capture checks below remain independently required.
        if(shared->desktop_context&&!operations::run_desktop_source_allowed(shared->desktop_context->observe(
            work->expected.hwnd,work->expected.pid,work->expected.tid,work->expected.born))){
            std::lock_guard lock(work->mutex);work->reason="native_worker_exact_Run_context_changed";return false;
        }
        std::lock_guard lock(work->mutex);
        if(work->cancelled||shared->stopping){work->reason="native_worker_admission_cancelled";return false;}
        if(!work->cleanup_owned&&!allowed_by_guard(work->guard,work->reason))return false;
        if(work->operation==Operation::Placement)
            work->position_flags=WindowWriter::placement_flags(work->expected,work->native);
        work->before=now();if(!work->before){work->reason="native_attempt_clock_unavailable";return false;}
        work->attempted=true;return true;
    }
    static bool visual_sequence(const std::shared_ptr<State>& shared,const std::shared_ptr<Work>& work){
        auto current=read_visual(work->expected.hwnd);if(!current)return false;
        const auto prepared=WindowWriter::prepare_visual(work->visual_before,*current,work->visual_target,work->alpha_change,work->top_change,work->cleanup_owned);
        if(!prepared.accepted){std::lock_guard lock(work->mutex);work->reason=prepared.reason;return false;}
        if(prepared.noop){std::lock_guard lock(work->mutex);
            if(work->cancelled||shared->stopping){work->reason="native_worker_admission_cancelled";return false;}
            if(!work->cleanup_owned&&!allowed_by_guard(work->guard,work->reason))return false;
            work->reason="visual_already_exact_worker_no_native_call";return true;}
        bool started=false;auto claim=[&]{if(started)return true;started=first_attempt(shared,work);return started;};
        const auto hwnd=work->expected.hwnd;const auto& target=work->visual_target;
        if(work->alpha_change&&!v::same_alpha(*current,target)){
            SetLastError(ERROR_SUCCESS);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);if(!ex&&GetLastError())return false;
            auto alpha_baseline=current;bool introduced_layered=false;
            if(current->layered!=target.layered){
                const auto latest=read_visual(hwnd);
                if(!latest||!v::same_alpha(*latest,*current))return false;
                if(!claim())return false;
                {std::lock_guard lock(work->mutex);work->visual_native.style_attempted=true;}
                SetLastError(ERROR_SUCCESS);const auto previous=SetWindowLongPtrW(hwnd,GWL_EXSTYLE,target.layered?(ex|WS_EX_LAYERED):(ex&~WS_EX_LAYERED));
                const DWORD error=!previous?GetLastError():ERROR_SUCCESS;
                {std::lock_guard lock(work->mutex);work->visual_native.style_previous=previous;work->visual_native.style_error=error;work->error=error;}
                if(error)return false;
                introduced_layered=target.layered;alpha_baseline=read_visual(hwnd);if(!alpha_baseline)return false;
            }
            if(target.layered){
                // A multi-call style/alpha sequence is already issued. Do not
                // leave an introduced layered style half-configured on UP;
                // observe completion without restoring NEW gesture authority.
                if(!native_visual_identity(work->expected,nullptr,work->capture,
                    started?GeometryContextPhase::IssuedCompletion:GeometryContextPhase::BeforeNativeCall))return false;
                const auto latest=read_visual(hwnd);
                const bool same_unknown_introduced=latest&&introduced_layered&&latest->layered&&alpha_baseline->layered&&
                    !latest->attributes_known&&!alpha_baseline->attributes_known;
                if(!latest||(!same_unknown_introduced&&!v::same_alpha(*latest,*alpha_baseline)))return false;
                if(!claim())return false;
                {std::lock_guard lock(work->mutex);work->visual_native.alpha_attempted=true;}
                const BOOL success=SetLayeredWindowAttributes(hwnd,target.color,target.alpha,target.flags);const DWORD error=success?ERROR_SUCCESS:GetLastError();
                {std::lock_guard lock(work->mutex);work->visual_native.alpha_succeeded=success!=FALSE;work->visual_native.alpha_error=error;work->error=error;}
                if(!success)return false;
            }
        }
        if(work->top_change&&current->topmost!=target.topmost){
            if(!native_visual_identity(work->expected,nullptr,work->capture,
                started?GeometryContextPhase::IssuedCompletion:GeometryContextPhase::BeforeNativeCall))return false;
            const auto latest=read_visual(hwnd);if(!latest||latest->topmost!=current->topmost)return false;
            if(!claim())return false;
            {std::lock_guard lock(work->mutex);work->visual_native.zorder_attempted=true;}
            const BOOL success=SetWindowPos(hwnd,target.topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            const DWORD error=success?ERROR_SUCCESS:GetLastError();
            {std::lock_guard lock(work->mutex);work->visual_native.zorder_succeeded=success!=FALSE;work->visual_native.zorder_error=error;work->error=error;}
            if(!success)return false;
        }
        return true;
    }
    static bool exact_context(const Snapshot& e,GuiContextMode* observed_mode=nullptr,
        const GestureCaptureLease& capture={},std::string* why=nullptr){
        DWORD pid{};if(GetWindowThreadProcessId(e.hwnd,&pid)!=e.tid||pid!=e.pid)return false;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if(!process)return false;FILETIME created{},exit{},kernel{},user{};
        const bool identity=WaitForSingleObject(process,0)==WAIT_TIMEOUT&&GetProcessTimes(process,&created,&exit,&kernel,&user)&&
            ((static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime)==e.born;
        CloseHandle(process);if(!identity)return false;
        RECT positioning{},visible{};
        const auto equal=[](const RECT& a,const Rect& b){return a.left==b.left()&&a.top==b.top()&&a.right==b.right()&&a.bottom==b.bottom();};
        if(!GetWindowRect(e.hwnd,&positioning)||FAILED(DwmGetWindowAttribute(e.hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&visible,sizeof(visible)))||
            !equal(positioning,e.positioning)||!equal(visible,e.visible)||GetDpiForWindow(e.hwnd)!=e.dpi||
            MonitorFromWindow(e.hwnd,MONITOR_DEFAULTTONULL)!=e.monitor||(IsZoomed(e.hwnd)!=FALSE)!=e.maximized||(IsIconic(e.hwnd)!=FALSE)!=e.minimized)return false;
        const auto gui=observe_gui_context(e);if(observed_mode)*observed_mode=gui.mode;
        if(!geometry_gui_context_allowed(e,gui,capture,GeometryContextPhase::BeforeNativeCall,why))return false;
        const auto desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!desktop)return false;
        BOOL receiving{};DWORD size{};
        const bool active=GetUserObjectInformationW(desktop,UOI_IO,&receiving,sizeof(receiving),&size)&&receiving;
        CloseDesktop(desktop);return active;
    }
    explicit NativeExecutor(operations::InteractiveDesktopContextRef context){
        state->desktop_context=std::move(context);
        if(!state->wake||!state->exited)throw std::runtime_error("native_worker_events_failed");
        state->late.reserve(8);
        thread=std::thread([shared=state]{
            while(WaitForSingleObject(shared->wake,INFINITE)==WAIT_OBJECT_0){
                if(shared->stopping)break;std::shared_ptr<Work> work;
                {std::lock_guard lock(shared->mutex);work=shared->pending;}
                if(!work)continue;
                GuiContextMode gui_mode=GuiContextMode::Unknown;
                std::string context_reason;
                bool context=work->operation==Operation::Visual?native_visual_identity(work->expected,&gui_mode,work->capture,
                    GeometryContextPhase::BeforeNativeCall,&context_reason):exact_context(work->expected,&gui_mode,work->capture,&context_reason);
                if(context&&shared->desktop_context&&!shared->desktop_context->active()){
                    context=false;context_reason="native_worker_exact_Run_not_active";
                }
                if(context&&work->destination&&!native_destination_unchanged(*work->destination)){
                    context=false;context_reason="destination_display_topology_changed_before_native";
                }
                bool issue=false;
                {
                    std::lock_guard lock(work->mutex);
                    if(work->cancelled||shared->stopping)work->reason="native_worker_admission_cancelled";
                    else if(!context)work->reason=context_reason==capture_matching_up_retired_reason?context_reason:
                        std::string("native_worker_context_changed:GUI_context=")+gui_context_mode_name(gui_mode)+":"+context_reason;
                    else if(!work->cleanup_owned&&!allowed_by_guard(work->guard,work->reason)){}
                    else issue=true;
                }
                if(issue){
                    // An UP after this claim retires later requests; it cannot
                    // undo a SetWindowPos already issued to the OS.
                    if(work->operation==Operation::Visual){
                        const bool success=visual_sequence(shared,work);const auto completed=now();
                        std::lock_guard lock(work->mutex);work->succeeded=work->attempted&&success;work->after=completed;
                    }else if(first_attempt(shared,work)){
                        const auto& r=work->native;
                        if(work->operation==Operation::Lower){std::lock_guard lock(work->mutex);work->visual_native.zorder_attempted=true;}
                        const BOOL success=work->operation==Operation::Lower?
                            SetWindowPos(work->expected.hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE):
                            SetWindowPos(work->expected.hwnd,nullptr,static_cast<int>(r.left()),static_cast<int>(r.top()),
                                static_cast<int>(r.width()),static_cast<int>(r.height()),work->position_flags);
                        const DWORD error=success?ERROR_SUCCESS:GetLastError();const auto completed=now();
                        std::lock_guard lock(work->mutex);work->succeeded=success!=FALSE;work->error=error;work->after=completed;
                        if(work->operation==Operation::Lower){work->visual_native.zorder_succeeded=success!=FALSE;work->visual_native.zorder_error=error;}
                    }
                }
                bool delayed{};
                {std::lock_guard lock(work->mutex);work->known=true;delayed=work->timed_out;}
                SetEvent(work->done);
                if(delayed){
                    {std::lock_guard lock(shared->mutex);shared->late.push_back(work);}
                    if(const auto owner=shared->notify.load();owner&&!shared->stopping)PostMessageW(owner,native_completion_message,0,0);
                }
            }
            SetEvent(shared->exited);
        });
    }
    std::shared_ptr<Work> execute(const Snapshot& expected,const Rect& native,const Rect& requested,Guard guard,std::uint64_t preflight_started,
        Operation operation=Operation::Placement,const v::VisualState& before={},const v::VisualState& target={},
        bool alpha=false,bool top=false,std::optional<v::VisualOwnership> cleanup_owned=std::nullopt,BeforeIssue before_issue={},GestureCaptureLease capture={},
        std::optional<DestinationDisplay> destination={}){
        auto work=std::make_shared<Work>();if(!work->done)return {};
        work->expected=expected;work->native=native;work->requested=requested;work->guard=std::move(guard);work->reason.reserve(128);
        work->preflight_started=preflight_started;
        work->capture=std::move(capture);
        work->destination=std::move(destination);
        work->operation=operation;work->visual_before=before;work->visual_target=target;
        work->alpha_change=alpha;work->top_change=top;work->cleanup_owned=std::move(cleanup_owned);
        {
            std::lock_guard lock(state->mutex);
            if(state->stopping||state->late.size()>=8||state->next_id==std::numeric_limits<std::uint64_t>::max()||
                (state->pending&&WaitForSingleObject(state->pending->done,0)!=WAIT_OBJECT_0))return {};
            work->id=++state->next_id;
        }
        if(operation==Operation::Visual){
            // No worker can observe this job before the recovery helper owns
            // a duplicate of its EXACT completion event. The callback is only
            // invoked here, synchronously; no Application reference escapes.
            bool acknowledged=false;try{acknowledged=before_issue&&before_issue(work->done);}catch(...){acknowledged=false;}
            if(!acknowledged){
                {std::lock_guard lock(work->mutex);work->cancelled=true;work->known=true;work->reason="visual_completion_barrier_not_acknowledged_no_native_call";}
                // Even an ACK-lost helper can now observe this real no-call
                // completion rather than waiting forever or guessing success.
                SetEvent(work->done);return work;
            }
        }
        {
            std::lock_guard lock(state->mutex);
            if(state->stopping||(state->pending&&WaitForSingleObject(state->pending->done,0)!=WAIT_OBJECT_0)){
                {std::lock_guard job_lock(work->mutex);work->cancelled=true;work->known=true;work->reason="native_worker_admission_cancelled_before_queue";}
                SetEvent(work->done);return work;
            }
            state->pending=work;
        }
        SetEvent(state->wake);
        // One bounded operation wait, not a cursor/event polling loop. This
        // does not hold any lock used by UP or emergency-stop processing.
        if(WaitForSingleObject(work->done,250)!=WAIT_OBJECT_0){
            std::lock_guard lock(work->mutex);work->cancelled=true;work->timed_out=!work->known;
            if(!work->known)work->reason=work->attempted?"native_call_completion_timeout_in_flight":"native_worker_admission_timeout";
        }
        return work;
    }
    bool stop(){
        if(!thread.joinable())return WaitForSingleObject(state->exited,0)==WAIT_OBJECT_0;
        state->notify=nullptr;state->stopping=true;
        {std::lock_guard lock(state->mutex);if(state->pending){
            const auto work=state->pending;std::lock_guard job_lock(work->mutex);work->cancelled=true;
            // first_attempt takes this SAME mutex and checks cancelled. Thus a
            // never-issued job retired here cannot later claim a native call.
            // An already claimed call is not cancelled or declared completed.
            if(WindowWriter::complete_unissued_on_stop(work->attempted,work->known)){
                work->known=true;work->reason="native_worker_unissued_admission_retired_on_stop";SetEvent(work->done);
            }
        }}
        SetEvent(state->wake);const bool exited=WaitForSingleObject(state->exited,100)==WAIT_OBJECT_0;
        if(exited)thread.join();else thread.detach();
        // A detached in-flight worker retains only shared State/Work and native
        // values, never a destroyed Application/Catalog/Guard stack reference.
        return exited;
    }
    ~NativeExecutor(){stop();}
};

WindowWriter::WindowWriter(Catalog& catalog)
    :catalog_(catalog),executor_(std::make_unique<NativeExecutor>(catalog.desktop_context())) {}
WindowWriter::~WindowWriter()=default;
bool WindowWriter::complete_unissued_on_stop(bool attempted,bool known)noexcept{return !attempted&&!known;}

WindowWriter::LowerZOrderKind WindowWriter::lower_z_order_completion(const Snapshot& expected,
    const LowerZOrderSample& first,const LowerZOrderSample& second)noexcept{
    if(!first.known||!second.known||first!=second||!expected.hwnd||!expected.pid||!expected.tid||
       first.source!=expected.hwnd||first.source_pid!=expected.pid||first.source_tid!=expected.tid||
       first.previous==first.source||first.next==first.source||first.source==first.shell)
        return LowerZOrderKind::Unconfirmed;
    if(!first.next)return LowerZOrderKind::RawBottom;
    if(first.shell&&first.next==first.shell&&first.shell_known&&first.shell_pid&&first.shell_tid&&
       first.previous!=first.shell&&first.shell_previous==first.source&&!first.shell_next)
        return LowerZOrderKind::ShellDesktopTail;
    return LowerZOrderKind::Unconfirmed;
}

const wchar_t* WindowWriter::lower_failure_notice(const Receipt& receipt)noexcept{
    if(receipt.accepted)return L"";
    if(!receipt.completion_known)return L"置后操作尚未取得真实完成回执；后续操作等待该操作结束。";
    if(!receipt.native_attempted)return L"置后未发出原生调用；请查看拒绝原因记录。";
    if(!receipt.native_succeeded)return L"置后原生调用已失败；请查看诊断记录。";
    return L"置后原生调用已结束，但窗口后验未确认；请查看诊断记录。";
}
bool WindowWriter::stop(){return executor_->stop();}
bool WindowWriter::has_unconfirmed_native_call()const{
    std::lock_guard lock(executor_->state->mutex);const auto work=executor_->state->pending;if(!work)return false;
    std::lock_guard job_lock(work->mutex);return work->attempted&&!work->known;
}
bool WindowWriter::has_pending_native_operation()const{
    std::lock_guard lock(executor_->state->mutex);const auto work=executor_->state->pending;if(!work)return false;
    std::lock_guard job_lock(work->mutex);return !work->known;
}
HANDLE WindowWriter::duplicate_pending_completion_event()const{
    SetLastError(ERROR_SUCCESS);std::lock_guard lock(executor_->state->mutex);const auto work=executor_->state->pending;
    if(!work)return nullptr;std::lock_guard job_lock(work->mutex);if(work->known)return nullptr;
    HANDLE copy{};if(!DuplicateHandle(GetCurrentProcess(),work->done,GetCurrentProcess(),&copy,SYNCHRONIZE,FALSE,0))return nullptr;
    return copy;
}
void WindowWriter::notify_on_completion(HWND owner){executor_->state->notify=owner;}
std::vector<WindowWriter::Receipt> WindowWriter::take_delayed_completions(const core::v1::Settings& settings){
    std::vector<std::shared_ptr<NativeExecutor::Work>> jobs;
    {std::lock_guard lock(executor_->state->mutex);jobs.swap(executor_->state->late);executor_->state->late.reserve(8);}
    std::vector<Receipt> result;result.reserve(jobs.size());
    for(const auto& job:jobs){
        Receipt r;r.before=job->expected;r.requested=job->requested;r.destination=job->destination;
        {std::lock_guard lock(job->mutex);r.call_id=job->id;r.native_attempted=job->attempted;r.native_succeeded=job->succeeded;
            r.completion_known=job->known;r.before_qpc=job->before;r.after_qpc=job->after;r.error=job->error;
            r.preflight_started_qpc=job->preflight_started;r.native_position_flags=job->position_flags;
            r.operation=job->operation;r.visual_before=job->visual_before;r.visual_target=job->visual_target;
            r.alpha_change=job->alpha_change;r.top_change=job->top_change;r.visual_native=job->visual_native;r.reason=job->reason;}
        if(r.operation!=Operation::Placement){observe_auxiliary(r,settings,job->capture);result.push_back(std::move(r));continue;}
        r.post_track_limits_query_requested=placement_post_needs_track_limits(r,job->native);
        r.after=catalog_.get(r.before.hwnd,settings,nullptr,false,*r.post_track_limits_query_requested);
        retain_placement_preflight_limits(r);
        observe_post_context(r,[&]{
            r.identity_valid=r.after&&Catalog::same(r.before,*r.after);
            r.geometry_exact=r.identity_valid&&r.after->visible==r.requested&&r.after->positioning==job->native;
            r.accepted=r.completion_known&&r.native_attempted&&r.native_succeeded&&r.geometry_exact;
            r.reason="delayed_native_completion_observed_no_authority_restored";
            if(!observe_placement_completion(r,catalog_,job->capture)){r.accepted=false;r.reason=r.post_context_reason;}
        },now);
        result.push_back(std::move(r));
    }return result;
}

WindowWriter::Preparation WindowWriter::prepare(const Snapshot& expected,
    const Snapshot& actual, const Rect& desired) {
    Preparation result;
    result.identity_valid = Catalog::same(expected, actual);
    if (!native_rect(expected.visible) || !native_rect(expected.positioning) ||
        !native_rect(actual.visible) || !native_rect(actual.positioning)) {
        result.reason = "invalid_or_out_of_native_range_frame";
        return result;
    }
    if (!consistent(expected, actual, result.reason) ||
        !target_allowed(expected, actual, desired, result.reason)) return result;
    const auto converted = operations::prepare_visible_rect_adjustment(actual.positioning, actual.visible, desired);
    if (converted.status != operations::RectAdjustmentStatus::Succeeded || !converted.positioning) {
        result.reason = "positioning_bridge_invalid_or_out_of_native_range";
        return result;
    }
    result.positioning = converted.positioning;
    result.noop = actual.visible == desired && actual.positioning == *result.positioning;
    result.accepted = true;
    result.reason = result.noop ? "already_exact_no_native_call" : "prepared";
    return result;
}

UINT WindowWriter::resize_message_edge(v::Edges e)noexcept{
    if((e.left&&e.right)||(e.top&&e.bottom)||!(e.left||e.top||e.right||e.bottom))return 0;
    if(e.top)return e.left?WMSZ_TOPLEFT:e.right?WMSZ_TOPRIGHT:WMSZ_TOP;
    if(e.bottom)return e.left?WMSZ_BOTTOMLEFT:e.right?WMSZ_BOTTOMRIGHT:WMSZ_BOTTOM;
    return e.left?WMSZ_LEFT:WMSZ_RIGHT;
}

bool WindowWriter::resize_request_scope_allowed(const ResizeRequest& request,std::size_t placement_count)noexcept{
    return resize_message_edge(request.edges)&&placement_count&&placement_count<=256&&
        (request.coupled||placement_count==1);
}

WindowWriter::ResizePreparation WindowWriter::prepare_resize_query(const Snapshot& expected,
    const Snapshot& actual,const Rect& requested,const ResizeRequest& request,const ResizeQueryFacts& facts){
    ResizePreparation result;
    const auto prepared=prepare(expected,actual,requested);
    if(!prepared.accepted||!prepared.positioning){result.reason=prepared.reason;return result;}
    const auto edge=resize_message_edge(request.edges);
    if(!edge||facts.edge!=edge){result.reason="resize_query_invalid_active_edges";return result;}
    if(!facts.attempted||!facts.api_return){result.reason="resize_query_failed_or_timeout";return result;}
    const auto& r=facts.requested_native;const auto& c=facts.returned_native;
    const auto& native=*prepared.positioning;
    if(r.left!=native.left()||r.top!=native.top()||r.right!=native.right()||r.bottom!=native.bottom()){
        result.reason="resize_query_requested_frame_mismatch";return result;
    }
    const bool changed=c.left!=r.left||c.top!=r.top||c.right!=r.right||c.bottom!=r.bottom;
    if(facts.message_result!=TRUE){
        // A delivered, unhandled and unchanged proposal keeps the old bounds
        // policy. A changed/ambiguous answer never silently supplies a grid.
        if(facts.message_result||changed){result.reason="resize_query_unhandled_changed_or_unknown_result";return result;}
        result.accepted=true;result.visible=requested;result.reason="resize_query_unhandled_unchanged";return result;
    }
    if(c.left>=c.right||c.top>=c.bottom){result.reason="resize_query_invalid_raw_rectangle";return result;}
    if((!request.edges.left&&c.left!=r.left)||(!request.edges.top&&c.top!=r.top)||
       (!request.edges.right&&c.right!=r.right)||(!request.edges.bottom&&c.bottom!=r.bottom)){
        result.reason="resize_query_changed_inactive_edge";return result;
    }
    if(request.coupled&&changed){result.reason="resize_query_would_change_coupled_plan";return result;}
    // Invert the SAME checked per-edge frame bridge, not total frame size or
    // inferred font/grid increments. Raw ordering was checked above.
    const auto visible=operations::prepare_visible_rect_adjustment(actual.visible,actual.positioning,
        operations::RawRectEdges{c.left,c.top,c.right,c.bottom});
    if(visible.status!=operations::RectAdjustmentStatus::Succeeded||!visible.positioning){
        result.reason="resize_query_visible_bridge_invalid";return result;
    }
    const auto final=prepare(expected,actual,*visible.positioning);
    if(!final.accepted||!final.positioning){result.reason="resize_query_"+final.reason;return result;}
    if(final.positioning->left()!=c.left||final.positioning->top()!=c.top||
       final.positioning->right()!=c.right||final.positioning->bottom()!=c.bottom){
        result.reason="resize_query_frame_roundtrip_mismatch";return result;
    }
    result.accepted=true;result.adjusted=changed;result.visible=*visible.positioning;
    result.reason=changed?"resize_query_app_adjusted_this_proposal":"resize_query_handled_unchanged";
    return result;
}

bool WindowWriter::placement_post_needs_track_limits(const Receipt& r,const Rect& native)noexcept{
    return !(r.operation==Operation::Placement&&!r.destination&&r.native_attempted&&r.completion_known&&r.native_succeeded&&
        r.requested.size()==r.before.visible.size()&&native.size()==r.before.positioning.size());
}

void WindowWriter::retain_placement_preflight_limits(Receipt& r){
    if(!r.post_track_limits_query_requested||*r.post_track_limits_query_requested||!r.after||
        !Catalog::same(r.before,*r.after))return;
    // These are the limits actually observed before THIS call, not fresh
    // post-call limits. All fresh placement admissions still query the app;
    // carrying them does not select a new intent or grant write authority.
    r.after->limits=r.before.limits;
    r.after->declared_track_limits_observed=false;
    r.after->application_grid_known=false;
    r.post_track_limits_carried_from_preflight=true;
}

UINT WindowWriter::placement_flags(const Snapshot& fresh,const Rect& positioning)noexcept{
    const UINT base=SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER;
    return base|(positioning.size()==fresh.positioning.size()?SWP_NOSIZE:0U);
}

WindowWriter::DestinationPreparation WindowWriter::prepare_destination(const Snapshot& before,const Rect& native,
    HMONITOR planned,DPI_AWARENESS awareness,std::span<const Display> displays){
    DestinationPreparation result;
    if(!before.monitor||!planned||!native_rect(native)){result.reason="destination_monitor_unavailable";return result;}
    if(planned==before.monitor){result.accepted=true;result.reason="same_display_no_transition_proof";return result;}
    if(!same_topology(displays,displays)){result.reason="destination_display_snapshot_incomplete";return result;}
    const auto source=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==before.monitor;});
    const auto target=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==planned;});
    if(source==displays.end()||target==displays.end()||source->work!=before.work||!source->dpi||!target->dpi||!before.dpi){
        result.reason="destination_display_not_in_current_snapshot";return result;
    }
    UINT window_dpi{};
    switch(awareness){
        case DPI_AWARENESS_UNAWARE:if(before.dpi==96)window_dpi=96;break;
        case DPI_AWARENESS_SYSTEM_AWARE:window_dpi=before.dpi;break;
        case DPI_AWARENESS_PER_MONITOR_AWARE:if(before.dpi==source->dpi)window_dpi=target->dpi;break;
        default:break;
    }
    if(!window_dpi){result.reason="destination_window_dpi_awareness_unknown_or_inconsistent";return result;}
    result.proof=DestinationDisplay{*source,*target,native,awareness,window_dpi,{displays.begin(),displays.end()}};
    result.accepted=true;result.reason="explicit_destination_display_bound";return result;
}

bool WindowWriter::placement_context_matches(const Snapshot& before,const Snapshot& after,
    const std::optional<DestinationDisplay>& destination,std::span<const Display> current,DPI_AWARENESS awareness,std::string& reason){
    if(!Catalog::same(before,after)){reason="post_native_window_identity_changed";return false;}
    if(before.maximized!=after.maximized||before.minimized!=after.minimized||before.can_resize!=after.can_resize){
        reason="post_native_window_state_changed";return false;
    }
    if(!destination){
        if(before.monitor!=after.monitor||before.work!=after.work||before.dpi!=after.dpi){reason="post_native_geometry_context_changed";return false;}
        return true;
    }
    const auto& proof=*destination;
    if(proof.source.monitor!=before.monitor||proof.source.work!=before.work||proof.target.monitor==before.monitor||
       !proof.window_dpi||awareness!=proof.awareness){reason="post_native_destination_proof_mismatch";return false;}
    if(!same_topology(proof.topology,current)){reason="post_native_destination_display_topology_changed";return false;}
    if(after.monitor!=proof.target.monitor||after.work!=proof.target.work||after.dpi!=proof.window_dpi){
        reason="post_native_not_on_explicit_destination_display";return false;
    }
    return true;
}

WindowWriter::VisualPreparation WindowWriter::prepare_visual(const v::VisualState& before,const v::VisualState& actual,
    const v::VisualState& target,bool alpha,bool top,const std::optional<v::VisualOwnership>& cleanup){
    VisualPreparation result;
    if(alpha&&((actual.layered&&!actual.attributes_known)||(target.layered&&!target.attributes_known))){result.reason="visual_alpha_tuple_unknown";return result;}
    if(!same_changed_visual(before,actual,alpha,top)){result.reason="visual_owned_baseline_changed";return result;}
    if(cleanup){
        if(alpha&&(!v::owns_alpha(*cleanup,actual)||!v::same_alpha(target,cleanup->original))){result.reason="visual_cleanup_not_owned_original_alpha";return result;}
        if(top&&(!v::owns_top(*cleanup,actual)||target.topmost!=cleanup->original.topmost)){result.reason="visual_cleanup_not_owned_original_top";return result;}
    }
    result.accepted=true;result.noop=same_changed_visual(actual,target,alpha,top);
    result.reason=result.noop?"visual_already_exact_no_native_call":"visual_prepared";return result;
}

void WindowWriter::observe_auxiliary(Receipt& result,const v::Settings& settings,const GestureCaptureLease& capture){
    std::string capture_reason;result.after=catalog_.get(result.before.hwnd,settings,&capture_reason,true,false);
    observe_post_context(result,[&]{
        result.identity_valid=result.after&&Catalog::same(result.before,*result.after);
        if(result.identity_valid)result.visual_after=read_visual(result.before.hwnd);
        result.geometry_exact=result.identity_valid&&result.after->visible==result.before.visible&&result.after->positioning==result.before.positioning;
        if(result.identity_valid){
            if(result.operation==Operation::Lower){
                // Exactly two finite observations, never a traversal/retry/poll.
                // Equality is a bounded stability check, not an atomic Z snapshot.
                result.z_lower_begin_qpc=now();
                result.z_lower_samples[0]=observe_lower_z_order(result.before);
                result.z_lower_samples[1]=observe_lower_z_order(result.before);
                result.z_lower_end_qpc=now();
                const auto& actual=result.z_lower_samples[1];
                result.z_previous=actual.previous;result.z_next=actual.next;
                result.z_bottom_observed=actual.known&&!actual.next;
                result.z_lower_kind=lower_z_order_completion(result.before,result.z_lower_samples[0],actual);
                result.z_lower_observed=result.z_lower_kind!=LowerZOrderKind::Unconfirmed;
            }else{
                result.z_previous=GetWindow(result.before.hwnd,GW_HWNDPREV);result.z_next=GetWindow(result.before.hwnd,GW_HWNDNEXT);
                result.z_bottom_observed=result.z_next==nullptr;
            }
        }
        result.visual_exact=result.visual_after&&same_changed_visual(*result.visual_after,result.visual_target,result.alpha_change,result.top_change);
        const bool context=observe_capture_completion(result,capture);
        if(!result.identity_valid){result.reason="visual_postverify_"+capture_reason;return;}
        if(!result.visual_after){result.reason="visual_postverify_attributes_unavailable";return;}
        if(!result.geometry_exact){result.reason="visual_native_changed_or_unattributed_geometry";return;}
        if(!context){result.reason=result.post_context_reason;return;}
        if(result.operation==Operation::Lower){
            result.accepted=result.native_attempted&&result.native_succeeded&&!result.visual_after->topmost&&result.z_lower_observed;
            result.reason=!result.accepted?"lower_native_completion_requires_zorder_review":
                result.z_lower_kind==LowerZOrderKind::ShellDesktopTail?"lower_native_shell_desktop_tail_observed":"lower_native_bottom_observed";return;
        }
        if(!result.native_attempted&&result.reason!="visual_already_exact_worker_no_native_call"&&result.reason!="visual_already_exact_no_native_call")return;
        result.accepted=result.visual_exact&&(!result.native_attempted||result.native_succeeded)&&result.after_qpc!=0;
        result.reason=result.accepted?"exact_visual_completion":!result.native_succeeded?"native_visual_sequence_failed":"native_visual_not_exact";
    },now);
}

WindowWriter::Receipt WindowWriter::visual(const Snapshot& expected,const v::VisualState& before,const v::VisualState& target,
    bool alpha,bool top,const v::Settings& settings,Guard guard,std::optional<v::VisualOwnership> cleanup,BeforeIssue before_issue,GestureCaptureLease capture){
    Receipt result;result.preflight_started_qpc=now();result.before=expected;result.requested=expected.visible;result.operation=Operation::Visual;
    result.visual_before=before;result.visual_target=target;result.alpha_change=alpha;result.top_change=top;
    if(!cleanup&&!settings.enabled){result.reason="visual_product_disabled";return result;}
    std::string why;const auto fresh=catalog_.get(expected.hwnd,settings,&why,cleanup.has_value(),false);
    if(!fresh||!Catalog::same(expected,*fresh)){result.reason="visual_preflight_"+why;return result;}
    result.before=*fresh;result.identity_valid=true;const auto actual=read_visual(fresh->hwnd);
    if(capture&&!geometry_gui_context_allowed(*fresh,observe_gui_context(*fresh),capture,GeometryContextPhase::BeforeNativeCall,&result.reason))return result;
    if(!actual){result.reason="visual_preflight_attributes_unavailable";return result;}
    // A known no-call rejection can still settle the helper's prepared tuple.
    // This is a preflight observation, never an invented native completion.
    result.visual_after=actual;result.geometry_observed_qpc=now();
    const auto prepared=prepare_visual(before,*actual,target,alpha,top,cleanup);
    if(!prepared.accepted){result.reason=prepared.reason;return result;}
    if(!cleanup&&!allowed_by_guard(guard,result.reason))return result;
    const auto work=executor_->execute(*fresh,fresh->positioning,fresh->visible,std::move(guard),result.preflight_started_qpc,Operation::Visual,before,target,alpha,top,std::move(cleanup),std::move(before_issue),std::move(capture));
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    result.visual_after.reset();result.geometry_observed_qpc=result.post_observed_qpc=0;
    {std::lock_guard lock(work->mutex);result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;result.reason=work->reason;result.visual_native=work->visual_native;}
    if(result.completion_known)observe_auxiliary(result,settings,work->capture);return result;
}

WindowWriter::Receipt WindowWriter::lower(const Snapshot& expected,const v::Settings& settings,Guard guard){
    Receipt result;result.preflight_started_qpc=now();result.before=expected;result.requested=expected.visible;result.operation=Operation::Lower;
    if(!settings.enabled){result.reason="lower_product_disabled";return result;}
    std::string why;const auto fresh=catalog_.get(expected.hwnd,settings,&why,false,false);
    if(!fresh||!consistent(expected,*fresh,why)){result.reason="lower_preflight_"+why;return result;}
    result.before=*fresh;if(!allowed_by_guard(guard,result.reason))return result;
    const auto work=executor_->execute(*fresh,fresh->positioning,fresh->visible,std::move(guard),result.preflight_started_qpc,Operation::Lower);
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    {std::lock_guard lock(work->mutex);result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;result.reason=work->reason;result.visual_native=work->visual_native;}
    if(result.completion_known)observe_auxiliary(result,settings);return result;
}

WindowWriter::Receipt WindowWriter::place(const Snapshot& expected,
    const Rect& desired, const core::v1::Settings& settings, Guard guard,GestureCaptureLease capture) {
    return place_with_destination(expected,desired,settings,std::move(guard),std::move(capture),nullptr);
}

bool WindowWriter::preserves_terminal_receipt(const Receipt& receipt,std::uint64_t generation,
    const GestureCaptureLease& capture,bool generation_allowed)noexcept{
    if(receipt.accepted&&receipt.identity_valid&&receipt.geometry_exact)return true;
    if(receipt.native_attempted)return receipt.native_succeeded&&receipt.completion_known&&
        receipt.identity_valid&&receipt.geometry_exact&&
        receipt.post_context_reason=="post_native_capture_cleanup_not_yet_confirmed";
    if(!receipt.completion_known)return false;
    if(receipt.reason=="operation_revoked_before_native_attempt")return true;
    const std::string_view context_reason=(receipt.reason=="batch_preflight_capture_matching_UP_retired_before_native_call"||
        receipt.reason=="resize_query_capture_matching_UP_retired_before_native_call")?
        capture_matching_up_retired_reason:std::string_view(receipt.reason);
    return gesture_gui_disposition(false,context_reason,generation,capture,generation_allowed)==GestureGuiDisposition::MatchingUpRetired;
}

WindowWriter::Receipt WindowWriter::place_with_destination(const Snapshot& expected,
    const Rect& desired,const core::v1::Settings& settings,Guard guard,GestureCaptureLease capture,
    const std::optional<DestinationDisplay>* frozen){
    const auto preflight_started=now();
    Receipt result = rejected(expected, desired, "not_attempted");result.preflight_started_qpc=preflight_started;
    if (!native_rect(desired)) {
        result.reason = "invalid_or_out_of_native_range_target";
        return result;
    }
    std::string capture_reason;
    const auto before = catalog_.get(expected.hwnd, settings, &capture_reason);
    if (!before) {
        result.reason = "preflight_" + capture_reason;
        return result;
    }
    result.before = *before;
    const auto prepared = prepare(expected, *before, desired);
    result.identity_valid = prepared.identity_valid;
    if (!prepared.accepted || !prepared.positioning) { result.reason = prepared.reason; return result; }
    if(capture&&!geometry_gui_context_allowed(*before,observe_gui_context(*before),capture,GeometryContextPhase::BeforeNativeCall,&result.reason))return result;
    if (prepared.noop) {
        // This permission check does not create an API attempt. Even an exact
        // no-op may not be credited after the caller has revoked its operation.
        if (!allowed_by_guard(guard, result.reason)) return result;
        result.after = *before;
        result.accepted = result.geometry_exact = true;
        result.reason = "already_exact_no_native_call";
        return result;
    }

    const auto& native = *prepared.positioning;
    if(frozen){
        result.destination=*frozen;
        if(result.destination){
            const auto& proof=*result.destination;
            const auto current=catalog_.displays();
            const auto awareness=GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(before->hwnd));
            if(proof.positioning!=native||planned_monitor(native)!=proof.target.monitor||proof.source.monitor!=before->monitor||
               proof.source.work!=before->work||awareness!=proof.awareness||!same_topology(proof.topology,current)){
                result.reason="batch_destination_display_proof_changed";return result;
            }
        }else if(planned_monitor(native)!=before->monitor){result.reason="batch_unbound_destination_display_changed";return result;}
    }else{
        const auto destination=observe_destination(catalog_,*before,native);
        if(!destination.accepted){result.reason=destination.reason;return result;}
        result.destination=destination.proof;
    }
    if (!allowed_by_guard(guard, result.reason)) return result;
    const auto work=executor_->execute(*before,native,desired,std::move(guard),result.preflight_started_qpc,Operation::Placement,{},{},false,false,std::nullopt,{},std::move(capture),result.destination);
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    {
        std::lock_guard lock(work->mutex);
        result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;
        result.native_position_flags=work->position_flags;
        if(!work->known||!work->attempted){result.reason=work->reason;return result;}
    }

    // This is observation of an issued call, not a new permission request.
    // In particular, do not call guard again: a matching normal UP must not
    // turn an otherwise exact completion into a false authority failure.
    result.post_track_limits_query_requested=placement_post_needs_track_limits(result,native);
    result.after = catalog_.get(before->hwnd, settings, &capture_reason,false,*result.post_track_limits_query_requested);
    retain_placement_preflight_limits(result);
    bool context{};
    observe_post_context(result,[&]{
        result.identity_valid = result.after && Catalog::same(*before, *result.after);
        result.geometry_exact = result.identity_valid && result.after->visible == desired &&
            result.after->positioning == native;
        context=observe_placement_completion(result,catalog_,work->capture);
    },now);
    if (!result.native_succeeded) result.reason = "native_placement_failed";
    else if (!result.after) result.reason = "postverify_" + capture_reason;
    else if (!result.identity_valid) result.reason = "identity_changed_after_native_attempt";
    else if (!result.geometry_exact) result.reason = "native_geometry_not_exact_constraints_unattributed";
    else if (!context) result.reason=result.post_context_reason;
    else if (!result.after_qpc) result.reason = "native_completion_clock_unavailable";
    else { result.accepted = true; result.reason = "exact_native_completion"; }
    return result;
}

std::vector<WindowWriter::Receipt> WindowWriter::batch(std::span<const Snapshot> identities,
    std::span<const core::v1::Placement> placements,
    const core::v1::Settings& settings, Guard guard,GestureCaptureLease capture,
    std::optional<ResizeRequest> resize) {
    std::vector<Receipt> result;
    // Bound the product batch, independently of the untrusted native desktop.
    if (identities.empty() || identities.size() > 256 || placements.empty() ||
        placements.size() > identities.size()) {
        result.push_back(rejected({}, {}, "invalid_or_unbounded_batch"));
        return result;
    }
    std::unordered_set<core::v1::Key> members, targets;
    std::vector<std::optional<DestinationDisplay>> destinations(placements.size());
    std::vector<Snapshot> preflight;
    if(resize)preflight.reserve(identities.size());
    std::unordered_set<HWND> windows;
    for (const auto& expected : identities) {
        if (!expected.key || !expected.hwnd || !members.insert(expected.key).second ||
            !windows.insert(expected.hwnd).second) {
            result.push_back(rejected(expected, expected.visible, "duplicate_or_invalid_batch_identity"));
            return result;
        }
    }
    for (const auto& placement : placements) {
        if (!members.contains(placement.id) || !targets.insert(placement.id).second) {
            result.push_back(rejected({}, placement.visible, "duplicate_or_out_of_scope_batch_target"));
            return result;
        }
    }
    if(resize&&!resize_request_scope_allowed(*resize,placements.size())){
        result.push_back(rejected(identities.front(),placements.front().visible,"invalid_live_resize_request"));return result;
    }
    // Complete ALL source/participant preflights before the first native call.
    // Fresh geometry cannot silently become a new baseline for a frozen plan.
    for (const auto& expected : identities) {
        std::string reason;
        const auto placement = std::find_if(placements.begin(), placements.end(),
            [&](const auto& target) { return target.id == expected.key; });
        const auto requested=placement!=placements.end()?placement->visible:expected.visible;
        const auto current = catalog_.get(expected.hwnd, settings, &reason);
        if (!current || !consistent(expected, *current, reason)) {
            result.push_back(rejected(expected, requested, "batch_preflight_" + reason));
            return result;
        }
        if(capture&&!geometry_gui_context_allowed(*current,observe_gui_context(*current),capture,GeometryContextPhase::BeforeNativeCall,&reason)){
            result.push_back(rejected(expected,requested,"batch_preflight_"+reason));return result;
        }
        if (placement != placements.end()) {
            const auto prepared = prepare(expected, *current, placement->visible);
            if (!prepared.accepted) {
                result.push_back(rejected(expected, placement->visible, "batch_preflight_" + prepared.reason));
                return result;
            }
            if(!prepared.noop&&prepared.positioning){
                const auto destination=observe_destination(catalog_,*current,*prepared.positioning);
                if(!destination.accepted){result.push_back(rejected(expected,placement->visible,"batch_preflight_"+destination.reason));return result;}
                destinations[static_cast<std::size_t>(placement-placements.begin())]=destination.proof;
            }
        }
        if(resize)preflight.push_back(*current);
    }
    std::vector<v::Placement> adjusted;
    std::vector<std::optional<ResizeQueryFacts>> queries;
    if(resize){
        adjusted.assign(placements.begin(),placements.end());queries.resize(placements.size());
        const auto resize_guard=[&](std::string& reason){
            if(allowed_by_guard(guard,reason))return true;
            if(reason=="operation_revoked_before_native_attempt")reason=capture&&capture->generation&&
                capture->matching_up_observed.load(std::memory_order_acquire)?
                "resize_query_capture_matching_UP_retired_before_native_call":"resize_query_operation_revoked_before_native_attempt";
            return false;
        };
        const auto reject_resize=[&](std::size_t failed,const std::string& reason,const Snapshot* observer=nullptr){
            // Prior callback attempts remain visible when a later member
            // rejects. No one of these receipts claims a placement attempt.
            result.clear();
            for(std::size_t index=0;index<placements.size();++index)if(queries[index]||(!observer&&index==failed)){
                const auto identity=std::find_if(identities.begin(),identities.end(),[&](const auto& item){return item.key==placements[index].id;});
                auto receipt=rejected(*identity,adjusted[index].visible,reason);
                receipt.resize_query=queries[index];receipt.resize_original_requested=placements[index].visible;
                result.push_back(std::move(receipt));
            }
            if(observer)result.push_back(rejected(*observer,observer->visible,reason));
        };
        for(std::size_t index=0;index<placements.size();++index){
            const auto& placement=placements[index];
            const auto current=std::find_if(preflight.begin(),preflight.end(),[&](const auto& item){return item.key==placement.id;});
            if(placement.visible.size()==current->visible.size())continue;
            // Coupled members can have the opposite edge from the leader.
            // Describe only this member's actual proposed changed edges.
            ResizeRequest member=*resize;
            if(member.coupled)member.edges={placement.visible.left()!=current->visible.left(),
                placement.visible.top()!=current->visible.top(),placement.visible.right()!=current->visible.right(),
                placement.visible.bottom()!=current->visible.bottom()};
            const auto edge=resize_message_edge(member.edges);
            if(!edge){reject_resize(index,"resize_query_ambiguous_member_edges");return result;}
            std::string reason;
            const auto before=catalog_.get(current->hwnd,settings,&reason);
            if(!before||!consistent(*current,*before,reason)){
                reject_resize(index,"batch_preflight_"+reason);return result;
            }
            if(capture&&!geometry_gui_context_allowed(*before,observe_gui_context(*before),capture,
                GeometryContextPhase::BeforeNativeCall,&reason)){
                reject_resize(index,"batch_preflight_"+reason);return result;
            }
            const auto prepared=prepare(*current,*before,placement.visible);
            if(!prepared.accepted||!prepared.positioning){reject_resize(index,"batch_preflight_"+prepared.reason);return result;}
            if(!resize_guard(reason)){reject_resize(index,reason);return result;}
            const auto& r=*prepared.positioning;
            ResizeQueryFacts facts;facts.edge=edge;
            facts.requested_native={static_cast<LONG>(r.left()),static_cast<LONG>(r.top()),static_cast<LONG>(r.right()),static_cast<LONG>(r.bottom())};
            facts.returned_native=facts.requested_native;
            // GUI adapter only, after all-member preflight and BEFORE any
            // placement. No input/resource/work mutex is held across a foreign
            // callback. The independent input owner can revoke on real UP.
            facts.before_qpc=now();SetLastError(ERROR_SUCCESS);facts.attempted=true;
            facts.api_return=SendMessageTimeoutW(before->hwnd,WM_SIZING,edge,reinterpret_cast<LPARAM>(&facts.returned_native),
                SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,30,&facts.message_result);
            facts.error=GetLastError();facts.after_qpc=now();queries[index]=facts;
            const auto decision=prepare_resize_query(*current,*before,placement.visible,member,facts);
            if(!decision.accepted||!decision.visible){reject_resize(index,decision.reason);return result;}
            adjusted[index].visible=*decision.visible;
            // A callback return is not write authority. Use the existing
            // exact normal-UP withdrawal reason; do not mask an API/context
            // failure above merely because UP also arrived during the call.
            if(!resize_guard(reason)){reject_resize(index,reason);return result;}
        }
        // Callbacks are application-affecting, not read-only snapshots. Recheck
        // ALL members after ALL callbacks against the frozen pre-call frames,
        // including participants with no placement. Never accept a new baseline.
        for(const auto& expected:identities){
            std::string reason;
            const auto found=std::find_if(adjusted.begin(),adjusted.end(),[&](const auto& item){return item.id==expected.key;});
            const auto index=found==adjusted.end()?std::size_t{0}:static_cast<std::size_t>(found-adjusted.begin());
            const auto reject_context=[&](const std::string& failure){reject_resize(index,failure,found==adjusted.end()?&expected:nullptr);};
            const auto current=catalog_.get(expected.hwnd,settings,&reason);
            if(!current||!consistent(expected,*current,reason)){
                reject_context("batch_preflight_"+reason);return result;
            }
            if(!resize_guard(reason)){reject_context(reason);return result;}
            if(capture&&!geometry_gui_context_allowed(*current,observe_gui_context(*current),capture,
                GeometryContextPhase::BeforeNativeCall,&reason)){
                reject_context("batch_preflight_"+reason);return result;
            }
            if(found!=adjusted.end()){
                const auto prepared=prepare(expected,*current,found->visible);
                if(!prepared.accepted||!prepared.positioning){reject_resize(index,"batch_preflight_"+prepared.reason);return result;}
                destinations[index].reset();
                if(!prepared.noop){
                    const auto destination=observe_destination(catalog_,*current,*prepared.positioning);
                    if(!destination.accepted){reject_resize(index,"batch_preflight_"+destination.reason);return result;}
                    destinations[index]=destination.proof;
                }
            }
        }
        for(auto& query:queries)if(query)query->context_revalidated=true;
    }
    const auto final_placements=resize?std::span<const v::Placement>{adjusted}:placements;
    result.reserve(placements.size());
    for (const auto& placement : final_placements) {
        const auto expected = std::find_if(identities.begin(), identities.end(),
            [&](const auto& member) { return member.key == placement.id; });
        const auto index=static_cast<std::size_t>(&placement-final_placements.data());
        result.push_back(place_with_destination(*expected,placement.visible,settings,guard,capture,&destinations[index]));
        if(resize){result.back().resize_query=queries[index];result.back().resize_original_requested=placements[index].visible;}
        if (!result.back().accepted) break;
    }
    return result;
}

} // namespace panebind::platform::windows::product
