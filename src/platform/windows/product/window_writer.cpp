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
    static bool exact_context(const Snapshot& e){
        DWORD pid{};if(GetWindowThreadProcessId(e.hwnd,&pid)!=e.tid||pid!=e.pid)return false;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if(!process)return false;FILETIME created{},exit{},kernel{},user{};
        const bool identity=WaitForSingleObject(process,0)==WAIT_TIMEOUT&&GetProcessTimes(process,&created,&exit,&kernel,&user)&&
            ((static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime)==e.born;
        CloseHandle(process);if(!identity)return false;
        RECT positioning{},visible{};GUITHREADINFO gui{sizeof(gui)};
        const auto equal=[](const RECT& a,const Rect& b){return a.left==b.left()&&a.top==b.top()&&a.right==b.right()&&a.bottom==b.bottom();};
        if(!GetWindowRect(e.hwnd,&positioning)||FAILED(DwmGetWindowAttribute(e.hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&visible,sizeof(visible)))||
            !equal(positioning,e.positioning)||!equal(visible,e.visible)||GetDpiForWindow(e.hwnd)!=e.dpi||
            MonitorFromWindow(e.hwnd,MONITOR_DEFAULTTONULL)!=e.monitor||(IsZoomed(e.hwnd)!=FALSE)!=e.maximized||(IsIconic(e.hwnd)!=FALSE)!=e.minimized||
            !GetGUIThreadInfo(e.tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner)return false;
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
                const bool context=exact_context(work->expected);
                bool issue=false;
                {
                    std::lock_guard lock(work->mutex);
                    if(work->cancelled||shared->stopping)work->reason="native_worker_admission_cancelled";
                    else if(!context)work->reason="native_worker_context_changed";
                    else if(!allowed_by_guard(work->guard,work->reason)){}
                    else{work->before=now();if(work->before){work->attempted=true;issue=true;}else work->reason="native_attempt_clock_unavailable";}
                }
                if(issue){
                    // An UP after this claim retires later requests; it cannot
                    // undo a SetWindowPos already issued to the OS.
                    const auto& r=work->native;
                    const BOOL success=SetWindowPos(work->expected.hwnd,nullptr,static_cast<int>(r.left()),static_cast<int>(r.top()),
                        static_cast<int>(r.width()),static_cast<int>(r.height()),SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
                    const DWORD error=success?ERROR_SUCCESS:GetLastError();const auto completed=now();
                    std::lock_guard lock(work->mutex);work->succeeded=success!=FALSE;work->error=error;work->after=completed;
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
    std::shared_ptr<Work> execute(const Snapshot& expected,const Rect& native,const Rect& requested,Guard guard){
        auto work=std::make_shared<Work>();if(!work->done)return {};
        work->expected=expected;work->native=native;work->requested=requested;work->guard=std::move(guard);work->reason.reserve(128);
        {
            std::lock_guard lock(state->mutex);
            if(state->stopping||state->late.size()>=8||state->next_id==std::numeric_limits<std::uint64_t>::max()||
                (state->pending&&WaitForSingleObject(state->pending->done,0)!=WAIT_OBJECT_0))return {};
            work->id=++state->next_id;
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
        {std::lock_guard lock(state->mutex);if(state->pending){std::lock_guard job_lock(state->pending->mutex);state->pending->cancelled=true;}}
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
bool WindowWriter::stop(){return executor_->stop();}
bool WindowWriter::has_unconfirmed_native_call()const{
    std::lock_guard lock(executor_->state->mutex);const auto work=executor_->state->pending;if(!work)return false;
    std::lock_guard job_lock(work->mutex);return work->attempted&&!work->known;
}
void WindowWriter::notify_on_completion(HWND owner){executor_->state->notify=owner;}
std::vector<WindowWriter::Receipt> WindowWriter::take_delayed_completions(const core::v1::Settings& settings){
    std::vector<std::shared_ptr<NativeExecutor::Work>> jobs;
    {std::lock_guard lock(executor_->state->mutex);jobs.swap(executor_->state->late);executor_->state->late.reserve(8);}
    std::vector<Receipt> result;result.reserve(jobs.size());
    for(const auto& job:jobs){
        Receipt r;r.before=job->expected;r.requested=job->requested;
        {std::lock_guard lock(job->mutex);r.call_id=job->id;r.native_attempted=job->attempted;r.native_succeeded=job->succeeded;
            r.completion_known=job->known;r.before_qpc=job->before;r.after_qpc=job->after;r.error=job->error;}
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
    else if (!result.geometry_exact) result.reason = "native_result_not_exact_or_app_constrained";
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
