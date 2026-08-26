#pragma once

#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_shell_inventory.h"
#include "platform/windows/operations/window_translation.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace panebind::platform::windows::explorer::detail {

using NativeWindowKey = std::uintptr_t;

class MonotonicIdSource final {
public:
    explicit MonotonicIdSource(std::uint64_t first = 1U) noexcept;

    MonotonicIdSource(const MonotonicIdSource&) = delete;
    MonotonicIdSource& operator=(const MonotonicIdSource&) = delete;

    [[nodiscard]] std::uint64_t issue() noexcept;

private:
    std::atomic<std::uint64_t> next_;
};

struct ExplorerLedgerEntry {
    std::uint64_t logical_id{};
    std::uint64_t generation{};
    NativeWindowKey native_key{};
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
};

enum class ExplorerLedgerIssueStatus {
    Succeeded,
    InvalidNativeKey,
    DuplicateNativeKey,
    AuthorityExhausted,
    GenerationExhausted,
};

struct ExplorerLedgerIssueResult {
    ExplorerLedgerIssueStatus status{
        ExplorerLedgerIssueStatus::InvalidNativeKey};
    std::optional<ExplorerWindowToken> token;
};

class ExplorerTokenLedger final {
public:
    ExplorerTokenLedger() noexcept;
    ExplorerTokenLedger(std::uint64_t controller_authority,
                        std::uint64_t session_authority,
                        std::uint64_t next_logical_id,
                        std::uint64_t next_generation) noexcept;

    [[nodiscard]] ExplorerLedgerIssueResult issue(
        NativeWindowKey native_key,
        std::uint32_t process_id,
        std::uint32_t thread_id);
    [[nodiscard]] bool retire_native(NativeWindowKey native_key) noexcept;
    [[nodiscard]] std::optional<ExplorerLedgerEntry> resolve(
        const ExplorerWindowToken& token) const noexcept;
    [[nodiscard]] bool contains(
        const ExplorerWindowToken& token) const noexcept;

private:
    std::uint64_t controller_authority_{};
    std::uint64_t session_authority_{};
    std::uint64_t next_logical_id_{1U};
    std::uint64_t next_generation_{1U};
    std::map<std::uint64_t, ExplorerLedgerEntry> active_by_logical_id_;
    std::map<NativeWindowKey, std::uint64_t> logical_id_by_native_key_;
};

class SinglePrimaryApplyGuard final {
public:
    [[nodiscard]] bool try_consume() noexcept {
        if (consumed_) {
            return false;
        }
        consumed_ = true;
        return true;
    }

    [[nodiscard]] bool consumed() const noexcept { return consumed_; }

private:
    bool consumed_{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_primary_apply_gate(
    bool token_active,
    bool allowance_consumed) noexcept;

[[nodiscard]] ExplorerEligibilityReason evaluate_restore_gate(
    bool token_active,
    bool primary_attempted,
    bool primary_before_available,
    bool primary_actual_available,
    bool restore_allowance_consumed) noexcept;

// Keeps creation failure and delayed frame readiness distinguishable without
// exposing a raw Shell automation object or inventing a fallback target.
[[nodiscard]] ExplorerEligibilityReason map_shell_creation_stage(
    ShellAutomationStage stage) noexcept;

[[nodiscard]] constexpr bool readiness_deadline_active(
    const std::chrono::steady_clock::time_point deadline,
    const std::chrono::steady_clock::time_point now) noexcept {
    return now < deadline;
}

[[nodiscard]] constexpr bool readiness_acceptance_allowed(
    const bool stable,
    const std::chrono::steady_clock::time_point deadline,
    const std::chrono::steady_clock::time_point now) noexcept {
    return stable && readiness_deadline_active(deadline, now);
}

[[nodiscard]] bool operation_snapshot_matches_expected(
    const ExplorerWindowSnapshot& expected,
    const ExplorerWindowSnapshot& current) noexcept;

struct InventoryLocationFingerprint {
    ShellLocationStatus status{ShellLocationStatus::Unavailable};
    ShellLocationSource source{ShellLocationSource::None};
    std::optional<FilesystemLocationIdentity> filesystem_location;
    std::optional<OpaqueLocationFingerprint> opaque_location;

    friend bool operator==(const InventoryLocationFingerprint&,
                           const InventoryLocationFingerprint&) = default;
};

struct InventoryFingerprint {
    NativeWindowKey native_key{};
    std::size_t shell_entry_count{};
    // Keep each status, file identity, and opaque LocationURL digest together.
    // Sorting these compound records makes Shell enumeration order irrelevant
    // without losing which witnesses belonged to which entry.
    std::vector<InventoryLocationFingerprint> locations;

    friend bool operator==(const InventoryFingerprint&,
                           const InventoryFingerprint&) = default;
};

struct InventoryModel {
    bool complete{};
    std::vector<InventoryFingerprint> windows;

    friend bool operator==(const InventoryModel&,
                           const InventoryModel&) = default;
};

struct CandidateEvaluation {
    ExplorerEligibilityReason reason{
        ExplorerEligibilityReason::InventoryUnavailable};
    std::size_t new_candidate_count{};
    bool baseline_unchanged{};
    bool exact_unique_location{};
};

[[nodiscard]] CandidateEvaluation evaluate_candidate_inventory(
    const InventoryModel& baseline,
    const InventoryModel& post_navigation,
    NativeWindowKey retained_window,
    const FilesystemLocationIdentity& target_location) noexcept;

struct EligibilityModelFacts {
    bool window_exists{};
    bool process_alive{};
    bool process_id_stable{};
    bool thread_id_stable{};
    bool image_matches{};
    bool class_allowed{};
    bool root_is_self{};
    bool child_style{};
    bool has_owner{};
    bool visible{};
    bool cloaked{};
    bool minimized{};
    bool maximized{};
    bool current_virtual_desktop{};
    bool security_query_succeeded{};
    bool same_user{};
    bool same_session{};
    bool same_integrity{};
    bool medium_integrity{};
    bool elevated{};
    bool ui_access{};
    bool app_container{};
    bool shell_entry_unique{};
    bool location_exact{};
    bool geometry_available{};
    bool dpi_context_supported{};
    bool monitor_available{};
    bool monitor_stable{};
    bool dpi_stable{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_eligibility_model(
    const EligibilityModelFacts& facts) noexcept;

[[nodiscard]] bool rect_is_contained(
    const core::geometry::Rect& inner,
    const core::geometry::Rect& outer) noexcept;

[[nodiscard]] ExplorerSafeDeltaResult select_safe_delta_from_candidates(
    const ExplorerWindowSnapshot& snapshot,
    std::span<const ExplorerTranslationDelta> candidates) noexcept;

struct PostVerificationModel {
    bool token_identity_stable{};
    bool process_identity_stable{};
    bool thread_identity_stable{};
    bool image_and_class_stable{};
    bool location_stable{};
    bool security_stable{};
    bool monitor_stable{};
    bool dpi_stable{};
    bool size_preserved{};
    bool visible_target_matches{};
    bool positioning_target_matches{};
};

[[nodiscard]] ExplorerEligibilityReason evaluate_post_verification(
    const PostVerificationModel& verification) noexcept;

[[nodiscard]] ExplorerEligibilityReason map_translation_status(
    operations::window_translation::TranslationPreparationStatus status)
    noexcept;

struct ExplorerDiagnosticNativeIdentity {
    NativeWindowKey native_key{};
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
};

// Fixture-only read path for correlating session evidence with observer JSONL.
// It is intentionally absent from explorer_session.h and cannot authorize an
// operation; all mutations still require ExplorerWindowToken.
class ExplorerSessionDiagnostics final {
public:
    [[nodiscard]] static ExplorerDiagnosticNativeIdentity read(
        const ExplorerTestSession& session) noexcept;
};

} // namespace panebind::platform::windows::explorer::detail
