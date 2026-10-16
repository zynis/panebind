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
