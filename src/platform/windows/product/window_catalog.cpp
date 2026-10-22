#include "platform/windows/product/window_catalog.h"

#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <shobjidl.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>
#include <utility>
#ifdef PANEBIND_V1_CATALOG_RULE_TEST
#include <iostream>
#endif

namespace panebind::platform::windows::product {
namespace {

using core::v1::Rect;

std::optional<bool> native_arranged(HWND window) noexcept {
    // The documented API is not declared by all supported Windows SDKs.
    // Resolve only the named export of the already-loaded system module;
    // absence is not evidence that a window is in the normal state.
    using Query = BOOL (WINAPI*)(HWND);
    static const auto query = reinterpret_cast<Query>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "IsWindowArranged"));
    if (!query) return std::nullopt;
    return query(window) != FALSE;
}

class Handle final {
public:
    explicit Handle(HANDLE value = nullptr) noexcept : value_(value) {}
    ~Handle() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    [[nodiscard]] HANDLE get() const noexcept { return value_; }
private:
    HANDLE value_{};
};

struct Security {
    std::vector<unsigned char> sid;
    DWORD session{}, integrity{};
    bool elevated{}, ui_access{}, app_container{}, valid{};
};

Security security(HANDLE process) {
    Security result;
    HANDLE raw_token{};
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw_token)) return result;
    const Handle token(raw_token);
    DWORD required{};
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &required);
    if (!required || required > 65536) return result;
    std::vector<unsigned char> user(required);
    if (!GetTokenInformation(token.get(), TokenUser, user.data(), required, &required)) return result;
    const auto user_sid = reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid;
    if (!IsValidSid(user_sid)) return result;
    result.sid.resize(GetLengthSid(user_sid));
    if (!CopySid(static_cast<DWORD>(result.sid.size()), result.sid.data(), user_sid)) return result;

    GetTokenInformation(token.get(), TokenIntegrityLevel, nullptr, 0, &required);
    if (!required || required > 65536) return result;
    std::vector<unsigned char> label(required);
    if (!GetTokenInformation(token.get(), TokenIntegrityLevel, label.data(), required, &required)) return result;
    const auto label_sid = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(label.data())->Label.Sid;
    if (!IsValidSid(label_sid) || *GetSidSubAuthorityCount(label_sid) == 0) return result;
    result.integrity = *GetSidSubAuthority(label_sid, *GetSidSubAuthorityCount(label_sid) - 1U);
    TOKEN_ELEVATION elevation{};
    DWORD ui_access{}, app_container{};
    if (!GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &required) ||
        !GetTokenInformation(token.get(), TokenSessionId, &result.session, sizeof(result.session), &required) ||
        !GetTokenInformation(token.get(), TokenUIAccess, &ui_access, sizeof(ui_access), &required) ||
        !GetTokenInformation(token.get(), TokenIsAppContainer, &app_container, sizeof(app_container), &required))
        return result;
    result.elevated = elevation.TokenIsElevated != 0;
    result.ui_access = ui_access != 0;
    result.app_container = app_container != 0;
    result.valid = true;
    return result;
}

bool normal_integrity(const Security& value) noexcept {
    return value.valid && value.integrity == SECURITY_MANDATORY_MEDIUM_RID &&
        !value.elevated && !value.ui_access && !value.app_container;
}

std::optional<std::wstring> wide(std::string_view text,bool path=false) {
    if (text.empty()) return std::wstring{};
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return std::nullopt;
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), size) != size) return std::nullopt;
    if(path)std::replace(result.begin(), result.end(), L'/', L'\\');
    return result;
}

bool equal_path(const std::wstring& first, const std::wstring& second) noexcept {
    return CompareStringOrdinal(first.c_str(), static_cast<int>(first.size()),
        second.c_str(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}

bool excluded_class(const std::wstring& name) {
    constexpr std::array names{L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"#32768", L"tooltips_class32", L"SysShadow", L"TaskListThumbnailWnd", L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow", L"Windows.UI.Core.CoreWindow"};
    return std::any_of(names.begin(), names.end(), [&](auto value) { return equal_path(name, value); });
}

bool rules_allow(const core::v1::Settings& settings, Snapshot& snapshot, std::string& why) {
    const auto separator = snapshot.exe.find_last_of(L"\\/");
    const auto basename = snapshot.exe.substr(separator == std::wstring::npos ? 0 : separator + 1);
    for (const auto& rule : settings.rules) {
        const auto exe = wide(rule.exe,true), cls = wide(rule.window_class);
        if (!exe || !cls) { why = "invalid_utf8_application_rule"; return false; }
        const bool full_path = exe->find_first_of(L"\\/") != std::wstring::npos;
        if (!equal_path(full_path ? snapshot.exe : basename, *exe) ||
            (!cls->empty() && !equal_path(snapshot.window_class, *cls))) continue;
        if (rule.excluded) { why = "application_excluded"; return false; }
        snapshot.feature_mask &= rule.feature_mask;
    }
    if (!snapshot.feature_mask) { why = "all_application_features_disabled"; return false; }
    return true;
}

std::optional<Rect> positive(const RECT& value) {
    if (value.left >= value.right || value.top >= value.bottom) return std::nullopt;
    return Rect{value.left, value.top, value.right, value.bottom};
}

std::uint64_t ticks(FILETIME value) noexcept {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) | value.dwLowDateTime;
}

// Deliberately a FRAME-RECTANGLE candidate policy, not a claim about opaque
// pixels/alpha/custom regions. One known front frame must contain the whole
// rear frame; partial coverage and unions of several fronts remain eligible.
std::vector<Snapshot> without_box_covered(std::vector<Snapshot> front_to_back,bool order_confirmed) {
    if(!order_confirmed)return front_to_back;
    std::vector<Snapshot> result;
    for(std::size_t index=0;index<front_to_back.size();++index){
        const auto& candidate=front_to_back[index];bool covered=false;
        for(std::size_t front=0;front<index;++front){
            const auto& above=front_to_back[front].visible;const auto& rear=candidate.visible;
            if(!above.empty()&&!rear.empty()&&above.left()<=rear.left()&&above.top()<=rear.top()&&
                above.right()>=rear.right()&&above.bottom()>=rear.bottom()){covered=true;break;}
        }
        if(!covered)result.push_back(candidate);
    }
    return result;
}

std::optional<std::vector<HWND>> bounded_z_order() {
    std::vector<HWND> result;
    // The official GetWindow contract warns about lifetime/loop races. This
    // finite observation rejects duplicate handles and is compared again; it
    // is not a replacement resident window-enumeration or polling mechanism.
    for(HWND current=GetTopWindow(nullptr);current;current=GetWindow(current,GW_HWNDNEXT)){
        if(result.size()>=1024||!IsWindow(current)||
            std::find(result.begin(),result.end(),current)!=result.end())return {};
        result.push_back(current);
    }
    return result;
}

bool frame_still_matches(const Snapshot& snapshot) {
    DWORD pid{};RECT positioning{},visible{};
    return IsWindowVisible(snapshot.hwnd)&&!IsIconic(snapshot.hwnd)&&
        GetWindowThreadProcessId(snapshot.hwnd,&pid)==snapshot.tid&&pid==snapshot.pid&&
        GetAncestor(snapshot.hwnd,GA_ROOT)==snapshot.hwnd&&
        GetWindowRect(snapshot.hwnd,&positioning)&&
        SUCCEEDED(DwmGetWindowAttribute(snapshot.hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&visible,sizeof(visible)))&&
        positive(positioning)==snapshot.positioning&&positive(visible)==snapshot.visible&&
        GetDpiForWindow(snapshot.hwnd)==snapshot.dpi&&
        MonitorFromWindow(snapshot.hwnd,MONITOR_DEFAULTTONULL)==snapshot.monitor;
}

bool pmv2_context() noexcept {
    return AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
}

bool active_default_desktop() noexcept {
    const HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    std::array<wchar_t, 128> input_name{}, owner_name{};
    DWORD required{};
    BOOL receiving_input{};
    const bool input_read = GetUserObjectInformationW(input, UOI_NAME, input_name.data(),
        static_cast<DWORD>(sizeof(input_name)), &required) != FALSE;
    const HDESK owner = GetThreadDesktop(GetCurrentThreadId());
    const bool owner_read = owner && GetUserObjectInformationW(owner, UOI_NAME, owner_name.data(),
        static_cast<DWORD>(sizeof(owner_name)), &required) != FALSE;
    const bool io_read = GetUserObjectInformationW(input, UOI_IO, &receiving_input,
        sizeof(receiving_input), &required) != FALSE;
    const bool closed = CloseDesktop(input) != FALSE;
    return input_read && owner_read && io_read && receiving_input != FALSE && closed &&
        CompareStringOrdinal(input_name.data(), -1, L"Default", -1, TRUE) == CSTR_EQUAL &&
        CompareStringOrdinal(input_name.data(), -1, owner_name.data(), -1, TRUE) == CSTR_EQUAL;
}

bool active_session(DWORD expected_session) noexcept {
    DWORD actual_session{};
    if (!expected_session || !ProcessIdToSessionId(GetCurrentProcessId(), &actual_session) ||
        actual_session != expected_session) return false;
    LPWSTR raw{};
    DWORD bytes{};
    const bool read = WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, actual_session,
        WTSConnectState, &raw, &bytes) != FALSE;
    const bool active = read && bytes >= sizeof(WTS_CONNECTSTATE_CLASS) &&
        *reinterpret_cast<const WTS_CONNECTSTATE_CLASS*>(raw) == WTSActive;
    if (raw) WTSFreeMemory(raw);
    return active;
}

constexpr GuiContextMode choose_gui_context_mode(bool identity_current,bool owner_succeeded,bool exact_console,
    bool foreground_before_matches,bool foreground_succeeded,bool active_matches,bool foreground_after_matches) noexcept {
    if(!identity_current)return GuiContextMode::Unknown;
    if(owner_succeeded)return GuiContextMode::OwnerThread;
    return exact_console&&foreground_before_matches&&foreground_succeeded&&active_matches&&foreground_after_matches?
        GuiContextMode::ForegroundConsole:GuiContextMode::Unknown;
}

} // namespace

GuiContext observe_gui_context(const Snapshot& expected) {
    GuiContext result;
    const auto current_identity=[&]{
        DWORD pid{};return expected.hwnd&&expected.pid&&expected.tid&&IsWindow(expected.hwnd)&&
            GetAncestor(expected.hwnd,GA_ROOT)==expected.hwnd&&
            GetWindowThreadProcessId(expected.hwnd,&pid)==expected.tid&&pid==expected.pid;
    };
    if(!current_identity())return result;
    result.foreground_before=GetForegroundWindow();SetLastError(ERROR_SUCCESS);
    result.owner_succeeded=GetGUIThreadInfo(expected.tid,&result.actual)!=FALSE;
    result.owner_error=result.owner_succeeded?ERROR_SUCCESS:GetLastError();
    bool exact_console=false;
    if(!result.owner_succeeded&&expected.window_class==L"ConsoleWindowClass"){
        std::array<wchar_t,64> actual_class{};
        exact_console=GetClassNameW(expected.hwnd,actual_class.data(),static_cast<int>(actual_class.size()))>0&&
            std::wstring_view(actual_class.data())==L"ConsoleWindowClass";
        result.foreground_before=GetForegroundWindow();
        if(exact_console&&result.foreground_before==expected.hwnd){
            GUITHREADINFO foreground{sizeof(foreground)};SetLastError(ERROR_SUCCESS);
            result.foreground_succeeded=GetGUIThreadInfo(0,&foreground)!=FALSE;
            result.foreground_error=result.foreground_succeeded?ERROR_SUCCESS:GetLastError();
            if(result.foreground_succeeded)result.actual=foreground;
        }
    }
    result.foreground_after=GetForegroundWindow();
    result.mode=choose_gui_context_mode(current_identity(),result.owner_succeeded,exact_console,
        result.foreground_before==expected.hwnd,result.foreground_succeeded,result.actual.hwndActive==expected.hwnd,
        result.foreground_after==expected.hwnd);
    return result;
}

namespace {
struct CaptureGuiObservation {
    bool source_identity{},overlay_identity{},overlay_absent{},source_known{},owner_known{};
    GUITHREADINFO source{sizeof(GUITHREADINFO)},owner{sizeof(GUITHREADINFO)};
    HWND foreground_before{},foreground_after{};
};
// The same fact decision is used by actual platform observation and the
// deterministic offline checks below. It does not grant target eligibility.
bool capture_context_facts_allowed(const Snapshot& target,const GuiContext& target_gui,
    const GestureCaptureLease& lease,GeometryContextPhase phase,
    const CaptureGuiObservation& actual,std::string* why){
    const auto reject=[&](const char* reason){if(why)*why=reason;return false;};
    if(!lease)return target_gui.idle()?true:reject("native_GUI_context_not_idle");
    const auto& source=lease->source;
    if(!lease->generation||!source.hwnd||!source.key||!source.pid||!source.tid||!source.born||
       !lease->overlay||!lease->owner_pid||!lease->owner_tid)return reject("capture_lease_identity_incomplete");
    if(!actual.source_identity||!actual.source_known)return reject("capture_source_identity_or_GUI_unknown");
    if(actual.foreground_before!=source.hwnd||actual.foreground_after!=source.hwnd)return reject("capture_source_foreground_changed");
    constexpr DWORD busy=GUI_INMOVESIZE|GUI_INMENUMODE|GUI_POPUPMENUMODE|GUI_SYSTEMMENUMODE;
    if(actual.source.hwndMoveSize||actual.source.hwndMenuOwner||(actual.source.flags&busy))return reject("capture_source_native_interaction_present");
    const bool detached=lease->association_detached.load(std::memory_order_acquire);
    const bool released=lease->own_release_confirmed.load(std::memory_order_acquire);
    const bool normal=lease->normal_up_observed.load(std::memory_order_acquire);
    const bool matching_up=lease->matching_up_observed.load(std::memory_order_acquire);
    const bool retired_before_call=phase==GeometryContextPhase::BeforeNativeCall&&matching_up;
    const auto own_or_empty=[&](HWND capture){return !capture||capture==lease->overlay;};
    const bool same_queue=target.pid==source.pid&&target.tid==source.tid;
    if(target.hwnd==source.hwnd&&!Catalog::same(target,source))return reject("capture_source_catalog_identity_changed");
    if(same_queue){
        if(target.born!=source.born||target_gui.mode!=GuiContextMode::OwnerThread||
           target_gui.actual.hwndMoveSize||target_gui.actual.hwndMenuOwner||(target_gui.actual.flags&busy))return reject("capture_same_queue_context_inconsistent");
        if(target_gui.actual.hwndCapture!=actual.source.hwndCapture){
            if(phase==GeometryContextPhase::IssuedCompletion&&normal&&
               ((target_gui.actual.hwndCapture==lease->overlay&&!actual.source.hwndCapture)||
                (!target_gui.actual.hwndCapture&&actual.source.hwndCapture==lease->overlay)))
                return reject("post_native_capture_cleanup_not_yet_confirmed");
            if(!retired_before_call||!own_or_empty(target_gui.actual.hwndCapture)||
               !own_or_empty(actual.source.hwndCapture))return reject("capture_same_queue_context_inconsistent");
        }
    }else if(!target_gui.idle())return reject("capture_unassociated_target_not_idle");
    if(retired_before_call){
        if(!own_or_empty(actual.source.hwndCapture))return reject("capture_foreign_window_present");
        if(actual.overlay_identity&&actual.owner_known){
            if(!own_or_empty(actual.owner.hwndCapture)||actual.owner.hwndMoveSize||
               actual.owner.hwndMenuOwner||(actual.owner.flags&busy))return reject("capture_owner_identity_or_actual_capture_changed");
        }else if(!(actual.overlay_absent&&normal&&released&&detached))
            return reject("capture_owner_identity_or_GUI_unknown_after_matching_UP");
        // A real UP may retire the generation while independent native reads
        // cross our own release. This remains a rejected new call, not native
        // success or NormalUp. Unknown / foreign context is never swallowed.
        return reject(capture_matching_up_retired_reason);
    }
    if(actual.source.hwndCapture==lease->overlay){
        if(!actual.overlay_identity||!actual.owner_known||actual.owner.hwndCapture!=lease->overlay||
           actual.owner.hwndMoveSize||actual.owner.hwndMenuOwner||(actual.owner.flags&busy))return reject("capture_owner_identity_or_actual_capture_changed");
        if(released||detached)return reject("capture_actual_ownership_conflicts_with_release_facts");
        if(phase==GeometryContextPhase::BeforeNativeCall&&normal)return reject("capture_normal_UP_revoked_new_native_call");
        return true;
    }
    if(actual.source.hwndCapture)return reject("capture_foreign_window_present");
    if(phase==GeometryContextPhase::BeforeNativeCall)return reject("capture_actual_ownership_lost_before_native_call");
    if(normal&&released&&detached)return true;
    if(normal)return reject("post_native_capture_cleanup_not_yet_confirmed");
    return reject("post_native_capture_lost_without_matching_normal_UP");
}
}

bool geometry_gui_context_allowed(const Snapshot& target,const GuiContext& gui,
    const GestureCaptureLease& lease,GeometryContextPhase phase,std::string* why){
    if(!lease)return capture_context_facts_allowed(target,gui,lease,phase,{},why);
    CaptureGuiObservation actual;const auto& source=lease->source;
    actual.foreground_before=GetForegroundWindow();DWORD pid{};
    actual.source_identity=source.hwnd&&source.pid&&source.tid&&source.born&&IsWindow(source.hwnd)&&
        GetAncestor(source.hwnd,GA_ROOT)==source.hwnd&&GetWindowThreadProcessId(source.hwnd,&pid)==source.tid&&pid==source.pid;
    if(actual.source_identity){
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,source.pid));FILETIME born{},exit{},kernel{},user{};
        actual.source_identity=process.get()&&WaitForSingleObject(process.get(),0)==WAIT_TIMEOUT&&
            GetProcessTimes(process.get(),&born,&exit,&kernel,&user)&&ticks(born)==source.born;
    }
    actual.source_known=actual.source_identity&&GetGUIThreadInfo(source.tid,&actual.source)!=FALSE;
    pid=0;const bool overlay_window=IsWindow(lease->overlay)!=FALSE;actual.overlay_absent=!overlay_window;
    actual.overlay_identity=lease->owner_pid==GetCurrentProcessId()&&lease->owner_tid&&lease->overlay&&
        overlay_window&&GetWindowThreadProcessId(lease->overlay,&pid)==lease->owner_tid&&pid==lease->owner_pid;
    actual.owner_known=actual.overlay_identity&&GetGUIThreadInfo(lease->owner_tid,&actual.owner)!=FALSE;
    actual.foreground_after=GetForegroundWindow();
    return capture_context_facts_allowed(target,gui,lease,phase,actual,why);
}

struct Catalog::Impl {
    operations::InteractiveDesktopContextRef desktop_context;
    DWORD owner{GetCurrentThreadId()}, own_pid{GetCurrentProcessId()};
    Security own_security{security(GetCurrentProcess())};
    IVirtualDesktopManager* desktops{};
    HWINEVENTHOOK hook{};
    std::unordered_map<HWND, Snapshot> identities;
    std::array<HWND, 256> invalidations{};
    std::size_t invalidation_count{};
    std::uint64_t next_key{1};
    bool lifecycle_overflow{};

    static std::vector<Impl*>& instances() {
        static thread_local std::vector<Impl*> values;
        return values;
    }
    static void CALLBACK lifecycle(HWINEVENTHOOK source, DWORD, HWND window,
        LONG object, LONG child, DWORD, DWORD) noexcept {
        if (!window || object != OBJID_WINDOW || child != CHILDID_SELF) return;
        for (auto* instance : instances()) {
            if (instance->hook != source) continue;
            if (instance->invalidation_count == instance->invalidations.size()) {
                instance->lifecycle_overflow = true;
                return;
            }
            instance->invalidations[instance->invalidation_count++] = window;
            return;
        }
    }
    explicit Impl(operations::InteractiveDesktopContextRef context):desktop_context(std::move(context)) {
        APTTYPE apartment{};
        APTTYPEQUALIFIER qualifier{};
        if (FAILED(CoGetApartmentType(&apartment, &qualifier)) ||
            (apartment != APTTYPE_STA && apartment != APTTYPE_MAINSTA)) return;
        if (FAILED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&desktops)))) desktops = nullptr;
        hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_DESTROY, nullptr,
            lifecycle, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (hook) instances().push_back(this);
    }
    ~Impl() {
        if (hook) UnhookWinEvent(hook);
        auto& values = instances();
        values.erase(std::remove(values.begin(), values.end(), this), values.end());
        if (desktops) desktops->Release();
    }
    void invalidate_observed() {
        if (lifecycle_overflow) identities.clear();
        else for (std::size_t n = 0; n < invalidation_count; ++n) identities.erase(invalidations[n]);
        invalidation_count = 0;
        lifecycle_overflow = false;
    }
};

Catalog::Catalog(operations::InteractiveDesktopContextRef context) : impl_(std::make_unique<Impl>(std::move(context))) {}
Catalog::~Catalog() = default;
operations::InteractiveDesktopContextRef Catalog::desktop_context()const{return impl_->desktop_context;}

bool Catalog::same(const Snapshot& first, const Snapshot& second) {
    return first.hwnd && first.key && first.key == second.key && first.hwnd == second.hwnd &&
        first.pid == second.pid && first.tid == second.tid && first.born == second.born &&
        first.born != 0 && equal_path(first.exe, second.exe) &&
        first.window_class == second.window_class;
}

std::optional<Snapshot> Catalog::get(HWND window, const core::v1::Settings& settings, std::string* why,
    bool include_minimized_for_restore,bool query_track_limits) {
    auto reject = [&](std::string reason) -> std::optional<Snapshot> {
        // A previously admitted identity cannot regain its old generation
        // after it has been observed ineligible or unreadable.
        if (window && impl_->owner == GetCurrentThreadId()) impl_->identities.erase(window);
        if (why) *why = std::move(reason);
        return std::nullopt;
    };
    if (impl_->owner != GetCurrentThreadId()) return reject("catalog_wrong_thread");
    impl_->invalidate_observed();
    if (!normal_integrity(impl_->own_security)) return reject("controller_not_medium_non_elevated");
    if (!pmv2_context()) return reject("physical_pixel_pmv2_context_required");
    if (!active_session(impl_->own_security.session)) return reject("interactive_session_not_active");
    if (impl_->desktop_context?!impl_->desktop_context->active():!active_default_desktop())
        return reject(impl_->desktop_context?"exact_Run_desktop_not_active":"interactive_default_desktop_not_active");
    if (!impl_->hook || !impl_->desktops) return reject("lifecycle_or_virtual_desktop_service_unavailable");
    if (!window || !IsWindow(window) || GetAncestor(window, GA_ROOT) != window)
        return reject("not_live_root_window");
    if (window == GetDesktopWindow() || window == GetShellWindow()) return reject("shell_desktop");
    if (!IsWindowVisible(window)) return reject("hidden_window");
    if(IsIconic(window)&&!include_minimized_for_restore){
        // Minimize revokes operational admission, not the actual HWND's
        // lifetime. Retain its intrinsic generation for explicit Restore;
        // CREATE/DESTROY still invalidate it and every operation recaptures.
        if(why)*why="minimized_not_a_gesture_target";
        return std::nullopt;
    }

    Snapshot result;
    result.hwnd = window;
    result.tid = GetWindowThreadProcessId(window, &result.pid);
    if (!result.tid || !result.pid || result.pid == impl_->own_pid) return reject("invalid_or_self_identity");
    std::array<wchar_t, 512> class_name{};
    const int class_length = GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size()));
    if (class_length <= 0 || class_length >= static_cast<int>(class_name.size()) - 1)
        return reject("window_class_unavailable");
    result.window_class.assign(class_name.data(), static_cast<std::size_t>(class_length));
    if (excluded_class(result.window_class)) return reject("excluded_system_window_class");

    SetLastError(ERROR_SUCCESS);
    const auto style = static_cast<ULONG_PTR>(GetWindowLongPtrW(window, GWL_STYLE));
    if ((!style && GetLastError() != ERROR_SUCCESS) || (style & WS_CHILD) ||
        ((style & WS_CAPTION) != WS_CAPTION && !(style & WS_THICKFRAME)))
        return reject("not_application_frame");
    DWORD cloak{};
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloak, sizeof(cloak))) || cloak != 0)
        return reject("cloaked_or_unknown");
    BOOL on_current_desktop{};
    const HRESULT desktop_result=impl_->desktops->IsWindowOnCurrentVirtualDesktop(window, &on_current_desktop);
    if(desktop_result!=S_OK)return reject("virtual_desktop_query_failed:"+std::to_string(static_cast<std::uint32_t>(desktop_result)));
    if(!operations::current_virtual_desktop_allowed(desktop_result,on_current_desktop))return reject("not_confirmed_current_virtual_desktop");

    const Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, result.pid));
    if (!process.get() || WaitForSingleObject(process.get(), 0) != WAIT_TIMEOUT)
        return reject("process_unavailable_or_exited");
    const auto target_security = security(process.get());
    if (!normal_integrity(target_security)) return reject("target_not_medium_non_elevated");
    if (target_security.session != impl_->own_security.session ||
        !EqualSid(const_cast<unsigned char*>(target_security.sid.data()), impl_->own_security.sid.data()))
        return reject("user_or_session_mismatch");
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &user) || !(result.born = ticks(created)))
        return reject("process_birth_unavailable");
    if(impl_->desktop_context&&!operations::run_desktop_source_allowed(
        impl_->desktop_context->observe(window,result.pid,result.tid,result.born)))
        return reject("exact_Run_source_membership_or_identity_unconfirmed");
    std::wstring path(32768, L'\0');
    DWORD path_length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &path_length) || !path_length)
        return reject("process_image_unavailable");
    path.resize(path_length);
    result.exe = std::move(path);
    std::string rule_reason;
    if (!rules_allow(settings, result, rule_reason)) return reject(std::move(rule_reason));

    const auto arranged_before = native_arranged(window);
    RECT positioning{}, visible{};
    if (!GetWindowRect(window, &positioning) ||
        FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible))))
        return reject("frame_geometry_unavailable");
    const auto p = positive(positioning), v = positive(visible);
    if (!p || !v) return reject("empty_frame_geometry");
    result.positioning = *p;
    result.visible = *v;
    result.monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
    MONITORINFO info{sizeof(info)};
    if (!result.monitor || !GetMonitorInfoW(result.monitor, &info) || !positive(info.rcWork))
        return reject("monitor_work_area_unavailable");
    result.work = *positive(info.rcWork);
    result.dpi = GetDpiForWindow(window);
    if (!result.dpi) return reject("window_dpi_unavailable");
    result.maximized = IsZoomed(window) != FALSE;
    result.minimized = IsIconic(window) != FALSE;
    result.can_resize = (style & WS_THICKFRAME) != 0;
    // Query the application's declared track bounds, on the normal GUI owner
    // (never in the hook). WM_GETMINMAXINFO is an OS-marshalled system message.
    // A timeout/unknown answer is not permission to resize past app limits.
    const auto frame_width = result.positioning.width() - result.visible.width();
    const auto frame_height = result.positioning.height() - result.visible.height();
    const int min_track_width = GetSystemMetricsForDpi(SM_CXMINTRACK, result.dpi);
    const int min_track_height = GetSystemMetricsForDpi(SM_CYMINTRACK, result.dpi);
    if (min_track_width <= 0 || min_track_height <= 0) return reject("system_min_track_unavailable");
    result.limits.min_width = std::max<std::int64_t>(1,
        static_cast<std::int64_t>(min_track_width) - frame_width);
    result.limits.min_height = std::max<std::int64_t>(1,
        static_cast<std::int64_t>(min_track_height) - frame_height);
    if(result.can_resize&&query_track_limits){
        MINMAXINFO limits{};
        limits.ptMaxSize={static_cast<LONG>(result.work.width()),static_cast<LONG>(result.work.height())};
        limits.ptMaxPosition={static_cast<LONG>(result.work.left()),static_cast<LONG>(result.work.top())};
        limits.ptMinTrackSize={min_track_width,min_track_height};
        limits.ptMaxTrackSize={GetSystemMetricsForDpi(SM_CXMAXTRACK,result.dpi),GetSystemMetricsForDpi(SM_CYMAXTRACK,result.dpi)};
        DWORD_PTR message_result{};
        if(!SendMessageTimeoutW(window,WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&limits),
            SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,30,&message_result))return reject("application_track_limits_unavailable");
        const auto minimum_width=static_cast<std::int64_t>(limits.ptMinTrackSize.x)-frame_width;
        const auto minimum_height=static_cast<std::int64_t>(limits.ptMinTrackSize.y)-frame_height;
        const auto maximum_width=static_cast<std::int64_t>(limits.ptMaxTrackSize.x)-frame_width;
        const auto maximum_height=static_cast<std::int64_t>(limits.ptMaxTrackSize.y)-frame_height;
        if(minimum_width<=0||minimum_height<=0||maximum_width<minimum_width||maximum_height<minimum_height)
            return reject("application_track_limits_invalid");
        result.limits.min_width=minimum_width;result.limits.min_height=minimum_height;
        result.limits.max_width=maximum_width;result.limits.max_height=maximum_height;
        result.declared_track_limits_observed=true;
    }
    if (!result.can_resize) {
        result.limits.min_width = result.limits.max_width = result.visible.width();
        result.limits.min_height = result.limits.max_height = result.visible.height();
    }

    DWORD final_pid{};
    if (GetWindowThreadProcessId(window, &final_pid) != result.tid || final_pid != result.pid ||
        !IsWindow(window) || WaitForSingleObject(process.get(), 0) != WAIT_TIMEOUT)
        return reject("identity_changed_during_capture");
    const auto arranged_after = native_arranged(window);
    if (arranged_before && arranged_after && *arranged_before == *arranged_after &&
        !(*arranged_after && (result.maximized || result.minimized)))
        result.native_arranged = arranged_after;
    auto previous = impl_->identities.find(window);
    // Compare the native identity before assigning/reusing the opaque key.
    if (previous != impl_->identities.end() && previous->second.pid == result.pid &&
        previous->second.tid == result.tid && previous->second.born == result.born &&
        equal_path(previous->second.exe, result.exe) && previous->second.window_class == result.window_class) {
        result.key = previous->second.key;
    } else {
        if (impl_->next_key == std::numeric_limits<std::uint64_t>::max()) return reject("window_generation_exhausted");
        result.key = impl_->next_key++;
    }
    impl_->identities.insert_or_assign(window, result);
    if (why) *why = "eligible_medium_same_user_current_desktop";
    return result;
}

std::vector<Snapshot> Catalog::all(const core::v1::Settings& settings) {
    std::vector<Snapshot> result;
    if (impl_->owner != GetCurrentThreadId()) return result;
    const auto before=bounded_z_order();
    struct Context { Catalog* catalog; const core::v1::Settings* settings; std::vector<Snapshot>* output;std::size_t roots{};bool truncated{}; };
    Context context{this, &settings, &result};
    const auto enumerate=[](HWND window, LPARAM value) -> BOOL {
        auto& ctx = *reinterpret_cast<Context*>(value);
        if(++ctx.roots>1024){ctx.truncated=true;return FALSE;}
        try {
            // Magnet candidates need actual geometry/identity, not hundreds
            // of synchronous app-constraint queries. Actual operation members
            // are re-admitted with declared limits before planning/writing.
            if (auto snapshot = ctx.catalog->get(window, *ctx.settings,nullptr,false,false)){
                if(ctx.output->size()==256){ctx.truncated=true;return FALSE;}
                ctx.output->push_back(std::move(*snapshot));
            }
            return TRUE;
        } catch (...) { return FALSE; }
    };
    const bool enumerated=(impl_->desktop_context?EnumDesktopWindows(impl_->desktop_context->desktop(),enumerate,
        reinterpret_cast<LPARAM>(&context)):EnumWindows(enumerate,reinterpret_cast<LPARAM>(&context)))!=FALSE;
    // A truncated set cannot stand in for the complete group/candidate set.
    if(!enumerated||context.truncated)return {};
    const bool frames_current=std::all_of(result.begin(),result.end(),frame_still_matches);
    const auto after=bounded_z_order();
    if(!before||!after||*before!=*after||!frames_current)return without_box_covered(std::move(result),false);
    for(const auto& snapshot:result)if(std::find(after->begin(),after->end(),snapshot.hwnd)==after->end())
        return without_box_covered(std::move(result),false);
    std::sort(result.begin(),result.end(),[&](const auto& first,const auto& second){
        return std::find(after->begin(),after->end(),first.hwnd)<std::find(after->begin(),after->end(),second.hwnd);
    });
    return without_box_covered(std::move(result),true);
}

std::vector<Display> Catalog::displays() {
    std::vector<Display> result;
    if (impl_->owner != GetCurrentThreadId() || !pmv2_context()) return result;
    // The target window's DPI may be virtualized for a legacy application.
    // Use an invisible, nonactivating PMv2 window OWNED BY THIS PROCESS to
    // measure each display independently. It is never shown, captures no input
    // and changes no foreign window or physical display settings.
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM value) -> BOOL {
        auto& output = *reinterpret_cast<std::vector<Display>*>(value);
        MONITORINFO info{sizeof(info)};
        if (!GetMonitorInfoW(monitor, &info) || !positive(info.rcWork) || !positive(info.rcMonitor)) return TRUE;
        const HWND measure = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"STATIC", L"", WS_POPUP, info.rcWork.left, info.rcWork.top, 1, 1,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!measure) return TRUE;
        const UINT dpi = GetDpiForWindow(measure);
        const bool same_monitor = MonitorFromWindow(measure, MONITOR_DEFAULTTONULL) == monitor;
        const bool destroyed = DestroyWindow(measure) != FALSE;
        if (!dpi || !same_monitor || !destroyed) return TRUE;
        try { output.push_back({monitor, *positive(info.rcWork), *positive(info.rcMonitor), dpi}); }
        catch (...) { return FALSE; }
        return output.size() < 32 ? TRUE : FALSE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

} // namespace panebind::platform::windows::product

#ifdef PANEBIND_V1_CATALOG_RULE_TEST
// Exercise the production conversion/matching path, without constructing a
// Catalog or observing/controlling any native window on the build host.
int main(){
    namespace p=panebind::platform::windows::product;
    namespace v=panebind::core::v1;
    int checks{},failures{};auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}};
    namespace desktop=panebind::platform::windows::operations;
    desktop::RunDesktopFacts actual_run{true,true,true,true,true,true,true,true,true,true};
    check(desktop::run_desktop_active(actual_run),"Run context needs actual Medium unlocked station owner input and held desktop");
    actual_run.owner_matches=false;check(!desktop::run_desktop_active(actual_run),"wrong owner Run cannot activate context");
    actual_run.owner_matches=true;actual_run.input_matches=false;
    check(!desktop::run_desktop_active(actual_run),"changed input desktop revokes the same context without a name fallback");
    actual_run.input_matches=true;actual_run.owner_active=false;
    check(!desktop::run_desktop_active(actual_run),"same owner name without actual owner UOI_IO is insufficient");
    actual_run.owner_active=true;actual_run.session_active_unlocked=false;
    check(!desktop::run_desktop_active(actual_run),"locked or unknown interactive session cannot use Run context");
    desktop::RunDesktopSourceFacts member;
    member.active=member.identity_stable=member.first_complete=member.second_complete=member.first_member=member.second_member=true;
    member.thread_desktop_error=ERROR_ACCESS_DENIED;
    check(desktop::run_desktop_source_allowed(member)&&!member.source_name_query&&!member.source_io_query,
        "actual shared Catalog shield writer predicate accepts separate double membership without falsifying unknown foreign queries");
    member.source_name_query=true;member.source_name_matches=false;
    check(!desktop::run_desktop_source_allowed(member),"known contrary source desktop name defeats otherwise complete membership");
    member.source_name_matches=true;member.source_io_query=true;member.source_io_active=false;
    check(!desktop::run_desktop_source_allowed(member),"known inactive source IO defeats otherwise complete membership");
    member.source_io_active=true;member.second_complete=false;
    check(!desktop::run_desktop_source_allowed(member),"truncated second enumeration grants no context evidence");
    member.second_complete=true;member.identity_stable=false;
    check(!desktop::run_desktop_source_allowed(member),"source identity changing across enumerations cannot be admitted");
    check(desktop::current_virtual_desktop_allowed(S_OK,TRUE)&&!desktop::current_virtual_desktop_allowed(S_OK,FALSE)&&
        !desktop::current_virtual_desktop_allowed(E_FAIL,TRUE)&&!desktop::current_virtual_desktop_allowed(S_FALSE,TRUE),
        "production VDM predicate still requires actual S_OK and true, not unknown success-like status");
    v::Settings settings=v::default_settings();p::Snapshot snapshot;
    snapshot.exe=L"C:\\Apps\\Editor.exe";snapshot.window_class=L"Frame/A";
    settings.rules={{"C:/Apps/EDITOR.exe","frame/A",true,0xffffffffu}};std::string why;
    check(!p::rules_allow(settings,snapshot,why)&&why=="application_excluded","full executable path separators normalize while class letters compare ordinally");
    why.clear();settings.rules.front().window_class="frame\\A";
    check(p::rules_allow(settings,snapshot,why)&&why.empty(),"different literal class slash cannot exclude another class");
    settings.rules.front().exe="editor.exe";settings.rules.front().window_class="FRAME/A";
    check(!p::rules_allow(settings,snapshot,why),"basename and class case-insensitive matching retained");
    snapshot.feature_mask=0xffffffffu;settings.rules.front().excluded=false;settings.rules.front().feature_mask=2u;
    check(p::rules_allow(settings,snapshot,why)&&snapshot.feature_mask==2u,"per-application feature mask applies to the exact class");
    snapshot.feature_mask=0xffffffffu;snapshot.window_class=L"Frame\\A";
    check(p::rules_allow(settings,snapshot,why)&&snapshot.feature_mask==0xffffffffu,"feature mask does not leak into other literal class");
    settings.rules.front().window_class.clear();settings.rules.front().excluded=true;
    check(!p::rules_allow(settings,snapshot,why),"empty class deliberately still matches all program windows");
    const auto box=[](std::uint64_t key,v::Rect r){p::Snapshot result;result.key=key;result.visible=r;return result;};
    const auto front=box(1,{0,0,100,100}),rear=box(2,{10,10,90,90});
    check(p::without_box_covered({front,rear},true).size()==1,"actual front frame fully containing rear removes only rear candidate");
    check(p::without_box_covered({rear,front},true).size()==2,"a larger frame behind cannot cover a front candidate");
    check(p::without_box_covered({front,box(3,{90,90,110,110})},true).size()==2,"partially exposed rear remains a geometry candidate");
    check(p::without_box_covered({front,box(3,{100,0,200,100})},true).size()==2,"edge-adjacent frames remain available for glue");
    check(p::without_box_covered({front,box(3,front.visible)},true).size()==1,"equal bounds respect confirmed actual front order");
    check(p::without_box_covered({front,rear},false).size()==2,"unknown or changed Z order cannot assert occlusion");
    check(p::without_box_covered({box(1,{0,0,50,100}),box(2,{50,0,100,100}),box(3,{0,0,100,100})},true).size()==3,
        "rectangle-only policy does not invent multi-window union coverage");
    using Mode=p::GuiContextMode;
    check(p::choose_gui_context_mode(true,true,false,false,false,false,false)==Mode::OwnerThread,"successful owner GUI query remains preferred for ordinary and follower frames");
    check(p::choose_gui_context_mode(true,false,true,true,true,true,true)==Mode::ForegroundConsole,"exact foreground console accepts actual matching GTI zero result");
    check(p::choose_gui_context_mode(true,false,false,true,true,true,true)==Mode::Unknown,"ordinary classes cannot use foreground-console fallback");
    check(p::choose_gui_context_mode(true,false,true,false,true,true,true)==Mode::Unknown,"background console cannot use fallback");
    check(p::choose_gui_context_mode(true,false,true,true,false,true,true)==Mode::Unknown,"failed GTI zero is unknown rather than empty capture");
    check(p::choose_gui_context_mode(true,false,true,true,true,false,true)==Mode::Unknown,"foreground GUI active HWND must be the exact console root");
    check(p::choose_gui_context_mode(true,false,true,true,true,true,false)==Mode::Unknown,"foreground change during observation rejects fallback");
    check(p::choose_gui_context_mode(false,true,true,true,true,true,true)==Mode::Unknown,"lost source identity rejects even successful native GUI reads");
    p::GuiContext gui;gui.mode=Mode::ForegroundConsole;
    check(gui.idle(),"confirmed GUI result with actual empty capture/move/menu is idle");
    gui.actual.hwndCapture=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(1));check(!gui.idle(),"actual console capture still rejects interaction");
    gui.actual.hwndCapture=nullptr;gui.actual.hwndMoveSize=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(1));check(!gui.idle(),"actual console native move-size still rejects interaction");
    gui.actual.hwndMoveSize=nullptr;gui.actual.hwndMenuOwner=reinterpret_cast<HWND>(static_cast<std::uintptr_t>(1));check(!gui.idle(),"actual console menu still rejects interaction");
    auto capture=std::make_shared<p::GestureCaptureContext>();capture->generation=7;
    capture->source=snapshot;capture->source.hwnd=reinterpret_cast<HWND>(std::uintptr_t{10});
    capture->source.pid=20;capture->source.tid=30;capture->source.born=40;capture->source.key=50;
    capture->overlay=reinterpret_cast<HWND>(std::uintptr_t{60});capture->owner_pid=70;capture->owner_tid=80;
    p::CaptureGuiObservation observed;observed.source_identity=observed.overlay_identity=observed.source_known=observed.owner_known=true;
    observed.foreground_before=observed.foreground_after=capture->source.hwnd;
    observed.source.hwndCapture=observed.owner.hwndCapture=capture->overlay;
    p::GuiContext captured;captured.mode=Mode::OwnerThread;captured.actual.hwndCapture=capture->overlay;
    const auto allowed=[&](const p::Snapshot& target,const p::GuiContext& context,p::GeometryContextPhase phase){why.clear();
        return p::capture_context_facts_allowed(target,context,capture,phase,observed,&why);};
    using Phase=p::GeometryContextPhase;
    check(allowed(capture->source,captured,Phase::BeforeNativeCall),"actual source and shield capture facts admit only pre-native context, not target authority");
    auto follower=capture->source;follower.hwnd=reinterpret_cast<HWND>(std::uintptr_t{11});++follower.key;
    check(allowed(follower,captured,Phase::BeforeNativeCall),"separately admitted same-PID same-TID follower observes the same legal shield queue");
    ++follower.tid;check(!allowed(follower,captured,Phase::BeforeNativeCall),"unassociated different-TID follower cannot inherit shield capture");
    auto idle=captured;idle.actual.hwndCapture=nullptr;
    check(allowed(follower,idle,Phase::BeforeNativeCall),"different-TID participant must remain independently idle while source capture is exact");
    observed.owner.hwndCapture=nullptr;
    check(!allowed(capture->source,captured,Phase::BeforeNativeCall),"source capture alone cannot replace actual shield-owner capture readback");
    observed.owner.hwndCapture=capture->overlay;observed.source.hwndCapture=nullptr;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall),"idle after actual capture loss never admits a new native call before Lost delivery");
    capture->normal_up_observed=true;
    capture->matching_up_observed=true;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why=="capture_matching_UP_retired_before_native_call",
        "matching normal UP during capture sampling retires an unissued call rather than reporting source loss");
    check(!allowed(capture->source,captured,Phase::BeforeNativeCall)&&why=="capture_matching_UP_retired_before_native_call",
        "pre-native capture reads crossing the same matching UP remain cancellation, never new-call authority");
    observed.source.hwndCapture=capture->overlay;
    check(!allowed(capture->source,captured,Phase::BeforeNativeCall)&&why=="capture_matching_UP_retired_before_native_call",
        "matching UP with own capture still present retires the unissued call before cleanup");
    observed.source.hwndCapture=nullptr;
    capture->normal_up_observed=false;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why==p::capture_matching_up_retired_reason&&
          !allowed(capture->source,idle,Phase::IssuedCompletion)&&why=="post_native_capture_lost_without_matching_normal_UP",
        "actual matched LL or Raw UP retires admission but cannot invent legacy UP or issued completion");
    observed.owner_known=false;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why=="capture_owner_identity_or_GUI_unknown_after_matching_UP",
        "matching UP never swallows an unknown live owner GUI observation");
    observed.overlay_identity=false;observed.overlay_absent=true;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why!="capture_matching_UP_retired_before_native_call",
        "missing overlay without confirmed normal release and detach is not known own cleanup");
    capture->normal_up_observed=true;capture->own_release_confirmed=true;capture->association_detached=true;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why==p::capture_matching_up_retired_reason,
        "actually absent own overlay after real normal release and detach is retired, never renewed authority");
    capture->own_release_confirmed=false;capture->association_detached=false;
    observed.overlay_absent=false;observed.overlay_identity=observed.owner_known=true;
    observed.owner.hwndCapture=reinterpret_cast<HWND>(std::uintptr_t{99});
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why!="capture_matching_UP_retired_before_native_call",
        "matching UP cannot hide a known foreign owner capture");
    observed.owner.hwndCapture=capture->overlay;observed.source_identity=false;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why=="capture_source_identity_or_GUI_unknown",
        "matching UP does not suppress an actual source identity failure");
    observed.source_identity=true;observed.foreground_after=nullptr;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why=="capture_source_foreground_changed",
        "matching UP does not suppress a real foreground change");
    observed.foreground_after=capture->source.hwnd;observed.source.hwndMoveSize=capture->source.hwnd;
    check(!allowed(capture->source,idle,Phase::BeforeNativeCall)&&why=="capture_source_native_interaction_present",
        "matching UP does not suppress a new native modal interaction");
    observed.source.hwndMoveSize=nullptr;
    check(!allowed(capture->source,idle,Phase::IssuedCompletion)&&why=="post_native_capture_cleanup_not_yet_confirmed",
        "actual normal UP without confirmed release and detach is explicit post-native unknown, not completion PASS");
    capture->own_release_confirmed=true;
    check(!allowed(capture->source,idle,Phase::IssuedCompletion),"release without actual detach still cannot claim clean issued context");
    capture->association_detached=true;
    check(allowed(capture->source,idle,Phase::IssuedCompletion)&&!allowed(capture->source,idle,Phase::BeforeNativeCall),
        "actual normal UP plus release and detach permits issued observation but never restores new-call admission");
    check(!allowed(capture->source,captured,Phase::IssuedCompletion)&&why=="post_native_capture_cleanup_not_yet_confirmed",
        "post-native snapshot crossing normal release is not silently accepted as fresh capture");
    observed.source.hwndCapture=reinterpret_cast<HWND>(std::uintptr_t{99});idle.actual.hwndCapture=observed.source.hwndCapture;
    check(!allowed(capture->source,idle,Phase::IssuedCompletion),"normal-UP cleanup facts cannot authorize actual foreign capture");
    observed.source.hwndCapture=nullptr;idle.actual.hwndCapture=nullptr;observed.foreground_after=nullptr;
    check(!allowed(capture->source,idle,Phase::IssuedCompletion),"issued geometry does not hide an actual foreground-context change");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
#endif
