#pragma once

#include "platform/windows/explorer/explorer_glue_session.h"
#include "platform/windows/explorer/explorer_glue_types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace panebind::platform::windows::explorer::detail {

struct ExplorerGlueTargetConsentPrefixFacts final {
    ExplorerAuthorityKind authority_kind{
        ExplorerAuthorityKind::LegacyAutoProvisionDiagnostic};
    ExplorerConsentGenerationFacts generations;
    bool baseline_exclusion_complete{};
    bool unique_new_target{};
    bool exact_target_location{};
    bool token_issued{};
    bool token_active{};
    bool move_authorized{};
    bool primary_authority_consumed{};
    bool primary_guard_consumed{};
    bool restore_guard_consumed{};
    bool glue_bound{};
};

[[nodiscard]] bool evaluate_target_consent_prefix(
    const ExplorerGlueTargetConsentPrefixFacts& facts) noexcept;

struct ExplorerGluePairAuthorityFacts final {
    bool owner_thread_matches{};
    bool leader_prefix_valid{};
    bool follower_prefix_valid{};
    bool sessions_distinct{};
    bool windows_distinct{};
    bool locations_distinct{};
    bool follower_baseline_excludes_leader{};
    bool same_monitor_and_dpi{};
};

[[nodiscard]] ExplorerGlueReason evaluate_pair_authority(
    const ExplorerGluePairAuthorityFacts& facts) noexcept;

[[nodiscard]] constexpr bool follower_feedback_after_registration(
    const std::uint64_t receipt_sequence,
    const std::uint64_t registration_watermark) noexcept {
    return receipt_sequence != 0U &&
           receipt_sequence > registration_watermark;
}

// A queued Follower LOCATION may be allowed to remain after a Leader receipt
// only when it is already attributable to a completed earlier native operation
// or to the exact last acknowledged geometry. This is evaluated before any new
// native apply so an unrelated/user Follower receipt cannot be hidden by the
// geometry PaneBind is about to write.
[[nodiscard]] constexpr bool follower_receipt_precedes_new_native_apply(
    const std::uint64_t receipt_sequence,
    const bool prior_expected_geometry_matches,
    const std::uint64_t prior_registration_watermark,
    const bool prior_native_apply_attempted,
    const bool prior_exact_result,
    const bool acknowledged_duplicate_geometry_matches) noexcept {
    return acknowledged_duplicate_geometry_matches ||
           (prior_expected_geometry_matches && prior_native_apply_attempted &&
            prior_exact_result &&
            follower_feedback_after_registration(
                receipt_sequence, prior_registration_watermark));
}

enum class ExplorerGlueLeaderStartGeometry : std::uint8_t {
    ExactLayout,
    TranslatedSinceReceipt,
    ResizeOrMixed,
    TargetInvalidated,
};

// Out-of-context START and LOCATION can already be queued together when the
// owner regains control. START therefore accepts only a same-baseline pure
// translation from the exact armed layout; all state/identity drift and any
// resize remain terminal.
[[nodiscard]] ExplorerGlueLeaderStartGeometry evaluate_leader_start_geometry(
    const ExplorerWindowSnapshot& armed_layout,
    const ExplorerWindowSnapshot& live_at_batch_drain);

class ExplorerGlueAuthoritySeal final {
public:
    ExplorerGlueAuthoritySeal() = delete;
    ExplorerGlueAuthoritySeal(const ExplorerGlueAuthoritySeal&) = default;

    [[nodiscard]] std::uint64_t authority_id() const noexcept {
        return authority_id_;
    }
    [[nodiscard]] std::uint64_t authority_generation() const noexcept {
        return authority_generation_;
    }

private:
    ExplorerGlueAuthoritySeal(std::uint64_t authority_id,
                              std::uint64_t authority_generation) noexcept
        : authority_id_(authority_id),
          authority_generation_(authority_generation) {}

    std::uint64_t authority_id_{};
    std::uint64_t authority_generation_{};

    friend class ::panebind::platform::windows::explorer::ExplorerGlueConsent;
    friend class ::panebind::platform::windows::explorer::ExplorerGlueSession;
    friend class ExplorerGlueSessionBridge;
};

class ExplorerGlueOperationPermit final {
public:
    ExplorerGlueOperationPermit() = delete;
    ExplorerGlueOperationPermit(const ExplorerGlueOperationPermit&) = default;

    [[nodiscard]] std::uint64_t authority_id() const noexcept {
        return authority_id_;
    }
    [[nodiscard]] std::uint64_t authority_generation() const noexcept {
        return authority_generation_;
    }
    [[nodiscard]] std::uint64_t glue_session_generation() const noexcept {
        return glue_session_generation_;
    }
    [[nodiscard]] std::uint64_t operation_generation() const noexcept {
        return operation_generation_;
    }
    [[nodiscard]] ExplorerGlueOperationPhase phase() const noexcept {
        return phase_;
    }
    [[nodiscard]] ExplorerGlueWindowRole role() const noexcept {
        return role_;
    }

private:
    ExplorerGlueOperationPermit(
        std::uint64_t authority_id,
        std::uint64_t authority_generation,
        std::uint64_t glue_session_generation,
        std::uint64_t operation_generation,
        ExplorerGlueOperationPhase phase,
        ExplorerGlueWindowRole role) noexcept
        : authority_id_(authority_id),
          authority_generation_(authority_generation),
          glue_session_generation_(glue_session_generation),
          operation_generation_(operation_generation),
          phase_(phase),
          role_(role) {}

    std::uint64_t authority_id_{};
    std::uint64_t authority_generation_{};
    std::uint64_t glue_session_generation_{};
    std::uint64_t operation_generation_{};
    ExplorerGlueOperationPhase phase_{ExplorerGlueOperationPhase::Setup};
    ExplorerGlueWindowRole role_{ExplorerGlueWindowRole::Leader};

    friend class ::panebind::platform::windows::explorer::ExplorerGlueSession;
    friend class ExplorerGlueSessionBridge;
};

class ExplorerGlueNativeBinding final {
public:
    ExplorerGlueNativeBinding() = delete;
    ExplorerGlueNativeBinding(const ExplorerGlueNativeBinding&) = default;
    ExplorerGlueNativeBinding(ExplorerGlueNativeBinding&&) = default;
    ExplorerGlueNativeBinding& operator=(const ExplorerGlueNativeBinding&) =
        default;
    ExplorerGlueNativeBinding& operator=(ExplorerGlueNativeBinding&&) =
        default;

private:
    ExplorerGlueNativeBinding(std::uintptr_t native_key,
                              std::uint32_t process_id,
                              std::uint32_t thread_id,
                              std::uint64_t capability_generation,
                              std::uint64_t consent_generation,
                              ExplorerGlueWindowRole role) noexcept
        : native_key_(native_key),
          process_id_(process_id),
          thread_id_(thread_id),
          capability_generation_(capability_generation),
          consent_generation_(consent_generation),
          role_(role) {}

    std::uintptr_t native_key_{};
    std::uint32_t process_id_{};
    std::uint32_t thread_id_{};
    std::uint64_t capability_generation_{};
    std::uint64_t consent_generation_{};
    ExplorerGlueWindowRole role_{ExplorerGlueWindowRole::Leader};

    friend class ::panebind::platform::windows::explorer::ExplorerGlueSession;
    friend class ExplorerGlueSessionBridge;
    friend class ExplorerGlueSessionDiagnostics;
};

struct ExplorerGluePairInspection final {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    std::optional<ExplorerDiagnostic> diagnostic;
    std::optional<ExplorerWindowSnapshot> leader_snapshot;
    std::optional<ExplorerWindowSnapshot> follower_snapshot;
    bool leader_target_consent_prefix_valid{};
    bool follower_target_consent_prefix_valid{};
    bool follower_baseline_excluded_leader{};
    bool pair_distinct{};
    bool same_monitor{};
    bool same_dpi{};
    bool same_monitor_and_dpi{};
    bool glue_authority_bound{};
    bool glue_authority_consumed{};
    bool native_apply_attempted{};
    bool temporary_peer_exception_retained{};

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible &&
               leader_snapshot.has_value() && follower_snapshot.has_value();
    }
};

// Pure readiness projection over one live pair inspection. It computes
// capacity only and cannot bind authority, perform a native operation, or arm
// the event source. Keeping this separate also makes too-large -> resized-by-
// the-user -> fits transitions deterministic and repeatable.
[[nodiscard]] ExplorerGlueLayoutReadinessResult
evaluate_layout_readiness_preview(ExplorerGluePairInspection inspection);

struct ExplorerGlueBindResult final {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    std::optional<ExplorerDiagnostic> diagnostic;
    std::optional<ExplorerGlueNativeBinding> leader;
    std::optional<ExplorerGlueNativeBinding> follower;
    std::optional<ExplorerWindowSnapshot> leader_snapshot;
    std::optional<ExplorerWindowSnapshot> follower_snapshot;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible && leader.has_value() &&
               follower.has_value() && leader_snapshot.has_value() &&
               follower_snapshot.has_value();
    }
};

class ExplorerGluePreparedTranslation final {
public:
    ExplorerGluePreparedTranslation() = delete;
    ExplorerGluePreparedTranslation(
        const ExplorerGluePreparedTranslation&) = default;
    ExplorerGluePreparedTranslation(ExplorerGluePreparedTranslation&&) =
        default;
    ExplorerGluePreparedTranslation& operator=(
        const ExplorerGluePreparedTranslation&) = default;
    ExplorerGluePreparedTranslation& operator=(
        ExplorerGluePreparedTranslation&&) = default;

private:
    ExplorerGluePreparedTranslation(
        ExplorerWindowSnapshot before,
        core::geometry::Rect requested_visible,
        core::geometry::Rect requested_positioning,
        std::uint64_t authority_id,
        std::uint64_t authority_generation,
        std::uint64_t glue_session_generation,
        std::uint64_t operation_generation,
        ExplorerGlueOperationPhase phase,
        ExplorerGlueWindowRole role) noexcept
        : before_(std::move(before)),
          requested_visible_(requested_visible),
          requested_positioning_(requested_positioning),
          authority_id_(authority_id),
          authority_generation_(authority_generation),
          glue_session_generation_(glue_session_generation),
          operation_generation_(operation_generation),
          phase_(phase),
          role_(role) {}

    ExplorerWindowSnapshot before_;
    core::geometry::Rect requested_visible_;
    core::geometry::Rect requested_positioning_;
    std::uint64_t authority_id_{};
    std::uint64_t authority_generation_{};
    std::uint64_t glue_session_generation_{};
    std::uint64_t operation_generation_{};
    ExplorerGlueOperationPhase phase_{ExplorerGlueOperationPhase::Setup};
    ExplorerGlueWindowRole role_{ExplorerGlueWindowRole::Leader};

    friend class ::panebind::platform::windows::explorer::ExplorerGlueSession;
    friend class ExplorerGlueSessionBridge;
};

struct ExplorerGluePrepareResult final {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    std::optional<ExplorerDiagnostic> diagnostic;
    std::optional<ExplorerGluePreparedTranslation> prepared;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible && prepared.has_value();
    }
};

class ExplorerGlueSessionBridge final {
private:
    using BeforeNativeApply = bool (*)(void*) noexcept;

    [[nodiscard]] static ExplorerGluePairInspection inspect_pair(
        ExplorerTestSession& leader,
        ExplorerTestSession& follower,
        bool retain_peer_exception);
    [[nodiscard]] static ExplorerGlueBindResult bind_pair(
        const ExplorerGlueAuthoritySeal& seal,
        ExplorerTestSession& leader,
        ExplorerTestSession& follower,
        const ExplorerWindowSnapshot& expected_leader,
        const ExplorerWindowSnapshot& expected_follower);
    [[nodiscard]] static ExplorerCaptureResult capture(
        const ExplorerGlueAuthoritySeal& seal,
        ExplorerGlueWindowRole role,
        ExplorerTestSession& session);
    [[nodiscard]] static ExplorerGluePrepareResult prepare_translation(
        const ExplorerGlueAuthoritySeal& seal,
        const ExplorerGlueOperationPermit& permit,
        ExplorerTestSession& session,
        const ExplorerWindowSnapshot& expected_before,
        const core::geometry::Rect& target_visible);
    [[nodiscard]] static ExplorerOperationResult apply_prepared(
        const ExplorerGlueAuthoritySeal& seal,
        const ExplorerGlueOperationPermit& permit,
        ExplorerTestSession& session,
        const ExplorerGluePreparedTranslation& prepared,
        BeforeNativeApply before_native_apply,
        void* before_native_apply_context);
    static void release_pair(const ExplorerGlueAuthoritySeal& seal,
                             ExplorerTestSession& leader,
                             ExplorerTestSession& follower) noexcept;

    friend class ::panebind::platform::windows::explorer::ExplorerGlueConsent;
    friend class ::panebind::platform::windows::explorer::ExplorerGlueSession;
};

struct ExplorerGlueDiagnosticIdentity final {
    std::uintptr_t native_key{};
    std::uint32_t process_id{};
    std::uint32_t thread_id{};
    std::uint64_t capability_generation{};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
};

struct ExplorerGlueDiagnostics final {
    std::optional<ExplorerGlueDiagnosticIdentity> leader;
    std::optional<ExplorerGlueDiagnosticIdentity> follower;
};

// Ignored-evidence correlation only. This cannot create an authority, bind a
// session, or execute an operation.
class ExplorerGlueSessionDiagnostics final {
public:
    [[nodiscard]] static ExplorerGlueDiagnostics read(
        const ExplorerGlueSession& session) noexcept;
};

} // namespace panebind::platform::windows::explorer::detail
