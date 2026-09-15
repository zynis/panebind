#include "platform/windows/explorer/explorer_group_session.h"
#include "platform/windows/explorer/explorer_group_capture_json.h"
#include "platform/windows/text_encoding.h"
#include "platform/windows/console/sta_console_line_reader.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace e=panebind::platform::windows::explorer;
namespace w=panebind::platform::windows;
using panebind::core::geometry::Rect;
namespace {
std::string quote(std::string_view text){return w::json_quote(text);}
std::string quote(std::wstring_view text){const auto s=w::utf16_to_utf8(text);if(!s.value)throw std::runtime_error("invalid UTF16");return quote(*s.value);}
const char* flag(bool value){return value?"true":"false";}
std::string rect(const Rect& r){return "["+std::to_string(r.left())+","+std::to_string(r.top())+","+std::to_string(r.right())+","+std::to_string(r.bottom())+"]";}
std::string snapshots(const e::detail::GroupSnapshots& values) {
    std::ostringstream s;s<<'[';
    for(std::size_t i=0;i<3;++i){if(i)s<<',';const auto& v=values[i];
        s<<"{\"member\":"<<i<<",\"visible\":"<<rect(v.visible_rect)<<",\"positioning\":"<<rect(v.positioning_rect)
         <<",\"pid\":"<<v.process_id<<",\"tid\":"<<v.thread_id<<",\"dpi\":"<<v.dpi
         <<",\"monitor\":"<<quote(v.monitor_device_name)<<",\"work_area\":"<<rect(v.monitor_work_area)
         <<",\"image\":"<<quote(v.process_image_path)<<",\"class\":"<<quote(v.window_class)
         <<",\"current_desktop\":"<<flag(v.on_current_virtual_desktop)<<",\"exact_location\":"<<flag(v.exact_test_location)
         <<",\"minimized\":"<<flag(v.minimized)<<",\"maximized\":"<<flag(v.maximized)<<'}';}
    s<<']';return s.str();
}
bool console(HANDLE handle){DWORD mode{};return handle && handle!=INVALID_HANDLE_VALUE && GetFileType(handle)==FILE_TYPE_CHAR && GetConsoleMode(handle,&mode);}
bool print(std::wstring_view text){DWORD written{};return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size();}
class Log {
public:
    explicit Log(const std::filesystem::path& path){file_=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);}
    ~Log(){if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
    bool record(std::string_view type,std::string_view fields={}){
        const auto line="{\"schema\":\"r1c4a/v1\",\"sequence\":"+std::to_string(++sequence_)+",\"type\":"+quote(type)+std::string(fields)+"}\n";
        DWORD written{};healthy_=healthy_ && file_!=INVALID_HANDLE_VALUE && WriteFile(file_,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) && written==line.size();
        return healthy_;
    }
    bool healthy()const{return healthy_;}
private:HANDLE file_{INVALID_HANDLE_VALUE};std::uint64_t sequence_{};bool healthy_{true};
};
std::optional<std::wstring> read_line(Log& log,w::console_input::StaConsoleLineReader& reader,
                                    std::string_view kind) {
    auto result=reader.read();
    std::ostringstream fields;
    fields<<",\"input_wait_kind\":"<<quote(kind)<<",\"wait_result\":"<<quote(w::console_input::line_status_name(result.status))
          <<",\"owner_thread\":"<<result.owner_thread<<",\"wait_call_count\":"<<result.wait_count
          <<",\"pump_call_count\":"<<result.pump_count<<",\"message_dispatch_count\":"<<result.message_dispatch_count
          <<",\"console_input_event_count\":"<<result.console_input_event_count
          <<",\"input_mode_before\":"<<result.input_mode_before<<",\"input_mode_after\":"<<result.input_mode_after
          <<",\"modes_observed\":"<<flag(result.modes_observed)<<",\"mode_changed\":"<<flag(result.mode_changed)
          <<",\"error\":"<<result.error;
    // One bounded summary per wait, no characters or per-message logs.
    if(!log.record("console_wait",fields.str()))return std::nullopt;
    return std::move(result.line);
}
void runtime_records(Log& log,const e::ExplorerGroupSession& group){
    std::array<std::size_t,3> logical_order{0,1,2};
    std::sort(logical_order.begin(),logical_order.end(),[&](auto a,auto b){
        return std::to_string(group.bindings()[a].window_id)<std::to_string(group.bindings()[b].window_id);
    });
    for(const auto& r:group.receipts()){
        std::ostringstream f;f<<",\"member\":"<<r.member_index<<",\"window_id\":"<<r.window_id
         <<",\"capability_generation\":"<<r.capability_generation<<",\"native_source\":"<<reinterpret_cast<std::uintptr_t>(r.native_source)
         <<",\"event\":"<<quote(r.kind==e::GroupEventKind::Start?"START":r.kind==e::GroupEventKind::End?"END":r.kind==e::GroupEventKind::Destroy?"DESTROY":"LOCATION")
         <<",\"receipt_sequence\":"<<r.sequence<<",\"native_thread\":"<<r.native_thread<<",\"native_time\":"<<r.native_time
         <<",\"callback_qpc\":"<<r.callback_qpc<<",\"ctrl_available\":"<<flag(r.ctrl.available)<<",\"ctrl\":"<<flag(r.ctrl.ctrl);
        log.record("receipt",f.str());
    }
    for(const auto& q:group.quanta()){
        std::ostringstream f;f<<",\"id\":"<<q.id<<",\"gesture\":"<<q.gesture<<",\"first_receipt\":"<<q.first_sequence
         <<",\"last_receipt\":"<<q.last_sequence<<",\"receipt_count\":"<<q.receipts<<",\"coalesced\":"<<q.coalesced
         <<",\"owner_qpc\":"<<q.owner_qpc<<",\"end_qpc\":"<<q.end_qpc;log.record("quantum",f.str());
    }
    for(std::size_t n=0;n<group.operations().size();++n){const auto& op=group.operations()[n];const auto& r=op.receipt;
        std::ostringstream f;f<<",\"group\":"<<op.group<<",\"gesture\":"<<op.gesture<<",\"batch\":"<<op.batch
         <<",\"phase\":"<<quote(op.gesture?"active":"restore")<<",\"source_member\":"<<op.source_member
         <<",\"source_receipt\":"<<op.source_sequence<<",\"watermark\":"<<op.watermark<<",\"pre_native_tick\":"<<op.pre_native_tick
         <<",\"receipt_qpc\":"<<op.receipt_qpc<<",\"owner_qpc\":"<<op.owner_qpc
         <<",\"source_visible\":"<<rect(op.source_visible)
         <<",\"native_start_qpc\":"<<r.native.native_start_qpc<<",\"native_return_qpc\":"<<r.native.native_return_qpc
         <<",\"postverify_qpc\":"<<r.postverify_qpc<<",\"native_path\":\"HDWP\",\"native_flags\":21"
         <<",\"all_preflight\":"<<flag(r.all_preflight)<<",\"all_pending_registered\":"<<flag(r.all_pending_registered)
         <<",\"native_attempted\":"<<flag(r.native.native_commit_attempted)<<",\"native_succeeded\":"<<flag(r.native.succeeded)
         <<",\"deferred_count\":"<<r.native.deferred<<",\"stage\":"<<static_cast<int>(r.native.stage)
         <<",\"error\":"<<r.native.error<<",\"all_postverify\":"<<flag(r.all_postverify)<<",\"reason\":"<<quote(r.reason)
         <<",\"before\":"<<snapshots(r.before)<<",\"members\":[";
        bool first=true;for(const auto i:logical_order)if(r.targets[i]){
            if(!first)f<<',';first=false;
            f<<"{\"member\":"<<i<<",\"target\":"<<rect(*r.targets[i])<<",\"positioning_target\":"<<(r.positioning_targets[i]?rect(*r.positioning_targets[i]):"null")
             <<",\"actual\":"<<(r.actual[i]?rect(r.actual[i]->visible_rect):"null")<<",\"actual_positioning\":"<<(r.actual[i]?rect(r.actual[i]->positioning_rect):"null")
             <<",\"exact\":"<<flag(r.exact[i])<<'}';
        }f<<']';log.record("batch",f.str());
    }
    for(const auto& f:group.feedback())log.record("feedback",",\"member\":"+std::to_string(f.member)+",\"gesture\":"+std::to_string(f.gesture)+",\"batch\":"+std::to_string(f.batch)+",\"receipt_sequence\":"+std::to_string(f.sequence)+",\"result\":"+quote(f.result)+",\"observed_visible\":"+rect(f.observed_visible));
    for(const auto& g:group.gestures()){
        std::ostringstream f;f<<",\"group\":"<<g.group<<",\"gesture\":"<<g.gesture<<",\"source_member\":"<<g.source_member
         <<",\"start_receipt\":"<<g.start_sequence<<",\"end_receipt\":"<<g.end_sequence
         <<",\"starts\":"<<g.starts<<",\"locations\":"<<g.locations<<",\"ends\":"<<g.ends<<",\"batches\":"<<g.batches
         <<",\"callback_ctrl\":"<<flag(g.callback_ctrl.ctrl)<<",\"owner_ctrl\":"<<flag(g.owner_ctrl.ctrl)
         <<",\"callback_qpc\":"<<g.callback_qpc<<",\"decision_qpc\":"<<g.decision_qpc
         <<",\"initial\":"<<snapshots(g.initial)<<",\"final\":"<<snapshots(g.final)
         <<",\"exact\":"<<flag(g.exact)<<",\"roles_cleared\":"<<flag(g.roles_cleared)<<",\"pending_empty\":"<<flag(g.pending_empty)
         <<",\"group_ready\":"<<flag(g.group_ready);log.record("gesture",f.str());
    }
}
bool record_readiness(Log& log,std::string_view type,const e::GroupReadinessPreview& preview) {
    const auto& a=preview.activity;
    std::ostringstream f;
    f<<",\"attempt\":"<<preview.attempt<<",\"valid\":"<<flag(preview.valid)
     <<",\"setup_check\":"<<flag(preview.setup_check)<<",\"ready\":"<<flag(preview.readiness.ready)
     <<",\"reason\":"<<quote(preview.reason)<<",\"snapshots\":"<<(preview.snapshots?snapshots(*preview.snapshots):"null")
     <<",\"member_sizes\":[";
    for(std::size_t i=0;i<3;++i){if(i)f<<',';f<<'['<<preview.member_sizes[i][0]<<','<<preview.member_sizes[i][1]<<']';}
    f<<"],\"work_area\":"<<rect(preview.work_area)<<",\"relation_count\":"<<preview.readiness.relation_count
     <<",\"pairs\":[";
    for(std::size_t i=0;i<3;++i){if(i)f<<',';const auto& p=preview.readiness.pairs[i];
        f<<"{\"first\":"<<p.first<<",\"second\":"<<p.second<<",\"relation\":"<<flag(p.relation)
         <<",\"first_edge\":"<<static_cast<int>(p.first_edge)<<",\"second_edge\":"<<static_cast<int>(p.second_edge)
         <<",\"signed_gap\":"<<p.signed_gap<<",\"orthogonal_overlap\":"<<p.orthogonal_overlap<<'}';}
    f<<"],\"components\":[";
    for(std::size_t i=0;i<3;++i){if(i)f<<',';f<<'[';bool first=true;
        for(const auto member:preview.readiness.components[i]){if(!first)f<<',';first=false;f<<member;}f<<']';}
    f<<"],\"group_generation\":"<<a.group_generation
     <<",\"gesture_generation\":"<<a.gesture_generation<<",\"native_apply_count\":"<<a.native_apply_count
     <<",\"hdwp_begin_count\":"<<a.hdwp_begin_count<<",\"event_source_running\":"<<flag(a.event_source_running)
     <<",\"pending_count\":"<<a.pending_count<<",\"leader_present\":"<<flag(a.leader_present)
     <<",\"owner_thread\":"<<flag(a.owner_thread)<<",\"capture_qpc\":"<<preview.capture_qpc
     <<",\"capture\":"<<e::detail::group_capture_json(preview.capture_result);
    return log.record(type,f.str());
}
bool print_readiness(const e::GroupReadinessPreview& p) {
    if(!p.capture_result) {
        const auto& c=p.capture_result;
        const auto label=e::detail::group_capture_stage_name(c.failure_stage);
        const auto reason=e::detail::capture_safe_label(c.reason);
        std::wstring text=L"\r\nCapture NOT READY, member="+
            (c.failed_member_index?std::wstring(1,static_cast<wchar_t>(L'A'+*c.failed_member_index)):L"UNKNOWN")+
            L", stage="+std::wstring(label.begin(),label.end())+L", reason="+std::wstring(reason.begin(),reason.end())+L"\r\n";
        if(c.recoverable())text+=L"身份仍有效，未 retire、未移动窗口。请移回原 monitor/DPI 后按 Enter 重试；不能接受当前布局。\r\n";
        return print(text);
    }
    const auto edge=[](panebind::core::geometry::Edge e)->std::wstring_view {
        using E=panebind::core::geometry::Edge;
        switch(e){case E::Left:return L"Left";case E::Right:return L"Right";
            case E::Top:return L"Top";case E::Bottom:return L"Bottom";}return L"?";
    };
    std::wstring text=L"\r\nTopology preview #"+std::to_wstring(p.attempt)+
        (p.setup_check?L"（接受前 fresh check）":L"（只读）")+L"\r\n";
    for(std::size_t i=0;i<3;++i)text+=std::wstring(1,static_cast<wchar_t>(L'A'+i))+L": "+
        std::to_wstring(p.member_sizes[i][0])+L" × "+std::to_wstring(p.member_sizes[i][1])+L" px\r\n";
    for(const auto& pair:p.readiness.pairs) {
        text+=std::wstring(1,static_cast<wchar_t>(L'A'+pair.first))+L"-"+
            std::wstring(1,static_cast<wchar_t>(L'A'+pair.second))+
            (pair.relation?L": relation YES, ":L": relation NO, nearest opposing edges: ")+
            std::wstring(edge(pair.first_edge))+L"/"+std::wstring(edge(pair.second_edge))+
            L", gap="+std::to_wstring(pair.signed_gap)+L", overlap="+std::to_wstring(pair.orthogonal_overlap)+L"\r\n";
    }
    text+=L"relation_count="+std::to_wstring(p.readiness.relation_count)+L"\r\n";
    for(std::size_t i=0;i<3;++i) {
        text+=L"connected_component("+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L") = ";
        for(const auto member:p.readiness.components[i])text+=std::wstring(1,static_cast<wchar_t>(L'A'+member))+L" ";
        text+=L"\r\n";
    }
    text+=p.readiness.ready?L"READY：三个成员连通；2 或 3 条 relation 均可。\r\n":
        L"NOT READY：请手工 Move + Resize，使三窗通过有正重叠的对边接触连通。\r\n";
    return print(text);
}
int run(Log& log){
    log.record("startup",",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"qpc_frequency\":"+std::to_string(e::glue_qpc_frequency())+",\"member_count\":3,\"interactive_console\":true,\"readiness_contract\":\"topology_neutral_accepted_baseline_v2\",\"capture_contract\":\"structured_preaccept_v1\",\"console_wait_contract\":\"sta_message_pump_v2\",\"console_mode_contract\":\"preserve_host_mode_v1\",\"console_input_contract\":\"readconsoleinputex_nowait_v1\",\"owner_sta_thread\":"+std::to_string(GetCurrentThreadId()));
    w::console_input::StaConsoleLineReader input{GetStdHandle(STD_INPUT_HANDLE),GetStdHandle(STD_OUTPUT_HANDLE)};
    std::string stage="MEMBER_A_PROVISIONING";
    const auto stop=[&](std::string_view reason){log.record("shutdown",",\"result\":\"BLOCKED\",\"reason\":"+quote(reason)+",\"stage\":"+quote(stage));return 2;};
    e::ExplorerGroupSession::OwnedMembers members;
    for(std::size_t i=0;i<3;++i){
        stage=i==0?"MEMBER_A_PROVISIONING":i==1?"MEMBER_B_PROVISIONING":"MEMBER_C_PROVISIONING";
        GUID guid{};wchar_t nonce[64]{};
        if(CoCreateGuid(&guid)!=S_OK || !StringFromGUID2(guid,nonce,64))return stop("nonce_failed");
        const auto path=std::filesystem::temp_directory_path()/(std::wstring{L"PaneBind-R1C4A-"}+nonce);
        if(!std::filesystem::create_directory(path))return stop("nonce_directory_failed");
        auto begin=e::ExplorerConsentProvisioning::begin(path);
        if(!begin.succeeded())return stop("baseline_failed");
        auto prompt=begin.provisioning->record_target_prompt();
        if(!prompt.succeeded())return stop("target_prompt_failed");
        log.record("target_prompt",",\"member\":"+std::to_string(i)+",\"nonce_directory\":"+quote(path.native())+",\"baseline_generation\":"+std::to_string(begin.facts.generations.baseline_generation)+",\"prompt_generation\":"+std::to_string(prompt.generation));
        if(!log.healthy())return stop("evidence_write_failed");
        const std::wstring label(1,static_cast<wchar_t>(L'A'+i));
        if(!print(L"\r\n成员 "+label+L"：请亲自新建一个 Explorer 窗口（不要使用已有窗口），进入以下目录：\r\n"+path.native()+L"\r\n完成后按 Enter；其他输入取消。\r\n"))return stop("console_failed");
        const auto kind=i==0?"member_a_confirmation":i==1?"member_b_confirmation":"member_c_confirmation";
        const auto line=read_line(log,input,kind);if(!line||!line->empty())return stop("target_declined");
        auto target=begin.provisioning->confirm_user_target();
        if(!target.succeeded())return stop("target_confirmation_failed");
        const auto& g=target.facts.generations;
        std::ostringstream f;f<<",\"member\":"<<i<<",\"target_confirmation_generation\":"<<g.target_confirmation_generation
         <<",\"eligibility_generation\":"<<g.eligibility_generation<<",\"token_generation\":"<<g.token_generation
         <<",\"baseline_exclusion_complete\":"<<flag(target.facts.baseline_exclusion_complete)
         <<",\"unique_new_target\":"<<flag(target.facts.unique_new_target)<<",\"exact_location\":"<<flag(target.facts.exact_target_location)
         <<",\"token_issued\":"<<flag(target.facts.token_issued)<<",\"input_source\":\"interactive_console\"";
        log.record("target_confirmed",f.str());members[i]=std::move(target.session);
    }
    stage="GROUP_CONSENT";
    if(!print(L"\r\n三个成员已单独确认。是否授权这三个新建窗口的 Ctrl+Move 和最后精确恢复？\r\n"
        L"绑定后 PaneBind 不会摆放窗口。请您手工 Move + Resize 为任意三窗连通拓扑；尺寸不必相同。\r\n"
        L"稍后单独接受当前拓扑作为恢复基线。PaneBind 不 resize、不关闭窗口、不改变 Z-order。\r\n"
        L"输入 Y 后 Enter 同意：\r\n"))return stop("console_failed");
    const auto consent=read_line(log,input,"group_consent");if(!consent||(*consent!=L"Y"&&*consent!=L"y"))return stop("group_declined");
    if(!log.record("group_consent",",\"confirmed\":true,\"input_source\":\"interactive_console\""))return stop("evidence_write_failed");
    stage="GROUP_BIND";
    auto group=e::ExplorerGroupSession::create(std::move(members));if(!group)return stop("group_binding_failed");
    for(std::size_t i=0;i<3;++i){const auto& binding=group->bindings()[i];
        std::ostringstream f;f<<",\"member\":"<<i<<",\"window_id\":"<<binding.window_id<<",\"capability_generation\":"<<binding.capability_generation
         <<",\"consent_generation\":"<<binding.consent_generation<<",\"hwnd\":"<<reinterpret_cast<std::uintptr_t>(binding.window)
         <<",\"pid\":"<<binding.process_id<<",\"tid\":"<<binding.thread_id<<",\"group\":"<<group->generation();log.record("binding",f.str());}
    if(!log.record("binding_snapshot",",\"snapshots\":"+snapshots(group->binding_snapshots())))return stop("evidence_write_failed");
    stage="TOPOLOGY_PREVIEW";
    bool success=false;
    for(;;) {
        auto preview=group->preview_readiness();
        if(!record_readiness(log,"readiness_preview",preview))return stop("evidence_write_failed");
        if(!preview.valid&&!preview.capture_result.recoverable())return stop(preview.reason);
        if(!print_readiness(preview))return stop("console_failed");
        if(!print(L"请手工 Move + Resize；勿导航、更换窗口或 monitor/DPI。\r\n"
            L"PaneBind native writes = 0。Enter 重新采集；READY 时输入 Y 接受当前拓扑；Q 取消。\r\n"))return stop("console_failed");
        const auto line=read_line(log,input,"readiness_recheck");
        if(!line)return stop("readiness_input_failed");
        if(*line==L"Q"||*line==L"q")return stop("readiness_cancelled");
        if(*line!=L"Y"&&*line!=L"y")continue;
        if(!preview.readiness.ready)continue;
        if(!log.record("topology_consent",",\"confirmed\":true,\"input_source\":\"interactive_console\",\"preview_attempt\":"+std::to_string(preview.attempt)))return stop("evidence_write_failed");
        success=group->setup(); // another capture; still no native placement
        preview=*group->last_readiness_preview();
        if(!record_readiness(log,"readiness_preview",preview))return stop("evidence_write_failed");
        if(group->accepted_readiness()) {
            if(!record_readiness(log,"readiness_accepted",*group->accepted_readiness()))return stop("evidence_write_failed");
            break;
        }
        if((!preview.valid&&!preview.capture_result.recoverable())||!group->healthy())return stop(preview.reason);
        if(!print_readiness(preview))return stop("console_failed");
    }
    stage="GESTURE";
    for(std::size_t i=0;success&&i<3;++i){
        print(L"\r\nGesture "+std::to_wstring(i+1)+L"：先按住 Ctrl，再拖动成员 "+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L" 的标题栏约 1 秒，再松鼠标。保持整组在工作区内。\r\n");
        success=group->run_gesture(i,std::chrono::seconds{120});
    }
    if(success){stage="RESTORE";success=group->restore();}
    runtime_records(log,*group);
    const auto facts=group->event_facts();
    std::ostringstream f;f<<",\"accepted\":"<<facts.accepted<<",\"ignored\":"<<facts.ignored<<",\"overflow\":"<<facts.overflow
     <<",\"post_failure\":"<<facts.post_failure<<",\"max_depth\":"<<facts.max_depth<<",\"poisoned\":"<<flag(facts.poisoned)
     <<",\"running\":"<<flag(facts.running)<<",\"vdm_queries\":"<<group->vdm_queries()<<",\"reconciled_missing\":"<<group->reconciled_missing()
     <<",\"restore_exact\":"<<flag(success)<<",\"reason\":"<<quote(group->reason());log.record("summary",f.str());
    if(!success)return stop(group->reason());
    stage="SUBJECTIVE";
    print(L"\r\n三个手势已完成，已恢复到接受的 setup 前位置，并保留您手工调整后的尺寸。请评价顺滑度 A/B/C/D/E：\r\n");const auto grade=read_line(log,input,"subjective_grade");
    print(L"从 A/B/C 任意成员抓住并拖动时，是否感觉像同一个刚体？输入 YES / MOSTLY / NO：\r\n");const auto feel=read_line(log,input,"rigid_body_feel");
    if(!grade||grade->size()!=1||grade->front()<L'A'||grade->front()>L'E'||!feel||(*feel!=L"YES"&&*feel!=L"MOSTLY"&&*feel!=L"NO"))return stop("subjective_response_invalid");
    log.record("subjective",",\"grade\":"+quote(*grade)+",\"rigid_body_feel\":"+quote(*feel)+",\"input_source\":\"interactive_console\"");
    log.record("shutdown",",\"result\":\"PASS\",\"user_windows_closed\":false");
    print(L"\r\n证据已保留。未关闭 Explorer；请按独立 review / human seal 流程提交日志。\r\n");return log.healthy()?0:2;
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=4 || std::wstring_view(argv[1])!=L"--interactive-consent-test" || std::wstring_view(argv[2])!=L"--evidence-log") {
        std::cout<<"Usage: panebind-explorer-group-harness --interactive-consent-test --evidence-log NEW_FILE\n";return 2;}
    if(!console(GetStdHandle(STD_INPUT_HANDLE))||!console(GetStdHandle(STD_OUTPUT_HANDLE)))return 2;
    try{Log log{argv[3]};return run(log);}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 2;}
}
