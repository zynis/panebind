#include "platform/windows/product/guest_path_scope.h"
#include "platform/windows/product/guest_capture_scope.h"
#include "platform/windows/product/v1_guest_up_evidence.h"
#include "platform/windows/product/v1_guest_native_result.h"
#include "platform/windows/product/v1_guest_actor_capture.h"
#include "platform/windows/product/v1_guest_product_log.h"
#include "platform/windows/product/v1_guest_physical_log.h"
#include "platform/windows/product/v1_guest_deadline.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    using panebind::platform::windows::product::guest::same_exact_directory;
    struct Case { const wchar_t* actual; const wchar_t* expected; bool equal; };
    const Case cases[]{
        {LR"(C:\PaneBindV1\Output\)", LR"(C:\PaneBindV1\Output)", true},
        {LR"(C:\PaneBindV1\Output)", LR"(C:\PaneBindV1\Output\)", true},
        {LR"(c:\PANEBINDV1\output\)", LR"(C:\PaneBindV1\Output)", true},
        {LR"(C:\PaneBindV1\OutputOther)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(C:\PaneBindV1\)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(C:\PaneBindV1\Output\child)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(D:\PaneBindV1\Output)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(PaneBindV1\Output)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(C:PaneBindV1\Output)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(C:\)", LR"(C:\)", true},
        {LR"(C:)", LR"(C:\)", false},
        {LR"(\\server\share\PaneBindV1\Output)", LR"(C:\PaneBindV1\Output)", false},
        {LR"(C:\PaneBindV1\Output\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-product-Debug\)",
         LR"(C:\PaneBindV1\Output\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-product-Debug)", true},
        {LR"(C:\PaneBindV1\Output\bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb-product-Debug\)",
         LR"(C:\PaneBindV1\Output\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-product-Debug)", false},
        {LR"(C:\PaneBindV1\Output\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-product-Release)",
         LR"(C:\PaneBindV1\Output\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-product-Debug)", false},
        {LR"(C:\PaneBindV1\Output\..\Output)", LR"(C:\PaneBindV1\Output)", false},
    };
    unsigned checked{};
    for (const auto& test : cases) {
        ++checked;
        if (same_exact_directory(test.actual, test.expected) != test.equal) {
            std::cerr << "guest exact-directory comparison failed case " << checked << '\n';
            return 1;
        }
    }
    namespace guest=panebind::platform::windows::product::guest;
    const guest::UpEvidenceIdentity identity{42,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",7,1000};
    const std::string status=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","sequence":9,"process_id":42,"tick":900,"type":"status","gesture_active":false,"mouse_up_callbacks":{"snapshot_known":true,"callback_count":1,"last":{"hook_observed_qpc":1001,"button":1,"generation_at_entry":7,"consumed_before":true,"aborted_before":false,"matching_branch_taken":true,"push_attempted":true,"push_succeeded":true}}})";
    const auto changed=[&](std::string_view from,std::string_view to){auto row=status;const auto at=row.find(from);
        if(at==row.npos){std::cerr<<"UP test mutation not present\n";std::exit(2);}row.replace(at,from.size(),to);return row;};
    const auto expect=[&](bool actual,bool wanted,const char* name){++checked;if(actual!=wanted){std::cerr<<"guest UP evidence case failed: "<<name<<'\n';return false;}return true;};
    const guest::NativeClientCompletionFacts native_complete{true,true,true,true,true,true,true};
    const auto native_result=guest::native_client_completion(native_complete);
    if(!expect(native_result.client==guest::NativeClientVerdict::Pass&&
            native_result.normal_close==guest::NativeClientVerdict::Pass&&native_result.overall==guest::NativeClientVerdict::Pass,true,
            "native overall PASS requires real client and exact normal-close completion"))return 1;
    auto native_incomplete=native_complete;native_incomplete.normal_close_confirmed=false;
    const auto native_close_failed=guest::native_client_completion(native_incomplete);
    if(!expect(native_close_failed.client==guest::NativeClientVerdict::Pass&&
            native_close_failed.normal_close==guest::NativeClientVerdict::Fail&&native_close_failed.overall==guest::NativeClientVerdict::Fail,true,
            "client success cannot hide unconfirmed native root close"))return 1;
    native_incomplete=native_complete;native_incomplete.source_idle=false;
    if(!expect(guest::native_client_completion(native_incomplete).overall==guest::NativeClientVerdict::Fail,true,
            "native source GUI idle UNKNOWN cannot pass overall"))return 1;
    native_incomplete=native_complete;native_incomplete.cleanup_error_free=false;
    if(!expect(guest::native_client_completion(native_incomplete).overall==guest::NativeClientVerdict::Fail,true,
            "native cleanup failure cannot pass overall"))return 1;
    native_incomplete=native_complete;native_incomplete.input_clear=false;
    if(!expect(guest::native_client_completion(native_incomplete).overall==guest::NativeClientVerdict::Fail,true,
            "native pending input cannot pass overall"))return 1;
    native_incomplete=native_complete;native_incomplete.client_passed=false;
    if(!expect(guest::native_client_completion(native_incomplete).overall==guest::NativeClientVerdict::Fail,true,
            "normal close alone cannot turn failed client operation into PASS"))return 1;
    native_incomplete=native_complete;native_incomplete.client_attempted=false;native_incomplete.client_passed=false;
    const auto native_not_started=guest::native_client_completion(native_incomplete);
    if(!expect(native_not_started.client==guest::NativeClientVerdict::NotRun&&native_not_started.overall==guest::NativeClientVerdict::Fail,true,
            "initialization gate refusal keeps client NOT_RUN and cannot award overall PASS"))return 1;
    native_incomplete=native_complete;native_incomplete.root_bound=false;native_incomplete.client_passed=false;
    const auto native_unbound=guest::native_client_completion(native_incomplete);
    if(!expect(native_unbound.client==guest::NativeClientVerdict::Blocked&&
            native_unbound.normal_close==guest::NativeClientVerdict::NotRun&&native_unbound.overall==guest::NativeClientVerdict::Blocked,true,
            "unknown native binding is BLOCKED and close NOT_RUN, not fabricated startup or close failure"))return 1;
    native_incomplete.client_passed=true;
    if(!expect(guest::native_client_completion(native_incomplete).overall!=guest::NativeClientVerdict::Pass,true,
            "a success flag without exact root binding cannot pass native completion"))return 1;
    namespace deadline=panebind::platform::windows::product::guest_test;
    constexpr std::uint64_t qpc_frequency=10000000,target_qpc=900*qpc_frequency;
    const auto reached=deadline::deadline_after_signal(target_qpc,target_qpc,qpc_frequency,false);
    const auto late=deadline::deadline_after_signal(target_qpc,target_qpc+1,qpc_frequency,false);
    const auto early=deadline::deadline_after_signal(target_qpc,target_qpc-74600,qpc_frequency,false);
    const auto too_early=deadline::deadline_after_signal(target_qpc,target_qpc-330000,qpc_frequency,false);
    const auto second_early=deadline::deadline_after_signal(target_qpc,target_qpc-1000,qpc_frequency,true);
    if(!expect(reached.decision==deadline::DeadlineSignal::Reached&&late.decision==deadline::DeadlineSignal::Reached,true,
            "fresh QPC at or after target, not timer signal alone, proves deadline")||
       !expect(early.decision==deadline::DeadlineSignal::RearmOnce&&early.remaining_qpc==74600&&
            early.rearm_due_100ns==394600&&early.rearm_due_100ns<=640000,true,
            "actual 7.46 ms early signal permits only one bounded remaining-plus-margin rearm")||
       !expect(too_early.decision==deadline::DeadlineSignal::TooEarly&&too_early.rearm_due_100ns==0,true,
            "signal over 32 ms early cannot become a 900-second PASS or a rearm")||
       !expect(second_early.decision==deadline::DeadlineSignal::SecondEarly&&second_early.rearm_due_100ns==0,true,
            "second early signal cannot request a third timer arm")||
       !expect(deadline::deadline_after_signal(target_qpc,target_qpc,0,false).decision==deadline::DeadlineSignal::Invalid,true,
            "unknown QPC frequency cannot prove deadline"))return 1;
    using ResourceOwner=guest::ResourceOwnerKind;using ResourceRoute=guest::ResourceRouteKind;
    if(!expect(guest::select_resource_route(ResourceOwner::OrdinaryUser,true,false)==ResourceRoute::ProductionMedium,true,
           "actual ordinary interactive Medium can use production resources without guest artifacts")||
       !expect(guest::select_resource_route(ResourceOwner::OrdinaryUser,false,true)==ResourceRoute::None,true,
           "High or inactive ordinary owner cannot acquire resources even with guest evidence")||
       !expect(guest::select_resource_route(ResourceOwner::ExactSandboxAccount,true,true)==ResourceRoute::ExactGuest,true,
           "interactive Medium sandbox account needs exact guest verification")||
       !expect(guest::select_resource_route(ResourceOwner::ExactSandboxAccount,true,false)==ResourceRoute::None,true,
           "unverified sandbox account cannot fall back to production")||
       !expect(guest::select_resource_route(ResourceOwner::ExactSandboxAccount,false,true)==ResourceRoute::None,true,
           "verified guest artifacts do not override High or inactive environment")||
       !expect(guest::select_resource_route(ResourceOwner::Unknown,true,true)==ResourceRoute::None,true,
           "unknown owner never receives resource authority"))return 1;
    // Exact old failure shape: the valid prefix was below the whole-file
    // read cap; the completed finite phase crossed it. The actual production
    // reader must preserve that phase's original verdict, not return empty.
    const std::string driver_prefix(1900000,'p');
    const std::string driver_phase=std::string(300000,'q')+"\n{\"result\":\"FAIL\"}\n";
    std::istringstream driver_stream(driver_prefix);
    const auto driver_mark=guest::driver_log_mark(driver_stream);
    driver_stream.str(driver_prefix+driver_phase);driver_stream.clear();
    if(!expect(guest::driver_log_suffix(driver_stream,driver_mark)==driver_phase,true,"a bounded phase suffix survives a whole driver log over 2 MiB"))return 1;
    driver_stream.clear();const auto driver_end=guest::driver_log_mark(driver_stream);
    if(!expect(guest::driver_log_suffix(driver_stream,driver_end).empty(),true,"an actually empty suffix stays empty"))return 1;
    const auto suffix_rejected=[&](std::uint64_t mark,std::size_t cap){driver_stream.clear();try{(void)guest::driver_log_suffix(driver_stream,mark,cap);return false;}catch(const std::runtime_error&){return true;}};
    if(!expect(suffix_rejected(driver_end+1,2*1024*1024),true,"truncated or invalid marks fail rather than erase old evidence")||
       !expect(suffix_rejected(driver_mark,100),true,"requested suffix keeps its own fixed bound"))return 1;
    if(!expect(guest::product_log_metadata_pair_retry_allowed(0,true,{2,std::system_category()},false),true,"plain file then actual Windows file-not-found may recollect one whole pair")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,true,{3,std::system_category()},false),true,"plain file then actual Windows path-not-found uses the same bounded opportunity")||
       !expect(guest::product_log_metadata_pair_retry_allowed(1,true,{2,std::system_category()},false),false,"persistent metadata not-found cannot gain a third read")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,false,{2,std::system_category()},false),false,"unknown directory or reparse attributes cannot authorize retry")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,true,{5,std::system_category()},false),false,"access denied is not a rotation exception")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,true,{2,std::generic_category()},false),false,"an unrelated errno value cannot impersonate a Windows rotation error")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,true,{2,std::system_category()},true),false,"oversized evidence stays rejected even with not-found code")||
       !expect(guest::product_log_metadata_pair_retry_allowed(0,true,{},true),false,"a confirmed oversize is never retried as missing metadata"))return 1;
    const auto log_row=[](std::uint64_t pid,std::uint64_t sequence,std::uint64_t tick,std::string_view type,std::string_view sha="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"){
        return "{\"schema\":\"panebind-v1/1\",\"source_sha\":\""+std::string(sha)+"\",\"sequence\":"+std::to_string(sequence)+
            ",\"process_id\":"+std::to_string(pid)+",\"tick\":"+std::to_string(tick)+",\"type\":\""+std::string(type)+"\"}\n";};
    guest::ProductLogEvidence log;std::string log_reason;log.begin(42,4200,100,std::string(identity.source_sha));
    const auto one=log_row(42,1,100,"input_preparation"),two=log_row(42,2,101,"capture_ready"),three=log_row(42,3,102,"gesture_start");
    if(!expect(log.observe("",one,log_reason),true,"actual complete first product record is accepted"))return 1;
    const auto before_rotation=log.text().size();
    if(!expect(log.observe(one+two,three,log_reason),true,"READY in rotated file and START in new current remain one continuous stream")||
       !expect(log.since(before_rotation)&&*log.since(before_rotation)==two+three,true,"pre-rotation logical byte mark reaches actual READY and START")||
       !expect(log.observe(one+two,three,log_reason)&&log.text()==one+two+three,true,"unchanged physical pair never duplicates real receipts"))return 1;
    const auto four=log_row(42,4,103,"status"),prefix=four.substr(0,four.size()-1);
    if(!expect(log.observe(two+three,prefix,log_reason)&&log.text()==one+two+three,true,"incomplete trailing JSON is not a receipt")||
       !expect(log.observe(two+three,four,log_reason)&&log.text()==one+two+three+four,true,"completed tail appends once after its actual newline"))return 1;
    const auto five=log_row(42,5,104,"legacy_left_up"),six=log_row(42,6,105,"isolation_gone");
    if(!expect(log.observe(three+four,six,log_reason),false,"missing sequence rejects an incomplete rotation snapshot")||
       !expect(log.text()==one+two+three+four,true,"rejected snapshot never commits partial evidence")||
       !expect(log.observe(four+five,six,log_reason),true,"one corrected physical snapshot can resolve a concurrent rotation gap")||
       !expect(log.observe(log_row(42,5,104,"different_original"),six,log_reason),false,"duplicate sequence with changed original text is rejected"))return 1;
    const auto old_mark=before_rotation,new_start_mark=log.text().size();log.begin(43,4300,200,std::string(identity.source_sha));
    const auto restarted=log_row(43,1,200,"startup");
    if(!expect(log.observe(one+two+three+four+five+six,restarted,log_reason),true,"new actual PID birth starts sequence one without accepting old PID rows")||
       !expect(log.current()==restarted&&log.since(new_start_mark)&&*log.since(new_start_mark)==restarted,true,"pre-launch tail mark observes only the new actual launch")||
       !expect(log.since(old_mark).has_value(),false,"a prior gesture's byte mark cannot cross a process restart"))return 1;
    log.begin(42,4201,300,std::string(identity.source_sha));const auto reused=log_row(42,1,300,"startup");
    if(!expect(log.observe(one+two+three,reused,log_reason)&&log.current()==reused,true,"reused PID cannot import records older than its actual launch boundary")||
       !expect(log.observe("",log_row(42,2,301,"status","bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),log_reason),false,"current launch source SHA mismatch is rejected"))return 1;
    guest::ProductLogEvidence missing;missing.begin(42,4200,100,std::string(identity.source_sha));
    if(!expect(missing.observe("",two,log_reason),false,"missing initial segment cannot masquerade as complete product evidence"))return 1;
    missing.reject();
    if(!expect(missing.observe("",one+two,log_reason),false,"persistent rejected evidence cannot silently regain acceptance"))return 1;
    guest::ProductLogEvidence hotkey_log;hotkey_log.begin(42,4200,100,std::string(identity.source_sha));
    const std::string paused_end=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","sequence":1,"process_id":42,"tick":100,"type":"gesture_end","generation":7,"end":4,"normal_up":false,"pending_retired":true})" "\n";
    const std::string hotkey_row=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","sequence":2,"process_id":42,"tick":101,"type":"emergency_hotkey","native_message":"WM_HOTKEY","registration_id":910,"received_count":1})";
    if(!expect(hotkey_log.observe("",paused_end,log_reason),true,"actual Paused END is a validated product prefix"))return 1;
    const auto hotkey_mark=hotkey_log.text().size();const auto only_end=hotkey_log.since(hotkey_mark);
    if(!expect(only_end&&only_end->empty()&&!guest::matching_emergency_hotkey_receipt(*only_end,42,identity.source_sha),true,
            "Paused END alone cannot award later GUI WM_HOTKEY receipt"))return 1;
    if(!expect(hotkey_log.observe(paused_end,hotkey_row+"\n",log_reason),true,
            "actual later WM_HOTKEY row appends after the already validated END"))return 1;
    const auto hotkey_suffix=hotkey_log.since(hotkey_mark);
    const auto actual_hotkey=hotkey_suffix&&hotkey_suffix->ends_with('\n')?
        hotkey_suffix->substr(0,hotkey_suffix->size()-1):std::string_view{};
    if(!expect(hotkey_suffix&&*hotkey_suffix==hotkey_row+"\n"&&
            guest::matching_emergency_hotkey_receipt(actual_hotkey,42,identity.source_sha),true,
            "one real current-product GUI WM_HOTKEY receipt is distinct from its earlier Paused END"))return 1;
    auto wrong_hotkey=hotkey_row;
    wrong_hotkey.replace(wrong_hotkey.find("\"registration_id\":910"),21,"\"registration_id\":911");
    if(!expect(!guest::matching_emergency_hotkey_receipt(wrong_hotkey,42,identity.source_sha),true,
            "different registered hotkey cannot satisfy I08")||
       !expect(!guest::matching_emergency_hotkey_receipt(hotkey_row,43,identity.source_sha),true,
            "other product PID cannot satisfy I08")||
       !expect(!guest::matching_emergency_hotkey_receipt(hotkey_row,42,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),true,
            "other product SHA cannot satisfy I08"))return 1;
    // Real Win32 file handles, in this test's uniquely created build-working
    // directory only. No test opens or deletes a user/product log.
    namespace fs=std::filesystem;
    LARGE_INTEGER unique{};QueryPerformanceCounter(&unique);
    const auto physical_dir=fs::current_path()/("panebind-v1-physical-log-"+std::to_string(GetCurrentProcessId())+"-"+
        std::to_string(GetCurrentThreadId())+"-"+std::to_string(unique.QuadPart));
    if(!expect(fs::create_directory(physical_dir),true,"own unique physical-log test directory created in build cwd"))return 1;
    struct OwnPhysicalFiles {
        fs::path directory;
        ~OwnPhysicalFiles(){
            std::error_code ignored;
            for(const auto name:{L"current.jsonl",L"previous.jsonl",L"oversize.jsonl",L"locked.jsonl",L"link.jsonl"})
                fs::remove(directory/name,ignored);
            fs::remove(directory,ignored);
        }
    } own_physical{physical_dir};
    const auto write_physical=[&](const fs::path& path,std::string_view bytes){
        const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        DWORD written{};const bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=FALSE&&written==bytes.size();
        CloseHandle(file);return ok;
    };
    const auto current_path=physical_dir/L"current.jsonl",previous_path=physical_dir/L"previous.jsonl";
    if(!expect(write_physical(current_path,"old\n"),true,"own old product-log physical bytes created"))return 1;
    {
        guest::ProductLogPhysicalFile old(current_path);
        if(!expect(old.opened_plain_file(),true,"one old plain file handle opened with delete sharing")||
           !expect(MoveFileExW(current_path.c_str(),previous_path.c_str(),0)!=FALSE,true,
               "own old log can rename while the reader holds its share-delete handle")||
           !expect(write_physical(current_path,"new\n"),true,"replacement current log is a distinct own file"))return 1;
        guest::ProductLogPhysicalFile now(current_path);
        const auto before_delete=old.snapshot(),current=now.snapshot();
        if(!expect(before_delete.state==guest::ProductLogPhysicalState::Ready&&before_delete.bytes=="old\n"&&
                   current.state==guest::ProductLogPhysicalState::Ready&&current.bytes=="new\n",true,
                   "renamed old handle and new path return independent bounded bytes")||
           !expect(DeleteFileW(previous_path.c_str())!=FALSE,true,
                   "own old log can be delete-pending while its read handle remains open"))return 1;
        const auto after_delete=old.snapshot();
        if(!expect(after_delete.state==guest::ProductLogPhysicalState::Ready&&after_delete.bytes=="old\n",true,
                   "same held handle reads original bytes after name deletion"))return 1;
    }
    guest::ProductLogPhysicalFile absent(previous_path);
    if(!expect(absent.snapshot().state==guest::ProductLogPhysicalState::Missing,true,
            "confirmed absent path is empty evidence, unlike access denied"))return 1;
    const auto big_path=physical_dir/L"oversize.jsonl";
    {const HANDLE big=CreateFileW(big_path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
          nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(!expect(big!=INVALID_HANDLE_VALUE,true,"own oversized test log created"))return 1;
        LARGE_INTEGER end{};end.QuadPart=2*1024*1024+1;
        const bool sized=SetFilePointerEx(big,end,nullptr,FILE_BEGIN)!=FALSE&&SetEndOfFile(big)!=FALSE;
        CloseHandle(big);if(!expect(sized,true,"own oversized log length established"))return 1;}
    guest::ProductLogPhysicalFile big(big_path);const auto too_big=big.snapshot();
    if(!expect(too_big.state==guest::ProductLogPhysicalState::Failure&&too_big.actual_size_known&&
               too_big.actual_size==2*1024*1024+1&&std::string_view(too_big.stage)=="size_limit",true,
               "same-handle size cap rejects before unbounded read"))return 1;
    guest::ProductLogPhysicalFile directory(physical_dir);
    if(!expect(directory.snapshot().state==guest::ProductLogPhysicalState::Failure&&
               std::string_view(directory.snapshot().stage)=="non_plain_handle_attributes",true,
               "actual directory handle is not accepted as a plain log"))return 1;
    const auto locked_path=physical_dir/L"locked.jsonl";
    {const HANDLE locked=CreateFileW(locked_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(!expect(locked!=INVALID_HANDLE_VALUE,true,"own exclusive-share test log created"))return 1;
        guest::ProductLogPhysicalFile denied(locked_path);const auto refusal=denied.snapshot();
        CloseHandle(locked);
        if(!expect(refusal.state==guest::ProductLogPhysicalState::Failure&&refusal.win32_error==ERROR_SHARING_VIOLATION&&
                   std::string_view(refusal.stage)=="open",true,
                   "sharing failure cannot be represented as missing or empty product evidence"))return 1;}
    const auto link_path=physical_dir/L"link.jsonl";
    if(CreateSymbolicLinkW(link_path.c_str(),current_path.c_str(),0)){
        guest::ProductLogPhysicalFile link(link_path);const auto refusal=link.snapshot();
        if(!expect(refusal.state==guest::ProductLogPhysicalState::Failure&&
                   std::string_view(refusal.stage)=="non_plain_handle_attributes",true,
                   "reparse link handle is rejected without reading its target"))return 1;
    }else{const auto error=GetLastError();
        if(error==ERROR_PRIVILEGE_NOT_HELD||error==ERROR_ACCESS_DENIED)
            std::cout<<"guest physical-log reparse case SKIP: symlink creation win32_error="<<error<<'\n';
        else if(!expect(false,true,"unexpected own reparse-test creation failure"))return 1;
    }
    const auto snapshot=[&](std::string_view row){return guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,row,identity);};
    guest::CapturedReleaseFacts capture;
    capture.generation=7;capture.expected_generation=7;capture.source=100;capture.ready_source=100;
    capture.source_pid=10;capture.ready_source_pid=10;capture.source_tid=11;capture.ready_source_tid=11;
    capture.overlay=200;capture.ready_actual_capture=200;capture.hit_root=200;capture.overlay_pid=42;capture.product_pid=42;
    capture.overlay_tid=12;capture.source_capture=200;capture.owner_capture=200;
    capture.product_identity=capture.ready_identity=capture.ready_attempt_succeeded=capture.source_gui=
        capture.owner_gui=capture.gui_idle_except_capture=capture.not_retired=true;
    if(!expect(guest::captured_release_hit_allowed(capture),true,"exact current product shield actual hit and both actual captures"))return 1;
    auto rejected=capture;rejected.expected_generation=8;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"old generation capture cannot authorize planned release"))return 1;
    rejected=capture;rejected.owner_capture=201;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"foreign actual owner capture rejects"))return 1;
    rejected=capture;rejected.overlay_pid=43;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"other process overlay rejects even if class matches"))return 1;
    rejected=capture;rejected.ready_source_tid=12;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"changed source thread rejects"))return 1;
    rejected=capture;rejected.not_retired=false;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"retired READY receipt never reauthorizes release target"))return 1;
    rejected=capture;rejected.hit_root=201;
    if(!expect(guest::captured_release_hit_allowed(rejected),false,"unknown hit root rejects"))return 1;
    auto source_spatial=capture;source_spatial.hit_root=source_spatial.source;
    if(!expect(guest::captured_release_route_allowed(source_spatial),true,"exact source spatial hit with both actual current shield captures permits one test release")||
       !expect(guest::captured_release_hit_allowed(source_spatial),false,"source spatial hit is not relabelled as a shield hit"))return 1;
    rejected=source_spatial;rejected.owner_capture=201;
    if(!expect(guest::captured_release_route_allowed(rejected),false,"source spatial hit does not waive foreign owner capture"))return 1;
    rejected=source_spatial;rejected.expected_generation=8;
    if(!expect(guest::captured_release_route_allowed(rejected),false,"source spatial hit does not revive an old generation"))return 1;
    rejected=source_spatial;rejected.hit_root=201;
    if(!expect(guest::captured_release_route_allowed(rejected),false,"foreign spatial hit remains rejected even while own capture is held"))return 1;
    guest::ActorStartCaptureReceipt actor;actor.version=2;actor.parent_pid=10;actor.parent_birth=101;
    actor.child_pid=20;actor.child_birth=201;actor.product_birth=421;actor.down_qpc=1000;actor.ready_qpc=1001;actor.capture=source_spatial;
    actor.product_image_sha256.fill('A');actor.product_image_sha256[64]='\0';
    actor.no_up_or_end_observed_before_ack=true;
    const auto actor_match=[&](const guest::ActorStartCaptureReceipt& actual){return guest::actor_start_capture_receipt_matches(actual,10,101,20,201);};
    if(!expect(actor_match(actor),true,"one immutable exact parent child START capture receipt is structurally bound"))return 1;
    auto old_actor=actor;old_actor.child_birth=202;
    if(!expect(actor_match(old_actor),false,"reused child PID cannot inherit the one-shot capture receipt"))return 1;
    old_actor=actor;old_actor.capture.expected_generation=8;
    if(!expect(actor_match(old_actor),false,"actor receipt rejects mismatched START READY generation"))return 1;
    old_actor=actor;old_actor.ready_qpc=999;
    if(!expect(actor_match(old_actor),false,"actor READY must not precede its actual DOWN receipt"))return 1;
    old_actor=actor;old_actor.capture.source_capture=201;
    if(!expect(actor_match(old_actor),false,"actor START ack cannot waive actual foreign capture"))return 1;
    old_actor=actor;old_actor.product_image_sha256[0]='\0';
    if(!expect(actor_match(old_actor),false,"incomplete product hash cannot bind an actor capture receipt"))return 1;
    old_actor=actor;old_actor.no_up_or_end_observed_before_ack=false;
    if(!expect(actor_match(old_actor),false,"missing actual parent no-UP no-END fact refuses a MOVE acknowledgement"))return 1;
    guest::ActorNativeThresholdCommand threshold;threshold.version=1;threshold.parent_pid=10;threshold.parent_birth=101;
    threshold.child_pid=20;threshold.child_birth=201;threshold.product_pid=42;threshold.product_birth=421;
    threshold.source=100;threshold.source_pid=10;threshold.source_tid=11;threshold.generation=7;threshold.source_key=81;threshold.down_qpc=1000;
    threshold.anchor_x=120;threshold.anchor_y=80;threshold.destination_x=125;threshold.destination_y=80;threshold.drag_x=4;threshold.drag_y=4;
    threshold.original_visible={100,60,500,420};threshold.product_image_sha256=actor.product_image_sha256;threshold.no_up_start_or_end_observed=true;
    const auto threshold_match=[&](const guest::ActorNativeThresholdCommand& actual){return guest::actor_native_threshold_command_matches(actual,10,101,20,201,100,11,120,80,999);};
    if(!expect(threshold_match(threshold),true,"actual pending exact parent child source authorizes only a threshold TEST command, not START"))return 1;
    auto invalid_threshold=threshold;invalid_threshold.child_birth=202;
    if(!expect(threshold_match(invalid_threshold),false,"threshold command rejects recycled child"))return 1;
    invalid_threshold=threshold;invalid_threshold.source_pid=42;
    if(!expect(threshold_match(invalid_threshold),false,"threshold command cannot adopt a non-parent source"))return 1;
    invalid_threshold=threshold;invalid_threshold.down_qpc=998;
    if(!expect(threshold_match(invalid_threshold),false,"threshold command rejects DOWN older than this child's single insertion"))return 1;
    invalid_threshold=threshold;invalid_threshold.no_up_start_or_end_observed=false;
    if(!expect(threshold_match(invalid_threshold),false,"retired or started generation cannot request threshold movement"))return 1;
    invalid_threshold=threshold;invalid_threshold.destination_x=126;
    if(!expect(threshold_match(invalid_threshold),false,"threshold command cannot widen the one system metric displacement"))return 1;
    invalid_threshold=threshold;invalid_threshold.generation=0;
    if(!expect(threshold_match(invalid_threshold),false,"threshold command requires actual generation"))return 1;
    invalid_threshold=threshold;invalid_threshold.product_image_sha256[0]='\0';
    if(!expect(threshold_match(invalid_threshold),false,"threshold command requires exact product hash"))return 1;
    invalid_threshold=threshold;invalid_threshold.anchor_x=std::numeric_limits<std::int64_t>::min();invalid_threshold.destination_x=std::numeric_limits<std::int64_t>::max();
    if(!expect(threshold_match(invalid_threshold),false,"malformed threshold coordinates reject before subtraction"))return 1;
    const std::string native_begin=R"({"process_id":42,"source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","generation":7,"type":"gesture_start","native_first_handoff":true,"original":[100,60,500,420],"actual_begin_geometry":[105,60,505,420]})";
    const std::string native_handoff=R"({"process_id":42,"source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","generation":7,"type":"native_move_handoff","accepted":true,"stage":"actual_END_capture_pending","intent_anchor":[100,60,500,420],"continuity_baseline":[105,60,505,420]})";
    const auto handoff_geometry=[&](std::string_view begin,std::string_view handoff,const std::array<std::int64_t,4>& actual){return
        guest::actor_native_handoff_geometry_matches(begin,handoff,42,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",7,{100,60,500,420},actual);};
    if(!expect(handoff_geometry(native_begin,native_handoff,{105,60,505,420}),true,"native prefix A to translated B preserves original intent and binds both actual END and START geometry")||
       !expect(handoff_geometry(native_begin,native_handoff,{106,60,506,420}),false,"unattributed current translation cannot replace recorded B")||
       !expect(handoff_geometry(native_begin,native_handoff,{105,60,506,420}),false,"native prefix cannot change source size"))return 1;
    auto changed_handoff=native_handoff;const std::string original_field="\"intent_anchor\":[100,60,500,420]";
    changed_handoff.replace(changed_handoff.find(original_field),original_field.size(),"\"intent_anchor\":[105,60,505,420]");
    if(!expect(handoff_geometry(native_begin,changed_handoff,{105,60,505,420}),false,"handoff cannot re-anchor intent to B"))return 1;
    auto changed_begin=native_begin;const std::string begin_field="\"actual_begin_geometry\":[105,60,505,420]";
    changed_begin.replace(changed_begin.find(begin_field),begin_field.size(),"\"actual_begin_geometry\":[100,60,500,420]");
    if(!expect(handoff_geometry(changed_begin,native_handoff,{105,60,505,420}),false,"actual START geometry must agree with END baseline B"))return 1;
    changed_handoff=native_handoff;changed_handoff.replace(changed_handoff.find("\"generation\":7"),14,"\"generation\":8");
    if(!expect(handoff_geometry(native_begin,changed_handoff,{105,60,505,420}),false,"old generation geometry cannot bind a new actor"))return 1;
    old_actor=actor;old_actor.version=1;
    if(!expect(actor_match(old_actor),false,"old receipt without the no-UP observation contract is rejected")||
       !expect(guest::actor_single_move_may_be_sent(1,0,0,true,true),true,"one synthetic DOWN plus actual START READY and fresh context permits one test MOVE, not a physical-held claim")||
       !expect(guest::actor_single_move_may_be_sent(0,0,0,true,true),false,"an unknown or failed synthetic DOWN cannot issue MOVE")||
       !expect(guest::actor_single_move_may_be_sent(1,1,0,true,true),false,"the one-shot actor cannot replay MOVE")||
       !expect(guest::actor_single_move_may_be_sent(1,0,1,true,true),false,"an actor that already attempted UP cannot issue MOVE")||
       !expect(guest::actor_single_move_may_be_sent(1,0,0,false,true),false,"the send ledger cannot replace actual START READY evidence")||
       !expect(guest::actor_single_move_may_be_sent(1,0,0,true,false),false,"the send ledger cannot waive changed foreground identity or capture"))return 1;
    const std::string legacy=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"legacy_left_up","overlay":200,"actual_legacy_up_accepted":true,"legacy_up_dispatch_qpc":1001})";
    const std::string gone=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"isolation_gone","overlay":200,"removal_reason":0,"overlay_destroyed":true,"release_attempted":true,"release_completed":true,"release_succeeded":true,"capture_after_release":0,"detach_attempted":true,"detach_completed":true,"detach_succeeded":true})";
    const std::string idle=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"status","hook_owner_running":true,"gesture_active":false,"native_operation_pending":false,"native_completion_unknown":false,"visual_restore_pending":false})";
    const std::string abort_status=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"status","native_operation_pending":true,"mouse_up_callbacks":{"snapshot_known":true,"callback_count":3,"last":{"hook_observed_qpc":1001,"button":1,"generation_at_entry":7,"consumed_before":false,"aborted_before":true,"matching_branch_taken":false,"push_attempted":false,"push_succeeded":false}}})";
    const std::string aborted_end=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_end","generation":7,"end":1,"normal_up":false,"pending_retired":true})";
    auto aborted_gone=gone;aborted_gone.replace(aborted_gone.find("\"removal_reason\":0"),18,"\"removal_reason\":1");
    const auto mutate=[](std::string row,std::string_view from,std::string_view to){
        const auto at=row.find(from);if(at==row.npos)std::exit(2);row.replace(at,from.size(),to);return row;};
    // 8e0/P323,P329,P333: actual LL callback matched g6 without a queued LL
    // event; the captured legacy UP and NormalUp cleanup were real. These are
    // selected original field values, not a simulated native-delivery claim.
    const guest::UpEvidenceIdentity captured_identity{2928,"11ae76a3976ffba62facc127fcb411ebabfeaf86",6,437812890};
    const std::string captured_status=R"({"schema":"panebind-v1/1","source_sha":"11ae76a3976ffba62facc127fcb411ebabfeaf86","sequence":333,"process_id":2928,"tick":43968,"type":"status","enabled":true,"hook_owner_running":true,"gesture_active":false,"native_move_handoff_pending":false,"native_operation_pending":false,"native_completion_unknown":false,"visual_restore_pending":false,"eligible_windows":2,"operations":13,"native_callsite_claims":15,"mouse_up_callbacks":{"snapshot_known":true,"callback_count":27,"last":{"hook_observed_qpc":437818411,"button":1,"generation_at_entry":6,"consumed_before":false,"aborted_before":false,"matching_branch_taken":true,"push_attempted":false,"push_succeeded":false}}})";
    const std::string captured_legacy=R"({"schema":"panebind-v1/1","source_sha":"11ae76a3976ffba62facc127fcb411ebabfeaf86","process_id":2928,"type":"gesture_input_resource","generation":6,"fact":"legacy_left_up","overlay":394100,"actual_legacy_up_accepted":true,"legacy_up_dispatch_qpc":437834820})";
    const std::string captured_gone=R"({"schema":"panebind-v1/1","source_sha":"11ae76a3976ffba62facc127fcb411ebabfeaf86","process_id":2928,"type":"gesture_input_resource","generation":6,"fact":"isolation_gone","overlay":394100,"removal_reason":0,"overlay_destroyed":true,"release_attempted":true,"release_completed":true,"release_succeeded":true,"capture_after_release":0,"detach_attempted":true,"detach_completed":true,"detach_succeeded":true})";
    const auto captured_match=[&](std::string_view row,std::string_view delivered,std::string_view removed){
        return guest::matching_captured_up_with_LL_snapshot(row,delivered,removed,captured_identity);};
    if(!expect(captured_match(captured_status,captured_legacy,captured_gone),true,"8e0 real captured LL snapshot and actual legacy cleanup are accepted without inventing a push")||
       !expect(guest::matching_status_up_snapshot(captured_status,captured_identity),false,"older consumed-and-queued snapshot contract remains unchanged")||
       !expect(captured_match(mutate(captured_status,"\"process_id\":2928","\"process_id\":2929"),captured_legacy,captured_gone),false,"captured callback must belong to the exact product PID")||
       !expect(captured_match(mutate(captured_status,"\"source_sha\":\"11ae76a3976ffba62facc127fcb411ebabfeaf86\"","\"source_sha\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\""),captured_legacy,captured_gone),false,"captured callback cannot cross product source identity")||
       !expect(captured_match(mutate(captured_status,"\"generation_at_entry\":6","\"generation_at_entry\":7"),captured_legacy,captured_gone),false,"captured callback cannot inherit another generation")||
       !expect(captured_match(mutate(captured_status,"\"hook_observed_qpc\":437818411","\"hook_observed_qpc\":437812889"),captured_legacy,captured_gone),false,"captured callback must follow the unique test UP attempt")||
       !expect(captured_match(mutate(captured_status,"\"hook_observed_qpc\":437818411","\"hook_observed_qpc\":0"),captured_legacy,captured_gone),false,"unknown captured callback clock cannot pass")||
       !expect(captured_match(mutate(captured_status,"\"snapshot_known\":true","\"snapshot_known\":false"),captured_legacy,captured_gone),false,"unknown captured callback snapshot cannot pass")||
       !expect(captured_match(mutate(captured_status,"\"matching_branch_taken\":true","\"matching_branch_taken\":false"),captured_legacy,captured_gone),false,"an unrelated LL callback cannot complete this gesture")||
       !expect(captured_match(mutate(captured_status,"\"aborted_before\":false","\"aborted_before\":true"),captured_legacy,captured_gone),false,"abort cleanup is not NormalUp evidence")||
       !expect(captured_match(mutate(captured_status,"\"push_attempted\":false","\"push_attempted\":true"),captured_legacy,captured_gone),false,"a different queue branch is not silently converted into the captured route")||
       !expect(captured_match(captured_status,"",captured_gone),false,"LL observation alone cannot replace actual legacy UP")||
       !expect(captured_match(captured_status,mutate(captured_legacy,"\"actual_legacy_up_accepted\":true","\"actual_legacy_up_accepted\":false"),captured_gone),false,"unaccepted legacy input cannot complete the captured route")||
       !expect(captured_match(captured_status,captured_legacy,mutate(captured_gone,"\"detach_succeeded\":true","\"detach_succeeded\":false")),false,"captured UP still requires actual association cleanup"))return 1;
    const std::string early_status=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"status","enabled":true,"hook_owner_running":true,"gesture_active":false,"native_operation_pending":false,"native_completion_unknown":false,"visual_restore_pending":false,"mouse_up_callbacks":{"snapshot_known":true,"callback_count":1,"last":{"hook_observed_qpc":1001,"button":1,"generation_at_entry":7,"consumed_before":true,"aborted_before":false,"matching_branch_taken":true,"push_attempted":false,"push_succeeded":false}}})";
    const std::string early_failure=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"resource_failure","source":100,"source_pid":10,"source_tid":11,"overlay":200,"setup_stage":8,"native_failure":4,"capture_attempted":false,"attach_attempted":false,"release_attempted":false,"release_completed":false,"release_succeeded":false,"detach_attempted":false,"detach_completed":false,"detach_succeeded":false,"capture_actual_after":0,"capture_after_release":0})";
    const auto early_gone=mutate(mutate(early_failure,"\"fact\":\"resource_failure\"","\"fact\":\"isolation_gone\""),
        "\"setup_stage\":8,\"native_failure\":4","\"setup_stage\":0,\"native_failure\":0,\"removal_reason\":3,\"overlay_destroyed\":true,\"hotkey_unregistered\":true");
    const auto early_match=[&](std::string_view status_row,std::string_view failed,std::string_view removed){
        return guest::matching_pre_capture_click_release(status_row,failed,removed,identity);};
    if(!expect(early_match(early_status,early_failure,early_gone),true,"actual first click UP before capture plus exact physical-button setup rejection and cleanup")||
       !expect(early_match(early_status,early_failure,""),false,"pre-capture first click still requires actual cleanup receipt")||
       !expect(early_match(mutate(early_status,"\"hook_observed_qpc\":1001","\"hook_observed_qpc\":999"),early_failure,early_gone),false,"old callback cannot complete this first click")||
       !expect(early_match(early_status,mutate(early_failure,"\"generation\":7","\"generation\":8"),early_gone),false,"other generation failure cannot explain first click")||
       !expect(early_match(early_status,mutate(early_failure,"\"native_failure\":4","\"native_failure\":3"),early_gone),false,"unrelated setup failure is not physical release evidence")||
       !expect(early_match(early_status,early_failure,mutate(early_gone,"\"removal_reason\":3","\"removal_reason\":0")),false,"NormalUp cannot masquerade as pre-capture failure")||
       !expect(early_match(early_status,mutate(early_failure,"\"capture_attempted\":false","\"capture_attempted\":true"),early_gone),false,"capture-attempted route requires its real normal or abort contract")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,early_status,identity,"",early_gone),false,"pre-capture click cleanup never satisfies the unchanged NormalUp gate"))return 1;
    const std::string native_click_pending=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"native_move_pending","generation":7,"source":100,"source_key":3,"source_pid":10,"source_tid":11,"actual_DOWN_region":2,"locked_modifiers":0,"down_point":[500,400],"down_native_time":5000,"down_hook_qpc":950,"down_consumed":false,"write_authority":false})";
    const std::string native_click_release=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"native_move_passthrough_release","generation":7,"source":100,"actual_LL_UP_qpc":1001,"up_native_time":5010,"normalized_NormalUp":false,"new_native_placement":false})";
    guest::NativeFirstClickFacts native_click_facts{100,10,11,900,500,2,true,true,true,true};
    const auto native_click_match=[&](std::string_view pending,std::string_view release,const guest::NativeFirstClickFacts& facts){
        return guest::matching_native_first_click_release(pending,release,identity,facts);};
    if(!expect(native_click_match(native_click_pending,native_click_release,native_click_facts),true,
            "exact native-first first click LL release is observed without NormalUp or writer authority")||
       !expect(native_click_match(mutate(native_click_pending,"\"generation\":7","\"generation\":8"),native_click_release,native_click_facts),false,
            "native-first first click cannot inherit another generation's pending DOWN")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"process_id\":42","\"process_id\":43"),native_click_facts),false,
            "native-first first click requires the actual product PID")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"source_sha\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"",
            "\"source_sha\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\""),native_click_facts),false,
            "native-first first click cannot cross a product SHA")||
       !expect(native_click_match(mutate(native_click_pending,"\"source\":100","\"source\":101"),native_click_release,native_click_facts),false,
            "native-first first click binds the exact owned HWND")||
       !expect(native_click_match(mutate(native_click_pending,"\"source_tid\":11","\"source_tid\":12"),native_click_release,native_click_facts),false,
            "native-first first click binds the exact fixture source TID")||
       !expect(native_click_match(mutate(native_click_pending,"\"down_hook_qpc\":950","\"down_hook_qpc\":899"),native_click_release,native_click_facts),false,
            "native-first first click refuses a DOWN before the one test send")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"actual_LL_UP_qpc\":1001","\"actual_LL_UP_qpc\":999"),native_click_facts),false,
            "native-first first click refuses an UP before the one test send")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"up_native_time\":5010","\"up_native_time\":4500"),native_click_facts),false,
            "native-first first click requires bounded native DOWN to UP stamp order")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"up_native_time\":5010","\"up_native_time\":5501"),native_click_facts),false,
            "native-first first click rejects an UP beyond its one double-click time bound")||
       !expect(native_click_match(mutate(native_click_pending,"\"down_consumed\":false","\"down_consumed\":true"),native_click_release,native_click_facts),false,
            "consumed DOWN cannot impersonate native passthrough")||
       !expect(native_click_match(mutate(native_click_pending,"\"write_authority\":false","\"write_authority\":true"),native_click_release,native_click_facts),false,
            "native first click cannot carry write authority")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"normalized_NormalUp\":false","\"normalized_NormalUp\":true"),native_click_facts),false,
            "native passthrough is never normalized NormalUp")||
       !expect(native_click_match(native_click_pending,mutate(native_click_release,"\"new_native_placement\":false","\"new_native_placement\":true"),native_click_facts),false,
            "native first click cannot claim a new product placement"))return 1;
    auto changed_native_click=native_click_facts;changed_native_click.no_enhanced_records=false;
    if(!expect(native_click_match(native_click_pending,native_click_release,changed_native_click),false,
            "START placement cancel or CaptureReady prevents native-only first click proof"))return 1;
    changed_native_click=native_click_facts;changed_native_click.geometry_unchanged=false;
    if(!expect(native_click_match(native_click_pending,native_click_release,changed_native_click),false,
            "native first click with changed owned geometry is not a stationary double-click first press"))return 1;
    changed_native_click=native_click_facts;changed_native_click.gui_idle=false;
    if(!expect(native_click_match(native_click_pending,native_click_release,changed_native_click),false,
            "native first click requires observed idle exact source GUI")||
       !expect(native_click_match(native_click_pending,"",native_click_facts),false,
            "unattributed Raw UP without a real same-generation product LL release is not first-click proof")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,early_status,identity),false,
            "native first click does not alter the unchanged NormalUp status gate"))return 1;
    const auto captured_revoke=mutate(mutate(abort_status,"\"consumed_before\":false","\"consumed_before\":true"),
        "\"matching_branch_taken\":false","\"matching_branch_taken\":true");
    const auto abort_match=[&](std::string_view row,std::string_view end,std::string_view resource){
        return guest::matching_abort_cleanup_up(row,end,resource,identity);};
    if(!expect(abort_match(abort_status,aborted_end,aborted_gone),true,"retired no-push actual LL cleanup UP does not require in-flight writer idle")||
       !expect(abort_match(captured_revoke,aborted_end,aborted_gone),true,"capture-pair actual UP revokes without normalized push")||
       !expect(abort_match("",aborted_end,aborted_gone),false,"missing actual product callback snapshot rejects abort cleanup UP")||
       !expect(abort_match(mutate(abort_status,"\"hook_observed_qpc\":1001","\"hook_observed_qpc\":999"),aborted_end,aborted_gone),false,"older callback cannot prove this single cleanup UP")||
       !expect(abort_match(mutate(abort_status,"\"generation_at_entry\":7","\"generation_at_entry\":8"),aborted_end,aborted_gone),false,"different generation callback rejects")||
       !expect(abort_match(mutate(abort_status,"\"button\":1","\"button\":2"),aborted_end,aborted_gone),false,"other mouse button is not left cleanup UP")||
       !expect(abort_match(abort_status,mutate(aborted_end,"\"normal_up\":false","\"normal_up\":true"),aborted_gone),false,"normal end cannot use abort-only evidence")||
       !expect(abort_match(abort_status,aborted_end,gone),false,"NormalUp resource removal cannot masquerade as abort cleanup")||
       !expect(abort_match(abort_status,aborted_end,mutate(aborted_gone,"\"detach_succeeded\":true","\"detach_succeeded\":false")),false,"unconfirmed detach rejects abort cleanup")||
       !expect(abort_match(mutate(abort_status,"\"process_id\":42","\"process_id\":43"),aborted_end,aborted_gone),false,"other product PID snapshot rejects")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,abort_status,identity),false,"abort cleanup snapshot cannot satisfy unchanged NormalUp gate"))return 1;
    // The actual inflight observation has LL UP before the later SourceLost:
    // matching capture branch, no normalized push, then exact release/detach.
    // It is not an after-abort UP and cannot satisfy the NormalUp predicate.
    const std::string inflight_status=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"status","hook_owner_running":true,"gesture_active":false,"native_operation_pending":true,"native_completion_unknown":true,"mouse_up_callbacks":{"snapshot_known":true,"callback_count":34,"last":{"hook_observed_qpc":1001,"button":1,"generation_at_entry":7,"consumed_before":false,"aborted_before":false,"matching_branch_taken":true,"push_attempted":false,"push_succeeded":false}}})";
    const std::string inflight_end=mutate(aborted_end,"\"end\":1","\"end\":3");
    const std::string inflight_release=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"capture_released","resource_observed_qpc":1010,"source":100,"source_pid":10,"source_tid":11,"overlay":200,"capture_owner_tid":12,"capture_release_before_HWND":200,"capture_owner_observation_known":true,"release_attempted":true,"release_completed":true,"release_succeeded":true,"capture_after_release":0})";
    const std::string inflight_gone=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"isolation_gone","resource_observed_qpc":1020,"source":100,"source_pid":10,"source_tid":11,"overlay":200,"capture_owner_tid":12,"capture_release_before_HWND":200,"removal_reason":2,"actual_legacy_up_accepted":false,"overlay_destroyed":true,"hotkey_unregistered":true,"attach_succeeded":true,"release_attempted":true,"release_completed":true,"release_succeeded":true,"capture_after_release":0,"detach_attempted":true,"detach_completed":true,"detach_succeeded":true})";
    const auto inflight_match=[&](std::string_view row,std::string_view end,std::string_view gone_row,std::string_view release){
        return guest::matching_inflight_up_before_abort(row,end,gone_row,release,identity);};
    if(!expect(inflight_match(inflight_status,inflight_end,inflight_gone,inflight_release),true,"actual same-generation LL UP before inflight abort plus complete release/detach is observed, not NormalUp")||
       !expect(abort_match(inflight_status,inflight_end,inflight_gone),false,"before-abort inflight shape does not alter generic after-abort matcher")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,inflight_status,identity),false,"before-abort inflight callback never grants NormalUp")||
       !expect(inflight_match(mutate(inflight_status,"\"generation_at_entry\":7","\"generation_at_entry\":8"),inflight_end,inflight_gone,inflight_release),false,"inflight old or other generation rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"hook_observed_qpc\":1001","\"hook_observed_qpc\":999"),inflight_end,inflight_gone,inflight_release),false,"callback predating the one test UP rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"process_id\":42","\"process_id\":43"),inflight_end,inflight_gone,inflight_release),false,"inflight wrong product PID rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"source_sha\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"","\"source_sha\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\""),inflight_end,inflight_gone,inflight_release),false,"inflight wrong product SHA rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"button\":1","\"button\":2"),inflight_end,inflight_gone,inflight_release),false,"inflight other mouse button rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"matching_branch_taken\":true","\"matching_branch_taken\":false"),inflight_end,inflight_gone,inflight_release),false,"unmatched callback rejects")||
       !expect(inflight_match(mutate(inflight_status,"\"push_attempted\":false","\"push_attempted\":true"),inflight_end,inflight_gone,inflight_release),false,"normalized push is not the before-abort route")||
       !expect(inflight_match(mutate(inflight_status,"\"native_completion_unknown\":true","\"native_completion_unknown\":false"),inflight_end,inflight_gone,inflight_release),true,"a later status completion does not erase the actual earlier LL UP; caller separately proves issued-call timing")||
       !expect(inflight_match(inflight_status,mutate(inflight_end,"\"normal_up\":false","\"normal_up\":true"),inflight_gone,inflight_release),false,"NormalUp end cannot use before-abort route")||
       !expect(inflight_match(inflight_status,mutate(inflight_end,"\"generation\":7","\"generation\":8"),inflight_gone,inflight_release),false,"other-generation retirement rejects")||
       !expect(inflight_match(inflight_status,inflight_end,"",inflight_release),false,"missing actual Gone rejects")||
       !expect(inflight_match(inflight_status,inflight_end,inflight_gone,""),false,"missing actual capture release rejects")||
       !expect(inflight_match(inflight_status,inflight_end,mutate(inflight_gone,"\"source\":100","\"source\":101"),inflight_release),false,"different source cleanup rejects")||
       !expect(inflight_match(inflight_status,inflight_end,mutate(inflight_gone,"\"detach_succeeded\":true","\"detach_succeeded\":false"),inflight_release),false,"failed detach rejects")||
       !expect(inflight_match(inflight_status,inflight_end,inflight_gone,mutate(inflight_release,"\"capture_after_release\":0","\"capture_after_release\":200")),false,"capture still held rejects"))return 1;
    const std::string lost_release=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"capture_released","source":100,"source_pid":10,"source_tid":11,"overlay":200,"capture_owner_tid":12,"capture_release_before_HWND":0,"capture_owner_observation_known":true,"capture_release_after_is_copied_no_call":true,"release_attempted":false,"release_completed":false,"release_succeeded":false,"capture_after_release":0})";
    const std::string lost_gone=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"isolation_gone","source":100,"source_pid":10,"source_tid":11,"overlay":200,"capture_owner_tid":12,"capture_release_before_HWND":0,"removal_reason":1,"overlay_destroyed":true,"hotkey_unregistered":true,"release_attempted":false,"release_completed":false,"release_succeeded":false,"capture_after_release":0,"detach_attempted":true,"detach_completed":true,"detach_succeeded":true})";
    const auto lost_match=[&](std::string_view release,std::string_view gone_row){
        return guest::matching_abort_cleanup_up(abort_status,aborted_end,gone_row,identity,release);};
    if(!expect(lost_match(lost_release,lost_gone),true,"typed actual owner-local absent capture plus destroyed/detached abort is not a fabricated ReleaseCapture")||
       !expect(lost_match("",lost_gone),false,"Gone default zero fields cannot invent the missing owner observation")||
       !expect(lost_match(mutate(lost_release,"\"capture_owner_observation_known\":true","\"capture_owner_observation_known\":false"),lost_gone),false,"unknown actual capture observation rejects")||
       !expect(lost_match(mutate(lost_release,"\"capture_owner_tid\":12","\"capture_owner_tid\":13"),lost_gone),false,"different cleanup owner rejects")||
       !expect(lost_match(mutate(lost_release,"\"source_tid\":11","\"source_tid\":13"),lost_gone),false,"different source thread rejects abort receipt")||
       !expect(lost_match(mutate(lost_release,"\"release_attempted\":false","\"release_attempted\":true"),lost_gone),false,"failed actual release cannot masquerade as absent-capture no-call")||
       !expect(lost_match(lost_release,mutate(lost_gone,"\"detach_succeeded\":true","\"detach_succeeded\":false")),false,"absent capture cannot mask failed detach")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,identity,legacy,lost_gone),false,"abort no-call cleanup never replaces strict NormalUp release"))return 1;
    if(!expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,identity,legacy,gone),true,"fresh public idle status plus actual captured legacy UP and confirmed cleanup")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,identity,"",gone),false,"Raw or idle status alone cannot replace actual legacy UP")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,identity,legacy,""),false,"missing release and detach are not NormalUp")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,"",identity,legacy,gone),false,"captured receipts do not replace required fresh public status")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,{42,identity.source_sha,8,1000},legacy,gone),false,"different generation resource receipts reject")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,false,idle,{42,identity.source_sha,7,1002},legacy,gone),false,"pre-send legacy UP cannot verify this release"))return 1;
    if(!expect(snapshot(status),true,"fresh matching public snapshot")||
       !expect(snapshot(""),false,"missing snapshot")||
       !expect(snapshot(changed("\"mouse_up_callbacks\":","\"missing_mouse_up_callbacks\":")),false,"status missing callback snapshot")||
       !expect(snapshot(changed("\"snapshot_known\":true","\"snapshot_known\":false")),false,"unknown snapshot")||
       !expect(snapshot(changed("\"callback_count\":1","\"callback_count\":0")),false,"no callbacks")||
       !expect(snapshot(changed("\"generation_at_entry\":7","\"generation_at_entry\":8")),false,"wrong generation")||
       !expect(snapshot(changed("\"hook_observed_qpc\":1001","\"hook_observed_qpc\":999")),false,"old QPC")||
       !expect(snapshot(changed("\"hook_observed_qpc\":1001","\"hook_observed_qpc\":0")),false,"unknown QPC")||
       !expect(snapshot(changed("\"button\":1","\"button\":2")),false,"wrong button")||
       !expect(snapshot(changed("\"aborted_before\":false","\"aborted_before\":true")),false,"aborted before")||
       !expect(snapshot(changed("\"consumed_before\":true","\"consumed_before\":false")),false,"not consumed at callback entry")||
       !expect(snapshot(changed("\"matching_branch_taken\":true","\"matching_branch_taken\":false")),false,"matching branch not taken")||
       !expect(snapshot(changed("\"push_attempted\":true","\"push_attempted\":false")),false,"product queue push not attempted")||
       !expect(snapshot(changed("\"push_succeeded\":true","\"push_succeeded\":false")),false,"failed product queue push")||
       !expect(snapshot(changed("\"process_id\":42","\"process_id\":43")),false,"different product PID")||
       !expect(snapshot(changed("\"source_sha\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"","\"source_sha\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\"")),false,"different product SHA")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::DiagnosticEvent,false,status,identity),false,"diagnostic missing event cannot use status")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::DiagnosticEvent,true,"",identity),true,"diagnostic retains actual event contract")||
       !expect(guest::accepted_up_evidence(guest::UpEvidenceMode::StatusSnapshot,true,"",identity),false,"snapshot mode cannot substitute event"))return 1;
    std::cout << checked << " guest-directory, UP-evidence, deadline and physical-log cases passed; bounded test-owned filesystem operations; no UI or input\n";
}
