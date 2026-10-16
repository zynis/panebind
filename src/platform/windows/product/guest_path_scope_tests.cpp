#include "platform/windows/product/guest_path_scope.h"
#include "platform/windows/product/v1_guest_up_evidence.h"
#include <cstdlib>
#include <iostream>
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
    const std::string legacy=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"legacy_left_up","overlay":200,"actual_legacy_up_accepted":true,"legacy_up_dispatch_qpc":1001})";
    const std::string gone=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"gesture_input_resource","generation":7,"fact":"isolation_gone","overlay":200,"removal_reason":0,"overlay_destroyed":true,"release_attempted":true,"release_completed":true,"release_succeeded":true,"capture_after_release":0,"detach_attempted":true,"detach_completed":true,"detach_succeeded":true})";
    const std::string idle=R"({"schema":"panebind-v1/1","source_sha":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","process_id":42,"type":"status","hook_owner_running":true,"gesture_active":false,"native_operation_pending":false,"native_completion_unknown":false,"visual_restore_pending":false})";
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
    std::cout << checked << " guest-directory and UP-evidence cases passed; no filesystem or input operation\n";
}
