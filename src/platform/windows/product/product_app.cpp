#include "product_app.h"
#include "input_service.h"
#include "settings_window.h"
#include "visual_recovery.h"
#include "window_writer.h"
#include "core/v1/layout.h"
#include "core/v1/settings.h"
#include "core/v1/gesture.h"
#include "core/v1/visual_ownership.h"
#include "core/geometry/magnet_constraint_solver.h"
#include "core/movement/translation.h"
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
namespace fs=std::filesystem;
namespace {
constexpr wchar_t app_class[]=L"PaneBind.V1.Application";
constexpr UINT tray_message=WM_APP+30, external_message=WM_APP+31;
constexpr UINT menu_pause=500,menu_settings=501,menu_exclude=502,menu_help=503,menu_diagnostic=504,menu_exit=505;
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
v::Edges edges(int region){switch(region){case 1:return {true,false,false,false};case 2:return {false,false,true,false};
case 3:return {false,true,false,false};case 4:return {false,false,false,true};case 5:return {true,true,false,false};
case 6:return {false,true,true,false};case 7:return {true,false,false,true};case 8:return {false,false,true,true};default:return {};}}
bool is_resize(v::Edges e){return e.left||e.right||e.top||e.bottom;}
std::wstring action_label(v::Action a){
    constexpr std::array<const wchar_t*,37> labels{
        L"不执行",L"最小化",L"最大化",L"恢复",L"居中",L"置于后方",
        L"向左微移",L"向右微移",L"向上微移",L"向下微移",L"减小宽度",L"增加宽度",L"减小高度",L"增加高度",
        L"移到下一显示器",L"切换置顶",L"降低不透明度",L"提高不透明度",
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
            <<",\"tick\":"<<GetTickCount64()<<",\"type\":\""<<escaped(type)<<"\""<<(fields.empty()?"":",")<<fields<<"}\n";
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
    static std::optional<v::VisualState> read(HWND hwnd){
        SetLastError(0);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        if(!ex&&GetLastError())return {};
        v::VisualState value;value.topmost=(ex&WS_EX_TOPMOST)!=0;value.layered=(ex&WS_EX_LAYERED)!=0;
        if(value.layered){COLORREF color{};BYTE alpha{};DWORD flags{};
            value.attributes_known=GetLayeredWindowAttributes(hwnd,&color,&alpha,&flags)!=FALSE;
            if(value.attributes_known){value.alpha=alpha;value.color=color;value.flags=flags;}
        }return value;
    }
    static bool assign_alpha(HWND hwnd,const v::VisualState& expected,const v::VisualState& target,
                             const std::function<bool()>& identity){
        const auto current=read(hwnd);
        if(!identity()||!current||!v::same_alpha(*current,expected)||
            (target.layered&&!target.attributes_known))return false;
        if(v::same_alpha(*current,target))return true;
        SetLastError(0);const auto ex=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        if(!ex&&GetLastError())return false;
        if(target.layered){
            if(!current->layered){SetLastError(0);
                const auto before=SetWindowLongPtrW(hwnd,GWL_EXSTYLE,ex|WS_EX_LAYERED);
                if(!before&&GetLastError())return false;
            }
            if(!identity()||!SetLayeredWindowAttributes(hwnd,target.color,target.alpha,target.flags))return false;
        }else{
            SetLastError(0);const auto before=SetWindowLongPtrW(hwnd,GWL_EXSTYLE,ex&~WS_EX_LAYERED);
            if(!before&&GetLastError())return false;
        }
        const auto after=read(hwnd);return identity()&&after&&v::same_alpha(*after,target);
    }
};
struct Gesture {
    InputEvent down;std::vector<Snapshot> members,targets;std::map<v::Key,Snapshot> expected;
    v::Settings config;
    v::Rect original,previous,scope_original;v::Edges edges;bool ctrl{};std::uint64_t last_tick{};
    std::optional<m::MagnetAxisPreference> x,y;v::ShakeDetector shake;
    std::optional<v::Action> dock_action;std::optional<v::Rect> dock_rect;
    std::optional<Visual> drag_visual;std::uint64_t seam_since{};HMONITOR preview_monitor{};
    UINT_PTR seam_timer{};int seam_zone{-1};POINT seam_point{};bool seam_timer_failed{};
    bool layout_drag_restore{};
    Snapshot operation_source;
    bool restore_armed{},restore_pending{},restore_requested{},was_maximized{};
    UINT_PTR restore_timer{};WINDOWPLACEMENT restore_placement{sizeof(WINDOWPLACEMENT)};
    std::optional<InputEvent> restore_move;
};
class Application {
public:
    struct BoundKey {int id{};v::Hotkey binding;};
    struct LayoutRecord {Snapshot before,after;v::Rect normal;bool drag_restore{};};
    HINSTANCE instance{};HWND window{},overlay{};HANDLE mutex{};UINT taskbar_created{};
    v::Settings settings=v::default_settings();Catalog catalog;WindowWriter writer{catalog};InputService input;Log log;
    SettingsWindow settings_ui;fs::path config_path;std::wstring problem;std::optional<Snapshot> last_target;
    std::vector<BoundKey> bound_keys;
    std::optional<Gesture> gesture;v::GestureEpoch epoch;std::map<v::Key,std::vector<LayoutRecord>> undo;
    std::map<v::Key,Visual> visual;std::uint64_t operations{},native_attempts{};bool stopping{},diagnostic{},last_apply_revoked_only{};
    std::optional<v::End> last_termination;
    std::map<v::Key,Snapshot> last_exact_receipts;
    VisualRecovery recovery;std::map<v::Key,v::VisualOwnership> recovery_owned;
    UINT_PTR seam_timer_serial{0x50420000};
    bool overlay_outline{};v::Rect overlay_bounds;std::vector<v::Rect> overlay_members;
    std::wstring overlay_hint;UINT overlay_dpi{96};COLORREF overlay_color{},overlay_key{};
    explicit Application(HINSTANCE h):instance(h){}
    ~Application(){stop();}
    void notify(std::wstring text){problem=std::move(text);log.event("notice","\"reason\":\""+escaped(utf8(problem))+"\"");tray();}
    void tray(){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
        n.uCallbackMessage=tray_message;n.hIcon=LoadIconW(nullptr,settings.enabled&&input.healthy()?IDI_APPLICATION:IDI_WARNING);
        const auto tip=std::wstring(L"PaneBind — ")+(settings.enabled?(input.healthy()?L"运行中；Shift 起手为原生操作":L"输入故障"):L"已暂停");
        wcsncpy_s(n.szTip,tip.c_str(),_TRUNCATE);Shell_NotifyIconW(NIM_MODIFY,&n);}
    void log_status(){
        const auto windows=catalog.all(settings);log.event("status","\"enabled\":"+std::string(settings.enabled?"true":"false")+
            ",\"hook_owner_running\":"+(input.healthy()?"true":"false")+",\"gesture_active\":"+(gesture?"true":"false")+
            ",\"eligible_windows\":"+std::to_string(windows.size())+",\"operations\":"+std::to_string(operations)+",\"native_callsite_claims\":"+std::to_string(native_attempts));
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
    bool apply(const v::Plan& plan,std::vector<Snapshot> identities,std::uint64_t generation=0,bool completion=false){
        last_exact_receipts.clear();
        last_apply_revoked_only=false;
        if(!plan.accepted)return false;if(plan.placements.empty())return true;
        for(auto& s:identities)if(gesture&&gesture->expected.contains(s.key))s=gesture->expected[s.key];
        ++operations;
        if(stopping||!settings.enabled)return false;
        const bool completed_normal_up=completion&&!gesture&&epoch.generation()==generation&&epoch.last_end()==v::End::NormalUp;
        const HWND authority_source=gesture?gesture->down.prepared->source.hwnd:completed_normal_up&&!identities.empty()?identities.front().hwnd:nullptr;
        const auto receipts=writer.batch(identities,plan.placements,settings,input.operation_guard(generation,completed_normal_up,authority_source));
        bool ok=receipts.size()==plan.placements.size();
        bool any_attempt=false,only_revocation=!receipts.empty();
        for(const auto& receipt:receipts){
            any_attempt=any_attempt||receipt.native_attempted;
            only_revocation=only_revocation&&receipt.reason=="operation_revoked_before_native_attempt";
            if(receipt.native_attempted)++native_attempts;
            if(diagnostic||!receipt.accepted)log.event("placement","\"generation\":"+std::to_string(generation)+",\"operation\":"+std::to_string(operations)+
                ",\"intent_reason\":\""+escaped(plan.reason)+"\""+
                ",\"key\":"+std::to_string(receipt.before.key)+",\"native_callsite_claimed\":"+(receipt.native_attempted?"true":"false")+
                ",\"native_attempted\":"+(receipt.native_attempted?(receipt.completion_known?"true":"null"):"false")+
                ",\"call_id\":"+std::to_string(receipt.call_id)+
                ",\"native_succeeded\":"+(receipt.native_succeeded?"true":"false")+",\"error\":"+std::to_string(receipt.error)+
                ",\"native_completion_known\":"+(receipt.completion_known?"true":"false")+
                ",\"target\":"+rectangle(receipt.requested)+",\"actual\":"+(receipt.after?rectangle(receipt.after->visible):"null")+
                ",\"identity_valid\":"+(receipt.identity_valid?"true":"false")+",\"exact\":"+(receipt.geometry_exact?"true":"false")+
                ",\"reason\":\""+escaped(receipt.reason)+"\",\"native_before_qpc\":"+std::to_string(receipt.before_qpc)+
                ",\"native_after_qpc\":"+std::to_string(receipt.after_qpc)+",\"authority_after\":"+(generation&&input.allowed(generation)?"true":"false")+
                ",\"post_observed_qpc\":"+std::to_string(receipt.post_observed_qpc)+
                ",\"normal_up_completion\":"+(completion?"true":"false"));
            if(receipt.after&&receipt.identity_valid&&gesture)gesture->expected[receipt.before.key]=*receipt.after;
            if(receipt.after&&receipt.accepted&&receipt.identity_valid&&receipt.geometry_exact)
                last_exact_receipts[receipt.before.key]=*receipt.after;
            ok=ok&&receipt.accepted&&receipt.identity_valid&&receipt.geometry_exact;
        }
        last_apply_revoked_only=!ok&&!any_attempt&&only_revocation;
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
    bool mutate_visual(const Snapshot& identity,const v::VisualState& before,const v::VisualState& target,
                       bool alpha_change,bool top_change,const std::function<bool()>& native,bool cleanup=false){
        auto owned=recovery_owned.contains(identity.key)?recovery_owned.at(identity.key):v::VisualOwnership{before,before,false,false};
        v::observe_visual(owned,before);
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
        const bool permitted=cleanup||!generation||(input.allowed(generation)&&GetForegroundWindow()==identity.hwnd);
        const bool attempted=permitted;
        const bool succeeded=attempted&&native();
        const auto actual=Visual::read(identity.hwnd);bool acknowledged=false;
        if(actual){
            auto completed=planned;
            if(planned.changed_top)v::record_top(completed,*actual);
            if(planned.changed_alpha&&(!actual->layered||actual->attributes_known))v::record_alpha(completed,*actual);
            acknowledged=recovery.commit(identity,completed);
            if(acknowledged){if(v::visual_changes(completed))recovery_owned[identity.key]=completed;else recovery_owned.erase(identity.key);}
        }
        log.event("visual_mutation","\"key\":"+std::to_string(identity.key)+",\"mutation_started\":"+(attempted?"true":"false")+
            ",\"cleanup_owned_property\":"+(cleanup?"true":"false")+
            ",\"native_sequence_returned_success\":"+(succeeded?"true":"false")+",\"recovery_commit_ack\":"+(acknowledged?"true":"false")+
            ",\"helper_pid\":"+std::to_string(recovery.helper_pid())+",\"no_foreground_recovery\":true");
        return succeeded&&acknowledged;
    }
    void reconcile_recovery(const Snapshot& identity){
        const auto entry=recovery_owned.find(identity.key);if(entry==recovery_owned.end())return;
        const auto actual=Visual::read(identity.hwnd);
        if(actual){v::observe_visual(entry->second,*actual);
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
            if(v::owns_alpha(record.ownership,*observed)){
                auto target=*observed;v::copy_alpha(target,record.ownership.original);
                const bool assigned=mutate_visual(record.identity,*observed,target,true,false,[&]{return Visual::assign_alpha(h,*observed,target,valid);},true);
                const auto after=Visual::read(h);
                if(assigned&&after&&v::same_alpha(*after,record.ownership.original))record.ownership.changed_alpha=false;
                else ok=false;
            }else record.ownership.changed_alpha=false;
        }
        observed=Visual::read(h);if(!observed)return false;
        if(record.ownership.changed_top){
            if(v::owns_top(record.ownership,*observed)){
                auto target=*observed;target.topmost=record.ownership.original.topmost;
                const bool assigned=mutate_visual(record.identity,*observed,target,false,true,[&]{return valid()&&SetWindowPos(h,target.topmost?HWND_TOPMOST:HWND_NOTOPMOST,
                    0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;},true);
                const auto after=Visual::read(h);
                if(assigned&&valid()&&after&&after->topmost==record.ownership.original.topmost)record.ownership.changed_top=false;
                else ok=false;
            }else record.ownership.changed_top=false;
        }reconcile_recovery(record.identity);return ok;
    }
    std::optional<Visual> visual_begin(const Snapshot& s,bool top,BYTE alpha,bool temporary){
        const auto now=catalog.get(s.hwnd,settings);if(!now||!Catalog::same(s,*now))return {};
        const auto before=Visual::read(s.hwnd);if(!before||(before->layered&&!before->attributes_known))return {};
        Visual record;record.identity=s;record.ownership.original=record.ownership.applied=*before;
        const auto valid=[&]{const auto current=catalog.get(s.hwnd,settings);return current&&Catalog::same(s,*current);};
        if(!temporary&&top!=before->topmost){
            auto target=*before;target.topmost=top;
            const bool assigned=mutate_visual(s,*before,target,false,true,[&]{return valid()&&SetWindowPos(s.hwnd,top?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;});
            const auto after=Visual::read(s.hwnd);
            if(!assigned||!valid()||!after||after->topmost!=top)return {};
            v::record_top(record.ownership,*after);
        }
        const auto current=Visual::read(s.hwnd);
        if(!current){visual_restore(record);return {};}
        if(!v::same_alpha(*current,record.ownership.original)){
            v::copy_alpha(record.ownership.original,*current);v::copy_alpha(record.ownership.applied,*current);
        }
        if(alpha!=v::effective_alpha(*current,LWA_ALPHA)){
            auto target=*current;target.layered=true;target.attributes_known=true;target.alpha=alpha;target.flags|=LWA_ALPHA;
            const bool assigned=mutate_visual(s,*current,target,true,false,[&]{return Visual::assign_alpha(s.hwnd,*current,target,valid);});
            const auto after=Visual::read(s.hwnd);
            if(after&&valid()&&v::same_alpha(*after,target))v::record_alpha(record.ownership,*after);
            if(!assigned){
                const bool restored=visual_restore(record);
                const auto final=Visual::read(s.hwnd);
                const bool rollback_exact=restored&&valid()&&final&&
                    v::same_alpha(*final,record.ownership.original)&&final->topmost==record.ownership.original.topmost;
                log.event("visual_failure","\"reason\":\"temporary_or_shake_alpha_not_exact\",\"owned_rollback_exact\":"+std::string(rollback_exact?"true":"false"));
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
        v::observe_visual(record.ownership,*before);
        const auto valid=[&]{const auto current=catalog.get(source.hwnd,settings);return current&&Catalog::same(record.identity,*current);};
        bool success{};
        if(action==v::Action::ToggleTopmost){
            const bool requested=!before->topmost;
            auto target=*before;target.topmost=requested;
            const bool assigned=mutate_visual(source,*before,target,false,true,[&]{return valid()&&SetWindowPos(source.hwnd,requested?HWND_TOPMOST:HWND_NOTOPMOST,
                0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)!=FALSE;});
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
            success=mutate_visual(source,*before,target,!v::same_alpha(*before,target),false,[&]{return Visual::assign_alpha(source.hwnd,*before,target,valid);});
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
        input.pause(true);finish(v::End::Paused);
        if(!paused){input.stop();if(!input.start(window)){notify(L"无法恢复输入资源，保持暂停。");settings.enabled=false;tray();return;}}
        input.pause(paused);auto next=settings;next.enabled=!paused;if(!save(next))settings.enabled=!paused;
        if(paused){for(auto& [key,value]:visual){(void)key;visual_restore(value);}visual.clear();}
        tray();if(!paused)prepare();log_status();
    }
    void show_preview(std::optional<v::Rect> rect){
        overlay_outline=false;overlay_members.clear();overlay_hint.clear();overlay_bounds={};
        if(!overlay)return;
        const auto& config=gesture?gesture->config:settings;
        if(!rect||!config.preview){ShowWindow(overlay,SW_HIDE);return;}
        overlay_color=static_cast<COLORREF>(config.preview_color);
        if(!SetLayeredWindowAttributes(overlay,0,static_cast<BYTE>(config.preview_alpha),LWA_ALPHA)){ShowWindow(overlay,SW_HIDE);return;}
        const auto& r=*rect;
        if(!SetWindowPos(overlay,HWND_TOPMOST,static_cast<int>(r.left()),static_cast<int>(r.top()),static_cast<int>(r.width()),static_cast<int>(r.height()),SWP_NOACTIVATE|SWP_SHOWWINDOW)){
            ShowWindow(overlay,SW_HIDE);return;}
        InvalidateRect(overlay,nullptr,FALSE);
    }
    void show_feedback(const std::vector<Snapshot>& actual,std::wstring hint){
        if(!overlay||!gesture||!gesture->config.feedback||!input.allowed(gesture->down.generation)||actual.empty()){
            show_preview({});return;
        }
        const auto displays=catalog.displays();if(displays.empty()){show_preview({});return;}
        constexpr auto lo=std::numeric_limits<std::int32_t>::min(),hi=std::numeric_limits<std::int32_t>::max();
        const auto native=[](const v::Rect& r){
            const auto width=g::checked_difference(r.right(),r.left()),height=g::checked_difference(r.bottom(),r.top());
            if(!width||!height||*width<=0||*height<=0||*width>hi||*height>hi)return false;
            for(const auto edge:{r.left(),r.top(),r.right(),r.bottom()})if(edge<lo||edge>hi)return false;
            return true;
        };
        auto desktop=displays.front().bounds;
        for(const auto& display:displays){
            if(!display.dpi){show_preview({});return;}
            desktop={std::min(desktop.left(),display.bounds.left()),std::min(desktop.top(),display.bounds.top()),
                std::max(desktop.right(),display.bounds.right()),std::max(desktop.bottom(),display.bounds.bottom())};
        }
        std::vector<v::Rect> clipped;
        for(const auto& snapshot:actual){
            if(!snapshot.dpi||!native(snapshot.visible)){show_preview({});return;}
            const auto visible=g::intersection(snapshot.visible,desktop);
            if(!visible){show_preview({});return;}clipped.push_back(*visible);
        }
        auto bounds=clipped.front();
        for(const auto& member:clipped)bounds={std::min(bounds.left(),member.left()),std::min(bounds.top(),member.top()),
            std::max(bounds.right(),member.right()),std::max(bounds.bottom(),member.bottom())};
        const auto caption=hint.empty()?0:dip(25,actual.front().dpi);
        const auto top=g::checked_add(bounds.top(),-caption);
        if(!top){show_preview({});return;}
        bounds={bounds.left(),std::max(desktop.top(),*top),bounds.right(),bounds.bottom()};
        if(!native(bounds)||!input.allowed(gesture->down.generation)){show_preview({});return;}
        overlay_outline=true;overlay_bounds=bounds;overlay_members=std::move(clipped);overlay_hint=std::move(hint);
        overlay_dpi=actual.front().dpi;overlay_color=static_cast<COLORREF>(gesture->config.preview_color);
        for(const auto candidate:{RGB(1,2,3),RGB(4,5,6),RGB(7,8,9),RGB(10,11,12)}){
            if(candidate!=overlay_color&&candidate!=GetSysColor(COLOR_INFOBK)&&candidate!=GetSysColor(COLOR_INFOTEXT)){overlay_key=candidate;break;}
        }
        // The background is truly color-keyed out. Unlike a filled rectangle,
        // group feedback leaves the applications between the outlines visible.
        if(!SetLayeredWindowAttributes(overlay,overlay_key,255,LWA_COLORKEY|LWA_ALPHA)||
            !SetWindowPos(overlay,HWND_TOPMOST,static_cast<int>(bounds.left()),static_cast<int>(bounds.top()),
                static_cast<int>(bounds.width()),static_cast<int>(bounds.height()),SWP_NOACTIVATE|SWP_SHOWWINDOW)){
            show_preview({});return;}
        InvalidateRect(overlay,nullptr,FALSE);
    }
    void command(v::Action action,std::optional<Snapshot> target={});
    void prepare();
    void begin(const InputEvent&);
    void motion(const InputEvent&);
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
    bool dock_preview(POINT,std::uint64_t,bool arm=true);
    void seam_timeout(UINT_PTR);
    void request_native_restore(const InputEvent&);
    void restore_progress();
    void restore_timeout(UINT_PTR);
    void cancel_restore_wait();
    void finish(v::End);
    void consume();
    void show_settings();
    void register_keys();
    std::optional<std::vector<BoundKey>> prepare_keys(const v::Settings&);
    void discard_new_keys(const std::vector<BoundKey>&);
    void commit_keys(std::vector<BoundKey>);
    void export_diagnostic();
    void menu();
    void stop();
    void log_delayed_completions(){
        for(const auto& r:writer.take_delayed_completions(settings))log.event("native_completion",
            "\"call_id\":"+std::to_string(r.call_id)+",\"key\":"+std::to_string(r.before.key)+
            ",\"native_attempted\":"+(r.native_attempted?"true":"false")+",\"native_succeeded\":"+(r.native_succeeded?"true":"false")+
            ",\"native_completion_known\":"+(r.completion_known?"true":"false")+",\"error\":"+std::to_string(r.error)+
            ",\"target\":"+rectangle(r.requested)+",\"delayed_actual\":"+(r.after?rectangle(r.after->visible):"null")+
            ",\"identity_valid\":"+(r.identity_valid?"true":"false")+",\"geometry_exact_at_delayed_observation\":"+(r.geometry_exact?"true":"false")+
            ",\"native_before_qpc\":"+std::to_string(r.before_qpc)+",\"native_after_qpc\":"+std::to_string(r.after_qpc)+
            ",\"post_observed_qpc\":"+std::to_string(r.post_observed_qpc)+",\"new_write_authority_granted\":false");
    }
};
Application* application{};

void Application::command(v::Action action,std::optional<Snapshot> target){
    if(action==v::Action::Pause){set_paused(settings.enabled);return;}
    if(action==v::Action::Settings){show_settings();return;}if(action==v::Action::Exit){DestroyWindow(window);return;}
    if(!settings.enabled){notify(L"增强已暂停；请先恢复后再执行窗口命令。");return;}
    if(gesture)finish(v::End::Paused);
    // A hotkey may be the first action after startup, without mouse motion.
    // Tray commands may fall back to the last admitted app when our menu owns
    // foreground; never accept an unrelated stale HWND without recapture.
    const auto foreground=target?std::optional<Snapshot>{}:catalog.get(GetForegroundWindow(),settings);
    const auto source=target?target:foreground?foreground:last_target;if(!source){notify(L"没有可操作的普通前台窗口。");return;}
    auto now=catalog.get(source->hwnd,settings,nullptr,action==v::Action::Restore);
    if(!now||!Catalog::same(*source,*now)){notify(L"目标窗口资格已经失效。");return;}
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
    case v::Action::Lower:SetWindowPos(s.hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);geometry=false;break;
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
        const auto index=(static_cast<std::size_t>(it-displays.begin())+1)%displays.size();const auto& d=displays[index];
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
    case v::Action::StretchHorizontal:case v::Action::StretchVertical:case v::Action::StretchAll:{if(!settings.stretch||!(s.feature_mask&8u))return;v::Edges e{};
        e.left=action==v::Action::StretchLeft||action==v::Action::StretchHorizontal||action==v::Action::StretchAll;
        e.right=action==v::Action::StretchRight||action==v::Action::StretchHorizontal||action==v::Action::StretchAll;
        e.top=action==v::Action::StretchTop||action==v::Action::StretchVertical||action==v::Action::StretchAll;
        e.bottom=action==v::Action::StretchBottom||action==v::Action::StretchVertical||action==v::Action::StretchAll;
        desired=v::stretch(s.visible,s.work,e,false,others,settings.smart_stretch);break;}
    default:if(action>=v::Action::DockLeft&&action<=v::Action::DockBottomRight&&settings.docking&&(s.feature_mask&4u))
        desired=v::smart_dock(s.work,action,others,settings.smart_docking);else return;
    }
    if(geometry){if(s.maximized){notify(L"请先恢复窗口再执行该几何操作。");return;}
        if(apply({true,"command",{{s.key,desired}}},{s})){
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
    input.prepared_point();if(gesture||!settings.enabled)return;
    const auto point=input.latest();HWND hit=WindowFromPoint(point),root=hit?GetAncestor(hit,GA_ROOT):nullptr;
    const auto snapshot=root?catalog.get(root,settings):std::nullopt;
    if(!snapshot||GetForegroundWindow()!=root){input.prepare({});return;}
    if(!gesture)owned_layout(*snapshot);
    GUITHREADINFO gui{sizeof(gui)};
    if(!GetGUIThreadInfo(snapshot->tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner){input.prepare({});return;}
    last_target=snapshot;DWORD_PTR value{};int region=-1;
    const auto lp=MAKELPARAM(static_cast<short>(point.x),static_cast<short>(point.y));DWORD pid{};GetWindowThreadProcessId(hit,&pid);
    if(pid==snapshot->pid&&SendMessageTimeoutW(hit,WM_NCHITTEST,0,lp,SMTO_ABORTIFHUNG|SMTO_BLOCK,30,&value)){
        switch(static_cast<LRESULT>(value)){case HTCAPTION:region=0;break;case HTLEFT:region=1;break;case HTRIGHT:region=2;break;
        case HTTOP:region=3;break;case HTBOTTOM:region=4;break;case HTTOPLEFT:region=5;break;case HTTOPRIGHT:region=6;break;
        case HTBOTTOMLEFT:region=7;break;case HTBOTTOMRIGHT:region=8;break;default:break;}
    }
    if(region>0&&(!snapshot->can_resize||snapshot->maximized))region=-1;if(region<0){input.prepare({});return;}
    auto ready=std::make_shared<Prepared>();ready->source=*snapshot;ready->point=point;ready->stamp=GetTickCount();ready->region=region;ready->config_generation=settings.generation;
    prepare_mappings(*ready,settings);
    input.prepare(std::move(ready));
}
void Application::begin(const InputEvent& e){
    if(gesture)finish(v::End::QueueFailure);if(!e.prepared||!epoch.begin(e.generation))return;
    const auto& p=*e.prepared;const auto fresh=catalog.get(p.source.hwnd,settings);
    if(!fresh||!Catalog::same(p.source,*fresh)||fresh->visible!=p.source.visible||p.config_generation!=settings.generation){
        input.revoke();epoch.finish(e.generation,v::End::SourceLost);return;}
    Gesture active;active.down=e;active.config=settings;active.original=fresh->visible;active.previous=fresh->visible;active.edges=edges(p.region);
    active.operation_source=*fresh;active.was_maximized=fresh->maximized;active.restore_armed=fresh->maximized;
    active.ctrl=(e.modifiers&2u)&&settings.glue&&(fresh->feature_mask&2u);active.last_tick=e.tick;active.members=catalog.all(settings);
    if(active.ctrl)active.members.erase(std::remove_if(active.members.begin(),active.members.end(),[](const auto& member){return !(member.feature_mask&2u);}),active.members.end());
    auto list=items(active.members);const auto selected=active.ctrl&&!fresh->maximized?v::connected(list,fresh->key,dip(settings.adjacency_dip,fresh->dpi)):std::vector<v::Key>{fresh->key};
    active.members.erase(std::remove_if(active.members.begin(),active.members.end(),[&](const auto& s){
        return std::find(selected.begin(),selected.end(),s.key)==selected.end()||(active.ctrl&&!(s.feature_mask&2u));}),active.members.end());
    if(active.members.empty()){input.revoke();epoch.finish(e.generation,v::End::SourceLost);return;}
    // Desktop enumeration is a read-only topology snapshot. Only the selected
    // operation participants query native tracking limits; no stale/default
    // bounds may become the frozen Ctrl resize contract.
    for(auto& member:active.members){
        const auto actual=catalog.get(member.hwnd,settings);
        if(!actual||!Catalog::same(member,*actual)||actual->visible!=member.visible||
            actual->positioning!=member.positioning||actual->dpi!=member.dpi||
            actual->monitor!=member.monitor||actual->work!=member.work||
            actual->maximized!=member.maximized||actual->minimized!=member.minimized||
            actual->can_resize!=member.can_resize||actual->feature_mask!=member.feature_mask){
            input.revoke();epoch.finish(e.generation,v::End::SourceLost);return;
        }
        if(member.key==fresh->key&&(actual->visible!=fresh->visible||actual->positioning!=fresh->positioning||
            actual->dpi!=fresh->dpi||actual->monitor!=fresh->monitor||actual->work!=fresh->work||
            actual->maximized!=fresh->maximized||actual->minimized!=fresh->minimized)){
            input.revoke();epoch.finish(e.generation,v::End::SourceLost);return;
        }
        member=*actual;
    }
    if(std::none_of(active.members.begin(),active.members.end(),[&](const auto& member){return member.key==fresh->key;})){
        input.revoke();epoch.finish(e.generation,v::End::SourceLost);return;
    }
    active.targets=catalog.all(settings);active.targets.erase(std::remove_if(active.targets.begin(),active.targets.end(),[&](const auto& s){
        return std::find(selected.begin(),selected.end(),s.key)!=selected.end()||!(s.feature_mask&1u);}),active.targets.end());
    for(const auto& s:active.members)active.expected[s.key]=s;
    active.scope_original=active.original;
    if(active.ctrl&&!is_resize(active.edges)){
        for(const auto& s:active.members)active.scope_original={std::min(active.scope_original.left(),s.visible.left()),
            std::min(active.scope_original.top(),s.visible.top()),std::max(active.scope_original.right(),s.visible.right()),
            std::max(active.scope_original.bottom(),s.visible.bottom())};
    }
    active.previous=active.scope_original;gesture=std::move(active);
    if(!fresh->maximized&&!gesture->ctrl&&!is_resize(gesture->edges)){
        if(const auto history=owned_layout(*fresh);history&&history->back().drag_restore){
            const auto anchor=v::restored_move_anchor(fresh->visible,history->back().normal,{e.point.x,e.point.y});
            if(anchor){gesture->layout_drag_restore=true;gesture->original=*anchor;gesture->scope_original=*anchor;gesture->previous=*anchor;}
        }
    }
    if(!fresh->maximized&&settings.drag_transparency&&(fresh->feature_mask&32u))gesture->drag_visual=visual_begin(*fresh,false,static_cast<BYTE>(settings.drag_alpha),true);
    log.event("gesture_start","\"generation\":"+std::to_string(e.generation)+",\"key\":"+std::to_string(fresh->key)+
        ",\"resize\":"+std::string(is_resize(gesture->edges)?"true":"false")+",\"ctrl\":"+(gesture->ctrl?"true":"false")+",\"original\":"+rectangle(fresh->visible));
    if(e.clicks==2&&p.region>0&&settings.stretch&&(fresh->feature_mask&8u)){
        const auto desired=v::stretch(fresh->visible,fresh->work,gesture->edges,(e.modifiers&4u)!=0,items(gesture->targets),settings.smart_stretch);
        if(input.allowed(e.generation)&&apply({true,"double_click_stretch",{{fresh->key,desired}}},{*fresh},e.generation))remember_receipt(*fresh,true);
        input.revoke();finish(v::End::Paused);
    }
}
void Application::cancel_seam_timer(bool reset){
    if(!gesture)return;
    auto& state=*gesture;
    if(state.seam_timer){KillTimer(window,state.seam_timer);state.seam_timer=0;}
    if(reset){state.seam_since=0;state.preview_monitor=nullptr;state.seam_zone=-1;state.seam_timer_failed=false;}
}
bool Application::dock_preview(POINT point,std::uint64_t tick,bool arm){
    if(!gesture)return false;
    auto& state=*gesture;const auto& config=state.config;const auto& source=state.operation_source;
    state.dock_action.reset();state.dock_rect.reset();
    if(!input.allowed(state.down.generation)){cancel_seam_timer();return false;}
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
    if(!input.allowed(state.down.generation))return false;
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
    GUITHREADINFO gui{sizeof(gui)};
    if(!GetGUIThreadInfo(source.tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner){finish(v::End::FocusLost);return;}
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
    GUITHREADINFO gui{sizeof(gui)};WINDOWPLACEMENT placement{sizeof(placement)};
    if(!fresh||!Catalog::same(original,*fresh)||!fresh->maximized||fresh->visible!=original.visible||
        fresh->positioning!=original.positioning||GetForegroundWindow()!=original.hwnd||
        !GetGUIThreadInfo(original.tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner||
        !GetWindowPlacement(original.hwnd,&placement)||placement.showCmd!=SW_SHOWMAXIMIZED||
        placement.rcNormalPosition.right<=placement.rcNormalPosition.left||placement.rcNormalPosition.bottom<=placement.rcNormalPosition.top){
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
    if(fresh->maximized||fresh->minimized)return;
    GUITHREADINFO gui{sizeof(gui)};
    if(!GetGUIThreadInfo(fresh->tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner){finish(v::End::FocusLost);return;}
    auto normal=fresh->visible;
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
void Application::motion(const InputEvent& e){
    if(!gesture||!epoch.motion(e.generation)||!input.allowed(e.generation))return;
    if(gesture->restore_armed||gesture->restore_pending){gesture->restore_move=e;if(gesture->restore_armed)request_native_restore(e);return;}
    auto& state=*gesture;const auto& config=state.config;const auto source=state.operation_source;const auto fresh=catalog.get(source.hwnd,settings);
    if(!fresh||!Catalog::same(source,*fresh)||GetForegroundWindow()!=source.hwnd){finish(v::End::SourceLost);return;}
    GUITHREADINFO gui{sizeof(gui)};if(!GetGUIThreadInfo(source.tid,&gui)||gui.hwndCapture||gui.hwndMoveSize||gui.hwndMenuOwner){finish(v::End::FocusLost);return;}
    refresh_targets();
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
    for(const auto& s:state.targets)targets.push_back({core::model::WindowId(std::to_string(s.key)),s.visible,false});
    const auto monitor=MonitorFromPoint(e.point,MONITOR_DEFAULTTONEAREST);const auto displays=catalog.displays();
    const auto display=std::find_if(displays.begin(),displays.end(),[&](const auto& d){return d.monitor==monitor;});
    if(display!=displays.end()&&targets.size()<256)targets.push_back({core::model::WindowId("work"),display->work,true});
    m::MagnetInput request;request.initial=state.scope_original;request.raw=raw;request.targets=targets;
    request.options.attraction_distance=dip(config.attraction_dip,source.dpi);request.options.max_targets=256;
    request.preferred_x=state.x;request.preferred_y=state.y;
    request.motion=m::MotionSample{state.previous,state.last_tick,e.tick,1000,dip(config.speed_dip,source.dpi)};
    request.interaction=is_resize(state.edges)?m::Interaction::Resize:m::Interaction::Move;
    request.edges={state.edges.left,state.edges.top,state.edges.right,state.edges.bottom};request.latched_resize=is_resize(state.edges);
    const bool magnet_on=(is_resize(state.edges)?config.magnet_resize:config.magnet_move)&&(source.feature_mask&1u);
    if(magnet_on&&!(state.ctrl&&is_resize(state.edges))){
        const auto result=m::MagnetConstraintSolver::solve(request);
        if(result.proposal){const auto& proposal=*result.proposal;
            if(state.ctrl){for(auto& p:plan.placements)p.visible=core::movement::translate_rect(p.visible,{proposal.move_delta.x,proposal.move_delta.y});}
            else if(proposal.corrected.width()>=source.limits.min_width&&proposal.corrected.height()>=source.limits.min_height&&
                proposal.corrected.width()<=source.limits.max_width&&proposal.corrected.height()<=source.limits.max_height)
                src->visible=proposal.corrected;
            const auto latch=[&](const auto& constraint)->std::optional<m::MagnetAxisPreference>{
                if(!constraint)return {};return m::MagnetAxisPreference{constraint->target,constraint->kind,constraint->moving_edge,constraint->target_edge,dip(config.release_dip,source.dpi)};};
            state.x=latch(proposal.selected_x);state.y=latch(proposal.selected_y);
        }else{state.x.reset();state.y.reset();}
    }
    state.previous=raw;state.last_tick=e.tick;const bool dock_shown=dock_preview(e.point,e.tick);
    if(!dock_shown)show_preview({});
    if(!apply(plan,state.members,e.generation)){
        // Only the matching actual UP event may finish NormalUp. A concurrent
        // hook-side revocation is not itself evidence of which terminator won.
        if(!last_apply_revoked_only){state.dock_action.reset();state.dock_rect.reset();finish(v::End::SourceLost);}return;}
    if(config.feedback&&!dock_shown){
        std::vector<Snapshot> actual;
        if(state.ctrl){for(const auto& member:state.members){
            const auto found=state.expected.find(member.key);if(found==state.expected.end()){actual.clear();break;}
            actual.push_back(found->second);
        }}else if(state.x||state.y){
            if(const auto found=state.expected.find(source.key);found!=state.expected.end())actual.push_back(found->second);
        }
        if(!actual.empty())show_feedback(actual,state.ctrl?
            (is_resize(state.edges)?L"Ctrl 联动缩放：":L"Ctrl 联动移动：")+std::to_wstring(actual.size())+L" 个临时邻接窗口":L"");
    }
    if(input.allowed(e.generation)&&config.shake&&!is_resize(state.edges)&&(source.feature_mask&32u)&&state.shake.sample({e.point.x,e.point.y},e.tick,
        static_cast<int>(dip(config.shake_amplitude_dip,source.dpi)),static_cast<unsigned>(config.shake_reversals),static_cast<std::uint64_t>(config.shake_window_ms))){
        if(state.drag_visual){visual_restore(*state.drag_visual);state.drag_visual.reset();}
        if(visual.contains(source.key)){visual_restore(visual[source.key]);visual.erase(source.key);}
        else if(input.allowed(e.generation)){const auto record=visual_begin(*fresh,true,static_cast<BYTE>(config.shake_alpha),false);if(record)visual[source.key]=*record;}
    }
}
void Application::finish(v::End reason){
    input.revoke(reason);cancel_seam_timer();cancel_restore_wait();show_preview({});if(!gesture)return;auto state=std::move(*gesture);gesture.reset();epoch.finish(state.down.generation,reason);last_termination=reason;
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
            else if(apply({true,"normal_up_dock",{{current->key,*state.dock_rect}}},{*current},state.down.generation,true))remember_receipt(*current,true);
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
    if(state.drag_visual)visual_restore(*state.drag_visual);
    input.finish_completion(state.down.generation);
    log.event("gesture_end","\"generation\":"+std::to_string(state.down.generation)+",\"end\":"+std::to_string(static_cast<int>(reason))+
        ",\"normal_up\":"+std::string(reason==v::End::NormalUp?"true":"false")+",\"pending_retired\":true");
}
void Application::consume(){InputEvent e;std::optional<InputEvent> latest_move;
    const auto flush=[&]{if(latest_move){motion(*latest_move);latest_move.reset();}};
    while(input.pop(e)){
    if(e.kind==InputKind::Move){latest_move=e;continue;}flush();
    if(e.kind==InputKind::Down)begin(e);
    else if(e.kind==InputKind::Mapped&&e.prepared){
        const auto action=e.prepared->region==0&&e.clicks==2&&e.action==v::Action::Maximize&&IsZoomed(e.prepared->source.hwnd)?v::Action::Restore:e.action;
        command(action,e.prepared->source);
    }
    else if(e.kind==InputKind::Up){if(gesture&&gesture->down.generation==e.generation)finish(e.end);}
    else if(e.kind==InputKind::Abort){if(gesture&&gesture->down.generation==e.generation)finish(e.end);}
    }flush();
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
    if(gesture)finish(v::End::Paused);
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
            input.stop();if(!input.start(window)){settings.enabled=false;input.pause(true);notify(L"配置已保存，但输入资源恢复失败；当前保持暂停。");tray();return true;}
        }
        if(withdraw||value.enabled!=previous.enabled)input.pause(!settings.enabled);
        tray();prepare();return true;
    },[this]{export_diagnostic();},[this]()->std::optional<v::Rule>{
        if(!last_target)return {};
        const auto fresh=catalog.get(last_target->hwnd,settings);
        if(!fresh||!Catalog::same(*last_target,*fresh))return {};
        return v::Rule{utf8(fresh->exe),utf8(fresh->window_class),true,0xffffffffu};
    });
}
void Application::export_diagnostic(){
    const auto name=config_path.parent_path()/(L"diagnostic-"+std::to_wstring(GetTickCount64())+L".txt");
    if(fs::exists(name)){notify(L"诊断目标已存在，未覆盖。");return;}
    auto redacted=settings;for(auto& rule:redacted.rules)rule.exe=utf8(fs::path(wide(rule.exe)).filename().wstring());
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
    if(!atomic_save(name,text.str())){notify(L"诊断写入失败；未声明导出成功。");return;}
    notify(L"诊断已导出："+name.filename().wstring());
}
void Application::menu(){POINT pt{};GetCursorPos(&pt);HMENU popup=CreatePopupMenu();
    AppendMenuW(popup,MF_STRING,menu_pause,settings.enabled?L"暂停增强 (&P)":L"恢复增强 (&P)");
    AppendMenuW(popup,MF_STRING,menu_settings,L"设置 (&S)");AppendMenuW(popup,MF_STRING,menu_exclude,L"排除上一个应用");
    HMENU commands=CreatePopupMenu();for(int a=static_cast<int>(v::Action::Minimize);a<=static_cast<int>(v::Action::Undo);++a)
        AppendMenuW(commands,MF_STRING,600+static_cast<UINT>(a),action_label(static_cast<v::Action>(a)).c_str());
    AppendMenuW(popup,MF_POPUP,reinterpret_cast<UINT_PTR>(commands),L"窗口操作");AppendMenuW(popup,MF_STRING,menu_help,L"帮助");
    AppendMenuW(popup,MF_STRING,menu_diagnostic,L"导出诊断");AppendMenuW(popup,MF_SEPARATOR,0,nullptr);AppendMenuW(popup,MF_STRING,menu_exit,L"退出 (&X)");
    // Activating our own user-requested tray menu is not source activation or
    // foreground recovery. No third-party window is activated by the product.
    SetForegroundWindow(window);TrackPopupMenu(popup,TPM_RIGHTBUTTON,pt.x,pt.y,0,window,nullptr);PostMessageW(window,WM_NULL,0,0);DestroyMenu(popup);
}
void Application::stop(){if(stopping)return;stopping=true;input.revoke();finish(v::End::Exit);input.stop();settings_ui.close();
    const bool native_writer_exited=writer.stop();
    const bool issued_completion_unknown=writer.has_unconfirmed_native_call();
    log_delayed_completions();
    for(auto& [key,record]:visual){(void)key;visual_restore(record);}visual.clear();
    const bool recovery_stopped=recovery.stop();
    for(const auto& key:bound_keys)UnregisterHotKey(window,key.id);bound_keys.clear();
    NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=window;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);
    if(overlay)DestroyWindow(overlay);overlay=nullptr;WTSUnRegisterSessionNotification(window);
    log.event("shutdown","\"own_hooks_stopped\":true,\"own_overlay_destroyed\":true,\"native_worker_exited\":"+
        std::string(native_writer_exited?"true":"false")+",\"issued_native_completion_unknown\":"+(issued_completion_unknown?"true":"false")+
        ",\"visual_recovery_stopped\":"+(recovery_stopped?"true":"false"));
}
LRESULT CALLBACK overlay_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_NCHITTEST)return HTTRANSPARENT;if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
    if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(w,&paint);RECT r{};GetClientRect(w,&r);
        const auto* app=application;
        HBRUSH brush=CreateSolidBrush(app?(app->overlay_outline?app->overlay_key:app->overlay_color):RGB(0,120,215));
        FillRect(dc,&r,brush);DeleteObject(brush);
        if(app&&app->overlay_outline){
            const int thickness=std::max(1,MulDiv(2,static_cast<int>(app->overlay_dpi),96));
            HBRUSH line=CreateSolidBrush(app->overlay_color);
            for(const auto& member:app->overlay_members){
                RECT border{static_cast<LONG>(member.left()-app->overlay_bounds.left()),static_cast<LONG>(member.top()-app->overlay_bounds.top()),
                    static_cast<LONG>(member.right()-app->overlay_bounds.left()),static_cast<LONG>(member.bottom()-app->overlay_bounds.top())};
                for(int stroke=0;stroke<thickness&&border.right>border.left&&border.bottom>border.top;++stroke){FrameRect(dc,&border,line);InflateRect(&border,-1,-1);}
            }
            DeleteObject(line);
            if(!app->overlay_hint.empty()){
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
        }
        EndPaint(w,&paint);return 0;}
    return DefWindowProcW(w,message,wp,lp);
}
void create_tray(Application& app){NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=app.window;n.uID=1;
    n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;n.uCallbackMessage=tray_message;n.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wcscpy_s(n.szTip,L"PaneBind");Shell_NotifyIconW(NIM_ADD,&n);n.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&n);app.tray();}
LRESULT CALLBACK main_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    auto* a=application;if(!a)return DefWindowProcW(w,message,wp,lp);
    try{
        if(message==a->taskbar_created){create_tray(*a);return 0;}
        switch(message){
        case prepare_message:a->prepare();return 0;
        case input_message:a->consume();return 0;
        case native_completion_message:a->log_delayed_completions();return 0;
        case WM_TIMER:if(a->gesture&&a->gesture->restore_timer==static_cast<UINT_PTR>(wp))a->restore_timeout(static_cast<UINT_PTR>(wp));
            else a->seam_timeout(static_cast<UINT_PTR>(wp));return 0;
        case catalog_message:{
            if(wp==2){a->settings.enabled=false;a->finish(v::End::Paused);a->tray();
                a->log.event("emergency_hotkey","\"native_message\":\"WM_HOTKEY\",\"registration_id\":910,\"received_count\":"+
                    std::to_string(a->input.emergency_hotkey_messages()));}
            else if(wp==1){a->input.revoke();a->finish(v::End::QueueFailure);a->notify(L"输入队列故障，已停止增强；请暂停后恢复。");}
            if(a->gesture&&a->gesture->restore_pending)a->restore_progress();
            if(!a->gesture){const auto front=GetForegroundWindow();if(auto target=a->catalog.get(front,a->settings))a->last_target=*target;a->prepare();}
            return 0;}
        case tray_message:
            if(LOWORD(lp)==WM_CONTEXTMENU||LOWORD(lp)==WM_RBUTTONUP)a->menu();
            else if(LOWORD(lp)==NIN_SELECT||LOWORD(lp)==WM_LBUTTONDBLCLK)a->show_settings();
            return 0;
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
            if(data&&data->dwData==0x50425631&&data->cbData>=1&&data->cbData<80&&data->lpData){
                const auto name=std::string_view(static_cast<const char*>(data->lpData),data->cbData-1);
                const auto command=v::action_from_name(name);if(command){a->command(*command);return TRUE;}}
            return FALSE;}
        case WM_DISPLAYCHANGE:case WM_DPICHANGED:case WM_SETTINGCHANGE:
            a->input.revoke();a->finish(v::End::SessionLost);a->prepare();return 0;
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
    if(const auto helper=VisualRecovery::helper_entry(argv))return *helper;
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
    if(existing){const auto target=FindWindowW(app_class,L"PaneBind");
        if(target){if(argv.size()==2&&argv[0]==L"--command"){
                const auto name=utf8(argv[1]);COPYDATASTRUCT data{0x50425631,static_cast<DWORD>(name.size()+1),const_cast<char*>(name.c_str())};DWORD_PTR result{};
                SendMessageTimeoutW(target,WM_COPYDATA,0,reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,1000,&result);
            }else PostMessageW(target,external_message,(!argv.empty()&&argv[0]==L"--quit")?1:(!argv.empty()&&argv[0]==L"--pause")?2:
                (!argv.empty()&&argv[0]==L"--status")?3:0,0);}
        CloseHandle(mutex);CoUninitialize();return target?0:2;
    }
    if(!argv.empty()&&(argv[0]==L"--quit"||argv[0]==L"--pause"||argv[0]==L"--status"||argv[0]==L"--command")){
        CloseHandle(mutex);CoUninitialize();return 2;}
    int result=1;
    try{
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
        Application app(instance);application=&app;app.mutex=mutex;app.initialize_paths();
        app.diagnostic=std::find(argv.begin(),argv.end(),L"--diagnostic")!=argv.end();
        WNDCLASSW wc{};wc.lpfnWndProc=main_proc;wc.hInstance=instance;wc.lpszClassName=app_class;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);
        WNDCLASSW ow{};ow.lpfnWndProc=overlay_proc;ow.hInstance=instance;ow.lpszClassName=L"PaneBind.V1.Preview";RegisterClassW(&ow);
        app.window=CreateWindowExW(WS_EX_TOOLWINDOW,app_class,L"PaneBind",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
        if(!app.window)throw std::runtime_error("owner_window_failed");
        app.writer.notify_on_completion(app.window);
        app.overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,ow.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,instance,nullptr);
        app.taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");create_tray(app);WTSRegisterSessionNotification(app.window,NOTIFY_FOR_THIS_SESSION);
        if(!app.input.start(app.window)){app.settings.enabled=false;app.notify(L"输入资源或紧急热键初始化失败，增强保持暂停。");}
        app.input.pause(!app.settings.enabled);app.register_keys();app.prepare();
        app.log.event("startup","\"entry\":\"production_gui\",\"integrity\":\"medium\",\"elevated\":false,\"initiator\":\"not_collected\"");
        if(!app.problem.empty())app.notify(app.problem);
        MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){if(app.settings_ui.translate(msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}
        app.stop();application=nullptr;UnregisterClassW(ow.lpszClassName,instance);UnregisterClassW(app_class,instance);result=0;
    }catch(const std::exception& e){application=nullptr;MessageBoxW(nullptr,wide(e.what()).c_str(),L"PaneBind 启动失败",MB_OK|MB_ICONERROR);}
    CloseHandle(mutex);CoUninitialize();return result;
}
} // namespace panebind::platform::windows::product
