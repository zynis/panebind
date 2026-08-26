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
    CreateBrowserWindow,
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

class RetainedExplorerShellWindow;
struct ExplorerShellWindowCreateResult;
[[nodiscard]] ExplorerShellWindowCreateResult
create_explorer_shell_window(
    std::chrono::steady_clock::time_point readiness_deadline);

class RetainedExplorerShellWindow final {
public:
    ~RetainedExplorerShellWindow();

    RetainedExplorerShellWindow(const RetainedExplorerShellWindow&) = delete;
    RetainedExplorerShellWindow& operator=(
        const RetainedExplorerShellWindow&) = delete;
    RetainedExplorerShellWindow(RetainedExplorerShellWindow&&) = delete;
    RetainedExplorerShellWindow& operator=(
        RetainedExplorerShellWindow&&) = delete;

    [[nodiscard]] HWND initial_hwnd() const noexcept;
    [[nodiscard]] DWORD owner_thread_id() const noexcept;
    [[nodiscard]] ShellWindowHandleResult current_hwnd() const;
    [[nodiscard]] ShellLocationFact current_location() const;

    // The caller must prove initial_hwnd() is not in its pre-launch baseline
    // before invoking this method. Navigation is to a PIDL variant; visibility
    // is requested only after Navigate2 succeeds.
    [[nodiscard]] ShellCallResult navigate_to_and_show(
        const std::filesystem::path& absolute_directory);

    // Conditional cleanup only. This calls Quit on this exact retained object;
    // it never enumerates or closes another Shell window. Destruction of this
    // C++ wrapper merely releases the COM pointer and does not call Quit.
    [[nodiscard]] ShellCallResult quit();

private:
    RetainedExplorerShellWindow(IWebBrowser2* browser,
                                HWND initial_window,
                                DWORD owner_thread_id) noexcept;

    IWebBrowser2* browser_{};
    HWND initial_window_{};
    DWORD owner_thread_id_{};

    friend struct ExplorerShellWindowCreateResult;
    friend ExplorerShellWindowCreateResult create_explorer_shell_window(
        std::chrono::steady_clock::time_point readiness_deadline);
};

struct ExplorerShellWindowCreateResult {
    std::unique_ptr<RetainedExplorerShellWindow> window;
    std::optional<ShellAutomationDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return window != nullptr && !diagnostic.has_value();
    }
};

// Makes exactly one CLSID_ShellBrowserWindow creation attempt, retains that
// exact automation object, and waits boundedly for its frame HWND on the
// caller's STA. The wait may pump the caller's STA message queue, but it never
// creates a second object and never navigates, shows, activates, or positions
// the object. The caller must compare the returned HWND with its baseline
// before any navigation.
[[nodiscard]] ExplorerShellWindowCreateResult
create_explorer_shell_window(
    std::chrono::steady_clock::time_point readiness_deadline);

} // namespace panebind::platform::windows::explorer
