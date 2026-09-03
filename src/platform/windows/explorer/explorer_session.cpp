#include "platform/windows/explorer/explorer_session.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/explorer/explorer_glue_session_internal.h"
#include "platform/windows/explorer/explorer_session_internal.h"
#include "platform/windows/explorer/explorer_shell_events.h"

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
#include <utility>
#include <vector>

namespace panebind::platform::windows::explorer {
namespace {

detail::MonotonicIdSource token_authority_ids;
detail::MonotonicIdSource operation_ids;

[[nodiscard]] detail::InventoryModel make_inventory_model(
    const ShellWindowInventory& inventory);

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

ExplorerLedgerIssueResult ExplorerTokenLedger::issue_legacy_auto(
    const NativeWindowKey native_key,
    const std::uint32_t process_id,
    const std::uint32_t thread_id,
    const std::int32_t registration_cookie,
    const std::uint64_t subscription_generation) {
    if (registration_cookie == 0 || subscription_generation == 0U) {
        return {ExplorerLedgerIssueStatus::InvalidRegistrationAuthority,
                std::nullopt};
    }
    return issue_with_authority(
        native_key,
        process_id,
        thread_id,
        ExplorerAuthorityKind::LegacyAutoProvisionDiagnostic,
        registration_cookie,
        subscription_generation,
        0U);
}

ExplorerLedgerIssueResult ExplorerTokenLedger::issue_user_consent(
    const NativeWindowKey native_key,
    const std::uint32_t process_id,
    const std::uint32_t thread_id,
    const std::uint64_t consent_generation) {
    if (consent_generation == 0U) {
        return {ExplorerLedgerIssueStatus::InvalidConsentAuthority,
                std::nullopt};
    }
    return issue_with_authority(native_key,
                                process_id,
                                thread_id,
                                ExplorerAuthorityKind::UserConsent,
                                0,
                                0U,
                                consent_generation);
}

ExplorerLedgerIssueResult ExplorerTokenLedger::issue_with_authority(
    const NativeWindowKey native_key,
    const std::uint32_t process_id,
    const std::uint32_t thread_id,
    const ExplorerAuthorityKind authority_kind,
    const std::int32_t registration_cookie,
    const std::uint64_t subscription_generation,
    const std::uint64_t consent_generation) {
    if (native_key == 0U) {
        return {ExplorerLedgerIssueStatus::InvalidNativeKey, std::nullopt};
    }
    if (logical_id_by_native_key_.contains(native_key)) {
        return {ExplorerLedgerIssueStatus::DuplicateNativeKey, std::nullopt};
    }
    if (authority_kind_.has_value() && *authority_kind_ != authority_kind) {
        return {ExplorerLedgerIssueStatus::AuthorityKindConflict,
                std::nullopt};
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
                                    authority_kind,
                                    consent_generation,
                                    native_key,
                                    process_id,
                                    thread_id,
                                    registration_cookie,
                                    subscription_generation};
    authority_kind_ = authority_kind;
    active_by_logical_id_.emplace(entry.logical_id, entry);
    logical_id_by_native_key_.emplace(entry.native_key, entry.logical_id);
    ++next_logical_id_;
    ++next_generation_;
    return {ExplorerLedgerIssueStatus::Succeeded,
            ExplorerWindowToken{controller_authority_,
                                session_authority_,
                                entry.logical_id,
                                entry.generation,
                                entry.authority_kind,
                                entry.consent_generation}};
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
        found->second.generation != token.generation_ ||
        found->second.authority_kind != token.authority_kind_ ||
        found->second.consent_generation != token.consent_generation_) {
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

BaselineExclusionEvaluation evaluate_baseline_exclusion(
    const std::span<const BaselineEntryModel> entries,
    const std::span<const NativeWindowKey> previously_forbidden) {
    BaselineExclusionEvaluation result;
    result.status = BaselineExclusionStatus::Complete;
    result.total_entry_count = entries.size();

    std::set<NativeWindowKey> forbidden;
    std::set<NativeWindowKey> reliable_baseline_hwnds;
    for (const auto native_key : previously_forbidden) {
        if (native_key != 0U) {
            forbidden.insert(native_key);
        }
    }

    for (const auto& entry : entries) {
        switch (entry.location) {
        case BaselineLocationDisposition::Valid:
            ++result.valid_location_count;
            break;
        case BaselineLocationDisposition::Empty:
            ++result.empty_location_count;
            break;
        case BaselineLocationDisposition::Inaccessible:
            ++result.inaccessible_location_count;
            break;
        }

        if (!entry.native_key_reliable || entry.native_key == 0U) {
            result.status = BaselineExclusionStatus::Blocked;
            continue;
        }
        ++result.reliable_entry_count;
        reliable_baseline_hwnds.insert(entry.native_key);
        forbidden.insert(entry.native_key);
    }

    result.reliable_hwnd_count = reliable_baseline_hwnds.size();
    result.forbidden_preexisting_hwnds.assign(forbidden.begin(),
                                               forbidden.end());
    return result;
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

ConsentCandidateEvaluation evaluate_consent_candidate_inventory(
    const InventoryModel& post_confirmation,
    const std::span<const NativeWindowKey> forbidden_preexisting_hwnds,
    const FilesystemLocationIdentity& target_location,
    const InventoryFingerprint* const frozen_candidate) {
    ConsentCandidateEvaluation result;
    if (!post_confirmation.complete) {
        result.reason = ExplorerEligibilityReason::InventoryUnavailable;
        return result;
    }
    if (!inventory_has_unique_nonzero_keys(post_confirmation)) {
        result.reason = ExplorerEligibilityReason::InventoryUnstable;
        return result;
    }

    std::set<NativeWindowKey> forbidden;
    for (const auto native_key : forbidden_preexisting_hwnds) {
        if (native_key != 0U) {
            forbidden.insert(native_key);
        }
    }

    const auto location_is_exact = [&target_location](
                                       const InventoryLocationFingerprint&
                                           location) noexcept {
        return location.status == ShellLocationStatus::Filesystem &&
               location.filesystem_location.has_value() &&
               *location.filesystem_location == target_location;
    };
    const auto window_has_exact_location = [&location_is_exact](
                                               const InventoryFingerprint&
                                                   window) noexcept {
        return std::any_of(window.locations.begin(),
                           window.locations.end(),
                           location_is_exact);
    };

    std::vector<const InventoryFingerprint*> candidates;
    for (const auto& window : post_confirmation.windows) {
        const bool exact = window_has_exact_location(window);
        if (exact) {
            ++result.exact_target_window_count;
        }
        if (forbidden.contains(window.native_key)) {
            result.forbidden_window_at_target |= exact;
            continue;
        }
        candidates.push_back(&window);
    }

    result.non_forbidden_window_count = candidates.size();
    if (result.forbidden_window_at_target) {
        result.reason = ExplorerEligibilityReason::PreexistingWindow;
        return result;
    }
    if (candidates.empty()) {
        result.reason = ExplorerEligibilityReason::TargetNotFound;
        return result;
    }
    if (candidates.size() != 1U) {
        result.reason = ExplorerEligibilityReason::AmbiguousCandidate;
        return result;
    }

    const auto& candidate = *candidates.front();
    result.candidate_fingerprint = candidate;
    if (candidate.shell_entry_count != 1U ||
        candidate.locations.size() != 1U) {
        result.reason = ExplorerEligibilityReason::AmbiguousCandidate;
        return result;
    }
    const auto& location = candidate.locations.front();
    if (location.status == ShellLocationStatus::NavigationPending) {
        result.reason = ExplorerEligibilityReason::LocationNotReady;
        return result;
    }
    result.exact_unique_location = location_is_exact(location);
    if (!result.exact_unique_location) {
        result.reason = ExplorerEligibilityReason::LocationMismatch;
        return result;
    }

    result.frozen_candidate_matched =
        frozen_candidate == nullptr || candidate == *frozen_candidate;
    if (!result.frozen_candidate_matched) {
        result.reason = ExplorerEligibilityReason::TargetInvalidated;
        return result;
    }
    result.reason = ExplorerEligibilityReason::Eligible;
    return result;
}

ConsentGenerationStatus evaluate_consent_generation_chain(
    const ConsentGenerationChain& chain) noexcept {
    if (chain.baseline_generation == 0U) {
        return ConsentGenerationStatus::MissingBaseline;
    }
    if (chain.target_prompt_generation == 0U) {
        return ConsentGenerationStatus::MissingTargetPrompt;
    }
    if (chain.target_confirmation_generation == 0U) {
        return ConsentGenerationStatus::MissingTargetConfirmation;
    }
    if (chain.eligibility_generation == 0U) {
        return ConsentGenerationStatus::MissingEligibility;
    }
    if (chain.token_generation == 0U) {
        return ConsentGenerationStatus::MissingToken;
    }
    if (chain.move_prompt_generation == 0U) {
        return ConsentGenerationStatus::MissingMovePrompt;
    }
    if (chain.move_confirmation_generation == 0U) {
        return ConsentGenerationStatus::MissingMoveConfirmation;
    }
    if (!(chain.baseline_generation < chain.target_prompt_generation &&
          chain.target_prompt_generation <
              chain.target_confirmation_generation &&
          chain.target_confirmation_generation <
              chain.eligibility_generation &&
          chain.eligibility_generation < chain.token_generation &&
          chain.token_generation < chain.move_prompt_generation &&
          chain.move_prompt_generation <
              chain.move_confirmation_generation)) {
        return ConsentGenerationStatus::NotStrictlyIncreasing;
    }
    return ConsentGenerationStatus::Valid;
}

ExplorerEligibilityReason ConsentMoveAuthority::authorize(
    const bool target_confirmation_present,
    const bool move_confirmation_present,
    const ConsentGenerationChain& chain) noexcept {
    if (consumed_) {
        return ExplorerEligibilityReason::OperationLimitReached;
    }
    if (authorized_) {
        return ExplorerEligibilityReason::OperationSequenceViolation;
    }
    if (!target_confirmation_present) {
        return ExplorerEligibilityReason::TargetConsentRequired;
    }
    if (!move_confirmation_present) {
        return ExplorerEligibilityReason::MoveConsentRequired;
    }
    if (evaluate_consent_generation_chain(chain) !=
        ConsentGenerationStatus::Valid) {
        return ExplorerEligibilityReason::ConsentGenerationMismatch;
    }
    token_consent_generation_ = chain.token_generation;
    authorized_ = true;
    return ExplorerEligibilityReason::Eligible;
}

ExplorerEligibilityReason ConsentMoveAuthority::try_consume(
    const std::uint64_t token_consent_generation) noexcept {
    if (consumed_) {
        return ExplorerEligibilityReason::OperationLimitReached;
    }
    if (!authorized_) {
        return ExplorerEligibilityReason::MoveConsentRequired;
    }
    if (token_consent_generation == 0U ||
        token_consent_generation != token_consent_generation_) {
        return ExplorerEligibilityReason::ConsentGenerationMismatch;
    }
    consumed_ = true;
    return ExplorerEligibilityReason::Eligible;
}

ShellReceiptSequence::ShellReceiptSequence(
    const std::uint64_t first_sequence) noexcept
    : sequence_(first_sequence) {}

ShellReceiptIssueResult ShellReceiptSequence::issue(
    const ShellRegistrationEventKind kind,
    const std::uint64_t subscription_generation,
    const std::int32_t cookie) noexcept {
    const auto sequence = sequence_.issue();
    if (sequence == 0U) {
        return {ShellReceiptIssueStatus::SequenceExhausted, std::nullopt};
    }
    return {ShellReceiptIssueStatus::Issued,
            ShellRegistrationReceipt{
                sequence, subscription_generation, cookie, kind}};
}

RegistrationCorrelationEvaluation evaluate_registration_correlation(
    const RegistrationCorrelationContext& context,
    const std::span<const RegistrationWitness> witnesses) {
    RegistrationCorrelationEvaluation result;
    result.observed_registration_count = witnesses.size();
    if (witnesses.empty()) {
        return result;
    }

    bool generation_mismatch = context.subscription_generation == 0U;
    bool matching_registration_revoked = false;
    bool preexisting_window = false;
    bool three_way_window_mismatch = false;
    bool target_location_mismatch = false;
    std::vector<std::int32_t> matching_cookies;

    for (const auto& witness : witnesses) {
        if (witness.subscription_generation !=
            context.subscription_generation) {
            generation_mismatch = true;
            continue;
        }
        if (witness.receipt_sequence == 0U || !witness.cookie_resolved) {
            ++result.unresolved_registration_count;
            continue;
        }
        if (!witness.canonical_com_identity_matches) {
            const bool shares_target_window =
                context.automation_window != 0U &&
                context.automation_window ==
                    context.live_eligibility_window &&
                witness.cookie_resolved_window ==
                    context.automation_window;
            if (shares_target_window) {
                ++result.shared_window_identity_conflict_count;
            } else {
                ++result.unrelated_registration_count;
            }
            continue;
        }
        if (witness.revoked_before_issuance) {
            matching_registration_revoked = true;
            continue;
        }

        const bool three_way_matches =
            context.automation_window != 0U &&
            context.automation_window == witness.cookie_resolved_window &&
            context.automation_window == context.live_eligibility_window;
        if (!three_way_matches) {
            three_way_window_mismatch = true;
            continue;
        }
        if (std::find(context.forbidden_preexisting_hwnds.begin(),
                      context.forbidden_preexisting_hwnds.end(),
                      context.automation_window) !=
            context.forbidden_preexisting_hwnds.end()) {
            preexisting_window = true;
            continue;
        }
        if (!witness.live_filesystem_location.has_value() ||
            *witness.live_filesystem_location !=
                context.expected_target_location) {
            target_location_mismatch = true;
            continue;
        }

        if (std::find(matching_cookies.begin(),
                      matching_cookies.end(),
                      witness.cookie) == matching_cookies.end()) {
            matching_cookies.push_back(witness.cookie);
        }
    }

    result.matching_cookie_count = matching_cookies.size();
    if (matching_cookies.size() == 1U) {
        result.matching_cookie = matching_cookies.front();
    }

    if (generation_mismatch) {
        result.status =
            RegistrationCorrelationStatus::SubscriptionGenerationMismatch;
        return result;
    }
    if (result.unresolved_registration_count != 0U) {
        result.status =
            RegistrationCorrelationStatus::UnresolvedRegistration;
        return result;
    }
    if (result.shared_window_identity_conflict_count != 0U) {
        result.status =
            RegistrationCorrelationStatus::SharedWindowIdentityConflict;
        return result;
    }
    if (matching_registration_revoked) {
        result.status =
            RegistrationCorrelationStatus::MatchingRegistrationRevoked;
        return result;
    }
    if (preexisting_window) {
        result.status = RegistrationCorrelationStatus::PreexistingWindow;
        return result;
    }
    if (three_way_window_mismatch) {
        result.status =
            RegistrationCorrelationStatus::ThreeWayWindowMismatch;
        return result;
    }
    if (target_location_mismatch) {
        result.status =
            RegistrationCorrelationStatus::TargetLocationMismatch;
        return result;
    }
    if (matching_cookies.size() > 1U) {
        result.status =
            RegistrationCorrelationStatus::AmbiguousMatchingRegistrations;
        return result;
    }
    if (matching_cookies.size() == 1U) {
        result.status = RegistrationCorrelationStatus::Eligible;
        return result;
    }

    result.status = RegistrationCorrelationStatus::UnrelatedRegistrationsOnly;
    return result;
}

LeaseCleanupAuthorization evaluate_lease_cleanup_authorization(
    const LeaseCleanupFacts& facts) noexcept {
    if (!facts.retained_automation_object_available) {
        return LeaseCleanupAuthorization::RetainedObjectUnavailable;
    }
    if (!facts.lease_belongs_to_current_session) {
        return LeaseCleanupAuthorization::WrongSession;
    }
    if (!facts.created_by_current_cocreate) {
        return LeaseCleanupAuthorization::WrongCreationAuthority;
    }
    if (!facts.subscription_generation_matches) {
        return LeaseCleanupAuthorization::SubscriptionGenerationMismatch;
    }
    if (!facts.canonical_identity_available) {
        return LeaseCleanupAuthorization::CanonicalIdentityUnavailable;
    }
    if (facts.canonical_identity_ambiguous) {
        return LeaseCleanupAuthorization::CanonicalIdentityAmbiguous;
    }
    if (!facts.window_not_preexisting) {
        return LeaseCleanupAuthorization::PreexistingWindow;
    }
    if (!facts.window_identity_matches) {
        return LeaseCleanupAuthorization::WindowIdentityMismatch;
    }
    if (!facts.canonical_identity_matches) {
        return LeaseCleanupAuthorization::CanonicalIdentityMismatch;
    }
    switch (facts.navigation) {
    case LeaseNavigationDisposition::NotStarted:
    case LeaseNavigationDisposition::PendingExpectedTarget:
    case LeaseNavigationDisposition::ExactExpectedTarget:
        return LeaseCleanupAuthorization::Authorized;
    case LeaseNavigationDisposition::LeftExpectedTarget:
    case LeaseNavigationDisposition::Unknown:
        return LeaseCleanupAuthorization::NavigationUnsafe;
    }
    return LeaseCleanupAuthorization::NavigationUnsafe;
}

bool browser_navigation_history_ambiguous(
    const std::uint64_t current_matching_navigate_complete_count,
    const std::optional<std::uint64_t> issued_baseline_count) noexcept {
    if (!issued_baseline_count.has_value()) {
        return current_matching_navigate_complete_count > 1U;
    }
    return current_matching_navigate_complete_count >
           *issued_baseline_count;
}

bool provisioning_attempt_passed(
    const ProvisioningAttemptSummary& attempt) noexcept {
    return attempt.baseline_exclusion_complete &&
           attempt.registration_subscription_active &&
           attempt.exactly_one_target &&
           attempt.target_not_preexisting &&
           attempt.matching_cookie_unique &&
           attempt.canonical_identity_matches &&
           attempt.three_way_window_matches &&
           attempt.exact_target_location &&
           attempt.full_eligibility_passed &&
           attempt.safe_cleanup_succeeded &&
           attempt.no_attributable_orphan &&
           attempt.existing_windows_untouched &&
           attempt.translation_not_attempted;
}

ProvisioningStabilityEvaluation evaluate_provisioning_stability(
    const std::span<const ProvisioningAttemptSummary> attempts) noexcept {
    constexpr std::size_t required_attempt_count = 3U;
    ProvisioningStabilityEvaluation result;
    result.attempt_count = attempts.size();
    result.fixed_attempt_count_violated =
        attempts.size() > required_attempt_count;

    for (std::size_t index = 0; index < attempts.size(); ++index) {
        if (attempts[index].attempt_index != index + 1U) {
            result.attempt_sequence_valid = false;
        }
        if (provisioning_attempt_passed(attempts[index])) {
            ++result.passing_attempt_count;
        } else if (!result.first_failing_attempt.has_value()) {
            result.first_failing_attempt = attempts[index].attempt_index;
        }
    }

    if (result.first_failing_attempt.has_value() ||
        result.fixed_attempt_count_violated ||
        !result.attempt_sequence_valid) {
        result.status = ProvisioningStabilityGateStatus::Blocked;
    } else if (attempts.size() == required_attempt_count) {
        result.status = ProvisioningStabilityGateStatus::Pass;
    }
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

ExplorerEligibilityReason evaluate_consent_live_eligibility(
    const ConsentLiveEligibilityFacts& facts) noexcept {
    if (!facts.token_active) {
        return ExplorerEligibilityReason::StaleToken;
    }
    if (!facts.candidate_fingerprint_stable ||
        !facts.navigation_epoch_stable) {
        return ExplorerEligibilityReason::TargetInvalidated;
    }
    return evaluate_eligibility_model(facts.eligibility);
}

ExplorerEligibilityReason evaluate_consent_restore_eligibility(
    const ConsentRestoreEligibilityFacts& facts) noexcept {
    const auto sequence = evaluate_restore_gate(
        facts.live.token_active,
        facts.primary_attempted,
        facts.primary_before_available,
        facts.primary_actual_available,
        facts.restore_allowance_consumed);
    if (sequence != ExplorerEligibilityReason::Eligible) {
        return sequence;
    }
    return evaluate_consent_live_eligibility(facts.live);
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
    case SubscribeShellEvents:
        return "subscribe_shell_events";
    case ResolveRegistrationCookie:
        return "resolve_registration_cookie";
    case QueryCanonicalIdentity:
        return "query_canonical_identity";
    case CreateBrowserWindow:
        return "create_browser_window";
    case SubscribeBrowserEvents:
        return "subscribe_browser_events";
    case AwaitBrowserReadiness:
        return "await_browser_readiness";
    case UnsubscribeBrowserEvents:
        return "unsubscribe_browser_events";
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
    case SubscribeShellEvents:
        return "IConnectionPoint::Advise(DShellWindowsEvents)";
    case ResolveRegistrationCookie:
        return "IShellWindows::FindWindowSW";
    case QueryCanonicalIdentity:
        return "IUnknown::QueryInterface(IID_IUnknown)";
    case CreateBrowserWindow:
        return "CoCreateInstance(CLSID_ShellBrowserWindow)";
    case SubscribeBrowserEvents:
        return "IConnectionPoint::Advise(DWebBrowserEvents2)";
    case AwaitBrowserReadiness:
        return "MsgWaitForMultipleObjectsEx";
    case UnsubscribeBrowserEvents:
        return "IConnectionPoint::Unadvise";
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
    ExplorerAuthorityKind authority_kind{
        ExplorerAuthorityKind::LegacyAutoProvisionDiagnostic};
    std::filesystem::path target_directory;
    FilesystemLocationIdentity target_location;
    std::vector<detail::NativeWindowKey> forbidden_preexisting_hwnds;
    std::unique_ptr<ShellWindowsSubscription> shell_subscription;
    std::unique_ptr<ExplorerProvisioningLease> provisioning_lease;
    std::unique_ptr<ExplorerConsentTargetObservation> consent_observation;
    std::optional<detail::InventoryFingerprint>
        consent_candidate_fingerprint;
    detail::ConsentMoveAuthority consent_move_authority;
    ExplorerConsentFacts consent;
    std::set<long> registered_cookies;
    std::set<long> revoked_cookies;
    std::set<long> unresolved_cookies;
    std::set<long> unrelated_cookies;
    std::map<long, detail::NativeWindowKey> unrelated_cookie_hwnds;
    std::set<long> shared_window_identity_conflict_cookies;
    std::set<long> matching_cookies;
    std::map<long, detail::NativeWindowKey> matching_cookie_hwnds;
    std::optional<long> matching_cookie;
    std::uint64_t shell_subscription_generation{};
    detail::LeaseNavigationDisposition navigation{
        detail::LeaseNavigationDisposition::NotStarted};
    bool matching_registration_revoked{};
    bool receipt_generation_mismatch{};
    bool cookie_lifecycle_ambiguous{};
    std::uint64_t issued_matching_navigate_complete_count{};
    bool browser_navigation_history_ambiguous{};
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
    std::uint64_t glue_authority_id{};
    std::uint64_t glue_authority_generation{};
    detail::NativeWindowKey glue_peer_native_key{};
    FilesystemLocationIdentity glue_peer_target_location{};
    DWORD glue_peer_process_id{};
    DWORD glue_peer_thread_id{};
    std::uint64_t glue_peer_capability_generation{};
    std::uint64_t glue_peer_consent_generation{};
    std::uint8_t glue_role{};
    std::size_t glue_native_operation_count{};
    bool glue_bound{};
    bool closing{};

    ~Impl() {
        const bool on_owner_sta =
            GetCurrentThreadId() == controller_thread_id;
        if (on_owner_sta) {
            bool lifecycle_safe = true;
            if (consent_observation != nullptr) {
                const HRESULT retired = consent_observation->retire();
                if (SUCCEEDED(retired)) {
                    consent_observation.reset();
                } else {
                    // A still-advised STA sink and its browser proxy must
                    // outlive the wrapper and apartment. Preserve them rather
                    // than releasing or uninitializing COM unsafely.
                    static_cast<void>(consent_observation.release());
                    lifecycle_safe = false;
                    com_initialized = false;
                }
            }
            if (provisioning_lease != nullptr) {
                const auto before =
                    provisioning_lease->browser_readiness_facts();
                static_cast<void>(
                    provisioning_lease->close_browser_events());
                const auto after =
                    provisioning_lease->browser_readiness_facts();
                const bool browser_lifecycle_clean =
                    after.unadvised && after.malformed_count == 0U &&
                    after.overflow_count == 0U &&
                    after.wrong_thread_count == 0U &&
                    after.post_retirement_count == 0U &&
                    after.identity_query_failure_count == 0U;
                const bool browser_safe =
                    !before.subscribed || browser_lifecycle_clean;
                provisioning.cleanup.browser_events_unadvised =
                    after.unadvised;
                provisioning.cleanup.browser_event_lifecycle_clean =
                    browser_lifecycle_clean;
                lifecycle_safe = lifecycle_safe && browser_safe;
            }
            if (shell_subscription != nullptr) {
                static_cast<void>(shell_subscription->close());
                const auto after = shell_subscription->facts();
                const bool shell_lifecycle_clean =
                    after.unadvised && after.malformed_count == 0U &&
                    after.overflow_count == 0U &&
                    after.wrong_thread_count == 0U &&
                    after.post_retirement_count == 0U;
                provisioning.cleanup.shell_events_unadvised =
                    after.unadvised;
                provisioning.cleanup.shell_event_lifecycle_clean =
                    shell_lifecycle_clean;
                lifecycle_safe = lifecycle_safe && shell_lifecycle_clean;
            }
            if (lifecycle_safe) {
                provisioning_lease.reset();
                shell_subscription.reset();
            } else {
                // A still-advised STA sink must outlive its wrapper and COM
                // apartment. Leaking the exact fixture objects is the
                // fail-safe path; it never authorizes a fallback close.
                static_cast<void>(provisioning_lease.release());
                static_cast<void>(shell_subscription.release());
                com_initialized = false;
            }
        } else {
            static_cast<void>(provisioning_lease.release());
            static_cast<void>(shell_subscription.release());
            com_initialized = false;
        }
        process.reset();
        if (com_initialized && on_owner_sta) {
            CoUninitialize();
        }
    }
};

struct ExplorerConsentProvisioning::Impl final {
    bool com_initialized{};
    DWORD controller_thread_id{};
    std::filesystem::path target_directory;
    FilesystemLocationIdentity target_location;
    std::vector<detail::NativeWindowKey> forbidden_preexisting_hwnds;
    ExplorerConsentFacts facts;
    std::uint64_t next_protocol_generation{1U};
    bool terminal{};

    [[nodiscard]] std::uint64_t issue_protocol_generation() noexcept {
        if (next_protocol_generation == 0U ||
            next_protocol_generation ==
                std::numeric_limits<std::uint64_t>::max()) {
            return 0U;
        }
        return next_protocol_generation++;
    }

    ~Impl() {
        const bool on_owner_sta =
            GetCurrentThreadId() == controller_thread_id;
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

enum class OwnerStaPumpStatus {
    ActivityDispatched,
    NoPendingMessage,
    TimedOut,
    QuitObserved,
    Failed,
};

struct OwnerStaPumpResult {
    OwnerStaPumpStatus status{OwnerStaPumpStatus::Failed};
    HRESULT diagnostic{E_FAIL};
};

[[nodiscard]] OwnerStaPumpResult pump_owner_sta_once(
    const std::chrono::steady_clock::time_point deadline) noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return {OwnerStaPumpStatus::TimedOut,
                HRESULT_FROM_WIN32(ERROR_TIMEOUT)};
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - now);
    const auto bounded = std::clamp<std::int64_t>(
        remaining.count() + 1, 1, std::numeric_limits<DWORD>::max() - 1);
    const DWORD wait_result = MsgWaitForMultipleObjectsEx(
        0U,
        nullptr,
        static_cast<DWORD>(bounded),
        QS_ALLINPUT,
        MWMO_INPUTAVAILABLE | MWMO_ALERTABLE);
    if (wait_result == WAIT_TIMEOUT) {
        return {OwnerStaPumpStatus::TimedOut,
                HRESULT_FROM_WIN32(ERROR_TIMEOUT)};
    }
    if (wait_result == WAIT_FAILED) {
        return {OwnerStaPumpStatus::Failed,
                HRESULT_FROM_WIN32(GetLastError())};
    }
    if (wait_result == WAIT_IO_COMPLETION) {
        return {OwnerStaPumpStatus::ActivityDispatched, S_OK};
    }

    MSG message{};
    if (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) == FALSE) {
        return {OwnerStaPumpStatus::ActivityDispatched, S_OK};
    }
    if (message.message == WM_QUIT) {
        PostQuitMessage(static_cast<int>(message.wParam));
        return {OwnerStaPumpStatus::QuitObserved,
                HRESULT_FROM_WIN32(ERROR_CANCELLED)};
    }
    static_cast<void>(TranslateMessage(&message));
    static_cast<void>(DispatchMessageW(&message));
    return {OwnerStaPumpStatus::ActivityDispatched, S_OK};
}

[[nodiscard]] OwnerStaPumpResult dispatch_one_pending_owner_message()
    noexcept {
    MSG message{};
    if (PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) == FALSE) {
        return {OwnerStaPumpStatus::NoPendingMessage, S_FALSE};
    }
    if (message.message == WM_QUIT) {
        PostQuitMessage(static_cast<int>(message.wParam));
        return {OwnerStaPumpStatus::QuitObserved,
                HRESULT_FROM_WIN32(ERROR_CANCELLED)};
    }
    static_cast<void>(TranslateMessage(&message));
    static_cast<void>(DispatchMessageW(&message));
    return {OwnerStaPumpStatus::ActivityDispatched, S_OK};
}

[[nodiscard]] detail::LeaseNavigationDisposition advance_navigation_state(
    const detail::LeaseNavigationDisposition current,
    const ShellLocationFact& location,
    const FilesystemLocationIdentity& expected) noexcept {
    if (current == detail::LeaseNavigationDisposition::LeftExpectedTarget ||
        current == detail::LeaseNavigationDisposition::Unknown) {
        return current;
    }
    if (location.filesystem()) {
        return location.identity == expected
                   ? detail::LeaseNavigationDisposition::ExactExpectedTarget
                   : detail::LeaseNavigationDisposition::LeftExpectedTarget;
    }
    if (location.status == ShellLocationStatus::NavigationPending ||
        location.status == ShellLocationStatus::Empty) {
        return current ==
                       detail::LeaseNavigationDisposition::ExactExpectedTarget
                   ? detail::LeaseNavigationDisposition::Unknown
                   : detail::LeaseNavigationDisposition::
                         PendingExpectedTarget;
    }
    return detail::LeaseNavigationDisposition::Unknown;
}

template <typename ImplType>
[[nodiscard]] bool forbidden_preexisting(
    const ImplType& impl,
    const detail::NativeWindowKey native_key) noexcept {
    return std::find(impl.forbidden_preexisting_hwnds.begin(),
                     impl.forbidden_preexisting_hwnds.end(),
                     native_key) != impl.forbidden_preexisting_hwnds.end();
}

template <typename ImplType>
void copy_event_facts(ImplType& impl) noexcept {
    if (impl.shell_subscription != nullptr) {
        const auto facts = impl.shell_subscription->facts();
        impl.provisioning.shell_subscription_advised = facts.subscribed;
        impl.provisioning.shell_subscription_generation =
            facts.subscription_generation;
        impl.provisioning.shell_callback_count = facts.callback_count;
        impl.provisioning.shell_malformed_event_count = facts.malformed_count;
        impl.provisioning.shell_overflow_event_count = facts.overflow_count;
        impl.provisioning.shell_wrong_thread_event_count =
            facts.wrong_thread_count;
        impl.provisioning.shell_post_retirement_event_count =
            facts.post_retirement_count;
    }
    if (impl.provisioning_lease != nullptr) {
        const auto facts =
            impl.provisioning_lease->browser_readiness_facts();
        impl.provisioning.browser_subscription_advised = facts.subscribed;
        impl.provisioning.browser_navigate_complete_count =
            facts.navigate_complete_count;
        impl.provisioning.browser_matching_navigate_complete_count =
            facts.matching_navigate_complete_count;
        impl.provisioning.browser_unrelated_navigate_complete_count =
            facts.unrelated_navigate_complete_count;
        impl.provisioning.browser_identity_query_failure_count =
            facts.identity_query_failure_count;
        impl.provisioning.browser_quit_event_count = facts.quit_count;
        impl.provisioning.browser_malformed_event_count = facts.malformed_count;
        impl.provisioning.browser_overflow_event_count = facts.overflow_count;
        impl.provisioning.browser_wrong_thread_event_count =
            facts.wrong_thread_count;
        impl.provisioning.browser_post_retirement_event_count =
            facts.post_retirement_count;
    }
}

template <typename ImplType>
[[nodiscard]] bool event_streams_healthy(
    const ImplType& impl) noexcept {
    if (impl.shell_subscription == nullptr ||
        impl.provisioning_lease == nullptr) {
        return false;
    }
    const auto shell = impl.shell_subscription->facts();
    const auto browser = impl.provisioning_lease->browser_readiness_facts();
    return shell.subscribed && shell.accepting &&
           shell.subscription_generation ==
               impl.shell_subscription_generation &&
           !impl.receipt_generation_mismatch &&
           !impl.cookie_lifecycle_ambiguous &&
           shell.malformed_count == 0U && shell.overflow_count == 0U &&
           shell.wrong_thread_count == 0U &&
           shell.post_retirement_count == 0U && browser.subscribed &&
           browser.accepting && browser.subscription_diagnostic == S_OK &&
           browser.malformed_count == 0U && browser.overflow_count == 0U &&
           browser.wrong_thread_count == 0U &&
           browser.post_retirement_count == 0U &&
           browser.identity_query_failure_count == 0U &&
           browser.quit_count == 0U;
}

template <typename ImplType>
void resolve_registration_cookie(ImplType& impl,
                                 const long cookie) {
    if (impl.shell_subscription == nullptr ||
        impl.provisioning_lease == nullptr || cookie == 0L) {
        impl.unresolved_cookies.insert(cookie);
        return;
    }
    auto resolution = impl.shell_subscription->resolve_cookie(cookie);
    if (!resolution.succeeded()) {
        impl.unresolved_cookies.insert(cookie);
        return;
    }
    impl.unresolved_cookies.erase(cookie);
    const auto native_key = reinterpret_cast<detail::NativeWindowKey>(
        resolution.resolved.window());
    if (resolution.resolved.same_object(*impl.provisioning_lease)) {
        const auto prior_matching =
            impl.matching_cookie_hwnds.find(cookie);
        if (impl.unrelated_cookies.contains(cookie) ||
            (prior_matching != impl.matching_cookie_hwnds.end() &&
             prior_matching->second != native_key)) {
            impl.cookie_lifecycle_ambiguous = true;
            impl.provisioning_lease->mark_identity_ambiguous();
            return;
        }
        impl.matching_cookies.insert(cookie);
        impl.matching_cookie_hwnds[cookie] = native_key;
        if (impl.revoked_cookies.contains(cookie)) {
            impl.matching_registration_revoked = true;
        }
        if (impl.matching_cookies.size() > 1U) {
            impl.provisioning_lease->mark_identity_ambiguous();
        }
        return;
    }
    const auto prior_unrelated = impl.unrelated_cookie_hwnds.find(cookie);
    if (impl.matching_cookies.contains(cookie) ||
        (prior_unrelated != impl.unrelated_cookie_hwnds.end() &&
         prior_unrelated->second != native_key)) {
        impl.cookie_lifecycle_ambiguous = true;
        impl.provisioning_lease->mark_identity_ambiguous();
        return;
    }
    impl.unrelated_cookies.insert(cookie);
    impl.unrelated_cookie_hwnds[cookie] = native_key;
}

template <typename ImplType>
void refresh_registration_state(ImplType& impl) {
    if (impl.provisioning_lease != nullptr) {
        const auto before =
            impl.provisioning_lease->browser_readiness_facts();
        static_cast<void>(impl.provisioning_lease->wait_for_browser_activity(
            before.latest_sequence, std::chrono::steady_clock::now()));
    }

    if (impl.shell_subscription != nullptr) {
        const auto receipts = impl.shell_subscription->take_receipts();
        for (const auto& receipt : receipts) {
            if (receipt.subscription_generation !=
                impl.shell_subscription_generation) {
                impl.receipt_generation_mismatch = true;
                continue;
            }
            if (receipt.kind == ShellWindowReceiptKind::Registered) {
                ++impl.provisioning.shell_registered_event_count;
                if (impl.registered_cookies.contains(receipt.cookie)) {
                    if (impl.revoked_cookies.contains(receipt.cookie)) {
                        // Re-registration after revoke is cookie lifecycle
                        // reuse, not a duplicate live receipt. Never refresh
                        // authority across that boundary.
                        impl.cookie_lifecycle_ambiguous = true;
                        impl.provisioning_lease->mark_identity_ambiguous();
                    } else {
                        resolve_registration_cookie(impl, receipt.cookie);
                    }
                    continue;
                }
                impl.registered_cookies.insert(receipt.cookie);
                resolve_registration_cookie(impl, receipt.cookie);
            } else {
                ++impl.provisioning.shell_revoked_event_count;
                impl.revoked_cookies.insert(receipt.cookie);
                if (impl.matching_cookies.contains(receipt.cookie)) {
                    impl.matching_registration_revoked = true;
                }
            }
        }
        const std::vector<long> retry(impl.unresolved_cookies.begin(),
                                      impl.unresolved_cookies.end());
        for (const long cookie : retry) {
            if (!impl.revoked_cookies.contains(cookie)) {
                resolve_registration_cookie(impl, cookie);
            }
        }
    }

    const auto lease_handle = impl.provisioning_lease == nullptr
                                  ? ShellWindowHandleResult{}
                                  : impl.provisioning_lease->current_hwnd();
    const auto lease_key = lease_handle.succeeded()
                               ? reinterpret_cast<detail::NativeWindowKey>(
                                     lease_handle.window)
                               : 0U;
    impl.provisioning.lease_hwnd_resolved =
        impl.provisioning.lease_hwnd_resolved || lease_key != 0U;
    if (lease_key != 0U) {
        for (const auto& [cookie, unrelated_key] :
             impl.unrelated_cookie_hwnds) {
            if (unrelated_key == lease_key) {
                impl.shared_window_identity_conflict_cookies.insert(cookie);
                impl.provisioning_lease->mark_identity_ambiguous();
            }
        }
    }

    impl.matching_cookie = impl.matching_cookies.size() == 1U
                               ? std::optional<long>{
                                     *impl.matching_cookies.begin()}
                               : std::nullopt;
    impl.provisioning.registered_cookie_count =
        impl.registered_cookies.size();
    impl.provisioning.revoked_cookie_count = impl.revoked_cookies.size();
    impl.provisioning.unrelated_registration_count =
        impl.unrelated_cookies.size();
    impl.provisioning.unresolved_registration_count =
        impl.unresolved_cookies.size();
    impl.provisioning.shared_window_identity_conflict_count =
        impl.shared_window_identity_conflict_cookies.size();
    impl.provisioning.matching_registration_count =
        impl.matching_cookies.size();
    impl.provisioning.shell_subscription_generation_mismatch =
        impl.receipt_generation_mismatch;
    impl.provisioning.shell_cookie_lifecycle_ambiguous =
        impl.cookie_lifecycle_ambiguous;
    impl.provisioning.matching_cookie = impl.matching_cookie.has_value()
                                            ? std::optional<std::int32_t>{
                                                  *impl.matching_cookie}
                                            : std::nullopt;
    copy_event_facts(impl);
    const auto issued_navigation_baseline =
        impl.provisioning.token_issued
            ? std::optional<std::uint64_t>{
                  impl.issued_matching_navigate_complete_count}
            : std::nullopt;
    if (detail::browser_navigation_history_ambiguous(
            impl.provisioning.browser_matching_navigate_complete_count,
            issued_navigation_baseline)) {
        impl.browser_navigation_history_ambiguous = true;
        impl.provisioning.browser_navigation_history_ambiguous = true;
        impl.navigation = detail::LeaseNavigationDisposition::Unknown;
    }
}

template <typename ImplType>
[[nodiscard]] bool drain_pending_event_messages(ImplType& impl) {
    constexpr std::size_t message_budget = 256U;
    refresh_registration_state(impl);
    for (std::size_t dispatched = 0U; dispatched < message_budget;
         ++dispatched) {
        const auto pump = dispatch_one_pending_owner_message();
        if (pump.status == OwnerStaPumpStatus::NoPendingMessage) {
            return true;
        }
        if (pump.status != OwnerStaPumpStatus::ActivityDispatched) {
            return false;
        }
        // Re-evaluate both event streams after each dispatched message. This
        // avoids letting one connection-point stream consume the wait budget
        // while evidence from the other stream is already available.
        refresh_registration_state(impl);
    }
    return false;
}

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
    std::optional<detail::ExplorerLedgerEntry> ledger_entry;
    if (token != nullptr) {
        ledger_entry = impl.ledger.resolve(*token);
        if (!ledger_entry.has_value()) {
            return {ExplorerEligibilityReason::StaleToken,
                    std::nullopt,
                    adapter_diagnostic(
                        1101U,
                        "ExplorerTokenLedger",
                        "token is not active in this Explorer session")};
        }
    }
    bool shell_entry_unique = false;
    bool current_exact_target_location = false;
    if (impl.authority_kind ==
        ExplorerAuthorityKind::LegacyAutoProvisionDiagnostic) {
    if (impl.closing || impl.window == nullptr ||
        impl.provisioning_lease == nullptr ||
        impl.shell_subscription == nullptr ||
        !impl.matching_cookie.has_value()) {
        return {ExplorerEligibilityReason::StaleToken,
                std::nullopt,
                adapter_diagnostic(1102U,
                                   "ExplorerTestSession",
                                   "Explorer session is retired or closing")};
    }

    if (!drain_pending_event_messages(impl)) {
        return {ExplorerEligibilityReason::ShellEventStreamInvalid,
                std::nullopt,
                adapter_diagnostic(1103U,
                                   "Explorer owner STA message queue",
                                   "queued event messages could not be drained within the bounded preflight budget")};
    }
    if (!event_streams_healthy(impl)) {
        return {ExplorerEligibilityReason::ShellEventStreamInvalid,
                std::nullopt,
                adapter_diagnostic(1103U,
                                   "Explorer event subscriptions",
                                   "Shell or browser event evidence is malformed, overflowed, retired, or unavailable")};
    }
    if (impl.browser_navigation_history_ambiguous) {
        return {ExplorerEligibilityReason::TargetInvalidated,
                std::nullopt,
                adapter_diagnostic(1112U,
                                   "DWebBrowserEvents2::NavigateComplete2",
                                   "same-object navigation history changed beyond the frozen issuance evidence")};
    }
    if (!impl.unresolved_cookies.empty()) {
        return {ExplorerEligibilityReason::RegistrationResolutionFailed,
                std::nullopt,
                adapter_diagnostic(1104U,
                                   "IShellWindows::FindWindowSW",
                                   "an additional registration remains unresolved, so target uniqueness is unproven")};
    }
    if (impl.shared_window_identity_conflict_cookies.size() != 0U ||
        impl.matching_cookies.size() > 1U ||
        impl.provisioning_lease->facts().identity_ambiguous) {
        return {ExplorerEligibilityReason::AmbiguousCandidate,
                std::nullopt,
                adapter_diagnostic(1105U,
                                   "Explorer registration correlation",
                                   "multiple matching cookies or a shared-HWND COM identity conflict invalidated authority")};
    }
    if (impl.matching_registration_revoked ||
        impl.revoked_cookies.contains(*impl.matching_cookie)) {
        return {ExplorerEligibilityReason::RegistrationRevoked,
                std::nullopt,
                adapter_diagnostic(1106U,
                                   "DShellWindowsEvents::WindowRevoked",
                                   "matching Explorer registration was revoked")};
    }
    if (ledger_entry.has_value() &&
        (ledger_entry->registration_cookie != *impl.matching_cookie ||
         ledger_entry->subscription_generation !=
             impl.shell_subscription_generation)) {
        return {ExplorerEligibilityReason::SubscriptionGenerationMismatch,
                std::nullopt,
                adapter_diagnostic(1107U,
                                   "ExplorerTokenLedger",
                                   "token cookie or subscription generation is stale")};
    }

    auto resolved =
        impl.shell_subscription->resolve_cookie(*impl.matching_cookie);
    if (!resolved.succeeded()) {
        return {ExplorerEligibilityReason::RegistrationResolutionFailed,
                std::nullopt,
                resolved.diagnostic.has_value()
                    ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                          *resolved.diagnostic,
                          "matching registration cookie no longer resolves")}
                    : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                          1108U,
                          "IShellWindows::FindWindowSW",
                          "matching registration cookie returned no window")}};
    }
    impl.provisioning.matching_cookie_find_window_resolved = true;
    impl.provisioning.cookie_hwnd_resolved =
        impl.provisioning.cookie_hwnd_resolved ||
        resolved.resolved.window() != nullptr;
    if (!resolved.resolved.same_object(*impl.provisioning_lease)) {
        if (resolved.resolved.window() == impl.window) {
            impl.provisioning_lease->mark_identity_ambiguous();
        }
        return {ExplorerEligibilityReason::CanonicalIdentityMismatch,
                std::nullopt,
                adapter_diagnostic(1109U,
                                   "IUnknown identity",
                                   "cookie-resolved dispatch is not the provisioning lease object")};
    }
    impl.provisioning.canonical_iunknown_identity_matches = true;

    const auto lease_handle = impl.provisioning_lease->current_hwnd();
    impl.provisioning.lease_hwnd_resolved =
        impl.provisioning.lease_hwnd_resolved || lease_handle.succeeded();
    const bool live_eligibility_hwnd_resolved =
        impl.window != nullptr && IsWindow(impl.window) != FALSE;
    impl.provisioning.live_eligibility_hwnd_resolved =
        impl.provisioning.live_eligibility_hwnd_resolved ||
        live_eligibility_hwnd_resolved;
    const bool three_way_hwnd_matches =
        lease_handle.succeeded() && lease_handle.window == impl.window &&
        resolved.resolved.window() == impl.window;
    impl.provisioning.lease_cookie_live_hwnd_match =
        impl.provisioning.lease_cookie_live_hwnd_match ||
        three_way_hwnd_matches;
    if (!three_way_hwnd_matches) {
        return {ExplorerEligibilityReason::WindowDestroyed,
                std::nullopt,
                lease_handle.diagnostic.has_value()
                    ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                          *lease_handle.diagnostic,
                          "lease, cookie, and live eligibility HWND no longer agree")}
                    : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                          1110U,
                          "Explorer HWND correlation",
                          "lease, cookie, and live eligibility HWND do not agree")}};
    }
    const auto native_key = reinterpret_cast<detail::NativeWindowKey>(
        impl.window);
    const bool current_target_preexisting =
        forbidden_preexisting(impl, native_key);
    impl.provisioning.target_hwnd_preexisting =
        impl.provisioning.target_hwnd_preexisting ||
        current_target_preexisting;
    if (current_target_preexisting) {
        return {ExplorerEligibilityReason::PreexistingWindow,
                std::nullopt,
                adapter_diagnostic(1111U,
                                   "Explorer baseline exclusion",
                                   "matching object uses a permanently forbidden baseline HWND")};
    }

    const auto live_location = impl.provisioning_lease->current_location();
    current_exact_target_location =
        live_location.filesystem() &&
        live_location.identity == impl.target_location;
    impl.provisioning.exact_target_location =
        impl.provisioning.exact_target_location ||
        current_exact_target_location;
    impl.navigation = advance_navigation_state(impl.navigation,
                                               live_location,
                                               impl.target_location);
    shell_entry_unique = impl.matching_cookies.size() == 1U;
    } else {
        if (impl.closing || impl.window == nullptr ||
            impl.consent_observation == nullptr ||
            !impl.consent_candidate_fingerprint.has_value()) {
            return {ExplorerEligibilityReason::StaleToken,
                    std::nullopt,
                    adapter_diagnostic(
                        1120U,
                        "Explorer user-consent session",
                        "consent observation is unavailable or retired")};
        }
        if (ledger_entry.has_value() &&
            (ledger_entry->authority_kind !=
                 ExplorerAuthorityKind::UserConsent ||
             ledger_entry->consent_generation !=
                 impl.consent.generations.token_generation)) {
            return {ExplorerEligibilityReason::ConsentGenerationMismatch,
                    std::nullopt,
                    adapter_diagnostic(
                        1121U,
                        "ExplorerTokenLedger",
                        "token does not match this consent generation")};
        }

        auto observation_facts = impl.consent_observation->facts();
        const auto pump_deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds{20};
        constexpr std::size_t consent_message_budget = 64U;
        for (std::size_t dispatched = 0U;
             dispatched < consent_message_budget &&
             std::chrono::steady_clock::now() < pump_deadline;
             ++dispatched) {
            const auto pump = impl.consent_observation->pump_until_activity(
                observation_facts.browser.latest_sequence, pump_deadline);
            if (pump.status == BrowserReadinessWaitStatus::TimedOut) {
                break;
            }
            if (pump.status != BrowserReadinessWaitStatus::ActivityObserved) {
                return {ExplorerEligibilityReason::ShellEventStreamInvalid,
                        std::nullopt,
                        hresult_diagnostic(
                            "DWebBrowserEvents2 consent observation",
                            pump.diagnostic,
                            "consent target event stream could not be drained safely")};
            }
            observation_facts = impl.consent_observation->facts();
        }
        observation_facts = impl.consent_observation->facts();
        const auto& browser = observation_facts.browser;
        const bool observation_healthy =
            observation_facts.canonical_identity_matches &&
            browser.subscribed && browser.accepting && !browser.unadvised &&
            browser.malformed_count == 0U && browser.overflow_count == 0U &&
            browser.wrong_thread_count == 0U &&
            browser.post_retirement_count == 0U &&
            browser.identity_query_failure_count == 0U &&
            browser.unrelated_navigate_complete_count == 0U &&
            browser.quit_count == 0U && !browser.latest_activity_was_quit &&
            !observation_facts.navigation_changed_since_binding();
        if (!observation_healthy) {
            return {observation_facts.navigation_changed_since_binding()
                        ? ExplorerEligibilityReason::TargetInvalidated
                        : ExplorerEligibilityReason::ShellEventStreamInvalid,
                    std::nullopt,
                    adapter_diagnostic(
                        1122U,
                        "DWebBrowserEvents2 consent observation",
                        "target navigation history or event lifecycle changed after binding")};
        }

        const auto observed_window =
            impl.consent_observation->current_window_key();
        if (!observed_window.succeeded() ||
            observed_window.window_key !=
                reinterpret_cast<detail::NativeWindowKey>(impl.window)) {
            return {ExplorerEligibilityReason::WindowDestroyed,
                    std::nullopt,
                    observed_window.diagnostic.has_value()
                        ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                              *observed_window.diagnostic,
                              "consent-bound Explorer HWND is no longer stable")}
                        : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                              1123U,
                              "Explorer consent HWND",
                              "consent-bound Shell object changed HWND")}};
        }

        const auto inventory = capture_shell_window_inventory();
        auto inventory_model = make_inventory_model(inventory);
        if (impl.glue_peer_native_key != 0U) {
            const auto peer = std::find_if(
                inventory_model.windows.begin(),
                inventory_model.windows.end(),
                [&](const detail::InventoryFingerprint& window) {
                    return window.native_key == impl.glue_peer_native_key;
                });
            if (peer == inventory_model.windows.end()) {
                return {ExplorerEligibilityReason::TargetInvalidated,
                        std::nullopt,
                        adapter_diagnostic(
                            1126U,
                            "Explorer Glue peer inventory",
                            "the separately authorized Glue peer disappeared")};
            }
            DWORD peer_process_id = 0U;
            const HWND peer_window =
                reinterpret_cast<HWND>(impl.glue_peer_native_key);
            const DWORD peer_thread_id =
                GetWindowThreadProcessId(peer_window, &peer_process_id);
            const bool peer_location_exact =
                peer->shell_entry_count == 1U &&
                peer->locations.size() == 1U &&
                peer->locations.front().status ==
                    ShellLocationStatus::Filesystem &&
                peer->locations.front().filesystem_location.has_value() &&
                *peer->locations.front().filesystem_location ==
                    impl.glue_peer_target_location;
            if (!peer_location_exact || peer_thread_id == 0U ||
                peer_thread_id != impl.glue_peer_thread_id ||
                peer_process_id != impl.glue_peer_process_id ||
                GetAncestor(peer_window, GA_ROOT) != peer_window ||
                impl.glue_peer_capability_generation == 0U ||
                impl.glue_peer_consent_generation == 0U) {
                return {ExplorerEligibilityReason::TargetInvalidated,
                        std::nullopt,
                        adapter_diagnostic(
                            1127U,
                            "Explorer Glue peer identity",
                            "the authorized Glue peer identity or exact location changed")};
            }
            inventory_model.windows.erase(peer);
        }
        const auto selection = detail::evaluate_consent_candidate_inventory(
            inventory_model,
            impl.forbidden_preexisting_hwnds,
            impl.target_location,
            &*impl.consent_candidate_fingerprint);
        impl.consent.preexisting_exact_location_detected =
            impl.consent.preexisting_exact_location_detected ||
            selection.forbidden_window_at_target;
        impl.consent.exact_new_candidate_count =
            selection.exact_target_window_count;
        if (!selection.eligible()) {
            return {selection.reason,
                    std::nullopt,
                    adapter_diagnostic(
                        1124U,
                        "Explorer consent inventory",
                        "unique non-baseline exact-location candidate did not remain stable")};
        }

        const auto live_location =
            impl.consent_observation->current_location();
        current_exact_target_location =
            live_location.filesystem() &&
            live_location.identity == impl.target_location;
        if (!current_exact_target_location) {
            return {ExplorerEligibilityReason::LocationMismatch,
                    std::nullopt,
                    live_location.diagnostic != S_OK
                        ? std::optional<ExplorerDiagnostic>{
                              hresult_diagnostic(
                                  "IWebBrowser2::get_LocationURL",
                                  live_location.diagnostic,
                                  "consent target no longer resolves to the nonce directory")}
                        : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                              1125U,
                              "Explorer consent location",
                              "consent target navigated away from the nonce directory")}};
        }
        shell_entry_unique = true;
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

    facts.shell_entry_unique = shell_entry_unique;
    facts.location_exact =
        current_exact_target_location &&
        (impl.authority_kind == ExplorerAuthorityKind::UserConsent ||
         impl.navigation ==
             detail::LeaseNavigationDisposition::ExactExpectedTarget);

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
        impl.authority_kind == ExplorerAuthorityKind::UserConsent
            ? detail::evaluate_consent_live_eligibility(
                  {token == nullptr || ledger_entry.has_value(),
                   true,
                   true,
                   facts})
            : detail::evaluate_eligibility_model(facts);
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
    case detail::ExplorerLedgerIssueStatus::InvalidRegistrationAuthority:
        return ExplorerEligibilityReason::TargetInvalidated;
    case detail::ExplorerLedgerIssueStatus::InvalidConsentAuthority:
    case detail::ExplorerLedgerIssueStatus::AuthorityKindConflict:
        return ExplorerEligibilityReason::ConsentGenerationMismatch;
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

[[nodiscard]] std::vector<detail::BaselineEntryModel>
expand_baseline_entries(const ShellWindowInventory& inventory) {
    std::vector<detail::BaselineEntryModel> entries;
    if (inventory.reported_entry_count > 0) {
        entries.reserve(
            static_cast<std::size_t>(inventory.reported_entry_count));
    }
    for (const auto& window : inventory.windows) {
        const auto native_key = reinterpret_cast<detail::NativeWindowKey>(
            window.window);
        for (std::size_t index = 0U; index < window.shell_entry_count;
             ++index) {
            detail::BaselineLocationDisposition disposition =
                detail::BaselineLocationDisposition::Inaccessible;
            if (index < window.locations.size()) {
                disposition = window.locations[index].status ==
                                      ShellLocationStatus::Empty
                                  ? detail::BaselineLocationDisposition::Empty
                                  : (window.locations[index].filesystem()
                                         ? detail::BaselineLocationDisposition::
                                               Valid
                                         : detail::BaselineLocationDisposition::
                                               Inaccessible);
            }
            entries.push_back(
                {native_key, native_key != 0U, disposition});
        }
    }
    const auto reported = inventory.reported_entry_count < 0
                              ? 0U
                              : static_cast<std::size_t>(
                                    inventory.reported_entry_count);
    while (entries.size() < reported) {
        entries.push_back(
            {0U,
             false,
             detail::BaselineLocationDisposition::Inaccessible});
    }
    return entries;
}

[[nodiscard]] detail::InventoryModel make_inventory_model(
    const ShellWindowInventory& inventory) {
    detail::InventoryModel result;
    result.complete = inventory.complete && inventory.reported_entry_count >= 0;
    result.windows.reserve(inventory.windows.size());
    for (const auto& window : inventory.windows) {
        detail::InventoryFingerprint fingerprint;
        fingerprint.native_key =
            reinterpret_cast<detail::NativeWindowKey>(window.window);
        fingerprint.shell_entry_count = window.shell_entry_count;
        fingerprint.locations.reserve(window.locations.size());
        for (const auto& location : window.locations) {
            fingerprint.locations.push_back(
                {location.status,
                 location.source,
                 location.identity,
                 location.opaque_fingerprint});
        }
        result.windows.push_back(std::move(fingerprint));
    }
    return result;
}

[[nodiscard]] ExplorerEligibilityReason map_consent_bind_reason(
    const ExplorerConsentTargetBindReason reason) noexcept {
    switch (reason) {
    case ExplorerConsentTargetBindReason::Succeeded:
        return ExplorerEligibilityReason::Eligible;
    case ExplorerConsentTargetBindReason::InvalidArgument:
        return ExplorerEligibilityReason::TargetInvalidated;
    case ExplorerConsentTargetBindReason::ApartmentUnavailable:
        return ExplorerEligibilityReason::ComApartmentUnavailable;
    case ExplorerConsentTargetBindReason::InventoryUnavailable:
        return ExplorerEligibilityReason::InventoryUnavailable;
    case ExplorerConsentTargetBindReason::TargetNotFound:
        return ExplorerEligibilityReason::TargetNotFound;
    case ExplorerConsentTargetBindReason::AmbiguousCandidate:
    case ExplorerConsentTargetBindReason::SharedWindow:
        return ExplorerEligibilityReason::AmbiguousCandidate;
    case ExplorerConsentTargetBindReason::CanonicalIdentityUnavailable:
        return ExplorerEligibilityReason::CanonicalIdentityMismatch;
    case ExplorerConsentTargetBindReason::BrowserEventSubscriptionUnavailable:
        return ExplorerEligibilityReason::BrowserEventSubscriptionUnavailable;
    case ExplorerConsentTargetBindReason::CandidateChangedDuringBinding:
        return ExplorerEligibilityReason::TargetInvalidated;
    }
    return ExplorerEligibilityReason::TargetInvalidated;
}

template <typename ImplType>
[[nodiscard]] bool retire_event_sources(ImplType& impl) noexcept {
    bool safe = true;
    if (impl.provisioning_lease != nullptr) {
        const auto before =
            impl.provisioning_lease->browser_readiness_facts();
        static_cast<void>(
            impl.provisioning_lease->close_browser_events());
        const auto after =
            impl.provisioning_lease->browser_readiness_facts();
        impl.provisioning.cleanup.browser_events_unadvised = after.unadvised;
        impl.provisioning.cleanup.browser_event_lifecycle_clean =
            after.unadvised && after.malformed_count == 0U &&
            after.overflow_count == 0U && after.wrong_thread_count == 0U &&
            after.post_retirement_count == 0U &&
            after.identity_query_failure_count == 0U;
        safe = safe &&
               (!before.subscribed ||
                impl.provisioning.cleanup.browser_event_lifecycle_clean);
    }
    if (impl.shell_subscription != nullptr) {
        static_cast<void>(impl.shell_subscription->close());
        const auto after = impl.shell_subscription->facts();
        impl.provisioning.cleanup.shell_events_unadvised = after.unadvised;
        impl.provisioning.cleanup.shell_event_lifecycle_clean =
            after.unadvised && after.malformed_count == 0U &&
            after.overflow_count == 0U && after.wrong_thread_count == 0U &&
            after.post_retirement_count == 0U &&
            after.subscription_generation ==
                impl.shell_subscription_generation &&
            !impl.receipt_generation_mismatch &&
            !impl.cookie_lifecycle_ambiguous;
        safe = safe &&
               impl.provisioning.cleanup.shell_event_lifecycle_clean;
    }
    // Capture final sink counters after both Unadvise transactions so any
    // reentrant/post-retirement callback remains visible in public evidence.
    copy_event_facts(impl);
    return safe;
}

template <typename ImplType>
[[nodiscard]] ExplorerProvisioningCleanupFacts cleanup_provisioning_lease(
    ImplType& impl,
    const std::chrono::steady_clock::time_point deadline,
    const bool retire_token_before_quit) {
    ExplorerProvisioningCleanupFacts cleanup;
    cleanup.orphan_attribution_known = true;
    const bool pending_messages_drained =
        drain_pending_event_messages(impl);

    const auto lease_facts = impl.provisioning_lease->facts();
    auto lease_handle = impl.provisioning_lease->current_hwnd();
    HWND exact_window = lease_handle.succeeded() ? lease_handle.window
                                                  : nullptr;
    bool canonical_matches = true;
    bool window_identity_matches = true;
    if (impl.matching_cookie.has_value()) {
        auto resolved = impl.shell_subscription->resolve_cookie(
            *impl.matching_cookie);
        canonical_matches = resolved.succeeded() &&
                            resolved.resolved.same_object(
                                *impl.provisioning_lease);
        window_identity_matches =
            canonical_matches && exact_window != nullptr &&
            resolved.resolved.window() == exact_window;
    }

    if (impl.navigation ==
            detail::LeaseNavigationDisposition::PendingExpectedTarget ||
        impl.navigation ==
            detail::LeaseNavigationDisposition::ExactExpectedTarget) {
        const auto location = impl.provisioning_lease->current_location();
        impl.navigation = advance_navigation_state(impl.navigation,
                                                   location,
                                                   impl.target_location);
    }

    const auto shell_stream = impl.shell_subscription->facts();
    const auto browser_stream =
        impl.provisioning_lease->browser_readiness_facts();
    const bool shell_cleanup_evidence_healthy =
        shell_stream.subscribed && shell_stream.accepting &&
        shell_stream.subscription_generation ==
            impl.shell_subscription_generation &&
        shell_stream.malformed_count == 0U &&
        shell_stream.overflow_count == 0U &&
        shell_stream.wrong_thread_count == 0U &&
        shell_stream.post_retirement_count == 0U &&
        !impl.receipt_generation_mismatch &&
        !impl.cookie_lifecycle_ambiguous;
    const bool browser_cleanup_evidence_healthy =
        impl.navigation == detail::LeaseNavigationDisposition::NotStarted
            ? (!browser_stream.subscribed ||
               (browser_stream.accepting &&
                browser_stream.malformed_count == 0U &&
                browser_stream.overflow_count == 0U &&
                browser_stream.wrong_thread_count == 0U &&
                browser_stream.post_retirement_count == 0U &&
                browser_stream.identity_query_failure_count == 0U &&
                browser_stream.quit_count == 0U))
            : (browser_stream.subscribed && browser_stream.accepting &&
               browser_stream.subscription_diagnostic == S_OK &&
               browser_stream.malformed_count == 0U &&
               browser_stream.overflow_count == 0U &&
               browser_stream.wrong_thread_count == 0U &&
               browser_stream.post_retirement_count == 0U &&
               browser_stream.identity_query_failure_count == 0U &&
               browser_stream.quit_count == 0U);

    const auto exact_key = reinterpret_cast<detail::NativeWindowKey>(
        exact_window);
    const bool registration_classification_incomplete = std::any_of(
        impl.registered_cookies.begin(),
        impl.registered_cookies.end(),
        [&impl](const long cookie) {
            return !impl.matching_cookies.contains(cookie) &&
                   !impl.unrelated_cookies.contains(cookie);
        });
    const detail::LeaseCleanupFacts authorization_facts{
        true,
        impl.provisioning_lease->session_authority() ==
            impl.ledger.session_authority(),
        lease_facts.created_by_single_co_create,
        impl.provisioning_lease->subscription_generation() ==
            impl.shell_subscription_generation,
        lease_facts.created_by_single_co_create,
        canonical_matches,
        lease_facts.identity_ambiguous ||
            !impl.shared_window_identity_conflict_cookies.empty() ||
            impl.matching_cookies.size() > 1U ||
            !impl.unresolved_cookies.empty() ||
            registration_classification_incomplete ||
            impl.browser_navigation_history_ambiguous ||
            !pending_messages_drained ||
            !shell_cleanup_evidence_healthy ||
            !browser_cleanup_evidence_healthy,
        exact_window == nullptr || !forbidden_preexisting(impl, exact_key),
        window_identity_matches,
        impl.navigation,
    };
    cleanup.cleanup_authorized =
        detail::evaluate_lease_cleanup_authorization(authorization_facts) ==
        detail::LeaseCleanupAuthorization::Authorized;

    if (cleanup.cleanup_authorized) {
        if (retire_token_before_quit && impl.window != nullptr) {
            static_cast<void>(impl.ledger.retire_native(
                reinterpret_cast<detail::NativeWindowKey>(impl.window)));
        }
        const auto quit = impl.provisioning_lease->quit();
        cleanup.native_quit_attempted = true;
        cleanup.native_quit_succeeded = quit.succeeded();
        constexpr std::size_t message_budget = 4096U;
        for (std::size_t dispatched = 0U;
             quit.succeeded() && dispatched < message_budget;
             ++dispatched) {
            refresh_registration_state(impl);
            if (exact_window == nullptr) {
                const auto late_handle =
                    impl.provisioning_lease->current_hwnd();
                if (late_handle.succeeded()) {
                    exact_window = late_handle.window;
                }
            }
            cleanup.matching_registration_revoked =
                impl.matching_registration_revoked ||
                (impl.matching_cookie.has_value() &&
                 impl.revoked_cookies.contains(*impl.matching_cookie));
            cleanup.exact_hwnd_invalidated =
                exact_window != nullptr && IsWindow(exact_window) == FALSE;
            if (cleanup.matching_registration_revoked ||
                cleanup.exact_hwnd_invalidated) {
                break;
            }
            const auto pump = pump_owner_sta_once(deadline);
            if (pump.status != OwnerStaPumpStatus::ActivityDispatched) {
                break;
            }
        }
    }

    cleanup.matching_registration_revoked =
        cleanup.matching_registration_revoked ||
        impl.matching_registration_revoked ||
        (impl.matching_cookie.has_value() &&
         impl.revoked_cookies.contains(*impl.matching_cookie));
    if (exact_window == nullptr) {
        const auto final_handle = impl.provisioning_lease->current_hwnd();
        if (final_handle.succeeded()) {
            exact_window = final_handle.window;
        }
    }
    cleanup.exact_hwnd_invalidated =
        cleanup.exact_hwnd_invalidated ||
        (exact_window != nullptr && IsWindow(exact_window) == FALSE);
    const bool exact_hwnd_still_valid =
        exact_window != nullptr && IsWindow(exact_window) != FALSE;
    cleanup.attributable_orphan =
        exact_hwnd_still_valid ||
        !(cleanup.matching_registration_revoked ||
          cleanup.exact_hwnd_invalidated);
    impl.provisioning.cleanup = cleanup;
    static_cast<void>(retire_event_sources(impl));
    cleanup.browser_events_unadvised =
        impl.provisioning.cleanup.browser_events_unadvised;
    cleanup.shell_events_unadvised =
        impl.provisioning.cleanup.shell_events_unadvised;
    cleanup.browser_event_lifecycle_clean =
        impl.provisioning.cleanup.browser_event_lifecycle_clean;
    cleanup.shell_event_lifecycle_clean =
        impl.provisioning.cleanup.shell_event_lifecycle_clean;
    impl.provisioning.cleanup = cleanup;
    return cleanup;
}

template <typename ImplType>
[[nodiscard]] ExplorerProvisionResult failed_provision_result(
    std::unique_ptr<ImplType> impl,
    const ExplorerEligibilityReason reason,
    std::optional<ExplorerDiagnostic> diagnostic,
    const std::chrono::steady_clock::time_point deadline) {
    if (impl->provisioning_lease != nullptr) {
        impl->provisioning.cleanup = cleanup_provisioning_lease(
            *impl, deadline, false);
    } else {
        static_cast<void>(retire_event_sources(*impl));
    }
    ExplorerProvisionResult result;
    result.reason = reason;
    result.diagnostic = std::move(diagnostic);
    result.facts = impl->provisioning;
    result.cleanup = result.facts.cleanup;
    impl.reset();
    return result;
}

} // namespace

ExplorerTestSession::ExplorerTestSession(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

ExplorerTestSession::~ExplorerTestSession() = default;

ExplorerConsentProvisioning::ExplorerConsentProvisioning(
    std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

ExplorerConsentProvisioning::~ExplorerConsentProvisioning() = default;

ExplorerConsentBeginResult ExplorerConsentProvisioning::begin(
    const std::filesystem::path& unique_empty_test_directory) {
    ExplorerConsentBeginResult result;
    if (!unique_empty_test_directory.is_absolute()) {
        result.diagnostic = adapter_diagnostic(
            1400U,
            "ExplorerConsentProvisioning::begin",
            "target directory must be absolute");
        return result;
    }

    std::error_code filesystem_error;
    const DWORD attributes =
        GetFileAttributesW(unique_empty_test_directory.c_str());
    const auto root = unique_empty_test_directory.root_path();
    const UINT drive_type =
        root.empty() ? DRIVE_UNKNOWN : GetDriveTypeW(root.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U ||
        drive_type != DRIVE_FIXED ||
        !std::filesystem::is_directory(unique_empty_test_directory,
                                       filesystem_error) ||
        filesystem_error) {
        result.diagnostic = adapter_diagnostic(
            1401U,
            "Explorer consent target directory",
            "target must be a plain directory on a local fixed filesystem");
        return result;
    }
    if (!std::filesystem::is_empty(unique_empty_test_directory,
                                   filesystem_error) ||
        filesystem_error) {
        result.reason = filesystem_error
                            ? ExplorerEligibilityReason::InvalidTargetDirectory
                            : ExplorerEligibilityReason::TargetDirectoryNotEmpty;
        result.diagnostic = adapter_diagnostic(
            1402U,
            "Explorer consent target directory",
            "target directory must be readable and empty");
        return result;
    }

    const HRESULT apartment = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (apartment != S_OK && apartment != S_FALSE) {
        result.reason = ExplorerEligibilityReason::ComApartmentUnavailable;
        result.diagnostic = hresult_diagnostic(
            "CoInitializeEx", apartment,
            "user-consent Explorer observation requires an STA");
        return result;
    }

    auto impl = std::make_unique<Impl>();
    impl->com_initialized = true;
    impl->controller_thread_id = GetCurrentThreadId();
    impl->target_directory = unique_empty_test_directory;

    const auto target_location =
        filesystem_location_identity(unique_empty_test_directory);
    if (!target_location.filesystem()) {
        result.reason = ExplorerEligibilityReason::InvalidTargetDirectory;
        result.diagnostic = hresult_diagnostic(
            "filesystem_location_identity",
            target_location.diagnostic,
            "consent target FILE_ID_INFO is unavailable");
        return result;
    }
    impl->target_location = *target_location.identity;

    const auto baseline = capture_shell_window_inventory();
    const auto baseline_entries = expand_baseline_entries(baseline);
    const auto exclusion =
        detail::evaluate_baseline_exclusion(baseline_entries);
    impl->facts.baseline_total_shell_entries =
        baseline.reported_entry_count < 0
            ? 0U
            : static_cast<std::size_t>(baseline.reported_entry_count);
    impl->facts.baseline_reliable_shell_entries =
        exclusion.reliable_entry_count;
    impl->facts.forbidden_preexisting_hwnd_count =
        exclusion.forbidden_preexisting_hwnds.size();
    impl->facts.baseline_exclusion_complete =
        baseline.complete && exclusion.complete() &&
        baseline_entries.size() ==
            impl->facts.baseline_total_shell_entries;
    impl->forbidden_preexisting_hwnds =
        exclusion.forbidden_preexisting_hwnds;
    if (!impl->facts.baseline_exclusion_complete) {
        result.reason =
            ExplorerEligibilityReason::BaselineWindowIdentityUnavailable;
        result.diagnostic =
            !baseline.issues.empty()
                ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                      baseline.issues.front(),
                      "every baseline Shell entry must expose a reliable HWND")}
                : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                      1403U,
                      "Explorer consent baseline exclusion",
                      "baseline inventory is incomplete")};
        result.facts = impl->facts;
        return result;
    }

    impl->facts.generations.baseline_generation =
        impl->issue_protocol_generation();
    if (impl->facts.generations.baseline_generation == 0U) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        result.diagnostic = adapter_diagnostic(
            1404U,
            "Explorer consent generation",
            "baseline generation could not be issued");
        result.facts = impl->facts;
        return result;
    }

    result.reason = ExplorerEligibilityReason::Eligible;
    result.facts = impl->facts;
    result.provisioning = std::unique_ptr<ExplorerConsentProvisioning>(
        new ExplorerConsentProvisioning(std::move(impl)));
    return result;
}

ExplorerConsentStepResult
ExplorerConsentProvisioning::record_target_prompt() {
    ExplorerConsentStepResult result;
    if (impl_ == nullptr || impl_->terminal ||
        GetCurrentThreadId() != impl_->controller_thread_id ||
        !impl_->facts.baseline_exclusion_complete ||
        impl_->facts.generations.target_prompt_generation != 0U) {
        result.reason = ExplorerEligibilityReason::TargetConsentRequired;
        result.diagnostic = adapter_diagnostic(
            1405U,
            "Explorer target consent prompt",
            "target prompt is out of order or the consent run is terminal");
        return result;
    }
    result.generation = impl_->issue_protocol_generation();
    if (result.generation == 0U) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        return result;
    }
    impl_->facts.generations.target_prompt_generation = result.generation;
    result.reason = ExplorerEligibilityReason::Eligible;
    return result;
}

ExplorerConsentProvisionResult
ExplorerConsentProvisioning::confirm_user_target() {
    ExplorerConsentProvisionResult result;
    if (impl_ == nullptr || impl_->terminal ||
        GetCurrentThreadId() != impl_->controller_thread_id ||
        impl_->facts.generations.target_prompt_generation == 0U ||
        impl_->facts.generations.target_confirmation_generation != 0U) {
        result.reason = ExplorerEligibilityReason::TargetConsentRequired;
        result.diagnostic = adapter_diagnostic(
            1406U,
            "Explorer target consent confirmation",
            "real-console target confirmation was not recorded in order");
        return result;
    }
    impl_->terminal = true;
    impl_->facts.generations.target_confirmation_generation =
        impl_->issue_protocol_generation();
    if (impl_->facts.generations.target_confirmation_generation == 0U) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        result.facts = impl_->facts;
        return result;
    }

    const auto post_confirmation = capture_shell_window_inventory();
    impl_->facts.post_confirmation_shell_entries =
        post_confirmation.reported_entry_count < 0
            ? 0U
            : static_cast<std::size_t>(
                  post_confirmation.reported_entry_count);
    auto selection = detail::evaluate_consent_candidate_inventory(
        make_inventory_model(post_confirmation),
        impl_->forbidden_preexisting_hwnds,
        impl_->target_location);
    impl_->facts.exact_new_candidate_count =
        selection.exact_target_window_count;
    impl_->facts.preexisting_exact_location_detected =
        selection.forbidden_window_at_target;
    impl_->facts.unique_new_target = selection.eligible();
    impl_->facts.exact_target_location = selection.exact_unique_location;
    if (!selection.eligible()) {
        result.reason = selection.reason;
        result.diagnostic = adapter_diagnostic(
            1407U,
            "Explorer consent candidate selection",
            "post-confirmation inventory did not contain one safe new exact target");
        result.facts = impl_->facts;
        return result;
    }

    const auto candidate_key = selection.candidate_fingerprint->native_key;
    auto bound = bind_explorer_consent_target(candidate_key,
                                               impl_->target_location);
    if (!bound.succeeded()) {
        result.reason = map_consent_bind_reason(bound.reason);
        result.diagnostic =
            bound.diagnostic.has_value()
                ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                      *bound.diagnostic,
                      "read-only consent target binding failed")}
                : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                      1408U,
                      "Explorer consent target binding",
                      "candidate could not be bound uniquely to one Shell object")};
        result.facts = impl_->facts;
        return result;
    }

    const auto rebound_inventory = capture_shell_window_inventory();
    const auto rebound = detail::evaluate_consent_candidate_inventory(
        make_inventory_model(rebound_inventory),
        impl_->forbidden_preexisting_hwnds,
        impl_->target_location,
        &*selection.candidate_fingerprint);
    if (!rebound.eligible()) {
        result.reason = rebound.reason;
        result.diagnostic = adapter_diagnostic(
            1409U,
            "Explorer consent target binding",
            "candidate changed after browser observation was established");
        result.facts = impl_->facts;
        return result;
    }

    auto session_impl = std::make_unique<ExplorerTestSession::Impl>();
    session_impl->controller_thread_id = impl_->controller_thread_id;
    session_impl->authority_kind = ExplorerAuthorityKind::UserConsent;
    session_impl->target_directory = impl_->target_directory;
    session_impl->target_location = impl_->target_location;
    session_impl->forbidden_preexisting_hwnds =
        impl_->forbidden_preexisting_hwnds;
    session_impl->consent_candidate_fingerprint =
        *selection.candidate_fingerprint;
    session_impl->consent_observation = std::move(bound.observation);
    session_impl->window = reinterpret_cast<HWND>(candidate_key);
    session_impl->consent = impl_->facts;
    session_impl->consent.browser_observation_active = true;
    session_impl->provisioning.baseline_total_shell_entries =
        impl_->facts.baseline_total_shell_entries;
    session_impl->provisioning.baseline_reliable_shell_entries =
        impl_->facts.baseline_reliable_shell_entries;
    session_impl->provisioning.forbidden_preexisting_hwnd_count =
        impl_->facts.forbidden_preexisting_hwnd_count;
    session_impl->provisioning.baseline_exclusion_complete = true;
    session_impl->provisioning.preexisting_window_count =
        impl_->facts.forbidden_preexisting_hwnd_count;
    session_impl->provisioning.new_candidate_count = 1U;
    session_impl->provisioning.retained_window_was_new_before_navigation =
        true;
    session_impl->provisioning.exact_target_location = true;
    session_impl->provisioning.exact_unique_test_location = true;

    DWORD process_id = 0U;
    const DWORD thread_id = GetWindowThreadProcessId(session_impl->window,
                                                      &process_id);
    if (thread_id == 0U || process_id == 0U) {
        result.reason = ExplorerEligibilityReason::WindowDestroyed;
        result.diagnostic = win32_diagnostic(
            "GetWindowThreadProcessId", GetLastError(),
            "consent target lost native identity before issuance");
        result.facts = impl_->facts;
        return result;
    }
    session_impl->process_id = process_id;
    session_impl->thread_id = thread_id;
    session_impl->process.reset(OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
        FALSE,
        process_id));
    if (!session_impl->process.valid()) {
        result.reason = ExplorerEligibilityReason::ProcessOpenFailed;
        result.diagnostic = win32_diagnostic(
            "OpenProcess", GetLastError(),
            "consent target process identity handle could not be opened");
        result.facts = impl_->facts;
        return result;
    }
    DWORD rechecked_process_id = 0U;
    const DWORD rechecked_thread_id = GetWindowThreadProcessId(
        session_impl->window, &rechecked_process_id);
    if (rechecked_thread_id != thread_id ||
        rechecked_process_id != process_id ||
        GetProcessId(session_impl->process.get()) != process_id ||
        WaitForSingleObject(session_impl->process.get(), 0U) != WAIT_TIMEOUT) {
        result.reason = ExplorerEligibilityReason::TargetInvalidated;
        result.diagnostic = adapter_diagnostic(
            1410U,
            "Explorer consent process identity",
            "PID, TID, or process instance changed during issuance");
        result.facts = impl_->facts;
        return result;
    }

    auto initial = validate_native_target(*session_impl, nullptr, true);
    if (initial.reason != ExplorerEligibilityReason::Eligible ||
        !initial.snapshot.has_value()) {
        result.reason = initial.reason;
        result.diagnostic = std::move(initial.diagnostic);
        result.facts = impl_->facts;
        return result;
    }
    if (!detail::rect_is_contained(initial.snapshot->visible_rect,
                                   initial.snapshot->monitor_work_area) ||
        !select_safe_test_delta(*initial.snapshot).succeeded()) {
        result.reason = ExplorerEligibilityReason::UnsafeDelta;
        result.diagnostic = adapter_diagnostic(
            1411U,
            "Explorer consent initial geometry",
            "target has no safe same-monitor test translation");
        result.facts = impl_->facts;
        return result;
    }

    session_impl->consent.generations.eligibility_generation =
        impl_->issue_protocol_generation();
    session_impl->consent.generations.token_generation =
        impl_->issue_protocol_generation();
    if (session_impl->consent.generations.eligibility_generation == 0U ||
        session_impl->consent.generations.token_generation == 0U) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        result.facts = session_impl->consent;
        return result;
    }
    auto issue = session_impl->ledger.issue_user_consent(
        candidate_key,
        process_id,
        thread_id,
        session_impl->consent.generations.token_generation);
    if (!issue.token.has_value()) {
        result.reason = ledger_failure_reason(issue.status);
        result.diagnostic = adapter_diagnostic(
            1412U,
            "ExplorerTokenLedger::issue_user_consent",
            "user-consent Explorer capability issuance failed");
        result.facts = session_impl->consent;
        return result;
    }
    session_impl->issued_token = *issue.token;
    session_impl->original_snapshot = *initial.snapshot;
    session_impl->provisioning.initial_snapshot = *initial.snapshot;
    session_impl->provisioning.token_issued = true;
    session_impl->consent.token_issued = true;
    session_impl->consent.exact_target_location = true;

    result.reason = ExplorerEligibilityReason::Eligible;
    result.facts = session_impl->consent;
    impl_->facts = session_impl->consent;
    session_impl->com_initialized = true;
    impl_->com_initialized = false;
    result.session = std::unique_ptr<ExplorerTestSession>(
        new ExplorerTestSession(std::move(session_impl)));
    return result;
}

const ExplorerConsentFacts& ExplorerConsentProvisioning::facts()
    const noexcept {
    return impl_->facts;
}

const std::filesystem::path&
ExplorerConsentProvisioning::target_directory() const noexcept {
    return impl_->target_directory;
}

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
    const auto cleanup_deadline = [] {
        return std::chrono::steady_clock::now() + std::chrono::seconds{3};
    };
    const auto fail = [&](const ExplorerEligibilityReason reason,
                          std::optional<ExplorerDiagnostic> diagnostic) {
        return failed_provision_result(std::move(impl),
                                       reason,
                                       std::move(diagnostic),
                                       cleanup_deadline());
    };

    const auto target_location =
        filesystem_location_identity(unique_empty_test_directory);
    if (!target_location.filesystem()) {
        return fail(
            ExplorerEligibilityReason::InvalidTargetDirectory,
            hresult_diagnostic("filesystem_location_identity",
                               target_location.diagnostic,
                               "target directory file identity is unavailable"));
    }
    impl->target_location = *target_location.identity;

    auto subscription =
        subscribe_shell_windows(impl->ledger.session_authority());
    if (!subscription.succeeded()) {
        return fail(
            ExplorerEligibilityReason::ShellEventSubscriptionUnavailable,
            subscription.diagnostic.has_value()
                ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                      *subscription.diagnostic,
                      "DShellWindowsEvents subscription failed before CoCreate")}
                : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                      1204U,
                      "subscribe_shell_windows",
                      "Shell registration subscription returned no authority")});
    }
    impl->shell_subscription = std::move(subscription.subscription);
    impl->shell_subscription_generation =
        impl->shell_subscription->generation();
    copy_event_facts(*impl);
    const auto subscribed_facts = impl->shell_subscription->facts();
    if (!subscribed_facts.subscribed || !subscribed_facts.accepting ||
        subscribed_facts.malformed_count != 0U ||
        subscribed_facts.overflow_count != 0U ||
        subscribed_facts.wrong_thread_count != 0U ||
        subscribed_facts.post_retirement_count != 0U) {
        return fail(
            ExplorerEligibilityReason::ShellEventSubscriptionUnavailable,
            adapter_diagnostic(1205U,
                               "DShellWindowsEvents subscription",
                               "subscription was not active and clean before baseline capture"));
    }

    const auto baseline = impl->shell_subscription->capture_baseline();
    const auto baseline_entries = expand_baseline_entries(baseline);
    const auto exclusion = detail::evaluate_baseline_exclusion(
        baseline_entries);
    impl->provisioning.baseline_total_shell_entries =
        baseline.reported_entry_count < 0
            ? 0U
            : static_cast<std::size_t>(baseline.reported_entry_count);
    impl->provisioning.baseline_reliable_shell_entries =
        exclusion.reliable_entry_count;
    impl->provisioning.baseline_reliable_unique_hwnd_count =
        exclusion.reliable_hwnd_count;
    impl->provisioning.forbidden_preexisting_hwnd_count =
        exclusion.forbidden_preexisting_hwnds.size();
    impl->provisioning.baseline_valid_location_count =
        exclusion.valid_location_count;
    impl->provisioning.baseline_empty_location_count =
        exclusion.empty_location_count;
    impl->provisioning.baseline_inaccessible_location_count =
        exclusion.inaccessible_location_count;
    impl->provisioning.baseline_exclusion_complete =
        baseline.complete && exclusion.complete() &&
        baseline_entries.size() ==
            impl->provisioning.baseline_total_shell_entries;
    impl->provisioning.preexisting_window_count =
        exclusion.reliable_hwnd_count;
    impl->provisioning.baseline_facts_unchanged =
        impl->provisioning.baseline_exclusion_complete;
    impl->forbidden_preexisting_hwnds =
        exclusion.forbidden_preexisting_hwnds;
    if (!impl->provisioning.baseline_exclusion_complete) {
        return fail(
            ExplorerEligibilityReason::BaselineWindowIdentityUnavailable,
            !baseline.issues.empty()
                ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                      baseline.issues.front(),
                      "at least one baseline Shell entry lacked a reliable HWND")}
                : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                      1206U,
                      "Explorer baseline exclusion",
                      "reported Shell entries could not all be assigned reliable HWND identities")});
    }

    auto created = create_explorer_provisioning_lease(
        impl->ledger.session_authority(),
        impl->shell_subscription_generation,
        unique_empty_test_directory);
    impl->provisioning_lease = std::move(created.lease);
    copy_event_facts(*impl);
    if (!created.succeeded()) {
        const auto reason =
            created.diagnostic.has_value() &&
                    created.diagnostic->stage ==
                        ShellAutomationStage::SubscribeBrowserEvents
                ? ExplorerEligibilityReason::
                      BrowserEventSubscriptionUnavailable
                : ExplorerEligibilityReason::ShellWindowCreationFailed;
        return fail(
            reason,
            created.diagnostic.has_value()
                ? std::optional<ExplorerDiagnostic>{shell_diagnostic(
                      *created.diagnostic,
                      reason == ExplorerEligibilityReason::
                                    BrowserEventSubscriptionUnavailable
                          ? "DWebBrowserEvents2 subscription failed; no navigation is authorized"
                          : "single CLSID_ShellBrowserWindow CoCreate failed")}
                : std::optional<ExplorerDiagnostic>{adapter_diagnostic(
                      1207U,
                      "create_explorer_provisioning_lease",
                      "CoCreate returned no retained provisioning lease")});
    }

    const auto navigate =
        impl->provisioning_lease->navigate_to_target_and_show();
    const auto lease_after_navigation = impl->provisioning_lease->facts();
    impl->navigation =
        navigate.succeeded()
            ? detail::LeaseNavigationDisposition::PendingExpectedTarget
            : (lease_after_navigation.navigation_requested
                   ? detail::LeaseNavigationDisposition::Unknown
                   : detail::LeaseNavigationDisposition::NotStarted);
    if (!navigate.succeeded()) {
        return fail(
            ExplorerEligibilityReason::LocationMismatch,
            shell_diagnostic(
                ShellAutomationDiagnostic{navigate.stage,
                                          navigate.hresult,
                                          -1},
                "the single provisioning lease could not navigate and show the nonce directory"));
    }

    const auto readiness_deadline =
        std::chrono::steady_clock::now() + readiness_timeout;
    // A canonical-matching NavigateComplete2 is recorded as a readiness hint,
    // never as authority. WindowRegistered cookie correlation plus the live
    // exact FILE_ID_INFO check below remain the issuance hard gates.
    constexpr std::size_t readiness_message_budget = 4096U;
    ExplorerEligibilityReason readiness_reason =
        ExplorerEligibilityReason::RegistrationNotObserved;
    std::optional<ExplorerDiagnostic> readiness_diagnostic;
    bool target_correlated = false;
    bool positive_queue_drained = false;
    for (std::size_t dispatched = 0U;
         dispatched < readiness_message_budget;
         ++dispatched) {
        refresh_registration_state(*impl);
        if (!event_streams_healthy(*impl)) {
            readiness_reason =
                ExplorerEligibilityReason::ShellEventStreamInvalid;
            readiness_diagnostic = adapter_diagnostic(
                1208U,
                "Explorer event streams",
                "registration or browser readiness evidence was malformed, overflowed, quit, or retired");
            break;
        }
        if (impl->browser_navigation_history_ambiguous) {
            readiness_reason = ExplorerEligibilityReason::TargetInvalidated;
            readiness_diagnostic = adapter_diagnostic(
                1219U,
                "DWebBrowserEvents2::NavigateComplete2",
                "more than one same-object navigation completion made pre-issuance history ambiguous");
            break;
        }
        if (!impl->shared_window_identity_conflict_cookies.empty() ||
            impl->matching_cookies.size() > 1U ||
            impl->provisioning_lease->facts().identity_ambiguous) {
            readiness_reason = ExplorerEligibilityReason::AmbiguousCandidate;
            readiness_diagnostic = adapter_diagnostic(
                1209U,
                "Explorer registration correlation",
                "multiple same-object cookies or a shared-HWND COM identity conflict was observed");
            break;
        }
        if (impl->matching_registration_revoked) {
            readiness_reason = ExplorerEligibilityReason::RegistrationRevoked;
            readiness_diagnostic = adapter_diagnostic(
                1210U,
                "DShellWindowsEvents::WindowRevoked",
                "the matching registration was revoked before token issuance");
            break;
        }
        if (!impl->unresolved_cookies.empty()) {
            readiness_reason =
                ExplorerEligibilityReason::RegistrationResolutionFailed;
        } else if (impl->matching_cookie.has_value()) {
            auto resolved = impl->shell_subscription->resolve_cookie(
                *impl->matching_cookie);
            impl->provisioning.matching_cookie_find_window_resolved =
                impl->provisioning.matching_cookie_find_window_resolved ||
                resolved.succeeded();
            if (!resolved.succeeded()) {
                impl->unresolved_cookies.insert(*impl->matching_cookie);
                readiness_reason =
                    ExplorerEligibilityReason::RegistrationResolutionFailed;
            } else if (!resolved.resolved.same_object(
                           *impl->provisioning_lease)) {
                readiness_reason =
                    ExplorerEligibilityReason::CanonicalIdentityMismatch;
                readiness_diagnostic = adapter_diagnostic(
                    1211U,
                    "IUnknown identity",
                    "matching cookie changed to a different canonical COM object");
                break;
            } else {
                impl->provisioning.canonical_iunknown_identity_matches = true;
                const auto lease_handle =
                    impl->provisioning_lease->current_hwnd();
                impl->provisioning.lease_hwnd_resolved =
                    impl->provisioning.lease_hwnd_resolved ||
                    lease_handle.succeeded();
                impl->provisioning.cookie_hwnd_resolved =
                    impl->provisioning.cookie_hwnd_resolved ||
                    resolved.resolved.window() != nullptr;
                const HWND candidate = resolved.resolved.window();
                const bool current_live_hwnd =
                    candidate != nullptr && IsWindow(candidate) != FALSE;
                impl->provisioning.live_eligibility_hwnd_resolved =
                    impl->provisioning.live_eligibility_hwnd_resolved ||
                    current_live_hwnd;
                const bool current_three_way_hwnd_match =
                    lease_handle.succeeded() &&
                    lease_handle.window == candidate && current_live_hwnd;
                impl->provisioning.lease_cookie_live_hwnd_match =
                    impl->provisioning.lease_cookie_live_hwnd_match ||
                    current_three_way_hwnd_match;
                const auto candidate_key =
                    reinterpret_cast<detail::NativeWindowKey>(candidate);
                const bool current_target_preexisting =
                    forbidden_preexisting(*impl, candidate_key);
                impl->provisioning.target_hwnd_preexisting =
                    impl->provisioning.target_hwnd_preexisting ||
                    current_target_preexisting;
                if (current_target_preexisting) {
                    readiness_reason =
                        ExplorerEligibilityReason::PreexistingWindow;
                    readiness_diagnostic = adapter_diagnostic(
                        1212U,
                        "Explorer baseline exclusion",
                        "same-object registration resolved to a forbidden baseline HWND");
                    break;
                }
                if (current_three_way_hwnd_match) {
                    const auto location =
                        impl->provisioning_lease->current_location();
                    const bool current_exact_target_location =
                        location.filesystem() &&
                        location.identity == impl->target_location;
                    impl->provisioning.exact_target_location =
                        impl->provisioning.exact_target_location ||
                        current_exact_target_location;
                    impl->navigation = advance_navigation_state(
                        impl->navigation, location, impl->target_location);
                    if (location.filesystem() &&
                        !current_exact_target_location) {
                        readiness_reason =
                            ExplorerEligibilityReason::LocationMismatch;
                        readiness_diagnostic = adapter_diagnostic(
                            1213U,
                            "Explorer target location",
                            "same-object target navigated away from the nonce directory");
                        break;
                    }
                    if (current_exact_target_location) {
                        if (std::chrono::steady_clock::now() >=
                            readiness_deadline) {
                            readiness_reason =
                                ExplorerEligibilityReason::LocationNotReady;
                            break;
                        }
                        if (!positive_queue_drained) {
                            if (!drain_pending_event_messages(*impl)) {
                                readiness_reason =
                                    ExplorerEligibilityReason::
                                        ShellEventStreamInvalid;
                                readiness_diagnostic = adapter_diagnostic(
                                    1218U,
                                    "Explorer owner STA message queue",
                                    "positive correlation could not drain queued receipts safely");
                                break;
                            }
                            positive_queue_drained = true;
                            continue;
                        }
                        impl->window = candidate;
                        target_correlated = true;
                        break;
                    }
                    readiness_reason =
                        ExplorerEligibilityReason::LocationNotReady;
                }
            }
        } else if (!impl->unrelated_cookies.empty()) {
            readiness_reason =
                ExplorerEligibilityReason::CanonicalIdentityMismatch;
        }

        const auto pump = pump_owner_sta_once(readiness_deadline);
        if (pump.status == OwnerStaPumpStatus::TimedOut) {
            break;
        }
        if (pump.status != OwnerStaPumpStatus::ActivityDispatched) {
            readiness_reason = ExplorerEligibilityReason::ShellEventStreamInvalid;
            readiness_diagnostic = hresult_diagnostic(
                "MsgWaitForMultipleObjectsEx",
                pump.diagnostic,
                "owner STA message pump failed during bounded provisioning");
            break;
        }
    }
    if (!target_correlated) {
        // One final exact state evaluation after the bounded wait records a
        // registration that became resolvable at the edge without turning
        // timeout handling into polling or extending the issuance deadline.
        refresh_registration_state(*impl);
        if (impl->browser_navigation_history_ambiguous) {
            readiness_reason = ExplorerEligibilityReason::TargetInvalidated;
        } else if (!impl->shared_window_identity_conflict_cookies.empty() ||
            impl->matching_cookies.size() > 1U) {
            readiness_reason = ExplorerEligibilityReason::AmbiguousCandidate;
        } else if (impl->matching_registration_revoked) {
            readiness_reason = ExplorerEligibilityReason::RegistrationRevoked;
        } else if (!impl->unresolved_cookies.empty()) {
            readiness_reason =
                ExplorerEligibilityReason::RegistrationResolutionFailed;
        } else if (impl->matching_cookie.has_value()) {
            auto final_resolved = impl->shell_subscription->resolve_cookie(
                *impl->matching_cookie);
            impl->provisioning.matching_cookie_find_window_resolved =
                impl->provisioning.matching_cookie_find_window_resolved ||
                final_resolved.succeeded();
            if (!final_resolved.succeeded()) {
                readiness_reason = ExplorerEligibilityReason::
                    RegistrationResolutionFailed;
            } else if (!final_resolved.resolved.same_object(
                           *impl->provisioning_lease)) {
                readiness_reason =
                    ExplorerEligibilityReason::CanonicalIdentityMismatch;
            } else {
                impl->provisioning.canonical_iunknown_identity_matches = true;
                const auto final_lease_hwnd =
                    impl->provisioning_lease->current_hwnd();
                impl->provisioning.lease_hwnd_resolved =
                    impl->provisioning.lease_hwnd_resolved ||
                    final_lease_hwnd.succeeded();
                impl->provisioning.cookie_hwnd_resolved =
                    impl->provisioning.cookie_hwnd_resolved ||
                    final_resolved.resolved.window() != nullptr;
                const bool final_three_way_hwnd_match =
                    final_lease_hwnd.succeeded() &&
                    final_lease_hwnd.window ==
                        final_resolved.resolved.window() &&
                    IsWindow(final_resolved.resolved.window()) != FALSE;
                impl->provisioning.lease_cookie_live_hwnd_match =
                    impl->provisioning.lease_cookie_live_hwnd_match ||
                    final_three_way_hwnd_match;
                if (final_three_way_hwnd_match) {
                    const auto final_location =
                        impl->provisioning_lease->current_location();
                    const bool final_exact_target_location =
                        final_location.filesystem() &&
                        final_location.identity == impl->target_location;
                    impl->provisioning.exact_target_location =
                        impl->provisioning.exact_target_location ||
                        final_exact_target_location;
                    impl->navigation = advance_navigation_state(
                        impl->navigation,
                        final_location,
                        impl->target_location);
                    readiness_reason =
                        final_exact_target_location
                            ? ExplorerEligibilityReason::LocationNotReady
                            : ExplorerEligibilityReason::LocationMismatch;
                }
            }
        }
        if (!readiness_diagnostic.has_value()) {
            readiness_diagnostic = adapter_diagnostic(
                1214U,
                "Explorer registration readiness",
                "bounded event-driven readiness ended without positive target attribution");
        }
        return fail(readiness_reason, std::move(readiness_diagnostic));
    }

    impl->provisioning.new_candidate_count =
        impl->matching_cookies.size();
    impl->provisioning.retained_window_was_new_before_navigation =
        !impl->provisioning.target_hwnd_preexisting;
    impl->provisioning.exact_unique_test_location =
        impl->provisioning.exact_target_location &&
        impl->matching_cookies.size() == 1U;

    DWORD process_id = 0U;
    const DWORD thread_id =
        GetWindowThreadProcessId(impl->window, &process_id);
    if (thread_id == 0U || process_id == 0U) {
        return fail(
            ExplorerEligibilityReason::WindowDestroyed,
            win32_diagnostic("GetWindowThreadProcessId",
                             GetLastError(),
                             "correlated Explorer target lost native identity"));
    }
    impl->process_id = process_id;
    impl->thread_id = thread_id;
    impl->process.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
                                        SYNCHRONIZE,
                                    FALSE,
                                    process_id));
    if (!impl->process.valid()) {
        return fail(
            ExplorerEligibilityReason::ProcessOpenFailed,
            win32_diagnostic("OpenProcess",
                             GetLastError(),
                             "Explorer process identity handle could not be opened"));
    }
    DWORD rechecked_process_id = 0U;
    const DWORD rechecked_thread_id =
        GetWindowThreadProcessId(impl->window, &rechecked_process_id);
    if (rechecked_thread_id != thread_id ||
        rechecked_process_id != process_id ||
        GetProcessId(impl->process.get()) != process_id ||
        WaitForSingleObject(impl->process.get(), 0U) != WAIT_TIMEOUT) {
        return fail(
            ExplorerEligibilityReason::TargetInvalidated,
            adapter_diagnostic(1215U,
                               "Explorer process identity",
                               "PID/TID/process instance changed during issuance"));
    }

    auto initial = validate_native_target(*impl, nullptr, true);
    if (initial.reason != ExplorerEligibilityReason::Eligible ||
        !initial.snapshot.has_value()) {
        return fail(initial.reason, std::move(initial.diagnostic));
    }
    if (!detail::rect_is_contained(initial.snapshot->visible_rect,
                                   initial.snapshot->monitor_work_area) ||
        !select_safe_test_delta(*initial.snapshot).succeeded()) {
        return fail(
            ExplorerEligibilityReason::UnsafeDelta,
            adapter_diagnostic(
                1216U,
                "Explorer initial geometry",
                "initial frame or every test delta falls outside one monitor work area"));
    }

    auto issue = impl->ledger.issue_legacy_auto(
        reinterpret_cast<detail::NativeWindowKey>(impl->window),
        process_id,
        thread_id,
        static_cast<std::int32_t>(*impl->matching_cookie),
        impl->shell_subscription_generation);
    if (!issue.token.has_value()) {
        return fail(
            ledger_failure_reason(issue.status),
            adapter_diagnostic(1217U,
                               "ExplorerTokenLedger::issue",
                               "Explorer capability issuance failed"));
    }
    impl->issued_token = *issue.token;
    impl->original_snapshot = *initial.snapshot;
    impl->provisioning.initial_snapshot = *initial.snapshot;
    impl->issued_matching_navigate_complete_count =
        impl->provisioning.browser_matching_navigate_complete_count;
    impl->provisioning.issued_matching_navigate_complete_count =
        impl->issued_matching_navigate_complete_count;
    impl->provisioning.token_issued = true;

    ExplorerProvisionResult result;
    result.reason = ExplorerEligibilityReason::Eligible;
    result.facts = impl->provisioning;
    result.session = std::unique_ptr<ExplorerTestSession>(
        new ExplorerTestSession(std::move(impl)));
    return result;

}

const ExplorerProvisioningFacts& ExplorerTestSession::provisioning_facts()
    const noexcept {
    return impl_->provisioning;
}

ExplorerAuthorityKind ExplorerTestSession::authority_kind() const noexcept {
    return impl_ == nullptr
               ? ExplorerAuthorityKind::LegacyAutoProvisionDiagnostic
               : impl_->authority_kind;
}

const ExplorerConsentFacts& ExplorerTestSession::consent_facts()
    const noexcept {
    return impl_->consent;
}

ExplorerConsentStepResult ExplorerTestSession::record_move_prompt(
    const ExplorerWindowToken& token_value) {
    ExplorerConsentStepResult result;
    result.reason = ExplorerEligibilityReason::MoveConsentRequired;
    if (impl_ == nullptr ||
        impl_->authority_kind != ExplorerAuthorityKind::UserConsent ||
        !impl_->ledger.contains(token_value) ||
        token_value.authority_kind() != ExplorerAuthorityKind::UserConsent ||
        token_value.consent_generation() !=
            impl_->consent.generations.token_generation ||
        impl_->consent.generations.move_prompt_generation != 0U ||
        impl_->consent_move_authority.authorized()) {
        result.diagnostic = adapter_diagnostic(
            1413U,
            "Explorer move consent prompt",
            "move prompt is out of order or token authority does not match");
        return result;
    }
    const auto token_generation =
        impl_->consent.generations.token_generation;
    if (token_generation == 0U ||
        token_generation >= std::numeric_limits<std::uint64_t>::max() - 1U) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        return result;
    }
    result.generation = token_generation + 1U;
    impl_->consent.generations.move_prompt_generation = result.generation;
    result.reason = ExplorerEligibilityReason::Eligible;
    return result;
}

ExplorerConsentStepResult
ExplorerTestSession::authorize_single_translation(
    const ExplorerWindowToken& token_value) {
    ExplorerConsentStepResult result;
    result.reason = ExplorerEligibilityReason::MoveConsentRequired;
    if (impl_ == nullptr ||
        impl_->authority_kind != ExplorerAuthorityKind::UserConsent ||
        !impl_->ledger.contains(token_value) ||
        token_value.authority_kind() != ExplorerAuthorityKind::UserConsent ||
        token_value.consent_generation() !=
            impl_->consent.generations.token_generation ||
        impl_->consent.generations.move_prompt_generation == 0U ||
        impl_->consent.generations.move_confirmation_generation != 0U) {
        result.diagnostic = adapter_diagnostic(
            1414U,
            "Explorer move consent confirmation",
            "real-console move confirmation was not recorded in order");
        return result;
    }
    if (impl_->consent.generations.move_prompt_generation ==
        std::numeric_limits<std::uint64_t>::max()) {
        result.reason = ExplorerEligibilityReason::GenerationExhausted;
        return result;
    }
    result.generation =
        impl_->consent.generations.move_prompt_generation + 1U;
    impl_->consent.generations.move_confirmation_generation =
        result.generation;

    auto validation = validate_or_retire(*impl_, token_value);
    if (validation.reason != ExplorerEligibilityReason::Eligible ||
        !validation.snapshot.has_value()) {
        result.reason = validation.reason;
        result.diagnostic = std::move(validation.diagnostic);
        return result;
    }
    const auto authority = impl_->consent_move_authority.authorize(
        true, true, impl_->consent.generations);
    if (authority != ExplorerEligibilityReason::Eligible) {
        result.reason = authority;
        result.diagnostic = adapter_diagnostic(
            1415U,
            "Explorer consent generation chain",
            "two consent facts did not form one valid move authority");
        retire_target(*impl_);
        return result;
    }
    impl_->original_snapshot = *validation.snapshot;
    impl_->provisioning.initial_snapshot = *validation.snapshot;
    impl_->consent.move_authorized = true;
    result.reason = ExplorerEligibilityReason::Eligible;
    result.snapshot = std::move(validation.snapshot);
    return result;
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
    if (!cleanup_operation &&
        impl.authority_kind == ExplorerAuthorityKind::UserConsent &&
        !impl.consent_move_authority.available()) {
        result.reason = ExplorerEligibilityReason::MoveConsentRequired;
        result.diagnostic = adapter_diagnostic(
            1416U,
            "Explorer move consent authority",
            "primary translation requires a fresh second user consent");
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
    if (!cleanup_operation &&
        impl.authority_kind == ExplorerAuthorityKind::UserConsent) {
        const auto consent_gate = impl.consent_move_authority.try_consume(
            token.consent_generation());
        if (consent_gate != ExplorerEligibilityReason::Eligible) {
            result.stage = ExplorerOperationStage::Preflight;
            result.reason = consent_gate;
            result.diagnostic = adapter_diagnostic(
                1417U,
                "Explorer move consent authority",
                "consent authority was stale, mismatched, or already consumed");
            return result;
        }
        impl.consent.primary_authority_consumed = true;
    }
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
    if (impl_ != nullptr &&
        impl_->authority_kind == ExplorerAuthorityKind::UserConsent) {
        result.reason = ExplorerEligibilityReason::SafeCleanupNotPerformed;
        result.native_close_attempted = false;
        result.token_retired = !impl_->ledger.contains(token_value);
        result.diagnostic = adapter_diagnostic(
            1418U,
            "Explorer user-consent close boundary",
            "a user-created Explorer window may only be closed by the user");
        return result;
    }
    if (impl_ == nullptr ||
        disappearance_timeout <= std::chrono::milliseconds::zero() ||
        !impl_->issued_token.has_value() ||
        *impl_->issued_token != token_value || impl_->closing) {
        result.reason = ExplorerEligibilityReason::StaleToken;
        return result;
    }

    std::optional<ExplorerDiagnostic> validation_diagnostic;
    if (impl_->ledger.contains(token_value)) {
        auto validation = validate_or_retire(*impl_, token_value);
        if (validation.reason != ExplorerEligibilityReason::Eligible) {
            validation_diagnostic = std::move(validation.diagnostic);
        }
    }

    impl_->closing = true;
    const auto deadline =
        std::chrono::steady_clock::now() + disappearance_timeout;
    const auto cleanup =
        cleanup_provisioning_lease(*impl_, deadline, true);
    result.native_close_attempted = cleanup.native_quit_attempted;
    result.native_close_succeeded = cleanup.native_quit_succeeded;
    result.matching_registration_revoked =
        cleanup.matching_registration_revoked;
    result.exact_hwnd_invalidated = cleanup.exact_hwnd_invalidated;
    result.orphan_attribution_known = cleanup.orphan_attribution_known;
    result.window_disappeared = cleanup.matching_registration_revoked ||
                                cleanup.exact_hwnd_invalidated;
    result.token_retired = !impl_->ledger.contains(token_value);
    result.attributable_orphan = cleanup.attributable_orphan;
    result.browser_events_unadvised = cleanup.browser_events_unadvised;
    result.shell_events_unadvised = cleanup.shell_events_unadvised;
    result.browser_event_lifecycle_clean =
        cleanup.browser_event_lifecycle_clean;
    result.shell_event_lifecycle_clean =
        cleanup.shell_event_lifecycle_clean;
    result.reason = cleanup.completed() && result.token_retired
                        ? ExplorerEligibilityReason::Eligible
                        : ExplorerEligibilityReason::SafeCleanupNotPerformed;
    if (result.reason != ExplorerEligibilityReason::Eligible) {
        result.diagnostic = validation_diagnostic.has_value()
                                ? std::move(validation_diagnostic)
                                : std::optional<ExplorerDiagnostic>{
                                      adapter_diagnostic(
                                          1310U,
                                          "Explorer provisioning lease cleanup",
                                          "exact-object Quit, revoke/invalidation proof, or event Unadvise did not complete safely")};
    }
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

namespace {

template <typename ImplType>
[[nodiscard]] bool target_consent_prefix_valid(
    const ImplType& impl) noexcept {
    return detail::evaluate_target_consent_prefix(
        {impl.authority_kind,
         impl.consent.generations,
         impl.provisioning.baseline_exclusion_complete,
         impl.consent.unique_new_target,
         impl.consent.exact_target_location,
         impl.provisioning.token_issued && impl.consent.token_issued &&
             impl.issued_token.has_value() &&
             impl.issued_token->authority_kind() ==
                 ExplorerAuthorityKind::UserConsent,
         impl.issued_token.has_value() &&
             impl.ledger.contains(*impl.issued_token),
         impl.consent.move_authorized,
         impl.consent.primary_authority_consumed,
         impl.primary_apply_guard.consumed(),
         impl.restore_guard.consumed(),
         impl.glue_bound});
}

[[nodiscard]] bool same_monitor_and_dpi(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept {
    return leader.monitor_device_name == follower.monitor_device_name &&
           leader.monitor_rect == follower.monitor_rect &&
           leader.monitor_work_area == follower.monitor_work_area &&
           leader.dpi == follower.dpi;
}

template <typename ImplType>
[[nodiscard]] bool glue_binding_matches(
    const ImplType& impl,
    const detail::ExplorerGlueAuthoritySeal& seal,
    const ExplorerGlueWindowRole role) noexcept {
    const std::uint8_t expected_role =
        role == ExplorerGlueWindowRole::Leader ? 1U : 2U;
    return impl.glue_bound && seal.authority_id() != 0U &&
           seal.authority_generation() != 0U &&
           impl.glue_authority_id == seal.authority_id() &&
           impl.glue_authority_generation == seal.authority_generation() &&
           impl.glue_role == expected_role && impl.issued_token.has_value() &&
           impl.ledger.contains(*impl.issued_token);
}

[[nodiscard]] bool permit_matches(
    const detail::ExplorerGlueAuthoritySeal& seal,
    const detail::ExplorerGlueOperationPermit& permit,
    const ExplorerGlueWindowRole role) noexcept {
    return permit.authority_id() == seal.authority_id() &&
           permit.authority_generation() == seal.authority_generation() &&
           permit.glue_session_generation() != 0U &&
           permit.operation_generation() != 0U && permit.role() == role &&
           (permit.phase() != ExplorerGlueOperationPhase::ActiveFollower ||
            role == ExplorerGlueWindowRole::Follower);
}

[[nodiscard]] ExplorerGlueReason glue_reason_from_eligibility(
    const ExplorerEligibilityReason reason) noexcept {
    switch (reason) {
    case ExplorerEligibilityReason::Eligible:
        return ExplorerGlueReason::Eligible;
    case ExplorerEligibilityReason::WrongThread:
        return ExplorerGlueReason::WrongOwnerThread;
    case ExplorerEligibilityReason::MonitorChanged:
    case ExplorerEligibilityReason::DpiChanged:
    case ExplorerEligibilityReason::DpiContextMismatch:
    case ExplorerEligibilityReason::MonitorUnavailable:
        return ExplorerGlueReason::MonitorOrDpiMismatch;
    default:
        return ExplorerGlueReason::TargetChanged;
    }
}

} // namespace

detail::ExplorerGluePairInspection
detail::ExplorerGlueSessionBridge::inspect_pair(
    ExplorerTestSession& leader,
    ExplorerTestSession& follower) {
    ExplorerGluePairInspection result;
    if (leader.impl_ == nullptr || follower.impl_ == nullptr) {
        result.diagnostic = adapter_diagnostic(
            1500U, "Explorer Glue pair", "one target session is unavailable");
        return result;
    }
    auto& leader_impl = *leader.impl_;
    auto& follower_impl = *follower.impl_;
    if (GetCurrentThreadId() != leader_impl.controller_thread_id ||
        GetCurrentThreadId() != follower_impl.controller_thread_id ||
        leader_impl.controller_thread_id != follower_impl.controller_thread_id) {
        result.reason = ExplorerGlueReason::WrongOwnerThread;
        result.diagnostic = adapter_diagnostic(
            1501U,
            "Explorer Glue pair",
            "both targets must belong to the current owner STA");
        return result;
    }

    result.leader_target_consent_prefix_valid =
        target_consent_prefix_valid(leader_impl);
    result.follower_target_consent_prefix_valid =
        target_consent_prefix_valid(follower_impl);
    if (!result.leader_target_consent_prefix_valid ||
        !result.follower_target_consent_prefix_valid) {
        result.reason = ExplorerGlueReason::TargetConsentIncomplete;
        result.diagnostic = adapter_diagnostic(
            1502U,
            "Explorer Glue target consent",
            "both targets require an unconsumed target-consent prefix");
        return result;
    }

    const auto leader_key = reinterpret_cast<detail::NativeWindowKey>(
        leader_impl.window);
    const auto follower_key = reinterpret_cast<detail::NativeWindowKey>(
        follower_impl.window);
    result.pair_distinct =
        &leader != &follower && leader_key != 0U && follower_key != 0U &&
        leader_key != follower_key &&
        leader_impl.ledger.session_authority() !=
            follower_impl.ledger.session_authority() &&
        leader_impl.target_location != follower_impl.target_location;
    if (!result.pair_distinct) {
        result.reason = ExplorerGlueReason::PairNotDistinct;
        result.diagnostic = adapter_diagnostic(
            1503U,
            "Explorer Glue pair",
            "leader and follower capability identities must be distinct");
        return result;
    }

    result.follower_baseline_excluded_leader =
        std::find(follower_impl.forbidden_preexisting_hwnds.begin(),
                  follower_impl.forbidden_preexisting_hwnds.end(),
                  leader_key) !=
        follower_impl.forbidden_preexisting_hwnds.end();
    if (!result.follower_baseline_excluded_leader) {
        result.reason = ExplorerGlueReason::FollowerBaselineMissingLeader;
        result.diagnostic = adapter_diagnostic(
            1504U,
            "Explorer Glue follower baseline",
            "follower provisioning did not permanently exclude the leader");
        return result;
    }

    auto follower_live =
        validate_or_retire(follower_impl, *follower_impl.issued_token);
    if (follower_live.reason != ExplorerEligibilityReason::Eligible ||
        !follower_live.snapshot.has_value()) {
        result.reason = glue_reason_from_eligibility(follower_live.reason);
        result.diagnostic = std::move(follower_live.diagnostic);
        return result;
    }
    leader_impl.glue_peer_native_key = follower_key;
    leader_impl.glue_peer_target_location = follower_impl.target_location;
    leader_impl.glue_peer_process_id = follower_impl.process_id;
    leader_impl.glue_peer_thread_id = follower_impl.thread_id;
    leader_impl.glue_peer_capability_generation =
        follower_impl.issued_token->generation();
    leader_impl.glue_peer_consent_generation =
        follower_impl.issued_token->consent_generation();
    auto leader_live =
        validate_or_retire(leader_impl, *leader_impl.issued_token);
    if (leader_live.reason != ExplorerEligibilityReason::Eligible ||
        !leader_live.snapshot.has_value()) {
        leader_impl.glue_peer_native_key = 0U;
        leader_impl.glue_peer_process_id = 0U;
        leader_impl.glue_peer_thread_id = 0U;
        leader_impl.glue_peer_capability_generation = 0U;
        leader_impl.glue_peer_consent_generation = 0U;
        result.reason = glue_reason_from_eligibility(leader_live.reason);
        result.diagnostic = std::move(leader_live.diagnostic);
        return result;
    }

    result.same_monitor_and_dpi =
        same_monitor_and_dpi(*leader_live.snapshot, *follower_live.snapshot);
    if (!result.same_monitor_and_dpi) {
        leader_impl.glue_peer_native_key = 0U;
        leader_impl.glue_peer_process_id = 0U;
        leader_impl.glue_peer_thread_id = 0U;
        leader_impl.glue_peer_capability_generation = 0U;
        leader_impl.glue_peer_consent_generation = 0U;
        result.reason = ExplorerGlueReason::MonitorOrDpiMismatch;
        result.diagnostic = adapter_diagnostic(
            1505U,
            "Explorer Glue monitor/DPI",
            "leader and follower are not on one identical monitor/DPI baseline");
        return result;
    }

    const auto pair_reason = detail::evaluate_pair_authority(
        {true,
         result.leader_target_consent_prefix_valid,
         result.follower_target_consent_prefix_valid,
         leader_impl.ledger.session_authority() !=
             follower_impl.ledger.session_authority(),
         leader_key != follower_key,
         leader_impl.target_location != follower_impl.target_location,
         result.follower_baseline_excluded_leader,
         result.same_monitor_and_dpi});
    if (pair_reason != ExplorerGlueReason::Eligible) {
        leader_impl.glue_peer_native_key = 0U;
        leader_impl.glue_peer_process_id = 0U;
        leader_impl.glue_peer_thread_id = 0U;
        leader_impl.glue_peer_capability_generation = 0U;
        leader_impl.glue_peer_consent_generation = 0U;
        result.reason = pair_reason;
        return result;
    }

    result.reason = ExplorerGlueReason::Eligible;
    result.leader_snapshot = std::move(leader_live.snapshot);
    result.follower_snapshot = std::move(follower_live.snapshot);
    return result;
}

detail::ExplorerGlueBindResult
detail::ExplorerGlueSessionBridge::bind_pair(
    const ExplorerGlueAuthoritySeal& seal,
    ExplorerTestSession& leader,
    ExplorerTestSession& follower,
    const ExplorerWindowSnapshot& expected_leader,
    const ExplorerWindowSnapshot& expected_follower) {
    ExplorerGlueBindResult result;
    if (seal.authority_id_ == 0U || seal.authority_generation_ == 0U) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        return result;
    }
    auto inspection = inspect_pair(leader, follower);
    if (!inspection.succeeded()) {
        result.reason = inspection.reason;
        result.diagnostic = std::move(inspection.diagnostic);
        return result;
    }
    if (*inspection.leader_snapshot != expected_leader ||
        *inspection.follower_snapshot != expected_follower) {
        leader.impl_->glue_peer_native_key = 0U;
        leader.impl_->glue_peer_process_id = 0U;
        leader.impl_->glue_peer_thread_id = 0U;
        leader.impl_->glue_peer_capability_generation = 0U;
        leader.impl_->glue_peer_consent_generation = 0U;
        result.reason = ExplorerGlueReason::TargetChanged;
        result.diagnostic = adapter_diagnostic(
            1506U,
            "Explorer Glue consent preview",
            "target facts changed between Glue prompt and confirmation");
        return result;
    }

    auto& leader_impl = *leader.impl_;
    auto& follower_impl = *follower.impl_;
    leader_impl.glue_authority_id = seal.authority_id_;
    leader_impl.glue_authority_generation = seal.authority_generation_;
    leader_impl.glue_role = 1U;
    leader_impl.glue_native_operation_count = 0U;
    leader_impl.glue_bound = true;
    follower_impl.glue_authority_id = seal.authority_id_;
    follower_impl.glue_authority_generation = seal.authority_generation_;
    follower_impl.glue_role = 2U;
    follower_impl.glue_native_operation_count = 0U;
    follower_impl.glue_bound = true;

    result.leader = ExplorerGlueNativeBinding{
        reinterpret_cast<NativeWindowKey>(leader_impl.window),
        leader_impl.process_id,
        leader_impl.thread_id,
        leader_impl.issued_token->generation(),
        leader_impl.issued_token->consent_generation(),
        ExplorerGlueWindowRole::Leader};
    result.follower = ExplorerGlueNativeBinding{
        reinterpret_cast<NativeWindowKey>(follower_impl.window),
        follower_impl.process_id,
        follower_impl.thread_id,
        follower_impl.issued_token->generation(),
        follower_impl.issued_token->consent_generation(),
        ExplorerGlueWindowRole::Follower};
    result.leader_snapshot = std::move(inspection.leader_snapshot);
    result.follower_snapshot = std::move(inspection.follower_snapshot);
    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

ExplorerCaptureResult detail::ExplorerGlueSessionBridge::capture(
    const ExplorerGlueAuthoritySeal& seal,
    const ExplorerGlueWindowRole role,
    ExplorerTestSession& session) {
    if (session.impl_ == nullptr ||
        !glue_binding_matches(*session.impl_, seal, role)) {
        return {ExplorerEligibilityReason::StaleToken,
                ExplorerOperationStage::Preflight,
                std::nullopt,
                adapter_diagnostic(1507U,
                                   "Explorer Glue binding",
                                   "role binding is stale or mismatched")};
    }
    auto validation =
        validate_or_retire(*session.impl_, *session.impl_->issued_token);
    return {validation.reason,
            ExplorerOperationStage::Preflight,
            std::move(validation.snapshot),
            std::move(validation.diagnostic)};
}

detail::ExplorerGluePrepareResult
detail::ExplorerGlueSessionBridge::prepare_translation(
    const ExplorerGlueAuthoritySeal& seal,
    const ExplorerGlueOperationPermit& permit,
    ExplorerTestSession& session,
    const ExplorerWindowSnapshot& expected_before,
    const core::geometry::Rect& target_visible) {
    ExplorerGluePrepareResult result;
    const ExplorerGlueWindowRole role = permit.role_;
    if (session.impl_ == nullptr ||
        !glue_binding_matches(*session.impl_, seal, role) ||
        !permit_matches(seal, permit, role)) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        result.diagnostic = adapter_diagnostic(
            1508U,
            "Explorer Glue operation permit",
            "operation permit does not match the bound role/session");
        return result;
    }
    if (session.impl_->glue_native_operation_count >= 512U) {
        result.reason = ExplorerGlueReason::AuthorityConsumed;
        result.diagnostic = adapter_diagnostic(
            1509U,
            "Explorer Glue operation limit",
            "bounded native operation allowance is exhausted");
        return result;
    }

    auto before =
        validate_or_retire(*session.impl_, *session.impl_->issued_token);
    if (before.reason != ExplorerEligibilityReason::Eligible ||
        !before.snapshot.has_value()) {
        result.reason = glue_reason_from_eligibility(before.reason);
        result.diagnostic = std::move(before.diagnostic);
        return result;
    }
    if (*before.snapshot != expected_before) {
        result.reason = ExplorerGlueReason::TargetChanged;
        result.diagnostic = adapter_diagnostic(
            1510U,
            "Explorer Glue expected snapshot",
            "target changed before operation registration");
        return result;
    }
    if (!detail::rect_is_contained(target_visible,
                                   before.snapshot->monitor_work_area)) {
        result.reason = ExplorerGlueReason::UnsafeLayout;
        result.diagnostic = adapter_diagnostic(
            1511U,
            "Explorer Glue target geometry",
            "target visible rectangle is outside the frozen work area");
        return result;
    }
    const auto preparation =
        operations::window_translation::prepare_visible_translation(
            before.snapshot->positioning_rect,
            before.snapshot->visible_rect,
            target_visible);
    const auto mapped = detail::map_translation_status(preparation.status);
    if (mapped != ExplorerEligibilityReason::Eligible ||
        !preparation.target_positioning_rect.has_value()) {
        result.reason = ExplorerGlueReason::LayoutOperationFailed;
        result.diagnostic = adapter_diagnostic(
            1512U,
            "prepare_visible_translation",
            "shared translation bridge rejected the Glue target");
        return result;
    }

    result.prepared = ExplorerGluePreparedTranslation{
        *before.snapshot,
        target_visible,
        *preparation.target_positioning_rect,
        seal.authority_id_,
        seal.authority_generation_,
        permit.glue_session_generation_,
        permit.operation_generation_,
        permit.phase_,
        role};
    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

ExplorerOperationResult detail::ExplorerGlueSessionBridge::apply_prepared(
    const ExplorerGlueAuthoritySeal& seal,
    const ExplorerGlueOperationPermit& permit,
    ExplorerTestSession& session,
    const ExplorerGluePreparedTranslation& prepared,
    const BeforeNativeApply before_native_apply,
    void* const before_native_apply_context) {
    ExplorerOperationResult result;
    result.operation_id = operation_ids.issue();
    result.stage = permit.phase_ == ExplorerGlueOperationPhase::Restore
                       ? ExplorerOperationStage::Restore
                       : ExplorerOperationStage::Preflight;
    result.cleanup_operation =
        permit.phase_ == ExplorerGlueOperationPhase::Restore;
    if (session.impl_ == nullptr ||
        !glue_binding_matches(*session.impl_, seal, permit.role_) ||
        !permit_matches(seal, permit, permit.role_) ||
        prepared.authority_id_ != permit.authority_id_ ||
        prepared.authority_generation_ != permit.authority_generation_ ||
        prepared.glue_session_generation_ != permit.glue_session_generation_ ||
        prepared.operation_generation_ != permit.operation_generation_ ||
        prepared.phase_ != permit.phase_ || prepared.role_ != permit.role_) {
        result.reason = ExplorerEligibilityReason::ConsentGenerationMismatch;
        result.diagnostic = adapter_diagnostic(
            1513U,
            "Explorer Glue prepared operation",
            "prepared translation or permit is stale/mismatched");
        return result;
    }
    result.receipt = ExplorerOperationReceipt{*session.impl_->issued_token};
    result.receipt->before = prepared.before_;
    result.receipt->requested_visible_rect = prepared.requested_visible_;
    result.receipt->requested_positioning_rect =
        prepared.requested_positioning_;

    auto immediate =
        validate_or_retire(*session.impl_, *session.impl_->issued_token);
    if (immediate.reason != ExplorerEligibilityReason::Eligible ||
        !immediate.snapshot.has_value()) {
        result.reason = immediate.reason;
        result.diagnostic = std::move(immediate.diagnostic);
        return result;
    }
    if (*immediate.snapshot != prepared.before_) {
        retire_target(*session.impl_);
        result.reason = ExplorerEligibilityReason::TargetInvalidated;
        result.diagnostic = adapter_diagnostic(
            1514U,
            "Explorer Glue immediate preflight",
            "target changed after receipt registration and before native apply");
        return result;
    }
    if (session.impl_->glue_native_operation_count >= 512U) {
        result.reason = ExplorerEligibilityReason::OperationLimitReached;
        return result;
    }
    if (before_native_apply != nullptr &&
        !before_native_apply(before_native_apply_context)) {
        result.reason = ExplorerEligibilityReason::OperationLimitReached;
        result.diagnostic = adapter_diagnostic(
            1515U,
            "Explorer Glue pre-native registration",
            "the bounded pending receipt could not be registered before native apply");
        return result;
    }

    ++session.impl_->glue_native_operation_count;
    result.stage = permit.phase_ == ExplorerGlueOperationPhase::Restore
                       ? ExplorerOperationStage::Restore
                       : ExplorerOperationStage::NativeApply;
    result.native_apply_attempted = true;
    SetLastError(ERROR_SUCCESS);
    const BOOL native_result = SetWindowPos(
        session.impl_->window,
        nullptr,
        static_cast<int>(prepared.requested_positioning_.left()),
        static_cast<int>(prepared.requested_positioning_.top()),
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    const DWORD native_error = GetLastError();

    auto actual = validate_native_target(
        *session.impl_, session.impl_->issued_token.operator->(), false);
    if (actual.snapshot.has_value()) {
        result.receipt->actual = *actual.snapshot;
    }
    if (native_result == FALSE) {
        result.reason = ExplorerEligibilityReason::NativeApplyFailed;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "SetWindowPos",
            native_error,
            "Explorer Glue pure-translation call failed");
        if (actual.reason != ExplorerEligibilityReason::Eligible) {
            retire_target(*session.impl_);
        }
        return result;
    }
    result.stage = ExplorerOperationStage::PostVerification;
    if (actual.reason != ExplorerEligibilityReason::Eligible ||
        !actual.snapshot.has_value()) {
        retire_target(*session.impl_);
        result.reason = actual.reason;
        result.diagnostic = std::move(actual.diagnostic);
        return result;
    }

    const auto& after = *actual.snapshot;
    auto& receipt = *result.receipt;
    receipt.visible_target_verified =
        after.visible_rect == receipt.requested_visible_rect;
    receipt.positioning_target_verified =
        after.positioning_rect == prepared.requested_positioning_;
    receipt.size_preserved =
        same_snapshot_size(prepared.before_.visible_rect,
                           after.visible_rect) &&
        same_snapshot_size(prepared.before_.positioning_rect,
                           after.positioning_rect);
    receipt.identity_stable =
        prepared.before_.process_id == after.process_id &&
        prepared.before_.thread_id == after.thread_id &&
        prepared.before_.process_image_path == after.process_image_path &&
        prepared.before_.window_class == after.window_class &&
        prepared.before_.controller_security == after.controller_security &&
        prepared.before_.target_security == after.target_security;
    receipt.location_stable = after.exact_test_location;
    receipt.monitor_and_dpi_stable =
        prepared.before_.monitor_device_name == after.monitor_device_name &&
        prepared.before_.monitor_rect == after.monitor_rect &&
        prepared.before_.monitor_work_area == after.monitor_work_area &&
        prepared.before_.dpi == after.dpi;
    result.reason = detail::evaluate_post_verification(
        {session.impl_->ledger.contains(*session.impl_->issued_token),
         prepared.before_.process_id == after.process_id,
         prepared.before_.thread_id == after.thread_id,
         prepared.before_.process_image_path == after.process_image_path &&
             prepared.before_.window_class == after.window_class,
         receipt.location_stable,
         prepared.before_.controller_security == after.controller_security &&
             prepared.before_.target_security == after.target_security,
         prepared.before_.monitor_device_name == after.monitor_device_name &&
             prepared.before_.monitor_rect == after.monitor_rect &&
             prepared.before_.monitor_work_area == after.monitor_work_area,
         prepared.before_.dpi == after.dpi,
         receipt.size_preserved,
         receipt.visible_target_verified,
         receipt.positioning_target_verified});
    if (result.reason != ExplorerEligibilityReason::Eligible) {
        result.diagnostic = adapter_diagnostic(
            1515U,
            "Explorer Glue post-verification",
            "native success did not satisfy the exact Glue operation contract");
        if (result.reason != ExplorerEligibilityReason::PostVerificationFailed) {
            retire_target(*session.impl_);
        }
    }
    return result;
}

void detail::ExplorerGlueSessionBridge::release_pair(
    const ExplorerGlueAuthoritySeal& seal,
    ExplorerTestSession& leader,
    ExplorerTestSession& follower) noexcept {
    const auto release = [&seal](ExplorerTestSession& session) {
        if (session.impl_ != nullptr && session.impl_->glue_bound &&
            session.impl_->glue_authority_id == seal.authority_id_ &&
            session.impl_->glue_authority_generation ==
                seal.authority_generation_) {
            session.impl_->glue_bound = false;
            session.impl_->glue_authority_id = 0U;
            session.impl_->glue_authority_generation = 0U;
            session.impl_->glue_peer_native_key = 0U;
            session.impl_->glue_peer_process_id = 0U;
            session.impl_->glue_peer_thread_id = 0U;
            session.impl_->glue_peer_capability_generation = 0U;
            session.impl_->glue_peer_consent_generation = 0U;
            session.impl_->glue_role = 0U;
        }
    };
    release(leader);
    release(follower);
}

} // namespace panebind::platform::windows::explorer
