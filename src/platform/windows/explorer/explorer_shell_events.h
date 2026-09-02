#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/windows/explorer/explorer_shell_inventory.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct IConnectionPoint;
struct IDispatch;
struct IShellWindows;
struct IUnknown;

namespace panebind::platform::windows::explorer {

class ExplorerProvisioningLease;
class ShellWindowEventSink;
class ShellSubscriptionLifecycleState;
struct ShellWindowsSubscriptionCreateResult;

enum class ShellWindowReceiptKind {
    Registered,
    Revoked,
};

struct ShellWindowReceipt {
    ShellWindowReceiptKind kind{ShellWindowReceiptKind::Registered};
    long cookie{};
    std::uint64_t subscription_generation{};
    std::uint64_t sequence{};
};

struct ShellWindowReceiptFacts {
    std::uint64_t subscription_generation{};
    std::uint64_t latest_sequence{};
    std::uint64_t callback_count{};
    std::uint64_t malformed_count{};
    std::uint64_t overflow_count{};
    std::uint64_t wrong_thread_count{};
    std::uint64_t post_retirement_count{};
    std::size_t queued_count{};
    bool accepting{};
    bool subscribed{};
    bool unadvised{};
};

class ResolvedShellWindow final {
public:
    ResolvedShellWindow() noexcept = default;
    ~ResolvedShellWindow();

    ResolvedShellWindow(const ResolvedShellWindow&) = delete;
    ResolvedShellWindow& operator=(const ResolvedShellWindow&) = delete;
    ResolvedShellWindow(ResolvedShellWindow&& other) noexcept;
    ResolvedShellWindow& operator=(ResolvedShellWindow&& other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] HWND window() const noexcept;
    [[nodiscard]] long cookie() const noexcept;
    [[nodiscard]] IDispatch* dispatch() const noexcept;

    // Equality is canonical-IUnknown equality, never equality of an arbitrary
    // automation interface pointer.
    [[nodiscard]] bool same_object(
        const ExplorerProvisioningLease& lease) const noexcept;

private:
    ResolvedShellWindow(IDispatch* dispatch,
                        IUnknown* canonical_identity,
                        HWND window,
                        long cookie,
                        DWORD owner_thread_id) noexcept;
    void reset() noexcept;

    IDispatch* dispatch_{};
    IUnknown* canonical_identity_{};
    HWND window_{};
    long cookie_{};
    DWORD owner_thread_id_{};

    friend class ShellWindowsSubscription;
    friend class ExplorerProvisioningLease;
};

struct ShellResolveResult {
    ResolvedShellWindow resolved;
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return static_cast<bool>(resolved) && !diagnostic.has_value();
    }
};

enum class ShellMessagePumpStatus {
    ActivityObserved,
    TimedOut,
    QuitObserved,
    MessageBudgetExhausted,
    Failed,
    WrongThread,
};

struct ShellMessagePumpResult {
    ShellMessagePumpStatus status{ShellMessagePumpStatus::Failed};
    std::uint64_t latest_sequence{};
    HRESULT diagnostic{S_OK};
    std::size_t dispatched_message_count{};
};

class ShellWindowsSubscription final {
public:
    ~ShellWindowsSubscription();

    ShellWindowsSubscription(const ShellWindowsSubscription&) = delete;
    ShellWindowsSubscription& operator=(const ShellWindowsSubscription&) =
        delete;
    ShellWindowsSubscription(ShellWindowsSubscription&&) = delete;
    ShellWindowsSubscription& operator=(ShellWindowsSubscription&&) = delete;

    [[nodiscard]] std::uint64_t session_authority() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] DWORD owner_thread_id() const noexcept;

    // Uses the exact IShellWindows object whose connection point was advised.
    [[nodiscard]] ShellWindowInventory capture_baseline() const;

    [[nodiscard]] ShellWindowReceiptFacts facts() const noexcept;
    // Draining allocates only on the owner STA, never in the COM callback.
    [[nodiscard]] std::vector<ShellWindowReceipt> take_receipts();

    // Resolves an event cookie through this exact IShellWindows using
    // VT_I4 + SWFO_COOKIEPASSED | SWFO_NEEDDISPATCH.
    [[nodiscard]] ShellResolveResult resolve_cookie(long cookie) const;

    // Pumps the owner STA until a receipt newer than after_sequence arrives or
    // a strict time/message budget ends. It never polls IShellWindows.
    [[nodiscard]] ShellMessagePumpResult pump_until_activity(
        std::uint64_t after_sequence,
        std::chrono::steady_clock::time_point deadline) const;

    // Must run on the owner STA before COM apartment shutdown. A successful
    // call guarantees Unadvise completed before the wrapper releases its sink.
    [[nodiscard]] HRESULT close() noexcept;

private:
    ShellWindowsSubscription(IShellWindows* shell_windows,
                             IConnectionPoint* connection_point,
                             ShellWindowEventSink* sink,
                             ShellSubscriptionLifecycleState* lifecycle_state,
                             DWORD advise_cookie,
                             std::uint64_t session_authority,
                             std::uint64_t generation,
                             DWORD owner_thread_id) noexcept;

    IShellWindows* shell_windows_{};
    IConnectionPoint* connection_point_{};
    ShellWindowEventSink* sink_{};
    ShellSubscriptionLifecycleState* lifecycle_state_{};
    DWORD advise_cookie_{};
    std::uint64_t session_authority_{};
    std::uint64_t generation_{};
    DWORD owner_thread_id_{};
    bool advised_{};
    bool unadvised_{};

    friend struct ShellWindowsSubscriptionCreateResult;
    friend ShellWindowsSubscriptionCreateResult
    subscribe_shell_windows(std::uint64_t session_authority);
};

struct ShellWindowsSubscriptionCreateResult {
    std::unique_ptr<ShellWindowsSubscription> subscription;
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return subscription != nullptr && !diagnostic.has_value();
    }
};

// Creates and advises exactly one IShellWindows object. No Explorer window is
// created, navigated, shown, positioned, or closed by this operation.
[[nodiscard]] ShellWindowsSubscriptionCreateResult subscribe_shell_windows(
    std::uint64_t session_authority);

} // namespace panebind::platform::windows::explorer
