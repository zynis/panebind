#include "platform/windows/explorer/explorer_group_session.h"
#include "platform/windows/text_encoding.h"
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
std::optional<std::wstring> read_line(){
    if(!console(GetStdHandle(STD_INPUT_HANDLE)))return std::nullopt;
    wchar_t buffer[256]{};DWORD count{};
    if(!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE),buffer,255,&count,nullptr)||count==255)return std::nullopt;
    std::wstring line(buffer,count);while(!line.empty()&&(line.back()==L'\r'||line.back()==L'\n'))line.pop_back();return line;
}
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
         <<",\"phase\":"<<quote(op.gesture?"active":n==0?"setup":"restore")<<",\"source_member\":"<<op.source_member
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
    f<<"],\"work_area\":"<<rect(preview.work_area)<<",\"required_width\":"<<preview.required_width
     <<",\"required_height\":"<<preview.required_height<<",\"width_deficit\":"<<preview.readiness.width_deficit
     <<",\"height_deficit\":"<<preview.readiness.height_deficit<<",\"group_generation\":"<<a.group_generation
     <<",\"gesture_generation\":"<<a.gesture_generation<<",\"native_apply_count\":"<<a.native_apply_count
     <<",\"hdwp_begin_count\":"<<a.hdwp_begin_count<<",\"event_source_running\":"<<flag(a.event_source_running)
     <<",\"pending_count\":"<<a.pending_count<<",\"leader_present\":"<<flag(a.leader_present)
     <<",\"owner_thread\":"<<flag(a.owner_thread)<<",\"capture_qpc\":"<<preview.capture_qpc;
    return log.record(type,f.str());
}
bool print_readiness(const e::GroupReadinessPreview& p) {
    const auto& area=p.work_area;
    std::wstring text=L"\r\nReadiness #"+std::to_wstring(p.attempt)+(p.setup_check?L"（setup fresh check）":L"（live preview）")+
        L"\r\n工作区："+std::to_wstring(area.right()-area.left())+L" × "+std::to_wstring(area.bottom()-area.top())+L" px\r\n";
    for(std::size_t i=0;i<3;++i)text+=L"Member "+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L": "+
        std::to_wstring(p.member_sizes[i][0])+L" × "+std::to_wstring(p.member_sizes[i][1])+L" px\r\n";
    text+=L"L-shape required："+std::to_wstring(p.required_width)+L" × "+std::to_wstring(p.required_height)+
        L" px\r\n宽度 = max(A宽+B宽, C宽)；高度 = max(A高+C高, B高)。\r\n"+
        L"宽度至少还需减少 "+std::to_wstring(p.readiness.width_deficit)+L" px；高度至少还需减少 "+
        std::to_wstring(p.readiness.height_deficit)+L" px。\r\n";
    text+=p.readiness.ready?L"结果：FIT。\r\n":L"结果：NOT FIT（"+std::wstring(p.reason.begin(),p.reason.end())+L"）。\r\n";
    if(!p.readiness.ready)text+=L"以上是最小缺口；建议额外留出移动余量，避免三窗恰好填满工作区。\r\n";
    if(!p.readiness.ready) {
        const auto area_width=area.right()-area.left(),area_height=area.bottom()-area.top();
        const auto ab_width=p.member_sizes[0][0]+p.member_sizes[1][0];
        const auto ac_height=p.member_sizes[0][1]+p.member_sizes[2][1];
        if(ab_width>area_width)text+=L"请将 A/B 的合计宽度至少缩小 "+std::to_wstring(ab_width-area_width)+L" px。\r\n";
        if(p.member_sizes[2][0]>area_width)text+=L"请将 C 的宽度至少缩小 "+std::to_wstring(p.member_sizes[2][0]-area_width)+L" px。\r\n";
        if(ac_height>area_height)text+=L"请将 A/C 的合计高度至少缩小 "+std::to_wstring(ac_height-area_height)+L" px。\r\n";
        if(p.member_sizes[1][1]>area_height)text+=L"请将 B 的高度至少缩小 "+std::to_wstring(p.member_sizes[1][1]-area_height)+L" px。\r\n";
        if(p.reason=="B_C_overlap_for_l_shape" || p.reason=="required_edges_mismatch")
            text+=L"为避免 B/C 额外接触或重叠，可手工缩小至 B 的高度不超过 A、C 的宽度不超过 A。\r\n";
    }
    return print(text);
}
int run(Log& log){
    log.record("startup",",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"qpc_frequency\":"+std::to_string(e::glue_qpc_frequency())+",\"member_count\":3,\"interactive_console\":true,\"readiness_contract\":\"live_preview_accepted_baseline_v1\"");
    const auto stop=[&](std::string_view reason){log.record("shutdown",",\"result\":\"BLOCKED\",\"reason\":"+quote(reason));return 2;};
    e::ExplorerGroupSession::OwnedMembers members;
    for(std::size_t i=0;i<3;++i){
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
        const auto line=read_line();if(!line||!line->empty())return stop("target_declined");
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
    if(!print(L"\r\n三个成员均已单独确认。是否授权三窗口纯平移 L 形布局、连续 Ctrl+Move 和最后精确恢复？\r\n如尺寸不适合，须由您手工缩小；最终恢复到调整完成后、setup 前接受的位置，保留调整后的尺寸。\r\nPaneBind 不会 resize、关闭窗口或改变 Z-order。输入 Y 后 Enter 同意：\r\n"))return stop("console_failed");
    const auto consent=read_line();if(!consent||(*consent!=L"Y"&&*consent!=L"y"))return stop("group_declined");
    if(!log.record("group_consent",",\"confirmed\":true,\"input_source\":\"interactive_console\""))return stop("evidence_write_failed");
    auto group=e::ExplorerGroupSession::create(std::move(members));if(!group)return stop("group_binding_failed");
    for(std::size_t i=0;i<3;++i){const auto& binding=group->bindings()[i];
        std::ostringstream f;f<<",\"member\":"<<i<<",\"window_id\":"<<binding.window_id<<",\"capability_generation\":"<<binding.capability_generation
         <<",\"consent_generation\":"<<binding.consent_generation<<",\"hwnd\":"<<reinterpret_cast<std::uintptr_t>(binding.window)
         <<",\"pid\":"<<binding.process_id<<",\"tid\":"<<binding.thread_id<<",\"group\":"<<group->generation();log.record("binding",f.str());}
    if(!log.record("binding_snapshot",",\"snapshots\":"+snapshots(group->binding_snapshots())))return stop("evidence_write_failed");
    bool success=false;
    for(;;) {
        auto preview=group->preview_readiness();
        if(!record_readiness(log,"readiness_preview",preview))return stop("evidence_write_failed");
        if(!preview.valid)return stop(preview.reason);
        if(!print_readiness(preview))return stop("console_failed");
        if(preview.readiness.ready) {
            success=group->setup(); // fresh capture/recompute, not preview reuse
            preview=*group->last_readiness_preview();
            if(!record_readiness(log,"readiness_preview",preview))return stop("evidence_write_failed");
            if(group->accepted_readiness()) {
                if(!record_readiness(log,"readiness_accepted",*group->accepted_readiness()))return stop("evidence_write_failed");
                break; // runtime errors retain the usual batch/summary evidence
            }
            if(!preview.valid || !group->healthy())return stop(preview.reason);
            if(!print_readiness(preview))return stop("console_failed");
        }
        if(!print(L"请按以上提示手工缩小一个或多个 Explorer；不要导航、关闭窗口或换 monitor/DPI。\r\nPaneBind 未执行任何 native movement。调整后回到控制台按 Enter 重新检查；输入 Q 取消。\r\n"))return stop("console_failed");
        for(;;) {
            const auto line=read_line();
            if(!line)return stop("readiness_input_failed");
            if(line->empty())break;
            if(*line==L"Q" || *line==L"q")return stop("readiness_cancelled");
            if(!print(L"仅支持 Enter 重新检查，或 Q 取消。\r\n"))return stop("console_failed");
        }
    }
    for(std::size_t i=0;success&&i<3;++i){
        print(L"\r\nGesture "+std::to_wstring(i+1)+L"：先按住 Ctrl，再拖动成员 "+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L" 的标题栏约 1 秒，再松鼠标。保持整组在工作区内。\r\n");
        success=group->run_gesture(i,std::chrono::seconds{120});
    }
    if(success)success=group->restore();
    runtime_records(log,*group);
    const auto facts=group->event_facts();
    std::ostringstream f;f<<",\"accepted\":"<<facts.accepted<<",\"ignored\":"<<facts.ignored<<",\"overflow\":"<<facts.overflow
     <<",\"post_failure\":"<<facts.post_failure<<",\"max_depth\":"<<facts.max_depth<<",\"poisoned\":"<<flag(facts.poisoned)
     <<",\"running\":"<<flag(facts.running)<<",\"vdm_queries\":"<<group->vdm_queries()<<",\"reconciled_missing\":"<<group->reconciled_missing()
     <<",\"restore_exact\":"<<flag(success)<<",\"reason\":"<<quote(group->reason());log.record("summary",f.str());
    if(!success)return stop(group->reason());
    print(L"\r\n三个手势已完成，已恢复到接受的 setup 前位置，并保留您手工调整后的尺寸。请评价顺滑度 A/B/C/D/E：\r\n");const auto grade=read_line();
    print(L"从 A/B/C 任意成员抓住并拖动时，是否感觉像同一个刚体？输入 YES / MOSTLY / NO：\r\n");const auto feel=read_line();
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
