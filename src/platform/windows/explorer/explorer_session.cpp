#include "platform/windows/explorer/explorer_session.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/explorer/explorer_session_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <sddl.h>
#include <ShObjIdl.h>
#include <shobjidl_core.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace panebind::platform::windows::explorer {
namespace {

detail::MonotonicIdSource token_authority_ids;
detail::MonotonicIdSource operation_ids;

[[nodiscard]] const detail::InventoryFingerprint* find_inventory_window(
    const detail::InventoryModel& inventory,
    const detail::NativeWindowKey native_key) noexcept {
    const auto found = std::find_if(
        inventory.windows.begin(),
        inventory.windows.end(),
        [native_key](const detail::InventoryFingerprint& fingerprint) {
            return fingerprint.native_key == native_key;
        });
    return found == inventory.windows.end() ? nullptr : &*found;
}

[[nodiscard]] bool inventory_has_unique_nonzero_keys(
    const detail::InventoryModel& inventory) noexcept {
    std::set<detail::NativeWindowKey> keys;
    for (const auto& window : inventory.windows) {
        if (window.native_key == 0U || !keys.insert(window.native_key).second) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool inventory_location_has_witness(
    const detail::InventoryLocationFingerprint& location) noexcept {
    return location.opaque_location.has_value();
}

[[nodiscard]] std::optional<core::geometry::Rect> translate_rect(
    const core::geometry::Rect& source,
    const ExplorerTranslationDelta delta) noexcept {
    const auto left = core::geometry::checked_add(source.left(), delta.dx);
    const auto top = core::geometry::checked_add(source.top(), delta.dy);
    const auto right = core::geometry::checked_add(source.right(), delta.dx);
    const auto bottom = core::geometry::checked_add(source.bottom(), delta.dy);
    if (!left.has_value() || !top.has_value() || !right.has_value() ||
        !bottom.has_value()) {
        return std::nullopt;
    }
    return core::geometry::Rect{*left, *top, *right, *bottom};
}

[[nodiscard]] bool rect_has_positive_extent(
    const core::geometry::Rect& rect) noexcept {
    const auto width = core::geometry::checked_difference(rect.right(),
                                                           rect.left());
    const auto height = core::geometry::checked_difference(rect.bottom(),
                                                            rect.top());
    return width.has_value() && height.has_value() && *width > 0 &&
           *height > 0;
}

} // namespace

namespace detail {

MonotonicIdSource::MonotonicIdSource(const std::uint64_t first) noexcept
    : next_(first) {}

std::uint64_t MonotonicIdSource::issue() noexcept {
    auto current = next_.load(std::memory_order_relaxed);
    for (;;) {
        if (current == 0U ||
            current == std::numeric_limits<std::uint64_t>::max()) {
            return 0U;
        }
        if (next_.compare_exchange_weak(current,
                                        current + 1U,
                                        std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
            return current;
        }
    }
}

ExplorerTokenLedger::ExplorerTokenLedger() noexcept
    : controller_authority_(token_authority_ids.issue()),
      session_authority_(token_authority_ids.issue()) {}

ExplorerTokenLedger::ExplorerTokenLedger(
    const std::uint64_t controller_authority,
    const std::uint64_t session_authority,
    const std::uint64_t next_logical_id,
    const std::uint64_t next_generation) noexcept
    : controller_authority_(controller_authority),
      session_authority_(session_authority),
      next_logical_id_(next_logical_id),
      next_generation_(next_generation) {}

ExplorerLedgerIssueResult ExplorerTokenLedger::issue(
    const NativeWindowKey native_key,
    const std::uint32_t process_id,
    const std::uint32_t thread_id) {
    if (native_key == 0U) {
        return {ExplorerLedgerIssueStatus::InvalidNativeKey, std::nullopt};
    }
    if (logical_id_by_native_key_.contains(native_key)) {
        return {ExplorerLedgerIssueStatus::DuplicateNativeKey, std::nullopt};
    }
    if (controller_authority_ == 0U || session_authority_ == 0U) {
        return {ExplorerLedgerIssueStatus::AuthorityExhausted, std::nullopt};
    }
    if (next_logical_id_ == 0U ||
        next_logical_id_ == std::numeric_limits<std::uint64_t>::max() ||
        next_generation_ == 0U ||
        next_generation_ == std::numeric_limits<std::uint64_t>::max()) {
        return {ExplorerLedgerIssueStatus::GenerationExhausted, std::nullopt};
    }

    const ExplorerLedgerEntry entry{next_logical_id_,
                                    next_generation_,
                                    native_key,
                                    process_id,
                                    thread_id};
    active_by_logical_id_.emplace(entry.logical_id, entry);
    logical_id_by_native_key_.emplace(entry.native_key, entry.logical_id);
    ++next_logical_id_;
    ++next_generation_;
    return {ExplorerLedgerIssueStatus::Succeeded,
            ExplorerWindowToken{controller_authority_,
                                session_authority_,
                                entry.logical_id,
                                entry.generation}};
}

bool ExplorerTokenLedger::retire_native(
    const NativeWindowKey native_key) noexcept {
    const auto native_found = logical_id_by_native_key_.find(native_key);
    if (native_found == logical_id_by_native_key_.end()) {
        return false;
    }
    active_by_logical_id_.erase(native_found->second);
    logical_id_by_native_key_.erase(native_found);
    return true;
}

std::optional<ExplorerLedgerEntry> ExplorerTokenLedger::resolve(
    const ExplorerWindowToken& token) const noexcept {
    if (token.controller_authority_ != controller_authority_ ||
        token.session_authority_ != session_authority_) {
        return std::nullopt;
    }
    const auto found = active_by_logical_id_.find(token.logical_id_);
    if (found == active_by_logical_id_.end() ||
        found->second.generation != token.generation_) {
        return std::nullopt;
    }
    return found->second;
}

bool ExplorerTokenLedger::contains(
    const ExplorerWindowToken& token) const noexcept {
    return resolve(token).has_value();
}

ExplorerEligibilityReason evaluate_primary_apply_gate(
    const bool token_active,
    const bool allowance_consumed) noexcept {
    if (!token_active) {
        return ExplorerEligibilityReason::StaleToken;
    }
    if (allowance_consumed) {
        return ExplorerEligibilityReason::OperationLimitReached;
    }
    return ExplorerEligibilityReason::Eligible;
}

ExplorerEligibilityReason evaluate_restore_gate(
    const bool token_active,
    const bool primary_attempted,
    const bool primary_before_available,
    const bool primary_actual_available,
    const bool restore_allowance_consumed) noexcept {
    if (!token_active) {
        return ExplorerEligibilityReason::StaleToken;
    }
    if (!primary_attempted || !primary_before_available ||
        !primary_actual_available) {
        return ExplorerEligibilityReason::OperationSequenceViolation;
    }
    if (restore_allowance_consumed) {
        return ExplorerEligibilityReason::OperationLimitReached;
    }
    return ExplorerEligibilityReason::Eligible;
}

bool operation_snapshot_matches_expected(
    const ExplorerWindowSnapshot& expected,
    const ExplorerWindowSnapshot& current) noexcept {
    return expected == current;
}

CandidateEvaluation evaluate_candidate_inventory(
    const InventoryModel& baseline,
    const InventoryModel& post_navigation,
    const NativeWindowKey retained_window,
    const FilesystemLocationIdentity& target_location) noexcept {
    CandidateEvaluation result;
    if (!baseline.complete || !post_navigation.complete) {
        result.reason = ExplorerEligibilityReason::InventoryUnavailable;
        return result;
    }
    const auto fully_fingerprinted = [](const InventoryModel& inventory) {
        return std::all_of(
            inventory.windows.begin(),
            inventory.windows.end(),
            [](const InventoryFingerprint& window) {
                return window.shell_entry_count != 0U &&
                       window.locations.size() == window.shell_entry_count &&
                       std::all_of(
                           window.locations.begin(),
                           window.locations.end(),
                           [](const InventoryLocationFingerprint& location) {
                               return inventory_location_has_witness(location);
                           });
            });
    };
    if (!fully_fingerprinted(baseline)) {
        result.reason = ExplorerEligibilityReason::InventoryUnavailable;
        return result;
    }
    if (!inventory_has_unique_nonzero_keys(baseline) ||
        !inventory_has_unique_nonzero_keys(post_navigation)) {
        result.reason = ExplorerEligibilityReason::InventoryUnstable;
        return result;
    }
    if (retained_window == 0U) {
        result.reason = ExplorerEligibilityReason::ShellWindowHandleMissing;
        return result;
    }
    if (find_inventory_window(baseline, retained_window) != nullptr) {
        result.reason = ExplorerEligibilityReason::PreexistingWindow;
        return result;
    }

    result.baseline_unchanged = std::all_of(
        baseline.windows.begin(),
        baseline.windows.end(),
        [&post_navigation](const InventoryFingerprint& before) {
            const auto* after =
                find_inventory_window(post_navigation, before.native_key);
            return after != nullptr && *after == before;
        });
    if (!result.baseline_unchanged) {
        result.reason = ExplorerEligibilityReason::BaselineChanged;
        return result;
    }

    std::vector<const InventoryFingerprint*> added;
    for (const auto& after : post_navigation.windows) {
        if (find_inventory_window(baseline, after.native_key) == nullptr) {
            added.push_back(&after);
        }
    }
    result.new_candidate_count = added.size();
    if (added.empty()) {
        result.reason = ExplorerEligibilityReason::ReusedExistingWindow;
        return result;
    }
    if (added.size() != 1U) {
        result.reason = ExplorerEligibilityReason::AmbiguousCandidate;
        return result;
    }
    if (added.front()->native_key != retained_window) {
        result.reason = ExplorerEligibilityReason::ReusedExistingWindow;
        return result;
    }

    const auto& candidate = *added.front();
    if (candidate.shell_entry_count != 1U ||
        candidate.locations.size() != 1U) {
        result.reason = ExplorerEligibilityReason::AmbiguousCandidate;
        return result;
    }
    const auto& candidate_location = candidate.locations.front();
    if (candidate_location.status ==
        ShellLocationStatus::NavigationPending) {
        result.reason = ExplorerEligibilityReason::LocationNotReady;
        return result;
    }
    result.exact_unique_location =
        candidate_location.status == ShellLocationStatus::Filesystem &&
        candidate_location.filesystem_location == target_location;
    result.reason = result.exact_unique_location
                        ? ExplorerEligibilityReason::Eligible
                        : ExplorerEligibilityReason::LocationMismatch;
    return result;
}

ExplorerEligibilityReason evaluate_eligibility_model(
    const EligibilityModelFacts& facts) noexcept {
    if (!facts.window_exists) {
        return ExplorerEligibilityReason::WindowDestroyed;
    }
    if (!facts.process_alive) {
        return ExplorerEligibilityReason::ProcessExited;
    }
    if (!facts.process_id_stable) {
        return ExplorerEligibilityReason::WrongProcess;
    }
    if (!facts.thread_id_stable) {
        return ExplorerEligibilityReason::WrongThread;
    }
    if (!facts.image_matches) {
        return ExplorerEligibilityReason::WrongImage;
    }
    if (!facts.class_allowed) {
        return ExplorerEligibilityReason::WrongClass;
    }
    if (!facts.root_is_self) {
        return ExplorerEligibilityReason::NotTopLevel;
    }
    if (facts.child_style) {
        return ExplorerEligibilityReason::ChildWindow;
    }
    if (facts.has_owner) {
        return ExplorerEligibilityReason::OwnedWindow;
    }
    if (!facts.visible) {
        return ExplorerEligibilityReason::Invisible;
    }
    if (facts.cloaked) {
        return ExplorerEligibilityReason::Cloaked;
    }
    if (facts.minimized) {
        return ExplorerEligibilityReason::Minimized;
    }
    if (facts.maximized) {
        return ExplorerEligibilityReason::Maximized;
    }
    if (!facts.current_virtual_desktop) {
        return ExplorerEligibilityReason::WrongVirtualDesktop;
    }
    if (!facts.security_query_succeeded) {
        return ExplorerEligibilityReason::SecurityQueryFailed;
    }
    if (!facts.same_user) {
        return ExplorerEligibilityReason::UserMismatch;
    }
    if (!facts.same_session) {
        return ExplorerEligibilityReason::SessionMismatch;
    }
    if (!facts.same_integrity) {
        return ExplorerEligibilityReason::IntegrityMismatch;
    }
    if (!facts.medium_integrity || facts.elevated) {
        return ExplorerEligibilityReason::Elevated;
    }
    if (facts.ui_access) {
        return ExplorerEligibilityReason::UiAccess;
    }
    if (facts.app_container) {
        return ExplorerEligibilityReason::AppContainer;
    }
    if (!facts.shell_entry_unique) {
        return ExplorerEligibilityReason::AmbiguousCandidate;
    }
    if (!facts.location_exact) {
        return ExplorerEligibilityReason::LocationMismatch;
    }
    if (!facts.geometry_available) {
        return ExplorerEligibilityReason::GeometryCaptureFailed;
    }
    if (!facts.dpi_context_supported) {
        return ExplorerEligibilityReason::DpiContextMismatch;
    }
    if (!facts.monitor_available) {
        return ExplorerEligibilityReason::MonitorUnavailable;
    }
    if (!facts.monitor_stable) {
        return ExplorerEligibilityReason::MonitorChanged;
    }
    if (!facts.dpi_stable) {
        return ExplorerEligibilityReason::DpiChanged;
    }
    return ExplorerEligibilityReason::Eligible;
}

bool rect_is_contained(const core::geometry::Rect& inner,
                       const core::geometry::Rect& outer) noexcept {
    return rect_has_positive_extent(inner) && rect_has_positive_extent(outer) &&
           inner.left() >= outer.left() && inner.top() >= outer.top() &&
           inner.right() <= outer.right() &&
           inner.bottom() <= outer.bottom();
}

ExplorerSafeDeltaResult select_safe_delta_from_candidates(
    const ExplorerWindowSnapshot& snapshot,
    const std::span<const ExplorerTranslationDelta> candidates) noexcept {
    if (!rect_has_positive_extent(snapshot.visible_rect) ||
        !rect_has_positive_extent(snapshot.monitor_work_area)) {
        return {ExplorerEligibilityReason::EmptyGeometry,
                std::nullopt,
                std::nullopt};
    }
    for (const auto candidate : candidates) {
        const auto target = translate_rect(snapshot.visible_rect, candidate);
        if (target.has_value() &&
            rect_is_contained(*target, snapshot.monitor_work_area)) {
            return {ExplorerEligibilityReason::Eligible, candidate, *target};
        }
    }
    return {ExplorerEligibilityReason::UnsafeDelta,
            std::nullopt,
            std::nullopt};
}

ExplorerEligibilityReason evaluate_post_verification(
    const PostVerificationModel& verification) noexcept {
    if (!verification.token_identity_stable ||
        !verification.process_identity_stable ||
        !verification.thread_identity_stable ||
        !verification.image_and_class_stable ||
        !verification.security_stable) {
        return ExplorerEligibilityReason::TargetInvalidated;
    }
    if (!verification.location_stable) {
        return ExplorerEligibilityReason::LocationMismatch;
    }
    if (!verification.monitor_stable) {
        return ExplorerEligibilityReason::MonitorChanged;
    }
    if (!verification.dpi_stable) {
        return ExplorerEligibilityReason::DpiChanged;
    }
    if (!verification.size_preserved ||
        !verification.visible_target_matches ||
        !verification.positioning_target_matches) {
        return ExplorerEligibilityReason::PostVerificationFailed;
    }
    return ExplorerEligibilityReason::Eligible;
}

ExplorerEligibilityReason map_translation_status(
    const operations::window_translation::TranslationPreparationStatus status)
    noexcept {
    using Status =
        operations::window_translation::TranslationPreparationStatus;
    switch (status) {
    case Status::Succeeded:
        return ExplorerEligibilityReason::Eligible;
    case Status::EmptyGeometry:
        return ExplorerEligibilityReason::EmptyGeometry;
    case Status::ResizeRejected:
        return ExplorerEligibilityReason::ResizeRejected;
    case Status::ArithmeticOverflow:
        return ExplorerEligibilityReason::ArithmeticOverflow;
    case Status::NativeCoordinateOutOfRange:
        return ExplorerEligibilityReason::NativeCoordinateOutOfRange;
    }
    return ExplorerEligibilityReason::ArithmeticOverflow;
}

ExplorerEligibilityReason map_shell_creation_stage(
    const ShellAutomationStage stage) noexcept {
    switch (stage) {
    case ShellAutomationStage::ApartmentValidation:
    case ShellAutomationStage::ThreadAffinity:
        return ExplorerEligibilityReason::ComApartmentUnavailable;
    case ShellAutomationStage::ReadWindowHandle:
    case ShellAutomationStage::AwaitWindowHandle:
        return ExplorerEligibilityReason::ShellWindowHandleMissing;
    default:
        return ExplorerEligibilityReason::ShellWindowCreationFailed;
    }
}

} // namespace detail

ExplorerSafeDeltaResult select_safe_test_delta(
    const ExplorerWindowSnapshot& snapshot) noexcept {
    constexpr std::array candidates{
        ExplorerTranslationDelta{80, 50},
        ExplorerTranslationDelta{-80, -50},
        ExplorerTranslationDelta{50, -50},
        ExplorerTranslationDelta{-50, 50},
    };
    return detail::select_safe_delta_from_candidates(snapshot, candidates);
}

namespace {

class UniqueHandle final {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) noexcept : value_(value) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept
        : value_(std::exchange(other.value_, nullptr)) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.value_, nullptr));
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

    void reset(HANDLE replacement = nullptr) noexcept {
        if (valid()) {
            static_cast<void>(CloseHandle(value_));
        }
        value_ = replacement;
    }

private:
    HANDLE value_{};
};

[[nodiscard]] ExplorerDiagnostic adapter_diagnostic(
    const std::uint64_t code,
    std::string api,
    std::string detail) {
    return {ExplorerDiagnosticDomain::Adapter,
            code,
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] ExplorerDiagnostic win32_diagnostic(
    std::string api,
    const DWORD code,
    std::string detail) {
    return {ExplorerDiagnosticDomain::Win32,
            static_cast<std::uint64_t>(code),
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] ExplorerDiagnostic hresult_diagnostic(
    std::string api,
    const HRESULT result,
    std::string detail) {
    return {ExplorerDiagnosticDomain::HResult,
            static_cast<std::uint64_t>(static_cast<std::uint32_t>(result)),
            std::move(api),
             std::move(detail)};
}

[[nodiscard]] constexpr std::string_view shell_stage_name(
    const ShellAutomationStage stage) noexcept {
    using enum ShellAutomationStage;
    switch (stage) {
    case None:
        return "none";
    case ApartmentValidation:
        return "apartment_validation";
    case CreateInventory:
        return "create_inventory";
    case ReadInventoryCount:
        return "read_inventory_count";
    case ReadInventoryItem:
        return "read_inventory_item";
    case QueryWebBrowser:
        return "query_web_browser";
    case ReadWindowHandle:
        return "read_window_handle";
    case AwaitWindowHandle:
        return "await_window_handle";
    case ReadLocation:
        return "read_location";
    case ParseLocation:
        return "parse_location";
    case OpenLocation:
        return "open_location";
    case QueryLocationIdentity:
        return "query_location_identity";
    case CreateBrowserWindow:
        return "create_browser_window";
    case CreateTargetItem:
        return "create_target_item";
    case ReadTargetPidl:
        return "read_target_pidl";
    case EncodeTargetPidl:
        return "encode_target_pidl";
    case Navigate:
        return "navigate";
    case Show:
        return "show";
    case Quit:
        return "quit";
    case ThreadAffinity:
        return "thread_affinity";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view shell_api_name(
    const ShellAutomationStage stage) noexcept {
    using enum ShellAutomationStage;
    switch (stage) {
    case None:
        return "none";
    case ApartmentValidation:
        return "CoGetApartmentType";
    case CreateInventory:
        return "CoCreateInstance(CLSID_ShellWindows)";
    case ReadInventoryCount:
        return "IShellWindows::get_Count";
    case ReadInventoryItem:
        return "IShellWindows::Item";
    case QueryWebBrowser:
        return "IUnknown::QueryInterface(IWebBrowser2)";
    case ReadWindowHandle:
        return "IWebBrowser2::get_HWND";
    case AwaitWindowHandle:
        return "MsgWaitForMultipleObjectsEx";
    case ReadLocation:
        return "IWebBrowser2::get_LocationURL";
    case ParseLocation:
        return "PathCreateFromUrlW";
    case OpenLocation:
        return "CreateFileW";
    case QueryLocationIdentity:
        return "GetFileInformationByHandleEx(FileIdInfo)";
    case CreateBrowserWindow:
        return "CoCreateInstance(CLSID_ShellBrowserWindow)";
    case CreateTargetItem:
        return "SHCreateItemFromParsingName";
    case ReadTargetPidl:
        return "SHGetIDListFromObject";
    case EncodeTargetPidl:
        return "InitVariantFromBuffer";
    case Navigate:
        return "IWebBrowser2::Navigate2";
    case Show:
        return "IWebBrowser2::put_Visible";
    case Quit:
        return "IWebBrowser2::Quit";
    case ThreadAffinity:
        return "GetCurrentThreadId";
    }
    return "unknown";
}

[[nodiscard]] ExplorerDiagnostic shell_diagnostic(
    const ShellAutomationDiagnostic& diagnostic,
    std::string detail) {
    return {ExplorerDiagnosticDomain::Shell,
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(diagnostic.hresult)),
            std::string{shell_api_name(diagnostic.stage)},
            std::move(detail),
            std::string{shell_stage_name(diagnostic.stage)}};
}

[[nodiscard]] bool location_identity_less(
    const FilesystemLocationIdentity& left,
    const FilesystemLocationIdentity& right) noexcept {
    if (left.volume_serial_number != right.volume_serial_number) {
        return left.volume_serial_number < right.volume_serial_number;
    }
    return std::lexicographical_compare(left.file_id.begin(),
                                        left.file_id.end(),
                                        right.file_id.begin(),
                                        right.file_id.end());
}

[[nodiscard]] bool opaque_location_less(
    const OpaqueLocationFingerprint& left,
    const OpaqueLocationFingerprint& right) noexcept {
    return std::lexicographical_compare(left.begin(),
                                        left.end(),
                                        right.begin(),
                                        right.end());
}

[[nodiscard]] bool inventory_location_less(
    const detail::InventoryLocationFingerprint& left,
    const detail::InventoryLocationFingerprint& right) noexcept {
    if (left.status != right.status) {
        return static_cast<int>(left.status) < static_cast<int>(right.status);
    }
    if (left.source != right.source) {
        return static_cast<int>(left.source) < static_cast<int>(right.source);
    }
    if (left.filesystem_location.has_value() !=
        right.filesystem_location.has_value()) {
        return !left.filesystem_location.has_value();
    }
    if (left.filesystem_location.has_value() &&
        *left.filesystem_location != *right.filesystem_location) {
        return location_identity_less(*left.filesystem_location,
                                      *right.filesystem_location);
    }
    if (left.opaque_location.has_value() !=
        right.opaque_location.has_value()) {
        return !left.opaque_location.has_value();
    }
    return left.opaque_location.has_value() &&
           *left.opaque_location != *right.opaque_location &&
           opaque_location_less(*left.opaque_location,
                                *right.opaque_location);
}

[[nodiscard]] detail::InventoryModel inventory_model(
    const ShellWindowInventory& inventory) {
    detail::InventoryModel model;
    model.complete = inventory.complete;
    model.windows.reserve(inventory.windows.size());
    for (const auto& native_window : inventory.windows) {
        detail::InventoryFingerprint fingerprint;
        fingerprint.native_key = reinterpret_cast<detail::NativeWindowKey>(
            native_window.window);
        fingerprint.shell_entry_count = native_window.shell_entry_count;
        fingerprint.locations.reserve(native_window.locations.size());
        for (const auto& location : native_window.locations) {
            detail::InventoryLocationFingerprint location_fingerprint;
            location_fingerprint.status = location.status;
            location_fingerprint.source = location.source;
            location_fingerprint.filesystem_location = location.identity;
            location_fingerprint.opaque_location =
                location.opaque_fingerprint;
            fingerprint.locations.push_back(std::move(location_fingerprint));
        }
        std::sort(fingerprint.locations.begin(),
                  fingerprint.locations.end(),
                  inventory_location_less);
        model.windows.push_back(std::move(fingerprint));
    }
    std::sort(model.windows.begin(),
              model.windows.end(),
              [](const detail::InventoryFingerprint& left,
                 const detail::InventoryFingerprint& right) {
                  return left.native_key < right.native_key;
              });
    return model;
}

[[nodiscard]] bool inventory_navigation_pending(
    const detail::InventoryModel& inventory) noexcept {
    return std::any_of(
        inventory.windows.begin(),
        inventory.windows.end(),
        [](const detail::InventoryFingerprint& window) {
            return std::any_of(
                window.locations.begin(),
                window.locations.end(),
                [](const detail::InventoryLocationFingerprint& location) {
                    return location.status ==
                           ShellLocationStatus::NavigationPending;
                });
        });
}

[[nodiscard]] bool inventory_locations_fully_fingerprinted(
    const detail::InventoryModel& inventory) noexcept {
    return std::all_of(
        inventory.windows.begin(),
        inventory.windows.end(),
        [](const detail::InventoryFingerprint& window) {
            return window.shell_entry_count != 0U &&
                   window.locations.size() == window.shell_entry_count &&
                   std::all_of(
                       window.locations.begin(),
                       window.locations.end(),
                       [](const detail::InventoryLocationFingerprint& location) {
                           return inventory_location_has_witness(location);
                       });
        });
}

struct StableInventoryResult {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::InventoryUnavailable};
    std::optional<detail::InventoryModel> model;
    std::optional<ExplorerDiagnostic> diagnostic;
};

[[nodiscard]] StableInventoryResult capture_stable_inventory(
    const std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::optional<detail::InventoryModel> prior;
    do {
        const auto native = capture_shell_window_inventory();
        const auto current = inventory_model(native);
        if (current.complete && !inventory_navigation_pending(current) &&
            inventory_locations_fully_fingerprinted(current)) {
            if (prior.has_value() && *prior == current) {
                return {ExplorerEligibilityReason::Eligible,
                        current,
                        std::nullopt};
            }
            prior = current;
        } else {
            prior.reset();
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            std::optional<ExplorerDiagnostic> diagnostic;
            if (!native.issues.empty()) {
                diagnostic = shell_diagnostic(
                    native.issues.front(),
                    "Shell inventory never became complete and stable");
            }
            return {prior.has_value()
                        ? ExplorerEligibilityReason::InventoryUnstable
                        : ExplorerEligibilityReason::InventoryUnavailable,
                    std::nullopt,
                    std::move(diagnostic)};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{40});
    } while (true);
}

[[nodiscard]] bool ordinal_path_equal(const std::wstring& left,
                                      const std::wstring& right) noexcept {
    return CompareStringOrdinal(left.c_str(),
                                static_cast<int>(left.size()),
                                right.c_str(),
                                static_cast<int>(right.size()),
                                TRUE) == CSTR_EQUAL;
}

[[nodiscard]] std::optional<std::wstring> system_explorer_path() {
    std::vector<wchar_t> buffer(512U);
    for (;;) {
        const UINT result = GetSystemWindowsDirectoryW(
            buffer.data(), static_cast<UINT>(buffer.size()));
        if (result == 0U) {
            return std::nullopt;
        }
        if (result < buffer.size()) {
            std::filesystem::path path{
                std::wstring{buffer.data(), static_cast<std::size_t>(result)}};
            path /= L"explorer.exe";
            return path.native();
        }
        buffer.resize(static_cast<std::size_t>(result) + 1U);
    }
}

[[nodiscard]] std::optional<std::wstring> query_process_image_path(
    const HANDLE process) {
    std::vector<wchar_t> buffer(1024U);
    for (;;) {
        DWORD size = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(process, 0U, buffer.data(), &size)) {
            return std::wstring{buffer.data(), static_cast<std::size_t>(size)};
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
            buffer.size() >= 32768U) {
            return std::nullopt;
        }
        buffer.resize(std::min<std::size_t>(buffer.size() * 2U, 32768U));
    }
}

[[nodiscard]] std::optional<std::wstring> query_final_path(
    const HANDLE file) {
    std::vector<wchar_t> buffer(1024U);
    for (;;) {
        const DWORD result = GetFinalPathNameByHandleW(
            file,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (result == 0U) {
            return std::nullopt;
        }
        if (result < buffer.size()) {
            return std::wstring{buffer.data(), static_cast<std::size_t>(result)};
        }
        buffer.resize(static_cast<std::size_t>(result) + 1U);
    }
}

struct OpenFileIdentity {
    FilesystemLocationIdentity identity;
    std::wstring final_path;
};

[[nodiscard]] std::optional<OpenFileIdentity> open_file_identity(
    const std::wstring& path) {
    UniqueHandle file{CreateFileW(path.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE |
                                      FILE_SHARE_DELETE,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr)};
    if (!file.valid()) {
        return std::nullopt;
    }
    FILE_ID_INFO native_identity{};
    if (!GetFileInformationByHandleEx(file.get(),
                                      FileIdInfo,
                                      &native_identity,
                                      sizeof(native_identity))) {
        return std::nullopt;
    }
    const auto final_path = query_final_path(file.get());
    if (!final_path.has_value()) {
        return std::nullopt;
    }
    OpenFileIdentity result;
    result.identity.volume_serial_number =
        native_identity.VolumeSerialNumber;
    std::memcpy(result.identity.file_id.data(),
                native_identity.FileId.Identifier,
                result.identity.file_id.size());
    result.final_path = *final_path;
    return result;
}

struct ImageValidationResult {
    bool matched{};
    std::wstring actual_path;
    std::optional<ExplorerDiagnostic> diagnostic;
};

[[nodiscard]] ImageValidationResult validate_explorer_image(
    const HANDLE process) {
    const auto expected_path = system_explorer_path();
    if (!expected_path.has_value()) {
        return {false,
                {},
                win32_diagnostic("GetSystemWindowsDirectoryW",
                                 GetLastError(),
                                 "system Windows directory query failed")};
    }
    const auto actual_path = query_process_image_path(process);
    if (!actual_path.has_value()) {
        return {false,
                {},
                win32_diagnostic("QueryFullProcessImageNameW",
                                 GetLastError(),
                                 "target process image query failed")};
    }
    const auto expected = open_file_identity(*expected_path);
    const auto actual = open_file_identity(*actual_path);
    if (!expected.has_value() || !actual.has_value()) {
        return {false,
                *actual_path,
                win32_diagnostic("CreateFileW/GetFileInformationByHandleEx",
                                 GetLastError(),
                                 "Explorer image file identity query failed")};
    }
    const bool matched =
        ordinal_path_equal(expected->final_path, actual->final_path) &&
        expected->identity == actual->identity;
    return {matched,
            *actual_path,
            matched ? std::nullopt
                    : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                          1001U,
                          "Explorer image validation",
                          "target image is not the installed system explorer.exe")}};
}

[[nodiscard]] bool query_token_value(const HANDLE token,
                                     const TOKEN_INFORMATION_CLASS info_class,
                                     void* value,
                                     const DWORD value_size) noexcept {
    DWORD returned = 0U;
    return GetTokenInformation(token,
                               info_class,
                               value,
                               value_size,
                               &returned) != FALSE;
}

struct NativeSecurityFacts {
    ExplorerProcessSecurityFacts public_facts;
    std::vector<std::byte> user_sid;
};

[[nodiscard]] std::optional<NativeSecurityFacts> query_process_security(
    const HANDLE process) {
    HANDLE token_value = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token_value)) {
        return std::nullopt;
    }
    UniqueHandle token{token_value};

    DWORD required = 0U;
    static_cast<void>(GetTokenInformation(
        token.get(), TokenIntegrityLevel, nullptr, 0U, &required));
    if (required == 0U || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }
    std::vector<std::byte> integrity_buffer(required);
    if (!query_token_value(token.get(),
                           TokenIntegrityLevel,
                           integrity_buffer.data(),
                           required)) {
        return std::nullopt;
    }
    const auto* mandatory_label =
        reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(
            integrity_buffer.data());
    if (mandatory_label->Label.Sid == nullptr ||
        !IsValidSid(mandatory_label->Label.Sid)) {
        return std::nullopt;
    }
    const UCHAR subauthority_count =
        *GetSidSubAuthorityCount(mandatory_label->Label.Sid);
    if (subauthority_count == 0U) {
        return std::nullopt;
    }

    TOKEN_ELEVATION elevation{};
    DWORD ui_access = 0U;
    DWORD app_container = 0U;
    DWORD session_id = 0U;
    if (!query_token_value(token.get(),
                           TokenElevation,
                           &elevation,
                           sizeof(elevation)) ||
        !query_token_value(token.get(),
                           TokenUIAccess,
                           &ui_access,
                           sizeof(ui_access)) ||
        !query_token_value(token.get(),
                           TokenIsAppContainer,
                           &app_container,
                           sizeof(app_container)) ||
        !query_token_value(token.get(),
                           TokenSessionId,
                           &session_id,
                           sizeof(session_id))) {
        return std::nullopt;
    }

    required = 0U;
    static_cast<void>(
        GetTokenInformation(token.get(), TokenUser, nullptr, 0U, &required));
    if (required == 0U || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return std::nullopt;
    }
    std::vector<std::byte> user_buffer(required);
    if (!query_token_value(
            token.get(), TokenUser, user_buffer.data(), required)) {
        return std::nullopt;
    }
    const auto* user = reinterpret_cast<const TOKEN_USER*>(user_buffer.data());
    if (user->User.Sid == nullptr || !IsValidSid(user->User.Sid)) {
        return std::nullopt;
    }
    const DWORD sid_length = GetLengthSid(user->User.Sid);
    if (sid_length == 0U) {
        return std::nullopt;
    }

    NativeSecurityFacts result;
    result.public_facts.integrity_rid = *GetSidSubAuthority(
        mandatory_label->Label.Sid,
        static_cast<DWORD>(subauthority_count - 1U));
    result.public_facts.session_id = session_id;
    result.public_facts.elevated = elevation.TokenIsElevated != 0U;
    result.public_facts.ui_access = ui_access != 0U;
    result.public_facts.app_container = app_container != 0U;
    result.user_sid.resize(sid_length);
    if (!CopySid(sid_length, result.user_sid.data(), user->User.Sid)) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] bool same_user(const NativeSecurityFacts& first,
                             const NativeSecurityFacts& second) noexcept {
    return !first.user_sid.empty() && !second.user_sid.empty() &&
           EqualSid(const_cast<std::byte*>(first.user_sid.data()),
                    const_cast<std::byte*>(second.user_sid.data())) != FALSE;
}

} // namespace

struct ExplorerTestSession::Impl final {
    bool com_initialized{};
    DWORD controller_thread_id{};
    std::filesystem::path target_directory;
    FilesystemLocationIdentity target_location;
    detail::InventoryModel baseline_inventory;
    std::unique_ptr<RetainedExplorerShellWindow> retained_window;
    HWND window{};
    UniqueHandle process;
    DWORD process_id{};
    DWORD thread_id{};
    std::wstring process_image_path;
    std::wstring window_class;
    NativeSecurityFacts controller_security;
    NativeSecurityFacts target_security;
    HMONITOR initial_monitor{};
    UINT initial_dpi{};
    detail::ExplorerTokenLedger ledger;
    detail::SinglePrimaryApplyGuard primary_apply_guard;
    detail::SinglePrimaryApplyGuard restore_guard;
    std::optional<ExplorerWindowToken> issued_token;
    ExplorerProvisioningFacts provisioning;
    std::optional<ExplorerWindowSnapshot> original_snapshot;
    std::optional<ExplorerWindowSnapshot> primary_before_snapshot;
    std::optional<ExplorerWindowSnapshot> primary_actual_snapshot;
    bool closing{};

    ~Impl() {
        const bool on_owner_sta =
            GetCurrentThreadId() == controller_thread_id;
        if (on_owner_sta) {
            retained_window.reset();
        } else {
            // IWebBrowser2 is an STA proxy. Calling Release (or Quit) from an
            // arbitrary destructor thread is less safe than leaking this one
            // test-fixture proxy. The public contract requires owner-STA
            // destruction; this branch is the fail-safe violation path.
            static_cast<void>(retained_window.release());
        }
        process.reset();
        if (com_initialized && on_owner_sta) {
            CoUninitialize();
        }
    }
};

namespace {

struct NativeValidationResult {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::TargetInvalidated};
    std::optional<ExplorerWindowSnapshot> snapshot;
    std::optional<ExplorerDiagnostic> diagnostic;
};

[[nodiscard]] std::optional<core::geometry::Rect> checked_native_rect(
    const RECT& rect) noexcept {
    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return std::nullopt;
    }
    return core::geometry::Rect{rect.left, rect.top, rect.right, rect.bottom};
}

[[nodiscard]] std::optional<std::wstring> query_window_class(
    const HWND window) {
    std::array<wchar_t, 256U> buffer{};
    const int length = GetClassNameW(
        window, buffer.data(), static_cast<int>(buffer.size()));
    if (length <= 0 || static_cast<std::size_t>(length) >= buffer.size() - 1U) {
        return std::nullopt;
    }
    return std::wstring{buffer.data(), static_cast<std::size_t>(length)};
}

[[nodiscard]] bool allowed_explorer_class(
    const std::wstring& class_name) noexcept {
    // These are a fail-closed current-Windows allowlist, not a claim that
    // Microsoft documents the class strings as a stable compatibility API.
    return class_name == L"CabinetWClass" || class_name == L"ExploreWClass";
}

[[nodiscard]] bool exact_shell_location(
    const ShellWindowInventory& inventory,
    const HWND window,
    const FilesystemLocationIdentity& target) noexcept {
    if (!inventory.complete) {
        return false;
    }
    const auto* entry = inventory.find(window);
    return entry != nullptr && entry->shell_entry_count == 1U &&
           entry->locations.size() == 1U &&
           entry->locations.front().filesystem() &&
           *entry->locations.front().identity == target;
}

[[nodiscard]] bool same_snapshot_size(
    const core::geometry::Rect& first,
    const core::geometry::Rect& second) noexcept {
    const auto first_width = core::geometry::checked_difference(first.right(),
                                                                 first.left());
    const auto first_height = core::geometry::checked_difference(first.bottom(),
                                                                  first.top());
    const auto second_width = core::geometry::checked_difference(second.right(),
                                                                  second.left());
    const auto second_height = core::geometry::checked_difference(second.bottom(),
                                                                   second.top());
    return first_width.has_value() && first_height.has_value() &&
           second_width.has_value() && second_height.has_value() &&
           *first_width == *second_width && *first_height == *second_height;
}

template <typename ImplType>
[[nodiscard]] NativeValidationResult validate_native_target(
    ImplType& impl,
    const ExplorerWindowToken* token,
    const bool initialize_anchor) {
    if (GetCurrentThreadId() != impl.controller_thread_id) {
        return {ExplorerEligibilityReason::WrongThread,
                std::nullopt,
                adapter_diagnostic(1100U,
                                   "ExplorerTestSession",
                                   "session used outside its provisioning STA")};
    }
    if (token != nullptr && !impl.ledger.contains(*token)) {
        return {ExplorerEligibilityReason::StaleToken,
                std::nullopt,
                adapter_diagnostic(1101U,
                                   "ExplorerTokenLedger",
                                   "token is not active in this Explorer session")};
    }
    if (impl.closing || impl.window == nullptr ||
        impl.retained_window == nullptr) {
        return {ExplorerEligibilityReason::StaleToken,
                std::nullopt,
                adapter_diagnostic(1102U,
                                   "ExplorerTestSession",
                                   "Explorer session is retired or closing")};
    }

    const auto retained_handle = impl.retained_window->current_hwnd();
    if (!retained_handle.succeeded() || retained_handle.window != impl.window) {
        return {ExplorerEligibilityReason::WindowDestroyed,
                std::nullopt,
                retained_handle.diagnostic.has_value()
                    ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                          *retained_handle.diagnostic,
                          "retained Shell object no longer identifies the target")}
                    : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                          1103U,
                          "IWebBrowser2::HWND",
                          "retained Shell HWND changed")}};
    }

    detail::EligibilityModelFacts facts{};
    facts.window_exists = IsWindow(impl.window) != FALSE;
    facts.process_alive =
        impl.process.valid() &&
        WaitForSingleObject(impl.process.get(), 0U) == WAIT_TIMEOUT;

    DWORD current_process_id = 0U;
    const DWORD current_thread_id =
        facts.window_exists
            ? GetWindowThreadProcessId(impl.window, &current_process_id)
            : 0U;
    facts.process_id_stable =
        current_process_id != 0U && current_process_id == impl.process_id &&
        GetProcessId(impl.process.get()) == impl.process_id;
    facts.thread_id_stable =
        current_thread_id != 0U && current_thread_id == impl.thread_id;

    const auto image = validate_explorer_image(impl.process.get());
    facts.image_matches =
        image.matched &&
        (initialize_anchor ||
         ordinal_path_equal(image.actual_path, impl.process_image_path));

    const auto class_name = query_window_class(impl.window);
    facts.class_allowed = class_name.has_value() &&
                          allowed_explorer_class(*class_name) &&
                          (initialize_anchor ||
                           *class_name == impl.window_class);

    facts.root_is_self = GetAncestor(impl.window, GA_ROOT) == impl.window;
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR style = GetWindowLongPtrW(impl.window, GWL_STYLE);
    const DWORD style_error = GetLastError();
    const bool style_available = style != 0 || style_error == ERROR_SUCCESS;
    facts.child_style = !style_available ||
                        (style & static_cast<LONG_PTR>(WS_CHILD)) != 0;
    facts.has_owner = GetWindow(impl.window, GW_OWNER) != nullptr;
    facts.visible = style_available && IsWindowVisible(impl.window) != FALSE;

    DWORD cloak = 0U;
    const HRESULT cloak_result = DwmGetWindowAttribute(impl.window,
                                                        DWMWA_CLOAKED,
                                                        &cloak,
                                                        sizeof(cloak));
    facts.cloaked = cloak_result != S_OK || cloak != 0U;
    facts.minimized = IsIconic(impl.window) != FALSE;
    facts.maximized = IsZoomed(impl.window) != FALSE;

    IVirtualDesktopManager* virtual_desktop = nullptr;
    BOOL on_current_desktop = FALSE;
    const HRESULT desktop_create = CoCreateInstance(
        CLSID_VirtualDesktopManager,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&virtual_desktop));
    HRESULT desktop_query = desktop_create;
    if (desktop_create == S_OK && virtual_desktop != nullptr) {
        desktop_query = virtual_desktop->IsWindowOnCurrentVirtualDesktop(
            impl.window, &on_current_desktop);
        virtual_desktop->Release();
    }
    facts.current_virtual_desktop =
        desktop_query == S_OK && on_current_desktop != FALSE;

    const auto current_controller_security =
        query_process_security(GetCurrentProcess());
    const auto current_target_security =
        query_process_security(impl.process.get());
    facts.security_query_succeeded =
        current_controller_security.has_value() &&
        current_target_security.has_value();
    if (facts.security_query_succeeded) {
        facts.same_user = same_user(*current_controller_security,
                                    *current_target_security);
        facts.same_session =
            current_controller_security->public_facts.session_id ==
            current_target_security->public_facts.session_id;
        facts.same_integrity =
            current_controller_security->public_facts.integrity_rid ==
            current_target_security->public_facts.integrity_rid;
        facts.medium_integrity =
            current_controller_security->public_facts.integrity_rid ==
                SECURITY_MANDATORY_MEDIUM_RID &&
            current_target_security->public_facts.integrity_rid ==
                SECURITY_MANDATORY_MEDIUM_RID;
        facts.elevated =
            current_controller_security->public_facts.elevated ||
            current_target_security->public_facts.elevated;
        facts.ui_access =
            current_controller_security->public_facts.ui_access ||
            current_target_security->public_facts.ui_access;
        facts.app_container =
            current_controller_security->public_facts.app_container ||
            current_target_security->public_facts.app_container;
        if (!initialize_anchor) {
            facts.security_query_succeeded =
                current_controller_security->public_facts ==
                    impl.controller_security.public_facts &&
                current_target_security->public_facts ==
                    impl.target_security.public_facts &&
                same_user(*current_controller_security,
                          impl.controller_security) &&
                same_user(*current_target_security, impl.target_security);
        }
    }

    const auto shell_inventory = capture_shell_window_inventory();
    const auto* shell_entry = shell_inventory.find(impl.window);
    facts.shell_entry_unique =
        shell_inventory.complete && shell_entry != nullptr &&
        shell_entry->shell_entry_count == 1U &&
        shell_entry->locations.size() == 1U;
    const auto retained_location = impl.retained_window->current_location();
    facts.location_exact =
        exact_shell_location(shell_inventory,
                             impl.window,
                             impl.target_location) &&
        retained_location.filesystem() &&
        retained_location.identity == impl.target_location;

    RECT positioning_native{};
    RECT visible_native{};
    const bool positioning_ok =
        GetWindowRect(impl.window, &positioning_native) != FALSE;
    const HRESULT visible_result = DwmGetWindowAttribute(
        impl.window,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        &visible_native,
        sizeof(visible_native));
    const auto positioning = positioning_ok
                                 ? checked_native_rect(positioning_native)
                                 : std::nullopt;
    const auto visible = visible_result == S_OK
                             ? checked_native_rect(visible_native)
                             : std::nullopt;
    facts.geometry_available = positioning.has_value() && visible.has_value();

    const DPI_AWARENESS_CONTEXT window_dpi_context =
        GetWindowDpiAwarenessContext(impl.window);
    const DPI_AWARENESS_CONTEXT caller_dpi_context =
        GetThreadDpiAwarenessContext();
    facts.dpi_context_supported =
        window_dpi_context != nullptr && caller_dpi_context != nullptr &&
        AreDpiAwarenessContextsEqual(
            window_dpi_context,
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE &&
        AreDpiAwarenessContextsEqual(
            caller_dpi_context,
            DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;

    const HMONITOR monitor = MonitorFromWindow(impl.window,
                                                MONITOR_DEFAULTTONULL);
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const bool monitor_ok = monitor != nullptr &&
                            GetMonitorInfoW(monitor, &monitor_info) != FALSE;
    const auto monitor_rect = monitor_ok
                                  ? checked_native_rect(monitor_info.rcMonitor)
                                  : std::nullopt;
    const auto work_area = monitor_ok
                               ? checked_native_rect(monitor_info.rcWork)
                               : std::nullopt;
    const UINT dpi = GetDpiForWindow(impl.window);
    facts.monitor_available = monitor_ok && monitor_rect.has_value() &&
                              work_area.has_value() && dpi != 0U;
    facts.monitor_stable = initialize_anchor ||
                           (monitor == impl.initial_monitor &&
                            monitor_info.szDevice ==
                                impl.provisioning.initial_snapshot
                                    .monitor_device_name &&
                            monitor_rect ==
                                impl.provisioning.initial_snapshot.monitor_rect &&
                            work_area == impl.provisioning.initial_snapshot
                                             .monitor_work_area);
    facts.dpi_stable = initialize_anchor || dpi == impl.initial_dpi;

    const ExplorerEligibilityReason reason =
        detail::evaluate_eligibility_model(facts);
    if (reason != ExplorerEligibilityReason::Eligible) {
        std::optional<ExplorerDiagnostic> diagnostic;
        if (reason == ExplorerEligibilityReason::WrongImage &&
            image.diagnostic.has_value()) {
            diagnostic = image.diagnostic;
        } else if (reason == ExplorerEligibilityReason::WrongClass &&
                   !class_name.has_value()) {
            diagnostic = win32_diagnostic("GetClassNameW",
                                          GetLastError(),
                                          "window class query failed");
        } else if (reason == ExplorerEligibilityReason::WrongVirtualDesktop) {
            diagnostic = hresult_diagnostic(
                "IVirtualDesktopManager::IsWindowOnCurrentVirtualDesktop",
                desktop_query,
                "target is not proven to be on the current virtual desktop");
        } else {
            diagnostic = adapter_diagnostic(
                1104U,
                "Explorer eligibility",
                "live Explorer allowlist validation failed");
        }
        return {reason, std::nullopt, std::move(diagnostic)};
    }

    ExplorerWindowSnapshot snapshot;
    snapshot.positioning_rect = *positioning;
    snapshot.visible_rect = *visible;
    snapshot.process_id = current_process_id;
    snapshot.thread_id = current_thread_id;
    snapshot.process_image_path = image.actual_path;
    snapshot.window_class = *class_name;
    snapshot.dpi = dpi;
    snapshot.monitor_device_name = monitor_info.szDevice;
    snapshot.monitor_rect = *monitor_rect;
    snapshot.monitor_work_area = *work_area;
    snapshot.controller_security =
        current_controller_security->public_facts;
    snapshot.target_security = current_target_security->public_facts;
    snapshot.root_top_level = facts.root_is_self && !facts.child_style &&
                              !facts.has_owner;
    snapshot.visible = facts.visible;
    snapshot.cloaked = facts.cloaked;
    snapshot.minimized = facts.minimized;
    snapshot.maximized = facts.maximized;
    snapshot.on_current_virtual_desktop = facts.current_virtual_desktop;
    snapshot.exact_test_location = facts.location_exact;

    if (initialize_anchor) {
        impl.process_image_path = image.actual_path;
        impl.window_class = *class_name;
        impl.controller_security = *current_controller_security;
        impl.target_security = *current_target_security;
        impl.initial_monitor = monitor;
        impl.initial_dpi = dpi;
    }
    return {ExplorerEligibilityReason::Eligible,
            std::move(snapshot),
            std::nullopt};
}

[[nodiscard]] ExplorerEligibilityReason ledger_failure_reason(
    const detail::ExplorerLedgerIssueStatus status) noexcept {
    switch (status) {
    case detail::ExplorerLedgerIssueStatus::Succeeded:
        return ExplorerEligibilityReason::Eligible;
    case detail::ExplorerLedgerIssueStatus::AuthorityExhausted:
        return ExplorerEligibilityReason::AuthorityExhausted;
    case detail::ExplorerLedgerIssueStatus::GenerationExhausted:
        return ExplorerEligibilityReason::GenerationExhausted;
    case detail::ExplorerLedgerIssueStatus::InvalidNativeKey:
    case detail::ExplorerLedgerIssueStatus::DuplicateNativeKey:
        return ExplorerEligibilityReason::TargetInvalidated;
    }
    return ExplorerEligibilityReason::TargetInvalidated;
}

template <typename ImplType>
void retire_target(ImplType& impl) noexcept {
    if (impl.window != nullptr) {
        static_cast<void>(impl.ledger.retire_native(
            reinterpret_cast<detail::NativeWindowKey>(impl.window)));
    }
}

template <typename ImplType>
[[nodiscard]] NativeValidationResult validate_or_retire(
    ImplType& impl,
    const ExplorerWindowToken& token) {
    auto validation = validate_native_target(impl, &token, false);
    if (validation.reason != ExplorerEligibilityReason::Eligible) {
        retire_target(impl);
    }
    return validation;
}

} // namespace

ExplorerTestSession::ExplorerTestSession(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

ExplorerTestSession::~ExplorerTestSession() = default;

ExplorerProvisionResult ExplorerTestSession::provision(
    const std::filesystem::path& unique_empty_test_directory,
    const std::chrono::milliseconds readiness_timeout) {
    if (readiness_timeout <= std::chrono::milliseconds::zero() ||
        !unique_empty_test_directory.is_absolute()) {
        return {ExplorerEligibilityReason::InvalidTargetDirectory,
                nullptr,
                adapter_diagnostic(1200U,
                                   "ExplorerTestSession::provision",
                                   "target must be an absolute path and timeout must be positive")};
    }

    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(unique_empty_test_directory,
                                       filesystem_error) ||
        filesystem_error) {
        return {ExplorerEligibilityReason::InvalidTargetDirectory,
                nullptr,
                adapter_diagnostic(1201U,
                                   "std::filesystem::is_directory",
                                   "target directory does not exist or cannot be inspected")};
    }
    const auto first_entry = std::filesystem::directory_iterator(
        unique_empty_test_directory, filesystem_error);
    if (filesystem_error) {
        return {ExplorerEligibilityReason::InvalidTargetDirectory,
                nullptr,
                adapter_diagnostic(1202U,
                                   "std::filesystem::directory_iterator",
                                   "target directory cannot be enumerated")};
    }
    if (first_entry != std::filesystem::directory_iterator{}) {
        return {ExplorerEligibilityReason::TargetDirectoryNotEmpty,
                nullptr,
                adapter_diagnostic(1203U,
                                   "ExplorerTestSession::provision",
                                   "target directory must be empty")};
    }

    const HRESULT apartment = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (apartment != S_OK && apartment != S_FALSE) {
        return {ExplorerEligibilityReason::ComApartmentUnavailable,
                nullptr,
                hresult_diagnostic("CoInitializeEx",
                                   apartment,
                                   "Explorer provisioning requires an STA")};
    }

    auto impl = std::make_unique<Impl>();
    impl->com_initialized = true;
    impl->controller_thread_id = GetCurrentThreadId();
    impl->target_directory = unique_empty_test_directory;

    const auto target_location =
        filesystem_location_identity(unique_empty_test_directory);
    if (!target_location.filesystem()) {
        return {ExplorerEligibilityReason::InvalidTargetDirectory,
                nullptr,
                hresult_diagnostic("filesystem_location_identity",
                                   target_location.diagnostic,
                                   "target directory file identity is unavailable")};
    }
    impl->target_location = *target_location.identity;

    auto baseline = capture_stable_inventory(readiness_timeout);
    if (baseline.reason != ExplorerEligibilityReason::Eligible ||
        !baseline.model.has_value()) {
        return {baseline.reason, nullptr, std::move(baseline.diagnostic)};
    }
    impl->baseline_inventory = *baseline.model;
    impl->provisioning.preexisting_window_count =
        impl->baseline_inventory.windows.size();

    // HWND readiness and post-navigation isolation share one bounded budget.
    // The Shell boundary makes exactly one CoCreateInstance attempt; retries
    // below are confined to get_HWND on that same retained automation object.
    const auto readiness_deadline =
        std::chrono::steady_clock::now() + readiness_timeout;
    auto created = create_explorer_shell_window(readiness_deadline);
    if (!created.succeeded()) {
        const auto failure_reason = created.diagnostic.has_value()
                                        ? detail::map_shell_creation_stage(
                                              created.diagnostic->stage)
                                        : ExplorerEligibilityReason::
                                              ShellWindowCreationFailed;
        return {failure_reason,
                nullptr,
                created.diagnostic.has_value()
                    ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                          *created.diagnostic,
                          failure_reason == ExplorerEligibilityReason::
                                                    ShellWindowHandleMissing
                              ? "new Shell browser frame HWND was not ready before timeout"
                              : "CLSID_ShellBrowserWindow creation failed")}
                    : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                          1204U,
                          "create_explorer_shell_window",
                          "Shell returned no retained browser object")}};
    }
    impl->retained_window = std::move(created.window);
    impl->window = impl->retained_window->initial_hwnd();
    const auto native_key =
        reinterpret_cast<detail::NativeWindowKey>(impl->window);
    if (native_key == 0U) {
        return {ExplorerEligibilityReason::ShellWindowHandleMissing,
                nullptr,
                adapter_diagnostic(1205U,
                                   "IWebBrowser2::HWND",
                                   "new Shell browser returned a null frame HWND")};
    }
    if (find_inventory_window(impl->baseline_inventory, native_key) != nullptr) {
        return {ExplorerEligibilityReason::PreexistingWindow,
                nullptr,
                adapter_diagnostic(1206U,
                                   "Explorer baseline",
                                   "retained Shell browser HWND existed before creation")};
    }
    impl->provisioning.retained_window_was_new_before_navigation = true;

    // Do not let a just-in-time HWND extend the bounded provisioning window.
    // Expiry here is fail-closed: the retained object is released without any
    // navigation, visibility, activation, or geometry call.
    if (!detail::readiness_deadline_active(
            readiness_deadline, std::chrono::steady_clock::now())) {
        return {ExplorerEligibilityReason::ShellWindowHandleMissing,
                nullptr,
                shell_diagnostic(
                    ShellAutomationDiagnostic{
                        ShellAutomationStage::AwaitWindowHandle,
                        HRESULT_FROM_WIN32(ERROR_TIMEOUT),
                        -1},
                    "readiness budget expired before Explorer navigation")};
    }

    const auto navigate = impl->retained_window->navigate_to_and_show(
        unique_empty_test_directory);
    if (!navigate.succeeded()) {
        return {ExplorerEligibilityReason::LocationMismatch,
                nullptr,
                hresult_diagnostic("IWebBrowser2::Navigate2/put_Visible",
                                   navigate.hresult,
                                   "new Shell browser could not navigate to the test directory")};
    }

    const auto deadline = readiness_deadline;
    const auto readiness_timeout_blocker = [] {
        return ExplorerProvisionResult{
            ExplorerEligibilityReason::InventoryUnstable,
            nullptr,
            adapter_diagnostic(
                1209U,
                "Explorer candidate readiness",
                "bounded readiness expired before capability issuance")};
    };
    std::optional<detail::InventoryModel> prior_eligible_inventory;
    detail::CandidateEvaluation last_candidate;
    std::optional<detail::InventoryModel> accepted_inventory;
    do {
        if (!detail::readiness_deadline_active(
                deadline, std::chrono::steady_clock::now())) {
            return readiness_timeout_blocker();
        }
        const auto current_handle = impl->retained_window->current_hwnd();
        if (!current_handle.succeeded() || current_handle.window != impl->window) {
            return {ExplorerEligibilityReason::ShellWindowHandleMissing,
                    nullptr,
                    current_handle.diagnostic.has_value()
                        ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                              *current_handle.diagnostic,
                              "retained Shell browser frame changed during navigation")}
                        : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                              1207U,
                              "IWebBrowser2::HWND",
                              "retained Shell browser frame changed during navigation")}};
        }

        const auto native_inventory = capture_shell_window_inventory();
        const auto current_inventory = inventory_model(native_inventory);
        last_candidate = detail::evaluate_candidate_inventory(
            impl->baseline_inventory,
            current_inventory,
            native_key,
            impl->target_location);
        if (last_candidate.reason == ExplorerEligibilityReason::Eligible) {
            const bool stable_inventory =
                prior_eligible_inventory.has_value() &&
                *prior_eligible_inventory == current_inventory;
            if (stable_inventory) {
                if (!detail::readiness_acceptance_allowed(
                        stable_inventory,
                        deadline,
                        std::chrono::steady_clock::now())) {
                    return readiness_timeout_blocker();
                }
                accepted_inventory = current_inventory;
                break;
            }
            prior_eligible_inventory = current_inventory;
        } else {
            prior_eligible_inventory.reset();
            if (last_candidate.reason ==
                    ExplorerEligibilityReason::AmbiguousCandidate ||
                last_candidate.reason ==
                    ExplorerEligibilityReason::BaselineChanged ||
                last_candidate.reason ==
                    ExplorerEligibilityReason::PreexistingWindow) {
                return {last_candidate.reason,
                        nullptr,
                        adapter_diagnostic(1208U,
                                           "Explorer candidate isolation",
                                           "post-navigation inventory violated isolation")};
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return {last_candidate.reason == ExplorerEligibilityReason::Eligible
                        ? ExplorerEligibilityReason::InventoryUnstable
                        : last_candidate.reason,
                    nullptr,
                    adapter_diagnostic(1209U,
                                       "Explorer candidate readiness",
                                       "no stable unique new Explorer target was observed before timeout")};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{40});
    } while (true);

    if (!detail::readiness_deadline_active(
            deadline, std::chrono::steady_clock::now())) {
        return readiness_timeout_blocker();
    }

    impl->provisioning.post_navigation_window_count =
        accepted_inventory->windows.size();
    impl->provisioning.new_candidate_count =
        last_candidate.new_candidate_count;
    impl->provisioning.baseline_facts_unchanged =
        last_candidate.baseline_unchanged;
    impl->provisioning.exact_unique_test_location =
        last_candidate.exact_unique_location;

    DWORD process_id = 0U;
    const DWORD thread_id =
        GetWindowThreadProcessId(impl->window, &process_id);
    if (thread_id == 0U || process_id == 0U) {
        return {ExplorerEligibilityReason::WindowDestroyed,
                nullptr,
                win32_diagnostic("GetWindowThreadProcessId",
                                 GetLastError(),
                                 "new Explorer target lost native identity")};
    }
    impl->process_id = process_id;
    impl->thread_id = thread_id;
    impl->process.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
                                        SYNCHRONIZE,
                                    FALSE,
                                    process_id));
    if (!impl->process.valid()) {
        return {ExplorerEligibilityReason::ProcessOpenFailed,
                nullptr,
                win32_diagnostic("OpenProcess",
                                 GetLastError(),
                                 "Explorer process identity handle could not be opened")};
    }
    DWORD rechecked_process_id = 0U;
    const DWORD rechecked_thread_id =
        GetWindowThreadProcessId(impl->window, &rechecked_process_id);
    if (rechecked_thread_id != thread_id ||
        rechecked_process_id != process_id ||
        GetProcessId(impl->process.get()) != process_id ||
        WaitForSingleObject(impl->process.get(), 0U) != WAIT_TIMEOUT) {
        return {ExplorerEligibilityReason::TargetInvalidated,
                nullptr,
                adapter_diagnostic(1210U,
                                   "Explorer process identity",
                                   "PID/TID/process instance changed during issuance")};
    }

    auto initial = validate_native_target(*impl, nullptr, true);
    if (initial.reason != ExplorerEligibilityReason::Eligible ||
        !initial.snapshot.has_value()) {
        return {initial.reason, nullptr, std::move(initial.diagnostic)};
    }
    if (!detail::rect_is_contained(initial.snapshot->visible_rect,
                                   initial.snapshot->monitor_work_area) ||
        !select_safe_test_delta(*initial.snapshot).succeeded()) {
        return {ExplorerEligibilityReason::UnsafeDelta,
                nullptr,
                adapter_diagnostic(
                    1212U,
                    "Explorer initial geometry",
                    "initial frame or every test delta falls outside one monitor work area")};
    }
    impl->provisioning.initial_snapshot = *initial.snapshot;
    std::this_thread::sleep_for(std::chrono::milliseconds{40});
    if (!detail::readiness_deadline_active(
            deadline, std::chrono::steady_clock::now())) {
        return readiness_timeout_blocker();
    }
    auto stable_initial = validate_native_target(*impl, nullptr, false);
    if (stable_initial.reason != ExplorerEligibilityReason::Eligible ||
        !stable_initial.snapshot.has_value()) {
        return {stable_initial.reason,
                nullptr,
                std::move(stable_initial.diagnostic)};
    }
    if (*stable_initial.snapshot != *initial.snapshot) {
        return {ExplorerEligibilityReason::TargetInvalidated,
                nullptr,
                adapter_diagnostic(
                    1213U,
                    "Explorer initial geometry",
                    "target geometry did not remain stable before capability issuance")};
    }
    if (!detail::readiness_deadline_active(
            deadline, std::chrono::steady_clock::now())) {
        return readiness_timeout_blocker();
    }
    auto issue = impl->ledger.issue(native_key, process_id, thread_id);
    if (!issue.token.has_value()) {
        return {ledger_failure_reason(issue.status),
                nullptr,
                adapter_diagnostic(1211U,
                                   "ExplorerTokenLedger::issue",
                                   "Explorer capability issuance failed")};
    }
    impl->issued_token = *issue.token;
    impl->original_snapshot = *stable_initial.snapshot;
    impl->provisioning.initial_snapshot = *stable_initial.snapshot;

    return {ExplorerEligibilityReason::Eligible,
            std::unique_ptr<ExplorerTestSession>(
                new ExplorerTestSession(std::move(impl))),
            std::nullopt};
}

const ExplorerProvisioningFacts& ExplorerTestSession::provisioning_facts()
    const noexcept {
    return impl_->provisioning;
}

const ExplorerWindowToken& ExplorerTestSession::token() const noexcept {
    return *impl_->issued_token;
}

bool ExplorerTestSession::contains(
    const ExplorerWindowToken& token_value) noexcept {
    if (impl_ == nullptr || !impl_->ledger.contains(token_value)) {
        return false;
    }
    try {
        return validate_or_retire(*impl_, token_value).reason ==
               ExplorerEligibilityReason::Eligible;
    } catch (...) {
        retire_target(*impl_);
        return false;
    }
}

namespace {

template <typename ImplType>
[[nodiscard]] ExplorerOperationResult run_single_translation(
    ImplType& impl,
    const ExplorerWindowToken& token,
    const core::geometry::Rect& target_visible_rect,
    const bool cleanup_operation,
    const ExplorerWindowSnapshot* expected_before) {
    ExplorerOperationResult result;
    result.operation_id = operation_ids.issue();
    result.cleanup_operation = cleanup_operation;
    result.stage = ExplorerOperationStage::Preflight;
    result.receipt = ExplorerOperationReceipt{token};
    result.receipt->requested_visible_rect = target_visible_rect;

    const auto operation_gate = cleanup_operation
                                    ? detail::evaluate_restore_gate(
                                          impl.ledger.contains(token),
                                          impl.primary_apply_guard.consumed(),
                                          impl.primary_before_snapshot.has_value(),
                                          impl.primary_actual_snapshot.has_value(),
                                          impl.restore_guard.consumed())
                                    : detail::evaluate_primary_apply_gate(
                                          impl.ledger.contains(token),
                                          impl.primary_apply_guard.consumed());
    if (operation_gate == ExplorerEligibilityReason::StaleToken) {
        result.reason = operation_gate;
        result.diagnostic = adapter_diagnostic(
            1298U,
            "ExplorerTokenLedger",
            "token is stale or belongs to another Explorer session");
        return result;
    }
    if (operation_gate == ExplorerEligibilityReason::OperationLimitReached ||
        operation_gate ==
            ExplorerEligibilityReason::OperationSequenceViolation) {
        result.reason = operation_gate;
        result.diagnostic = adapter_diagnostic(
            1299U,
            "Explorer operation sequence guard",
            cleanup_operation
                ? "restore requires exactly one primary attempt and can run only once"
                : "R1-C2A permits at most one primary native translation per session");
        return result;
    }

    auto before = validate_or_retire(impl, token);
    if (before.reason != ExplorerEligibilityReason::Eligible ||
        !before.snapshot.has_value()) {
        result.reason = before.reason;
        result.diagnostic = std::move(before.diagnostic);
        return result;
    }
    result.receipt->before = *before.snapshot;

    if (expected_before == nullptr ||
        !detail::operation_snapshot_matches_expected(*expected_before,
                                                     *before.snapshot)) {
        retire_target(impl);
        result.reason = ExplorerEligibilityReason::TargetInvalidated;
        result.diagnostic = adapter_diagnostic(
            1306U,
            "Explorer operation expected geometry",
            cleanup_operation
                ? "target changed after primary apply and before restore"
                : "target changed after issuance and before primary apply");
        return result;
    }

    if (!detail::rect_is_contained(target_visible_rect,
                                   before.snapshot->monitor_work_area)) {
        result.reason = ExplorerEligibilityReason::UnsafeDelta;
        result.diagnostic = adapter_diagnostic(
            1300U,
            "Explorer translation preflight",
            "requested visible target is not wholly inside the original work area");
        return result;
    }

    const auto preparation =
        operations::window_translation::prepare_visible_translation(
            before.snapshot->positioning_rect,
            before.snapshot->visible_rect,
            target_visible_rect);
    result.reason = detail::map_translation_status(preparation.status);
    if (result.reason != ExplorerEligibilityReason::Eligible ||
        !preparation.target_positioning_rect.has_value()) {
        result.diagnostic = adapter_diagnostic(
            1301U,
            "prepare_visible_translation",
            "shared translation bridge rejected the Explorer request");
        return result;
    }
    result.receipt->requested_positioning_rect =
        *preparation.target_positioning_rect;

    // Close the gap between geometry preparation and native apply with another
    // complete live check. Any user or Shell change cancels the operation.
    auto immediate = validate_or_retire(impl, token);
    if (immediate.reason != ExplorerEligibilityReason::Eligible ||
        !immediate.snapshot.has_value()) {
        result.reason = immediate.reason;
        result.diagnostic = std::move(immediate.diagnostic);
        return result;
    }
    if (*immediate.snapshot != *before.snapshot) {
        retire_target(impl);
        result.reason = ExplorerEligibilityReason::TargetInvalidated;
        result.diagnostic = adapter_diagnostic(
            1302U,
            "Explorer immediate preflight",
            "target facts changed between capture and native apply");
        return result;
    }

    result.stage = cleanup_operation ? ExplorerOperationStage::Restore
                                     : ExplorerOperationStage::NativeApply;
    if (!cleanup_operation && !impl.primary_apply_guard.try_consume()) {
        result.stage = ExplorerOperationStage::Preflight;
        result.reason = ExplorerEligibilityReason::OperationLimitReached;
        result.diagnostic = adapter_diagnostic(
            1305U,
            "Explorer primary apply guard",
            "primary operation allowance was already consumed");
        return result;
    }
    if (cleanup_operation && !impl.restore_guard.try_consume()) {
        result.stage = ExplorerOperationStage::Preflight;
        result.reason = ExplorerEligibilityReason::OperationLimitReached;
        result.diagnostic = adapter_diagnostic(
            1307U,
            "Explorer restore guard",
            "R1-C2A permits at most one cleanup restore per session");
        return result;
    }
    if (!cleanup_operation) {
        impl.primary_before_snapshot = *before.snapshot;
    }
    result.native_apply_attempted = true;
    SetLastError(ERROR_SUCCESS);
    const BOOL native_result = SetWindowPos(
        impl.window,
        nullptr,
        static_cast<int>(preparation.target_positioning_rect->left()),
        static_cast<int>(preparation.target_positioning_rect->top()),
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    const DWORD native_error = GetLastError();

    auto actual = validate_native_target(impl, &token, false);
    if (actual.snapshot.has_value()) {
        result.receipt->actual = *actual.snapshot;
        if (!cleanup_operation &&
            actual.reason == ExplorerEligibilityReason::Eligible) {
            impl.primary_actual_snapshot = *actual.snapshot;
        }
    }
    if (native_result == FALSE) {
        result.reason = ExplorerEligibilityReason::NativeApplyFailed;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "SetWindowPos",
            native_error,
            "single Explorer pure-translation call failed");
        if (actual.reason != ExplorerEligibilityReason::Eligible) {
            retire_target(impl);
        }
        return result;
    }

    result.stage = ExplorerOperationStage::PostVerification;
    if (actual.reason != ExplorerEligibilityReason::Eligible ||
        !actual.snapshot.has_value()) {
        retire_target(impl);
        result.reason = actual.reason;
        result.diagnostic = std::move(actual.diagnostic);
        return result;
    }

    const auto& after = *actual.snapshot;
    auto& receipt = *result.receipt;
    receipt.visible_target_verified =
        after.visible_rect == receipt.requested_visible_rect;
    receipt.positioning_target_verified =
        receipt.requested_positioning_rect.has_value() &&
        after.positioning_rect == *receipt.requested_positioning_rect;
    receipt.size_preserved =
        same_snapshot_size(before.snapshot->visible_rect,
                           after.visible_rect) &&
        same_snapshot_size(before.snapshot->positioning_rect,
                           after.positioning_rect);
    receipt.identity_stable =
        before.snapshot->process_id == after.process_id &&
        before.snapshot->thread_id == after.thread_id &&
        before.snapshot->process_image_path == after.process_image_path &&
        before.snapshot->window_class == after.window_class &&
        before.snapshot->controller_security == after.controller_security &&
        before.snapshot->target_security == after.target_security;
    receipt.location_stable = after.exact_test_location;
    receipt.monitor_and_dpi_stable =
        before.snapshot->monitor_device_name == after.monitor_device_name &&
        before.snapshot->monitor_rect == after.monitor_rect &&
        before.snapshot->monitor_work_area == after.monitor_work_area &&
        before.snapshot->dpi == after.dpi;

    const detail::PostVerificationModel verification{
        impl.ledger.contains(token),
        before.snapshot->process_id == after.process_id,
        before.snapshot->thread_id == after.thread_id,
        before.snapshot->process_image_path == after.process_image_path &&
            before.snapshot->window_class == after.window_class,
        receipt.location_stable,
        before.snapshot->controller_security == after.controller_security &&
            before.snapshot->target_security == after.target_security,
        before.snapshot->monitor_device_name == after.monitor_device_name &&
            before.snapshot->monitor_rect == after.monitor_rect &&
            before.snapshot->monitor_work_area == after.monitor_work_area,
        before.snapshot->dpi == after.dpi,
        receipt.size_preserved,
        receipt.visible_target_verified,
        receipt.positioning_target_verified,
    };
    result.reason = detail::evaluate_post_verification(verification);
    if (result.reason != ExplorerEligibilityReason::Eligible) {
        result.diagnostic = adapter_diagnostic(
            1303U,
            "Explorer post-verification",
            "native success did not satisfy the exact Explorer operation contract");
        if (result.reason !=
            ExplorerEligibilityReason::PostVerificationFailed) {
            retire_target(impl);
        }
    }
    return result;
}

} // namespace

ExplorerWindowOperations::ExplorerWindowOperations(
    ExplorerTestSession& session) noexcept
    : session_(&session) {}

ExplorerCaptureResult ExplorerWindowOperations::capture(
    const ExplorerWindowToken& token) {
    if (session_ == nullptr || session_->impl_ == nullptr) {
        return {ExplorerEligibilityReason::StaleToken,
                ExplorerOperationStage::Preflight,
                std::nullopt,
                adapter_diagnostic(1304U,
                                   "ExplorerWindowOperations::capture",
                                   "session is unavailable")};
    }
    auto validation = validate_or_retire(*session_->impl_, token);
    return {validation.reason,
            ExplorerOperationStage::Preflight,
            std::move(validation.snapshot),
            std::move(validation.diagnostic)};
}

ExplorerOperationResult ExplorerWindowOperations::apply_single(
    const ExplorerWindowToken& token,
    const core::geometry::Rect& target_visible_rect) {
    if (session_ == nullptr || session_->impl_ == nullptr) {
        ExplorerOperationResult result;
        result.operation_id = operation_ids.issue();
        result.reason = ExplorerEligibilityReason::StaleToken;
        result.receipt = ExplorerOperationReceipt{token};
        return result;
    }
    return run_single_translation(*session_->impl_,
                                  token,
                                  target_visible_rect,
                                  false,
                                  session_->impl_->original_snapshot.has_value()
                                      ? &*session_->impl_->original_snapshot
                                      : nullptr);
}

ExplorerOperationResult ExplorerWindowOperations::restore(
    const ExplorerWindowToken& token) {
    if (session_ == nullptr || session_->impl_ == nullptr) {
        ExplorerOperationResult result;
        result.operation_id = operation_ids.issue();
        result.reason = ExplorerEligibilityReason::StaleToken;
        result.stage = ExplorerOperationStage::Restore;
        result.cleanup_operation = true;
        result.receipt = ExplorerOperationReceipt{token};
        return result;
    }
    const auto restore_gate = detail::evaluate_restore_gate(
        session_->impl_->ledger.contains(token),
        session_->impl_->primary_apply_guard.consumed(),
        session_->impl_->primary_before_snapshot.has_value(),
        session_->impl_->primary_actual_snapshot.has_value(),
        session_->impl_->restore_guard.consumed());
    if (restore_gate != ExplorerEligibilityReason::Eligible) {
        ExplorerOperationResult result;
        result.operation_id = operation_ids.issue();
        result.reason = restore_gate;
        result.stage = ExplorerOperationStage::Restore;
        result.cleanup_operation = true;
        result.receipt = ExplorerOperationReceipt{token};
        return result;
    }
    return run_single_translation(
        *session_->impl_,
        token,
        session_->impl_->primary_before_snapshot->visible_rect,
        true,
        &*session_->impl_->primary_actual_snapshot);
}

ExplorerCleanupResult ExplorerTestSession::close_test_window(
    const ExplorerWindowToken& token_value,
    const std::chrono::milliseconds disappearance_timeout) {
    ExplorerCleanupResult result;
    if (impl_ == nullptr ||
        disappearance_timeout <= std::chrono::milliseconds::zero() ||
        !impl_->ledger.contains(token_value)) {
        result.reason = ExplorerEligibilityReason::StaleToken;
        return result;
    }

    auto validation = validate_or_retire(*impl_, token_value);
    if (validation.reason != ExplorerEligibilityReason::Eligible) {
        result.reason = ExplorerEligibilityReason::SafeCleanupNotPerformed;
        result.token_retired = !impl_->ledger.contains(token_value);
        result.diagnostic = std::move(validation.diagnostic);
        return result;
    }

    impl_->closing = true;
    result.token_retired = impl_->ledger.retire_native(
        reinterpret_cast<detail::NativeWindowKey>(impl_->window));
    const auto quit = impl_->retained_window->quit();
    result.native_close_attempted = true;
    const auto deadline =
        std::chrono::steady_clock::now() + disappearance_timeout;
    do {
        const auto inventory = capture_shell_window_inventory();
        const bool native_gone = IsWindow(impl_->window) == FALSE;
        const bool inventory_gone =
            inventory.complete && inventory.find(impl_->window) == nullptr;
        if (native_gone && inventory_gone) {
            result.reason = ExplorerEligibilityReason::Eligible;
            result.window_disappeared = true;
            impl_->retained_window.reset();
            return result;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{40});
    } while (true);

    result.reason = ExplorerEligibilityReason::SafeCleanupNotPerformed;
    result.diagnostic = hresult_diagnostic(
        "IWebBrowser2::Quit",
        quit.hresult,
        quit.succeeded()
            ? "exact test window did not disappear before the bounded deadline"
            : "exact retained Shell object rejected the graceful close request");
    return result;
}

detail::ExplorerDiagnosticNativeIdentity
detail::ExplorerSessionDiagnostics::read(
    const ExplorerTestSession& session) noexcept {
    if (session.impl_ == nullptr) {
        return {};
    }
    return {reinterpret_cast<detail::NativeWindowKey>(session.impl_->window),
            session.impl_->process_id,
            session.impl_->thread_id};
}

} // namespace panebind::platform::windows::explorer
