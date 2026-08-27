#include "platform/windows/explorer/explorer_shell_events.h"

#include <objbase.h>
#include <olectl.h>
#include <oleauto.h>
#include <ocidl.h>
#include <exdisp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <new>
#include <utility>

namespace panebind::platform::windows::explorer {
namespace {

constexpr DISPID kWindowRegisteredDispid = 200;
constexpr DISPID kWindowRevokedDispid = 201;
constexpr std::size_t kReceiptCapacity = 128U;
constexpr std::size_t kMessageDispatchBudget = 512U;

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

[[nodiscard]] bool on_thread(const DWORD owner_thread_id) noexcept {
    return GetCurrentThreadId() == owner_thread_id;
}

[[nodiscard]] HRESULT release_if_present(IUnknown*& pointer) noexcept {
    if (pointer == nullptr) {
        return S_FALSE;
    }
    static_cast<void>(pointer->Release());
    pointer = nullptr;
    return S_OK;
}

[[nodiscard]] DWORD bounded_wait_milliseconds(
    const std::chrono::steady_clock::time_point deadline) noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return 0U;
    }
    auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count();
    if (milliseconds <= 0) {
        milliseconds = 1;
    }
    constexpr auto maximum =
        static_cast<long long>(std::numeric_limits<DWORD>::max() - 1U);
    return static_cast<DWORD>(std::min(milliseconds, maximum));
}

[[nodiscard]] std::optional<std::uint64_t>
next_subscription_generation() noexcept {
    static std::atomic<std::uint64_t> next{1U};
    std::uint64_t value = next.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint64_t>::max()) {
        if (next.compare_exchange_weak(value,
                                       value + 1U,
                                       std::memory_order_relaxed,
                                       std::memory_order_relaxed)) {
            return value;
        }
    }
    return std::nullopt;
}

} // namespace

class ShellWindowEventSink final : public IDispatch {
public:
    ShellWindowEventSink(const std::uint64_t generation,
                         const DWORD owner_thread_id) noexcept
        : generation_(generation), owner_thread_id_(owner_thread_id) {
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
            InlineIsEqualGUID(interface_id, DIID_DShellWindowsEvents)) {
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
        const bool registered = member == kWindowRegisteredDispid;
        const bool revoked = member == kWindowRevokedDispid;
        const bool valid_interface =
            InlineIsEqualGUID(interface_id, IID_NULL) != FALSE;
        const bool valid_flags = (flags & DISPATCH_METHOD) != 0U;
        const bool valid_shape =
            parameters != nullptr && parameters->cArgs == 1U &&
            parameters->cNamedArgs == 0U && parameters->rgvarg != nullptr &&
            V_VT(&parameters->rgvarg[0]) == VT_I4;

        AcquireSRWLockExclusive(&lock_);
        if (!accepting_) {
            ++post_retirement_count_;
            ReleaseSRWLockExclusive(&lock_);
            return S_OK;
        }
        ++callback_count_;
        if (GetCurrentThreadId() != owner_thread_id_) {
            ++wrong_thread_count_;
        }
        if ((!registered && !revoked) || !valid_interface || !valid_flags ||
            !valid_shape) {
            ++malformed_count_;
            ReleaseSRWLockExclusive(&lock_);
            if (argument_error != nullptr) {
                *argument_error = 0U;
            }
            return (!registered && !revoked) ? DISP_E_MEMBERNOTFOUND
                                             : DISP_E_TYPEMISMATCH;
        }

        if (latest_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
            ++malformed_count_;
            ReleaseSRWLockExclusive(&lock_);
            return DISP_E_OVERFLOW;
        }
        ++latest_sequence_;
        const ShellWindowReceipt receipt{
            registered ? ShellWindowReceiptKind::Registered
                       : ShellWindowReceiptKind::Revoked,
            V_I4(&parameters->rgvarg[0]),
            generation_,
            latest_sequence_};
        if (queued_count_ < receipts_.size()) {
            receipts_[queued_count_] = receipt;
            ++queued_count_;
        } else {
            ++overflow_count_;
        }
        ReleaseSRWLockExclusive(&lock_);
        return S_OK;
    }

    [[nodiscard]] ShellWindowReceiptFacts facts() const noexcept {
        AcquireSRWLockShared(&lock_);
        ShellWindowReceiptFacts result;
        result.subscription_generation = generation_;
        result.latest_sequence = latest_sequence_;
        result.callback_count = callback_count_;
        result.malformed_count = malformed_count_;
        result.overflow_count = overflow_count_;
        result.wrong_thread_count = wrong_thread_count_;
        result.post_retirement_count = post_retirement_count_;
        result.queued_count = queued_count_;
        result.accepting = accepting_;
        ReleaseSRWLockShared(&lock_);
        return result;
    }

    [[nodiscard]] std::vector<ShellWindowReceipt> take_receipts() {
        std::array<ShellWindowReceipt, kReceiptCapacity> copied{};
        std::size_t copied_count = 0U;
        AcquireSRWLockExclusive(&lock_);
        copied_count = queued_count_;
        std::copy_n(receipts_.begin(), copied_count, copied.begin());
        queued_count_ = 0U;
        ReleaseSRWLockExclusive(&lock_);
        return std::vector<ShellWindowReceipt>(
            copied.begin(),
            copied.begin() + static_cast<std::ptrdiff_t>(copied_count));
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
    ~ShellWindowEventSink() = default;

    std::atomic<ULONG> reference_count_{1U};
    mutable SRWLOCK lock_{};
    std::array<ShellWindowReceipt, kReceiptCapacity> receipts_{};
    std::size_t queued_count_{};
    std::uint64_t generation_{};
    std::uint64_t latest_sequence_{};
    std::uint64_t callback_count_{};
    std::uint64_t malformed_count_{};
    std::uint64_t overflow_count_{};
    std::uint64_t wrong_thread_count_{};
    std::uint64_t post_retirement_count_{};
    DWORD owner_thread_id_{};
    bool accepting_{true};
};

class ShellSubscriptionLifecycleState final {
public:
    ShellSubscriptionLifecycleState() noexcept {
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

    void begin_close(const ShellWindowReceiptFacts& facts) noexcept {
        AcquireSRWLockExclusive(&lock_);
        closing_ = true;
        final_facts_ = facts;
        final_facts_.subscribed = true;
        final_facts_.unadvised = false;
        ReleaseSRWLockExclusive(&lock_);
    }

    void complete_close(const HRESULT result,
                        const ShellWindowReceiptFacts& facts) noexcept {
        AcquireSRWLockExclusive(&lock_);
        closing_ = false;
        completed_ = true;
        close_result_ = result;
        final_facts_ = facts;
        final_facts_.subscribed = true;
        final_facts_.unadvised = result == S_OK;
        ReleaseSRWLockExclusive(&lock_);
    }

    [[nodiscard]] ShellWindowReceiptFacts facts() const noexcept {
        AcquireSRWLockShared(&lock_);
        const ShellWindowReceiptFacts result = final_facts_;
        ReleaseSRWLockShared(&lock_);
        return result;
    }

private:
    ~ShellSubscriptionLifecycleState() = default;

    std::atomic<ULONG> reference_count_{1U};
    mutable SRWLOCK lock_{};
    ShellWindowReceiptFacts final_facts_{};
    HRESULT close_result_{S_OK};
    bool closing_{};
    bool completed_{};
};

class ShellWindowEventSinkPin final {
public:
    explicit ShellWindowEventSinkPin(ShellWindowEventSink* sink) noexcept
        : sink_(sink) {
        if (sink_ != nullptr) {
            sink_->pin();
        }
    }
    ~ShellWindowEventSinkPin() {
        if (sink_ != nullptr) {
            sink_->unpin();
        }
    }
    ShellWindowEventSinkPin(const ShellWindowEventSinkPin&) = delete;
    ShellWindowEventSinkPin& operator=(const ShellWindowEventSinkPin&) = delete;
    [[nodiscard]] ShellWindowEventSink* get() const noexcept { return sink_; }

private:
    ShellWindowEventSink* sink_{};
};

class ShellLifecyclePin final {
public:
    explicit ShellLifecyclePin(
        ShellSubscriptionLifecycleState* state) noexcept
        : state_(state) {
        if (state_ != nullptr) {
            state_->pin();
        }
    }
    ~ShellLifecyclePin() {
        if (state_ != nullptr) {
            state_->unpin();
        }
    }
    ShellLifecyclePin(const ShellLifecyclePin&) = delete;
    ShellLifecyclePin& operator=(const ShellLifecyclePin&) = delete;
    [[nodiscard]] ShellSubscriptionLifecycleState* get() const noexcept {
        return state_;
    }

private:
    ShellSubscriptionLifecycleState* state_{};
};

ResolvedShellWindow::ResolvedShellWindow(IDispatch* dispatch,
                                         IUnknown* canonical_identity,
                                         const HWND window,
                                         const long cookie,
                                         const DWORD owner_thread_id) noexcept
    : dispatch_(dispatch),
      canonical_identity_(canonical_identity),
      window_(window),
      cookie_(cookie),
      owner_thread_id_(owner_thread_id) {}

ResolvedShellWindow::~ResolvedShellWindow() {
    reset();
}

ResolvedShellWindow::ResolvedShellWindow(ResolvedShellWindow&& other) noexcept
    : dispatch_(std::exchange(other.dispatch_, nullptr)),
      canonical_identity_(std::exchange(other.canonical_identity_, nullptr)),
      window_(std::exchange(other.window_, nullptr)),
      cookie_(std::exchange(other.cookie_, 0L)),
      owner_thread_id_(std::exchange(other.owner_thread_id_, 0U)) {}

ResolvedShellWindow& ResolvedShellWindow::operator=(
    ResolvedShellWindow&& other) noexcept {
    if (this != &other) {
        reset();
        dispatch_ = std::exchange(other.dispatch_, nullptr);
        canonical_identity_ =
            std::exchange(other.canonical_identity_, nullptr);
        window_ = std::exchange(other.window_, nullptr);
        cookie_ = std::exchange(other.cookie_, 0L);
        owner_thread_id_ = std::exchange(other.owner_thread_id_, 0U);
    }
    return *this;
}

ResolvedShellWindow::operator bool() const noexcept {
    return dispatch_ != nullptr && canonical_identity_ != nullptr &&
           window_ != nullptr;
}

HWND ResolvedShellWindow::window() const noexcept {
    return window_;
}

long ResolvedShellWindow::cookie() const noexcept {
    return cookie_;
}

IDispatch* ResolvedShellWindow::dispatch() const noexcept {
    return dispatch_;
}

bool ResolvedShellWindow::same_object(
    const ExplorerProvisioningLease& lease) const noexcept {
    return canonical_identity_ != nullptr &&
           canonical_identity_ == lease.canonical_identity_;
}

void ResolvedShellWindow::reset() noexcept {
    if (dispatch_ == nullptr && canonical_identity_ == nullptr) {
        return;
    }
    // These are STA interface pointers. A wrong-thread destruction cannot
    // safely release them; fail closed by leaking the two references rather
    // than making an illegal cross-apartment COM call.
    if (!on_thread(owner_thread_id_) || require_sta() != S_OK) {
        dispatch_ = nullptr;
        canonical_identity_ = nullptr;
        window_ = nullptr;
        return;
    }
    IUnknown* dispatch_identity = dispatch_;
    static_cast<void>(release_if_present(dispatch_identity));
    dispatch_ = nullptr;
    static_cast<void>(release_if_present(canonical_identity_));
    window_ = nullptr;
}

ShellWindowsSubscription::ShellWindowsSubscription(
    IShellWindows* shell_windows,
    IConnectionPoint* connection_point,
    ShellWindowEventSink* sink,
    ShellSubscriptionLifecycleState* lifecycle_state,
    const DWORD advise_cookie,
    const std::uint64_t session_authority,
    const std::uint64_t generation,
    const DWORD owner_thread_id) noexcept
    : shell_windows_(shell_windows),
      connection_point_(connection_point),
      sink_(sink),
      lifecycle_state_(lifecycle_state),
      advise_cookie_(advise_cookie),
      session_authority_(session_authority),
      generation_(generation),
      owner_thread_id_(owner_thread_id),
      advised_(true) {}

ShellWindowsSubscription::~ShellWindowsSubscription() {
    static_cast<void>(close());
    if (lifecycle_state_ != nullptr) {
        lifecycle_state_->unpin();
        lifecycle_state_ = nullptr;
    }
}

std::uint64_t ShellWindowsSubscription::session_authority() const noexcept {
    return session_authority_;
}

std::uint64_t ShellWindowsSubscription::generation() const noexcept {
    return generation_;
}

DWORD ShellWindowsSubscription::owner_thread_id() const noexcept {
    return owner_thread_id_;
}

ShellWindowInventory ShellWindowsSubscription::capture_baseline() const {
    if (!on_thread(owner_thread_id_)) {
        if (sink_ != nullptr) {
            sink_->note_wrong_thread();
        }
        ShellWindowInventory result;
        result.issues.push_back({ShellAutomationStage::ThreadAffinity,
                                 RPC_E_WRONG_THREAD,
                                 -1});
        return result;
    }
    if (shell_windows_ == nullptr || !advised_) {
        ShellWindowInventory result;
        result.issues.push_back({ShellAutomationStage::CreateInventory,
                                 E_UNEXPECTED,
                                 -1});
        return result;
    }
    return capture_shell_window_inventory(shell_windows_);
}

ShellWindowReceiptFacts ShellWindowsSubscription::facts() const noexcept {
    const bool live_sink = sink_ != nullptr;
    ShellWindowReceiptFacts result = live_sink
                                         ? sink_->facts()
                                         : (lifecycle_state_ == nullptr
                                                ? ShellWindowReceiptFacts{}
                                                : lifecycle_state_->facts());
    if (live_sink) {
        result.subscribed = advised_;
        result.unadvised = false;
    }
    return result;
}

std::vector<ShellWindowReceipt> ShellWindowsSubscription::take_receipts() {
    if (!on_thread(owner_thread_id_)) {
        if (sink_ != nullptr) {
            sink_->note_wrong_thread();
        }
        return {};
    }
    return sink_ == nullptr ? std::vector<ShellWindowReceipt>{}
                            : sink_->take_receipts();
}

ShellResolveResult ShellWindowsSubscription::resolve_cookie(
    const long cookie) const {
    ShellResolveResult result;
    if (!on_thread(owner_thread_id_)) {
        if (sink_ != nullptr) {
            sink_->note_wrong_thread();
        }
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ThreadAffinity, RPC_E_WRONG_THREAD, -1};
        return result;
    }
    if (shell_windows_ == nullptr || !advised_) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ResolveRegistrationCookie,
            E_UNEXPECTED,
            -1};
        return result;
    }

    VARIANT location;
    VARIANT root;
    VariantInit(&location);
    VariantInit(&root);
    V_VT(&location) = VT_I4;
    V_I4(&location) = cookie;

    long native_window = 0L;
    IDispatch* dispatch = nullptr;
    const HRESULT find_result = shell_windows_->FindWindowSW(
        &location,
        &root,
        SWC_EXPLORER,
        &native_window,
        SWFO_COOKIEPASSED | SWFO_NEEDDISPATCH,
        &dispatch);
    static_cast<void>(VariantClear(&root));
    static_cast<void>(VariantClear(&location));
    if (find_result != S_OK || dispatch == nullptr) {
        if (dispatch != nullptr) {
            static_cast<void>(dispatch->Release());
        }
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ResolveRegistrationCookie,
            find_result == S_OK ? E_POINTER : find_result,
            -1};
        return result;
    }

    IUnknown* canonical_identity = nullptr;
    const HRESULT identity_result = dispatch->QueryInterface(
        IID_IUnknown,
        reinterpret_cast<void**>(&canonical_identity));
    if (identity_result != S_OK || canonical_identity == nullptr) {
        static_cast<void>(dispatch->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::QueryCanonicalIdentity,
            identity_result == S_OK ? E_NOINTERFACE : identity_result,
            -1};
        return result;
    }

    const HWND window =
        reinterpret_cast<HWND>(LongToHandle(native_window));
    if (window == nullptr) {
        static_cast<void>(canonical_identity->Release());
        static_cast<void>(dispatch->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ResolveRegistrationCookie,
            HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE),
            -1};
        return result;
    }

    result.resolved = ResolvedShellWindow{dispatch,
                                          canonical_identity,
                                          window,
                                          cookie,
                                          owner_thread_id_};
    return result;
}

ShellMessagePumpResult ShellWindowsSubscription::pump_until_activity(
    const std::uint64_t after_sequence,
    const std::chrono::steady_clock::time_point deadline) const {
    ShellMessagePumpResult result;
    if (!on_thread(owner_thread_id_)) {
        if (sink_ != nullptr) {
            sink_->note_wrong_thread();
        }
        result.status = ShellMessagePumpStatus::WrongThread;
        result.diagnostic = RPC_E_WRONG_THREAD;
        return result;
    }
    ShellWindowEventSink* const borrowed_sink = sink_;
    if (borrowed_sink == nullptr || !advised_) {
        result.status = ShellMessagePumpStatus::Failed;
        result.diagnostic = E_UNEXPECTED;
        return result;
    }
    ShellWindowEventSinkPin sink_pin{borrowed_sink};
    ShellWindowEventSink* const sink = sink_pin.get();

    while (result.dispatched_message_count < kMessageDispatchBudget) {
        const HRESULT apartment_result = require_sta();
        if (apartment_result != S_OK) {
            result.status = ShellMessagePumpStatus::Failed;
            result.diagnostic = apartment_result;
            return result;
        }
        result.latest_sequence = sink->facts().latest_sequence;
        if (result.latest_sequence > after_sequence) {
            result.status = ShellMessagePumpStatus::ActivityObserved;
            return result;
        }

        const DWORD wait_milliseconds = bounded_wait_milliseconds(deadline);
        if (wait_milliseconds == 0U) {
            result.status = ShellMessagePumpStatus::TimedOut;
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
            result.status = ShellMessagePumpStatus::TimedOut;
            result.diagnostic = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            return result;
        }
        if (wait_result == WAIT_FAILED) {
            result.status = ShellMessagePumpStatus::Failed;
            result.diagnostic = HRESULT_FROM_WIN32(GetLastError());
            return result;
        }
        if (wait_result == WAIT_IO_COMPLETION) {
            continue;
        }

        MSG message{};
        while (result.dispatched_message_count < kMessageDispatchBudget &&
               PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) != FALSE) {
            ++result.dispatched_message_count;
            if (message.message == WM_QUIT) {
                PostQuitMessage(static_cast<int>(message.wParam));
                result.status = ShellMessagePumpStatus::QuitObserved;
                result.diagnostic = HRESULT_FROM_WIN32(ERROR_CANCELLED);
                return result;
            }
            static_cast<void>(TranslateMessage(&message));
            static_cast<void>(DispatchMessageW(&message));
        }
    }

    result.latest_sequence = sink->facts().latest_sequence;
    result.status = result.latest_sequence > after_sequence
                        ? ShellMessagePumpStatus::ActivityObserved
                        : ShellMessagePumpStatus::MessageBudgetExhausted;
    result.diagnostic = result.status == ShellMessagePumpStatus::ActivityObserved
                            ? S_OK
                            : HRESULT_FROM_WIN32(ERROR_RETRY);
    return result;
}

HRESULT ShellWindowsSubscription::close() noexcept {
    if (unadvised_) {
        return S_FALSE;
    }
    if (!on_thread(owner_thread_id_)) {
        if (sink_ != nullptr) {
            sink_->note_wrong_thread();
        }
        return RPC_E_WRONG_THREAD;
    }
    const HRESULT apartment_result = require_sta();
    if (apartment_result != S_OK) {
        return apartment_result;
    }
    if (!advised_ || connection_point_ == nullptr || sink_ == nullptr) {
        return E_UNEXPECTED;
    }

    ShellLifecyclePin lifecycle_pin{lifecycle_state_};
    ShellSubscriptionLifecycleState* const lifecycle = lifecycle_pin.get();
    IConnectionPoint* const connection_point = connection_point_;
    IShellWindows* const shell_windows = shell_windows_;
    ShellWindowEventSink* const sink = sink_;
    const DWORD advise_cookie = advise_cookie_;

    // Poison/detach the wrapper before the first reentrant COM call. A nested
    // close observes no live transaction and cannot Unadvise twice.
    connection_point_ = nullptr;
    shell_windows_ = nullptr;
    sink_ = nullptr;
    advise_cookie_ = 0U;
    advised_ = false;
    unadvised_ = true;

    sink->retire();
    if (lifecycle != nullptr) {
        lifecycle->begin_close(sink->facts());
    }
    const HRESULT unadvise_result =
        connection_point->Unadvise(advise_cookie);
    const ShellWindowReceiptFacts final_facts = sink->facts();
    if (lifecycle != nullptr) {
        lifecycle->complete_close(unadvise_result, final_facts);
    }
    if (unadvise_result != S_OK) {
        // Retain all local references. The sink is self-contained, so even a
        // server callback after this wrapper's destruction cannot access the
        // wrapper. Leaking is safer than releasing an actively advised sink.
        return unadvise_result;
    }

    if (require_sta() != S_OK) {
        // Reentrant code shut down the apartment. Detached COM references are
        // intentionally leaked rather than released after CoUninitialize.
        return CO_E_NOTINITIALIZED;
    }

    // The wrapper may have been destroyed reentrantly. Only detached locals
    // and the independently pinned lifecycle state are touched from here.
    static_cast<void>(connection_point->Release());
    static_cast<void>(shell_windows->Release());
    static_cast<void>(sink->Release());
    return S_OK;
}

ShellWindowsSubscriptionCreateResult subscribe_shell_windows(
    const std::uint64_t session_authority) {
    ShellWindowsSubscriptionCreateResult result;
    if (session_authority == 0U) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents, E_INVALIDARG, -1};
        return result;
    }
    const HRESULT apartment_result = require_sta();
    if (apartment_result != S_OK) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::ApartmentValidation,
            apartment_result,
            -1};
        return result;
    }

    IShellWindows* shell_windows = nullptr;
    const HRESULT create_result = CoCreateInstance(
        CLSID_ShellWindows,
        nullptr,
        CLSCTX_LOCAL_SERVER,
        IID_PPV_ARGS(&shell_windows));
    if (create_result != S_OK || shell_windows == nullptr) {
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::CreateInventory,
            create_result == S_OK ? E_POINTER : create_result,
            -1};
        return result;
    }

    IConnectionPointContainer* container = nullptr;
    const HRESULT container_result = shell_windows->QueryInterface(
        IID_PPV_ARGS(&container));
    if (container_result != S_OK || container == nullptr) {
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents,
            container_result == S_OK ? E_NOINTERFACE : container_result,
            -1};
        return result;
    }

    IConnectionPoint* connection_point = nullptr;
    const HRESULT point_result = container->FindConnectionPoint(
        DIID_DShellWindowsEvents,
        &connection_point);
    static_cast<void>(container->Release());
    if (point_result != S_OK || connection_point == nullptr) {
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents,
            point_result == S_OK ? CONNECT_E_NOCONNECTION : point_result,
            -1};
        return result;
    }

    const auto generation = next_subscription_generation();
    if (!generation.has_value()) {
        static_cast<void>(connection_point->Release());
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents,
            HRESULT_FROM_WIN32(ERROR_TOO_MANY_SESS),
            -1};
        return result;
    }
    auto* sink = new (std::nothrow)
        ShellWindowEventSink(*generation, GetCurrentThreadId());
    if (sink == nullptr) {
        static_cast<void>(connection_point->Release());
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents, E_OUTOFMEMORY, -1};
        return result;
    }
    auto* lifecycle_state =
        new (std::nothrow) ShellSubscriptionLifecycleState();
    if (lifecycle_state == nullptr) {
        static_cast<void>(sink->Release());
        static_cast<void>(connection_point->Release());
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents, E_OUTOFMEMORY, -1};
        return result;
    }

    void* const subscription_storage =
        ::operator new(sizeof(ShellWindowsSubscription), std::nothrow);
    if (subscription_storage == nullptr) {
        lifecycle_state->unpin();
        static_cast<void>(sink->Release());
        static_cast<void>(connection_point->Release());
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents, E_OUTOFMEMORY, -1};
        return result;
    }

    DWORD advise_cookie = 0U;
    const HRESULT advise_result =
        connection_point->Advise(static_cast<IDispatch*>(sink),
                                 &advise_cookie);
    if (advise_result != S_OK || advise_cookie == 0U) {
        ::operator delete(subscription_storage);
        if (advise_result == S_OK) {
            sink->retire();
        }
        lifecycle_state->unpin();
        static_cast<void>(sink->Release());
        static_cast<void>(connection_point->Release());
        static_cast<void>(shell_windows->Release());
        result.diagnostic = ShellAutomationDiagnostic{
            ShellAutomationStage::SubscribeShellEvents,
            advise_result == S_OK ? E_UNEXPECTED : advise_result,
            -1};
        return result;
    }

    auto* const subscription = new (subscription_storage)
        ShellWindowsSubscription(shell_windows,
                                 connection_point,
                                 sink,
                                 lifecycle_state,
                                 advise_cookie,
                                 session_authority,
                                 *generation,
                                 GetCurrentThreadId());
    result.subscription.reset(subscription);
    return result;
}

} // namespace panebind::platform::windows::explorer
