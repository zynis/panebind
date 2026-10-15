#pragma once

// Disposable-guest fixture support only, not a product observer or timer loop.
// Call once after actual Release UI/matrix checks, with the retained exact
// product process HANDLE. This neither creates a process nor sends input.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace panebind::platform::windows::product::guest_test {

struct IdleCounters {
    bool valid{};
    std::uint64_t process_birth{},kernel_100ns{},user_100ns{},qpc{};
    std::uint64_t working_bytes{},private_commit_bytes{},lifetime_peak_working_bytes{};
    DWORD handles{},gdi{},user{};
};
// One logical-core utilization: do NOT divide by logical processor count or
// clamp at 100%. Summed thread CPU may legitimately exceed one core.
inline constexpr std::optional<double> idle_one_core_percent(
    std::uint64_t kernel_before,std::uint64_t user_before,
    std::uint64_t kernel_after,std::uint64_t user_after,double seconds){
    if(!(seconds>0)||seconds>std::numeric_limits<double>::max()||
       kernel_after<kernel_before||user_after<user_before)return {};
    return (static_cast<double>(kernel_after-kernel_before)+static_cast<double>(user_after-user_before))/100000.0/seconds;
}
struct IdleMeasurement {
    bool completed{};
    DWORD pid{},session{},win32_error{};
    std::uint64_t expected_birth{},qpc_frequency{};
    IdleCounters before,after;
    double elapsed_seconds{},one_core_cpu_percent{};
    std::string reason{"NOT_RUN"};
    [[nodiscard]] std::string json_fields()const{
        std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(10);
        auto sample=[&](const IdleCounters& value){
            out<<"{\"valid\":"<<(value.valid?"true":"false")<<",\"process_birth\":"<<value.process_birth
               <<",\"kernel_100ns\":"<<value.kernel_100ns<<",\"user_100ns\":"<<value.user_100ns<<",\"qpc\":"<<value.qpc
               <<",\"working_bytes\":"<<value.working_bytes<<",\"private_commit_bytes\":"<<value.private_commit_bytes
               <<",\"lifetime_peak_working_bytes\":"<<value.lifetime_peak_working_bytes
               <<",\"handles\":"<<value.handles<<",\"gdi\":"<<value.gdi<<",\"user\":"<<value.user<<'}';};
        out<<"\"completed\":"<<(completed?"true":"false")<<",\"reason\":\""<<reason<<"\",\"win32_error\":"<<win32_error
           <<",\"pid\":"<<pid<<",\"session\":"<<session<<",\"expected_birth\":"<<expected_birth
           <<",\"requested_idle_seconds\":60,\"elapsed_seconds\":"<<elapsed_seconds
           <<",\"qpc_frequency\":"<<qpc_frequency<<",\"one_logical_core_cpu_percent\":"<<one_core_cpu_percent<<",\"before\":";
        sample(before);out<<",\"after\":";sample(after);
        out<<",\"private_means_committed_not_resident\":true,\"peak_means_process_lifetime\":true"
              ",\"function_pass_inferred\":false,\"fifteen_minute_I11_pass_inferred\":false";
        return out.str();
    }
};
namespace idle_detail {
struct Handle {
    HANDLE value{};
    ~Handle(){if(value)CloseHandle(value);}
    Handle()=default;Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
inline std::uint64_t ticks(FILETIME t){return(static_cast<std::uint64_t>(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
inline bool capture(HANDLE process,IdleCounters& result,IdleMeasurement& report){
    auto fail=[&](const char* api){report.reason=api;report.win32_error=GetLastError();return false;};
    if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT){report.reason="product_not_running_at_sample";return false;}
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return fail("GetProcessTimes_failed");
    LARGE_INTEGER clock{};if(!QueryPerformanceCounter(&clock)||clock.QuadPart<=0)return fail("QueryPerformanceCounter_failed");
    result.process_birth=ticks(created);result.kernel_100ns=ticks(kernel);result.user_100ns=ticks(user);result.qpc=static_cast<std::uint64_t>(clock.QuadPart);
    if(result.process_birth!=report.expected_birth){report.reason="product_birth_mismatch";return false;}
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    if(!GetProcessMemoryInfo(process,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)))return fail("GetProcessMemoryInfo_failed");
    result.working_bytes=memory.WorkingSetSize;result.private_commit_bytes=memory.PrivateUsage;result.lifetime_peak_working_bytes=memory.PeakWorkingSetSize;
    if(!GetProcessHandleCount(process,&result.handles))return fail("GetProcessHandleCount_failed");
    SetLastError(ERROR_SUCCESS);result.gdi=GetGuiResources(process,GR_GDIOBJECTS);
    if(!result.gdi&&GetLastError()!=ERROR_SUCCESS)return fail("GetGuiResources_GDI_failed");
    SetLastError(ERROR_SUCCESS);result.user=GetGuiResources(process,GR_USEROBJECTS);
    if(!result.user&&GetLastError()!=ERROR_SUCCESS)return fail("GetGuiResources_USER_failed");
    if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT){report.reason="product_exited_during_sample";return false;}
    result.valid=true;return true;
}
} // namespace idle_detail

// The caller's existing guest guard establishes WDAG/RunId/Medium/Release and
// idle UI state. No PID lookup or re-open can silently target another process.
// A duplicate of the retained HANDLE stays valid even if another owner closes
// its original. A supplied cancellation event is also duplicated for the wait.
inline IdleMeasurement measure_idle_60s(HANDLE exact_product,DWORD expected_pid,
    std::uint64_t expected_birth,HANDLE cancellation_event=nullptr){
    IdleMeasurement report;report.pid=expected_pid;report.expected_birth=expected_birth;
    auto fail=[&](const char* reason,DWORD error=ERROR_SUCCESS){report.reason=reason;report.win32_error=error;return report;};
    if(!exact_product||exact_product==INVALID_HANDLE_VALUE||!expected_pid||expected_pid==GetCurrentProcessId()||!expected_birth)
        return fail("exact_product_handle_required");
    idle_detail::Handle process,cancel;
    if(!DuplicateHandle(GetCurrentProcess(),exact_product,GetCurrentProcess(),&process.value,0,FALSE,DUPLICATE_SAME_ACCESS))
        return fail("Duplicate_product_handle_failed",GetLastError());
    if(GetProcessId(process.value)!=expected_pid)return fail("product_pid_mismatch",GetLastError());
    DWORD own_session{};
    if(!ProcessIdToSessionId(expected_pid,&report.session)||!ProcessIdToSessionId(GetCurrentProcessId(),&own_session))
        return fail("process_session_unavailable",GetLastError());
    if(!report.session||report.session!=own_session)return fail("product_session_mismatch");
    if(cancellation_event&&!DuplicateHandle(GetCurrentProcess(),cancellation_event,GetCurrentProcess(),&cancel.value,0,FALSE,DUPLICATE_SAME_ACCESS))
        return fail("Duplicate_cancel_event_failed",GetLastError());
    LARGE_INTEGER frequency{};
    if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0||frequency.QuadPart>std::numeric_limits<LONGLONG>::max()/61)
        return fail("QPC_frequency_invalid",GetLastError());
    report.qpc_frequency=static_cast<std::uint64_t>(frequency.QuadPart);
    if(!idle_detail::capture(process.value,report.before,report))return report;
    if(report.before.qpc>std::numeric_limits<std::uint64_t>::max()-report.qpc_frequency*60)return fail("QPC_deadline_overflow");
    const auto deadline=report.before.qpc+report.qpc_frequency*60;
    const HANDLE waits[]{process.value,cancel.value};const DWORD count=cancel.value?2u:1u;
    for(;;){
        LARGE_INTEGER now{};if(!QueryPerformanceCounter(&now)||now.QuadPart<=0)return fail("QPC_wait_failed",GetLastError());
        const auto current=static_cast<std::uint64_t>(now.QuadPart);
        if(current<report.before.qpc)return fail("QPC_clock_reversed");
        if(current>=deadline)break;
        const auto milliseconds=std::ceil(static_cast<double>(deadline-current)*1000.0/static_cast<double>(report.qpc_frequency));
        const auto timeout=static_cast<DWORD>(milliseconds>60000?60000:milliseconds<1?1:milliseconds);
        const DWORD waited=MsgWaitForMultipleObjectsEx(count,waits,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(waited==WAIT_OBJECT_0)return fail("product_exited_during_idle");
        if(cancel.value&&waited==WAIT_OBJECT_0+1)return fail("idle_cancelled");
        if(waited==WAIT_FAILED)return fail("idle_wait_failed",GetLastError());
        if(waited==WAIT_OBJECT_0+count){
            MSG message{};unsigned budget=256;
            while(budget--&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return fail("driver_quit_during_idle");}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
        }else if(waited!=WAIT_TIMEOUT)return fail("unexpected_idle_wait_result",waited);
    }
    if(cancel.value&&WaitForSingleObject(cancel.value,0)==WAIT_OBJECT_0)return fail("idle_cancelled_at_deadline");
    if(!idle_detail::capture(process.value,report.after,report))return report;
    if(report.after.qpc<report.before.qpc)return fail("QPC_sample_reversed");
    report.elapsed_seconds=static_cast<double>(report.after.qpc-report.before.qpc)/static_cast<double>(report.qpc_frequency);
    const auto cpu=idle_one_core_percent(report.before.kernel_100ns,report.before.user_100ns,
        report.after.kernel_100ns,report.after.user_100ns,report.elapsed_seconds);
    if(!cpu||report.elapsed_seconds<60)return fail("invalid_idle_counter_delta");
    report.one_core_cpu_percent=*cpu;report.completed=true;report.reason="MEASURED_NOT_FUNCTION_ACCEPTANCE";return report;
}

static_assert(idle_one_core_percent(0,0,6000000,0,60)==1.0);
static_assert(idle_one_core_percent(10,20,10,20,60)==0.0);
static_assert(idle_one_core_percent(0,0,1200000000,0,60)==200.0);
static_assert(!idle_one_core_percent(20,10,19,10,60));
static_assert(!idle_one_core_percent(0,0,1,1,0));

} // namespace panebind::platform::windows::product::guest_test
