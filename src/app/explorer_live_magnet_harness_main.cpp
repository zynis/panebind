#include "platform/windows/explorer/explorer_live_magnet_session.h"
#include "platform/windows/explorer/explorer_group_capture_json.h"
#include "platform/windows/console/sta_console_line_reader.h"
#include "platform/windows/text_encoding.h"
#include <sstream>
#include <iostream>
#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif
namespace e=panebind::platform::windows::explorer;
namespace w=panebind::platform::windows;
namespace b=panebind::core::behavior;
using panebind::core::geometry::Rect;
namespace {
const char* flag(bool value){return value?"true":"false";}
std::string quote(std::string_view s){return w::json_quote(s);}
std::string quote(std::wstring_view s){auto v=w::utf16_to_utf8(s);if(!v.value)throw std::runtime_error("invalid UTF16");return quote(*v.value);}
std::string rect(const Rect& r){return "["+std::to_string(r.left())+","+std::to_string(r.top())+","+std::to_string(r.right())+","+std::to_string(r.bottom())+"]";}
std::string edges(panebind::core::magnet::ParticipatingEdges e){return "["+std::to_string(e.left)+","+std::to_string(e.top)+","+std::to_string(e.right)+","+std::to_string(e.bottom)+"]";}
std::string snapshots(const e::detail::GroupSnapshots& values){
    std::ostringstream s;s<<'[';for(std::size_t i=0;i<3;++i){if(i)s<<',';const auto& v=values[i];
        s<<"{\"member\":"<<i<<",\"visible\":"<<rect(v.visible_rect)<<",\"positioning\":"<<rect(v.positioning_rect)
         <<",\"pid\":"<<v.process_id<<",\"tid\":"<<v.thread_id<<",\"dpi\":"<<v.dpi<<",\"monitor\":"<<quote(v.monitor_device_name)
         <<",\"work_area\":"<<rect(v.monitor_work_area)<<",\"image\":\"explorer.exe\",\"class\":"<<quote(v.window_class)
         <<",\"current_desktop\":"<<flag(v.on_current_virtual_desktop)<<",\"exact_location\":"<<flag(v.exact_test_location)
         <<",\"minimized\":"<<flag(v.minimized)<<",\"maximized\":"<<flag(v.maximized)<<'}';}s<<']';return s.str();
}
std::string topology(const e::GroupLayoutReadiness& r){
    std::ostringstream s;s<<"{\"relation_count\":"<<r.relation_count<<",\"ready\":"<<flag(r.ready)<<",\"pairs\":[";
    for(std::size_t i=0;i<3;++i){if(i)s<<',';const auto& p=r.pairs[i];s<<"{\"first\":"<<p.first<<",\"second\":"<<p.second<<",\"relation\":"<<flag(p.relation)
        <<",\"first_edge\":"<<static_cast<int>(p.first_edge)<<",\"second_edge\":"<<static_cast<int>(p.second_edge)<<",\"signed_gap\":"<<p.signed_gap<<",\"orthogonal_overlap\":"<<p.orthogonal_overlap<<'}';}
    s<<"],\"components\":[";for(std::size_t i=0;i<3;++i){if(i)s<<',';s<<'[';bool first=true;for(const auto m:r.components[i]){if(!first)s<<',';first=false;s<<m;}s<<']';}s<<"]}";return s.str();
}
std::string constraint(const std::optional<panebind::core::magnet::SatisfiedConstraint>& c){
    if(!c)return "null";return "{\"target_id\":"+quote(c->target.value())+",\"kind\":"+std::to_string(static_cast<int>(c->kind))+
        ",\"moving_edge\":"+std::to_string(static_cast<int>(c->moving_edge))+",\"target_edge\":"+std::to_string(static_cast<int>(c->target_edge))+
        ",\"delta\":"+std::to_string(c->delta)+",\"overlap\":"+std::to_string(c->predicted_overlap)+"}";
}
bool print(std::wstring_view s){DWORD n{};return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),s.data(),static_cast<DWORD>(s.size()),&n,nullptr)&&n==s.size();}
bool console(HANDLE h){DWORD mode{};return h&&h!=INVALID_HANDLE_VALUE&&GetFileType(h)==FILE_TYPE_CHAR&&GetConsoleMode(h,&mode);}
class Log {
public:
    explicit Log(const std::filesystem::path& path):file_(CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)){}
    ~Log(){if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
    bool record(std::string_view type,std::string_view fields={}){
        const auto line="{\"schema\":\"r1c4b/v1\",\"sequence\":"+std::to_string(++sequence_)+",\"type\":"+quote(type)+std::string(fields)+"}\n";
        DWORD n{};healthy_=healthy_&&file_!=INVALID_HANDLE_VALUE&&WriteFile(file_,line.data(),static_cast<DWORD>(line.size()),&n,nullptr)&&n==line.size();return healthy_;
    }
    bool healthy()const noexcept{return healthy_;}
private:HANDLE file_;std::uint64_t sequence_{};bool healthy_{true};
};
struct Work {Log* log;e::ExplorerLiveMagnetSession* live;};
std::optional<std::wstring> read(Log& log,w::console_input::StaConsoleLineReader& reader,std::string_view kind,e::ExplorerLiveMagnetSession* live=nullptr){
    Work work{&log,live};const auto pump=[](void* p)noexcept{auto& w=*static_cast<Work*>(p);return w.log->healthy()&&w.live->pump();};
    const auto result=reader.read(live?w::console_input::StaOwnerWork{&work,pump}:w::console_input::StaOwnerWork{});
    std::ostringstream s;s<<",\"kind\":"<<quote(kind)<<",\"result\":"<<quote(w::console_input::line_status_name(result.status))<<",\"owner\":"<<result.owner_thread
        <<",\"waits\":"<<result.wait_count<<",\"pumps\":"<<result.pump_count<<",\"messages\":"<<result.message_dispatch_count
        <<",\"events\":"<<result.console_input_event_count<<",\"mode_before\":"<<result.input_mode_before<<",\"mode_after\":"<<result.input_mode_after
        <<",\"modes_observed\":"<<flag(result.modes_observed)<<",\"mode_changed\":"<<flag(result.mode_changed)<<",\"error\":"<<result.error<<",\"live_pump\":"<<flag(live!=nullptr);
    if(!log.record("console_wait",s.str()))return {};
    if(live&&result.line&&(result.line->empty()||*result.line==L"Y"||*result.line==L"y")&&!live->pump())return {};
    return result.line;
}
void runtime(Log& log,const e::ExplorerLiveMagnetSession& live){
    std::array<std::size_t,3> logical_order{0,1,2};
    std::sort(logical_order.begin(),logical_order.end(),[&](auto a,auto b){return std::to_string(live.group().bindings()[a].window_id)<std::to_string(live.group().bindings()[b].window_id);});
    for(const auto& r:live.receipts()){
        std::ostringstream s;s<<",\"receipt\":"<<r.sequence<<",\"member\":"<<r.member_index<<",\"window_id\":"<<r.window_id<<",\"capability\":"<<r.capability_generation
            <<",\"hwnd\":"<<reinterpret_cast<std::uintptr_t>(r.native_source)<<",\"tid\":"<<r.native_thread<<",\"native_time\":"<<r.native_time
            <<",\"callback_qpc\":"<<r.callback_qpc<<",\"event\":"<<quote(r.kind==e::GroupEventKind::Start?"START":r.kind==e::GroupEventKind::End?"END":r.kind==e::GroupEventKind::Destroy?"DESTROY":"LOCATION")
            <<",\"ctrl_available\":"<<flag(r.ctrl.available)<<",\"ctrl\":"<<flag(r.ctrl.ctrl);log.record("receipt",s.str());
    }
    for(const auto& q:live.quanta()){
        std::ostringstream s;s<<",\"id\":"<<q.id<<",\"gesture\":"<<q.gesture<<",\"first\":"<<q.first<<",\"last\":"<<q.last<<",\"raw_receipts\":"<<q.receipts<<",\"coalesced\":"<<q.coalesced
            <<",\"solver_calls\":"<<q.solver_calls<<",\"corrections\":"<<q.corrections<<",\"owner_qpc\":"<<q.owner_qpc<<",\"end_qpc\":"<<q.end_qpc
            <<",\"active_inventory\":"<<q.inventory_active<<",\"active_manager_creates\":"<<q.manager_creates_active;log.record("quantum",s.str());
    }
    for(const auto& s:live.samples())log.record("sample",",\"gesture\":"+std::to_string(s.gesture)+",\"receipt\":"+std::to_string(s.receipt)+",\"source\":"+std::to_string(s.source)+
        ",\"provider_tick\":"+std::to_string(s.provider_tick)+",\"operation\":"+std::to_string(s.feedback_operation)+",\"raw\":"+rect(s.raw)+",\"result\":"+quote(s.result)+",\"callback_qpc\":"+std::to_string(s.callback_qpc)+",\"owner_qpc\":"+std::to_string(s.owner_qpc));
    for(const auto& op:live.operations()){
        const auto& c=op.command;const auto& n=op.native;
        const bool x=c.raw.left()!=c.proposal.corrected.left()||c.raw.right()!=c.proposal.corrected.right();
        const bool y=c.raw.top()!=c.proposal.corrected.top()||c.raw.bottom()!=c.proposal.corrected.bottom();
        std::ostringstream s;s<<",\"group\":"<<c.group<<",\"gesture\":"<<c.gesture<<",\"operation\":"<<c.operation<<",\"source\":"<<c.source<<",\"source_id\":"<<quote(c.source_id.value())
            <<",\"capability\":"<<c.capability<<",\"consent\":"<<c.consent<<",\"source_receipt\":"<<c.source_receipt<<",\"watermark\":"<<op.watermark<<",\"registration_tick\":"<<op.provider_tick
            <<",\"raw\":"<<rect(c.raw)<<",\"corrected\":"<<rect(c.proposal.corrected)<<",\"target_positioning\":"<<(n.target_positioning?rect(*n.target_positioning):"null")
            <<",\"dx\":"<<c.proposal.move_delta.x<<",\"dy\":"<<c.proposal.move_delta.y<<",\"edges\":"<<edges(c.proposal.edges)<<",\"selected_x\":"<<constraint(c.proposal.selected_x)<<",\"selected_y\":"<<constraint(c.proposal.selected_y)
            <<",\"axes\":"<<quote(x?(y?"XY":"X"):(y?"Y":"NONE"))<<",\"preflight\":"<<flag(n.all_preflight)<<",\"pending_registered\":"<<flag(n.pending_registered)
            <<",\"native_attempted\":"<<flag(n.native_attempted)<<",\"native_success\":"<<flag(n.native_success)<<",\"native_calls\":"<<(n.native_attempted?1:0)<<",\"flags\":"<<n.flags<<",\"error\":"<<n.error
            <<",\"exact\":"<<flag(n.exact)<<",\"actual\":"<<(n.actual?snapshots(*n.actual):"null")<<",\"before\":"<<snapshots(n.before)
            <<",\"receipt_qpc\":"<<op.trigger.callback_qpc<<",\"owner_qpc\":"<<op.owner_qpc<<",\"native_start_qpc\":"<<n.native_start_qpc<<",\"native_return_qpc\":"<<n.native_return_qpc<<",\"postverify_qpc\":"<<n.postverify_qpc
            <<",\"reason\":"<<quote(n.reason)
            <<",\"capture_failure\":"<<(n.capture_failure?e::detail::group_capture_json(*n.capture_failure):"null")
            <<",\"postverify\":"<<(n.postverify?w::operations::magnet_postverify_json(*n.postverify):"null")
            <<",\"postverify_failure\":"<<(n.postverify&&!n.exact?w::operations::magnet_postverify_json(*n.postverify):"null")
            <<",\"immediate_actual_visible\":"<<(n.immediate_visible?rect(*n.immediate_visible):"null")
            <<",\"immediate_actual_positioning\":"<<(n.immediate_positioning?rect(*n.immediate_positioning):"null")
            <<",\"immediate_positioning_error\":"<<n.immediate_positioning_error<<",\"immediate_visible_hresult\":"<<n.immediate_visible_error
            <<",\"immediate_capture_qpc\":"<<n.immediate_capture_qpc;
        // Link only observed, same-source receipts in this gesture; do not
        // reinterpret callback-time events as captured geometry or invent END.
        const e::GroupEventReceipt *next=nullptr,*location=nullptr,*end=nullptr;
        if(n.native_attempted)for(const auto& r:live.receipts()) {
            if(r.member_index!=c.source||r.sequence<=op.watermark||r.callback_qpc<=n.native_return_qpc)continue;
            if(r.kind==e::GroupEventKind::Start)break;
            if(!next)next=&r;
            if(!location&&r.kind==e::GroupEventKind::Location)location=&r;
            if(r.kind==e::GroupEventKind::End){end=&r;break;}
        }
        const auto receipt=[](const auto* r){return r?std::to_string(r->sequence):"null";};
        s<<",\"next_receipt_after_correction\":"<<receipt(next)<<",\"next_location_after_correction\":"<<receipt(location)
         <<",\"end_receipt_after_correction\":"<<receipt(end);log.record("correction",s.str());
        if(n.capture_failure)log.record("capture_diagnostic",",\"capture\":"+e::detail::group_capture_json(*n.capture_failure));
    }
    for(const auto& g:live.gestures()){
        const auto& c=g.counters;std::ostringstream s;
        s<<",\"gesture\":"<<g.generation<<",\"source\":"<<g.source<<",\"start\":"<<g.start_receipt<<",\"end\":"<<g.end_receipt<<",\"ctrl\":"<<flag(g.ctrl)
            <<",\"route\":"<<quote(b::magnet_route_name(g.route))<<",\"edges\":"<<edges(g.edges)<<",\"completed\":"<<flag(g.completed)<<",\"initial\":"<<snapshots(g.initial)<<",\"final\":"<<snapshots(g.final)
            <<",\"raw_receipts\":"<<g.raw_receipts<<",\"meaningful\":"<<c.meaningful<<",\"solver_calls\":"<<c.solver_calls<<",\"motion_suppressed\":"<<c.motion_suppressed
            <<",\"proposals\":"<<c.proposals<<",\"corrections\":"<<c.corrections<<",\"acknowledged\":"<<c.acknowledged<<",\"duplicates\":"<<c.duplicates<<",\"ambiguous\":"<<c.ambiguous<<",\"missing\":"<<c.missing
            <<",\"x_latches\":"<<c.x_latches<<",\"y_latches\":"<<c.y_latches<<",\"latch_releases\":"<<c.latch_releases<<",\"reason\":"<<quote(g.reason);log.record("gesture",s.str());
        if(g.completed)log.record("relation_graph",",\"gesture\":"+std::to_string(g.generation)+",\"snapshots\":"+snapshots(g.final)+",\"graph\":"+topology(g.topology));
    }
    for(const auto& g:live.group().gestures()){
        std::ostringstream s;s<<",\"gesture\":"<<g.gesture<<",\"source\":"<<g.source_member<<",\"start\":"<<g.start_sequence<<",\"end\":"<<g.end_sequence<<",\"batches\":"<<g.batches<<",\"exact\":"<<flag(g.exact)
            <<",\"roles_cleared\":"<<flag(g.roles_cleared)<<",\"pending_empty\":"<<flag(g.pending_empty)<<",\"group_ready\":"<<flag(g.group_ready)<<",\"initial\":"<<snapshots(g.initial)<<",\"final\":"<<snapshots(g.final);log.record("glue_gesture",s.str());
    }
    for(const auto& op:live.group().operations()){
        const auto& r=op.receipt;std::ostringstream s;s<<",\"gesture\":"<<op.gesture<<",\"batch\":"<<op.batch<<",\"source\":"<<op.source_member<<",\"source_receipt\":"<<op.source_sequence<<",\"watermark\":"<<op.watermark
            <<",\"source_visible\":"<<rect(op.source_visible)<<",\"pre_native_tick\":"<<op.pre_native_tick
            <<",\"preflight\":"<<flag(r.all_preflight)<<",\"pending_registered\":"<<flag(r.all_pending_registered)<<",\"native_attempted\":"<<flag(r.native.native_commit_attempted)<<",\"native_success\":"<<flag(r.native.succeeded)<<",\"exact\":"<<flag(r.all_postverify)
            <<",\"flags\":21,\"deferred\":"<<r.native.deferred<<",\"error\":"<<r.native.error<<",\"native_start_qpc\":"<<r.native.native_start_qpc<<",\"native_return_qpc\":"<<r.native.native_return_qpc<<",\"postverify_qpc\":"<<r.postverify_qpc<<",\"members\":[";
        bool first=true;for(const auto i:logical_order)if(r.targets[i]){if(!first)s<<',';first=false;s<<"{\"member\":"<<i<<",\"target\":"<<rect(*r.targets[i])<<",\"actual\":"<<(r.actual[i]?rect(r.actual[i]->visible_rect):"null")<<",\"positioning_target\":"<<(r.positioning_targets[i]?rect(*r.positioning_targets[i]):"null")<<",\"actual_positioning\":"<<(r.actual[i]?rect(r.actual[i]->positioning_rect):"null")<<'}';}s<<']';log.record("glue_batch",s.str());
    }
    for(const auto& f:live.group().feedback())log.record("glue_feedback",",\"gesture\":"+std::to_string(f.gesture)+",\"batch\":"+std::to_string(f.batch)+",\"member\":"+std::to_string(f.member)+",\"receipt\":"+std::to_string(f.sequence)+",\"result\":"+quote(f.result)+",\"observed\":"+rect(f.observed_visible));
}
bool relation(const e::detail::GroupSnapshots& s,std::size_t first,std::size_t second){for(const auto& p:e::group_layout_readiness(s).pairs)if(p.first==first&&p.second==second)return p.relation;return false;}
void print_graph(const e::detail::GroupSnapshots& snapshots){
    const auto graph=e::group_layout_readiness(snapshots);std::wstring text=L"\r\n当前实际 Relation Graph：\r\n";
    for(const auto& p:graph.pairs)text+=std::wstring(1,static_cast<wchar_t>(L'A'+p.first))+L"-"+std::wstring(1,static_cast<wchar_t>(L'A'+p.second))+
        (p.relation?L" YES":L" NO")+L", gap="+std::to_wstring(p.signed_gap)+L", overlap="+std::to_wstring(p.orthogonal_overlap)+L"\r\n";
    text+=L"relation_count="+std::to_wstring(graph.relation_count)+L"; component(A)=";
    for(const auto m:graph.components[0])text+=std::wstring(1,static_cast<wchar_t>(L'A'+m))+L" ";print(text+L"\r\n");
}
enum class Action { M1,M2,M3,R1,R2,R3,D1,Reconnect };
bool goal(Action action,const e::ExplorerLiveMagnetSession& live,std::size_t first_gesture,std::size_t first_op){
    const auto& s=live.current();bool move=false,resize_bottom=false,resize_top=false,detach=false,xy=false;
    for(std::size_t i=first_gesture;i<live.gestures().size();++i){const auto& g=live.gestures()[i];if(!g.completed||g.ctrl)continue;
        if(g.route==b::MagnetRoute::MagnetMove&&g.source==(action==Action::M1?2:1))move=true;
        if(g.route==b::MagnetRoute::MagnetResize&&g.source==1){resize_bottom|=g.edges==panebind::core::magnet::ParticipatingEdges{false,false,false,true};resize_top|=g.edges==panebind::core::magnet::ParticipatingEdges{false,true,false,false};}
        if(g.route==b::MagnetRoute::MagnetResize&&g.source==2&&g.edges==panebind::core::magnet::ParticipatingEdges{false,false,true,false}&&relation(g.initial,1,2)&&!relation(g.final,1,2))detach=true;
    }
    bool correction=false;for(std::size_t i=first_op;i<live.operations().size();++i){const auto& op=live.operations()[i];if(!op.native.exact)continue;
        const auto e=op.command.proposal.edges;
        if(action==Action::M1)correction|=op.command.source==2&&e==panebind::core::magnet::ParticipatingEdges{};
        if(action==Action::M2||action==Action::M3)correction|=op.command.source==1&&e==panebind::core::magnet::ParticipatingEdges{};
        if(action==Action::R2)correction|=op.command.source==1&&e==panebind::core::magnet::ParticipatingEdges{false,false,false,true};
        if(action==Action::R3)correction|=op.command.source==1&&e==panebind::core::magnet::ParticipatingEdges{false,true,false,false};
        xy|=op.command.source==1&&e==panebind::core::magnet::ParticipatingEdges{}&&op.command.proposal.move_delta.x!=0&&op.command.proposal.move_delta.y!=0;
    }
    switch(action){
    case Action::M1:return move&&correction&&relation(s,0,2);
    case Action::M2:return move&&correction&&relation(s,0,1);
    case Action::M3:return move&&xy&&relation(s,0,1)&&relation(s,0,2)&&relation(s,1,2)&&s[1].visible_rect.left()==s[0].visible_rect.right()&&s[1].visible_rect.bottom()==s[2].visible_rect.top();
    case Action::R1:return resize_bottom&&s[1].visible_rect.bottom()<s[2].visible_rect.top()&&relation(s,0,1);
    case Action::R2:return resize_bottom&&correction&&relation(s,1,2);
    case Action::R3:return resize_top&&correction&&relation(s,1,2)&&s[1].visible_rect.top()==s[0].visible_rect.top()&&s[1].visible_rect.bottom()==s[2].visible_rect.top();
    case Action::D1:return detach&&!relation(s,1,2);
    case Action::Reconnect:return e::group_layout_readiness(s).ready;
    }return false;
}
int run(Log& log,std::string_view sha){
    log.record("startup",",\"evidence_kind\":\"human_interactive\",\"implementation_sha\":"+quote(sha)+",\"owner_sta\":"+std::to_string(GetCurrentThreadId())+",\"qpc_frequency\":"+std::to_string(e::glue_qpc_frequency())+
        ",\"member_count\":3,\"attraction\":10,\"release\":16,\"speed_limit\":2000,\"screen_magnet\":false,\"window_magnet\":true,\"live_magnet\":true,\"glue_move\":true,\"glue_resize\":false,\"magnet_contract\":\"source_only_exact_v1\",\"console_contract\":\"preserved_mode_live_owner_v1\",\"postverify_contract\":\"placement_v1\"");
    if(!log.healthy())return 2;
    std::unique_ptr<e::ExplorerLiveMagnetSession> live;bool flushed=false;
    const auto stop=[&](std::string_view reason){if(live&&!flushed){runtime(log,*live);flushed=true;log.record("capture_diagnostic",",\"capture\":"+e::detail::group_capture_json(live->last_capture()));}log.record("shutdown",",\"result\":\"BLOCKED\",\"reason\":"+quote(reason));return 2;};
    w::console_input::StaConsoleLineReader reader{GetStdHandle(STD_INPUT_HANDLE),GetStdHandle(STD_OUTPUT_HANDLE)};
    e::ExplorerGroupSession::OwnedMembers members;
    for(std::size_t i=0;i<3;++i){GUID guid{};wchar_t nonce[64]{};
        if(CoCreateGuid(&guid)!=S_OK||!StringFromGUID2(guid,nonce,64))return stop("nonce_failed");
        const auto path=std::filesystem::temp_directory_path()/(std::wstring{L"PaneBind-R1C4B-"}+static_cast<wchar_t>(L'A'+i)+L"-"+nonce);if(!std::filesystem::create_directory(path))return stop("nonce_directory_failed");
        auto begin=e::ExplorerConsentProvisioning::begin(path);if(!begin.succeeded())return stop("target_baseline_failed");auto prompt=begin.provisioning->record_target_prompt();if(!prompt.succeeded())return stop("target_prompt_failed");
        log.record("target_prompt",",\"member\":"+std::to_string(i)+",\"nonce_id\":"+quote(nonce)+",\"baseline\":"+std::to_string(begin.facts.generations.baseline_generation)+",\"prompt\":"+std::to_string(prompt.generation));
        print(L"\r\n成员 "+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L"：请亲自新建一个 Explorer 窗口（不要复用已有窗口），打开：\r\n"+path.native()+L"\r\n完成后回控制台按 Enter；其它输入取消。\r\n");
        const auto answer=read(log,reader,"target_confirmation");if(!answer||!answer->empty())return stop("target_declined");
        auto confirmed=begin.provisioning->confirm_user_target();if(!confirmed.succeeded())return stop("target_confirmation_failed");const auto& f=confirmed.facts;const auto& g=f.generations;
        log.record("target_confirmed",",\"member\":"+std::to_string(i)+",\"confirmation\":"+std::to_string(g.target_confirmation_generation)+",\"eligibility\":"+std::to_string(g.eligibility_generation)+",\"token\":"+std::to_string(g.token_generation)+",\"baseline_excluded\":"+flag(f.baseline_exclusion_complete)+",\"unique\":"+flag(f.unique_new_target)+",\"exact_location\":"+flag(f.exact_target_location)+",\"token_issued\":"+flag(f.token_issued));members[i]=std::move(confirmed.session);
    }
    print(L"\r\nC4B 独立授权：只在您主动 Move/Resize 这三扇新窗口时，修正最后几像素以磁吸对齐；Ctrl+Move 使用 Glue；最后恢复到您稍后接受的拓扑基线。\r\n不会导航、关闭、改 Z-order、激活窗口，不控制已有 Explorer 或其它应用。Ctrl+Resize 尚未实现。\r\n输入 Y 后 Enter 同意：\r\n");
    const auto consent=read(log,reader,"live_consent");if(!consent||(*consent!=L"Y"&&*consent!=L"y"))return stop("live_consent_declined");
    log.record("live_consent",",\"confirmed\":true,\"source_correction\":true,\"glue_move\":true,\"accepted_restore\":true");
    live=e::ExplorerLiveMagnetSession::create(std::move(members),e::ExplorerLiveMagnetSession::Consent::Confirmed);if(!live)return stop("live_binding_failed");
    for(std::size_t i=0;i<3;++i){const auto& b=live->group().bindings()[i];std::ostringstream s;s<<",\"member\":"<<i<<",\"group\":"<<live->group().generation()<<",\"window_id\":"<<b.window_id<<",\"capability\":"<<b.capability_generation<<",\"consent\":"<<b.consent_generation<<",\"hwnd\":"<<reinterpret_cast<std::uintptr_t>(b.window)<<",\"pid\":"<<b.process_id<<",\"tid\":"<<b.thread_id;log.record("binding",s.str());}
    log.record("binding_snapshot",",\"snapshots\":"+snapshots(live->current()));
    print(L"\r\n磁吸已开启（屏幕边缘吸附关闭）。请先手工把三窗适当缩小，留出同一工作区内的拖动空间；大小不必相等。窗口活动在等待 Enter 时仍实时处理。\r\n准备好后按 Enter；Q 取消。\r\n");
    auto answer=read(log,reader,"prepare",live.get());if(!answer||!answer->empty()||!live->healthy())return stop(live->healthy()?"preparation_cancelled":live->reason());
    const std::array<std::pair<Action,std::wstring_view>,8> actions{{
        {Action::M1,L"M1：把 C 粗略拖到 A 下方附近，看到自动贴上后松开。"},
        {Action::M2,L"M2：把 B 粗略拖到 A 右边附近，看到自动贴上后松开。"},
        {Action::M3,L"M3：让 C 的右边适当伸到 B 下方；把 B 明显拖开，再斜向拖回 A/C 夹角，尝试同时横向、纵向吸附。"},
        {Action::R1,L"R1：不按 Ctrl，把 B 的下边向上缩短；保留 B 在 A 右侧。"},
        {Action::R2,L"R2：不按 Ctrl，把 B 下边向下拉近 C 上边，看到它自动贴上。"},
        {Action::R3,L"R3：先把 B 上边向下缩短一点，再向上拉近 A 上边；保持 B 下边与 C 上边的接触。"},
        {Action::D1,L"D1：把 C 的右边向左缩，直到 C 不再延伸到 B 下方，让 B/C 关系自然解除。"},
        {Action::Reconnect,L"请用普通 Move/Resize 和磁吸重新组成任意三窗连通布局，留出整组移动空间。"}}};
    for(std::size_t index=0;index<actions.size();++index){bool passed=false;
        for(int attempt=1;attempt<=5&&!passed;++attempt){const auto first_gesture=live->gestures().size(),first_op=live->operations().size();
            print(L"\r\n"+std::wstring(actions[index].second)+L"\r\n可以反复手工调整；完成后回控制台按 Enter 检查。无需测像素。Q 取消。\r\n");
            const auto reply=read(log,reader,"magnet_action",live.get());if(!reply||!reply->empty()||!live->healthy())return stop(live->healthy()?"action_cancelled":live->reason());
            passed=live->idle()&&goal(actions[index].first,*live,first_gesture,first_op);
            print_graph(live->current());
            log.record("action",",\"action\":"+std::to_string(index)+",\"attempt\":"+std::to_string(attempt)+",\"first_gesture\":"+std::to_string(first_gesture+1)+",\"last_gesture\":"+std::to_string(live->gestures().size())+",\"passed\":"+flag(passed)+",\"snapshots\":"+snapshots(live->current())+",\"graph\":"+topology(e::group_layout_readiness(live->current())));
            if(!passed)print(L"目标尚未达到（可能只命中单轴，或窗口需要再缩小/调整）。请再尝试；本步骤最多 5 次检查，不重新绑定。\r\n");
        }if(!passed)return stop("action_retry_limit");
    }
    print_graph(live->current());
    print(L"\r\n当前三窗已连通。输入 Y 接受当前位置和大小作为最后恢复基线；其它输入取消。\r\n");
    answer=read(log,reader,"topology_acceptance",live.get());if(!answer||(*answer!=L"Y"&&*answer!=L"y"))return stop("topology_declined");
    if(!live->accept_topology())return stop(live->healthy()?"topology_not_ready":live->reason());
    log.record("accepted",",\"confirmed\":true,\"snapshots\":"+snapshots(*live->accepted_baseline())+",\"graph\":"+topology(e::group_layout_readiness(*live->accepted_baseline()))+",\"capture_qpc\":"+std::to_string(e::glue_qpc_now()));
    for(std::size_t i=0;i<3;++i){const auto count=live->group().gestures().size();
        print(L"\r\nG"+std::to_wstring(i+1)+L"：先按住 Ctrl，再拖成员 "+std::wstring(1,static_cast<wchar_t>(L'A'+i))+L" 的标题栏约 1 秒；保持整组在工作区内，松鼠标后回控制台按 Enter。不要 Ctrl+Resize。\r\n");
        const auto reply=read(log,reader,"glue_action",live.get());if(!reply||!reply->empty()||!live->healthy())return stop(live->healthy()?"glue_cancelled":live->reason());
        const auto& gs=live->group().gestures();if(gs.size()!=count+1||gs.back().source_member!=i||!gs.back().exact||!gs.back().batches)return stop("glue_action_incomplete");
        log.record("glue_action",",\"source\":"+std::to_string(i)+",\"passed\":true");
    }
    if(!live->restore())return stop(live->reason());
    runtime(log,*live);flushed=true;
    const auto facts=live->group().event_facts();const auto active=static_cast<std::size_t>(e::ConsentValidationPhase::Active);
    log.record("summary",",\"restore_exact\":true,\"final\":"+snapshots(live->current())+",\"raw_receipts\":"+std::to_string(facts.accepted)+",\"overflow\":"+std::to_string(facts.overflow)+",\"post_failure\":"+std::to_string(facts.post_failure)+",\"max_queue\":"+std::to_string(facts.max_depth)+",\"active_inventory\":"+std::to_string(live->audit().inventory_calls[active])+",\"active_manager_creates\":"+std::to_string(live->audit().manager_creates[active])+",\"glue_missing\":"+std::to_string(live->group().reconciled_missing())+",\"vdm_queries\":"+std::to_string(live->group().vdm_queries()));
    const std::array<std::pair<std::string_view,std::wstring_view>,5> questions{{{"magnet_feel",L"Magnet 手感 A/B/C/D/E："},{"too_sticky",L"磁吸是否过粘 YES/NO："},{"misses",L"是否漏吸 YES/NO："},{"visible_jitter",L"是否有明显抖动 YES/NO："},{"rigid_body_feel",L"Ctrl 组移动是否像刚体 YES/MOSTLY/NO："}}};
    for(std::size_t i=0;i<questions.size();++i){print(std::wstring(questions[i].second)+L"\r\n");const auto reply=read(log,reader,questions[i].first);
        if(!reply||(i==0?(reply->size()!=1||(*reply)[0]<L'A'||(*reply)[0]>L'E'):(*reply!=L"YES"&&*reply!=L"NO"&&(i!=4||*reply!=L"MOSTLY"))))return stop("subjective_invalid");
        log.record("subjective",",\"question\":"+quote(questions[i].first)+",\"answer\":"+quote(*reply));
    }
    log.record("shutdown",",\"result\":\"PASS\",\"user_windows_closed\":false");print(L"\r\n证据已保存，窗口未关闭。等待独立人工验收。\r\n");return log.healthy()?0:2;
}
}
int wmain(int argc,wchar_t** argv){
#ifdef NDEBUG
    constexpr bool debug=false;
#else
    constexpr bool debug=true;
#endif
    if(argc==2&&std::wstring_view(argv[1])==L"--build-identity"){std::cout<<"{\"implementation_sha\":"<<quote(PANEBIND_BUILD_SHA)<<",\"debug\":"<<flag(debug)<<"}\n";return 0;}
    if(argc!=6||std::wstring_view(argv[1])!=L"--interactive-consent-test"||std::wstring_view(argv[2])!=L"--evidence-log"||std::wstring_view(argv[4])!=L"--implementation-sha")return 2;
    auto sha=w::utf16_to_utf8(argv[5]);if(!debug||!sha.value||*sha.value!=PANEBIND_BUILD_SHA||sha.value->size()!=40)return 2;
    if(!console(GetStdHandle(STD_INPUT_HANDLE))||!console(GetStdHandle(STD_OUTPUT_HANDLE)))return 2;
    try{Log log{argv[3]};return run(log,*sha.value);}catch(const std::exception&){std::cerr<<"C4B harness failed; preserve incomplete evidence\n";return 2;}
}
