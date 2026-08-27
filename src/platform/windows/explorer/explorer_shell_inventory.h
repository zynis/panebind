#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

struct IWebBrowser2;
struct IShellWindows;
struct IConnectionPoint;
struct IUnknown;

namespace panebind::platform::windows::explorer {

// This module is a Windows-internal Shell automation boundary. Its HWNDs are
// observations and provisioning witnesses, not PaneBind window capabilities.

enum class ShellAutomationStage {
    None,
    ApartmentValidation,
    CreateInventory,
    ReadInventoryCount,
    ReadInventoryItem,
    QueryWebBrowser,
    ReadWindowHandle,
    AwaitWindowHandle,
    ReadLocation,
    ParseLocation,
    OpenLocation,
    QueryLocationIdentity,
    SubscribeShellEvents,
    ResolveRegistrationCookie,
    QueryCanonicalIdentity,
    CreateBrowserWindow,
    SubscribeBrowserEvents,
    AwaitBrowserReadiness,
    UnsubscribeBrowserEvents,
    CreateTargetItem,
    ReadTargetPidl,
    EncodeTargetPidl,
    Navigate,
    Show,
    Quit,
    ThreadAffinity,
};

struct ShellAutomationDiagnostic {
    ShellAutomationStage stage{ShellAutomationStage::None};
    HRESULT hresult{S_OK};
    long item_index{-1};
};

enum class ShellLocationSource {
    None,
    FileUrl,
    AbsolutePath,
};

enum class ShellLocationStatus {
    Filesystem,
    Empty,
    NavigationPending,
    Unavailable,
    Unsupported,
    InvalidPath,
    NotDirectory,
    OpenFailed,
    IdentityQueryFailed,
};

struct FilesystemLocationIdentity {
    std::uint64_t volume_serial_number{};
    std::array<std::byte, 16U> file_id{};

    friend bool operator==(const FilesystemLocationIdentity&,
                           const FilesystemLocationIdentity&) = default;
};

using OpaqueLocationFingerprint = std::array<std::byte, 32U>;

struct ShellLocationFact {
    ShellLocationStatus status{ShellLocationStatus::Unavailable};
    ShellLocationSource source{ShellLocationSource::None};
    std::optional<FilesystemLocationIdentity> identity;
    // SHA-256 of the exact non-empty LocationURL UTF-16 payload. This is only
    // a path-free change-detection witness for inventory entries; it is never
    // location authority and cannot authorize an Explorer target.
    std::optional<OpaqueLocationFingerprint> opaque_fingerprint;
    HRESULT diagnostic{S_OK};

    [[nodiscard]] bool filesystem() const noexcept {
        return status == ShellLocationStatus::Filesystem &&
               identity.has_value();
    }
};

[[nodiscard]] bool same_filesystem_location(
    const ShellLocationFact& left,
    const ShellLocationFact& right) noexcept;

struct ShellWindowInventoryEntry {
    HWND window{};
    std::size_t shell_entry_count{};
    std::vector<ShellLocationFact> locations;
};

struct ShellWindowInventory {
    bool complete{};
    long reported_entry_count{-1};
    std::vector<ShellWindowInventoryEntry> windows;
    std::vector<ShellAutomationDiagnostic> issues;

    [[nodiscard]] const ShellWindowInventoryEntry* find(
        HWND window) const noexcept;
};

// The caller must already be running in a COM STA (or the main STA). This
// operation never initializes COM and never pumps messages on the caller's
// behalf.
[[nodiscard]] ShellWindowInventory capture_shell_window_inventory();

// Internal overload used by ShellWindowsSubscription so baseline capture and
// cookie resolution share the exact same IShellWindows automation object. The
// function borrows shell_windows and never releases it.
[[nodiscard]] ShellWindowInventory capture_shell_window_inventory(
    IShellWindows* shell_windows);

// Produces the same path-free file identity used for Shell LocationURL facts.
// Only absolute DOS/UNC filesystem paths are accepted.
[[nodiscard]] ShellLocationFact filesystem_location_identity(
    const std::filesystem::path& absolute_path);

struct ShellCallResult {
    ShellAutomationStage stage{ShellAutomationStage::None};
    HRESULT hresult{S_OK};

    [[nodiscard]] bool succeeded() const noexcept {
        return hresult == S_OK;
    }
};

struct ShellWindowHandleResult {
    HWND window{};
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return window != nullptr && !diagnostic.has_value();
    }
};

class ResolvedShellWindow;
class BrowserReadinessEventSink;
class BrowserSubscriptionLifecycleState;
struct ExplorerProvisioningLeaseCreateResult;

struct BrowserReadinessFacts {
    std::uint64_t callback_sequence{};
    std::uint64_t latest_sequence{};
    std::uint64_t navigate_complete_count{};
    std::uint64_t matching_navigate_complete_count{};
    std::uint64_t unrelated_navigate_complete_count{};
    std::uint64_t identity_query_failure_count{};
    std::uint64_t quit_count{};
    std::uint64_t malformed_count{};
    std::uint64_t overflow_count{};
    std::uint64_t wrong_thread_count{};
    std::uint64_t post_retirement_count{};
    bool latest_activity_was_quit{};
    bool accepting{};
    bool subscribed{};
    bool unadvised{};
    HRESULT subscription_diagnostic{S_OK};
};

enum class BrowserReadinessWaitStatus {
    ActivityObserved,
    TimedOut,
    QuitObserved,
    MessageBudgetExhausted,
    Failed,
    WrongThread,
    SubscriptionUnavailable,
};

struct BrowserReadinessWaitResult {
    BrowserReadinessWaitStatus status{
        BrowserReadinessWaitStatus::SubscriptionUnavailable};
    std::uint64_t latest_sequence{};
    HRESULT diagnostic{S_OK};
    std::size_t dispatched_message_count{};
};

enum class ExplorerConsentTargetBindReason {
    Succeeded,
    InvalidArgument,
    ApartmentUnavailable,
    InventoryUnavailable,
    TargetNotFound,
    AmbiguousCandidate,
    SharedWindow,
    CanonicalIdentityUnavailable,
    BrowserEventSubscriptionUnavailable,
    CandidateChangedDuringBinding,
};

struct ShellWindowKeyResult {
    std::uintptr_t window_key{};
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return window_key != 0U && !diagnostic.has_value();
    }
};

struct ExplorerConsentTargetObservationFacts {
    std::uintptr_t bound_window_key{};
    std::size_t exact_location_match_count{};
    std::size_t bound_window_entry_count{};
    std::uint64_t navigation_epoch_at_binding{};
    BrowserReadinessFacts browser;
    bool canonical_identity_matches{};

    [[nodiscard]] bool navigation_changed_since_binding() const noexcept {
        return browser.matching_navigate_complete_count >
               navigation_epoch_at_binding;
    }
};

class ExplorerConsentTargetObservation;

struct ExplorerConsentTargetBindResult {
    ExplorerConsentTargetBindReason reason{
        ExplorerConsentTargetBindReason::InvalidArgument};
    std::unique_ptr<ExplorerConsentTargetObservation> observation;
    std::optional<ShellAutomationDiagnostic> diagnostic;
    std::size_t exact_location_match_count{};
    std::size_t bound_window_entry_count{};

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerConsentTargetBindReason::Succeeded &&
               observation != nullptr && !diagnostic.has_value();
    }
};

// A read-only, owner-STA observation of one user-created Explorer Shell entry.
// It retains the exact IWebBrowser2/canonical-IUnknown pair selected from
// CLSID_ShellWindows and observes DWebBrowserEvents2 navigation epochs. It has
// deliberately no Navigate, visibility, positioning, or Quit operation.
class ExplorerConsentTargetObservation final {
public:
    ~ExplorerConsentTargetObservation();

    ExplorerConsentTargetObservation(
        const ExplorerConsentTargetObservation&) = delete;
    ExplorerConsentTargetObservation& operator=(
        const ExplorerConsentTargetObservation&) = delete;
    ExplorerConsentTargetObservation(
        ExplorerConsentTargetObservation&&) = delete;
    ExplorerConsentTargetObservation& operator=(
        ExplorerConsentTargetObservation&&) = delete;

    [[nodiscard]] DWORD owner_thread_id() const noexcept;
    [[nodiscard]] std::uintptr_t bound_window_key() const noexcept;
    [[nodiscard]] const FilesystemLocationIdentity& expected_location()
        const noexcept;
    [[nodiscard]] ShellWindowKeyResult current_window_key() const;
    [[nodiscard]] ShellLocationFact current_location() const;
    [[nodiscard]] ExplorerConsentTargetObservationFacts facts() noexcept;
    [[nodiscard]] BrowserReadinessWaitResult pump_until_activity(
        std::uint64_t after_sequence,
        std::chrono::steady_clock::time_point deadline) const;

    // Retires and Unadvises only. It never navigates, hides, positions, or
    // closes the observed Explorer window.
    [[nodiscard]] HRESULT retire() noexcept;

private:
    ExplorerConsentTargetObservation(
        IWebBrowser2* browser,
        IUnknown* canonical_identity,
        IConnectionPoint* browser_connection_point,
        BrowserReadinessEventSink* browser_event_sink,
        BrowserSubscriptionLifecycleState* browser_lifecycle_state,
        DWORD browser_advise_cookie,
        std::uintptr_t bound_window_key,
        FilesystemLocationIdentity expected_location,
        std::size_t exact_location_match_count,
        std::size_t bound_window_entry_count,
        std::uint64_t navigation_epoch_at_binding,
        DWORD owner_thread_id) noexcept;

    IWebBrowser2* browser_{};
    IUnknown* canonical_identity_{};
    IConnectionPoint* browser_connection_point_{};
    BrowserReadinessEventSink* browser_event_sink_{};
    BrowserSubscriptionLifecycleState* browser_lifecycle_state_{};
    DWORD browser_advise_cookie_{};
    std::uintptr_t bound_window_key_{};
    FilesystemLocationIdentity expected_location_{};
    std::size_t exact_location_match_count_{};
    std::size_t bound_window_entry_count_{};
    std::uint64_t navigation_epoch_at_binding_{};
    DWORD owner_thread_id_{};
    bool browser_events_advised_{};
    bool browser_events_unadvised_{};

    friend ExplorerConsentTargetBindResult bind_explorer_consent_target(
        std::uintptr_t,
        const FilesystemLocationIdentity&);
};

// Enumerates CLSID_ShellWindows read-only and binds only when exactly one
// Shell entry has both expected_window_key and expected_location. A shared
// frame/tab, zero match, multiple exact-location matches, incomplete
// inventory, or event-subscription failure returns no observation.
[[nodiscard]] ExplorerConsentTargetBindResult bind_explorer_consent_target(
    std::uintptr_t expected_window_key,
    const FilesystemLocationIdentity& expected_location);

struct ExplorerProvisioningLeaseFacts {
    bool created_by_single_co_create{};
    bool navigation_requested{};
    bool navigation_succeeded{};
    bool visibility_requested{};
    bool visibility_succeeded{};
    bool identity_ambiguous{};
    bool quit_attempted{};
};

class ExplorerProvisioningLease final {
public:
    ~ExplorerProvisioningLease();

    ExplorerProvisioningLease(const ExplorerProvisioningLease&) = delete;
    ExplorerProvisioningLease& operator=(const ExplorerProvisioningLease&) =
        delete;
    ExplorerProvisioningLease(ExplorerProvisioningLease&&) = delete;
    ExplorerProvisioningLease& operator=(ExplorerProvisioningLease&&) =
        delete;

    [[nodiscard]] std::uint64_t session_authority() const noexcept;
    [[nodiscard]] std::uint64_t subscription_generation() const noexcept;
    [[nodiscard]] DWORD owner_thread_id() const noexcept;
    [[nodiscard]] const FilesystemLocationIdentity& target_identity()
        const noexcept;
    [[nodiscard]] bool same_object(
        const ResolvedShellWindow& resolved) const noexcept;
    [[nodiscard]] ExplorerProvisioningLeaseFacts facts() const noexcept;
    // Correlation code must permanently mark the lease ambiguous if more than
    // one registration claims the canonical identity. Ambiguous leases cannot
    // navigate further or invoke cleanup Quit.
    void mark_identity_ambiguous() noexcept;
    [[nodiscard]] ShellWindowHandleResult current_hwnd() const;
    [[nodiscard]] ShellLocationFact current_location() const;

    // Uses only this lease's prevalidated nonce directory. Navigate2 receives
    // a PIDL variant; visibility is requested only after Navigate2 succeeds.
    [[nodiscard]] ShellCallResult navigate_to_target_and_show();

    [[nodiscard]] BrowserReadinessFacts browser_readiness_facts()
        const noexcept;
    [[nodiscard]] BrowserReadinessWaitResult wait_for_browser_activity(
        std::uint64_t after_sequence,
        std::chrono::steady_clock::time_point deadline) const;

    // Conditional cleanup only. This calls Quit on the exact CoCreate-returned
    // automation object. Destruction merely unadvises/releases; it never Quits.
    [[nodiscard]] ShellCallResult quit();

    // Explicit owner-STA retirement for the optional DWebBrowserEvents2 sink.
    // The lease must be closed/destroyed before the matching CoUninitialize;
    // otherwise it detects the missing STA and deliberately leaks COM refs.
    [[nodiscard]] HRESULT close_browser_events() noexcept;

private:
    ExplorerProvisioningLease(
        IWebBrowser2* browser,
        IUnknown* canonical_identity,
        IConnectionPoint* browser_connection_point,
        BrowserReadinessEventSink* browser_event_sink,
        BrowserSubscriptionLifecycleState* browser_lifecycle_state,
        DWORD browser_advise_cookie,
        HRESULT browser_subscription_diagnostic,
        std::uint64_t session_authority,
        std::uint64_t subscription_generation,
        std::filesystem::path target_directory,
        FilesystemLocationIdentity target_identity,
        DWORD owner_thread_id) noexcept;

    IWebBrowser2* browser_{};
    IUnknown* canonical_identity_{};
    IConnectionPoint* browser_connection_point_{};
    BrowserReadinessEventSink* browser_event_sink_{};
    BrowserSubscriptionLifecycleState* browser_lifecycle_state_{};
    DWORD browser_advise_cookie_{};
    HRESULT browser_subscription_diagnostic_{S_OK};
    std::uint64_t session_authority_{};
    std::uint64_t subscription_generation_{};
    std::filesystem::path target_directory_;
    FilesystemLocationIdentity target_identity_{};
    DWORD owner_thread_id_{};
    bool browser_events_advised_{};
    bool browser_events_unadvised_{};
    ExplorerProvisioningLeaseFacts facts_{};

    friend class ResolvedShellWindow;
    friend struct ExplorerProvisioningLeaseCreateResult;
    friend ExplorerProvisioningLeaseCreateResult
    create_explorer_provisioning_lease(
        std::uint64_t,
        std::uint64_t,
        const std::filesystem::path&);
};

struct ExplorerProvisioningLeaseCreateResult {
    // A failed browser-event subscription deliberately returns both a
    // diagnostic and the exact CoCreate lease. succeeded() is false; the
    // caller may use only that retained lease for safe exact-object cleanup.
    std::unique_ptr<ExplorerProvisioningLease> lease;
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return lease != nullptr && !diagnostic.has_value();
    }
};

// Validates target identity before side effects, makes exactly one
// CLSID_ShellBrowserWindow CoCreate attempt, and immediately retains that
// object plus canonical IUnknown identity. It does not wait/poll for HWND,
// navigate, show, activate, position, or close a window.
[[nodiscard]] ExplorerProvisioningLeaseCreateResult
create_explorer_provisioning_lease(
    std::uint64_t session_authority,
    std::uint64_t subscription_generation,
    const std::filesystem::path& target_directory);

} // namespace panebind::platform::windows::explorer
