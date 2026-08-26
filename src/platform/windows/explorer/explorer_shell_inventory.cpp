#include "platform/windows/explorer/explorer_shell_inventory.h"

#include <bcrypt.h>
#include <objbase.h>
#include <oleauto.h>
#include <exdisp.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace panebind::platform::windows::explorer {
namespace {

constexpr HRESULT kInvalidWindowHandle =
    HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
constexpr auto kWindowHandleReadinessSlice = std::chrono::milliseconds{40};
constexpr std::size_t kMaximumMessagesPerReadinessSlice = 64U;

template <typename Interface>
class ComPtr final {
public:
    ComPtr() noexcept = default;
    ~ComPtr() {
        reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : pointer_(other.detach()) {}
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset(other.detach());
        }
        return *this;
    }

    [[nodiscard]] Interface* get() const noexcept {
        return pointer_;
    }

    [[nodiscard]] Interface** put() noexcept {
        reset();
        return &pointer_;
    }

    [[nodiscard]] Interface* operator->() const noexcept {
        return pointer_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return pointer_ != nullptr;
    }

    [[nodiscard]] Interface* detach() noexcept {
        return std::exchange(pointer_, nullptr);
    }

    void reset(Interface* pointer = nullptr) noexcept {
        if (pointer_ != nullptr) {
            static_cast<void>(pointer_->Release());
        }
        pointer_ = pointer;
    }

private:
    Interface* pointer_{};
};

class UniqueBstr final {
public:
    UniqueBstr() noexcept = default;
    ~UniqueBstr() {
        reset();
    }

    UniqueBstr(const UniqueBstr&) = delete;
    UniqueBstr& operator=(const UniqueBstr&) = delete;

    [[nodiscard]] BSTR* put() noexcept {
        reset();
        return &value_;
    }

    [[nodiscard]] BSTR get() const noexcept {
        return value_;
    }

    [[nodiscard]] UINT size() const noexcept {
        return value_ == nullptr ? 0U : SysStringLen(value_);
    }

    void reset(BSTR value = nullptr) noexcept {
        if (value_ != nullptr) {
            SysFreeString(value_);
        }
        value_ = value;
    }

private:
    BSTR value_{};
};

class UniqueHandle final {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        reset();
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(CloseHandle(handle_));
        }
        handle_ = handle;
    }

private:
    HANDLE handle_{};
};

class UniqueLocalString final {
public:
    UniqueLocalString() noexcept = default;
    ~UniqueLocalString() {
        reset();
    }

    UniqueLocalString(const UniqueLocalString&) = delete;
    UniqueLocalString& operator=(const UniqueLocalString&) = delete;

    [[nodiscard]] PWSTR* put() noexcept {
        reset();
        return &value_;
    }

    [[nodiscard]] PCWSTR get() const noexcept {
        return value_;
    }

    void reset(PWSTR value = nullptr) noexcept {
        if (value_ != nullptr) {
            static_cast<void>(LocalFree(value_));
        }
        value_ = value;
    }

private:
    PWSTR value_{};
};

class UniquePidl final {
public:
    UniquePidl() noexcept = default;
    ~UniquePidl() {
        reset();
    }

    UniquePidl(const UniquePidl&) = delete;
    UniquePidl& operator=(const UniquePidl&) = delete;

    [[nodiscard]] PIDLIST_ABSOLUTE* put() noexcept {
        reset();
        return &value_;
    }

    [[nodiscard]] PIDLIST_ABSOLUTE get() const noexcept {
        return value_;
    }

    void reset(PIDLIST_ABSOLUTE value = nullptr) noexcept {
        if (value_ != nullptr) {
            CoTaskMemFree(value_);
        }
        value_ = value;
    }

private:
    PIDLIST_ABSOLUTE value_{};
};

class ScopedVariant final {
public:
    ScopedVariant() noexcept {
        VariantInit(&value_);
    }

    ~ScopedVariant() {
        static_cast<void>(VariantClear(&value_));
    }

    ScopedVariant(const ScopedVariant&) = delete;
    ScopedVariant& operator=(const ScopedVariant&) = delete;

    [[nodiscard]] VARIANT* get() noexcept {
        return &value_;
    }

    [[nodiscard]] VARIANT& value() noexcept {
        return value_;
    }

private:
    VARIANT value_{};
};

[[nodiscard]] HRESULT require_sta() noexcept {
    APTTYPE apartment_type = APTTYPE_CURRENT;
    APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;
    const HRESULT result = CoGetApartmentType(&apartment_type, &qualifier);
    if (result != S_OK) {
        return result;
    }

    if (apartment_type != APTTYPE_STA &&
        apartment_type != APTTYPE_MAINSTA) {
        return RPC_E_WRONG_THREAD;
    }
    return S_OK;
}

[[nodiscard]] bool on_thread(const DWORD thread_id) noexcept {
    return GetCurrentThreadId() == thread_id;
}

[[nodiscard]] bool is_separator(const wchar_t value) noexcept {
    return value == L'\\' || value == L'/';
}

[[nodiscard]] bool ascii_equal_case_insensitive(const wchar_t left,
                                                const wchar_t right) noexcept {
    const auto normalize = [](const wchar_t value) noexcept {
        if (value >= L'A' && value <= L'Z') {
            return static_cast<wchar_t>(value - L'A' + L'a');
        }
        return value;
    };
    return normalize(left) == normalize(right);
}

[[nodiscard]] bool starts_with_case_insensitive(
    const std::wstring_view value,
    const std::wstring_view prefix) noexcept {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < prefix.size(); ++index) {
        if (!ascii_equal_case_insensitive(value[index], prefix[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_unc_tail(const std::wstring_view path,
                                  const std::size_t start) noexcept {
    if (start >= path.size()) {
        return false;
    }

    const std::size_t server_end = path.find_first_of(L"\\/", start);
    if (server_end == std::wstring_view::npos || server_end == start) {
        return false;
    }

    const std::size_t share_start = server_end + 1U;
    if (share_start >= path.size() || is_separator(path[share_start])) {
        return false;
    }
    const std::size_t share_end =
        path.find_first_of(L"\\/", share_start);
    return share_end == std::wstring_view::npos || share_end > share_start;
}

[[nodiscard]] bool absolute_dos_or_unc(
    const std::wstring_view path) noexcept {
    if (path.size() >= 3U &&
        ((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && is_separator(path[2])) {
        return true;
    }

    if (path.size() >= 7U && is_separator(path[0]) &&
        is_separator(path[1]) && path[2] == L'?' &&
        is_separator(path[3]) &&
        ((path[4] >= L'A' && path[4] <= L'Z') ||
         (path[4] >= L'a' && path[4] <= L'z')) &&
        path[5] == L':' && is_separator(path[6])) {
        return true;
    }

    if (path.size() >= 8U && is_separator(path[0]) &&
        is_separator(path[1]) && path[2] == L'?' &&
        is_separator(path[3]) &&
        starts_with_case_insensitive(path.substr(4U), L"UNC\\")) {
        return valid_unc_tail(path, 8U);
    }

    return path.size() >= 5U && is_separator(path[0]) &&
           is_separator(path[1]) && path[2] != L'?' && path[2] != L'.' &&
           valid_unc_tail(path, 2U);
}

[[nodiscard]] bool contains_embedded_null(
    const std::wstring_view value) noexcept {
    return value.find(L'\0') != std::wstring_view::npos;
}

[[nodiscard]] ShellLocationFact location_failure(
    const ShellLocationStatus status,
    const ShellLocationSource source,
    const HRESULT diagnostic) noexcept {
    ShellLocationFact fact;
    fact.status = status;
    fact.source = source;
    fact.diagnostic = diagnostic;
    return fact;
}

[[nodiscard]] std::optional<OpaqueLocationFingerprint>
opaque_location_fingerprint(const std::wstring_view location) noexcept {
    if (location.empty() ||
        location.size() >
            std::numeric_limits<ULONG>::max() / sizeof(wchar_t)) {
        return std::nullopt;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    const NTSTATUS open_result = BCryptOpenAlgorithmProvider(
        &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U);
    if (!BCRYPT_SUCCESS(open_result) || algorithm == nullptr) {
        return std::nullopt;
    }

    OpaqueLocationFingerprint fingerprint{};
    const auto input_bytes = static_cast<ULONG>(
        location.size() * sizeof(wchar_t));
    const NTSTATUS hash_result = BCryptHash(
        algorithm,
        nullptr,
        0U,
        reinterpret_cast<PUCHAR>(
            const_cast<wchar_t*>(location.data())),
        input_bytes,
        reinterpret_cast<PUCHAR>(fingerprint.data()),
        static_cast<ULONG>(fingerprint.size()));
    static_cast<void>(BCryptCloseAlgorithmProvider(algorithm, 0U));
    if (!BCRYPT_SUCCESS(hash_result)) {
        return std::nullopt;
    }
    return fingerprint;
}

[[nodiscard]] ShellLocationFact identify_absolute_path(
    const std::wstring_view path,
    const ShellLocationSource source) {
    if (path.empty() || contains_embedded_null(path) ||
        !absolute_dos_or_unc(path)) {
        return location_failure(ShellLocationStatus::InvalidPath,
                                source,
                                E_INVALIDARG);
    }

    const std::wstring terminated_path{path};
    UniqueHandle handle{CreateFileW(terminated_path.c_str(),
                                    0U,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE |
                                        FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS,
                                    nullptr)};
    if (!handle) {
        return location_failure(ShellLocationStatus::OpenFailed,
                                source,
                                HRESULT_FROM_WIN32(GetLastError()));
    }

    BY_HANDLE_FILE_INFORMATION basic{};
    if (GetFileInformationByHandle(handle.get(), &basic) == FALSE) {
        return location_failure(ShellLocationStatus::IdentityQueryFailed,
                                source,
                                HRESULT_FROM_WIN32(GetLastError()));
    }
    if ((basic.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) {
        return location_failure(ShellLocationStatus::NotDirectory,
                                source,
                                HRESULT_FROM_WIN32(ERROR_DIRECTORY));
    }

    FILE_ID_INFO native_identity{};
    if (GetFileInformationByHandleEx(handle.get(),
                                     FileIdInfo,
                                     &native_identity,
                                     sizeof(native_identity)) == FALSE) {
        return location_failure(ShellLocationStatus::IdentityQueryFailed,
                                source,
                                HRESULT_FROM_WIN32(GetLastError()));
    }

    static_assert(sizeof(native_identity.FileId.Identifier) == 16U);
    FilesystemLocationIdentity identity;
    identity.volume_serial_number = native_identity.VolumeSerialNumber;
    std::memcpy(identity.file_id.data(),
                native_identity.FileId.Identifier,
                identity.file_id.size());

    ShellLocationFact fact;
    fact.status = ShellLocationStatus::Filesystem;
    fact.source = source;
    fact.identity = identity;
    fact.diagnostic = S_OK;
    return fact;
}

[[nodiscard]] ShellLocationFact parse_location(
    const std::wstring_view location) {
    if (location.empty()) {
        return location_failure(ShellLocationStatus::NavigationPending,
                                ShellLocationSource::None,
                                S_FALSE);
    }
    if (contains_embedded_null(location)) {
        return location_failure(ShellLocationStatus::InvalidPath,
                                ShellLocationSource::None,
                                E_INVALIDARG);
    }

    if (starts_with_case_insensitive(location, L"file:")) {
        const std::wstring terminated_location{location};
        UniqueLocalString converted;
        const HRESULT conversion = PathCreateFromUrlAlloc(
            terminated_location.c_str(), converted.put(), 0U);
        if (conversion != S_OK || converted.get() == nullptr) {
            return location_failure(ShellLocationStatus::InvalidPath,
                                    ShellLocationSource::FileUrl,
                                    conversion == S_OK ? E_POINTER : conversion);
        }
        return identify_absolute_path(converted.get(),
                                      ShellLocationSource::FileUrl);
    }

    if (!absolute_dos_or_unc(location)) {
        return location_failure(ShellLocationStatus::Unsupported,
                                ShellLocationSource::None,
                                E_INVALIDARG);
    }
    return identify_absolute_path(location, ShellLocationSource::AbsolutePath);
}

[[nodiscard]] ShellLocationFact read_location(IWebBrowser2* browser) {
    if (browser == nullptr) {
        return location_failure(ShellLocationStatus::Unavailable,
                                ShellLocationSource::None,
                                E_POINTER);
    }

    UniqueBstr location;
    const HRESULT result = browser->get_LocationURL(location.put());
    if (result == S_FALSE) {
        return location_failure(ShellLocationStatus::NavigationPending,
                                ShellLocationSource::None,
                                S_FALSE);
    }
    if (result != S_OK) {
        return location_failure(ShellLocationStatus::Unavailable,
                                ShellLocationSource::None,
                                result);
    }
    if (location.get() == nullptr || location.size() == 0U) {
        return location_failure(ShellLocationStatus::NavigationPending,
                                ShellLocationSource::None,
                                S_FALSE);
    }

    const std::wstring_view location_url{location.get(), location.size()};
    ShellLocationFact fact = parse_location(location_url);
    fact.opaque_fingerprint = opaque_location_fingerprint(location_url);
    return fact;
}

[[nodiscard]] ShellWindowHandleResult read_window_handle(
    IWebBrowser2* browser,
    const ShellAutomationStage stage) {
    ShellWindowHandleResult result;
    if (browser == nullptr) {
        result.diagnostic = ShellAutomationDiagnostic{stage, E_POINTER, -1};
        return result;
    }

    SHANDLE_PTR native_window = 0;
    const HRESULT get_result = browser->get_HWND(&native_window);
    if (get_result != S_OK) {
        result.diagnostic =
            ShellAutomationDiagnostic{stage, get_result, -1};
        return result;
    }

    result.window = reinterpret_cast<HWND>(native_window);
    if (result.window == nullptr || IsWindow(result.window) == FALSE) {
        result.window = nullptr;
        result.diagnostic =
            ShellAutomationDiagnostic{stage, kInvalidWindowHandle, -1};
    }
    return result;
}

[[nodiscard]] std::optional<ShellAutomationDiagnostic>
pump_sta_for_window_handle_readiness(
    const std::chrono::steady_clock::time_point deadline) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return std::nullopt;
    }

    const auto remaining = deadline - now;
    const auto slice = std::min(remaining,
                                std::chrono::duration_cast<
                                    std::chrono::steady_clock::duration>(
                                    kWindowHandleReadinessSlice));
    auto wait_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(slice).count();
    if (wait_milliseconds <= 0) {
        wait_milliseconds = 1;
    }

    const DWORD wait_result = MsgWaitForMultipleObjectsEx(
        0U,
        nullptr,
        static_cast<DWORD>(wait_milliseconds),
        QS_ALLINPUT,
        MWMO_INPUTAVAILABLE);
    if (wait_result == WAIT_FAILED) {
        return ShellAutomationDiagnostic{
            ShellAutomationStage::AwaitWindowHandle,
            HRESULT_FROM_WIN32(GetLastError()),
            -1};
    }
    if (wait_result != WAIT_OBJECT_0) {
        return std::nullopt;
    }

    MSG message{};
    for (std::size_t count = 0U;
         count < kMaximumMessagesPerReadinessSlice &&
         PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) != FALSE;
         ++count) {
        if (message.message == WM_QUIT) {
            PostQuitMessage(static_cast<int>(message.wParam));
            return ShellAutomationDiagnostic{
                ShellAutomationStage::AwaitWindowHandle,
                HRESULT_FROM_WIN32(ERROR_CANCELLED),
                -1};
        }
        static_cast<void>(TranslateMessage(&message));
        static_cast<void>(DispatchMessageW(&message));
    }
    return std::nullopt;
}

[[nodiscard]] ShellCallResult thread_failure() noexcept {
    return {ShellAutomationStage::ThreadAffinity, RPC_E_WRONG_THREAD};
}

} // namespace

bool same_filesystem_location(const ShellLocationFact& left,
                              const ShellLocationFact& right) noexcept {
    return left.filesystem() && right.filesystem() &&
           left.identity == right.identity;
}

const ShellWindowInventoryEntry* ShellWindowInventory::find(
    const HWND window) const noexcept {
    const auto found =
        std::find_if(windows.begin(),
                     windows.end(),
                     [window](const ShellWindowInventoryEntry& entry) {
                         return entry.window == window;
                     });
    return found == windows.end() ? nullptr : &*found;
}

ShellWindowInventory capture_shell_window_inventory() {
    ShellWindowInventory inventory;

    const HRESULT apartment = require_sta();
    if (apartment != S_OK) {
        inventory.issues.push_back({ShellAutomationStage::ApartmentValidation,
                                    apartment,
                                    -1});
        return inventory;
    }

    ComPtr<IShellWindows> shell_windows;
    const HRESULT create_result = CoCreateInstance(CLSID_ShellWindows,
                                                   nullptr,
                                                   CLSCTX_LOCAL_SERVER,
                                                   IID_PPV_ARGS(
                                                       shell_windows.put()));
    if (create_result != S_OK || !shell_windows) {
        inventory.issues.push_back(
            {ShellAutomationStage::CreateInventory,
             create_result == S_OK ? E_POINTER : create_result,
             -1});
        return inventory;
    }

    long count = 0;
    const HRESULT count_result = shell_windows->get_Count(&count);
    if (count_result != S_OK || count < 0) {
        inventory.issues.push_back(
            {ShellAutomationStage::ReadInventoryCount,
             count_result == S_OK ? E_UNEXPECTED : count_result,
             -1});
        return inventory;
    }

    inventory.reported_entry_count = count;
    inventory.complete = true;
    inventory.windows.reserve(static_cast<std::size_t>(count));

    for (long index = 0; index < count; ++index) {
        ScopedVariant item_index;
        V_VT(item_index.get()) = VT_I4;
        V_I4(item_index.get()) = index;

        ComPtr<IDispatch> dispatch;
        const HRESULT item_result =
            shell_windows->Item(item_index.value(), dispatch.put());
        if (item_result != S_OK || !dispatch) {
            inventory.complete = false;
            inventory.issues.push_back(
                {ShellAutomationStage::ReadInventoryItem,
                 item_result == S_OK ? E_POINTER : item_result,
                 index});
            continue;
        }

        ComPtr<IWebBrowser2> browser;
        const HRESULT query_result = dispatch->QueryInterface(
            IID_PPV_ARGS(browser.put()));
        if (query_result != S_OK || !browser) {
            inventory.complete = false;
            inventory.issues.push_back(
                {ShellAutomationStage::QueryWebBrowser,
                 query_result == S_OK ? E_NOINTERFACE : query_result,
                 index});
            continue;
        }

        ShellWindowHandleResult window_result = read_window_handle(
            browser.get(), ShellAutomationStage::ReadWindowHandle);
        if (!window_result.succeeded()) {
            inventory.complete = false;
            ShellAutomationDiagnostic diagnostic =
                window_result.diagnostic.value_or(ShellAutomationDiagnostic{
                    ShellAutomationStage::ReadWindowHandle,
                    kInvalidWindowHandle,
                    index});
            diagnostic.item_index = index;
            inventory.issues.push_back(diagnostic);
            continue;
        }

        auto found = std::find_if(
            inventory.windows.begin(),
            inventory.windows.end(),
            [window = window_result.window](
                const ShellWindowInventoryEntry& entry) {
                return entry.window == window;
            });
        if (found == inventory.windows.end()) {
            ShellWindowInventoryEntry entry;
            entry.window = window_result.window;
            inventory.windows.push_back(std::move(entry));
            found = std::prev(inventory.windows.end());
        }

        ++found->shell_entry_count;
        found->locations.push_back(read_location(browser.get()));
    }

    return inventory;
}

ShellLocationFact filesystem_location_identity(
    const std::filesystem::path& absolute_path) {
    const std::wstring& native_path = absolute_path.native();
    return identify_absolute_path(native_path,
                                  ShellLocationSource::AbsolutePath);
}

RetainedExplorerShellWindow::RetainedExplorerShellWindow(
    IWebBrowser2* browser,
    const HWND initial_window,
    const DWORD owner_thread_id) noexcept
    : browser_(browser),
      initial_window_(initial_window),
      owner_thread_id_(owner_thread_id) {}

RetainedExplorerShellWindow::~RetainedExplorerShellWindow() {
    if (browser_ != nullptr) {
        static_cast<void>(browser_->Release());
        browser_ = nullptr;
    }
}

HWND RetainedExplorerShellWindow::initial_hwnd() const noexcept {
    return initial_window_;
}

DWORD RetainedExplorerShellWindow::owner_thread_id() const noexcept {
    return owner_thread_id_;
}

ShellWindowHandleResult RetainedExplorerShellWindow::current_hwnd() const {
    if (!on_thread(owner_thread_id_)) {
        ShellWindowHandleResult result;
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ThreadAffinity,
            RPC_E_WRONG_THREAD,
            -1};
        return result;
    }
    return read_window_handle(browser_, ShellAutomationStage::ReadWindowHandle);
}

ShellLocationFact RetainedExplorerShellWindow::current_location() const {
    if (!on_thread(owner_thread_id_)) {
        return location_failure(ShellLocationStatus::Unavailable,
                                ShellLocationSource::None,
                                RPC_E_WRONG_THREAD);
    }
    return read_location(browser_);
}

ShellCallResult RetainedExplorerShellWindow::navigate_to_and_show(
    const std::filesystem::path& absolute_directory) {
    if (!on_thread(owner_thread_id_)) {
        return thread_failure();
    }
    if (browser_ == nullptr) {
        return {ShellAutomationStage::Navigate, E_POINTER};
    }

    const ShellLocationFact target_identity =
        filesystem_location_identity(absolute_directory);
    if (!target_identity.filesystem()) {
        return {ShellAutomationStage::OpenLocation,
                target_identity.diagnostic == S_OK
                    ? E_INVALIDARG
                    : target_identity.diagnostic};
    }

    ComPtr<IShellItem> item;
    const HRESULT item_result = SHCreateItemFromParsingName(
        absolute_directory.c_str(),
        nullptr,
        IID_PPV_ARGS(item.put()));
    if (item_result != S_OK || !item) {
        return {ShellAutomationStage::CreateTargetItem,
                item_result == S_OK ? E_POINTER : item_result};
    }

    UniquePidl pidl;
    const HRESULT pidl_result = SHGetIDListFromObject(item.get(), pidl.put());
    if (pidl_result != S_OK || pidl.get() == nullptr) {
        return {ShellAutomationStage::ReadTargetPidl,
                pidl_result == S_OK ? E_POINTER : pidl_result};
    }

    const UINT pidl_bytes = ILGetSize(pidl.get());
    if (pidl_bytes == 0U) {
        return {ShellAutomationStage::ReadTargetPidl, E_UNEXPECTED};
    }

    ScopedVariant target;
    const HRESULT encode_result =
        InitVariantFromBuffer(pidl.get(), pidl_bytes, target.get());
    if (encode_result != S_OK) {
        return {ShellAutomationStage::EncodeTargetPidl, encode_result};
    }

    ScopedVariant empty;
    const HRESULT navigation_result = browser_->Navigate2(target.get(),
                                                          empty.get(),
                                                          empty.get(),
                                                          empty.get(),
                                                          empty.get());
    if (navigation_result != S_OK) {
        return {ShellAutomationStage::Navigate, navigation_result};
    }

    const HRESULT visibility_result = browser_->put_Visible(VARIANT_TRUE);
    if (visibility_result != S_OK) {
        return {ShellAutomationStage::Show, visibility_result};
    }
    return {ShellAutomationStage::Show, S_OK};
}

ShellCallResult RetainedExplorerShellWindow::quit() {
    if (!on_thread(owner_thread_id_)) {
        return thread_failure();
    }
    if (browser_ == nullptr) {
        return {ShellAutomationStage::Quit, E_POINTER};
    }
    return {ShellAutomationStage::Quit, browser_->Quit()};
}

ExplorerShellWindowCreateResult create_explorer_shell_window(
    const std::chrono::steady_clock::time_point readiness_deadline) {
    ExplorerShellWindowCreateResult result;

    // Reject an already exhausted budget before any COM creation side effect.
    if (std::chrono::steady_clock::now() >= readiness_deadline) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::AwaitWindowHandle,
            HRESULT_FROM_WIN32(ERROR_TIMEOUT),
            -1};
        return result;
    }

    const HRESULT apartment = require_sta();
    if (apartment != S_OK) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ApartmentValidation,
            apartment,
            -1};
        return result;
    }

    ComPtr<IWebBrowser2> browser;
    const HRESULT create_result = CoCreateInstance(CLSID_ShellBrowserWindow,
                                                   nullptr,
                                                   CLSCTX_LOCAL_SERVER,
                                                   IID_PPV_ARGS(browser.put()));
    if (create_result != S_OK || !browser) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateBrowserWindow,
            create_result == S_OK ? E_POINTER : create_result,
            -1};
        return result;
    }

    // CoCreateInstance above is deliberately the only creation attempt. A
    // newly returned local-server proxy can exist before its frame HWND is
    // readable, so readiness retries get_HWND only on this exact proxy. No
    // navigation, visibility, activation, or geometry method is called here.
    ShellWindowHandleResult handle_result;
    do {
        if (std::chrono::steady_clock::now() >= readiness_deadline) {
            result.diagnostic = ShellAutomationDiagnostic{
                ShellAutomationStage::AwaitWindowHandle,
                HRESULT_FROM_WIN32(ERROR_TIMEOUT),
                -1};
            return result;
        }
        handle_result = read_window_handle(
            browser.get(), ShellAutomationStage::ReadWindowHandle);
        if (handle_result.succeeded()) {
            break;
        }
        if (std::chrono::steady_clock::now() >= readiness_deadline) {
            result.diagnostic = handle_result.diagnostic.value_or(
                ShellAutomationDiagnostic{
                    ShellAutomationStage::ReadWindowHandle,
                    kInvalidWindowHandle,
                    -1});
            return result;
        }
        if (auto pump_failure =
                pump_sta_for_window_handle_readiness(readiness_deadline);
            pump_failure.has_value()) {
            result.diagnostic = *pump_failure;
            return result;
        }
    } while (true);

    if (!handle_result.succeeded()) {
        result.diagnostic = handle_result.diagnostic.value_or(
            ShellAutomationDiagnostic{ShellAutomationStage::ReadWindowHandle,
                                      kInvalidWindowHandle,
                                      -1});
        return result;
    }

    result.window = std::unique_ptr<RetainedExplorerShellWindow>(
        new RetainedExplorerShellWindow(browser.detach(),
                                        handle_result.window,
                                        GetCurrentThreadId()));
    return result;
}

} // namespace panebind::platform::windows::explorer
