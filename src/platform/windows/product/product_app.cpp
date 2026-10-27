#include "product_app.h"
#include "input_service.h"
#include "settings_window.h"
#include "visual_recovery.h"
#include "window_writer.h"
#include "tray_callback_route.h"
#include "guest_capture_scope.h"
#include "guest_queue_failure_command.h"
#include "core/v1/layout.h"
#include "core/v1/settings.h"
#include "core/v1/gesture.h"
#include "core/v1/caption_hit_guard.h"
#include "core/v1/visual_ownership.h"
#include "core/geometry/magnet_constraint_solver.h"
#include "core/movement/translation.h"
#include "core/behavior/move_handoff_gate.h"
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <wtsapi32.h>
#include <dwmapi.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <sstream>
#ifndef PANEBIND_BUILD_SHA
#define PANEBIND_BUILD_SHA "unknown"
#endif
namespace panebind::platform::windows::product {
namespace v=core::v1;
namespace g=core::geometry;
namespace m=core::magnet;
namespace o=panebind::platform::windows::operations;
namespace fs=std::filesystem;
namespace {
constexpr wchar_t app_class[]=L"PaneBind.V1.Application";
constexpr UINT tray_message=WM_APP+30, external_message=WM_APP+31, queued_command_message=WM_APP+32;
constexpr UINT guest_queue_failure_message=WM_APP+33;
constexpr UINT guest_callback_owner_stop_message=WM_APP+34;
using guest::GuestQueueFailureRequest;
using guest::parse_guest_queue_failure;
using guest::parse_guest_callback_owner_stop;
constexpr UINT menu_pause=500,menu_settings=501,menu_exclude=502,menu_help=503,menu_diagnostic=504,menu_exit=505;
const char* shield_event_name(o::GestureShieldEventKind kind){
    using K=o::GestureShieldEventKind;
    switch(kind){
    case K::RawMouse:return "raw_mouse";case K::LegacyLeftUp:return "legacy_left_up";
    case K::IsolationReady:return "isolation_ready";case K::IsolationGone:return "isolation_gone";
    case K::CaptureReady:return "capture_ready";case K::CaptureFailure:return "capture_failure";
    case K::CaptureLost:return "capture_lost";case K::CaptureReleased:return "capture_released";
    case K::ThreadAssociated:return "thread_associated";case K::ThreadDetached:return "thread_detached";
    case K::ThreadAssociationFailure:return "thread_association_failure";
    case K::ThreadAssociationAttempt:return "thread_association_attempt";
    case K::ThreadAssociationPrepared:return "thread_association_prepared";
    case K::CaptureReleaseAttempt:return "capture_release_attempt";
    case K::HotkeyStop:return "hotkey_stop";case K::Deadline:return "deadline";
    case K::ContextLost:return "context_lost";case K::ResourceFailure:return "resource_failure";
    case K::OverlayMouseRoute:return "overlay_mouse_route";case K::LegacyLeftUpRejected:return "legacy_up_rejected";
    }return "unknown";
}
std::string mouse_up_callback_json(const MouseUpCallbackSnapshot& value){
    if(!value.known)return "{\"snapshot_known\":false,\"callback_count\":null,\"last\":null}";
    const auto prefix="{\"snapshot_known\":true,\"callback_count\":"+std::to_string(value.callback_count)+",\"last\":";
    if(!value.callback_count)return prefix+"null}";
    return prefix+"{\"hook_observed_qpc\":"+std::to_string(value.hook_observed_qpc)+
        ",\"button\":"+std::to_string(value.button)+",\"generation_at_entry\":"+std::to_string(value.generation_at_entry)+
        ",\"consumed_before\":"+(value.consumed_before?"true":"false")+",\"aborted_before\":"+(value.aborted_before?"true":"false")+
        ",\"matching_branch_taken\":"+(value.matching_branch_taken?"true":"false")+
        ",\"push_attempted\":"+(value.push_attempted?"true":"false")+",\"push_succeeded\":"+(value.push_succeeded?"true":"false")+"}}";
}
std::wstring wide(std::string_view s){
    if(s.empty())return {};const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n)return {};std::wstring r(static_cast<std::size_t>(n),L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n);return r;
}
std::string utf8(std::wstring_view s){
    if(s.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(!n)return {};std::string r(static_cast<std::size_t>(n),' ');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),r.data(),n,nullptr,nullptr);return r;
}
std::wstring exe_path(){std::wstring p(32768,L' ');p.resize(GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size())));return p;}
std::string rectangle(const v::Rect& r){return "["+std::to_string(r.left())+","+std::to_string(r.top())+","+std::to_string(r.right())+","+std::to_string(r.bottom())+"]";}
std::string escaped(std::string_view s){std::string out;for(const unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=static_cast<char>(c);}else if(c<32)out+=' ';else out+=static_cast<char>(c);}return out;}
std::int64_t dip(int n,UINT dpi){return (static_cast<std::int64_t>(n)*dpi+48)/96;}
constexpr bool drag_alpha_completion_failed(bool issued,bool known,bool succeeded,bool exact){
    return issued&&known&&(!succeeded||!exact);
}
static_assert(drag_alpha_completion_failed(true,true,false,false));
static_assert(drag_alpha_completion_failed(true,true,true,false));
static_assert(!drag_alpha_completion_failed(true,true,true,true));
static_assert(!drag_alpha_completion_failed(false,true,false,false));
static_assert(!drag_alpha_completion_failed(true,false,false,false));
static_assert(!drag_alpha_completion_failed(false,false,false,false));
v::Edges edges(int region){switch(region){case 1:return {true,false,false,false};case 2:return {false,false,true,false};
case 3:return {false,true,false,false};case 4:return {false,false,false,true};case 5:return {true,true,false,false};
case 6:return {false,true,true,false};case 7:return {true,false,false,true};case 8:return {false,false,true,true};default:return {};}}
bool is_resize(v::Edges e){return e.left||e.right||e.top||e.bottom;}
struct FeedbackLine {
    v::Point first,last;
    bool operator==(const FeedbackLine&)const=default;
};
constexpr std::optional<FeedbackLine> selected_feedback_edge(v::Rect actual,v::Rect target,g::Edge moving,g::Edge matched){
    if(actual.empty()||target.empty()||!g::edges_share_axis(moving,matched)||
        g::edge_coordinate(actual,moving)!=g::edge_coordinate(target,matched))return {};
    const auto coordinate=g::edge_coordinate(actual,moving);
    return moving==g::Edge::Left||moving==g::Edge::Right?
        FeedbackLine{{coordinate,actual.top()},{coordinate,actual.bottom()}}:
        FeedbackLine{{actual.left(),coordinate},{actual.right(),coordinate}};
}
constexpr std::optional<FeedbackLine> shared_feedback_contact(const v::layout_detail::EdgeSegment& first,
    const v::layout_detail::EdgeSegment& second,bool x){
    const auto start=std::max(first.start,second.start),end=std::min(first.end,second.end);
    if(first.index==second.index||first.low==second.low||first.coordinate!=second.coordinate||start>=end)return {};
    return x?FeedbackLine{{first.coordinate,start},{first.coordinate,end}}:
        FeedbackLine{{start,first.coordinate},{end,first.coordinate}};
}
std::vector<FeedbackLine> shared_feedback_edges(std::span<const v::Item> actual,v::Key source,v::Edges active,std::int64_t tolerance){
    const auto found=std::find_if(actual.begin(),actual.end(),[&](const auto& item){return item.id==source;});
    if(found==actual.end())return {};
    const auto index=static_cast<std::size_t>(found-actual.begin());std::vector<FeedbackLine> result;
    const auto axis=[&](bool x,bool low){
        // Reuse the production layout's contiguous shared-line selection, but
        // render only contacts ALREADY present in actual geometry. A tolerance
        // gap or proposed line is not drawn as a successful shared edge.
        const auto selected=v::layout_detail::resize_axis(actual,index,x,low,0,tolerance,true);
        if(!selected.accepted)return;
        for(std::size_t i=0;i<selected.edges.size();++i)for(std::size_t j=i+1;j<selected.edges.size();++j)
            if(const auto line=shared_feedback_contact(selected.edges[i],selected.edges[j],x);
                line&&std::find(result.begin(),result.end(),*line)==result.end())result.push_back(*line);
    };
    if(active.left||active.right)axis(true,active.left);
    if(active.top||active.bottom)axis(false,active.top);
    return result;
}
static_assert(selected_feedback_edge({10,20,110,100},{110,50,210,130},g::Edge::Right,g::Edge::Left)==
    std::optional<FeedbackLine>{{{110,20},{110,100}}});
static_assert(!selected_feedback_edge({10,20,109,100},{110,50,210,130},g::Edge::Right,g::Edge::Left));
static_assert(!selected_feedback_edge({10,20,110,100},{10,110,210,130},g::Edge::Left,g::Edge::Top));
static_assert(shared_feedback_contact({0,false,110,20,100},{1,true,110,50,130},true)==
    std::optional<FeedbackLine>{{{110,50},{110,100}}});
static_assert(!shared_feedback_contact({0,false,110,20,50},{1,true,110,50,130},true));
static_assert(!shared_feedback_contact({0,false,110,20,100},{1,true,111,50,130},true));
v::Edges stretch_edges(v::Action action){
    return {action==v::Action::StretchLeft||action==v::Action::StretchHorizontal||action==v::Action::StretchAll,
        action==v::Action::StretchTop||action==v::Action::StretchVertical||action==v::Action::StretchAll,
        action==v::Action::StretchRight||action==v::Action::StretchHorizontal||action==v::Action::StretchAll,
        action==v::Action::StretchBottom||action==v::Action::StretchVertical||action==v::Action::StretchAll};
}
std::wstring action_label(v::Action a){
    constexpr std::array<const wchar_t*,37> labels{
        L"不执行",L"最小化",L"最大化",L"恢复",L"居中",L"置于后方",
        L"向左微移",L"向右微移",L"向上微移",L"向下微移",L"减小宽度",L"增加宽度",L"减小高度",L"增加高度",
        L"移到最近相邻显示器",L"切换置顶",L"降低不透明度",L"提高不透明度",
        L"左半屏",L"右半屏",L"上半屏",L"下半屏",L"左上四分屏",L"右上四分屏",L"左下四分屏",L"右下四分屏",
        L"向左拉伸",L"向右拉伸",L"向上拉伸",L"向下拉伸",L"水平双向拉伸",L"垂直双向拉伸",L"双轴拉伸",
        L"撤销布局",L"暂停／恢复",L"打开设置",L"退出"};
    static_assert(labels.size()==v::settings_detail::action_names.size());
    const auto index=static_cast<std::size_t>(a);
    return index<labels.size()?labels[index]:L"不支持的操作";
}
class Log {
    fs::path path_;std::uint64_t sequence_{};
public:
    void open(fs::path p){path_=std::move(p);}
    void event(std::string_view type,std::string fields={}){
        if(path_.empty())return;
        std::error_code ec;
        if(fs::exists(path_,ec)&&fs::file_size(path_,ec)>512*1024){auto old=path_;old+=L".1";fs::remove(old,ec);fs::rename(path_,old,ec);}
        std::ofstream out(path_,std::ios::app|std::ios::binary);
        if(out)out<<"{\"schema\":\"panebind-v1/1\",\"source_sha\":\""<<PANEBIND_BUILD_SHA<<"\",\"sequence\":"<<++sequence_
            <<",\"process_id\":"<<GetCurrentProcessId()<<",\"tick\":"<<GetTickCount64()<<",\"type\":\""<<escaped(type)<<"\""<<(fields.empty()?"":",")<<fields<<"}\n";
    }
    const fs::path& path()const{return path_;}
};
bool read_file(const fs::path& p,std::string& text){std::error_code ec;if(!fs::exists(p,ec)||fs::file_size(p,ec)>262144)return false;
    std::ifstream in(p,std::ios::binary);text.assign(std::istreambuf_iterator<char>(in),{});return in.good()||in.eof();}
bool atomic_save(const fs::path& p,std::string_view text){
    if(text.empty()||text.size()>262144)return false;
    const fs::path temp=p.wstring()+L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;DWORD actual{};
    const bool ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&actual,nullptr)&&actual==text.size()&&FlushFileBuffers(file);
    CloseHandle(file);if(!ok){DeleteFileW(temp.c_str());return false;}
    if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES){if(ReplaceFileW(p.c_str(),temp.c_str(),nullptr,REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr))return true;}
    else if(MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileW(temp.c_str());return false;
}
struct Visual {
    Snapshot identity;v::VisualOwnership ownership;
    static void observe_owned(v::VisualOwnership& record,const v::VisualState& actual){
        // An unreadable layered tuple is not evidence that another writer
        // replaced our alpha. Topmost remains independently observable.
        if(record.changed_alpha&&actual.layered&&!actual.attributes_known){
            if(record.changed_top&&!v::owns_top(record,actual))record.changed_top=false;
            if(!record.changed_top)record.original.topmost=record.applied.topmost=actual.topmost;
        }else v::observe_visual(record,actual);
    }
    static std::string fields(const v::VisualState& value){return "{\"layered\":"+std::string(value.layered?"true":"false")+
        ",\"attributes_known\":"+(value.attributes_known?"true":"false")+",\"alpha\":"+std::to_string(value.alpha)+
        ",\"color\":"+std::to_string(value.color)+",\"flags\":"+std::to_string(value.flags)+
        ",\"topmost\":"+(value.topmost?"true":"false")+"}";}
    static std::optional<v::VisualState> read(HWND hwnd){
        SetLastError(0);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        if(!ex&&GetLastError())return {};
        v::VisualState value;value.topmost=(ex&WS_EX_TOPMOST)!=0;value.layered=(ex&WS_EX_LAYERED)!=0;
        if(value.layered){COLORREF color{};BYTE alpha{};DWORD flags{};
            value.attributes_known=GetLayeredWindowAttributes(hwnd,&color,&alpha,&flags)!=FALSE;
            if(value.attributes_known){value.alpha=alpha;value.color=color;value.flags=flags;}
        }return value;
    }
};
struct Gesture {
    InputEvent down;std::vector<Snapshot> members,targets;std::map<v::Key,Snapshot> expected;
    v::Settings config;
    v::Rect original,previous,scope_original;v::Edges edges;bool ctrl{};std::uint64_t last_tick{};
    std::optional<m::MagnetAxisPreference> x,y;v::ShakeDetector shake;
    std::optional<v::Action> dock_action;std::optional<v::Rect> dock_rect;
    std::optional<Visual> drag_visual;
    // A limitation belongs to this gesture, not to a global notification or
    // the next gesture's authority. It does not imply any geometry result.
    std::string drag_visual_limit_reason;std::wstring drag_visual_limit;
    std::uint64_t seam_since{};HMONITOR preview_monitor{};
    UINT_PTR seam_timer{};int seam_zone{-1};POINT seam_point{};bool seam_timer_failed{};
    bool layout_drag_restore{};
    bool normal_up_motion_consumed{};
    std::optional<ExactMotionPoint> exact_motion;
    Snapshot operation_source;
    bool restore_armed{},restore_pending{},restore_requested{},was_maximized{};
    NativeRestoreKind restore_kind{NativeRestoreKind::None};
    UINT_PTR restore_timer{};WINDOWPLACEMENT restore_placement{sizeof(WINDOWPLACEMENT)};
    std::optional<InputEvent> restore_move;
};
struct NativeMovePending {
    InputEvent down;
    core::behavior::MoveHandoffGate gate;
    std::shared_ptr<const NativeMoveReceipt> start,end;
    HWND overlay{};
    bool cancel_succeeded{};
};
// Optional GUI-owner diagnostics only. Zero means that clock/stage was not
// observed; these receipts never participate in input or writer authority.
struct ContextualBeginTiming {
    std::uint64_t begin_qpc{},fresh_qpc{},first_all_qpc{},members_fresh_qpc{},other_qpc{};
    std::uint64_t apply_before_qpc{},apply_returned_qpc{};
    bool fresh_returned{},fresh_present{},contextual_evaluated{},contextual_branch{},apply_invoked{},apply_returned{},applied{};
};
std::uint64_t diagnostic_qpc()noexcept{
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value)&&value.QuadPart>0?static_cast<std::uint64_t>(value.QuadPart):0;
}
class Application {
public:
    struct BoundKey {int id{};v::Hotkey binding;};
    struct LayoutRecord {Snapshot before,after;v::Rect normal;bool drag_restore{};};
    HINSTANCE instance{};HWND window{},overlay{};HANDLE mutex{};UINT taskbar_created{};
    operations::InteractiveDesktopContextRef desktop_context;
    v::Settings settings=v::default_settings();Catalog catalog;WindowWriter writer{catalog};InputService input;Log log;
    SettingsWindow settings_ui;fs::path config_path;std::wstring problem;std::optional<Snapshot> last_target;
    std::vector<BoundKey> bound_keys;
    std::optional<Gesture> gesture;v::GestureEpoch epoch;std::map<v::Key,std::vector<LayoutRecord>> undo;
    std::optional<NativeMovePending> native_move;
    std::map<v::Key,Visual> visual;std::uint64_t operations{},native_attempts{};bool stopping{},diagnostic{},last_apply_revoked_only{};
    std::optional<v::End> last_termination;std::string last_prepare_diagnostic;
    std::uint64_t admission_dropped_reported{},admission_wake_failures_reported{};
    // GUI-owner-only bounded observations; no LL/resource callback IO.
    unsigned tray_ingress_records{},tray_menu_records{},contextual_begin_records{};
    std::map<v::Key,Snapshot> last_exact_receipts;
    VisualRecovery recovery;std::map<v::Key,v::VisualOwnership> recovery_owned;
    // Keep the gesture's own original tuple, not the helper journal's older
    // permanent-visual baseline, while an already-issued native call finishes.
    std::optional<Visual> deferred_drag_visual;
    struct PendingVisual {Snapshot identity;v::VisualOwnership planned;std::uint64_t generation{};bool temporary_alpha{};};
    std::map<std::uint64_t,PendingVisual> pending_visual;
    v::TimingHistogram input_post_timing;std::map<std::uint64_t,std::uint64_t> delayed_input_qpc;
    UINT_PTR seam_timer_serial{0x50420000};
    bool overlay_outline{};v::Rect overlay_bounds;std::vector<v::Rect> overlay_members;
    std::vector<FeedbackLine> overlay_magnet_edges,overlay_shared_edges;
    std::wstring overlay_hint;UINT overlay_dpi{96};COLORREF overlay_color{},overlay_key{};
    std::uint64_t overlay_limit_generation{};v::Key overlay_limit_key{};
    UINT_PTR idle_notice_timer{};
    std::optional<GuestQueueFailureRequest> guest_queue_failure_pending;
    std::optional<GuestQueueFailureRequest> guest_callback_owner_stop_pending;
    explicit Application(HINSTANCE h,operations::InteractiveDesktopContextRef context)
        :instance(h),desktop_context(std::move(context)),catalog(desktop_context),writer(catalog),
         input(desktop_context),recovery(desktop_context){}
    ~Application(){stop();}
    void observe_tray_ingress(HWND receiver,WPARAM wp,LPARAM lp)noexcept{
        if(tray_ingress_records>=16)return;const auto index=++tray_ingress_records;
        try{
            const auto tick=GetTickCount64();LARGE_INTEGER qpc{};const bool clock=QueryPerformanceCounter(&qpc)!=FALSE;
            DWORD receiver_pid{},foreground_pid{};const DWORD receiver_tid=GetWindowThreadProcessId(receiver,&receiver_pid);
            const HWND foreground=GetForegroundWindow();const DWORD foreground_tid=foreground?GetWindowThreadProcessId(foreground,&foreground_pid):0;
            const UINT event=LOWORD(lp);const auto routed=tray_callback_route(event);
            const char* route=routed==TrayCallbackRoute::Menu?"menu":
                routed==TrayCallbackRoute::Settings?"settings":"ignored_by_existing_route";
            log.event("tray_callback_ingress","\"record_index\":"+std::to_string(index)+",\"record_limit\":16"+
                ",\"observed_tick\":"+std::to_string(tick)+",\"observed_qpc\":"+(clock?std::to_string(qpc.QuadPart):"null")+
                ",\"actual_receiver_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(receiver))+
                ",\"actual_receiver_PID\":"+std::to_string(receiver_pid)+",\"actual_receiver_TID\":"+std::to_string(receiver_tid)+
                ",\"dispatch_TID\":"+std::to_string(GetCurrentThreadId())+",\"message\":"+std::to_string(tray_message)+
                ",\"raw_WPARAM\":"+std::to_string(static_cast<std::uintptr_t>(wp))+
                ",\"raw_LPARAM\":"+std::to_string(static_cast<std::intptr_t>(lp))+
                ",\"event_LOWORD\":"+std::to_string(event)+",\"icon_id_HIWORD\":"+std::to_string(HIWORD(lp))+
                ",\"existing_route\":\""+route+"\",\"foreground_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground))+
                ",\"foreground_PID\":"+std::to_string(foreground_pid)+",\"foreground_TID\":"+std::to_string(foreground_tid)+
                ",\"authenticated_Shell_sender_inferred\":false,\"route_execution_not_yet_observed\":true");
        }catch(...){/* Optional logging must not suppress the original route. */}
    }
    void callback_owner_stop_fact(bool exit_fact){
        const auto generation=input.guest_callback_owner_stop_generation();if(!generation)return;
        const auto dispatch=guest_callback_stop_dispatch(input.guest_callback_owner_stop_triggered(),generation,
            gesture?gesture->down.generation:0,native_move?native_move->down.generation:0);
        log.event(exit_fact?"input_callback_owner_exit":"input_callback_owner_stop","\"generation\":"+
            std::to_string(generation)+","+input.guest_callback_owner_stop_json());
        if(!dispatch.revoke_exact_generation)return;
        // A one-shot wait notification may arrive after public recovery. Its
        // original permit is never global revocation authority for a new pair.
        input.revoke_generation(generation,v::End::InputUnavailable);
        if(dispatch.finish_gui_gesture){finish(v::End::InputUnavailable);
            if(!input.healthy())notify(L"输入回调资源不可用，增强已中止；请暂停后恢复。");}
        else if(dispatch.clear_native_pending)native_move.reset();
    }
    void notify(std::wstring text){problem=std::move(text);log.event("notice","\"reason\":\""+escaped(utf8(problem))+"\"");tray();show_idle_notice();}
    void tray(){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
        n.uCallbackMessage=tray_message;n.hIcon=LoadIconW(nullptr,settings.enabled&&input.healthy()?IDI_APPLICATION:IDI_WARNING);
        const auto tip=std::wstring(L"PaneBind — ")+(settings.enabled?(input.healthy()?L"运行中；Shift 起手为原生操作":L"输入故障"):L"已暂停");
        wcsncpy_s(n.szTip,tip.c_str(),_TRUNCATE);Shell_NotifyIconW(NIM_MODIFY,&n);}
    void log_status(){
        const auto windows=catalog.all(settings);log.event("status","\"enabled\":"+std::string(settings.enabled?"true":"false")+
            ",\"hook_owner_running\":"+(input.healthy()?"true":"false")+",\"gesture_active\":"+(gesture?"true":"false")+
            ",\"native_move_handoff_pending\":"+(input.native_move_in_progress()?"true":"false")+
            ",\"native_operation_pending\":"+(writer.has_pending_native_operation()?"true":"false")+
            ",\"native_completion_unknown\":"+(writer.has_unconfirmed_native_call()?"true":"false")+
            ",\"visual_restore_pending\":"+(!pending_visual.empty()||deferred_drag_visual?"true":"false")+
            ",\"eligible_windows\":"+std::to_string(windows.size())+",\"operations\":"+std::to_string(operations)+",\"native_callsite_claims\":"+std::to_string(native_attempts)+
            ",\"mouse_up_callbacks\":"+mouse_up_callback_json(input.mouse_up_callbacks()));
    }
    bool save(v::Settings value){
        const auto error=v::validate(value);if(!error.empty()){notify(L"配置无效："+wide(error));return false;}
        if(settings.generation==std::numeric_limits<std::uint64_t>::max()){notify(L"配置代际已满，未保存。");return false;}
        value.generation=settings.generation+1;
        if(!atomic_save(config_path,v::serialize(value))){notify(L"无法原子保存配置，原配置未覆盖。");return false;}
        settings=std::move(value);return true;
    }
    void initialize_paths(){
        const fs::path base=fs::path(exe_path()).parent_path();
        if(fs::exists(base/L"portable.flag"))config_path=base/L"settings.ini";
        else{PWSTR local{};if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))){config_path=fs::path(local)/L"PaneBind"/L"settings.ini";CoTaskMemFree(local);}}
        if(config_path.empty())throw std::runtime_error("no_config_path");
        fs::create_directories(config_path.parent_path());log.open(config_path.parent_path()/L"panebind.log.jsonl");
        std::string text;if(fs::exists(config_path)){
            if(read_file(config_path,text)){auto parsed=v::parse(text);if(parsed.value){settings=*parsed.value;return;}}
            const auto bad=config_path.wstring()+L".corrupt."+std::to_wstring(GetTickCount64());
            if(!MoveFileW(config_path.c_str(),bad.c_str()))throw std::runtime_error("corrupt_config_backup_failed");
            problem=L"配置损坏，原文件已留存。当前使用默认值。";
        }
    }
    std::vector<v::Item> items(const std::vector<Snapshot>& list)const{std::vector<v::Item> result;
        for(const auto& s:list)result.push_back({s.key,s.visible,s.limits});return result;}
    bool apply(const v::Plan& plan,std::vector<Snapshot> identities,std::uint64_t generation=0,bool completion=false,std::uint64_t observed_qpc=0,
               std::optional<WindowWriter::ResizeRequest> resize=std::nullopt){
        last_exact_receipts.clear();
        last_apply_revoked_only=false;
        if(!plan.accepted)return false;if(plan.placements.empty())return true;
        for(auto& s:identities)if(gesture&&gesture->expected.contains(s.key))s=gesture->expected[s.key];
        ++operations;
        if(stopping||!settings.enabled)return false;
        // Release-to-commit docking is an explicitly discrete command. An UP
        // receipt never authorizes a new continuous Move/Resize placement,
        // including a plan whose GUI consumer lagged behind the input owner.
        const bool completed_normal_up=completion&&plan.reason=="normal_up_dock"&&
            !gesture&&epoch.generation()==generation&&epoch.last_end()==v::End::NormalUp;
        if(completion&&!completed_normal_up)return false;
        const HWND authority_source=gesture?gesture->down.prepared->source.hwnd:completed_normal_up&&!identities.empty()?identities.front().hwnd:nullptr;
        const auto capture=completed_normal_up?GestureCaptureLease{}:input.capture_context(generation);
        const auto receipts=writer.batch(identities,plan.placements,settings,
            input.operation_guard(generation,completed_normal_up,authority_source),
            capture,resize);
        bool ok=receipts.size()==plan.placements.size();
        bool only_terminal_withdrawal=!receipts.empty();
        for(const auto& receipt:receipts){
            if(receipt.resize_query&&(diagnostic||!receipt.accepted)){
                const auto& q=*receipt.resize_query;
                const auto native_rect=[](const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+
                    std::to_string(r.right)+","+std::to_string(r.bottom)+"]";};
                log.event("application_resize_proposal","\"generation\":"+std::to_string(generation)+
                    ",\"operation\":"+std::to_string(operations)+",\"key\":"+std::to_string(receipt.before.key)+
                    ",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(receipt.before.hwnd))+
                    ",\"source_PID\":"+std::to_string(receipt.before.pid)+",\"source_TID\":"+std::to_string(receipt.before.tid)+
                    ",\"edge\":"+std::to_string(q.edge)+",\"callback_attempted\":"+(q.attempted?"true":"false")+
                    ",\"API_return\":"+(q.attempted?std::to_string(q.api_return):"null")+
                    ",\"API_last_error\":"+(q.attempted?std::to_string(q.error):"null")+
                    ",\"last_error_is_failure_diagnostic_only\":true,\"message_LRESULT\":"+
                        (q.attempted&&q.api_return?std::to_string(q.message_result):"null")+
                    ",\"R_native\":"+(q.attempted?native_rect(q.requested_native):"null")+
                    ",\"C_native\":"+(q.attempted&&q.api_return?native_rect(q.returned_native):"null")+
                    ",\"R_visible\":"+(receipt.resize_original_requested?rectangle(*receipt.resize_original_requested):"null")+
                    ",\"candidate_visible\":"+rectangle(receipt.requested)+
                    ",\"before_qpc\":"+std::to_string(q.before_qpc)+",\"after_qpc\":"+std::to_string(q.after_qpc)+
                    ",\"context_revalidated\":"+(q.context_revalidated?"true":"false")+
                    ",\"preprobe_limits_from_batch_preflight\":"+(q.preprobe_limits_from_batch_preflight?"true":"false")+
                    ",\"hook_observed_qpc\":"+std::to_string(observed_qpc)+
                    ",\"batch_result_reason\":\""+escaped(receipt.reason)+"\",\"callback_is_placement_attempt\":false"+
                    ",\"global_application_grid_known\":false");
            }
            only_terminal_withdrawal=only_terminal_withdrawal&&WindowWriter::preserves_terminal_receipt(
                receipt,generation,capture,input.allowed(generation));
            if(receipt.native_attempted)++native_attempts;
            if(receipt.native_attempted){
                if(receipt.completion_known)input_post_timing.observe_qpc(observed_qpc,receipt.post_observed_qpc,input.counter_frequency());
                else delayed_input_qpc[receipt.call_id]=observed_qpc; // At most the one issued worker call.
            }
            if(diagnostic||!receipt.accepted)log.event("placement","\"generation\":"+std::to_string(generation)+",\"operation\":"+std::to_string(operations)+
                ",\"intent_reason\":\""+escaped(plan.reason)+"\""+
                ",\"key\":"+std::to_string(receipt.before.key)+",\"native_callsite_claimed\":"+(receipt.native_attempted?"true":"false")+
                ",\"native_attempted\":"+(receipt.native_attempted?(receipt.completion_known?"true":"null"):"false")+
                ",\"call_id\":"+std::to_string(receipt.call_id)+
                ",\"native_succeeded\":"+(receipt.native_succeeded?"true":"false")+",\"error\":"+std::to_string(receipt.error)+
                ",\"native_completion_known\":"+(receipt.completion_known?"true":"false")+
                ",\"target\":"+rectangle(receipt.requested)+",\"actual\":"+(receipt.after?rectangle(receipt.after->visible):"null")+
                ",\"identity_valid\":"+(receipt.identity_valid?"true":"false")+",\"exact\":"+(receipt.geometry_exact?"true":"false")+
                ",\"declared_track_limits_observed\":"+(receipt.before.declared_track_limits_observed?"true":"false")+
                ",\"application_grid_known\":"+(receipt.before.application_grid_known?"true":"false")+
                ",\"reason\":\""+escaped(receipt.reason)+"\",\"native_before_qpc\":"+std::to_string(receipt.before_qpc)+
                ",\"native_after_qpc\":"+std::to_string(receipt.after_qpc)+",\"authority_after\":"+(generation&&input.allowed(generation)?"true":"false")+
                ",\"native_position_flags\":"+(receipt.operation==WindowWriter::Operation::Placement&&receipt.native_attempted&&receipt.completion_known?std::to_string(receipt.native_position_flags):"null")+
                ",\"preflight_started_qpc\":"+std::to_string(receipt.preflight_started_qpc)+
                ",\"geometry_observed_qpc\":"+std::to_string(receipt.geometry_observed_qpc)+
                ",\"post_observed_qpc\":"+std::to_string(receipt.post_observed_qpc)+
                ",\"post_context_checked\":"+(receipt.post_context_checked?"true":"false")+
                ",\"post_context_valid\":"+(receipt.post_context_valid?"true":"false")+
                ",\"post_context_reason\":\""+escaped(receipt.post_context_reason)+"\""+
                ",\"post_track_limits_query_requested\":"+(receipt.post_track_limits_query_requested?(*receipt.post_track_limits_query_requested?"true":"false"):"null")+
                ",\"post_track_limits_carried_from_preflight\":"+(receipt.post_track_limits_carried_from_preflight?"true":"false")+
                ",\"explicit_destination_display_bound\":"+(receipt.destination?"true":"false")+
                ",\"destination_monitor\":"+std::to_string(receipt.destination?reinterpret_cast<std::uintptr_t>(receipt.destination->target.monitor):0)+
                ",\"destination_window_dpi\":"+std::to_string(receipt.destination?receipt.destination->window_dpi:0)+
                ",\"hook_observed_qpc\":"+std::to_string(observed_qpc)+
                ",\"normal_up_completion\":"+(completion?"true":"false"));
            if(receipt.after&&receipt.identity_valid&&gesture)gesture->expected[receipt.before.key]=*receipt.after;
            if(receipt.after&&receipt.accepted&&receipt.identity_valid&&receipt.geometry_exact){
                last_exact_receipts[receipt.before.key]=*receipt.after;
                // This is the writer's exact actual context for the next
                // quantum, not a new DOWN/intent or member geometry anchor.
                if(gesture&&gesture->operation_source.key==receipt.before.key&&
                   Catalog::same(gesture->operation_source,*receipt.after))gesture->operation_source=*receipt.after;
            }
            ok=ok&&receipt.accepted&&receipt.identity_valid&&receipt.geometry_exact;
        }
        // A completed exact prefix remains an actual completion, even if UP
        // retires the next admission or the capture-release receipt is still
        // being published. Keep that UNKNOWN context separate from a native
        // failure, and let the actual terminal event choose the end reason.
        last_apply_revoked_only=!ok&&generation&&!input.allowed(generation)&&only_terminal_withdrawal;
        return ok;
    }
    std::vector<LayoutRecord>* owned_layout(const Snapshot& actual){
        const auto found=undo.find(actual.key);if(found==undo.end()||found->second.empty())return nullptr;
        // Native maximize/minimize has its own normal-state restoration. It is
        // not proof that our normal layout was manually rearranged.
        if(actual.maximized||actual.minimized)return nullptr;
        const auto& expected=found->second.back().after;
        if(!Catalog::same(expected,actual)||expected.visible!=actual.visible||expected.positioning!=actual.positioning||
            expected.monitor!=actual.monitor||expected.dpi!=actual.dpi){undo.erase(found);return nullptr;}
        return &found->second;
    }
    void remember(const Snapshot& before,const Snapshot& after,bool drag_restore=false){
        if(!Catalog::same(before,after)||before.maximized||after.maximized||
            (before.visible==after.visible&&before.positioning==after.positioning))return;
        auto normal=before.visible;
        if(const auto previous=owned_layout(before);drag_restore&&previous&&previous->back().drag_restore)normal=previous->back().normal;
        auto& list=undo[before.key];if(list.size()==8)list.erase(list.begin());list.push_back({before,after,normal,drag_restore});
    }
    void remember_receipt(const Snapshot& before,bool drag_restore=false){
        if(const auto actual=last_exact_receipts.find(before.key);actual!=last_exact_receipts.end())remember(before,actual->second,drag_restore);
    }
    bool commit_visual(const Snapshot& identity,v::VisualOwnership planned,const v::VisualState& actual){
        if(planned.changed_top)v::record_top(planned,actual);
        if(planned.changed_alpha){
            if(actual.layered&&!actual.attributes_known)return false;
            v::record_alpha(planned,actual);
        }
        if(!recovery.commit(identity,planned))return false;
        if(v::visual_changes(planned))recovery_owned[identity.key]=planned;else recovery_owned.erase(identity.key);
        return true;
    }
    bool mutate_visual(const Snapshot& identity,const v::VisualState& before,const v::VisualState& target,
                       bool alpha_change,bool top_change,std::optional<v::VisualOwnership> cleanup_owned={},
                       WindowWriter::Receipt* actual_receipt=nullptr,bool temporary_alpha=false){
        const bool cleanup=cleanup_owned.has_value();
        // No property operation may overtake a geometry/property native call
        // whose completion is unknown. In particular never collapse helper
        // prior/planned tuples merely because the bounded UI wait ended.
        if(writer.has_pending_native_operation()||!pending_visual.empty())return false;
        auto owned=recovery_owned.contains(identity.key)?recovery_owned.at(identity.key):v::VisualOwnership{before,before,false,false};
        Visual::observe_owned(owned,before);
        auto planned=owned;planned.applied=target;
        // During prepare a restoring transition is still owned; commit clears
        // the flag only after actual equality with the original is observed.
        planned.changed_alpha=owned.changed_alpha||alpha_change;
        planned.changed_top=owned.changed_top||top_change;
        if(!v::visual_changes(planned))return true;
        if(!recovery.remember(identity,planned)){
            log.event("visual_recovery","\"phase\":\"prepare_rejected\",\"reason\":\""+escaped(recovery.error())+"\"");return false;
        }
        const auto generation=gesture?gesture->down.generation:0;
        // Cleanup authority is the already-owned, actually observed tuple,
        // not renewed input authority. Capture no Application/Visual references
        // in work which can outlive this bounded call.
        auto cleanup_claim=std::make_shared<std::atomic<bool>>(cleanup);
        auto guard=cleanup?WindowWriter::Guard{[cleanup_claim]{return cleanup_claim->load();}}:
            input.operation_guard(generation,false,generation?identity.hwnd:nullptr);
        const auto receipt=writer.visual(identity,before,target,alpha_change,top_change,settings,std::move(guard),cleanup_owned,
            [this](HANDLE completion){return recovery.defer_until_native_completion(completion);},
            // Owned-property cleanup is not new motion authority, but an
            // active gesture still needs its exact actual capture context.
            // The writer independently rejects stale/foreign/retired capture.
            input.capture_context(generation));
        if(actual_receipt)*actual_receipt=receipt;
        *cleanup_claim=false;
        bool acknowledged=false;
        if(!receipt.completion_known&&receipt.call_id)pending_visual.emplace(receipt.call_id,PendingVisual{identity,planned,generation,temporary_alpha});
        else if(receipt.identity_valid&&receipt.visual_after)acknowledged=commit_visual(identity,planned,*receipt.visual_after);
        if(receipt.completion_known&&!acknowledged&&receipt.call_id)pending_visual.emplace(receipt.call_id,PendingVisual{identity,planned,generation,temporary_alpha});
        log.event("visual_mutation","\"key\":"+std::to_string(identity.key)+",\"generation\":"+std::to_string(generation)+
            ",\"mutation_started\":"+(receipt.native_attempted?"true":"false")+
            ",\"call_id\":"+std::to_string(receipt.call_id)+
            ",\"native_attempted\":"+(receipt.native_attempted?(receipt.completion_known?"true":"null"):"false")+
            ",\"native_completion_known\":"+(receipt.completion_known?"true":"false")+
            ",\"before\":"+Visual::fields(before)+",\"target\":"+Visual::fields(target)+
            ",\"actual\":"+(receipt.visual_after?Visual::fields(*receipt.visual_after):"null")+
            ",\"style_native_attempted\":"+(receipt.visual_native.style_attempted?"true":"false")+
            ",\"alpha_native_attempted\":"+(receipt.visual_native.alpha_attempted?"true":"false")+
            ",\"zorder_native_attempted\":"+(receipt.visual_native.zorder_attempted?"true":"false")+
            ",\"cleanup_owned_property\":"+(cleanup?"true":"false")+
            ",\"native_sequence_returned_success\":"+(receipt.native_succeeded?"true":"false")+
            ",\"visual_exact\":"+(receipt.visual_exact?"true":"false")+",\"reason\":\""+escaped(receipt.reason)+"\",\"recovery_commit_ack\":"+(acknowledged?"true":"false")+
            ",\"helper_pid\":"+std::to_string(recovery.helper_pid())+",\"no_foreground_recovery\":true");
        return receipt.accepted&&receipt.visual_exact&&acknowledged;
    }
    void reconcile_recovery(const Snapshot& identity){
        if(writer.has_pending_native_operation()||!pending_visual.empty())return;
        const auto entry=recovery_owned.find(identity.key);if(entry==recovery_owned.end())return;
        const auto actual=Visual::read(identity.hwnd);
        if(actual){Visual::observe_owned(entry->second,*actual);
            if(!v::visual_changes(entry->second)&&recovery.forget(identity.key))recovery_owned.erase(entry);}
    }
    bool visual_restore(Visual& record){
        const auto now=catalog.get(record.identity.hwnd,settings,nullptr,true);if(!now||!Catalog::same(record.identity,*now))return false;
        const HWND h=now->hwnd;bool ok=true;
        const auto valid=[&]{const auto current=catalog.get(h,settings,nullptr,true);return current&&Catalog::same(record.identity,*current);};
        auto observed=Visual::read(h);if(!observed)return false;
        // A different alpha/color/flags/layered tuple belongs to the app, not
        // to our old record. No restoration write is allowed in that case.
        if(record.ownership.changed_alpha){
            if(observed->layered&&!observed->attributes_known)ok=false;
            else if(v::owns_alpha(record.ownership,*observed)){
                auto target=*observed;v::copy_alpha(target,record.ownership.original);
                const bool assigned=mutate_visual(*now,*observed,target,true,false,record.ownership);
                const auto after=Visual::read(h);
                if(assigned&&after&&v::same_alpha(*after,record.ownership.original))record.ownership.changed_alpha=false;
                else ok=false;
            }else record.ownership.changed_alpha=false;
        }
        observed=Visual::read(h);if(!observed)return false;
        if(record.ownership.changed_top){
            if(v::owns_top(record.ownership,*observed)){
                auto target=*observed;target.topmost=record.ownership.original.topmost;
                const bool assigned=mutate_visual(*now,*observed,target,false,true,record.ownership);
                const auto after=Visual::read(h);
                if(assigned&&valid()&&after&&after->topmost==record.ownership.original.topmost)record.ownership.changed_top=false;
                else ok=false;
            }else record.ownership.changed_top=false;
        }reconcile_recovery(record.identity);return ok;
    }
    void retry_deferred_drag_visual(){
        if(!deferred_drag_visual||writer.has_pending_native_operation()||!pending_visual.empty())return;
        auto& record=*deferred_drag_visual;
        // No replacement HWND is touched. A still-existing but ineligible or
        // unreadable source stays pending rather than being called restored.
        if(!IsWindow(record.identity.hwnd)){
            log.event("temporary_visual_cleanup","\"key\":"+std::to_string(record.identity.key)+
                ",\"status\":\"source_destroyed_no_restore\",\"new_write_authority_granted\":false");
            deferred_drag_visual.reset();return;
        }
        const bool restored=visual_restore(record);
        if(!v::visual_changes(record.ownership)){
            log.event("temporary_visual_cleanup","\"key\":"+std::to_string(record.identity.key)+
                ",\"status\":\"held_attributes_retired\",\"restore_confirmed\":"+(restored?"true":"false")+
                ",\"new_write_authority_granted\":false");
            deferred_drag_visual.reset();
        }
    }
    void restore_drag_visual(Visual record){
        // visual_begin refuses a second temporary mutation until this single
        // slot is retired, so no later gesture can replace the saved original.
        if(deferred_drag_visual){
            log.event("temporary_visual_cleanup","\"key\":"+std::to_string(record.identity.key)+
                ",\"status\":\"unexpected_existing_record\",\"restore_confirmed\":false");return;
        }
        deferred_drag_visual=std::move(record);retry_deferred_drag_visual();
        if(deferred_drag_visual)log.event("temporary_visual_cleanup","\"key\":"+
            std::to_string(deferred_drag_visual->identity.key)+",\"status\":\"deferred\",\"restore_confirmed\":false");
    }
    std::optional<Visual> visual_begin(const Snapshot& s,bool top,BYTE alpha,bool temporary){
        if(temporary){retry_deferred_drag_visual();if(deferred_drag_visual)return {};}
        const auto now=catalog.get(s.hwnd,settings);if(!now||!Catalog::same(s,*now))return {};
        const auto before=Visual::read(s.hwnd);if(!before)return {};
        if(before->layered&&!before->attributes_known){
            if(temporary)drag_visual_limited(s,"alpha_attributes_unknown",L"无法读取原透明状态，本次不应用拖动透明");
            return {};
        }
        Visual record;record.identity=s;record.ownership.original=record.ownership.applied=*before;
        const auto valid=[&]{const auto current=catalog.get(s.hwnd,settings);return current&&Catalog::same(s,*current);};
        if(!temporary&&top!=before->topmost){
            auto target=*before;target.topmost=top;
            const bool assigned=mutate_visual(s,*before,target,false,true);
            const auto after=Visual::read(s.hwnd);
            if(!assigned||!valid()||!after||after->topmost!=top)return {};
            v::record_top(record.ownership,*after);
        }
        const auto current=Visual::read(s.hwnd);
        if(!current){visual_restore(record);return {};}
        if(current->layered&&!current->attributes_known){
            if(temporary)drag_visual_limited(s,"alpha_attributes_unknown",L"无法读取原透明状态，本次不应用拖动透明");
            visual_restore(record);return {};
        }
        if(!v::same_alpha(*current,record.ownership.original)){
            v::copy_alpha(record.ownership.original,*current);v::copy_alpha(record.ownership.applied,*current);
        }
        if(alpha!=v::effective_alpha(*current,LWA_ALPHA)){
            auto target=*current;target.layered=true;target.attributes_known=true;target.alpha=alpha;target.flags|=LWA_ALPHA;
            const bool mutation_admitted=!writer.has_pending_native_operation()&&pending_visual.empty();
            WindowWriter::Receipt receipt;
            const bool assigned=mutate_visual(s,*current,target,true,false,{},&receipt,temporary);
            const auto after=Visual::read(s.hwnd);
            if(after&&valid()&&v::same_alpha(*after,target))v::record_alpha(record.ownership,*after);
            if(!assigned){
                const bool pending_this_alpha=mutation_admitted&&std::any_of(pending_visual.begin(),pending_visual.end(),[&](const auto& pending){
                    return pending.second.identity.key==s.key&&pending.second.planned.changed_alpha&&
                        v::same_alpha(pending.second.planned.applied,target);
                });
                if(temporary&&pending_this_alpha){
                    // This is a pending tuple, not an observed success. Once
                    // the actual call completes, cleanup may restore it ONLY
                    // if the actual tuple still matches this exact target.
                    const auto original=record.ownership.original;
                    v::copy_alpha(record.ownership.applied,target);record.ownership.changed_alpha=true;
                    restore_drag_visual(std::move(record));
                    if(drag_alpha_completion_failed(receipt.native_attempted,receipt.completion_known,receipt.native_succeeded,receipt.visual_exact)){
                        const auto final=Visual::read(s.hwnd);
                        const bool rollback_exact=!deferred_drag_visual&&valid()&&final&&v::same_alpha(*final,original)&&final->topmost==original.topmost;
                        drag_visual_limited(s,"issued_alpha_not_exact",rollback_exact?
                            L"本次拖动透明未完成，原透明状态已确认保留":L"本次拖动透明未完成，原状态恢复尚未确认");
                    }
                    return {};
                }
                const bool restored=visual_restore(record);
                const auto final=Visual::read(s.hwnd);
                const bool rollback_exact=restored&&valid()&&final&&
                    v::same_alpha(*final,record.ownership.original)&&final->topmost==record.ownership.original.topmost;
                log.event("visual_failure","\"reason\":\"temporary_or_shake_alpha_not_exact\",\"owned_rollback_exact\":"+std::string(rollback_exact?"true":"false"));
                // Busy/unissued admission, a pending call, or an ownership
                // journal failure is not proof of a failed native alpha API.
                if(temporary&&drag_alpha_completion_failed(receipt.native_attempted,receipt.completion_known,receipt.native_succeeded,receipt.visual_exact))
                    drag_visual_limited(s,"issued_alpha_not_exact",rollback_exact?
                        L"本次拖动透明未完成，原透明状态已确认保留":L"本次拖动透明未完成，原状态恢复尚未确认");
                return {};
            }
        }return record;
    }
    bool visual_action(const Snapshot& source,v::Action action){
        const auto fresh=catalog.get(source.hwnd,settings);
        if(!fresh||!Catalog::same(source,*fresh))return false;
        const auto before=Visual::read(source.hwnd);if(!before)return false;
        auto stored=visual.find(source.key);
        if(stored!=visual.end()&&!Catalog::same(stored->second.identity,*fresh)){visual.erase(stored);stored=visual.end();}
        Visual record;
        if(stored!=visual.end())record=stored->second;
        else{record.identity=*fresh;record.ownership.original=record.ownership.applied=*before;}
        Visual::observe_owned(record.ownership,*before);
        const auto valid=[&]{const auto current=catalog.get(source.hwnd,settings);return current&&Catalog::same(record.identity,*current);};
        bool success{};
        if(action==v::Action::ToggleTopmost){
            const bool requested=!before->topmost;
            auto target=*before;target.topmost=requested;
            const bool assigned=mutate_visual(*fresh,*before,target,false,true);
            const auto after=Visual::read(source.hwnd);
            if(after&&valid()){
                // Keep alpha ownership only if the application did not change
                // that independently while processing the topmost request.
                if(record.ownership.changed_alpha&&!v::owns_alpha(record.ownership,*after))record.ownership.changed_alpha=false;
                if(!record.ownership.changed_alpha){v::copy_alpha(record.ownership.original,*after);v::copy_alpha(record.ownership.applied,*after);}
                if(assigned&&after->topmost==requested){v::record_top(record.ownership,*after);success=true;}
                else if(record.ownership.changed_top&&!v::owns_top(record.ownership,*after))record.ownership.changed_top=false;
            }
        }else if(action==v::Action::OpacityDown||action==v::Action::OpacityUp){
            if(before->layered&&!before->attributes_known)return false;
            const int alpha=v::stepped_alpha(v::effective_alpha(*before,LWA_ALPHA),action==v::Action::OpacityUp);
            auto target=*before;
            if(record.ownership.changed_alpha&&alpha==v::effective_alpha(record.ownership.original,LWA_ALPHA)){
                // Returning to the original opacity also restores its original
                // layered/colorkey/flags state, not just the numeric byte.
                v::copy_alpha(target,record.ownership.original);
            }else if(alpha!=v::effective_alpha(*before,LWA_ALPHA)){
                target.layered=true;target.attributes_known=true;target.alpha=static_cast<BYTE>(alpha);target.flags|=LWA_ALPHA;
            }
            success=mutate_visual(*fresh,*before,target,!v::same_alpha(*before,target),false);
            const auto after=Visual::read(source.hwnd);
            if(after&&valid()){
                if(record.ownership.changed_top&&!v::owns_top(record.ownership,*after))record.ownership.changed_top=false;
                if(v::same_alpha(*after,target))v::record_alpha(record.ownership,*after);
                else if(record.ownership.changed_alpha&&!v::owns_alpha(record.ownership,*after))record.ownership.changed_alpha=false;
            }
        }
        if(v::visual_changes(record.ownership))visual[source.key]=record;
        else visual.erase(source.key);
        return success;
    }
    void set_paused(bool paused){
        hide_preview("pause_or_resume");
        input.pause(true);finish(v::End::Paused);
        if(!paused){input.stop();if(!input.start(window,diagnostic)){notify(L"无法恢复输入资源，保持暂停。");settings.enabled=false;tray();return;}}
        input.pause(paused);auto next=settings;next.enabled=!paused;if(!save(next))settings.enabled=!paused;
        if(paused){for(auto& [key,value]:visual){(void)key;visual_restore(value);}visual.clear();}
        tray();if(!paused)prepare();log_status();
    }
    void hide_preview(const char* reason="preview_hidden"){
        // SetTimer is retired on its first matching message. A queued message
        // with an older serial cannot hide a later notice or gesture overlay.
        const auto notice=idle_notice_timer;idle_notice_timer=0;
        const bool timer_cancelled=notice&&KillTimer(window,notice)!=FALSE;
        overlay_outline=false;overlay_members.clear();overlay_magnet_edges.clear();overlay_shared_edges.clear();overlay_hint.clear();overlay_bounds={};
        if(!overlay)return;
        ShowWindow(overlay,SW_HIDE);const bool title_cleared=SetWindowTextW(overlay,L"")!=FALSE;
        if(overlay_limit_generation){
            log.event("drag_transparency_limit_presentation","\"phase\":\"hidden\",\"generation\":"+std::to_string(overlay_limit_generation)+
                ",\"key\":"+std::to_string(overlay_limit_key)+",\"overlay_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(overlay))+
                ",\"actual_visible\":"+(IsWindowVisible(overlay)?"true":"false")+",\"title_cleared\":"+(title_cleared?"true":"false"));
            overlay_limit_generation=0;overlay_limit_key=0;
        }
        if(notice)log.event("notice_presentation","\"phase\":\"hidden\",\"serial\":"+std::to_string(notice)+
            ",\"reason\":\""+escaped(reason)+"\",\"timer_cancelled\":"+(timer_cancelled?"true":"false")+
            ",\"overlay_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(overlay))+
            ",\"actual_visible\":"+(IsWindowVisible(overlay)?"true":"false")+",\"title_cleared\":"+(title_cleared?"true":"false"));
    }
    void show_idle_notice(){
        const auto skipped=[&](const char* reason){log.event("notice_presentation","\"phase\":\"not_shown\",\"reason\":\""+
            escaped(reason)+"\",\"queued\":false,\"new_write_authority_granted\":false");};
        // This is presentation, never deferred command/input authority. Busy
        // notices stay in the existing log only; no queue or retry is created.
        if(stopping||!overlay||!window||problem.empty()){skipped("presentation_unavailable");return;}
        if(gesture||native_move||input.native_move_in_progress()||writer.has_pending_native_operation()||
            !pending_visual.empty()||deferred_drag_visual){skipped("operation_or_gesture_busy");return;}
        const auto foreground=GetForegroundWindow();
        if(!foreground){skipped("foreground_unknown");return;}
        const auto monitor=MonitorFromWindow(foreground,MONITOR_DEFAULTTOPRIMARY);
        const auto displays=catalog.displays();
        const auto display=std::find_if(displays.begin(),displays.end(),[&](const auto& value){return value.monitor==monitor;});
        if(display==displays.end()||!display->dpi||display->work.empty()){skipped("current_monitor_work_unknown");return;}
        const auto& work=display->work;const auto height=dip(25,display->dpi);
        const auto width=std::min(work.width(),dip(560,display->dpi));
        if(height<=0||work.height()<height||width<=0){skipped("current_work_too_small");return;}
        const auto left=work.left()+(work.width()-width)/2;
        const auto top=std::max(work.top(),work.bottom()-height-dip(32,display->dpi));
        const v::Rect bounds{left,top,left+width,top+height};
        constexpr auto lo=std::numeric_limits<int>::min(),hi=std::numeric_limits<int>::max();
        if(width>hi||height>hi||bounds.left()<lo||bounds.top()<lo||bounds.right()>hi||bounds.bottom()>hi){skipped("notice_native_bounds_unrepresentable");return;}
        if(seam_timer_serial==std::numeric_limits<UINT_PTR>::max()){skipped("notice_timer_serial_exhausted");return;}
        if(idle_notice_timer)hide_preview("replaced_by_new_notice");
        overlay_outline=true;overlay_members.clear();overlay_magnet_edges.clear();overlay_shared_edges.clear();
        overlay_bounds=bounds;overlay_dpi=display->dpi;overlay_hint=problem.substr(0,160);
        if(problem.size()>overlay_hint.size())overlay_hint+=L"…";
        overlay_color=GetSysColor(COLOR_INFOTEXT);
        for(const auto candidate:{RGB(1,2,3),RGB(4,5,6),RGB(7,8,9),RGB(10,11,12)})
            if(candidate!=GetSysColor(COLOR_INFOBK)&&candidate!=overlay_color){overlay_key=candidate;break;}
        const bool styled=SetLayeredWindowAttributes(overlay,overlay_key,255,LWA_COLORKEY|LWA_ALPHA)!=FALSE;
        const bool title_set=styled&&SetWindowTextW(overlay,overlay_hint.c_str())!=FALSE;
        const auto serial=++seam_timer_serial;
        const auto timer_return=styled?SetTimer(window,serial,4000,nullptr):0;
        const bool armed=timer_return!=0;
        if(armed)idle_notice_timer=serial;
        const bool positioned=armed&&SetWindowPos(overlay,HWND_TOPMOST,static_cast<int>(left),static_cast<int>(top),
            static_cast<int>(width),static_cast<int>(height),SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
        const bool visible=IsWindowVisible(overlay)!=FALSE;
        log.event("notice_presentation","\"phase\":\"show\",\"serial\":"+std::to_string(serial)+
            ",\"overlay_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(overlay))+",\"text\":\""+escaped(utf8(overlay_hint))+
            "\",\"requested_bounds\":"+rectangle(bounds)+",\"SetLayeredWindowAttributes_succeeded\":"+(styled?"true":"false")+
            ",\"SetWindowText_attempted\":"+(styled?"true":"false")+",\"SetWindowText_succeeded\":"+(title_set?"true":"false")+
            ",\"SetTimer_attempted\":"+(styled?"true":"false")+",\"SetTimer_return\":"+std::to_string(timer_return)+
            ",\"SetTimer_succeeded\":"+(armed?"true":"false")+",\"deadline_ms\":4000,\"SetWindowPos_attempted\":"+(armed?"true":"false")+
            ",\"SetWindowPos_succeeded\":"+(positioned?"true":"false")+",\"actual_visible\":"+(visible?"true":"false")+
            ",\"foreground_unchanged\":"+(GetForegroundWindow()==foreground?"true":"false")+
            ",\"queued\":false,\"new_write_authority_granted\":false,\"text_property_not_rendering_proof\":true");
        if(!positioned||!visible){hide_preview("notice_show_failed");return;}
        InvalidateRect(overlay,nullptr,FALSE);
    }
    void idle_notice_timeout(UINT_PTR serial){
        if(!serial||idle_notice_timer!=serial)return;
        hide_preview("notice_deadline");
    }
    std::optional<Snapshot> drag_visual_limit_source(){
        if(!gesture||gesture->drag_visual_limit.empty()||stopping||!input.allowed(gesture->down.generation))return {};
        const auto& source=gesture->down.prepared->source;
        if(GetForegroundWindow()!=source.hwnd)return {};
        const auto actual=catalog.get(source.hwnd,settings);
        if(!actual||!Catalog::same(source,*actual))return {};
        return actual;
    }
    bool position_overlay(const v::Rect& bounds,const char* mode,bool limited=false){
        if(limited&&(!gesture||!input.allowed(gesture->down.generation)||
            GetForegroundWindow()!=gesture->down.prepared->source.hwnd)){hide_preview();return false;}
        const bool title_set=SetWindowTextW(overlay,overlay_hint.c_str())!=FALSE;
        const bool positioned=SetWindowPos(overlay,HWND_TOPMOST,static_cast<int>(bounds.left()),static_cast<int>(bounds.top()),
            static_cast<int>(bounds.width()),static_cast<int>(bounds.height()),SWP_NOACTIVATE|SWP_SHOWWINDOW)!=FALSE;
        const bool visible=IsWindowVisible(overlay)!=FALSE;
        if(limited&&(!overlay_limit_generation||!positioned||!visible)){
            const auto& source=gesture->down.prepared->source;
            log.event("drag_transparency_limit_presentation","\"phase\":\"show\",\"generation\":"+std::to_string(gesture->down.generation)+
                ",\"key\":"+std::to_string(source.key)+",\"reason\":\""+escaped(gesture->drag_visual_limit_reason)+"\",\"mode\":\""+mode+
                "\",\"text\":\""+escaped(utf8(overlay_hint))+"\",\"overlay_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(overlay))+
                ",\"requested_bounds\":"+rectangle(bounds)+",\"SetWindowText_succeeded\":"+(title_set?"true":"false")+
                ",\"SetWindowPos_attempted\":true,\"SetWindowPos_succeeded\":"+(positioned?"true":"false")+
                ",\"actual_visible\":"+(visible?"true":"false")+",\"source_is_foreground\":"+(GetForegroundWindow()==source.hwnd?"true":"false")+
                ",\"text_property_not_rendering_proof\":true");
            if(positioned&&visible){overlay_limit_generation=gesture->down.generation;overlay_limit_key=source.key;}
        }
        if(!positioned||!visible){hide_preview();return false;}
        InvalidateRect(overlay,nullptr,FALSE);return true;
    }
    bool show_drag_visual_limit(){
        const auto source=drag_visual_limit_source();if(!source)return false;
        show_feedback({*source},L"");return IsWindowVisible(overlay)!=FALSE;
    }
    void drag_visual_limited(const Snapshot& source,const char* reason,std::wstring text){
        if(!gesture||gesture->down.prepared->source.key!=source.key||!Catalog::same(gesture->down.prepared->source,source)||
            !input.allowed(gesture->down.generation))return;
        gesture->drag_visual_limit_reason=reason;gesture->drag_visual_limit=std::move(text);
        log.event("drag_transparency_limited","\"generation\":"+std::to_string(gesture->down.generation)+
            ",\"key\":"+std::to_string(source.key)+",\"reason\":\""+escaped(reason)+"\",\"text\":\""+
            escaped(utf8(gesture->drag_visual_limit))+"\",\"new_write_authority_granted\":false");
        // The existing preview window is non-activating and hit-transparent.
        // Unlike the tray tooltip/log, this paints during the actual gesture.
        show_drag_visual_limit();
    }
    void show_preview(std::optional<v::Rect> rect){
        // Idle preparation/hover is not a new gesture and must not immediately
        // erase a user-visible rejection. New DOWN/handoff retires it below.
        if(idle_notice_timer&&!gesture&&!native_move&&!input.native_move_in_progress())return;
        overlay_outline=false;overlay_members.clear();overlay_magnet_edges.clear();overlay_shared_edges.clear();overlay_hint.clear();overlay_bounds={};
        if(!overlay)return;
        const auto& config=gesture?gesture->config:settings;
        if(!rect||!config.preview){if(!show_drag_visual_limit())hide_preview();return;}
        overlay_color=static_cast<COLORREF>(config.preview_color);
        if(!SetLayeredWindowAttributes(overlay,0,static_cast<BYTE>(config.preview_alpha),LWA_ALPHA)){hide_preview();return;}
        const auto& r=*rect;
        constexpr auto low=std::numeric_limits<int>::min(),high=std::numeric_limits<int>::max();
        const auto width=g::checked_difference(r.right(),r.left()),height=g::checked_difference(r.bottom(),r.top());
        if(!width||!height||*width<=0||*height<=0||*width>high||*height>high||
            r.left()<low||r.top()<low||r.right()>high||r.bottom()>high){hide_preview();return;}
        const auto limited_source=drag_visual_limit_source();
        if(limited_source){overlay_hint=gesture->drag_visual_limit;overlay_dpi=limited_source->dpi;}
        overlay_bounds=r;position_overlay(r,"preview",limited_source.has_value());
    }
    void show_feedback(const std::vector<Snapshot>& actual,std::wstring hint,
        std::vector<FeedbackLine> magnet_edges={},std::vector<FeedbackLine> shared_edges={}){
        const bool limited=drag_visual_limit_source().has_value();
        if(!overlay||!gesture||(!gesture->config.feedback&&!limited)||!input.allowed(gesture->down.generation)||actual.empty()){
            hide_preview();return;
        }
        const bool only_limitation=limited&&(!gesture->config.feedback||(!gesture->ctrl&&magnet_edges.empty()));
        if(!gesture->ctrl&&magnet_edges.empty()&&!limited){hide_preview();return;}
        if(limited){if(!hint.empty())hint+=L"；";hint+=gesture->drag_visual_limit;}
        const auto displays=catalog.displays();if(displays.empty()){hide_preview();return;}
        constexpr auto lo=std::numeric_limits<std::int32_t>::min(),hi=std::numeric_limits<std::int32_t>::max();
        const auto native=[](const v::Rect& r){
            const auto width=g::checked_difference(r.right(),r.left()),height=g::checked_difference(r.bottom(),r.top());
            if(!width||!height||*width<=0||*height<=0||*width>hi||*height>hi)return false;
            for(const auto edge:{r.left(),r.top(),r.right(),r.bottom()})if(edge<lo||edge>hi)return false;
            return true;
        };
        auto desktop=displays.front().bounds;
        for(const auto& display:displays){
            if(!display.dpi){hide_preview();return;}
            desktop={std::min(desktop.left(),display.bounds.left()),std::min(desktop.top(),display.bounds.top()),
                std::max(desktop.right(),display.bounds.right()),std::max(desktop.bottom(),display.bounds.bottom())};
        }
        std::vector<v::Rect> clipped;
        for(const auto& snapshot:actual){
            if(!snapshot.dpi||!native(snapshot.visible)){hide_preview();return;}
            const auto visible=g::intersection(snapshot.visible,desktop);
            if(!visible){hide_preview();return;}clipped.push_back(*visible);
        }
        auto bounds=clipped.front();
        for(const auto& member:clipped)bounds={std::min(bounds.left(),member.left()),std::min(bounds.top(),member.top()),
            std::max(bounds.right(),member.right()),std::max(bounds.bottom(),member.bottom())};
        const auto caption=hint.empty()?0:dip(25,actual.front().dpi);
        const auto top=g::checked_add(bounds.top(),-caption);
        if(!top){hide_preview();return;}
        bounds={bounds.left(),std::max(desktop.top(),*top),bounds.right(),bounds.bottom()};
        if(only_limitation){
            // A small opaque system-colour strip remains available even with
            // optional magnet/group feedback disabled or no adopted edge.
            const auto work=g::intersection(actual.front().work,desktop);
            if(!work||work->height()<caption){hide_preview();return;}
            const auto width=std::min(work->width(),std::max(clipped.front().width(),dip(480,actual.front().dpi)));
            const auto left=std::clamp(clipped.front().left(),work->left(),work->right()-width);
            const auto y=std::clamp(*top,work->top(),work->bottom()-caption);
            bounds={left,y,left+width,y+caption};
        }
        if(!native(bounds)||!input.allowed(gesture->down.generation)){hide_preview();return;}
        const auto clip_lines=[&](std::vector<FeedbackLine>& lines){
            std::vector<FeedbackLine> result;
            for(auto line:lines){
                if(line.first.x==line.last.x&&line.first.x>=bounds.left()&&line.first.x<=bounds.right()){
                    line.first.y=std::max(line.first.y,bounds.top());line.last.y=std::min(line.last.y,bounds.bottom());
                    if(line.first.y<line.last.y)result.push_back(line);
                }else if(line.first.y==line.last.y&&line.first.y>=bounds.top()&&line.first.y<=bounds.bottom()){
                    line.first.x=std::max(line.first.x,bounds.left());line.last.x=std::min(line.last.x,bounds.right());
                    if(line.first.x<line.last.x)result.push_back(line);
                }
            }return result;
        };
        overlay_outline=true;overlay_bounds=bounds;overlay_members=gesture->ctrl&&!only_limitation?std::move(clipped):std::vector<v::Rect>{};overlay_hint=std::move(hint);
        overlay_magnet_edges=only_limitation?std::vector<FeedbackLine>{}:clip_lines(magnet_edges);
        overlay_shared_edges=only_limitation?std::vector<FeedbackLine>{}:clip_lines(shared_edges);
        overlay_dpi=actual.front().dpi;overlay_color=static_cast<COLORREF>(gesture->config.preview_color);
        for(const auto candidate:{RGB(1,2,3),RGB(4,5,6),RGB(7,8,9),RGB(10,11,12)}){
            if(candidate!=overlay_color&&candidate!=GetSysColor(COLOR_GRAYTEXT)&&candidate!=GetSysColor(COLOR_INFOBK)&&candidate!=GetSysColor(COLOR_INFOTEXT)){overlay_key=candidate;break;}
        }
        // The background is truly color-keyed out. Unlike a filled rectangle,
        // group feedback leaves the applications between the outlines visible.
        if(!SetLayeredWindowAttributes(overlay,overlay_key,255,LWA_COLORKEY|LWA_ALPHA)){hide_preview();return;}
        position_overlay(bounds,only_limitation?"limitation_strip":"feedback",limited);
    }
    void command(v::Action action,std::optional<Snapshot> target={},std::uint64_t hook_observed_qpc=0);
    void prepare();
    void refresh_idle_catalog(){
        if(!idle_catalog_observation_allowed(gesture.has_value()||native_move.has_value()||input.native_move_in_progress(),writer.has_pending_native_operation()))return;
        const auto front=GetForegroundWindow();
        // This is a read-only last-target observation, not an operation's
        // declared tracking-limit admission or issued-call postverification.
        if(auto target=catalog.get(front,settings,nullptr,false,false))last_target=*target;
        prepare();
    }
    void begin(const InputEvent&,ContextualBeginTiming* = nullptr);
    void native_move_event(const InputEvent&);
    void native_move_resource(const InputEvent&);
    bool native_group_unchanged(const InputEvent& down,const Snapshot& actual_source){
        if(!down.prepared)return false;const auto& prepared=*down.prepared;
        if(!(down.modifiers&2u)||!settings.glue||!(prepared.source.feature_mask&2u))return true;
        if(!prepared.native_ctrl_members_valid||prepared.native_ctrl_members.empty())return false;
        bool source_found=false;
        for(const auto& member:prepared.native_ctrl_members){
            if(member.key==prepared.source.key){
                source_found=Catalog::same(member,prepared.source)&&native_geometry_handoff_compatible(prepared,actual_source);
                if(!source_found)return false;
            }else{
                const auto actual=catalog.get(member.hwnd,settings,nullptr,false,false);
                if(!actual||actual->feature_mask!=member.feature_mask||
                   !WindowWriter::prepare(member,*actual,member.visible).accepted)return false;
            }
        }
        return source_found;
    }
    GestureGuiDisposition gesture_gui_context(const Snapshot& source,std::uint64_t generation) {
        LARGE_INTEGER before{},after{};if(diagnostic)QueryPerformanceCounter(&before);
        const auto gui=observe_gui_context(source);
        const auto capture=input.capture_context(generation);
        std::string reason;
        const bool accepted=geometry_gui_context_allowed(source,gui,capture,
            GeometryContextPhase::BeforeNativeCall,&reason);
        const auto disposition=gesture_gui_disposition(accepted,reason,generation,capture,input.allowed(generation));
        if(!accepted&&diagnostic){
            const auto foreground_after_predicate=GetForegroundWindow();
            QueryPerformanceCounter(&after);
            // GUI-owner evidence only, on the existing rejection path. Do not
            // let a later UP snapshot stand in for the context that retired a
            // gesture, and never perform this logging on the input thread.
            const auto hwnd=[](HWND value){return std::to_string(reinterpret_cast<std::uintptr_t>(value));};
            log.event("gesture_GUI_context_rejected","\"generation\":"+std::to_string(generation)+
                ",\"query_before_qpc\":"+std::to_string(before.QuadPart)+
                ",\"query_after_qpc\":"+std::to_string(after.QuadPart)+
                ",\"source\":"+hwnd(source.hwnd)+",\"source_key\":"+std::to_string(source.key)+
                ",\"reason\":\""+escaped(reason)+"\",\"GUI_context_mode\":\""+gui_context_mode_name(gui.mode)+
                "\",\"source_GUI_known\":"+(gui.known()?"true":"false")+
                ",\"source_GUI_error\":"+std::to_string(gui.owner_error)+
                ",\"observed_capture\":"+hwnd(gui.actual.hwndCapture)+
                ",\"observed_move_size\":"+hwnd(gui.actual.hwndMoveSize)+
                ",\"observed_menu_owner\":"+hwnd(gui.actual.hwndMenuOwner)+
                ",\"observed_GUI_flags\":"+std::to_string(gui.actual.flags)+
                ",\"foreground_after_predicate\":"+hwnd(foreground_after_predicate)+
                ",\"capture_lease_present\":"+(capture?"true":"false")+
                ",\"lease_overlay\":"+hwnd(capture?capture->overlay:nullptr)+
                ",\"lease_matching_UP_observed\":"+(capture&&capture->matching_up_observed.load()?"true":"false")+
                ",\"await_actual_terminal_receipt\":"+(disposition==GestureGuiDisposition::MatchingUpRetired?"true":"false")+
                ",\"lease_normal_UP_observed\":"+(capture&&capture->normal_up_observed.load()?"true":"false")+
                ",\"lease_own_release_confirmed\":"+(capture&&capture->own_release_confirmed.load()?"true":"false")+
                ",\"lease_association_detached\":"+(capture&&capture->association_detached.load()?"true":"false")+
                ",\"new_native_attempt\":false");
        }
        return disposition;
    }
    bool gesture_gui_allowed(const Snapshot& source,std::uint64_t generation) {
        // The native-handoff begin gate still requires actual admission; a
        // retired observation can never be used as a true may_write input.
        return gesture_gui_context(source,generation)==GestureGuiDisposition::Allowed;
    }
    void motion(const InputEvent&,bool normal_up_completion=false);
    void refresh_targets(){
        if(!gesture)return;auto& targets=gesture->targets;
        for(auto it=targets.begin();it!=targets.end();){
            const auto actual=catalog.get(it->hwnd,settings,nullptr,false,false);
            // Read-only magnet candidates are not writer participants. A moved,
            // minimized or closed neighbour changes the candidates, not source
            // authority. Never inherit a replaced HWND's old target lifetime.
            if(!actual||!Catalog::same(*it,*actual)||!(actual->feature_mask&1u))it=targets.erase(it);
            else{*it=*actual;++it;}
        }
    }
    void cancel_seam_timer(bool reset=true);
    bool dock_preview(POINT,std::uint64_t,bool arm=true,bool normal_up_completion=false);
    void seam_timeout(UINT_PTR);
    void request_native_restore(const InputEvent&);
    void restore_progress();
    void restore_timeout(UINT_PTR);
    void cancel_restore_wait();
    void finish(v::End,std::uint64_t observed_qpc=0);
    void consume();
    void drain_button_admissions(bool final=false);
    void show_settings();
    void register_keys();
    std::optional<std::vector<BoundKey>> prepare_keys(const v::Settings&);
    void discard_new_keys(const std::vector<BoundKey>&);
    void commit_keys(std::vector<BoundKey>);
    void export_diagnostic();
    void menu();
    void stop();
    void log_delayed_completions(){
        for(const auto& r:writer.take_delayed_completions(settings)){
            if(r.operation!=WindowWriter::Operation::Placement){
                bool acknowledged=false;
                if(const auto pending=pending_visual.find(r.call_id);pending!=pending_visual.end()){
                    if(r.completion_known&&r.identity_valid&&r.visual_after)
                        acknowledged=commit_visual(pending->second.identity,pending->second.planned,*r.visual_after);
                    if(pending->second.temporary_alpha&&gesture&&gesture->down.generation==pending->second.generation&&
                        drag_alpha_completion_failed(r.native_attempted,r.completion_known,r.native_succeeded,r.visual_exact))
                        drag_visual_limited(pending->second.identity,"issued_alpha_completion_not_exact",
                            L"本次拖动透明未完成，原状态恢复尚待确认");
                    // A failed commit leaves the helper's pending tuple intact.
                    // No new authority is granted by this late observation.
                    if(acknowledged)pending_visual.erase(pending);
                }
                log.event("property_native_completion","\"call_id\":"+std::to_string(r.call_id)+
                    ",\"key\":"+std::to_string(r.before.key)+",\"native_succeeded\":"+(r.native_succeeded?"true":"false")+
                    ",\"identity_valid\":"+(r.identity_valid?"true":"false")+",\"visual_exact\":"+(r.visual_exact?"true":"false")+
                    ",\"recovery_commit_ack\":"+(acknowledged?"true":"false")+",\"new_write_authority_granted\":false");
                continue;
            }
            const auto found=delayed_input_qpc.find(r.call_id);
            input_post_timing.observe_qpc(found==delayed_input_qpc.end()?0:found->second,r.post_observed_qpc,input.counter_frequency());
            if(found!=delayed_input_qpc.end())delayed_input_qpc.erase(found);
            log.event("native_completion",
            "\"call_id\":"+std::to_string(r.call_id)+",\"key\":"+std::to_string(r.before.key)+
            ",\"native_attempted\":"+(r.native_attempted?"true":"false")+",\"native_succeeded\":"+(r.native_succeeded?"true":"false")+
            ",\"native_completion_known\":"+(r.completion_known?"true":"false")+",\"error\":"+std::to_string(r.error)+
            ",\"target\":"+rectangle(r.requested)+",\"delayed_actual\":"+(r.after?rectangle(r.after->visible):"null")+
            ",\"identity_valid\":"+(r.identity_valid?"true":"false")+",\"geometry_exact_at_delayed_observation\":"+(r.geometry_exact?"true":"false")+
            ",\"native_before_qpc\":"+std::to_string(r.before_qpc)+",\"native_after_qpc\":"+std::to_string(r.after_qpc)+
            ",\"native_position_flags\":"+(r.operation==WindowWriter::Operation::Placement&&r.native_attempted&&r.completion_known?std::to_string(r.native_position_flags):"null")+
            ",\"preflight_started_qpc\":"+std::to_string(r.preflight_started_qpc)+
            ",\"geometry_observed_qpc\":"+std::to_string(r.geometry_observed_qpc)+
            ",\"post_observed_qpc\":"+std::to_string(r.post_observed_qpc)+
            ",\"post_track_limits_query_requested\":"+(r.post_track_limits_query_requested?(*r.post_track_limits_query_requested?"true":"false"):"null")+
            ",\"post_track_limits_carried_from_preflight\":"+(r.post_track_limits_carried_from_preflight?"true":"false")+
            ",\"explicit_destination_display_bound\":"+(r.destination?"true":"false")+
            ",\"destination_monitor\":"+std::to_string(r.destination?reinterpret_cast<std::uintptr_t>(r.destination->target.monitor):0)+
            ",\"destination_window_dpi\":"+std::to_string(r.destination?r.destination->window_dpi:0)+
            ",\"post_context_valid\":"+(r.post_context_valid?"true":"false")+
            ",\"post_context_reason\":\""+escaped(r.post_context_reason)+"\",\"new_write_authority_granted\":false");
        }
        retry_deferred_drag_visual();
    }
};
Application* application{};

void Application::command(v::Action action,std::optional<Snapshot> target,std::uint64_t hook_observed_qpc){
    if(action==v::Action::Pause){set_paused(settings.enabled);return;}
    if(action==v::Action::Settings){show_settings();return;}if(action==v::Action::Exit){DestroyWindow(window);return;}
    if(!settings.enabled){notify(L"增强已暂停；请先恢复后再执行窗口命令。");return;}
    if(gesture||native_move||input.native_move_in_progress())finish(v::End::Paused);
    // A hotkey may be the first action after startup, without mouse motion.
    // Tray commands may fall back to the last admitted app when our menu owns
    // foreground; never accept an unrelated stale HWND without recapture.
    const auto foreground=target?std::optional<Snapshot>{}:catalog.get(GetForegroundWindow(),settings);
    const auto source=target?target:foreground?foreground:last_target;if(!source){notify(L"没有可操作的普通前台窗口。");return;}
    std::string admission_reason;auto now=catalog.get(source->hwnd,settings,&admission_reason,action==v::Action::Restore);
    if(!now||!Catalog::same(*source,*now)){
        log.event("command_rejected","\"action\":\""+std::string(v::action_name(action))+"\",\"reason\":\""+
            escaped(now?"identity_changed":admission_reason)+"\",\"expected_key\":"+std::to_string(source->key)+
            ",\"actual_key\":"+(now?std::to_string(now->key):"null"));
        notify(L"目标窗口资格已经失效。");return;
    }
    auto s=*now;if(!(s.feature_mask&16u)){notify(L"应用规则禁用了窗口命令。");return;}last_target=s;v::Rect desired=s.visible;bool geometry=true,keep_history=true;
    const auto step=dip(settings.step_dip,s.dpi);auto others=items(catalog.all(settings));
    others.erase(std::remove_if(others.begin(),others.end(),[&](const auto& item){return item.id==s.key;}),others.end());
    switch(action){
    case v::Action::Minimize:ShowWindowAsync(s.hwnd,SW_MINIMIZE);geometry=false;break;
    case v::Action::Maximize:if(!s.maximized)ShowWindowAsync(s.hwnd,SW_MAXIMIZE);geometry=false;break;
    case v::Action::Restore:
        if(IsZoomed(s.hwnd)||IsIconic(s.hwnd)){ShowWindowAsync(s.hwnd,SW_RESTORE);geometry=false;}
        else{const auto history=owned_layout(s);if(!history){geometry=false;break;}
            desired=history->back().before.visible;keep_history=false;}
        break;
    case v::Action::Lower:{const auto receipt=writer.lower(s,settings,input.operation_guard(0));
        const auto z_sample=[](const WindowWriter::LowerZOrderSample& z){return
            "{\"known\":"+std::string(z.known?"true":"false")+",\"shell_known\":"+(z.shell_known?"true":"false")+
            ",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.source))+
            ",\"source_pid\":"+std::to_string(z.source_pid)+",\"source_tid\":"+std::to_string(z.source_tid)+
            ",\"previous\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.previous))+",\"next\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.next))+
            ",\"shell\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.shell))+
            ",\"shell_pid\":"+std::to_string(z.shell_pid)+",\"shell_tid\":"+std::to_string(z.shell_tid)+
            ",\"shell_previous\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.shell_previous))+
            ",\"shell_next\":"+std::to_string(reinterpret_cast<std::uintptr_t>(z.shell_next))+"}";};
        log.event("lower_result","\"key\":"+std::to_string(s.key)+",\"call_id\":"+std::to_string(receipt.call_id)+
            ",\"native_attempted\":"+(receipt.native_attempted?(receipt.completion_known?"true":"null"):"false")+
            ",\"native_completion_known\":"+(receipt.completion_known?"true":"false")+
            ",\"native_succeeded\":"+(receipt.native_succeeded?"true":"false")+
            ",\"bottom_observed\":"+(receipt.z_bottom_observed?"true":"false")+
            ",\"lower_observed\":"+(receipt.z_lower_observed?"true":"false")+
            ",\"shell_desktop_tail_observed\":"+(receipt.z_lower_kind==WindowWriter::LowerZOrderKind::ShellDesktopTail?"true":"false")+
            ",\"actual_previous\":"+std::to_string(reinterpret_cast<std::uintptr_t>(receipt.z_previous))+
            ",\"actual_next\":"+std::to_string(reinterpret_cast<std::uintptr_t>(receipt.z_next))+
            ",\"z_observation_begin_qpc\":"+std::to_string(receipt.z_lower_begin_qpc)+",\"z_observation_end_qpc\":"+std::to_string(receipt.z_lower_end_qpc)+
            ",\"z_samples\":["+z_sample(receipt.z_lower_samples[0])+","+z_sample(receipt.z_lower_samples[1])+
            "],\"global_atomic_Z_snapshot\":false"+
            ",\"reason\":\""+escaped(receipt.reason)+"\"");
        if(!receipt.accepted)notify(WindowWriter::lower_failure_notice(receipt));geometry=false;break;}
    case v::Action::Center:{const auto l=s.work.left()+(s.work.width()-s.visible.width())/2,t=s.work.top()+(s.work.height()-s.visible.height())/2;
        desired={l,t,l+s.visible.width(),t+s.visible.height()};break;}
    case v::Action::MoveLeft:case v::Action::MoveRight:case v::Action::MoveUp:case v::Action::MoveDown:{
        desired=core::movement::translate_rect(s.visible,{action==v::Action::MoveLeft?-step:action==v::Action::MoveRight?step:0,
            action==v::Action::MoveUp?-step:action==v::Action::MoveDown?step:0});break;}
    case v::Action::WidthLess:case v::Action::WidthMore:case v::Action::HeightLess:case v::Action::HeightMore:{
        const bool width=action==v::Action::WidthLess||action==v::Action::WidthMore,more=action==v::Action::WidthMore||action==v::Action::HeightMore;
        const auto plan=v::resize_single({s.key,s.visible,s.limits},{false,false,width,!width},{width?(more?step:-step):0,width?0:(more?step:-step)});
        if(!plan.accepted){notify(L"尺寸约束不允许此调整。");return;}if(!plan.placements.empty())desired=plan.placements[0].visible;break;}
    case v::Action::NextDisplay:{auto displays=catalog.displays();if(displays.size()<2){notify(L"当前只有一个显示器。");return;}
        const auto it=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==s.monitor;});
        if(it==displays.end()||!it->dpi){notify(L"源显示器或实际 DPI 不可确认，未移动窗口。");return;}
        std::vector<v::Rect> bounds;bounds.reserve(displays.size());for(const auto& display:displays)bounds.push_back(display.bounds);
        const auto index=v::nearest_adjacent_display(bounds,static_cast<std::size_t>(it-displays.begin()));
        if(!index){notify(L"没有可确认的相邻显示器，未移动窗口。");return;}const auto& d=displays[*index];
        if(!d.dpi){notify(L"目标显示器 DPI 不可确认，未移动窗口。");return;}
        const auto scaled=[&](std::int64_t pixels)->std::optional<std::int64_t>{
            const auto round=static_cast<std::int64_t>(it->dpi/2);
            if(pixels<=0||pixels>(std::numeric_limits<std::int64_t>::max()-round)/d.dpi)return {};
            return std::max<std::int64_t>(1,(pixels*d.dpi+round)/it->dpi);
        };
        const auto width=scaled(s.visible.width()),height=scaled(s.visible.height());
        if(!width||!height){notify(L"跨屏尺寸计算溢出，未移动窗口。");return;}
        auto limits=s.limits;limits.max_width=std::min(limits.max_width,d.work.width());limits.max_height=std::min(limits.max_height,d.work.height());
        const auto sized=v::resize_single({s.key,s.visible,limits},{false,false,true,true},{*width-s.visible.width(),*height-s.visible.height()});
        if(!sized.accepted){notify(L"目标工作区与窗口尺寸约束没有交集，未移动窗口。");return;}
        const auto size=sized.placements.empty()?s.visible:sized.placements.front().visible;
        desired={d.work.left(),d.work.top(),d.work.left()+size.width(),d.work.top()+size.height()};break;}
    case v::Action::ToggleTopmost:case v::Action::OpacityDown:case v::Action::OpacityUp:{
        if(!(s.feature_mask&32u))return;
        if(!visual_action(s,action))notify(L"应用不支持该透明度／置顶操作或属性已变化，未假报成功。");geometry=false;break;}
    case v::Action::Undo:{
        if(s.maximized||s.minimized){ShowWindowAsync(s.hwnd,SW_RESTORE);geometry=false;break;}
        const auto history=owned_layout(s);if(!history){notify(L"当前窗口已改变，或没有仍归属本窗口的布局可撤销。");return;}
        desired=history->back().before.visible;keep_history=false;break;}
    case v::Action::StretchLeft:case v::Action::StretchRight:case v::Action::StretchTop:case v::Action::StretchBottom:
    case v::Action::StretchHorizontal:case v::Action::StretchVertical:case v::Action::StretchAll:{if(!settings.stretch||!(s.feature_mask&8u))return;
        desired=v::stretch(s.visible,s.work,stretch_edges(action),false,others,settings.smart_stretch);break;}
    default:if(action>=v::Action::DockLeft&&action<=v::Action::DockBottomRight&&settings.docking&&(s.feature_mask&4u))
        desired=v::smart_dock(s.work,action,others,settings.smart_docking);else return;
    }
    if(geometry){if(s.maximized){notify(L"请先恢复窗口再执行该几何操作。");return;}
        if(apply({true,"command",{{s.key,desired}}},{s},0,false,hook_observed_qpc)){
            if(keep_history)remember_receipt(s,(action>=v::Action::DockLeft&&action<=v::Action::DockBottomRight)||
                (action>=v::Action::StretchLeft&&action<=v::Action::StretchAll));
            else{auto& history=undo[s.key];history.pop_back();
                if(history.empty())undo.erase(s.key);
                else if(const auto actual=last_exact_receipts.find(s.key);actual!=last_exact_receipts.end())history.back().after=actual->second;
            }
        }
        else notify(L"窗口操作未精确完成；已停止，详见诊断日志。");}
    log.event("command","\"action\":\""+std::string(v::action_name(action))+"\",\"key\":"+std::to_string(s.key));
}
void Application::prepare(){
    input.prepared_point();if(gesture||native_move||input.native_move_in_progress()||!settings.enabled)return;
    const auto point=input.latest();HWND hit=WindowFromPoint(point),root=hit?GetAncestor(hit,GA_ROOT):nullptr;
    // Hover preparation needs identity/physical geometry, not repeated foreign
    // tracking-limit messages. Only the frozen operation members below query
    // actual declared bounds before any placement; an unknown reply still
    // rejects that operation rather than silently authorizing a resize.
    std::string admission;const auto snapshot=root?catalog.get(root,settings,&admission,false,false):std::nullopt;
    GuiContext gui_context;auto& gui=gui_context.actual;bool gui_read=false,hit_read=false;DWORD_PTR value{};int region=-1;DWORD hit_pid{};
    const auto query_dpi_context=GetThreadDpiAwarenessContext();
    const auto window_dpi_context=root?GetWindowDpiAwarenessContext(root):nullptr;
    const UINT window_dpi=root?GetDpiForWindow(root):0;
    RECT caption_buttons{},caption_window{};HRESULT caption_hresult=E_PENDING;
    bool caption_checked=false,caption_window_known=false,caption_standard_buttons=false;
    auto caption_result=v::CaptionHit::Unknown;
    const auto raw_caption_rect=[](const RECT& r){return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+
        std::to_string(r.right)+","+std::to_string(r.bottom)+"]";};
    const auto explain=[&](std::string_view reason){
        if(!diagnostic)return;
        const auto key=snapshot?snapshot->key:0;
        const auto identity=std::string(reason)+":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+":"+
            std::to_string(key)+":"+std::to_string(region)+":"+gui_context_mode_name(gui_context.mode)+":"+
            (gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"unknown");
        // Point is evidence, not part of the dedup key: motion within one
        // prepared/rejected region does not create a per-mouse log stream.
        if(identity==last_prepare_diagnostic)return;last_prepare_diagnostic=identity;
        log.event("input_preparation","\"reason\":\""+escaped(reason)+"\",\"root\":"+std::to_string(reinterpret_cast<std::uintptr_t>(root))+
            ",\"key\":"+std::to_string(key)+",\"hit\":"+std::to_string(reinterpret_cast<std::uintptr_t>(hit))+
            ",\"hit_pid\":"+std::to_string(hit_pid)+",\"point\":["+std::to_string(point.x)+","+std::to_string(point.y)+"]"+
            ",\"foreground\":"+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+
            ",\"region\":"+std::to_string(region)+",\"actual_hit_test\":"+(hit_read?std::to_string(static_cast<LRESULT>(value)):"null")+
            ",\"GUI_context_mode\":\""+gui_context_mode_name(gui_context.mode)+"\",\"GUI_owner_query_error\":"+std::to_string(gui_context.owner_error)+
            ",\"GUI_foreground_query_error\":"+std::to_string(gui_context.foreground_error)+
            ",\"source_GUI_read\":"+(gui_read?"true":"false")+",\"capture\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndCapture)):"null")+
            ",\"move_size\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMoveSize)):"null")+
            ",\"menu_owner\":"+(gui_read?std::to_string(reinterpret_cast<std::uintptr_t>(gui.hwndMenuOwner)):"null")+
            ",\"query_thread_DPI_context\":"+std::to_string(reinterpret_cast<std::intptr_t>(query_dpi_context))+
            ",\"query_thread_PMV2\":"+(AreDpiAwarenessContextsEqual(query_dpi_context,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)?"true":"false")+
            ",\"window_DPI_context\":"+(window_dpi_context?std::to_string(reinterpret_cast<std::intptr_t>(window_dpi_context)):"null")+
            ",\"window_DPI\":"+(window_dpi?std::to_string(window_dpi):"null")+
            ",\"caption_buttons_checked\":"+(caption_checked?"true":"false")+
            ",\"caption_buttons_hresult\":"+(caption_checked?std::to_string(caption_hresult):"null")+
            ",\"caption_buttons_window_relative\":"+(caption_checked&&SUCCEEDED(caption_hresult)?
                raw_caption_rect(caption_buttons):"null")+
            ",\"caption_positioning\":"+(caption_window_known?
                raw_caption_rect(caption_window):"null")+
            ",\"standard_caption_buttons_possible\":"+(caption_checked?(caption_standard_buttons?"true":"false"):"null")+
            ",\"caption_button_guard\":"+(caption_checked?"\""+std::string(v::caption_hit_name(caption_result))+"\"":"null")+
            ",\"config_generation\":"+std::to_string(settings.generation));
    };
    if(!snapshot||GetForegroundWindow()!=root){explain(!snapshot?(root?admission:"no_actual_hit_root"):"source_not_actual_foreground");input.prepare({});show_preview({});return;}
    gui_context=observe_gui_context(*snapshot);gui_read=gui_context.known();
    if(!gui_context.idle()){explain(gui_read?"source_native_GUI_interaction_present":"source_GUI_unreadable");input.prepare({});show_preview({});return;}
    if(!gesture)owned_layout(*snapshot);
    last_target=snapshot;
    const auto lp=MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y));GetWindowThreadProcessId(hit,&hit_pid);
    // WM_NCHITTEST carries signed 16-bit screen coordinates. Do not query a
    // different wrapped point on a virtual desktop outside that message range.
    const bool hit_point_representable=point.x>=std::numeric_limits<short>::min()&&point.x<=std::numeric_limits<short>::max()&&
        point.y>=std::numeric_limits<short>::min()&&point.y<=std::numeric_limits<short>::max();
    hit_read=hit_point_representable&&hit_pid==snapshot->pid&&SendMessageTimeoutW(hit,WM_NCHITTEST,0,lp,SMTO_ABORTIFHUNG|SMTO_BLOCK,30,&value)!=FALSE;
    if(hit_read){
        switch(static_cast<LRESULT>(value)){case HTCAPTION:region=0;break;case HTLEFT:region=1;break;case HTRIGHT:region=2;break;
        case HTTOP:region=3;break;case HTBOTTOM:region=4;break;case HTTOPLEFT:region=5;break;case HTTOPRIGHT:region=6;break;
        case HTBOTTOMLEFT:region=7;break;case HTBOTTOMRIGHT:region=8;break;default:break;}
    }
    if(region==0){
        // This is an independent exclusion, not a replacement hit test and
        // never a call into a foreign DwmDefWindowProc. Preserve raw bounds:
        // DWM returns WINDOW-relative edges, so use the positioning origin.
        caption_checked=true;caption_hresult=DwmGetWindowAttribute(root,DWMWA_CAPTION_BUTTON_BOUNDS,&caption_buttons,sizeof(caption_buttons));
        caption_window_known=GetWindowRect(root,&caption_window)!=FALSE;
        SetLastError(0);const auto style=GetWindowLongPtrW(root,GWL_STYLE);const bool style_known=style!=0||GetLastError()==0;
        caption_standard_buttons=!style_known||((style&WS_CAPTION)==WS_CAPTION&&(style&(WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX))!=0);
        const v::CaptionButtonFacts facts{{point.x,point.y},
            {caption_window.left,caption_window.top,caption_window.right,caption_window.bottom},
            {caption_buttons.left,caption_buttons.top,caption_buttons.right,caption_buttons.bottom},
            caption_window_known,SUCCEEDED(caption_hresult),
            query_dpi_context&&window_dpi_context&&window_dpi&&AreDpiAwarenessContextsEqual(query_dpi_context,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2),
            IsWindowVisible(root)&&!IsIconic(root),caption_standard_buttons};
        caption_result=v::caption_hit(facts);
        if(caption_result!=v::CaptionHit::Caption){explain(v::caption_hit_name(caption_result));input.prepare({});show_preview({});return;}
    }
    if(region>0&&(!snapshot->can_resize||snapshot->maximized))region=-1;if(region<0){explain(hit_read?"actual_hit_not_eligible_titlebar_or_resize":"actual_hit_test_unavailable_or_foreign_child");input.prepare({});show_preview({});return;}
    auto ready=std::make_shared<Prepared>();ready->source=*snapshot;ready->point=point;ready->stamp=GetTickCount();ready->region=region;ready->config_generation=settings.generation;
    if(region==0&&!snapshot->maximized)if(const auto history=owned_layout(*snapshot);history&&history->back().drag_restore)
        ready->layout_restore_normal=history->back().normal;
    if(!snapshot->maximized&&settings.glue&&(snapshot->feature_mask&2u)){
        // Freeze original group geometry before Windows can move/resize the
        // leader. The subsequent native prefix must not dissolve the group or
        // silently accept a moved follower as a new DOWN baseline. Only the
        // chosen participants query actual tracking limits in begin().
        auto members=catalog.all(settings);
        members.erase(std::remove_if(members.begin(),members.end(),[](const auto& member){return !(member.feature_mask&2u);}),members.end());
        const auto source=std::find_if(members.begin(),members.end(),[&](const auto& member){return member.key==snapshot->key;});
        if(source!=members.end()&&Catalog::same(*source,*snapshot)&&source->visible==snapshot->visible&&source->positioning==snapshot->positioning){
            *source=*snapshot;
            const auto selected=v::connected(items(members),snapshot->key,dip(settings.adjacency_dip,snapshot->dpi));
            members.erase(std::remove_if(members.begin(),members.end(),[&](const auto& member){
                return std::find(selected.begin(),selected.end(),member.key)==selected.end();}),members.end());
            ready->native_ctrl_members_valid=!members.empty();ready->native_ctrl_members=std::move(members);
        }
    }
    prepare_mappings(*ready,settings);
    // A read-only hover preview for the actual configured left double-click.
    // It does not claim that a double-click occurred or authorize any writer.
    std::optional<v::Rect> stretch_preview;
    if(region>0&&settings.stretch&&(snapshot->feature_mask&8u)){
        const unsigned mods=((GetAsyncKeyState(VK_MENU)&0x8000)?1u:0u)|((GetAsyncKeyState(VK_CONTROL)&0x8000)?2u:0u)|
            ((GetAsyncKeyState(VK_SHIFT)&0x8000)?4u:0u)|(((GetAsyncKeyState(VK_LWIN)|GetAsyncKeyState(VK_RWIN))&0x8000)?8u:0u);
        const auto route=route_down(*ready,1,2,mods);
        const bool mapped=route.route==DownRoute::Mapped&&route.action>=v::Action::StretchLeft&&route.action<=v::Action::StretchAll;
        const bool corner=route.route==DownRoute::Gesture&&region>=5;
        if(mapped||corner){auto other=catalog.all(settings);
            other.erase(std::remove_if(other.begin(),other.end(),[&](const auto& item){return item.key==snapshot->key;}),other.end());
            stretch_preview=v::stretch(snapshot->visible,snapshot->work,mapped?stretch_edges(route.action):edges(region),corner&&(mods&4u),items(other),settings.smart_stretch);
        }
    }
    show_preview(stretch_preview);
    input.prepare(std::move(ready));explain("prepared_actual_source_and_hit");
}
namespace {
bool native_move_buttons_held(SHORT* observed_left=nullptr){
    const SHORT left=GetAsyncKeyState(VK_LBUTTON);if(observed_left)*observed_left=left;
    return (left&0x8000)!=0&&
        !(GetAsyncKeyState(VK_RBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_MBUTTON)&0x8000)&&
        !(GetAsyncKeyState(VK_XBUTTON1)&0x8000)&&!(GetAsyncKeyState(VK_XBUTTON2)&0x8000);
}
bool native_move_overlay_hits(HWND overlay){
    if(!overlay||!IsWindow(overlay)||!IsWindowVisible(overlay))return false;
    DWORD pid{};if(!GetWindowThreadProcessId(overlay,&pid)||pid!=GetCurrentProcessId())return false;
    const auto ex=GetWindowLongPtrW(overlay,GWL_EXSTYLE);
    if((ex&(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_LAYERED))!=(WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_LAYERED)||
       (ex&WS_EX_TRANSPARENT))return false;
    POINT cursor{};return GetCursorPos(&cursor)&&WindowFromPoint(cursor)==overlay;
}
bool native_move_gui(const Snapshot& source,bool native){
    const auto gui=observe_gui_context(source);
    if(!gui.known()||gui.actual.hwndMenuOwner||
       (gui.actual.flags&(GUI_INMENUMODE|GUI_SYSTEMMENUMODE|GUI_POPUPMENUMODE)))return false;
    return native?(gui.actual.hwndCapture==source.hwnd&&gui.actual.hwndMoveSize==source.hwnd):gui.idle();
}
}
void Application::native_move_event(const InputEvent& e){
    if(idle_notice_timer&&(e.kind==InputKind::NativeMovePending||e.kind==InputKind::NativeMoveStart))hide_preview("new_native_handoff");
    const auto generation=e.generation;
    const auto reject=[&](const char* reason){
        log.event("native_move_handoff","\"generation\":"+std::to_string(generation)+
            ",\"accepted\":false,\"reason\":\""+reason+"\",\"new_native_placement\":false");
        input.revoke_generation(generation);
        if(native_move&&native_move->down.generation==generation)native_move.reset();
    };
    if(e.kind==InputKind::NativeMovePending){
        log.event("native_move_pending","\"generation\":"+std::to_string(generation)+
            ",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(e.prepared?e.prepared->source.hwnd:nullptr))+
            ",\"source_key\":"+std::to_string(e.prepared?e.prepared->source.key:0)+
            ",\"source_pid\":"+std::to_string(e.prepared?e.prepared->source.pid:0)+
            ",\"source_tid\":"+std::to_string(e.prepared?e.prepared->source.tid:0)+
            ",\"actual_DOWN_region\":"+std::to_string(e.prepared?e.prepared->region:-1)+
            ",\"locked_modifiers\":"+std::to_string(e.modifiers)+
            ",\"down_point\":["+std::to_string(e.point.x)+","+std::to_string(e.point.y)+"]"+
            ",\"down_native_time\":"+std::to_string(e.native_message_time)+
            ",\"down_hook_qpc\":"+std::to_string(e.hook_observed_qpc)+
            ",\"down_consumed\":false,\"write_authority\":false");
        if(input.native_move_pending(generation)&&(!native_move||native_move->down.generation!=generation)){
            NativeMovePending pending;pending.down=e;native_move=std::move(pending);
        }
        return;
    }
    if(e.kind==InputKind::NativeMoveReleased){
        log.event("native_move_passthrough_release","\"generation\":"+std::to_string(generation)+
            ",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(e.prepared?e.prepared->source.hwnd:nullptr))+
            ",\"actual_LL_UP_qpc\":"+std::to_string(e.hook_observed_qpc)+
            ",\"up_native_time\":"+std::to_string(e.native_message_time)+
            ",\"normalized_NormalUp\":false,\"new_native_placement\":false");
        const bool refresh=native_release_requests_preparation(e,native_move?native_move->down.generation:0,
            gesture.has_value(),input.native_release_preparation_idle(),settings.enabled);
        if(native_move&&native_move->down.generation==generation)native_move.reset();
        // reject() may already have retired this GUI slot. In both orders the
        // actual UP cleared the hook cache; honor it with one fresh GUI sample.
        // prepare() keeps every current identity/geometry/foreground/hit gate.
        if(refresh)prepare();return;
    }
    if(!e.prepared||!e.native_move_receipt)return reject("missing_actual_native_receipt");
    const auto& actual=*e.native_move_receipt;
    SHORT async_left{};const bool buttons_held=native_move_buttons_held(&async_left);
    log.event(e.kind==InputKind::NativeMoveStart?"native_move_start":"native_move_end",
        "\"generation\":"+std::to_string(generation)+",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(actual.source))+
        ",\"event\":"+std::to_string(actual.event)+",\"event_tid\":"+std::to_string(actual.thread_id)+
        ",\"native_time\":"+std::to_string(actual.stamp)+",\"owner_sequence\":"+std::to_string(actual.sequence)+
        ",\"original_DOWN_qpc\":"+std::to_string(e.hook_observed_qpc)+
        ",\"down_native_time\":"+std::to_string(e.native_message_time)+
        ",\"DOWN_classified_operation\":\""+(e.prepared->region==0?std::string("titlebar_move"):std::string("resize"))+"\""+
        ",\"async_left_return\":"+std::to_string(async_left)+
        ",\"native_physical_button_gate\":"+(buttons_held?"true":"false")+
        ",\"receipt_not_write_authority\":true");
    if(!input.native_move_pending(generation)||!settings.enabled||stopping||gesture||writer.has_pending_native_operation()||
       e.prepared->config_generation!=settings.generation||GetForegroundWindow()!=e.prepared->source.hwnd||
       !buttons_held)return reject("up_revoke_or_context_before_native_handoff");
    const auto fresh=catalog.get(e.prepared->source.hwnd,settings,nullptr,false,false);
    if(!fresh||!native_geometry_handoff_compatible(*e.prepared,*fresh))return reject("source_or_geometry_context_changed");
    if(!native_arranged_handoff_compatible(*e.prepared,*fresh,e.button,e.clicks,e.modifiers))
        return reject("plain_move_arranged_state_unknown_or_changed");
    if(!native_group_unchanged(e,*fresh))return reject("original_DOWN_group_changed_before_handoff");
    if(e.kind==InputKind::NativeMoveStart){
        if((native_move&&(native_move->down.generation!=generation||native_move->start))||!native_move_gui(*fresh,true)||
           !native_move_receipt_matches(actual,*e.prepared,generation,e.native_message_time,EVENT_SYSTEM_MOVESIZESTART))
            return reject("native_START_or_actual_move_loop_rejected");
        NativeMovePending pending;pending.down=e;pending.down.kind=InputKind::NativeMovePending;
        pending.start=e.native_move_receipt;
        const bool classified=native_geometry_entry(input.capture_route_enabled(),*e.prepared,e.button,e.clicks,e.modifiers,
            route_down(*e.prepared,e.button,e.clicks,e.modifiers));
        const auto operation=!classified?core::behavior::NativeGeometryOperation::Unclassified:
            (e.prepared->region==0?core::behavior::NativeGeometryOperation::TitlebarMove:core::behavior::NativeGeometryOperation::Resize);
        if(!pending.gate.begin_native_geometry(generation,Catalog::same(e.prepared->source,*fresh),buttons_held,operation))
            return reject("native_handoff_gate_rejected");
        native_move=std::move(pending);
        if(!input.request_native_move_isolation(generation,e.native_move_receipt))return reject("isolation_request_rejected");
        return;
    }
    if(!native_move||native_move->down.generation!=generation||!native_move->cancel_succeeded||
       !native_move->gate.cancel_sent()||!native_move->start||!native_move_end_follows(*native_move->start,actual)||
       !native_move_receipt_matches(actual,*e.prepared,generation,e.native_message_time,EVENT_SYSTEM_MOVESIZEEND)||
       !native_move_gui(*fresh,false)||!native_move_overlay_hits(native_move->overlay)||
       !native_move->gate.native_end_observed(generation,true))return reject("END_without_accepted_cancel_or_fresh_idle");
    native_move->end=e.native_move_receipt;
    if(!input.request_native_move_capture(generation,e.native_move_receipt,*fresh))return reject("capture_request_after_actual_END_rejected");
    log.event("native_move_handoff","\"generation\":"+std::to_string(generation)+
        ",\"accepted\":true,\"stage\":\"actual_END_capture_pending\",\"intent_anchor\":"+rectangle(e.prepared->source.visible)+
        ",\"continuity_baseline\":"+rectangle(fresh->visible)+",\"new_native_placement\":false");
}
void Application::native_move_resource(const InputEvent& e){
    if(!e.resource_event||!native_move||native_move->down.generation!=e.generation)return;
    const auto& fact=*e.resource_event;auto& pending=*native_move;
    if(fact.kind==o::GestureShieldEventKind::IsolationGone){native_move.reset();return;}
    if(fact.kind!=o::GestureShieldEventKind::IsolationReady)return;
    const auto g=e.generation;
    const auto reject=[&](const char* why,const char* first_failed_gate=nullptr){
        log.event("native_move_cancel","\"generation\":"+std::to_string(g)+
            ",\"native_attempted\":false,\"reason\":\""+why+"\""+
            (first_failed_gate?",\"first_failed_gate\":\""+std::string(first_failed_gate)+"\"":""));
        input.revoke_generation(g);native_move.reset();};
    const auto& source=pending.down.prepared->source;
    const auto fresh=catalog.get(source.hwnd,settings,nullptr,false,false);
    // Preserve the former OR's exact order/short circuit, including the fresh
    // capture above the pending check. Never eagerly sample native helpers for
    // logging: UP/foreground/context can change, and later gates must not run
    // after an earlier rejection. This names one observed first failure only.
    const char* first_failed_gate=nullptr;
    if(!input.native_move_pending(g))first_failed_gate="pending_generation";
    else if(!fresh)first_failed_gate="fresh_source_snapshot";
    else if(!native_geometry_handoff_compatible(*pending.down.prepared,*fresh))first_failed_gate="geometry_handoff";
    else if(!native_arranged_handoff_compatible(*pending.down.prepared,*fresh,pending.down.button,pending.down.clicks,pending.down.modifiers))first_failed_gate="arranged_handoff";
    else if(!settings.enabled)first_failed_gate="product_enabled";
    else if(stopping)first_failed_gate="product_not_stopping";
    else if(pending.down.prepared->config_generation!=settings.generation)first_failed_gate="configuration_generation";
    else if(!native_group_unchanged(pending.down,*fresh))first_failed_gate="original_DOWN_group";
    else if(!pending.start)first_failed_gate="native_START_receipt";
    else if(fact.entry_mode!=o::GestureShieldEntryMode::NativeMoveHandoff)first_failed_gate="isolation_entry_mode";
    else if(fact.gesture.generation!=g)first_failed_gate="isolation_generation";
    else if(fact.gesture.source!=source.hwnd)first_failed_gate="isolation_source_HWND";
    else if(fact.gesture.process_id!=source.pid)first_failed_gate="isolation_source_PID";
    else if(fact.gesture.thread_id!=source.tid)first_failed_gate="isolation_source_TID";
    else if(GetForegroundWindow()!=source.hwnd)first_failed_gate="source_foreground";
    else if(!native_move_buttons_held())first_failed_gate="native_buttons_held";
    else if(!native_move_gui(*fresh,true))first_failed_gate="native_GUI_move_context";
    else if(!native_move_overlay_hits(fact.overlay))first_failed_gate="own_isolation_hit";
    if(first_failed_gate)return reject("fresh_isolation_or_source_rejected",first_failed_gate);
    if(!pending.gate.isolation_ready(g,true)||!pending.gate.may_cancel(g)||
       !input.claim_native_move_cancel(g)||!pending.gate.cancel_issued(g))return reject("cancel_claim_revoked");
    pending.overlay=fact.overlay;
    // The input-owner's exact-generation claim is not a successful API call.
    // It is never held as a mutex across this one bounded foreign call.
    if(!input.native_move_pending(g))return reject("revoked_after_cancel_claim");
    DWORD_PTR result{};LARGE_INTEGER before{},after{};QueryPerformanceCounter(&before);SetLastError(ERROR_SUCCESS);
    const bool sent=SendMessageTimeoutW(source.hwnd,WM_CANCELMODE,0,0,
        SMTO_ABORTIFHUNG|SMTO_ERRORONEXIT|SMTO_BLOCK,100,&result)!=0;
    const auto error=sent?ERROR_SUCCESS:GetLastError();QueryPerformanceCounter(&after);pending.cancel_succeeded=sent;
    log.event("native_move_cancel","\"generation\":"+std::to_string(g)+
        ",\"native_attempted\":true,\"bounded_timeout_ms\":100,\"api_returned_success\":"+(sent?"true":"false")+
        ",\"before_qpc\":"+std::to_string(before.QuadPart)+",\"after_qpc\":"+std::to_string(after.QuadPart)+
        ",\"native_END_not_inferred\":true,\"win32_error\":"+std::to_string(error));
    if(!sent){input.revoke_generation(g);native_move.reset();}
}
void Application::begin(const InputEvent& e,ContextualBeginTiming* stages){
    if(stages)stages->begin_qpc=diagnostic_qpc();
    if(idle_notice_timer)hide_preview("new_gesture");
    // A real UP may have reached the hook before the GUI sees this DOWN. Its
    // distinct terminal capability permits building this exact context only;
    // it does not restore active input or authorize any queued MOVE.
    if(!input.context_allowed(e.generation))return;
    if(gesture)finish(v::End::QueueFailure);if(!e.prepared||!epoch.begin(e.generation))return;
    const auto& p=*e.prepared;const auto fresh=catalog.get(p.source.hwnd,settings,nullptr,false,false);
    if(stages){stages->fresh_qpc=diagnostic_qpc();stages->fresh_returned=true;stages->fresh_present=fresh.has_value();}
    const bool native_handoff=e.handoff_snapshot&&e.native_move_receipt;
    const auto& baseline=native_handoff?*e.handoff_snapshot:p.source;
    if(!fresh||!Catalog::same(p.source,*fresh)||
       (native_handoff?!WindowWriter::prepare(baseline,*fresh,baseline.visible).accepted:fresh->visible!=p.source.visible)||
       (native_handoff&&(!native_geometry_handoff_compatible(p,*fresh)||
           !native_arranged_handoff_compatible(p,*fresh,e.button,e.clicks,e.modifiers)))||p.config_generation!=settings.generation){
        if(diagnostic)log.event("gesture_begin_rejected","\"generation\":"+std::to_string(e.generation)+
            ",\"source\":"+std::to_string(reinterpret_cast<std::uintptr_t>(p.source.hwnd))+
            ",\"fresh_snapshot\":"+(fresh?"true":"false")+",\"identity_same\":"+(fresh&&Catalog::same(p.source,*fresh)?"true":"false")+
            ",\"geometry_same\":"+(fresh&&fresh->visible==p.source.visible?"true":"false")+
            ",\"configuration_same\":"+(p.config_generation==settings.generation?"true":"false"));
        input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);return;}
    if(native_handoff&&(!native_group_unchanged(e,*fresh)||!native_move||native_move->down.generation!=e.generation||!native_move->end||
       native_move->end!=e.native_move_receipt||!native_move->gate.may_write(e.generation,input.allowed(e.generation),true,
            gesture_gui_allowed(*fresh,e.generation)))){
        input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);native_move.reset();return;
    }
    Gesture active;active.down=e;active.config=settings;active.original=native_handoff?p.source.visible:fresh->visible;
    active.previous=fresh->visible;active.edges=edges(p.region);
    active.operation_source=*fresh;active.was_maximized=fresh->maximized;
    active.restore_kind=native_restore_entry_kind(p,*fresh,native_handoff,e.button,e.clicks,e.modifiers);
    active.restore_armed=active.restore_kind!=NativeRestoreKind::None;
    active.ctrl=(e.modifiers&2u)&&settings.glue&&(fresh->feature_mask&2u);active.last_tick=e.tick;
    active.members=native_handoff&&active.ctrl?p.native_ctrl_members:catalog.all(settings);
    if(stages&&!(native_handoff&&active.ctrl))stages->first_all_qpc=diagnostic_qpc();
    if(active.ctrl)active.members.erase(std::remove_if(active.members.begin(),active.members.end(),[](const auto& member){return !(member.feature_mask&2u);}),active.members.end());
    auto list=items(active.members);std::vector<v::Key> selected;
    if(native_handoff&&active.ctrl){for(const auto& member:active.members)selected.push_back(member.key);}
    else selected=active.ctrl&&!fresh->maximized?v::connected(list,fresh->key,dip(settings.adjacency_dip,fresh->dpi)):std::vector<v::Key>{fresh->key};
    active.members.erase(std::remove_if(active.members.begin(),active.members.end(),[&](const auto& s){
        return std::find(selected.begin(),selected.end(),s.key)==selected.end()||(active.ctrl&&!(s.feature_mask&2u));}),active.members.end());
    if(active.members.empty()){input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);return;}
    // Desktop enumeration is a read-only topology snapshot. Only the selected
    // operation participants query native tracking limits; no stale/default
    // bounds may become the frozen Ctrl resize contract.
    for(auto& member:active.members){
        const auto actual=catalog.get(member.hwnd,settings);
        const auto& expected=native_handoff&&member.key==p.source.key?baseline:member;
        if(!actual||!Catalog::same(member,*actual)||actual->visible!=expected.visible||
            actual->positioning!=expected.positioning||actual->dpi!=member.dpi||
            actual->monitor!=member.monitor||actual->work!=member.work||
            actual->maximized!=member.maximized||actual->minimized!=member.minimized||
            actual->can_resize!=member.can_resize||actual->feature_mask!=member.feature_mask){
            input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);return;
        }
        if(member.key==fresh->key&&(actual->visible!=fresh->visible||actual->positioning!=fresh->positioning||
            actual->dpi!=fresh->dpi||actual->monitor!=fresh->monitor||actual->work!=fresh->work||
            actual->maximized!=fresh->maximized||actual->minimized!=fresh->minimized||
            (native_handoff&&!native_arranged_handoff_compatible(p,*actual,e.button,e.clicks,e.modifiers)))){
            input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);return;
        }
        member=*actual;
        if(member.key==fresh->key)active.operation_source=member;
    }
    if(std::none_of(active.members.begin(),active.members.end(),[&](const auto& member){return member.key==fresh->key;})){
        input.revoke_generation(e.generation);epoch.finish(e.generation,v::End::SourceLost);return;
    }
    if(stages)stages->members_fresh_qpc=diagnostic_qpc();
    const bool contextual_double_click=e.clicks==2&&p.region>0&&settings.stretch&&(fresh->feature_mask&8u);
    if(stages){stages->contextual_evaluated=true;stages->contextual_branch=contextual_double_click;}
    // 双击在本次 begin 内离散完成，不消费连续 motion 的 targets。
    // 保留首次 source 资格门与末次独立 stretch 候选采样。
    if(!contextual_double_click){
        active.targets=catalog.all(settings);active.targets.erase(std::remove_if(active.targets.begin(),active.targets.end(),[&](const auto& s){
            return std::find(selected.begin(),selected.end(),s.key)!=selected.end()||!(s.feature_mask&1u);}),active.targets.end());
    }
    for(const auto& s:active.members)active.expected[s.key]=s;
    if(native_handoff)for(auto& member:active.members)if(member.key==p.source.key){
        const auto actual_limits=member.limits;const bool observed_limits=member.declared_track_limits_observed;
        member=p.source;member.limits=actual_limits;member.declared_track_limits_observed=observed_limits;
    }
    active.scope_original=active.original;
    if(active.ctrl&&!is_resize(active.edges)){
        for(const auto& s:active.members)active.scope_original={std::min(active.scope_original.left(),s.visible.left()),
            std::min(active.scope_original.top(),s.visible.top()),std::max(active.scope_original.right(),s.visible.right()),
            std::max(active.scope_original.bottom(),s.visible.bottom())};
    }
    active.previous=active.scope_original;gesture=std::move(active);
    if(!gesture->restore_armed&&!fresh->maximized&&!gesture->ctrl&&!is_resize(gesture->edges)){
        std::optional<v::Rect> normal=native_handoff?p.layout_restore_normal:std::nullopt;
        if(!native_handoff)if(const auto history=owned_layout(*fresh);history&&history->back().drag_restore)normal=history->back().normal;
        if(normal)if(const auto anchor=v::restored_move_anchor(native_handoff?p.source.visible:fresh->visible,*normal,{e.point.x,e.point.y})){
            gesture->layout_drag_restore=true;gesture->original=*anchor;gesture->scope_original=*anchor;gesture->previous=*anchor;
        }
    }
    if(input.allowed(e.generation)&&!gesture->restore_armed&&!fresh->maximized&&settings.drag_transparency&&(fresh->feature_mask&32u))gesture->drag_visual=visual_begin(*fresh,false,static_cast<BYTE>(settings.drag_alpha),true);
    log.event("gesture_start","\"generation\":"+std::to_string(e.generation)+",\"key\":"+std::to_string(fresh->key)+
        ",\"resize\":"+std::string(is_resize(gesture->edges)?"true":"false")+",\"ctrl\":"+(gesture->ctrl?"true":"false")+
        ",\"original\":"+rectangle(gesture->original)+",\"native_first_handoff\":"+(native_handoff?"true":"false")+
        ",\"original_DOWN_group_frozen\":"+(native_handoff&&gesture->ctrl?"true":"false")+
        ",\"group_members\":"+std::to_string(gesture->members.size())+
        ",\"native_restore_kind\":\""+native_restore_kind_name(gesture->restore_kind)+"\""+
        ",\"original_DOWN_arranged\":"+(p.source.native_arranged?(*p.source.native_arranged?"true":"false"):"null")+
        ",\"actual_begin_arranged\":"+(fresh->native_arranged?(*fresh->native_arranged?"true":"false"):"null")+
        ",\"actual_begin_geometry\":"+rectangle(fresh->visible));
    if(native_handoff)native_move.reset();
    if(gesture->ctrl&&input.allowed(e.generation))show_feedback(gesture->members,
        L"Ctrl 临时邻接："+std::to_wstring(gesture->members.size())+(is_resize(gesture->edges)?L" 个窗口；粗线为实际共享边，尚未缩放":L" 个窗口；细框仅表示成员，尚未移动"),{},
        is_resize(gesture->edges)?shared_feedback_edges(items(gesture->members),fresh->key,gesture->edges,
            dip(settings.adjacency_dip,fresh->dpi)):std::vector<FeedbackLine>{});
    if(contextual_double_click){
        auto other=catalog.all(settings);
        if(stages)stages->other_qpc=diagnostic_qpc();
        other.erase(std::remove_if(other.begin(),other.end(),[&](const auto& item){return item.key==fresh->key;}),other.end());
        const auto desired=v::stretch(fresh->visible,fresh->work,gesture->edges,(e.modifiers&4u)!=0,items(other),settings.smart_stretch);
        if(input.allowed(e.generation)){
            if(stages){stages->apply_before_qpc=diagnostic_qpc();stages->apply_invoked=true;}
            const bool applied=apply({true,"double_click_stretch",{{fresh->key,desired}}},{*fresh},e.generation,false,e.hook_observed_qpc);
            if(stages){stages->apply_returned_qpc=diagnostic_qpc();stages->apply_returned=true;stages->applied=applied;}
            if(applied)remember_receipt(*fresh,true);
        }
        input.revoke_generation(e.generation);finish(v::End::Paused);
    }
}
void Application::cancel_seam_timer(bool reset){
    if(!gesture)return;
    auto& state=*gesture;
    if(state.seam_timer){KillTimer(window,state.seam_timer);state.seam_timer=0;}
    if(reset){state.seam_since=0;state.preview_monitor=nullptr;state.seam_zone=-1;state.seam_timer_failed=false;}
}
bool Application::dock_preview(POINT point,std::uint64_t tick,bool arm,bool normal_up_completion){
    if(!gesture)return false;
    auto& state=*gesture;const auto& config=state.config;const auto& source=state.operation_source;
    state.dock_action.reset();state.dock_rect.reset();
    const auto permitted=[&]{return normal_up_completion?input.completion_allowed(state.down.generation):input.allowed(state.down.generation);};
    if(!permitted()){cancel_seam_timer();return false;}
    if(!config.docking||!(source.feature_mask&4u)||is_resize(state.edges)||state.ctrl){cancel_seam_timer();return false;}
    const auto monitor=MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST);const auto displays=catalog.displays();
    const auto display=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==monitor;});
    if(display==displays.end()){cancel_seam_timer();return false;}
    const auto& work=display->work;const auto gap=dip(12,source.dpi);
    const bool left=point.x<=work.left()+gap,right=point.x>=work.right()-gap,
        top=point.y<=work.top()+gap,bottom=point.y>=work.bottom()-gap;
    const int zone=left?(top?4:bottom?6:0):right?(top?5:bottom?7:1):top?2:bottom?3:-1;
    if(zone<0){cancel_seam_timer();return false;}
    const auto action=config.dock_actions[static_cast<std::size_t>(zone)];
    if(action==v::Action::None){cancel_seam_timer();return false;}
    // A collinear boundary far away is not a monitor seam at this cursor.
    const bool seam=std::any_of(displays.begin(),displays.end(),[&](const auto& d){
        if(d.monitor==monitor)return false;
        const bool vertical=point.y>=d.bounds.top()&&point.y<d.bounds.bottom();
        const bool horizontal=point.x>=d.bounds.left()&&point.x<d.bounds.right();
        return (vertical&&((left&&d.bounds.right()==display->bounds.left())||(right&&d.bounds.left()==display->bounds.right())))||
            (horizontal&&((top&&d.bounds.bottom()==display->bounds.top())||(bottom&&d.bounds.top()==display->bounds.bottom())));
    });
    if(seam){
        if(state.preview_monitor!=monitor||state.seam_zone!=zone||!state.seam_since){
            cancel_seam_timer();state.seam_since=tick;state.preview_monitor=monitor;state.seam_zone=zone;
        }
        state.seam_point=point;
        const auto delay=static_cast<std::uint64_t>(config.seam_dwell_ms);
        if(tick<state.seam_since||tick-state.seam_since<delay){
            if(arm&&!state.seam_timer&&!state.seam_timer_failed){
                const auto elapsed=tick>=state.seam_since?tick-state.seam_since:0;
                const auto remaining=delay-elapsed;
                // Every wait gets a new identity. A queued WM_TIMER from an
                // earlier zone or generation cannot consume the current wait.
                if(seam_timer_serial==std::numeric_limits<UINT_PTR>::max()){
                    state.seam_timer_failed=true;notify(L"接缝预览计时标识已用尽；本次保持自由移动。");return false;
                }
                const auto requested=++seam_timer_serial;
                // With a non-null HWND the API returns only a nonzero success
                // value; WM_TIMER and KillTimer use OUR requested identifier.
                if(SetTimer(window,requested,static_cast<UINT>(remaining),nullptr))state.seam_timer=requested;
                else{state.seam_timer_failed=true;notify(L"无法建立本次接缝停留预览；保持自由移动。");}
            }return false;
        }
        cancel_seam_timer(false);
    }else cancel_seam_timer();
    if(!permitted())return false;
    state.dock_action=action;state.dock_rect=v::smart_dock(work,action,items(state.targets),config.smart_docking);
    if(state.dock_rect->empty()){state.dock_action.reset();state.dock_rect.reset();return false;}
    show_preview(state.dock_rect);return true;
}
void Application::seam_timeout(UINT_PTR timer){
    if(!gesture||gesture->seam_timer!=timer)return;
    KillTimer(window,timer);gesture->seam_timer=0;
    const auto generation=gesture->down.generation;
    if(stopping||!epoch.motion(generation)||!input.allowed(generation)){
        cancel_seam_timer();gesture->dock_action.reset();gesture->dock_rect.reset();show_preview({});return;
    }
    const auto point=input.latest();
    if(point.x!=gesture->seam_point.x||point.y!=gesture->seam_point.y){
        // A real newer sample is queued for the dispatcher. Do not invent its
        // motion/intent here or show the old edge; that sample owns rearming.
        cancel_seam_timer();gesture->dock_action.reset();gesture->dock_rect.reset();show_preview({});return;
    }
    const auto source=gesture->operation_source;
    const auto current=catalog.get(source.hwnd,settings);
    if(!current||!Catalog::same(source,*current)||GetForegroundWindow()!=source.hwnd){finish(v::End::FocusLost);return;}
    const auto gui_context=gesture_gui_context(source,generation);
    if(gui_context!=GestureGuiDisposition::Allowed){
        if(gui_context==GestureGuiDisposition::Rejected)finish(v::End::FocusLost);return;
    }
    for(const auto& [key,expected]:gesture->expected){
        (void)key;const auto actual=catalog.get(expected.hwnd,settings);
        if(!actual||!Catalog::same(expected,*actual)||actual->visible!=expected.visible||actual->positioning!=expected.positioning||
            actual->dpi!=expected.dpi||actual->monitor!=expected.monitor||actual->work!=expected.work){finish(v::End::SourceLost);return;}
    }
    refresh_targets();
    // Timeout is a preview-only event, never a synthetic Move quantum. Physical
    // UP remains the only path to normal completion and a discrete dock write.
    if(!input.allowed(generation)||!dock_preview(point,GetTickCount64(),false))show_preview({});
}
void Application::cancel_restore_wait(){
    if(gesture&&gesture->restore_timer){KillTimer(window,gesture->restore_timer);gesture->restore_timer=0;}
}
void Application::request_native_restore(const InputEvent& move){
    if(!gesture||!gesture->restore_armed||gesture->restore_requested||!input.allowed(move.generation))return;
    const auto original=gesture->down.prepared->source;
    const auto dx=static_cast<std::int64_t>(move.point.x)-gesture->down.point.x;
    const auto dy=static_cast<std::int64_t>(move.point.y)-gesture->down.point.y;
    const auto threshold_x=std::max(1,GetSystemMetricsForDpi(SM_CXDRAG,original.dpi)/2);
    const auto threshold_y=std::max(1,GetSystemMetricsForDpi(SM_CYDRAG,original.dpi)/2);
    if(dx>-threshold_x&&dx<threshold_x&&dy>-threshold_y&&dy<threshold_y)return;
    const auto fresh=catalog.get(original.hwnd,settings);
    WINDOWPLACEMENT placement{sizeof(placement)};
    if(!fresh||!Catalog::same(original,*fresh)||GetForegroundWindow()!=original.hwnd){
        finish(v::End::SourceLost);return;
    }
    const auto gui_context=gesture_gui_context(original,move.generation);
    if(gui_context!=GestureGuiDisposition::Allowed){
        if(gui_context==GestureGuiDisposition::Rejected)finish(v::End::SourceLost);return;
    }
    if(!GetWindowPlacement(original.hwnd,&placement)||
        !native_restore_request_ready(gesture->restore_kind,original,gesture->operation_source,*fresh,
            placement.showCmd,input.allowed(move.generation))||
        placement.rcNormalPosition.right<=placement.rcNormalPosition.left||placement.rcNormalPosition.bottom<=placement.rcNormalPosition.top){
        // Matching real UP may have revoked admission during the read-only
        // capture. Preserve its queued terminal receipt, never relabel that
        // normal withdrawal as a failed native restore or rearm permission.
        if(!input.allowed(move.generation))return;
        finish(v::End::SourceLost);return;
    }
    if(!input.allowed(move.generation))return;
    if(seam_timer_serial==std::numeric_limits<UINT_PTR>::max()){finish(v::End::Deadline);return;}
    const auto timer=++seam_timer_serial;
    if(!SetTimer(window,timer,1000,nullptr)){finish(v::End::Deadline);return;}
    gesture->restore_timer=timer;gesture->restore_placement=placement;
    if(!input.allowed(move.generation)){cancel_restore_wait();return;}
    // The saved rcNormalPosition is WORKSPACE data for ordinary windows. It
    // is evidence only: no conversion guess or SetWindowPos uses those values.
    gesture->restore_requested=true;gesture->restore_pending=true;gesture->restore_armed=false;
    const bool started=ShowWindowAsync(original.hwnd,SW_RESTORE)!=FALSE;
    log.event("native_restore_request","\"generation\":"+std::to_string(move.generation)+
        ",\"restore_kind\":\""+native_restore_kind_name(gesture->restore_kind)+"\""+
        ",\"original_DOWN_arranged\":"+(original.native_arranged?(*original.native_arranged?"true":"false"):"null")+
        ",\"before_arranged\":"+(fresh->native_arranged?(*fresh->native_arranged?"true":"false"):"null")+
        ",\"native_attempted\":true,\"request_started\":"+(started?"true":"false")+
        ",\"actual_normal_observed\":false,\"placement_show_cmd\":"+std::to_string(placement.showCmd)+
        ",\"normal_position_raw\":["+std::to_string(placement.rcNormalPosition.left)+","+
        std::to_string(placement.rcNormalPosition.top)+","+std::to_string(placement.rcNormalPosition.right)+","+
        std::to_string(placement.rcNormalPosition.bottom)+"]");
    if(!started)finish(v::End::SourceLost);
}
void Application::restore_progress(){
    if(!gesture||!gesture->restore_pending||!input.allowed(gesture->down.generation))return;
    const auto original=gesture->down.prepared->source;
    const auto fresh=catalog.get(original.hwnd,settings);
    if(!fresh||!Catalog::same(original,*fresh)||GetForegroundWindow()!=original.hwnd){finish(v::End::SourceLost);return;}
    const auto readiness=native_restore_progress(gesture->restore_kind,original,*fresh,
        static_cast<std::int64_t>(gesture->restore_placement.rcNormalPosition.right)-gesture->restore_placement.rcNormalPosition.left,
        static_cast<std::int64_t>(gesture->restore_placement.rcNormalPosition.bottom)-gesture->restore_placement.rcNormalPosition.top,
        input.allowed(gesture->down.generation));
    if(readiness==NativeRestoreProgress::Rejected){
        if(!input.allowed(gesture->down.generation))return;
        finish(v::End::SourceLost);return;
    }
    if(readiness==NativeRestoreProgress::Pending)return;
    const auto gui_context=gesture_gui_context(*fresh,gesture->down.generation);
    if(gui_context!=GestureGuiDisposition::Allowed){
        if(gui_context==GestureGuiDisposition::Rejected)finish(v::End::FocusLost);return;
    }
    auto normal=fresh->visible;
    if(gesture->restore_kind!=NativeRestoreKind::Arranged)
        if(const auto history=owned_layout(*fresh);history&&history->back().drag_restore)normal=history->back().normal;
    const auto anchor=v::restored_move_anchor(original.visible,normal,{gesture->down.point.x,gesture->down.point.y});
    if(!anchor){finish(v::End::SourceLost);return;}
    const auto latest=gesture->restore_move;const auto generation=gesture->down.generation;
    if(!input.allowed(generation))return;
    cancel_restore_wait();gesture->restore_pending=false;gesture->layout_drag_restore=true;
    gesture->operation_source=*fresh;gesture->members={*fresh};gesture->expected.clear();gesture->expected[fresh->key]=*fresh;
    gesture->original=gesture->scope_original=gesture->previous=*anchor;gesture->last_tick=gesture->down.tick;
    gesture->targets=catalog.all(settings);
    gesture->targets.erase(std::remove_if(gesture->targets.begin(),gesture->targets.end(),[&](const auto& item){return item.key==fresh->key||!(item.feature_mask&1u);}),gesture->targets.end());
    log.event("native_restore_observed","\"generation\":"+std::to_string(generation)+
        ",\"restore_kind\":\""+native_restore_kind_name(gesture->restore_kind)+"\""+
        ",\"actual_arranged\":"+(fresh->native_arranged?(*fresh->native_arranged?"true":"false"):"null")+
        ",\"wake\":\"catalog_event\",\"actual_normal_observed\":true,\"actual\":"+rectangle(fresh->visible)+
        ",\"positioning\":"+rectangle(fresh->positioning)+",\"intent_anchor\":"+rectangle(*anchor)+
        ",\"original_down_preserved\":true");
    if(input.allowed(generation)&&gesture->config.drag_transparency&&(fresh->feature_mask&32u))
        gesture->drag_visual=visual_begin(*fresh,false,static_cast<BYTE>(gesture->config.drag_alpha),true);
    const auto point=input.latest();
    if(latest&&point.x==latest->point.x&&point.y==latest->point.y&&input.allowed(generation))motion(*latest);
}
void Application::restore_timeout(UINT_PTR timer){
    if(!gesture||gesture->restore_timer!=timer)return;
    KillTimer(window,timer);gesture->restore_timer=0;
    // A hook-side terminal event already revoked this generation. Its actual
    // queued UP/Abort owns the termination reason; timeout is not UP evidence.
    if(!input.allowed(gesture->down.generation))return;
    if(gesture->restore_pending){
        log.event("native_restore_timeout","\"generation\":"+std::to_string(gesture->down.generation)+
            ",\"actual_normal_observed\":false,\"issued_request_not_claimed_cancelled\":true");
        finish(v::End::Deadline);
    }
}
void Application::motion(const InputEvent& e,bool normal_up_completion){
    // Already-issued native calls keep their actual completion receipt. This
    // dispatcher may not turn a terminal input sample into a new native call.
    if(normal_up_completion)return;
    if(!gesture||!epoch.motion(e.generation)||!input.motion_allowed(e,normal_up_completion,gesture->normal_up_motion_consumed))return;
    // Claim the real terminal observation once in the serialized dispatcher.
    // The native guard independently rechecks the shared capability, so a new
    // DOWN, stop, failure or foreground change can still reject before issue.
    if(normal_up_completion)gesture->normal_up_motion_consumed=true;
    if(gesture->restore_armed||gesture->restore_pending){
        if(normal_up_completion){
            log.event("normal_up_motion_rejected","\"generation\":"+std::to_string(e.generation)+
                ",\"reason\":\"native_restore_requires_active_gesture\",\"new_native_attempt\":false");return;
        }
        gesture->restore_move=e;if(gesture->restore_armed)request_native_restore(e);return;
    }
    auto& state=*gesture;const auto& config=state.config;const auto source=state.operation_source;const auto fresh=catalog.get(source.hwnd,settings);
    if(!fresh||!Catalog::same(source,*fresh)||GetForegroundWindow()!=source.hwnd){finish(v::End::SourceLost);return;}
    const auto gui_context=gesture_gui_context(source,e.generation);
    if(gui_context!=GestureGuiDisposition::Allowed){
        if(gui_context==GestureGuiDisposition::Rejected)finish(v::End::FocusLost);return;
    }
    // Ctrl 在 START 已锁定；这些 targets 仅供磁吸，不是 writer 参与者。
    // Ctrl Resize 不使用磁吸或停靠，仍保留 source/member 的真实权限检查。
    const bool ctrl_resize=state.ctrl&&is_resize(state.edges);
    if(!ctrl_resize)refresh_targets();
    if(motion_point_already_exact(e,normal_up_completion,state.exact_motion)){
        // A duplicate physical cursor is not a new motion quantum. Re-solving
        // with an equal millisecond sample can discard a retained snap; even a
        // later timestamp must not snap a previously speed-suppressed free move.
        // Validate the actual completed geometry, never accept a new baseline.
        for(const auto& member:state.members){
            const auto expected=state.expected.find(member.key);
            const auto actual=member.key==source.key?fresh:catalog.get(member.hwnd,settings);
            if(expected==state.expected.end()||!actual||actual->feature_mask!=expected->second.feature_mask||
               !WindowWriter::prepare(expected->second,*actual,expected->second.visible).accepted){
                state.dock_action.reset();state.dock_rect.reset();finish(v::End::SourceLost);return;
            }
        }
        // Still refresh an actual dock/seam preview: finish owns its distinct
        // normal-UP commit. Exact free/magnet progress is not a completed dock.
        const bool dock_shown=dock_preview(e.point,e.tick,!normal_up_completion,normal_up_completion);
        if(!dock_shown)show_preview({});
        log.event(normal_up_completion?"normal_up_motion_already_exact":"active_motion_already_exact","\"generation\":"+std::to_string(e.generation)+
            ",\"key\":"+std::to_string(source.key)+",\"actual\":"+rectangle(fresh->visible)+
            ",\"geometry_revalidated\":true,\"new_native_attempt\":false,\"dock_preview_refreshed\":"+(dock_shown?"true":"false"));
        return;
    }
    const v::Point delta{static_cast<std::int64_t>(e.point.x)-state.down.point.x,static_cast<std::int64_t>(e.point.y)-state.down.point.y};
    auto list=items(state.members);v::Plan plan;
    if(state.layout_drag_restore)for(auto& member:list)if(member.id==source.key)member.visible=state.original;
    if(is_resize(state.edges))plan=state.ctrl?v::resize_group(list,source.key,state.edges,delta,dip(config.adjacency_dip,source.dpi)):
        v::resize_single({source.key,state.original,source.limits},state.edges,delta);
    else plan=v::move_group(list,source.key,delta,state.ctrl?dip(config.adjacency_dip,source.dpi):0);
    if(!plan.accepted){finish(v::End::SourceLost);return;}if(plan.placements.empty())return;
    auto src=std::find_if(plan.placements.begin(),plan.placements.end(),[&](const auto& p){return p.id==source.key;});if(src==plan.placements.end())return;
    const auto raw=state.ctrl&&!is_resize(state.edges)?core::movement::translate_rect(state.scope_original,{delta.x,delta.y}):src->visible;
    std::vector<m::MagnetTarget> targets;
    if(!ctrl_resize)for(const auto& s:state.targets)targets.push_back({core::model::WindowId(std::to_string(s.key)),s.visible,false});
    const HMONITOR monitor=ctrl_resize?nullptr:MonitorFromPoint(e.point,MONITOR_DEFAULTTONEAREST);
    const auto displays=ctrl_resize?std::vector<Display>{}:catalog.displays();
    const auto display=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==monitor;});
    if(display!=displays.end()&&targets.size()<256)targets.push_back({core::model::WindowId("work"),display->work,true});
    m::MagnetInput request;request.initial=state.scope_original;request.raw=raw;request.targets=targets;
    request.options.attraction_distance=dip(config.attraction_dip,source.dpi);request.options.max_targets=256;
    request.preferred_x=state.x;request.preferred_y=state.y;
    request.motion=m::MotionSample{state.previous,state.last_tick,e.tick,1000,dip(config.speed_dip,source.dpi)};
    request.interaction=is_resize(state.edges)?m::Interaction::Resize:m::Interaction::Move;
    request.edges={state.edges.left,state.edges.top,state.edges.right,state.edges.bottom};request.latched_resize=is_resize(state.edges);
    std::optional<m::SatisfiedConstraint> adopted_x,adopted_y;
    const bool magnet_on=(is_resize(state.edges)?config.magnet_resize:config.magnet_move)&&(source.feature_mask&1u);
    if(magnet_on&&!(state.ctrl&&is_resize(state.edges))){
        const auto result=m::MagnetConstraintSolver::solve(request);
        if(result.proposal){const auto& proposal=*result.proposal;
            bool adopted=false;
            if(state.ctrl){for(auto& p:plan.placements)p.visible=core::movement::translate_rect(p.visible,{proposal.move_delta.x,proposal.move_delta.y});adopted=true;}
            else if(proposal.corrected.width()>=source.limits.min_width&&proposal.corrected.height()>=source.limits.min_height&&
                proposal.corrected.width()<=source.limits.max_width&&proposal.corrected.height()<=source.limits.max_height){
                src->visible=proposal.corrected;adopted=true;}
            if(adopted){adopted_x=proposal.selected_x;adopted_y=proposal.selected_y;}
            const auto latch=[&](const auto& constraint)->std::optional<m::MagnetAxisPreference>{
                if(!constraint)return {};return m::MagnetAxisPreference{constraint->target,constraint->kind,constraint->moving_edge,constraint->target_edge,dip(config.release_dip,source.dpi)};};
            state.x=latch(proposal.selected_x);state.y=latch(proposal.selected_y);
        }else{state.x.reset();state.y.reset();}
    }
    state.previous=raw;state.last_tick=e.tick;const bool dock_shown=dock_preview(e.point,e.tick,!normal_up_completion,normal_up_completion);
    if(!dock_shown)show_preview({});
    // One terminal quantum chooses docking OR its absolute move/resize plan;
    // never issue a free placement followed by a corrected dock placement.
    if(normal_up_completion&&dock_shown)return;
    // Only a live Resize asks the application about this quantum's native
    // sizing proposal. Move, UP docking and discrete commands retain their
    // old paths; the original DOWN/intent anchor is never rebased to its reply.
    const auto resize_request=is_resize(state.edges)?
        std::optional<WindowWriter::ResizeRequest>{WindowWriter::ResizeRequest{state.edges,state.ctrl||plan.placements.size()>1}}:std::nullopt;
    if(!apply(plan,state.members,e.generation,normal_up_completion,e.hook_observed_qpc,resize_request)){
        // Only the matching actual UP event may finish NormalUp. A concurrent
        // hook-side revocation is not itself evidence of which terminator won.
        if(!last_apply_revoked_only){state.dock_action.reset();state.dock_rect.reset();finish(v::End::SourceLost);}return;}
    if(resize_request){
        const auto actual=last_exact_receipts.find(source.key);
        if(actual!=last_exact_receipts.end()&&actual->second.visible!=src->visible){
            const auto& accepted=actual->second.visible;
            // Application-owned constraints outrank a solver selection. Do not
            // retain a magnet latch or draw a claimed edge after C changed R.
            if(accepted.left()!=src->visible.left()||accepted.right()!=src->visible.right()){
                state.x.reset();adopted_x.reset();
            }
            if(accepted.top()!=src->visible.top()||accepted.bottom()!=src->visible.bottom()){
                state.y.reset();adopted_y.reset();
            }
        }
    }
    state.exact_motion=ExactMotionPoint{e.generation,e.point};
    if((config.feedback||!state.drag_visual_limit.empty())&&!dock_shown){
        std::vector<Snapshot> actual;
        if(state.ctrl){for(const auto& member:state.members){
            const auto found=state.expected.find(member.key);if(found==state.expected.end()){actual.clear();break;}
            actual.push_back(found->second);
        }}else if(state.x||state.y||!state.drag_visual_limit.empty()){
            if(const auto found=state.expected.find(source.key);found!=state.expected.end())actual.push_back(found->second);
        }
        std::vector<FeedbackLine> adopted_edges,shared_edges;
        if(!actual.empty()){
            auto scope=actual.front().visible;
            for(const auto& member:actual)scope={std::min(scope.left(),member.visible.left()),std::min(scope.top(),member.visible.top()),
                std::max(scope.right(),member.visible.right()),std::max(scope.bottom(),member.visible.bottom())};
            const auto selected=[&](const std::optional<m::SatisfiedConstraint>& constraint){
                if(!constraint)return;
                std::optional<v::Rect> actual_target;
                if(constraint->target.value()=="work"&&display!=displays.end()){
                    const auto current_displays=catalog.displays();const auto current=std::find_if(current_displays.begin(),current_displays.end(),
                        [&](const auto& d){return d.monitor==display->monitor;});
                    if(current!=current_displays.end()&&current->work==display->work)actual_target=current->work;
                }else for(const auto& target:state.targets)if(std::to_string(target.key)==constraint->target.value()){
                    const auto current=catalog.get(target.hwnd,settings,nullptr,false,false);
                    if(current&&Catalog::same(target,*current)&&(current->feature_mask&1u))actual_target=current->visible;
                    break;
                }
                // Solver selection alone is not success evidence. The actual
                // accepted writer result and a still-current target must agree.
                if(actual_target)if(const auto line=selected_feedback_edge(scope,*actual_target,
                    constraint->moving_edge,constraint->target_edge))adopted_edges.push_back(*line);
            };
            selected(adopted_x);selected(adopted_y);
            if(state.ctrl&&is_resize(state.edges))shared_edges=shared_feedback_edges(items(actual),source.key,state.edges,dip(config.adjacency_dip,source.dpi));
            show_feedback(actual,state.ctrl?
                (is_resize(state.edges)?L"Ctrl 联动缩放：":L"Ctrl 联动移动：")+std::to_wstring(actual.size())+
                    L" 个临时成员；细框为成员，粗线为实际吸附／共享边":L"",std::move(adopted_edges),std::move(shared_edges));
        }
    }
    if(input.allowed(e.generation)&&config.shake&&!is_resize(state.edges)&&(source.feature_mask&32u)&&state.shake.sample({e.point.x,e.point.y},e.tick,
        static_cast<int>(dip(config.shake_amplitude_dip,source.dpi)),static_cast<unsigned>(config.shake_reversals),static_cast<std::uint64_t>(config.shake_window_ms))){
        if(state.drag_visual){restore_drag_visual(std::move(*state.drag_visual));state.drag_visual.reset();}
        if(const auto stored=visual.find(source.key);stored!=visual.end()){
            (void)visual_restore(stored->second);
            // A failed or partial restoration still owns its unchanged
            // attributes. Keep that responsibility for a later explicit
            // restore/stop; app-owned changes are retired by visual_restore.
            if(!v::visual_changes(stored->second.ownership))visual.erase(stored);
        }
        else if(input.allowed(e.generation)){const auto record=visual_begin(*fresh,true,static_cast<BYTE>(config.shake_alpha),false);if(record)visual[source.key]=*record;}
    }
}
void Application::finish(v::End reason,std::uint64_t observed_qpc){
    if(native_move){input.revoke_generation(native_move->down.generation,reason);native_move.reset();}
    if(gesture)input.finish_generation(gesture->down.generation,reason);else input.revoke(reason);
    cancel_seam_timer();cancel_restore_wait();hide_preview();if(!gesture)return;auto state=std::move(*gesture);gesture.reset();epoch.finish(state.down.generation,reason);last_termination=reason;
    if(reason==v::End::NormalUp&&state.dock_action&&state.dock_rect){
        const auto current=catalog.get(state.down.prepared->source.hwnd,settings);
        if(current&&Catalog::same(state.down.prepared->source,*current)&&
            state.expected.contains(current->key)&&current->visible==state.expected.at(current->key).visible&&
            current->positioning==state.expected.at(current->key).positioning&&!stopping&&settings.enabled){
            if(*state.dock_action==v::Action::Maximize){
                const auto guard=input.operation_guard(state.down.generation,true,current->hwnd);
                const bool claimed=guard();const bool requested=claimed&&ShowWindowAsync(current->hwnd,SW_MAXIMIZE)!=FALSE;
                log.event("normal_up_maximize","\"generation\":"+std::to_string(state.down.generation)+
                    ",\"native_attempted\":"+(claimed?"true":"false")+",\"request_started\":"+(requested?"true":"false")+
                    ",\"actual_state_completion\":\"not_observed_by_request_return\"");
            }
            else if(apply({true,"normal_up_dock",{{current->key,*state.dock_rect}}},{*current},state.down.generation,true,observed_qpc))remember_receipt(*current,true);
        }
    }else if(reason==v::End::Escape){
        if(state.was_maximized){
            const auto source=state.down.prepared->source;const auto now=catalog.get(source.hwnd,settings);
            if(now&&Catalog::same(source,*now)&&GetForegroundWindow()==source.hwnd&&
                (!now->maximized||state.restore_requested)){
                // SW_RESTORE was asynchronous: it can still be queued while
                // the snapshot is maximized. Queue the compensating state
                // request, but do not claim its completion from this BOOL.
                const bool requested=ShowWindowAsync(source.hwnd,SW_MAXIMIZE)!=FALSE;
                log.event("native_restore_escape","\"generation\":"+std::to_string(state.down.generation)+
                    ",\"native_attempted\":true,\"request_started\":"+(requested?"true":"false")+
                    ",\"actual_maximized_verified\":false");
            }
        }else{
            std::vector<Snapshot> current;v::Plan restore{true,"escape_restore",{}};
            for(const auto& old:state.members){const auto now=catalog.get(old.hwnd,settings);if(!now||!Catalog::same(old,*now)){restore.accepted=false;break;}
                current.push_back(*now);restore.placements.push_back({old.key,old.visible});}
            if(restore.accepted)apply(restore,current);
        }
    }
    if(reason==v::End::NormalUp&&!state.dock_action){
        for(const auto& before:state.members){const auto after=state.expected.find(before.key);
            if(after!=state.expected.end()&&(after->second.visible!=before.visible||after->second.positioning!=before.positioning))undo.erase(before.key);
        }
    }
    if(state.drag_visual)restore_drag_visual(std::move(*state.drag_visual));
    input.finish_completion(state.down.generation);
    log.event("gesture_end","\"generation\":"+std::to_string(state.down.generation)+",\"end\":"+std::to_string(static_cast<int>(reason))+
        ",\"normal_up\":"+std::string(reason==v::End::NormalUp?"true":"false")+",\"pending_retired\":true");
}
void Application::consume(){InputEvent e;std::optional<InputEvent> latest_move;
    const auto flush=[&]{if(latest_move){motion(*latest_move);latest_move.reset();}};
    while(input.pop(e)){
    const auto GUI_dequeued_qpc=diagnostic&&(e.kind==InputKind::Down||e.kind==InputKind::Mapped||e.kind==InputKind::Up)?diagnostic_qpc():0;
    if(e.kind==InputKind::NativeMovePending||e.kind==InputKind::NativeMoveStart||
       e.kind==InputKind::NativeMoveEnd||e.kind==InputKind::NativeMoveReleased){
        flush();native_move_event(e);continue;
    }
    if(e.kind==InputKind::Resource&&e.resource_event){
        flush();const auto& fact=*e.resource_event;const auto& capture=fact.capture;
        const auto& association=capture.association;
        const auto boolean=[](bool value){return value?"true":"false";};
        const auto handle=[](HWND value){return std::to_string(reinterpret_cast<std::uintptr_t>(value));};
        // These are immutable receipts drained on the GUI thread. Never do
        // disk IO in the input/resource callback or infer a common native
        // ordering merely from the order of these JSONL lines.
        if(diagnostic||fact.kind!=o::GestureShieldEventKind::RawMouse||
            (fact.raw_button_flags&RI_MOUSE_LEFT_BUTTON_UP))
            log.event("gesture_input_resource","\"generation\":"+std::to_string(fact.generation)+
                ",\"fact\":\""+shield_event_name(fact.kind)+"\",\"receipt_not_write_authority\":true"+
                ",\"resource_observed_qpc\":"+std::to_string(e.resource_observed_qpc)+
                ",\"entry_mode\":\""+(fact.entry_mode==o::GestureShieldEntryMode::NativeMoveHandoff?"native_move_handoff":"consumed_down")+"\""+
                ",\"source\":"+handle(fact.gesture.source)+
                ",\"source_pid\":"+std::to_string(fact.gesture.process_id)+
                ",\"source_tid\":"+std::to_string(fact.gesture.thread_id)+
                ",\"overlay\":"+handle(fact.overlay)+",\"foreground\":"+handle(fact.observed_foreground)+
                ",\"native_message\":"+std::to_string(fact.route_message)+
                ",\"native_message_time\":"+std::to_string(fact.message_time)+
                ",\"raw_sequence\":"+std::to_string(fact.raw_sequence)+
                ",\"raw_button_flags\":"+std::to_string(fact.raw_button_flags)+
                ",\"actual_legacy_up_accepted\":"+boolean(fact.legacy_up_accepted)+
                ",\"legacy_up_dispatch_qpc\":"+std::to_string(fact.legacy_up_dispatch_qpc)+
                ",\"capture_attempted\":"+boolean(capture.attempted)+
                ",\"SetCapture_previous_HWND\":"+handle(capture.previous)+
                ",\"capture_actual_after\":"+handle(capture.actual_after)+
                ",\"capture_failure\":"+std::to_string(static_cast<unsigned>(capture.failure))+
                ",\"release_attempted\":"+boolean(capture.release_attempted)+
                ",\"release_completed\":"+boolean(capture.release_completed)+
                ",\"release_succeeded\":"+boolean(capture.release_succeeded)+
                ",\"capture_owner_tid\":"+std::to_string(capture.owner_thread_id)+
                ",\"capture_release_before_HWND\":"+handle(capture.release_before)+
                ",\"capture_owner_observation_known\":"+boolean(fact.kind==o::GestureShieldEventKind::CaptureReleased&&capture.owner_thread_id!=0)+
                ",\"capture_release_after_is_copied_no_call\":"+boolean(fact.kind==o::GestureShieldEventKind::CaptureReleased&&!capture.release_attempted)+
                ",\"capture_after_release\":"+handle(capture.release_after)+
                ",\"release_before_qpc\":"+std::to_string(capture.release_before_qpc)+
                ",\"release_after_qpc\":"+std::to_string(capture.release_after_qpc)+
                ",\"desktop_verified\":"+boolean(association.desktop_verified)+
                ",\"owner_desktop_query\":"+boolean(association.owner_desktop_query)+
                ",\"owner_desktop_input\":"+boolean(association.owner_desktop_input)+
                ",\"source_desktop_query\":"+boolean(association.source_desktop_query)+
                ",\"source_desktop_input\":"+boolean(association.source_desktop_input)+
                ",\"input_desktop_query\":"+boolean(association.input_desktop_query)+
                ",\"input_desktop_active\":"+boolean(association.input_desktop_active)+
                ",\"Run_membership_evidence_used\":"+boolean(association.run_desktop_membership_used)+
                ",\"source_thread_desktop_error\":"+std::to_string(association.run_desktop.thread_desktop_error)+
                ",\"source_desktop_name_query\":"+boolean(association.run_desktop.source_name_query)+
                ",\"source_desktop_name_matches\":"+boolean(association.run_desktop.source_name_matches)+
                ",\"source_desktop_name_error\":"+std::to_string(association.run_desktop.source_name_error)+
                ",\"source_desktop_IO_error\":"+std::to_string(association.run_desktop.source_io_error)+
                ",\"Run_enum_first_complete\":"+boolean(association.run_desktop.first_complete)+
                ",\"Run_enum_second_complete\":"+boolean(association.run_desktop.second_complete)+
                ",\"Run_enum_first_member\":"+boolean(association.run_desktop.first_member)+
                ",\"Run_enum_second_member\":"+boolean(association.run_desktop.second_member)+
                ",\"Run_source_identity_stable\":"+boolean(association.run_desktop.identity_stable)+
                ",\"attach_attempted\":"+boolean(association.attach_attempted)+
                ",\"attach_completed\":"+boolean(association.attach_completed)+
                ",\"attach_succeeded\":"+boolean(association.attach_succeeded)+
                ",\"attach_before_qpc\":"+std::to_string(association.attach_before_qpc)+
                ",\"attach_after_qpc\":"+std::to_string(association.attach_after_qpc)+
                ",\"detach_attempted\":"+boolean(association.detach_attempted)+
                ",\"detach_completed\":"+boolean(association.detach_completed)+
                ",\"detach_succeeded\":"+boolean(association.detach_succeeded)+
                ",\"detach_before_qpc\":"+std::to_string(association.detach_before_qpc)+
                ",\"detach_after_qpc\":"+std::to_string(association.detach_after_qpc)+
                ",\"setup_stage\":"+std::to_string(static_cast<unsigned>(fact.setup_stage))+
                ",\"native_failure\":"+std::to_string(static_cast<unsigned>(fact.native_failure))+
                ",\"removal_reason\":"+std::to_string(static_cast<unsigned>(fact.removal_reason))+
                ",\"overlay_destroyed\":"+boolean(fact.overlay_destroyed)+
                ",\"hotkey_unregistered\":"+boolean(fact.hotkey_unregistered)+
                ",\"win32_error\":"+std::to_string(fact.win32_error));
        native_move_resource(e);continue;
    }
    // Receipt of an already-normalized hook observation, not a reconstruction
    // from async key state. Keep a retired generation's real matching UP
    // visible without recreating its gesture or granting write permission.
    if(diagnostic&&(e.kind==InputKind::Down||e.kind==InputKind::Mapped||e.kind==InputKind::Up))
        log.event("input_button_observed","\"source\":\""+
            std::string(e.origin==InputOrigin::ShieldLegacy?"shield_queued_legacy":e.origin==InputOrigin::MouseHook?"WH_MOUSE_LL":"platform_abort")+
            "\",\"kind\":\""+
            std::string(e.kind==InputKind::Up?"UP":"DOWN")+"\",\"generation\":"+std::to_string(e.generation)+
            ",\"route\":\""+(e.action!=v::Action::None?"mapped":"gesture")+"\",\"action\":\""+
            std::string(v::action_name(e.action))+"\",\"action_enum\":"+std::to_string(static_cast<unsigned>(e.action))+
            ",\"button\":"+std::to_string(e.button)+",\"hook_observed_qpc\":"+std::to_string(e.hook_observed_qpc)+
            ",\"GUI_dequeued_qpc\":"+(GUI_dequeued_qpc?std::to_string(GUI_dequeued_qpc):"null")+
            ",\"native_message_time\":"+std::to_string(e.native_message_time)+
            ",\"resource_observed_qpc\":"+std::to_string(e.resource_observed_qpc)+
            ",\"source_key\":"+std::to_string(e.prepared?e.prepared->source.key:0)+
            ",\"event_end\":"+std::to_string(static_cast<unsigned>(e.end))+
            ",\"gesture_generation_active_at_GUI_receive\":"+(gesture&&gesture->down.generation==e.generation?"true":"false")+
            ",\"write_authority_granted_by_receipt\":false");
    if(e.kind==InputKind::Move){latest_move=e;continue;}flush();
    if(e.kind==InputKind::Down){
        std::optional<ContextualBeginTiming> stages;
        unsigned stage_index{};
        if(diagnostic&&e.clicks==2&&e.prepared&&e.prepared->region>0&&contextual_begin_records<4){
            stages.emplace();stage_index=++contextual_begin_records;
        }
        begin(e,stages?&*stages:nullptr);
        if(stages){
            const auto returned_qpc=diagnostic_qpc();
            const auto clock=[](std::uint64_t value){return value?std::to_string(value):"null";};
            // One bounded record after the real handler returns. Existing S is
            // later, inside place_with_destination, after batch admission.
            log.event("contextual_double_click_stages","\"record_index\":"+std::to_string(stage_index)+",\"record_limit\":4"+
                ",\"generation\":"+std::to_string(e.generation)+",\"source_key\":"+std::to_string(e.prepared->source.key)+
                ",\"source_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(e.prepared->source.hwnd))+
                ",\"source_PID\":"+std::to_string(e.prepared->source.pid)+",\"source_TID\":"+std::to_string(e.prepared->source.tid)+
                ",\"hook_observed_qpc\":"+std::to_string(e.hook_observed_qpc)+",\"qpc_frequency\":"+std::to_string(input.counter_frequency())+
                ",\"GUI_dequeued_qpc\":"+clock(GUI_dequeued_qpc)+",\"begin_entered_qpc\":"+clock(stages->begin_qpc)+
                ",\"first_source_fresh_returned_qpc\":"+clock(stages->fresh_qpc)+",\"first_source_fresh_present\":"+(stages->fresh_returned?(stages->fresh_present?"true":"false"):"null")+
                ",\"first_topology_all_returned_qpc\":"+clock(stages->first_all_qpc)+",\"selected_members_fresh_passed_qpc\":"+clock(stages->members_fresh_qpc)+
                ",\"actual_contextual_branch\":"+(stages->contextual_evaluated?(stages->contextual_branch?"true":"false"):"null")+",\"final_other_all_returned_qpc\":"+clock(stages->other_qpc)+
                ",\"apply_invoked\":"+(stages->apply_invoked?"true":"false")+",\"apply_before_qpc\":"+clock(stages->apply_before_qpc)+
                ",\"apply_returned\":"+(stages->apply_returned?"true":"false")+",\"apply_returned_qpc\":"+clock(stages->apply_returned_qpc)+
                ",\"apply_result\":"+(stages->apply_returned?(stages->applied?"true":"false"):"null")+
                ",\"handler_returned\":true,\"handler_returned_qpc\":"+clock(returned_qpc)+",\"stage_receipt_not_write_authority\":true");
        }
    }
    else if(e.kind==InputKind::Mapped){
        const auto action=e.prepared&&e.prepared->region==0&&e.clicks==2&&e.action==v::Action::Maximize&&
            IsZoomed(e.prepared->source.hwnd)?v::Action::Restore:e.action;
        // A discrete command may return after the hook has observed UP and a
        // later DOWN. Retire only this generation, including rejection/throw;
        // a check followed by unconditional revoke would race that new DOWN.
        try{if(e.prepared)command(action,e.prepared->source,e.hook_observed_qpc);}
        catch(...){input.revoke_generation(e.generation);throw;}
        const bool retired=input.revoke_generation(e.generation);
        if(diagnostic)log.event("mapped_command_completed","\"generation\":"+std::to_string(e.generation)+
            ",\"action\":\""+std::string(v::action_name(action))+"\",\"handler_invoked\":"+(e.prepared?"true":"false")+
            ",\"handler_returned\":true,\"active_generation_revoke_CAS_succeeded\":"+(retired?"true":"false")+
            ",\"native_success_not_inferred_from_handler_return\":true");
    }
    else if(e.kind==InputKind::Up){if(gesture&&gesture->down.generation==e.generation){
        // Queued continuous MOVE is retired by the input-side UP barrier.
        // finish() may commit an already-armed discrete dock preview, never a
        // late continuous motion plan. In-flight completion is observed by the
        // writer's existing exact receipt path, not recreated from this UP.
        if(gesture&&gesture->down.generation==e.generation)finish(e.end,e.hook_observed_qpc);
    }}
    else if(e.kind==InputKind::Abort){
        if(diagnostic&&e.abort_observation){
            const auto& facts=*e.abort_observation;
            const auto hwnd=[](HWND value){return std::to_string(reinterpret_cast<std::uintptr_t>(value));};
            log.event("input_abort_observed","\"generation\":"+std::to_string(e.generation)+
                ",\"end\":"+std::to_string(static_cast<unsigned>(e.end))+
                ",\"origin\":\""+input_abort_origin_name(facts.origin)+
                "\",\"trigger_event\":"+std::to_string(facts.trigger_event)+
                ",\"trigger_hwnd\":"+hwnd(facts.trigger_hwnd)+
                ",\"native_time\":"+std::to_string(facts.native_time)+
                ",\"observed_qpc\":"+std::to_string(facts.observed_qpc)+
                ",\"sample_phase\":\"after_generation_revoked\""+
                ",\"foreground\":"+hwnd(facts.foreground)+
                ",\"source\":"+hwnd(e.prepared?e.prepared->source.hwnd:nullptr)+
                ",\"source_PID_TID_current\":"+(facts.source_identity_current?"true":"false")+
                ",\"source_GUI_attempted\":"+(facts.source_gui_attempted?"true":"false")+
                ",\"source_GUI_known\":"+(facts.source_gui_known?"true":"false")+
                ",\"source_GUI_error\":"+std::to_string(facts.source_gui_error)+
                ",\"source_capture\":"+hwnd(facts.source_capture)+
                ",\"source_move_size\":"+hwnd(facts.source_move_size)+
                ",\"source_menu_owner\":"+hwnd(facts.source_menu_owner)+
                ",\"source_GUI_flags\":"+std::to_string(facts.source_gui_flags)+
                ",\"owner_TID\":"+std::to_string(facts.owner_thread_id)+
                ",\"owner_GUI_attempted\":"+(facts.owner_gui_attempted?"true":"false")+
                ",\"owner_GUI_known\":"+(facts.owner_gui_known?"true":"false")+
                ",\"owner_GUI_error\":"+std::to_string(facts.owner_gui_error)+
                ",\"owner_capture\":"+hwnd(facts.owner_capture)+
                ",\"observation_not_atomic_or_write_authority\":true");
        }
        if(native_move&&native_move->down.generation==e.generation)native_move.reset();
        if(gesture&&gesture->down.generation==e.generation)finish(e.end);
    }
    }flush();drain_button_admissions();
}
void Application::drain_button_admissions(bool final){
    if(!diagnostic)return;
    const auto boolean=[](bool value){return value?"true":"false";};
    const auto handle=[](HWND value){return std::to_string(reinterpret_cast<std::uintptr_t>(value));};
    const auto prepared=[&](const ButtonPreparedFacts& p){
        return std::string("{\"present\":")+boolean(p.present)+",\"root\":"+handle(p.root)+
            ",\"pid\":"+std::to_string(p.pid)+",\"tid\":"+std::to_string(p.tid)+",\"born\":"+std::to_string(p.born)+
            ",\"key\":"+std::to_string(p.key)+",\"config_generation\":"+std::to_string(p.config_generation)+
            ",\"point\":["+std::to_string(p.point.x)+","+std::to_string(p.point.y)+"],\"stamp\":"+std::to_string(p.stamp)+
            ",\"region\":"+std::to_string(p.region)+",\"feature_mask\":"+std::to_string(p.feature_mask)+
            ",\"maximized\":"+boolean(p.maximized)+",\"minimized\":"+boolean(p.minimized)+",\"can_resize\":"+boolean(p.can_resize)+"}";
    };
    const auto state=[&](const ButtonAdmissionState& p){
        return std::string("{\"generation\":")+std::to_string(p.generation)+",\"authority\":"+std::to_string(p.authority)+
            ",\"completion\":"+std::to_string(p.completion)+",\"consumed\":"+boolean(p.consumed)+
            ",\"native_move\":"+boolean(p.native_move)+",\"capture_pair\":"+boolean(p.capture_pair)+
            ",\"paused\":"+boolean(p.paused)+",\"healthy\":"+boolean(p.healthy)+",\"available\":"+boolean(p.available)+
            ",\"aborted\":"+boolean(p.aborted)+",\"stop_requested\":"+boolean(p.stop_requested)+"}";
    };
    ButtonAdmission row;
    for(unsigned n=0;n<ButtonAdmissionQueue::capacity&&input.pop_button_admission(row);++n){
        const char* route=!row.route_evaluated?"not_evaluated":row.route==DownRoute::Gesture?"gesture":row.route==DownRoute::Mapped?"mapped":"pass";
        log.event("button_admission","\"callback_sequence\":"+std::to_string(row.sequence)+
            ",\"kind\":\""+(row.down?"DOWN":"UP")+"\",\"native_message\":"+std::to_string(row.message)+
            ",\"native_time\":"+std::to_string(row.native_time)+",\"hook_observed_qpc\":"+std::to_string(row.hook_qpc)+
            ",\"callback_tick\":"+std::to_string(row.tick)+",\"point\":["+std::to_string(row.point.x)+","+std::to_string(row.point.y)+"]"+
            ",\"button\":"+std::to_string(row.button)+",\"packet_flags\":"+std::to_string(row.flags)+",\"mouse_data\":"+std::to_string(row.mouse_data)+
            ",\"entry_state\":"+state(row.before)+",\"exit_state\":"+state(row.after)+
            ",\"down_branch_entered\":"+boolean(row.down_branch_entered)+",\"cache_read_by_DOWN_branch\":"+boolean(row.cache_read)+
            ",\"DOWN_branch_prepared\":"+(row.cache_read?prepared(row.prepared):"null")+
            ",\"entry_active_pair_prepared\":"+prepared(row.active_prepared)+
            ",\"prepared_gate_passed\":"+(row.down_branch_entered?boolean(row.prepared_gate_passed):"null")+
            ",\"foreground_read_by_existing_branch\":"+boolean(row.foreground_read)+
            ",\"foreground\":"+(row.foreground_read?handle(row.foreground):"null")+
            ",\"route\":\""+route+"\",\"route_evaluated\":"+boolean(row.route_evaluated)+
            ",\"action\":"+(row.route_evaluated?"\""+std::string(v::action_name(row.action))+"\"":"null")+
            ",\"modifiers\":"+(row.down_branch_entered?std::to_string(row.modifiers):"null")+
            ",\"clicks\":"+(row.route_evaluated?std::to_string(row.clicks):"null")+
            ",\"dispatch_routed\":"+(row.route_evaluated?boolean(row.routed):"null")+",\"own_consume_decision\":"+boolean(row.consume)+
            ",\"click_candidate_sampled_at_DOWN_gate\":"+boolean(row.down_branch_entered)+
            ",\"candidate_eligible\":"+(row.down_branch_entered?boolean(row.candidate_eligible):"null")+
            ",\"candidate_pressing\":"+(row.down_branch_entered?boolean(row.candidate_pressing):"null")+
            ",\"candidate_anchor\":"+(row.down_branch_entered?"["+std::to_string(row.candidate_anchor.x)+","+std::to_string(row.candidate_anchor.y)+"]":"null")+
            ",\"previous_DOWN_native_time\":"+(row.down_branch_entered?std::to_string(row.previous_down_time):"null")+
            ",\"previous_source_key\":"+(row.down_branch_entered?std::to_string(row.previous_source_key):"null")+
            ",\"previous_region\":"+(row.down_branch_entered?std::to_string(row.previous_region):"null")+
            ",\"previous_button\":"+(row.down_branch_entered?std::to_string(row.previous_button):"null")+
            ",\"previous_modifiers\":"+(row.down_branch_entered?std::to_string(row.previous_modifiers):"null")+
            ",\"system_double_click_time_ms\":"+(row.down_branch_entered?std::to_string(row.double_click_time):"null")+
            ",\"matching_UP_branch\":"+(row.down?"null":boolean(row.matching_up))+
            ",\"normalized_UP_push_attempted\":"+(row.down?"null":boolean(row.up_push_attempted))+
            ",\"normalized_UP_push_succeeded\":"+(row.up_push_attempted?boolean(row.up_push_succeeded):"null")+
            ",\"native_release_push_attempted\":"+(row.down?"null":boolean(row.native_release_push_attempted))+
            ",\"native_release_push_succeeded\":"+(row.native_release_push_attempted?boolean(row.native_release_push_succeeded):"null")+
            ",\"observation_not_atomic_or_write_authority\":true,\"unobserved_rejection_reason\":\"UNKNOWN\",\"GUI_log_order_not_native_total_order\":true");
    }
    const auto dropped=input.button_admission_dropped(),wake_failed=input.button_admission_wake_failures();
    if(final||dropped!=admission_dropped_reported||wake_failed!=admission_wake_failures_reported){
        log.event("button_admission_queue","\"capacity\":64,\"dropped\":"+std::to_string(dropped)+
            ",\"wake_failures\":"+std::to_string(wake_failed)+",\"producer_joined\":"+boolean(final)+
            ",\"diagnostic_loss_does_not_revoke_input\":true,\"missing_facts_are_UNKNOWN\":true");
        admission_dropped_reported=dropped;admission_wake_failures_reported=wake_failed;
    }
}
void Application::discard_new_keys(const std::vector<BoundKey>& next){
    for(const auto& item:next)if(std::none_of(bound_keys.begin(),bound_keys.end(),[&](const auto& old){return old.id==item.id;}))
        UnregisterHotKey(window,item.id);
}
std::optional<std::vector<Application::BoundKey>> Application::prepare_keys(const v::Settings& value){
    if(!v::validate(value).empty())return {};
    std::vector<BoundKey> next;
    for(const auto& hotkey:value.hotkeys){
        const auto old=std::find_if(bound_keys.begin(),bound_keys.end(),[&](const auto& key){return key.binding.key==hotkey.key&&key.binding.modifiers==hotkey.modifiers;});
        if(old!=bound_keys.end()){next.push_back({old->id,hotkey});continue;}
        int id=1000;for(;id<1256;++id)if(std::none_of(bound_keys.begin(),bound_keys.end(),[&](const auto& key){return key.id==id;})&&
            std::none_of(next.begin(),next.end(),[&](const auto& key){return key.id==id;}))break;
        if(id==1256||!RegisterHotKey(window,id,hotkey.modifiers|MOD_NOREPEAT,hotkey.key)){
            discard_new_keys(next);notify(L"快捷键注册冲突："+action_label(hotkey.action)+L"。原有效映射保留，配置未提交。");return {};}
        next.push_back({id,hotkey});
    }return next;
}
void Application::commit_keys(std::vector<BoundKey> next){
    for(const auto& key:bound_keys)if(std::none_of(next.begin(),next.end(),[&](const auto& item){return item.id==key.id;}))UnregisterHotKey(window,key.id);
    bound_keys=std::move(next);
}
void Application::register_keys(){
    if(auto next=prepare_keys(settings))commit_keys(std::move(*next));
}
bool set_startup(bool enabled){
    HKEY key{};if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE|KEY_QUERY_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    const auto expected=L"\""+exe_path()+L"\" --startup";
    std::array<wchar_t,32768> existing{};DWORD type{},size=static_cast<DWORD>(existing.size()*sizeof(wchar_t));
    const auto found=RegQueryValueExW(key,L"PaneBind",nullptr,&type,reinterpret_cast<BYTE*>(existing.data()),&size);
    if(found!=ERROR_FILE_NOT_FOUND&&(found!=ERROR_SUCCESS||type!=REG_SZ||size<sizeof(wchar_t)||existing.back()!=0||std::wstring(existing.data())!=expected)){
        RegCloseKey(key);return false;
    }
    LONG result{};if(enabled){const auto text=L"\""+exe_path()+L"\" --startup";
        result=RegSetValueExW(key,L"PaneBind",0,REG_SZ,reinterpret_cast<const BYTE*>(text.c_str()),static_cast<DWORD>((text.size()+1)*sizeof(wchar_t)));}
    else{result=RegDeleteValueW(key,L"PaneBind");if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}
    RegCloseKey(key);return result==ERROR_SUCCESS;
}
void Application::show_settings(){
    if(gesture||native_move||input.native_move_in_progress())finish(v::End::Paused);
    settings_ui.show(window,settings,[this](const v::Settings& value){
        const auto previous=settings;
        const bool withdraw=!value.enabled||value.rules!=settings.rules;
        if(withdraw){finish(v::End::Paused);for(auto& [key,record]:visual){(void)key;visual_restore(record);}visual.clear();}
        auto keys=prepare_keys(value);if(!keys)return false;
        // Startup changes are an explicit setting action, never performed by
        // build/tests or silently on first launch. Default remains off.
        if(value.startup!=settings.startup&&!set_startup(value.startup)){discard_new_keys(*keys);notify(L"无法更改自己的登录启动入口，配置未更改。");return false;}
        if(!save(value)){discard_new_keys(*keys);if(value.startup!=previous.startup)set_startup(previous.startup);return false;}
        commit_keys(std::move(*keys));
        if(value.enabled&&!previous.enabled){
            input.stop();if(!input.start(window,diagnostic)){settings.enabled=false;input.pause(true);notify(L"配置已保存，但输入资源恢复失败；当前保持暂停。");tray();return true;}
        }
        if(withdraw||value.enabled!=previous.enabled)input.pause(!settings.enabled);
        tray();prepare();return true;
    },[this]{export_diagnostic();},[this]()->std::optional<v::Rule>{
        if(!last_target)return {};
        const auto fresh=catalog.get(last_target->hwnd,settings);
        if(!fresh||!Catalog::same(*last_target,*fresh))return {};
        return v::Rule{utf8(fresh->exe),utf8(fresh->window_class),true,0xffffffffu};
    });
    log.event("settings_open","\"created\":"+std::string(settings_ui.handle()?"true":"false")+
        ",\"foreground_is_settings\":"+(GetForegroundWindow()==settings_ui.handle()?"true":"false")+
        ",\"own_focus_present\":"+(GetFocus()?"true":"false"));
}
void Application::export_diagnostic(){
    const auto failure=[this](const wchar_t* message){
        notify(message);
        // This is feedback to an explicit settings/tray export request. Own
        // the dialog with our settings window when present, never a target
        // application, and do not force foreground or a system-modal/topmost UI.
        const HWND settings_owner=settings_ui.handle(),owner=settings_owner?settings_owner:window;
        SetLastError(0);const int result=MessageBoxW(owner,message,L"PaneBind：诊断导出失败",MB_OK|MB_ICONERROR);
        const DWORD error=result?0:GetLastError();
        log.event("diagnostic_export_failure_dialog","\"MessageBoxW_return\":"+std::to_string(result)+
            ",\"native_error\":"+std::to_string(error)+",\"owner_is_settings\":"+(settings_owner?"true":"false")+
            ",\"native_return_is_not_visible_observation\":true");
    };
    const auto name=config_path.parent_path()/(L"diagnostic-"+std::to_wstring(GetTickCount64())+L".txt");
    if(fs::exists(name)){failure(L"诊断目标已存在，未覆盖；未导出新文件。");return;}
    auto redacted=settings;std::size_t rule_index{};
    // A basename can collide after redaction although the full-path rules
    // were distinct. Unique opaque names preserve a valid serializable copy.
    for(auto& rule:redacted.rules)rule.exe="redacted-app-"+std::to_string(++rule_index)+".exe";
    std::array<wchar_t,64> build{};DWORD length=static_cast<DWORD>(sizeof(build)),ubr{},dword_length=sizeof(ubr);
    const bool build_known=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"CurrentBuildNumber",RRF_RT_REG_SZ,nullptr,build.data(),&length)==ERROR_SUCCESS;
    const bool ubr_known=RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"UBR",RRF_RT_REG_DWORD,nullptr,&ubr,&dword_length)==ERROR_SUCCESS;
    std::ostringstream text;text<<"PaneBind V1\nSOURCE_SHA="<<PANEBIND_BUILD_SHA<<"\nHUMAN_UAT=PENDING\n"
        <<"OS_BUILD="<<(build_known?utf8(build.data()):"UNKNOWN")<<"\nOS_UBR="<<(ubr_known?std::to_string(ubr):"UNKNOWN")
        <<"\nLAST_GESTURE_END="<<(last_termination?std::to_string(static_cast<int>(*last_termination)):"NOT_OBSERVED")
        <<"\nHOOK_OWNER_RUNNING="<<(input.healthy()?"true":"false")<<"\n";
    for(const auto& display:catalog.displays())text<<"DISPLAY_DPI="<<display.dpi<<" WORK="<<rectangle(display.work)<<"\n";
    if(last_target)text<<"LAST_WINDOW_CLASS="<<utf8(last_target->window_class)<<" DPI="<<last_target->dpi<<"\n";
    text<<v::serialize(redacted)<<"\nRecent failure: "<<utf8(problem)
        <<"\nWindow titles, document contents and full rule paths are not collected.\n";
    if(!atomic_save(name,text.str())){failure(L"诊断写入失败；未声明导出成功。");return;}
    notify(L"诊断已导出："+name.filename().wstring());
}
void Application::menu(){
    const bool record=tray_menu_records<8;const unsigned index=record?++tray_menu_records:0;
    POINT pt{};const BOOL cursor=GetCursorPos(&pt);const DWORD cursor_error=cursor?ERROR_SUCCESS:GetLastError();
    HMENU popup=CreatePopupMenu();const DWORD popup_error=popup?ERROR_SUCCESS:GetLastError();
    unsigned append_calls{},append_succeeded{},first_append_failure{};DWORD first_append_error{};
    std::array<BOOL,64> append_returns{};
    const auto append=[&](HMENU target,UINT flags,UINT_PTR id,const wchar_t* text){
        const BOOL result=AppendMenuW(target,flags,id,text);const DWORD error=result?ERROR_SUCCESS:GetLastError();
        if(record){if(append_calls<append_returns.size())append_returns[append_calls]=result;
            ++append_calls;if(result)++append_succeeded;else if(!first_append_failure){first_append_failure=append_calls;first_append_error=error;}}
        return result;
    };
    append(popup,MF_STRING,menu_pause,settings.enabled?L"暂停增强 (&P)":L"恢复增强 (&P)");
    append(popup,MF_STRING,menu_settings,L"设置 (&S)");append(popup,MF_STRING,menu_exclude,L"排除上一个应用");
    HMENU commands=CreatePopupMenu();const DWORD commands_error=commands?ERROR_SUCCESS:GetLastError();
    for(int a=static_cast<int>(v::Action::Minimize);a<=static_cast<int>(v::Action::Undo);++a)
        append(commands,MF_STRING,600+static_cast<UINT>(a),action_label(static_cast<v::Action>(a)).c_str());
    append(popup,MF_POPUP,reinterpret_cast<UINT_PTR>(commands),L"窗口操作");append(popup,MF_STRING,menu_help,L"帮助");
    append(popup,MF_STRING,menu_diagnostic,L"导出诊断");append(popup,MF_SEPARATOR,0,nullptr);append(popup,MF_STRING,menu_exit,L"退出 (&X)");
    // Activating our own user-requested tray menu is not source activation or
    // foreground recovery. No third-party window is activated by the product.
    const HWND foreground_before=record?GetForegroundWindow():nullptr;
    const BOOL foreground_return=SetForegroundWindow(window);
    const HWND foreground_after=record?GetForegroundWindow():nullptr;
    if(record){try{
        std::string raw_append="[";for(unsigned n=0;n<append_calls&&n<append_returns.size();++n){
            if(n)raw_append+=',';raw_append+=std::to_string(append_returns[n]);}raw_append+=']';
        log.event("tray_menu_before_track","\"record_index\":"+std::to_string(index)+",\"record_limit\":8"+
            ",\"owner_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(window))+",\"dispatch_TID\":"+std::to_string(GetCurrentThreadId())+
            ",\"GetCursorPos_return\":"+(cursor?"true":"false")+",\"GetCursorPos_native_BOOL\":"+std::to_string(cursor)+
            ",\"GetCursorPos_error_on_failure\":"+(cursor?"null":std::to_string(cursor_error))+
            ",\"original_screen_point\":["+std::to_string(pt.x)+","+std::to_string(pt.y)+"]"+
            ",\"CreatePopupMenu_return\":"+std::to_string(reinterpret_cast<std::uintptr_t>(popup))+
            ",\"CreatePopupMenu_error_on_failure\":"+(popup?"null":std::to_string(popup_error))+
            ",\"CreatePopupMenu_submenu_return\":"+std::to_string(reinterpret_cast<std::uintptr_t>(commands))+
            ",\"CreatePopupMenu_submenu_error_on_failure\":"+(commands?"null":std::to_string(commands_error))+
            ",\"AppendMenuW_attempts\":"+std::to_string(append_calls)+",\"AppendMenuW_nonzero_returns\":"+std::to_string(append_succeeded)+
            ",\"AppendMenuW_native_BOOLs\":"+raw_append+",\"AppendMenuW_returns_truncated\":"+(append_calls>append_returns.size()?"true":"false")+
            ",\"first_AppendMenuW_failure_index\":"+(first_append_failure?std::to_string(first_append_failure):"null")+
            ",\"first_AppendMenuW_error_on_failure\":"+(first_append_failure?std::to_string(first_append_error):"null")+
            ",\"SetForegroundWindow_return\":"+(foreground_return?"true":"false")+
            ",\"SetForegroundWindow_native_BOOL\":"+std::to_string(foreground_return)+
            ",\"foreground_before_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_before))+
            ",\"foreground_after_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(foreground_after))+
            ",\"SetForegroundWindow_LastError_not_defined_or_used\":true"+
            ",\"TrackPopupMenu_flags\":"+std::to_string(TPM_RIGHTBUTTON)+",\"TrackPopupMenu_reserved\":0"+
            ",\"TrackPopupMenu_call_not_yet_executed\":true,\"next_call_is_original_TrackPopupMenu\":true");
    }catch(...){}}
    LARGE_INTEGER before{},after{};const bool before_clock=record&&QueryPerformanceCounter(&before)!=FALSE;
    const BOOL tracked=TrackPopupMenu(popup,TPM_RIGHTBUTTON,pt.x,pt.y,0,window,nullptr);
    const DWORD track_error=tracked?ERROR_SUCCESS:GetLastError();
    const bool after_clock=record&&QueryPerformanceCounter(&after)!=FALSE;
    const BOOL posted=PostMessageW(window,WM_NULL,0,0);const DWORD post_error=posted?ERROR_SUCCESS:GetLastError();
    const BOOL destroyed=DestroyMenu(popup);const DWORD destroy_error=destroyed?ERROR_SUCCESS:GetLastError();
    if(record){try{
        log.event("tray_menu_after_track","\"record_index\":"+std::to_string(index)+",\"record_limit\":8"+
            ",\"owner_HWND\":"+std::to_string(reinterpret_cast<std::uintptr_t>(window))+
            ",\"TrackPopupMenu_attempted\":true,\"TrackPopupMenu_completed\":true,\"TrackPopupMenu_return\":"+(tracked?"true":"false")+
            ",\"TrackPopupMenu_native_BOOL\":"+std::to_string(tracked)+
            ",\"TrackPopupMenu_error_on_failure\":"+(tracked?"null":std::to_string(track_error))+
            ",\"TrackPopupMenu_before_qpc\":"+(before_clock?std::to_string(before.QuadPart):"null")+
            ",\"TrackPopupMenu_after_qpc\":"+(after_clock?std::to_string(after.QuadPart):"null")+
            ",\"PostMessage_WM_NULL_return\":"+(posted?"true":"false")+
            ",\"PostMessage_WM_NULL_native_BOOL\":"+std::to_string(posted)+
            ",\"PostMessage_WM_NULL_error_on_failure\":"+(posted?"null":std::to_string(post_error))+
            ",\"DestroyMenu_return\":"+(destroyed?"true":"false")+
            ",\"DestroyMenu_native_BOOL\":"+std::to_string(destroyed)+
            ",\"DestroyMenu_error_on_failure\":"+(destroyed?"null":std::to_string(destroy_error))+
            ",\"TPM_RETURNCMD_used\":false,\"menu_selection_or_resource_cleanup_PASS_inferred\":false");
    }catch(...){}}
}
void Application::stop(){if(stopping)return;stopping=true;hide_preview("stop");input.revoke();input.stop();finish(v::End::Exit);settings_ui.close();
    drain_button_admissions(true);
    log_delayed_completions();
    for(auto& [key,record]:visual){(void)key;visual_restore(record);}visual.clear();
    const bool native_writer_exited=writer.stop();
    const bool issued_completion_unknown=writer.has_unconfirmed_native_call();
    log_delayed_completions();
    // The input producer has stopped and the issued writer's final drain is
    // now observed. Unfinished/unknown calls remain explicit, never zeroed to
    // make a partial timing population look like the complete shutdown set.
    const auto timing_fields=[](const v::TimingSnapshot& value,unsigned percentile){
        const auto upper=value.percentile_upper_us(percentile);
        return "\"samples\":"+std::to_string(value.samples)+",\"discarded\":"+std::to_string(value.discarded)+
            ",\"missing_clock\":"+std::to_string(value.missing_clock)+",\"reversed_clock\":"+std::to_string(value.reversed_clock)+
            ",\"conversion_overflow\":"+std::to_string(value.conversion_overflow)+",\"saturated\":"+(value.saturated?"true":"false")+
            ",\"percentile\":"+std::to_string(percentile)+",\"percentile_bucket_upper_us\":"+(upper?std::to_string(*upper):"null")+
            ",\"maximum_us\":"+std::to_string(value.maximum_us);
    };
    log.event("timing_summary","\"qpc_frequency\":"+std::to_string(input.counter_frequency())+
        ",\"clock_scope\":\"same_machine_QPC_software_observation_cross_thread_one_tick_quantization\""+
        ",\"after_native_writer_stop_and_final_drain\":true,\"native_writer_exited\":"+(native_writer_exited?"true":"false")+
        ",\"issued_native_completion_unknown\":"+(issued_completion_unknown?"true":"false")+
        ",\"mouse_callback\":{"+timing_fields(input.callback_timing(),99)+",\"excludes_downstream_CallNextHookEx\":true}"+
        ",\"hook_observed_to_postverify\":{"+timing_fields(input_post_timing.snapshot(),95)+
        ",\"sample_unit\":\"native_placement\",\"endpoint\":\"geometry_and_post_context_complete\",\"not_physical_display_latency\":true,\"unfinished_calls\":"+std::to_string(delayed_input_qpc.size())+"}");
    const bool recovery_stopped=recovery.stop();
    // A confirmed helper stop postverified all its held originals AFTER the
    // exact completion barrier. Retire local journals too, including a job
    // cancelled before issue by stop() (which need not produce a late wake).
    // This is final property cleanup only, never a successful old mutation or
    // renewed input/geometry authority.
    if(recovery_stopped&&!writer.has_pending_native_operation()){
        pending_visual.clear();deferred_drag_visual.reset();recovery_owned.clear();
    }
    const bool visual_restore_confirmed=recovery_stopped&&pending_visual.empty()&&!deferred_drag_visual;
    for(const auto& key:bound_keys)UnregisterHotKey(window,key.id);bound_keys.clear();
    NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);
    if(overlay)DestroyWindow(overlay);overlay=nullptr;WTSUnRegisterSessionNotification(window);
    log.event("shutdown","\"own_hooks_stopped\":"+std::string(input.guest_callback_owner_stop_clean()?"true":"false")+
        ",\"own_overlay_destroyed\":true,\"native_worker_exited\":"+
        std::string(native_writer_exited?"true":"false")+",\"issued_native_completion_unknown\":"+(issued_completion_unknown?"true":"false")+
        ",\"visual_recovery_stopped\":"+(recovery_stopped?"true":"false")+
        ",\"visual_restore_confirmed\":"+(visual_restore_confirmed?"true":"false")+
        ",\"guest_capture_route_used\":"+(input.capture_route_is_guest()?"true":"false")+
        ",\"capture_resource_route_used\":"+(input.capture_route_enabled()?"true":"false")+
        ",\"capture_resource_route\":\""+escaped(input.capture_route_mode())+"\""+
        ",\"own_input_resources_stopped\":"+((!input.capture_route_enabled()||input.capture_stop_facts().clean())&&input.guest_callback_owner_stop_clean()?"true":"false")+
        ",\"own_capture_released\":"+(input.capture_route_enabled()?(input.capture_stop_facts().capture_released?"true":"false"):"null")+
        ",\"own_input_association_detached\":"+(input.capture_route_enabled()?(input.capture_stop_facts().association_detached?"true":"false"):"null")+
        ",\"mouse_up_callbacks\":"+mouse_up_callback_json(input.mouse_up_callbacks()));
}
LRESULT CALLBACK overlay_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_NCHITTEST)return HTTRANSPARENT;if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(w,&paint);RECT r{};GetClientRect(w,&r);
        const auto* app=application;
        HBRUSH brush=CreateSolidBrush(app?(app->overlay_outline?app->overlay_key:app->overlay_color):RGB(0,120,215));
        FillRect(dc,&r,brush);DeleteObject(brush);
        if(app&&app->overlay_outline){
            const int thickness=std::max(1,MulDiv(1,static_cast<int>(app->overlay_dpi),96));
            HBRUSH line=CreateSolidBrush(GetSysColor(COLOR_GRAYTEXT));
            for(const auto& member:app->overlay_members){
                RECT border{static_cast<LONG>(member.left()-app->overlay_bounds.left()),static_cast<LONG>(member.top()-app->overlay_bounds.top()),
                    static_cast<LONG>(member.right()-app->overlay_bounds.left()),static_cast<LONG>(member.bottom()-app->overlay_bounds.top())};
                for(int stroke=0;stroke<thickness&&border.right>border.left&&border.bottom>border.top;++stroke){FrameRect(dc,&border,line);InflateRect(&border,-1,-1);}
            }
            DeleteObject(line);
            const auto draw_edges=[&](const std::vector<FeedbackLine>& lines,int dip_width){
                const LONG width=std::max(1,MulDiv(dip_width,static_cast<int>(app->overlay_dpi),96));
                HBRUSH highlight=CreateSolidBrush(app->overlay_color);
                for(const auto& segment:lines){
                    const LONG left=static_cast<LONG>(segment.first.x-app->overlay_bounds.left());
                    const LONG top=static_cast<LONG>(segment.first.y-app->overlay_bounds.top());
                    const LONG right=static_cast<LONG>(segment.last.x-app->overlay_bounds.left());
                    const LONG bottom=static_cast<LONG>(segment.last.y-app->overlay_bounds.top());
                    RECT strip{};
                    if(left==right){const LONG x=std::clamp(left-width/2,0L,std::max(0L,r.right-width));
                        strip={x,top,std::min(r.right,x+width),bottom};}
                    else{const LONG y=std::clamp(top-width/2,0L,std::max(0L,r.bottom-width));
                        strip={left,y,right,std::min(r.bottom,y+width)};}
                    if(strip.left<strip.right&&strip.top<strip.bottom)FillRect(dc,&strip,highlight);
                }
                DeleteObject(highlight);
            };
            // Member membership is a thin neutral outline. Thick coloured
            // strokes identify only actual selected magnet/shared-edge facts.
            draw_edges(app->overlay_shared_edges,3);draw_edges(app->overlay_magnet_edges,4);
        }
        // A drag-alpha limitation is also painted over an ordinary docking
        // preview, not only when optional outlined feedback is enabled.
        if(app&&!app->overlay_hint.empty()){
                RECT caption=r;caption.bottom=std::min<LONG>(caption.bottom,MulDiv(25,static_cast<int>(app->overlay_dpi),96));
                HBRUSH background=CreateSolidBrush(GetSysColor(COLOR_INFOBK));FillRect(dc,&caption,background);DeleteObject(background);
                const auto font=CreateFontW(-MulDiv(9,static_cast<int>(app->overlay_dpi),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                    DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft YaHei UI");
                const auto old=font?SelectObject(dc,font):nullptr;
                const auto previous_mode=SetBkMode(dc,TRANSPARENT);const auto previous_color=SetTextColor(dc,GetSysColor(COLOR_INFOTEXT));
                caption.left+=MulDiv(4,static_cast<int>(app->overlay_dpi),96);
                DrawTextW(dc,app->overlay_hint.c_str(),static_cast<int>(app->overlay_hint.size()),&caption,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                SetTextColor(dc,previous_color);SetBkMode(dc,previous_mode);if(font){SelectObject(dc,old);DeleteObject(font);}
        }
        EndPaint(w,&paint);return 0;}
    return DefWindowProcW(w,message,wp,lp);
}
void create_tray(Application& app){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=app.window;n.uID=1;
    n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=tray_message;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(n.szTip,L"PaneBind");const bool added=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;
    n.uVersion=NOTIFYICON_VERSION_4;const bool versioned=added&&Shell_NotifyIconW(NIM_SETVERSION,&n)!=FALSE;
    app.log.event("tray_registration","\"owner_hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(app.window))+
        ",\"uid\":1,\"NIM_ADD_succeeded\":"+(added?"true":"false")+",\"NIM_SETVERSION_attempted\":"+(added?"true":"false")+
        ",\"NIM_SETVERSION_succeeded\":"+(versioned?"true":"false")+",\"icon_reachability_or_menu_delivery_not_inferred\":true");app.tray();}
LRESULT CALLBACK main_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    auto* a=application;if(!a)return DefWindowProcW(w,message,wp,lp);
    try{
        if(message==a->taskbar_created){create_tray(*a);return 0;}
        switch(message){
        case prepare_message:a->prepare();return 0;
        case input_message:a->consume();return 0;
        case native_completion_message:
            a->log_delayed_completions();
            // A skipped catalog notification is resumed only by the actual
            // completion event; another still-pending call keeps it deferred.
            a->refresh_idle_catalog();return 0;
        case WM_TIMER:if(a->idle_notice_timer==static_cast<UINT_PTR>(wp))a->idle_notice_timeout(static_cast<UINT_PTR>(wp));
            else if(a->gesture&&a->gesture->restore_timer==static_cast<UINT_PTR>(wp))a->restore_timeout(static_cast<UINT_PTR>(wp));
            else a->seam_timeout(static_cast<UINT_PTR>(wp));return 0;
        case catalog_message:{
            if(wp==2){a->settings.enabled=false;a->finish(v::End::Paused);a->tray();
                a->log.event("emergency_hotkey","\"native_message\":\"WM_HOTKEY\",\"registration_id\":910,\"received_count\":"+
                    std::to_string(a->input.emergency_hotkey_messages()));}
            else if(wp==1){a->log.event("input_queue_failure",a->input.guest_queue_failure_json());
                a->input.revoke();a->finish(v::End::QueueFailure);a->notify(L"输入队列故障，已停止增强；请暂停后恢复。");}
            else if(wp==3)a->callback_owner_stop_fact(false);
            else if(wp==4)a->callback_owner_stop_fact(true);
            if(a->gesture&&a->gesture->restore_pending)a->restore_progress();
            a->refresh_idle_catalog();
            return 0;}
        case tray_message:{
            a->observe_tray_ingress(w,wp,lp);
            const auto route=tray_callback_route(LOWORD(lp));
            if(route==TrayCallbackRoute::Menu)a->menu();
            else if(route==TrayCallbackRoute::Settings)a->show_settings();
            return 0;}
        case WM_HOTKEY:{
            const auto found=std::find_if(a->bound_keys.begin(),a->bound_keys.end(),[&](const auto& key){return key.id==static_cast<int>(wp);});
            if(found!=a->bound_keys.end())a->command(found->binding.action);return 0;}
        case WM_COMMAND:{
            const auto id=LOWORD(wp);if(id>=600&&id<=600+static_cast<int>(v::Action::Undo)){a->command(static_cast<v::Action>(id-600));return 0;}
            if(id==menu_pause)a->set_paused(a->settings.enabled);
            else if(id==menu_settings)a->show_settings();
            else if(id==menu_exclude&&a->last_target){
                a->finish(v::End::Paused);auto next=a->settings;
                const auto exe=utf8(a->last_target->exe);const bool exists=std::any_of(next.rules.begin(),next.rules.end(),[&](const auto& r){return r.exe==exe&&r.excluded;});
                if(!exists){
                    // Restore only properties still owned by us, while the
                    // old rule snapshot still permits that exact identity.
                    for(auto it=a->visual.begin();it!=a->visual.end();){
                        if(it->second.identity.exe==a->last_target->exe){a->visual_restore(it->second);it=a->visual.erase(it);}else ++it;
                    }
                    next.rules.push_back({exe,"",true,0xffffffffu});a->save(next);
                }a->prepare();
            }else if(id==menu_help){
                MessageBoxW(w,L"普通权限运行。标题栏移动；边角缩放；Ctrl 起手联动邻接组，Shift 移动临时透传。\n"
                    L"边双击拉伸，Shift 双击沿同轴拉伸；屏边显示停靠预览，松开提交。\n"
                    L"托盘提供设置、窗口命令、排除、暂停和退出。Ctrl+Shift+F11 紧急暂停，Esc 取消当前增强手势。\n"
                    L"未准备或不合格窗口保留原生操作；不控制高权限／安全桌面窗口。\n"
                    L"当前构建的实际验证及限制请看随包中文说明，不将开发状态当作验收通过。",L"PaneBind 帮助",MB_OK|MB_ICONINFORMATION);
            }else if(id==menu_diagnostic)a->export_diagnostic();
            else if(id==menu_exit)DestroyWindow(w);return 0;}
        case external_message:
            if(wp==1)DestroyWindow(w);else if(wp==2)a->set_paused(true);else if(wp==3)a->log_status();else a->show_settings();return 0;
        case WM_COPYDATA:{
            const auto* data=reinterpret_cast<COPYDATASTRUCT*>(lp);
            if(data&&data->dwData==0x50424331&&data->cbData>=1&&data->cbData<=128&&data->lpData){
                const auto bytes=static_cast<const char*>(data->lpData);
                if(bytes[data->cbData-1]!='\0'||a->guest_callback_owner_stop_pending)return FALSE;
                const auto request=parse_guest_callback_owner_stop(std::string_view(bytes,data->cbData-1));
                if(!request)return FALSE;a->guest_callback_owner_stop_pending=*request;
                if(PostMessageW(w,guest_callback_owner_stop_message,0,0))return TRUE;
                a->guest_callback_owner_stop_pending.reset();return FALSE;
            }
            if(data&&data->dwData==0x50425131&&data->cbData>=1&&data->cbData<=128&&data->lpData){
                const auto bytes=static_cast<const char*>(data->lpData);
                if(bytes[data->cbData-1]!='\0'||a->guest_queue_failure_pending)return FALSE;
                const auto request=parse_guest_queue_failure(std::string_view(bytes,data->cbData-1));
                if(!request)return FALSE;a->guest_queue_failure_pending=*request;
                if(PostMessageW(w,guest_queue_failure_message,0,0))return TRUE;
                a->guest_queue_failure_pending.reset();return FALSE;
            }
            if(data&&data->dwData==0x50425631&&data->cbData>=1&&data->cbData<80&&data->lpData){
                const auto name=std::string_view(static_cast<const char*>(data->lpData),data->cbData-1);
                const auto command=v::action_from_name(name);
                // Copy only the validated enum, never retain WM_COPYDATA's
                // borrowed pointer. Catalog's COM desktop admission must run
                // in the normal STA pump, not a cross-process SendMessage.
                if(command)return PostMessageW(w,queued_command_message,static_cast<WPARAM>(*command),0)?TRUE:FALSE;}
            return FALSE;}
        case guest_queue_failure_message:{
            if(!a->guest_queue_failure_pending)return 0;
            const auto request=std::move(*a->guest_queue_failure_pending);a->guest_queue_failure_pending.reset();
            std::string why="no_current_exact_plain_Move";bool armed=false;
            if(!a->stopping&&a->settings.enabled&&!a->writer.has_pending_native_operation()&&a->gesture&&!a->gesture->ctrl&&!is_resize(a->gesture->edges)&&
               a->gesture->down.generation==request.generation&&a->input.allowed(request.generation)&&
               reinterpret_cast<std::uintptr_t>(a->gesture->operation_source.hwnd)==request.source&&a->gesture->exact_motion&&
               a->gesture->exact_motion->generation==request.generation){
                const auto& source=a->gesture->operation_source;
                const auto actual=a->catalog.get(source.hwnd,a->settings);
                const auto exact=a->last_exact_receipts.find(source.key);
                if(actual&&exact!=a->last_exact_receipts.end()&&Catalog::same(source,*actual)&&
                   WindowWriter::prepare(exact->second,*actual,exact->second.visible).accepted)
                    armed=a->input.arm_guest_queue_failure(request.run,request.generation,source.hwnd,why);
                else why="fresh_geometry_has_no_current_exact_writer_receipt";
            }
            a->log.event("guest_queue_failure_arm","\"armed\":"+std::string(armed?"true":"false")+
                ",\"reason\":\""+escaped(why)+"\",\"run_id\":\""+request.run+"\",\"generation\":"+
                std::to_string(request.generation)+",\"source_HWND\":"+std::to_string(request.source)+","+
                a->input.guest_queue_failure_json());return 0;}
        case guest_callback_owner_stop_message:{
            if(!a->guest_callback_owner_stop_pending)return 0;
            const auto request=std::move(*a->guest_callback_owner_stop_pending);a->guest_callback_owner_stop_pending.reset();
            std::string why="no_current_exact_plain_Move";bool requested=false;
            if(!a->stopping&&a->settings.enabled&&!a->writer.has_pending_native_operation()&&a->gesture&&!a->gesture->ctrl&&!is_resize(a->gesture->edges)&&
               a->gesture->down.generation==request.generation&&a->input.allowed(request.generation)&&
               reinterpret_cast<std::uintptr_t>(a->gesture->operation_source.hwnd)==request.source&&a->gesture->exact_motion&&
               a->gesture->exact_motion->generation==request.generation){
                const auto& source=a->gesture->operation_source;const auto actual=a->catalog.get(source.hwnd,a->settings);
                const auto exact=a->last_exact_receipts.find(source.key);
                if(actual&&exact!=a->last_exact_receipts.end()&&Catalog::same(source,*actual)&&
                   WindowWriter::prepare(exact->second,*actual,exact->second.visible).accepted)
                    requested=a->input.request_guest_callback_owner_stop(request.run,request.generation,source.hwnd,why);
                else why="fresh_geometry_has_no_current_exact_writer_receipt";
            }
            a->log.event("guest_callback_owner_stop_arm","\"requested\":"+std::string(requested?"true":"false")+
                ",\"reason\":\""+escaped(why)+"\",\"run_id\":\""+request.run+"\",\"generation\":"+
                std::to_string(request.generation)+",\"source_HWND\":"+std::to_string(request.source)+","+
                a->input.guest_callback_owner_stop_json());return 0;}
        case queued_command_message:
            if(wp<v::settings_detail::action_names.size())a->command(static_cast<v::Action>(wp));return 0;
        case WM_DISPLAYCHANGE:case WM_DPICHANGED:case WM_SETTINGCHANGE:
            a->input.revoke();a->finish(v::End::SessionLost);a->prepare();return 0;
        case WM_POWERBROADCAST:
            if(wp==PBT_APMSUSPEND||wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND){
                a->input.revoke();a->finish(v::End::SessionLost);a->input.prepare({});
                // Automatic resume does not prove that a user is present.
                // New preparation still performs active-input-desktop checks.
                if(wp!=PBT_APMSUSPEND)a->prepare();return TRUE;
            }break;
        case WM_WTSSESSION_CHANGE:
            if(wp==WTS_SESSION_LOCK||wp==WTS_SESSION_LOGOFF||wp==WTS_CONSOLE_DISCONNECT||wp==WTS_REMOTE_DISCONNECT){
                a->input.revoke();a->finish(v::End::SessionLost);a->input.prepare({});}
            else a->prepare();return 0;
        case WM_QUERYENDSESSION:a->input.revoke();a->finish(v::End::SessionLost);return TRUE;
        case WM_CLOSE:DestroyWindow(w);return 0;
        case WM_DESTROY:a->stop();PostQuitMessage(0);return 0;
        default:break;
        }
    }catch(const std::exception& error){a->input.revoke();a->finish(v::End::QueueFailure);a->notify(L"操作已中止："+wide(error.what()));}
    return DefWindowProcW(w,message,wp,lp);
}
bool ordinary_token(){
    HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;
    TOKEN_ELEVATION elevation{};DWORD size{},ui{},app{};bool ok=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)&&!elevation.TokenIsElevated&&
        GetTokenInformation(token,TokenUIAccess,&ui,sizeof(ui),&size)&&!ui&&GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&size)&&!app;
    GetTokenInformation(token,TokenIntegrityLevel,nullptr,0,&size);std::vector<BYTE> bytes(size);
    if(!size||!GetTokenInformation(token,TokenIntegrityLevel,bytes.data(),size,&size))ok=false;
    else{const auto sid=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(bytes.data())->Label.Sid;
        const auto rid=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1);ok=ok&&rid==SECURITY_MANDATORY_MEDIUM_RID;}
    CloseHandle(token);return ok;
}
void output(std::string_view text){DWORD written{};auto h=GetStdHandle(STD_OUTPUT_HANDLE);
    if(h&&h!=INVALID_HANDLE_VALUE)WriteFile(h,text.data(),static_cast<DWORD>(text.size()),&written,nullptr);}
} // namespace

int run(HINSTANCE instance,const wchar_t*){
    int count{};LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 1;
    std::vector<std::wstring> argv;for(int i=1;i<count;++i)argv.emplace_back(args[i]);LocalFree(args);
    const bool fault_switch=std::find(argv.begin(),argv.end(),L"--guest-queue-failure")!=argv.end();
    const bool callback_stop_switch=std::find(argv.begin(),argv.end(),L"--guest-callback-owner-stop")!=argv.end();
    std::optional<GuestQueueFailureRequest> queue_failure_command;
    std::optional<GuestQueueFailureRequest> callback_stop_command;
    if(callback_stop_switch){
        const auto scope=guest::detect_capture_scope(PANEBIND_BUILD_SHA);if(!scope.enabled)return 78;
        if(argv.size()!=4||argv[0]!=L"--guest-callback-owner-stop")return 2;
        callback_stop_command=parse_guest_callback_owner_stop("callback-owner-stop "+utf8(argv[1])+" "+utf8(argv[2])+" "+utf8(argv[3]));
        if(!callback_stop_command||callback_stop_command->run!=scope.run_id)return 78;
    }
    if(fault_switch){
        // A test fault request must not silently become an ordinary launch,
        // nor may --build-identity bypass its exact disposable-guest guard.
        const auto scope=guest::detect_capture_scope(PANEBIND_BUILD_SHA);
        if(!scope.enabled)return 78;
        if(argv.size()!=4||argv[0]!=L"--guest-queue-failure")return 2;
        queue_failure_command=parse_guest_queue_failure("queue-failure "+utf8(argv[1])+" "+utf8(argv[2])+" "+utf8(argv[3]));
        if(!queue_failure_command||queue_failure_command->run!=scope.run_id)return 78;
    }
    if(const auto helper=VisualRecovery::helper_entry(argv,PANEBIND_BUILD_SHA))return *helper;
    if(std::find(argv.begin(),argv.end(),L"--build-identity")!=argv.end()){
        output("{\"product\":\"PaneBind\",\"version\":\"1.0.0-dev\",\"source_commit\":\"" PANEBIND_BUILD_SHA "\",\"subsystem\":\"windows\",\"requires_elevation\":false}\n");return 0;}
    if(argv.size()==2&&argv[0]==L"--config-check"){
        std::string text;const auto parsed=read_file(argv[1],text)?v::parse(text):v::ParseResult{};
        output(parsed.value?"{\"valid\":true}\n":"{\"valid\":false}\n");return parsed.value?0:2;
    }
    if(!ordinary_token()){MessageBoxW(nullptr,L"PaneBind V1 正式入口需要普通 Medium 权限。请不要以管理员身份运行。\nHigh guest 的旧测试例外不适用于正式产品。",L"PaneBind — 权限不适用",MB_OK|MB_ICONWARNING);return 78;}
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(com))return 1;
    const HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\PaneBind.V1.SingleInstance");
    const bool existing=mutex&&GetLastError()==ERROR_ALREADY_EXISTS;
    if(!mutex){CoUninitialize();return 1;}
    if(existing){const auto target=FindWindowW(app_class,L"PaneBind");int delivered=2;
        const bool opens_settings=argv.empty()||argv[0]==L"--settings"||
            (argv.size()==2&&argv[0]==L"--command"&&argv[1]==L"Settings");
        if(target&&opens_settings){DWORD target_pid{};
            if(GetWindowThreadProcessId(target,&target_pid)&&target_pid)AllowSetForegroundWindow(target_pid);}
        if(target){if(callback_stop_command){
                const auto name="callback-owner-stop "+callback_stop_command->run+" "+std::to_string(callback_stop_command->generation)+" "+std::to_string(callback_stop_command->source);
                COPYDATASTRUCT data{0x50424331,static_cast<DWORD>(name.size()+1),const_cast<char*>(name.c_str())};DWORD_PTR result{};
                delivered=SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,1000,&result)&&result?0:2;
            }else if(queue_failure_command){
                const auto name="queue-failure "+queue_failure_command->run+" "+std::to_string(queue_failure_command->generation)+" "+std::to_string(queue_failure_command->source);
                COPYDATASTRUCT data{0x50425131,static_cast<DWORD>(name.size()+1),const_cast<char*>(name.c_str())};DWORD_PTR result{};
                delivered=SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,1000,&result)&&result?0:2;
            }else if(argv.size()==2&&argv[0]==L"--command"){
                const auto name=utf8(argv[1]);COPYDATASTRUCT data{0x50425631,static_cast<DWORD>(name.size()+1),const_cast<char*>(name.c_str())};DWORD_PTR result{};
                delivered=SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,1000,&result)&&result?0:2;
            }else delivered=PostMessageW(target,external_message,(!argv.empty()&&argv[0]==L"--quit")?1:(!argv.empty()&&argv[0]==L"--pause")?2:
                (!argv.empty()&&argv[0]==L"--status")?3:0,0)?0:2;}
        CloseHandle(mutex);CoUninitialize();return delivered;
    }
    if(callback_stop_command||queue_failure_command||(!argv.empty()&&(argv[0]==L"--quit"||argv[0]==L"--pause"||argv[0]==L"--status"||argv[0]==L"--command"))){
        CloseHandle(mutex);CoUninitialize();return 2;}
    int result=1;
    try{
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
        const auto route=guest::detect_resource_route(PANEBIND_BUILD_SHA);
        Application app(instance,route.desktop_context);application=&app;app.mutex=mutex;app.initialize_paths();
        app.diagnostic=std::find(argv.begin(),argv.end(),L"--diagnostic")!=argv.end();
        WNDCLASSW wc{};wc.lpfnWndProc=main_proc;wc.hInstance=instance;wc.lpszClassName=app_class;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
        WNDCLASSW ow{};ow.lpfnWndProc=overlay_proc;ow.hInstance=instance;ow.lpszClassName=L"PaneBind.V1.Preview";RegisterClassW(&ow);
        app.window=CreateWindowExW(WS_EX_TOOLWINDOW,app_class,L"PaneBind",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
        if(!app.window)throw std::runtime_error("owner_window_failed");
        app.writer.notify_on_completion(app.window);
        app.overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,ow.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
        app.taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");create_tray(app);WTSRegisterSessionNotification(app.window,NOTIFY_FOR_THIS_SESSION);
        if(!app.input.start(app.window,app.diagnostic)){app.settings.enabled=false;app.notify(L"输入资源或紧急热键初始化失败，增强保持暂停。");}
        app.input.pause(!app.settings.enabled);app.register_keys();app.prepare();
        app.log.event("startup","\"entry\":\"production_gui\",\"integrity\":\"medium\",\"elevated\":false,\"initiator\":\"not_collected\"");
        app.log.event("input_route","\"guest_consumed_down_capture\":"+std::string(app.input.capture_route_is_guest()?"true":"false")+
            ",\"capture_resource_route_enabled\":"+(app.input.capture_route_enabled()?"true":"false")+
            ",\"capture_resource_route\":\""+escaped(app.input.capture_route_mode())+"\""+
            ",\"reason\":\""+escaped(app.input.capture_route_reason())+"\",\"initialization_success_not_function_PASS\":true");
        app.log.event("desktop_context","\"route\":\""+std::string(app.desktop_context?"exact_manifest_Run":"historical_Default")+
            "\",\"run_id\":\""+(app.desktop_context?app.desktop_context->run():std::string{})+
            "\",\"resource_context_not_window_authority\":true,\"native_validation\":\"NOT_RUN_AT_INITIALIZATION\"");
        if(!app.problem.empty())app.notify(app.problem);
        // An explicit ordinary launch must be discoverable even when the
        // shell cannot publish our notification icon. Background login and
        // diagnostic modes retain their existing no-window startup.
        if(argv.empty()||argv[0]==L"--settings")app.show_settings();
        MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(app.settings_ui.translate(msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}
        app.stop();application=nullptr;UnregisterClassW(ow.lpszClassName,instance);UnregisterClassW(app_class,instance);result=0;
    }catch(const std::exception& e){application=nullptr;MessageBoxW(nullptr,wide(e.what()).c_str(),L"PaneBind 启动失败",MB_OK|MB_ICONERROR);}
    CloseHandle(mutex);CoUninitialize();return result;
}
} // namespace panebind::platform::windows::product
