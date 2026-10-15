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

} // namespace

struct Catalog::Impl {
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
    Impl() {
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

Catalog::Catalog() : impl_(std::make_unique<Impl>()) {}
Catalog::~Catalog() = default;

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
    if (!active_default_desktop()) return reject("interactive_default_desktop_not_active");
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
    if(on_current_desktop==FALSE)return reject("not_confirmed_current_virtual_desktop");

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
    std::wstring path(32768, L'\0');
    DWORD path_length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &path_length) || !path_length)
        return reject("process_image_unavailable");
    path.resize(path_length);
    result.exe = std::move(path);
    std::string rule_reason;
    if (!rules_allow(settings, result, rule_reason)) return reject(std::move(rule_reason));

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
    struct Context { Catalog* catalog; const core::v1::Settings* settings; std::vector<Snapshot>* output; };
    Context context{this, &settings, &result};
    EnumWindows([](HWND window, LPARAM value) -> BOOL {
        auto& ctx = *reinterpret_cast<Context*>(value);
        try {
            // Magnet candidates need actual geometry/identity, not hundreds
            // of synchronous app-constraint queries. Actual operation members
            // are re-admitted with declared limits before planning/writing.
            if (auto snapshot = ctx.catalog->get(window, *ctx.settings,nullptr,false,false)) ctx.output->push_back(std::move(*snapshot));
            return ctx.output->size() < 256 ? TRUE : FALSE;
        } catch (...) { return FALSE; }
    }, reinterpret_cast<LPARAM>(&context));
    return result;
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
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
#endif
