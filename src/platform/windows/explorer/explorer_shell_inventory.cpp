#include "platform/windows/explorer/explorer_shell_inventory.h"

#include "platform/windows/explorer/explorer_shell_events.h"

#include <bcrypt.h>
#include <objbase.h>
#include <ocidl.h>
#include <olectl.h>
#include <oleauto.h>
#include <exdisp.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace panebind::platform::windows::explorer {
namespace {

constexpr HRESULT kInvalidWindowHandle =
    HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
constexpr DISPID kNavigateComplete2Dispid = 252;
constexpr DISPID kOnQuitDispid = 253;
constexpr std::size_t kBrowserReceiptCapacity = 32U;
constexpr std::size_t kBrowserMessageDispatchBudget = 512U;

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
        return location_failure(ShellLocationStatus::Empty,
                                ShellLocationSource::None,
                                S_OK);
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
        return location_failure(ShellLocationStatus::Empty,
                                ShellLocationSource::None,
                                S_OK);
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

[[nodiscard]] DWORD bounded_wait_milliseconds(
    const std::chrono::steady_clock::time_point deadline) noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return 0U;
    }
    auto wait_milliseconds = std::chrono::duration_cast<
                                 std::chrono::milliseconds>(deadline - now)
                                 .count();
    if (wait_milliseconds <= 0) {
        wait_milliseconds = 1;
    }
    constexpr auto maximum =
        static_cast<long long>(std::numeric_limits<DWORD>::max() - 1U);
    return static_cast<DWORD>(std::min(wait_milliseconds, maximum));
}

[[nodiscard]] ShellCallResult thread_failure() noexcept {
    return {ShellAutomationStage::ThreadAffinity, RPC_E_WRONG_THREAD};
}

} // namespace

enum class BrowserReadinessReceiptKind {
    NavigateComplete,
    Quit,
};

struct BrowserReadinessReceipt {
    BrowserReadinessReceiptKind kind{
        BrowserReadinessReceiptKind::NavigateComplete};
    IDispatch* dispatch{};
    std::uint64_t callback_sequence{};
};

struct BrowserReadinessReceiptBatch {
    std::array<BrowserReadinessReceipt, kBrowserReceiptCapacity> receipts{};
    std::size_t count{};
};

class BrowserReadinessEventSink final : public IDispatch {
public:
    BrowserReadinessEventSink(IUnknown* expected_identity,
                              const DWORD owner_thread_id) noexcept
        : expected_identity_(expected_identity),
          owner_thread_id_(owner_thread_id) {
        InitializeSRWLock(&lock_);
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id,
                                             void** object) noexcept override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (InlineIsEqualGUID(interface_id, IID_IUnknown) ||
            InlineIsEqualGUID(interface_id, IID_IDispatch) ||
            InlineIsEqualGUID(interface_id, DIID_DWebBrowserEvents2)) {
            *object = static_cast<IDispatch*>(this);
            static_cast<void>(AddRef());
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return reference_count_.fetch_add(1U, std::memory_order_relaxed) + 1U;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const ULONG remaining =
            reference_count_.fetch_sub(1U, std::memory_order_acq_rel) - 1U;
        if (remaining == 0U) {
            delete this;
        }
        return remaining;
    }

    void pin() noexcept {
        reference_count_.fetch_add(1U, std::memory_order_relaxed);
    }

    void unpin() noexcept {
        static_cast<void>(Release());
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) noexcept override {
        if (count == nullptr) {
            return E_POINTER;
        }
        *count = 0U;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,
                                          LCID,
                                          ITypeInfo**) noexcept override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,
                                            LPOLESTR*,
                                            UINT,
                                            LCID,
                                            DISPID*) noexcept override {
        return DISP_E_UNKNOWNNAME;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID member,
                                     REFIID interface_id,
                                     LCID,
                                     WORD flags,
                                     DISPPARAMS* parameters,
                                     VARIANT*,
                                     EXCEPINFO*,
                                     UINT* argument_error) noexcept override {
        const bool navigate_complete = member == kNavigateComplete2Dispid;
        const bool on_quit = member == kOnQuitDispid;
        const bool valid_interface =
            InlineIsEqualGUID(interface_id, IID_NULL) != FALSE;
        const bool valid_flags = (flags & DISPATCH_METHOD) != 0U;
        const bool navigate_shape =
            navigate_complete && parameters != nullptr &&
            parameters->cArgs == 2U && parameters->cNamedArgs == 0U &&
            parameters->rgvarg != nullptr &&
            V_VT(&parameters->rgvarg[1]) == VT_DISPATCH &&
            V_DISPATCH(&parameters->rgvarg[1]) != nullptr &&
            V_VT(&parameters->rgvarg[0]) == (VT_VARIANT | VT_BYREF) &&
            V_VARIANTREF(&parameters->rgvarg[0]) != nullptr;
        const bool quit_shape =
            on_quit && parameters != nullptr && parameters->cArgs == 0U &&
            parameters->cNamedArgs == 0U;
        IDispatch* retained_dispatch = nullptr;
        if (valid_interface && valid_flags && navigate_shape) {
            retained_dispatch = V_DISPATCH(&parameters->rgvarg[1]);
            // Take the receipt reference before entering the non-recursive
            // lock. The callback performs no QI or identity comparison.
            static_cast<void>(retained_dispatch->AddRef());
        }

        AcquireSRWLockExclusive(&lock_);
        if (!accepting_) {
            ++post_retirement_count_;
            ReleaseSRWLockExclusive(&lock_);
            if (retained_dispatch != nullptr) {
                static_cast<void>(retained_dispatch->Release());
            }
            return S_OK;
        }
        if (GetCurrentThreadId() != owner_thread_id_) {
            ++wrong_thread_count_;
            ReleaseSRWLockExclusive(&lock_);
            if (retained_dispatch != nullptr) {
                static_cast<void>(retained_dispatch->Release());
            }
            return S_OK;
        }
        if ((!navigate_complete && !on_quit) || !valid_interface ||
            !valid_flags || (!navigate_shape && !quit_shape)) {
            ++malformed_count_;
            ReleaseSRWLockExclusive(&lock_);
            if (retained_dispatch != nullptr) {
                static_cast<void>(retained_dispatch->Release());
            }
            if (argument_error != nullptr) {
                *argument_error = 0U;
            }
            return (!navigate_complete && !on_quit)
                       ? DISP_E_MEMBERNOTFOUND
                       : DISP_E_TYPEMISMATCH;
        }
        if (callback_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
            ++malformed_count_;
            ReleaseSRWLockExclusive(&lock_);
            if (retained_dispatch != nullptr) {
                static_cast<void>(retained_dispatch->Release());
            }
            return DISP_E_OVERFLOW;
        }
        ++callback_sequence_;
        if (navigate_complete) {
            ++navigate_complete_count_;
        }
        if (queued_count_ < receipts_.size()) {
            BrowserReadinessReceipt receipt;
            receipt.kind = navigate_complete
                               ? BrowserReadinessReceiptKind::NavigateComplete
                               : BrowserReadinessReceiptKind::Quit;
            receipt.callback_sequence = callback_sequence_;
            if (navigate_complete) {
                receipt.dispatch = retained_dispatch;
                retained_dispatch = nullptr;
            }
            receipts_[queued_count_] = receipt;
            ++queued_count_;
        } else {
            ++overflow_count_;
        }
        ReleaseSRWLockExclusive(&lock_);
        if (retained_dispatch != nullptr) {
            static_cast<void>(retained_dispatch->Release());
        }
        return S_OK;
    }

    [[nodiscard]] BrowserReadinessFacts facts() const noexcept {
        AcquireSRWLockShared(&lock_);
        BrowserReadinessFacts result;
        result.callback_sequence = callback_sequence_;
        result.latest_sequence = latest_sequence_;
        result.navigate_complete_count = navigate_complete_count_;
        result.matching_navigate_complete_count =
            matching_navigate_complete_count_;
        result.unrelated_navigate_complete_count =
            unrelated_navigate_complete_count_;
        result.identity_query_failure_count = identity_query_failure_count_;
        result.quit_count = quit_count_;
        result.malformed_count = malformed_count_;
        result.overflow_count = overflow_count_;
        result.wrong_thread_count = wrong_thread_count_;
        result.post_retirement_count = post_retirement_count_;
        result.latest_activity_was_quit = latest_activity_was_quit_;
        result.accepting = accepting_;
        ReleaseSRWLockShared(&lock_);
        return result;
    }

    [[nodiscard]] BrowserReadinessReceiptBatch take_receipts() noexcept {
        BrowserReadinessReceiptBatch result;
        AcquireSRWLockExclusive(&lock_);
        result.count = queued_count_;
        std::copy_n(receipts_.begin(), result.count, result.receipts.begin());
        std::fill_n(receipts_.begin(), result.count,
                    BrowserReadinessReceipt{});
        queued_count_ = 0U;
        ReleaseSRWLockExclusive(&lock_);
        return result;
    }

    void process_receipts() noexcept {
        const BrowserReadinessReceiptBatch batch = take_receipts();
        for (std::size_t index = 0U; index < batch.count; ++index) {
            const BrowserReadinessReceipt& receipt = batch.receipts[index];
            if (receipt.kind == BrowserReadinessReceiptKind::Quit) {
                classify_quit(receipt.callback_sequence);
                continue;
            }

            IUnknown* event_identity = nullptr;
            const HRESULT identity_result =
                receipt.dispatch == nullptr
                    ? E_POINTER
                    : receipt.dispatch->QueryInterface(
                          IID_IUnknown,
                          reinterpret_cast<void**>(&event_identity));
            const bool identity_succeeded =
                identity_result == S_OK && event_identity != nullptr;
            const bool matching = identity_succeeded &&
                                  event_identity == expected_identity_;
            classify_navigate(receipt.callback_sequence,
                              identity_succeeded,
                              matching);
            if (event_identity != nullptr) {
                static_cast<void>(event_identity->Release());
            }
            if (receipt.dispatch != nullptr) {
                static_cast<void>(receipt.dispatch->Release());
            }
        }
    }

    void classify_navigate(const std::uint64_t callback_sequence,
                           const bool identity_query_succeeded,
                           const bool matching) noexcept {
        AcquireSRWLockExclusive(&lock_);
        if (!identity_query_succeeded) {
            ++identity_query_failure_count_;
        } else if (matching) {
            ++matching_navigate_complete_count_;
            if (callback_sequence >= latest_sequence_) {
                latest_sequence_ = callback_sequence;
                latest_activity_was_quit_ = false;
            }
        } else {
            ++unrelated_navigate_complete_count_;
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    void classify_quit(const std::uint64_t callback_sequence) noexcept {
        AcquireSRWLockExclusive(&lock_);
        ++quit_count_;
        if (callback_sequence >= latest_sequence_) {
            latest_sequence_ = callback_sequence;
            latest_activity_was_quit_ = true;
        }
        ReleaseSRWLockExclusive(&lock_);
    }

    void note_wrong_thread() noexcept {
        AcquireSRWLockExclusive(&lock_);
        ++wrong_thread_count_;
        ReleaseSRWLockExclusive(&lock_);
    }

    void retire() noexcept {
        AcquireSRWLockExclusive(&lock_);
        accepting_ = false;
        ReleaseSRWLockExclusive(&lock_);
    }

private:
    ~BrowserReadinessEventSink() {
        if (!on_thread(owner_thread_id_) || require_sta() != S_OK) {
            return;
        }
        for (std::size_t index = 0U; index < queued_count_; ++index) {
            if (receipts_[index].dispatch != nullptr) {
                static_cast<void>(receipts_[index].dispatch->Release());
            }
        }
        if (expected_identity_ != nullptr) {
            static_cast<void>(expected_identity_->Release());
            expected_identity_ = nullptr;
        }
    }

    std::atomic<ULONG> reference_count_{1U};
    IUnknown* expected_identity_{};
    mutable SRWLOCK lock_{};
    std::array<BrowserReadinessReceipt, kBrowserReceiptCapacity> receipts_{};
    std::size_t queued_count_{};
    std::uint64_t callback_sequence_{};
    std::uint64_t latest_sequence_{};
    std::uint64_t navigate_complete_count_{};
    std::uint64_t matching_navigate_complete_count_{};
    std::uint64_t unrelated_navigate_complete_count_{};
    std::uint64_t identity_query_failure_count_{};
    std::uint64_t quit_count_{};
    std::uint64_t malformed_count_{};
    std::uint64_t overflow_count_{};
    std::uint64_t wrong_thread_count_{};
    std::uint64_t post_retirement_count_{};
    bool latest_activity_was_quit_{};
    DWORD owner_thread_id_{};
    bool accepting_{true};
};

class BrowserSubscriptionLifecycleState final {
public:
    BrowserSubscriptionLifecycleState() noexcept {
        InitializeSRWLock(&lock_);
    }

    void pin() noexcept {
        reference_count_.fetch_add(1U, std::memory_order_relaxed);
    }
    void unpin() noexcept {
        if (reference_count_.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
            delete this;
        }
    }
    void begin_close(const BrowserReadinessFacts& facts) noexcept {
        AcquireSRWLockExclusive(&lock_);
        final_facts_ = facts;
        final_facts_.subscribed = true;
        final_facts_.unadvised = false;
        ReleaseSRWLockExclusive(&lock_);
    }
    void complete_close(const HRESULT result,
                        const BrowserReadinessFacts& facts) noexcept {
        AcquireSRWLockExclusive(&lock_);
        final_facts_ = facts;
        final_facts_.subscribed = true;
        final_facts_.unadvised = result == S_OK;
        close_result_ = result;
        ReleaseSRWLockExclusive(&lock_);
    }
    [[nodiscard]] BrowserReadinessFacts facts() const noexcept {
        AcquireSRWLockShared(&lock_);
        const BrowserReadinessFacts result = final_facts_;
        ReleaseSRWLockShared(&lock_);
        return result;
    }

private:
    ~BrowserSubscriptionLifecycleState() = default;

    std::atomic<ULONG> reference_count_{1U};
    mutable SRWLOCK lock_{};
    BrowserReadinessFacts final_facts_{};
    HRESULT close_result_{S_OK};
};

class BrowserReadinessEventSinkPin final {
public:
    explicit BrowserReadinessEventSinkPin(
        BrowserReadinessEventSink* sink) noexcept
        : sink_(sink) {
        if (sink_ != nullptr) {
            sink_->pin();
        }
    }
    ~BrowserReadinessEventSinkPin() {
        if (sink_ != nullptr) {
            sink_->unpin();
        }
    }
    BrowserReadinessEventSinkPin(const BrowserReadinessEventSinkPin&) = delete;
    BrowserReadinessEventSinkPin& operator=(
        const BrowserReadinessEventSinkPin&) = delete;
    [[nodiscard]] BrowserReadinessEventSink* get() const noexcept {
        return sink_;
    }

private:
    BrowserReadinessEventSink* sink_{};
};

class BrowserLifecyclePin final {
public:
    explicit BrowserLifecyclePin(
        BrowserSubscriptionLifecycleState* state) noexcept
        : state_(state) {
        if (state_ != nullptr) {
            state_->pin();
        }
    }
    ~BrowserLifecyclePin() {
        if (state_ != nullptr) {
            state_->unpin();
        }
    }
    BrowserLifecyclePin(const BrowserLifecyclePin&) = delete;
    BrowserLifecyclePin& operator=(const BrowserLifecyclePin&) = delete;
    [[nodiscard]] BrowserSubscriptionLifecycleState* get() const noexcept {
        return state_;
    }

private:
    BrowserSubscriptionLifecycleState* state_{};
};

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

    return capture_shell_window_inventory(shell_windows.get());
}

ShellWindowInventory capture_shell_window_inventory(
    IShellWindows* shell_windows) {
    ShellWindowInventory inventory;

    const HRESULT apartment = require_sta();
    if (apartment != S_OK) {
        inventory.issues.push_back({ShellAutomationStage::ApartmentValidation,
                                    apartment,
                                    -1});
        return inventory;
    }
    if (shell_windows == nullptr) {
        inventory.issues.push_back({ShellAutomationStage::CreateInventory,
                                    E_POINTER,
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

ExplorerProvisioningLease::ExplorerProvisioningLease(
    IWebBrowser2* browser,
    IUnknown* canonical_identity,
    IConnectionPoint* browser_connection_point,
    BrowserReadinessEventSink* browser_event_sink,
    BrowserSubscriptionLifecycleState* browser_lifecycle_state,
    const DWORD browser_advise_cookie,
    const HRESULT browser_subscription_diagnostic,
    const std::uint64_t session_authority,
    const std::uint64_t subscription_generation,
    std::filesystem::path target_directory,
    const FilesystemLocationIdentity target_identity,
    const DWORD owner_thread_id) noexcept
    : browser_(browser),
      canonical_identity_(canonical_identity),
      browser_connection_point_(browser_connection_point),
      browser_event_sink_(browser_event_sink),
      browser_lifecycle_state_(browser_lifecycle_state),
      browser_advise_cookie_(browser_advise_cookie),
      browser_subscription_diagnostic_(browser_subscription_diagnostic),
      session_authority_(session_authority),
      subscription_generation_(subscription_generation),
      target_directory_(std::move(target_directory)),
      target_identity_(target_identity),
      owner_thread_id_(owner_thread_id),
      browser_events_advised_(browser_connection_point != nullptr &&
                              browser_event_sink != nullptr &&
                              browser_advise_cookie != 0U) {
    facts_.created_by_single_co_create =
        browser_ != nullptr && canonical_identity_ != nullptr;
}

ExplorerProvisioningLease::~ExplorerProvisioningLease() {
    if (!on_thread(owner_thread_id_) || require_sta() != S_OK) {
        // All stored COM pointers are apartment-bound. Deliberately retain
        // them rather than performing illegal cross-apartment or post-
        // CoUninitialize Unadvise/Release calls.
        return;
    }
    const HRESULT close_result = close_browser_events();
    if (FAILED(close_result)) {
        // An actively connected sink owns no pointer back to this lease. Keep
        // all local references alive when Unadvise fails.
        return;
    }
    if (canonical_identity_ != nullptr) {
        static_cast<void>(canonical_identity_->Release());
        canonical_identity_ = nullptr;
    }
    if (browser_ != nullptr) {
        static_cast<void>(browser_->Release());
        browser_ = nullptr;
    }
    if (browser_lifecycle_state_ != nullptr) {
        browser_lifecycle_state_->unpin();
        browser_lifecycle_state_ = nullptr;
    }
}

std::uint64_t ExplorerProvisioningLease::session_authority() const noexcept {
    return session_authority_;
}

std::uint64_t
ExplorerProvisioningLease::subscription_generation() const noexcept {
    return subscription_generation_;
}

DWORD ExplorerProvisioningLease::owner_thread_id() const noexcept {
    return owner_thread_id_;
}

const FilesystemLocationIdentity&
ExplorerProvisioningLease::target_identity() const noexcept {
    return target_identity_;
}

bool ExplorerProvisioningLease::same_object(
    const ResolvedShellWindow& resolved) const noexcept {
    return canonical_identity_ != nullptr &&
           canonical_identity_ == resolved.canonical_identity_;
}

ExplorerProvisioningLeaseFacts
ExplorerProvisioningLease::facts() const noexcept {
    return facts_;
}

void ExplorerProvisioningLease::mark_identity_ambiguous() noexcept {
    if (!on_thread(owner_thread_id_)) {
        if (browser_event_sink_ != nullptr) {
            browser_event_sink_->note_wrong_thread();
        }
        return;
    }
    facts_.identity_ambiguous = true;
}

ShellWindowHandleResult ExplorerProvisioningLease::current_hwnd() const {
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

ShellLocationFact ExplorerProvisioningLease::current_location() const {
    if (!on_thread(owner_thread_id_)) {
        return location_failure(ShellLocationStatus::Unavailable,
                                ShellLocationSource::None,
                                RPC_E_WRONG_THREAD);
    }
    return read_location(browser_);
}

ShellCallResult ExplorerProvisioningLease::navigate_to_target_and_show() {
    if (!on_thread(owner_thread_id_)) {
        return thread_failure();
    }
    if (browser_ == nullptr) {
        return {ShellAutomationStage::Navigate, E_POINTER};
    }
    if (facts_.identity_ambiguous) {
        return {ShellAutomationStage::QueryCanonicalIdentity,
                HRESULT_FROM_WIN32(ERROR_INVALID_STATE)};
    }
    if (facts_.navigation_requested) {
        return {ShellAutomationStage::Navigate,
                HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)};
    }
    if (!browser_events_advised_) {
        return {ShellAutomationStage::SubscribeBrowserEvents,
                browser_subscription_diagnostic_ == S_OK
                    ? CONNECT_E_NOCONNECTION
                    : browser_subscription_diagnostic_};
    }

    const ShellLocationFact live_target_identity =
        filesystem_location_identity(target_directory_);
    if (!live_target_identity.filesystem()) {
        return {ShellAutomationStage::OpenLocation,
                live_target_identity.diagnostic == S_OK
                    ? E_INVALIDARG
                    : live_target_identity.diagnostic};
    }
    if (*live_target_identity.identity != target_identity_) {
        return {ShellAutomationStage::QueryLocationIdentity,
                HRESULT_FROM_WIN32(ERROR_FILE_INVALID)};
    }

    ComPtr<IShellItem> item;
    const HRESULT item_result = SHCreateItemFromParsingName(
        target_directory_.c_str(),
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
    facts_.navigation_requested = true;
    const HRESULT navigation_result = browser_->Navigate2(target.get(),
                                                          empty.get(),
                                                          empty.get(),
                                                          empty.get(),
                                                          empty.get());
    if (navigation_result != S_OK) {
        return {ShellAutomationStage::Navigate, navigation_result};
    }
    facts_.navigation_succeeded = true;

    facts_.visibility_requested = true;
    const HRESULT visibility_result = browser_->put_Visible(VARIANT_TRUE);
    if (visibility_result != S_OK) {
        return {ShellAutomationStage::Show, visibility_result};
    }
    facts_.visibility_succeeded = true;
    return {ShellAutomationStage::Show, S_OK};
}

BrowserReadinessFacts
ExplorerProvisioningLease::browser_readiness_facts() const noexcept {
    const bool live_sink = browser_event_sink_ != nullptr;
    BrowserReadinessFacts facts =
        live_sink
            ? browser_event_sink_->facts()
            : (browser_lifecycle_state_ == nullptr
                   ? BrowserReadinessFacts{}
                   : browser_lifecycle_state_->facts());
    if (live_sink) {
        facts.subscribed = browser_events_advised_;
        facts.unadvised = false;
    }
    facts.subscription_diagnostic = browser_subscription_diagnostic_;
    return facts;
}

BrowserReadinessWaitResult
ExplorerProvisioningLease::wait_for_browser_activity(
    const std::uint64_t after_sequence,
    const std::chrono::steady_clock::time_point deadline) const {
    BrowserReadinessWaitResult result;
    if (!on_thread(owner_thread_id_)) {
        if (browser_event_sink_ != nullptr) {
            browser_event_sink_->note_wrong_thread();
        }
        result.status = BrowserReadinessWaitStatus::WrongThread;
        result.diagnostic = RPC_E_WRONG_THREAD;
        return result;
    }
    BrowserReadinessEventSink* const borrowed_sink = browser_event_sink_;
    if (!browser_events_advised_ || borrowed_sink == nullptr) {
        result.status = BrowserReadinessWaitStatus::SubscriptionUnavailable;
        result.diagnostic = browser_subscription_diagnostic_ == S_OK
                                ? CONNECT_E_NOCONNECTION
                                : browser_subscription_diagnostic_;
        return result;
    }
    BrowserReadinessEventSinkPin sink_pin{borrowed_sink};
    BrowserReadinessEventSink* const sink = sink_pin.get();

    while (result.dispatched_message_count < kBrowserMessageDispatchBudget) {
        const HRESULT apartment_result = require_sta();
        if (apartment_result != S_OK) {
            result.status = BrowserReadinessWaitStatus::Failed;
            result.diagnostic = apartment_result;
            return result;
        }
        sink->process_receipts();
        const BrowserReadinessFacts facts = sink->facts();
        result.latest_sequence = facts.latest_sequence;
        if (result.latest_sequence > after_sequence) {
            result.status = facts.latest_activity_was_quit
                                ? BrowserReadinessWaitStatus::QuitObserved
                                : BrowserReadinessWaitStatus::ActivityObserved;
            return result;
        }

        const DWORD wait_milliseconds = bounded_wait_milliseconds(deadline);
        if (wait_milliseconds == 0U) {
            result.status = BrowserReadinessWaitStatus::TimedOut;
            result.diagnostic = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            return result;
        }
        const DWORD wait_result = MsgWaitForMultipleObjectsEx(
            0U,
            nullptr,
            wait_milliseconds,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE | MWMO_ALERTABLE);
        if (wait_result == WAIT_TIMEOUT) {
            result.status = BrowserReadinessWaitStatus::TimedOut;
            result.diagnostic = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            return result;
        }
        if (wait_result == WAIT_FAILED) {
            result.status = BrowserReadinessWaitStatus::Failed;
            result.diagnostic = HRESULT_FROM_WIN32(GetLastError());
            return result;
        }
        if (wait_result == WAIT_IO_COMPLETION) {
            continue;
        }

        MSG message{};
        while (result.dispatched_message_count <
                   kBrowserMessageDispatchBudget &&
               PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) != FALSE) {
            ++result.dispatched_message_count;
            if (message.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(message.wParam));
                result.status = BrowserReadinessWaitStatus::QuitObserved;
                result.diagnostic = HRESULT_FROM_WIN32(ERROR_CANCELLED);
                return result;
            }
            static_cast<void>(TranslateMessage(&message));
            static_cast<void>(DispatchMessageW(&message));
        }
    }

    sink->process_receipts();
    const BrowserReadinessFacts final_facts = sink->facts();
    result.latest_sequence = final_facts.latest_sequence;
    result.status = result.latest_sequence <= after_sequence
                        ? BrowserReadinessWaitStatus::MessageBudgetExhausted
                        : (final_facts.latest_activity_was_quit
                               ? BrowserReadinessWaitStatus::QuitObserved
                               : BrowserReadinessWaitStatus::ActivityObserved);
    result.diagnostic =
        result.status == BrowserReadinessWaitStatus::ActivityObserved
            ? S_OK
            : HRESULT_FROM_WIN32(ERROR_RETRY);
    return result;
}

ShellCallResult ExplorerProvisioningLease::quit() {
    if (!on_thread(owner_thread_id_)) {
        return thread_failure();
    }
    if (browser_ == nullptr) {
        return {ShellAutomationStage::Quit, E_POINTER};
    }
    if (facts_.identity_ambiguous ||
        !facts_.created_by_single_co_create) {
        return {ShellAutomationStage::Quit, E_ACCESSDENIED};
    }
    if (facts_.quit_attempted) {
        return {ShellAutomationStage::Quit,
                HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)};
    }
    facts_.quit_attempted = true;
    return {ShellAutomationStage::Quit, browser_->Quit()};
}

HRESULT ExplorerProvisioningLease::close_browser_events() noexcept {
    if (browser_events_unadvised_) {
        return S_FALSE;
    }
    if (!browser_events_advised_) {
        return S_FALSE;
    }
    if (!on_thread(owner_thread_id_)) {
        if (browser_event_sink_ != nullptr) {
            browser_event_sink_->note_wrong_thread();
        }
        return RPC_E_WRONG_THREAD;
    }
    const HRESULT apartment_result = require_sta();
    if (apartment_result != S_OK) {
        return apartment_result;
    }
    if (browser_connection_point_ == nullptr ||
        browser_event_sink_ == nullptr || browser_advise_cookie_ == 0U) {
        return E_UNEXPECTED;
    }

    BrowserLifecyclePin lifecycle_pin{browser_lifecycle_state_};
    BrowserSubscriptionLifecycleState* const lifecycle = lifecycle_pin.get();
    IConnectionPoint* const connection_point = browser_connection_point_;
    BrowserReadinessEventSink* const sink = browser_event_sink_;
    const DWORD advise_cookie = browser_advise_cookie_;
    const HRESULT subscription_diagnostic =
        browser_subscription_diagnostic_;

    // Detach before Unadvise: that COM call may dispatch arbitrary STA work,
    // including a nested close or destruction of this lease.
    browser_connection_point_ = nullptr;
    browser_event_sink_ = nullptr;
    browser_advise_cookie_ = 0U;
    browser_events_advised_ = false;
    browser_events_unadvised_ = true;

    sink->process_receipts();
    sink->retire();
    BrowserReadinessFacts starting_facts = sink->facts();
    starting_facts.subscription_diagnostic = subscription_diagnostic;
    if (lifecycle != nullptr) {
        lifecycle->begin_close(starting_facts);
    }
    const HRESULT result = connection_point->Unadvise(advise_cookie);
    BrowserReadinessFacts final_facts = sink->facts();
    final_facts.subscription_diagnostic = subscription_diagnostic;
    if (lifecycle != nullptr) {
        lifecycle->complete_close(result, final_facts);
    }
    if (result != S_OK) {
        // Retired sink and COM references intentionally remain alive when the
        // server does not acknowledge Unadvise.
        return result;
    }

    if (require_sta() != S_OK) {
        return CO_E_NOTINITIALIZED;
    }

    // Do not touch this after the reentrant call. Detached locals and the
    // lifecycle pin remain valid even if the wrapper was destroyed.
    static_cast<void>(connection_point->Release());
    static_cast<void>(sink->Release());
    return S_OK;
}

ExplorerProvisioningLeaseCreateResult
create_explorer_provisioning_lease(
    const std::uint64_t session_authority,
    const std::uint64_t subscription_generation,
    const std::filesystem::path& target_directory) {
    ExplorerProvisioningLeaseCreateResult result;

    if (session_authority == 0U || subscription_generation == 0U) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateBrowserWindow, E_INVALIDARG, -1};
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

    const ShellLocationFact target_fact =
        filesystem_location_identity(target_directory);
    if (!target_fact.filesystem()) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::OpenLocation,
            target_fact.diagnostic == S_OK ? E_INVALIDARG
                                           : target_fact.diagnostic,
            -1};
        return result;
    }

    // Complete every potentially throwing allocation before CoCreate. Once
    // the automation object exists, PaneBind must not lose exact cleanup
    // authority because a C++ wrapper allocation failed.
    std::filesystem::path retained_target_directory{target_directory};
    auto* browser_lifecycle_state =
        new (std::nothrow) BrowserSubscriptionLifecycleState();
    if (browser_lifecycle_state == nullptr) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateBrowserWindow, E_OUTOFMEMORY, -1};
        return result;
    }
    void* const lease_storage =
        ::operator new(sizeof(ExplorerProvisioningLease), std::nothrow);
    if (lease_storage == nullptr) {
        browser_lifecycle_state->unpin();
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateBrowserWindow, E_OUTOFMEMORY, -1};
        return result;
    }

    ComPtr<IWebBrowser2> browser;
    const HRESULT create_result = CoCreateInstance(CLSID_ShellBrowserWindow,
                                                   nullptr,
                                                   CLSCTX_LOCAL_SERVER,
                                                   IID_PPV_ARGS(browser.put()));
    if (create_result != S_OK || !browser) {
        ::operator delete(lease_storage);
        browser_lifecycle_state->unpin();
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateBrowserWindow,
            create_result == S_OK ? E_POINTER : create_result,
            -1};
        return result;
    }

    IUnknown* canonical_identity = nullptr;
    const HRESULT identity_result = browser->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&canonical_identity));
    if (identity_result != S_OK || canonical_identity == nullptr) {
        ::operator delete(lease_storage);
        browser_lifecycle_state->unpin();
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::QueryCanonicalIdentity,
            identity_result == S_OK ? E_NOINTERFACE : identity_result,
            -1};
        return result;
    }

    IConnectionPoint* browser_connection_point = nullptr;
    BrowserReadinessEventSink* browser_event_sink = nullptr;
    DWORD browser_advise_cookie = 0U;
    HRESULT browser_subscription_diagnostic = S_OK;
    IUnknown* readiness_identity = nullptr;

    IConnectionPointContainer* container = nullptr;
    HRESULT subscribe_result = browser->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&readiness_identity));
    if (subscribe_result == S_OK && readiness_identity != nullptr) {
        subscribe_result = browser->QueryInterface(IID_PPV_ARGS(&container));
    } else if (subscribe_result == S_OK) {
        subscribe_result = E_NOINTERFACE;
    }
    if (subscribe_result == S_OK && container != nullptr) {
        subscribe_result = container->FindConnectionPoint(
            DIID_DWebBrowserEvents2,
            &browser_connection_point);
        static_cast<void>(container->Release());
        container = nullptr;
    } else if (container != nullptr) {
        static_cast<void>(container->Release());
        container = nullptr;
    }

    if (subscribe_result == S_OK && browser_connection_point != nullptr) {
        browser_event_sink = new (std::nothrow)
            BrowserReadinessEventSink(readiness_identity,
                                      GetCurrentThreadId());
        if (browser_event_sink == nullptr) {
            subscribe_result = E_OUTOFMEMORY;
        } else {
            readiness_identity = nullptr;
            subscribe_result = browser_connection_point->Advise(
                static_cast<IDispatch*>(browser_event_sink),
                &browser_advise_cookie);
            if (subscribe_result == S_OK && browser_advise_cookie == 0U) {
                // The server reported success but supplied no usable
                // Unadvise cookie. Retire the self-contained sink before its
                // local reference is released; the server-held reference may
                // otherwise continue to receive callbacks.
                browser_event_sink->retire();
                subscribe_result = E_UNEXPECTED;
            }
        }
    } else if (subscribe_result == S_OK) {
        subscribe_result = CONNECT_E_NOCONNECTION;
    }

    if (subscribe_result != S_OK) {
        browser_subscription_diagnostic = subscribe_result;
        if (browser_event_sink != nullptr) {
            static_cast<void>(browser_event_sink->Release());
            browser_event_sink = nullptr;
        }
        if (browser_connection_point != nullptr) {
            static_cast<void>(browser_connection_point->Release());
            browser_connection_point = nullptr;
        }
        browser_advise_cookie = 0U;
    }
    if (readiness_identity != nullptr) {
        static_cast<void>(readiness_identity->Release());
        readiness_identity = nullptr;
    }

    auto* const lease = new (lease_storage) ExplorerProvisioningLease(
        browser.detach(),
        canonical_identity,
        browser_connection_point,
        browser_event_sink,
        browser_lifecycle_state,
        browser_advise_cookie,
        browser_subscription_diagnostic,
        session_authority,
        subscription_generation,
        std::move(retained_target_directory),
        *target_fact.identity,
        GetCurrentThreadId());
    result.lease.reset(lease);
    if (browser_subscription_diagnostic != S_OK) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeBrowserEvents,
            browser_subscription_diagnostic,
            -1};
    }
    return result;
}

} // namespace panebind::platform::windows::explorer
