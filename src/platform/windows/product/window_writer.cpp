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
bool native_visual_identity(const Snapshot& expected,GuiContextMode* observed_mode=nullptr){
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
    if(!gui.idle())return false;
    const auto desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!desktop)return false;
    BOOL receiving{};DWORD bytes{};valid=GetUserObjectInformationW(desktop,UOI_IO,&receiving,sizeof(receiving),&bytes)&&receiving;
    CloseDesktop(desktop);return valid;
}

std::uint64_t now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart > 0 ?
        static_cast<std::uint64_t>(value.QuadPart) : 0;
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
        Operation operation{Operation::Placement};v::VisualState visual_before,visual_target;
        bool alpha_change{},top_change{};std::optional<v::VisualOwnership> cleanup_owned;VisualNativeFacts visual_native;
        HANDLE done{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::mutex mutex;bool cancelled{},attempted{},succeeded{},known{},timed_out{};
        DWORD error{};std::uint64_t before{},after{};std::string reason;
        ~Work(){if(done)CloseHandle(done);}
    };
    struct State {
        HANDLE wake{CreateEventW(nullptr,FALSE,FALSE,nullptr)},exited{CreateEventW(nullptr,TRUE,FALSE,nullptr)};
        std::atomic<bool> stopping{};std::atomic<HWND> notify{};
        std::mutex mutex;std::shared_ptr<Work> pending;std::vector<std::shared_ptr<Work>> late;
        std::uint64_t next_id{};
        ~State(){if(wake)CloseHandle(wake);if(exited)CloseHandle(exited);}
    };
    std::shared_ptr<State> state{std::make_shared<State>()};std::thread thread;
    static bool first_attempt(const std::shared_ptr<State>& shared,const std::shared_ptr<Work>& work){
        std::lock_guard lock(work->mutex);
        if(work->cancelled||shared->stopping){work->reason="native_worker_admission_cancelled";return false;}
        if(!work->cleanup_owned&&!allowed_by_guard(work->guard,work->reason))return false;
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
                if(!native_visual_identity(work->expected))return false;
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
            if(!native_visual_identity(work->expected))return false;
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
    static bool exact_context(const Snapshot& e,GuiContextMode* observed_mode=nullptr){
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
        const auto gui=observe_gui_context(e);if(observed_mode)*observed_mode=gui.mode;if(!gui.idle())return false;
        const auto desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);if(!desktop)return false;
        BOOL receiving{};DWORD size{};
        const bool active=GetUserObjectInformationW(desktop,UOI_IO,&receiving,sizeof(receiving),&size)&&receiving;
        CloseDesktop(desktop);return active;
    }
    NativeExecutor(){
        if(!state->wake||!state->exited)throw std::runtime_error("native_worker_events_failed");
        state->late.reserve(8);
        thread=std::thread([shared=state]{
            while(WaitForSingleObject(shared->wake,INFINITE)==WAIT_OBJECT_0){
                if(shared->stopping)break;std::shared_ptr<Work> work;
                {std::lock_guard lock(shared->mutex);work=shared->pending;}
                if(!work)continue;
                GuiContextMode gui_mode=GuiContextMode::Unknown;
                const bool context=work->operation==Operation::Visual?native_visual_identity(work->expected,&gui_mode):exact_context(work->expected,&gui_mode);
                bool issue=false;
                {
                    std::lock_guard lock(work->mutex);
                    if(work->cancelled||shared->stopping)work->reason="native_worker_admission_cancelled";
                    else if(!context)work->reason=std::string("native_worker_context_changed:GUI_context=")+gui_context_mode_name(gui_mode);
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
                                static_cast<int>(r.width()),static_cast<int>(r.height()),SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
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
    std::shared_ptr<Work> execute(const Snapshot& expected,const Rect& native,const Rect& requested,Guard guard,
        Operation operation=Operation::Placement,const v::VisualState& before={},const v::VisualState& target={},
        bool alpha=false,bool top=false,std::optional<v::VisualOwnership> cleanup_owned=std::nullopt,BeforeIssue before_issue={}){
        auto work=std::make_shared<Work>();if(!work->done)return {};
        work->expected=expected;work->native=native;work->requested=requested;work->guard=std::move(guard);work->reason.reserve(128);
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

WindowWriter::WindowWriter(Catalog& catalog) : catalog_(catalog),executor_(std::make_unique<NativeExecutor>()) {}
WindowWriter::~WindowWriter()=default;
bool WindowWriter::complete_unissued_on_stop(bool attempted,bool known)noexcept{return !attempted&&!known;}
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
        Receipt r;r.before=job->expected;r.requested=job->requested;
        {std::lock_guard lock(job->mutex);r.call_id=job->id;r.native_attempted=job->attempted;r.native_succeeded=job->succeeded;
            r.completion_known=job->known;r.before_qpc=job->before;r.after_qpc=job->after;r.error=job->error;
            r.operation=job->operation;r.visual_before=job->visual_before;r.visual_target=job->visual_target;
            r.alpha_change=job->alpha_change;r.top_change=job->top_change;r.visual_native=job->visual_native;r.reason=job->reason;}
        if(r.operation!=Operation::Placement){observe_auxiliary(r,settings);result.push_back(std::move(r));continue;}
        r.after=catalog_.get(r.before.hwnd,settings);r.post_observed_qpc=now();
        r.identity_valid=r.after&&Catalog::same(r.before,*r.after);
        r.geometry_exact=r.identity_valid&&r.after->visible==r.requested&&r.after->positioning==job->native;
        r.accepted=r.completion_known&&r.native_attempted&&r.native_succeeded&&r.geometry_exact;
        r.reason="delayed_native_completion_observed_no_authority_restored";result.push_back(std::move(r));
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

void WindowWriter::observe_auxiliary(Receipt& result,const v::Settings& settings){
    std::string capture_reason;result.after=catalog_.get(result.before.hwnd,settings,&capture_reason,true,false);result.post_observed_qpc=now();
    result.identity_valid=result.after&&Catalog::same(result.before,*result.after);
    if(result.identity_valid)result.visual_after=read_visual(result.before.hwnd);
    result.geometry_exact=result.identity_valid&&result.after->visible==result.before.visible&&result.after->positioning==result.before.positioning;
    if(result.identity_valid){result.z_previous=GetWindow(result.before.hwnd,GW_HWNDPREV);result.z_next=GetWindow(result.before.hwnd,GW_HWNDNEXT);result.z_bottom_observed=result.z_next==nullptr;}
    result.visual_exact=result.visual_after&&same_changed_visual(*result.visual_after,result.visual_target,result.alpha_change,result.top_change);
    if(!result.identity_valid){result.reason="visual_postverify_"+capture_reason;return;}
    if(!result.visual_after){result.reason="visual_postverify_attributes_unavailable";return;}
    if(!result.geometry_exact){result.reason="visual_native_changed_or_unattributed_geometry";return;}
    if(result.operation==Operation::Lower){
        result.accepted=result.native_attempted&&result.native_succeeded&&!result.visual_after->topmost&&result.z_bottom_observed;
        result.reason=result.accepted?"lower_native_bottom_observed":"lower_native_completion_requires_zorder_review";return;
    }
    if(!result.native_attempted&&result.reason!="visual_already_exact_worker_no_native_call"&&result.reason!="visual_already_exact_no_native_call")return;
    result.accepted=result.visual_exact&&(!result.native_attempted||result.native_succeeded)&&result.after_qpc!=0;
    result.reason=result.accepted?"exact_visual_completion":!result.native_succeeded?"native_visual_sequence_failed":"native_visual_not_exact";
}

WindowWriter::Receipt WindowWriter::visual(const Snapshot& expected,const v::VisualState& before,const v::VisualState& target,
    bool alpha,bool top,const v::Settings& settings,Guard guard,std::optional<v::VisualOwnership> cleanup,BeforeIssue before_issue){
    Receipt result;result.before=expected;result.requested=expected.visible;result.operation=Operation::Visual;
    result.visual_before=before;result.visual_target=target;result.alpha_change=alpha;result.top_change=top;
    if(!cleanup&&!settings.enabled){result.reason="visual_product_disabled";return result;}
    std::string why;const auto fresh=catalog_.get(expected.hwnd,settings,&why,cleanup.has_value(),false);
    if(!fresh||!Catalog::same(expected,*fresh)){result.reason="visual_preflight_"+why;return result;}
    result.before=*fresh;result.identity_valid=true;const auto actual=read_visual(fresh->hwnd);
    if(!actual){result.reason="visual_preflight_attributes_unavailable";return result;}
    // A known no-call rejection can still settle the helper's prepared tuple.
    // This is a preflight observation, never an invented native completion.
    result.visual_after=actual;result.post_observed_qpc=now();
    const auto prepared=prepare_visual(before,*actual,target,alpha,top,cleanup);
    if(!prepared.accepted){result.reason=prepared.reason;return result;}
    if(!cleanup&&!allowed_by_guard(guard,result.reason))return result;
    const auto work=executor_->execute(*fresh,fresh->positioning,fresh->visible,std::move(guard),Operation::Visual,before,target,alpha,top,std::move(cleanup),std::move(before_issue));
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    result.visual_after.reset();result.post_observed_qpc=0;
    {std::lock_guard lock(work->mutex);result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;result.reason=work->reason;result.visual_native=work->visual_native;}
    if(result.completion_known)observe_auxiliary(result,settings);return result;
}

WindowWriter::Receipt WindowWriter::lower(const Snapshot& expected,const v::Settings& settings,Guard guard){
    Receipt result;result.before=expected;result.requested=expected.visible;result.operation=Operation::Lower;
    if(!settings.enabled){result.reason="lower_product_disabled";return result;}
    std::string why;const auto fresh=catalog_.get(expected.hwnd,settings,&why,false,false);
    if(!fresh||!consistent(expected,*fresh,why)){result.reason="lower_preflight_"+why;return result;}
    result.before=*fresh;if(!allowed_by_guard(guard,result.reason))return result;
    const auto work=executor_->execute(*fresh,fresh->positioning,fresh->visible,std::move(guard),Operation::Lower);
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    {std::lock_guard lock(work->mutex);result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;result.reason=work->reason;result.visual_native=work->visual_native;}
    if(result.completion_known)observe_auxiliary(result,settings);return result;
}

WindowWriter::Receipt WindowWriter::place(const Snapshot& expected,
    const Rect& desired, const core::v1::Settings& settings, Guard guard) {
    Receipt result = rejected(expected, desired, "not_attempted");
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
    if (!allowed_by_guard(guard, result.reason)) return result;
    const auto work=executor_->execute(*before,native,desired,std::move(guard));
    if(!work){result.reason="native_worker_busy_or_stopped_no_new_call";return result;}
    {
        std::lock_guard lock(work->mutex);
        result.call_id=work->id;result.native_attempted=work->attempted;result.native_succeeded=work->succeeded;
        result.completion_known=work->known;result.error=work->error;result.before_qpc=work->before;result.after_qpc=work->after;
        if(!work->known||!work->attempted){result.reason=work->reason;return result;}
    }

    // This is observation of an issued call, not a new permission request.
    // In particular, do not call guard again: a matching normal UP must not
    // turn an otherwise exact completion into a false authority failure.
    result.after = catalog_.get(before->hwnd, settings, &capture_reason);result.post_observed_qpc=now();
    result.identity_valid = result.after && Catalog::same(*before, *result.after);
    result.geometry_exact = result.identity_valid && result.after->visible == desired &&
        result.after->positioning == native;
    if (!result.native_succeeded) result.reason = "native_placement_failed";
    else if (!result.after) result.reason = "postverify_" + capture_reason;
    else if (!result.identity_valid) result.reason = "identity_changed_after_native_attempt";
    else if (!result.geometry_exact) result.reason = "native_geometry_not_exact_constraints_unattributed";
    else if (!result.after_qpc) result.reason = "native_completion_clock_unavailable";
    else { result.accepted = true; result.reason = "exact_native_completion"; }
    return result;
}

std::vector<WindowWriter::Receipt> WindowWriter::batch(std::span<const Snapshot> identities,
    std::span<const core::v1::Placement> placements,
    const core::v1::Settings& settings, Guard guard) {
    std::vector<Receipt> result;
    // Bound the product batch, independently of the untrusted native desktop.
    if (identities.empty() || identities.size() > 256 || placements.empty() ||
        placements.size() > identities.size()) {
        result.push_back(rejected({}, {}, "invalid_or_unbounded_batch"));
        return result;
    }
    std::unordered_set<core::v1::Key> members, targets;
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
    // Complete ALL source/participant preflights before the first native call.
    // Fresh geometry cannot silently become a new baseline for a frozen plan.
    for (const auto& expected : identities) {
        std::string reason;
        const auto current = catalog_.get(expected.hwnd, settings, &reason);
        if (!current || !consistent(expected, *current, reason)) {
            result.push_back(rejected(expected, expected.visible, "batch_preflight_" + reason));
            return result;
        }
        const auto placement = std::find_if(placements.begin(), placements.end(),
            [&](const auto& target) { return target.id == expected.key; });
        if (placement != placements.end()) {
            const auto prepared = prepare(expected, *current, placement->visible);
            if (!prepared.accepted) {
                result.push_back(rejected(expected, placement->visible, "batch_preflight_" + prepared.reason));
                return result;
            }
        }
    }
    result.reserve(placements.size());
    for (const auto& placement : placements) {
        const auto expected = std::find_if(identities.begin(), identities.end(),
            [&](const auto& member) { return member.key == placement.id; });
        result.push_back(place(*expected, placement.visible, settings, guard));
        if (!result.back().accepted) break;
    }
    return result;
}

} // namespace panebind::platform::windows::product
