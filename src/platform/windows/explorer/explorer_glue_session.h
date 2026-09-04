#pragma once

#include "core/behavior/glue_move_coordinator.h"
#include "platform/windows/explorer/explorer_glue_types.h"
#include "platform/windows/explorer/explorer_session.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace panebind::platform::windows::explorer {

namespace detail {
class ExplorerGlueSessionBridge;
class ExplorerGlueSessionDiagnostics;
}

class ExplorerGlueConsent;
class ExplorerGlueSession;
class ExplorerGlueEventSource;
struct ExplorerGlueEvent;
struct ExplorerGlueBeginResult;
struct ExplorerGlueAuthorizeResult;

enum class ExplorerGlueReason : std::uint8_t {
    Eligible,
    InvalidArgument,
    WrongOwnerThread,
    TargetConsentIncomplete,
    PairNotDistinct,
    FollowerBaselineMissingLeader,
    TargetChanged,
    MonitorOrDpiMismatch,
    GlueConsentRequired,
    ConsentGenerationMismatch,
    AuthorityConsumed,
    UnsafeLayout,
    LayoutOperationFailed,
    TopologyInvalid,
    EventSourceFailed,
    EventQueueOverflow,
    BehaviorAborted,
    TimedOut,
    RestoreFailed,
    CleanupLifecycleFailed,
};

enum class ExplorerGlueStage : std::uint8_t {
    PairValidation,
    Consent,
    Layout,
    Topology,
    EventSource,
    ActiveSession,
    Completion,
    Restore,
};

enum class ExplorerGlueLayoutOrientation : std::uint8_t {
    Horizontal,
    Vertical,
};

enum class ExplorerGlueOperationPhase : std::uint8_t {
    Setup,
    ActiveFollower,
    Restore,
};

struct ExplorerGlueConsentGenerations {
    std::uint64_t pair_preview_generation{};
    std::uint64_t prompt_generation{};
    std::uint64_t confirmation_generation{};
    std::uint64_t authority_generation{};

    friend bool operator==(const ExplorerGlueConsentGenerations&,
                           const ExplorerGlueConsentGenerations&) = default;
};

struct ExplorerGlueLayoutPlan {
    ExplorerGlueLayoutOrientation orientation{
        ExplorerGlueLayoutOrientation::Horizontal};
    core::geometry::Rect leader_target_visible;
    core::geometry::Rect follower_target_visible;

    friend bool operator==(const ExplorerGlueLayoutPlan&,
                           const ExplorerGlueLayoutPlan&) = default;
};

struct ExplorerGlueLayoutFit final {
    // An unrepresentable required span remains explicit rather than wrapping;
    // that orientation cannot fit. Native Explorer rectangles are narrower,
    // but deterministic model tests retain the checked-arithmetic contract.
    std::optional<core::geometry::Size> required;
    core::geometry::Size available;
    bool fits{};

    friend bool operator==(const ExplorerGlueLayoutFit&,
                           const ExplorerGlueLayoutFit&) = default;
};

struct ExplorerGlueLayoutReadiness final {
    core::geometry::Size leader_visible_size;
    core::geometry::Size follower_visible_size;
    core::geometry::Size work_area_size;
    ExplorerGlueLayoutFit horizontal;
    ExplorerGlueLayoutFit vertical;
    std::optional<ExplorerGlueLayoutOrientation> orientation;

    friend bool operator==(const ExplorerGlueLayoutReadiness&,
                           const ExplorerGlueLayoutReadiness&) = default;
};

struct ExplorerGlueLayoutReadinessResult final {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    ExplorerGlueStage stage{ExplorerGlueStage::PairValidation};
    std::optional<ExplorerDiagnostic> diagnostic;
    bool leader_target_consent_prefix_valid{};
    bool follower_target_consent_prefix_valid{};
    bool follower_baseline_excluded_leader{};
    bool pair_distinct{};
    bool same_monitor{};
    bool same_dpi{};
    std::optional<ExplorerWindowSnapshot> leader_snapshot;
    std::optional<ExplorerWindowSnapshot> follower_snapshot;
    std::optional<ExplorerGlueLayoutReadiness> readiness;
    std::optional<ExplorerGlueLayoutPlan> layout;

    // These explicit audit fields remain false for every preview result. The
    // preview may retire an already-invalid target during live validation, but
    // it never binds/consumes Glue authority, performs a native apply, or arms
    // an event source.
    bool glue_authority_bound{};
    bool glue_authority_consumed{};
    bool native_apply_attempted{};
    bool event_source_armed{};
    bool temporary_peer_exception_retained{};

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible &&
               readiness.has_value() && layout.has_value();
    }
};

struct ExplorerGlueOperationRecord {
    ExplorerGlueOperationPhase phase{ExplorerGlueOperationPhase::Setup};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
    std::uint64_t behavior_operation_generation{};
    std::uint64_t source_leader_sequence{};
    ExplorerOperationResult result;
};

struct ExplorerGlueTraceRecord {
    std::uint64_t trace_sequence{};
    std::uint64_t glue_session_generation{};
    std::uint64_t event_sequence{};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
    std::optional<core::behavior::GlueEventKind> event_kind;
    std::optional<core::behavior::GlueDecisionKind> decision_kind;
    std::optional<core::behavior::GlueAbortReason> abort_reason;
    std::optional<core::geometry::Rect> visible_rect;
    std::uint64_t behavior_operation_generation{};
};

struct ExplorerGlueFacts {
    ExplorerGlueConsentGenerations consent_generations;
    std::uint64_t glue_authority_id{};
    std::uint64_t glue_session_generation{};
    bool leader_target_consent_prefix_valid{};
    bool follower_target_consent_prefix_valid{};
    bool follower_baseline_excluded_leader{};
    bool pair_distinct{};
    bool same_monitor_and_dpi{};
    bool glue_consent_confirmed{};
    bool test_layout_planned{};
    bool test_layout_exact{};
    bool topology_exact_two_window_component{};
    bool event_source_armed{};
    bool event_source_stopped{};
    bool event_source_lifecycle_clean{};
    bool leader_restore_attempted{};
    bool follower_restore_attempted{};
    bool leader_restored_exact{};
    bool follower_restored_exact{};
    bool user_windows_close_attempted{};
    std::size_t leader_start_count{};
    std::size_t leader_location_count{};
    std::size_t leader_end_count{};
    std::size_t follower_native_apply_count{};
    std::size_t follower_noop_count{};
    std::size_t follower_feedback_count{};
    std::size_t pending_capacity{};
    std::size_t max_pending_depth{};
    std::size_t event_queue_capacity{};
    std::size_t max_event_queue_depth{};
    std::size_t accepted_event_count{};
    std::size_t ignored_event_count{};
    std::size_t suppressed_feedback_count{};
    std::size_t duplicate_feedback_count{};
    std::size_t missing_feedback_count{};
    std::size_t reconciled_feedback_count{};
    std::size_t unexpected_feedback_count{};
    core::behavior::GlueMoveState behavior_state{
        core::behavior::GlueMoveState::Idle};
    std::optional<core::behavior::GlueAbortReason> behavior_abort_reason;
    std::optional<ExplorerGlueLayoutPlan> layout;
    std::optional<ExplorerWindowSnapshot> leader_original;
    std::optional<ExplorerWindowSnapshot> follower_original;
    std::optional<ExplorerWindowSnapshot> leader_layout;
    std::optional<ExplorerWindowSnapshot> follower_layout;
    std::optional<ExplorerWindowSnapshot> leader_final;
    std::optional<ExplorerWindowSnapshot> follower_final;
    std::optional<ExplorerWindowSnapshot> leader_restored;
    std::optional<ExplorerWindowSnapshot> follower_restored;
};

struct ExplorerGlueStepResult {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    ExplorerGlueStage stage{ExplorerGlueStage::PairValidation};
    std::optional<ExplorerDiagnostic> diagnostic;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible;
    }
};

// Owner-thread-affine because it retains two consent-bound COM sessions until
// confirmation or cancellation. Its owning unique_ptr must not cross threads.
class ExplorerGlueConsent final {
public:
    ~ExplorerGlueConsent() noexcept;

    ExplorerGlueConsent(const ExplorerGlueConsent&) = delete;
    ExplorerGlueConsent& operator=(const ExplorerGlueConsent&) = delete;
    ExplorerGlueConsent(ExplorerGlueConsent&&) = delete;
    ExplorerGlueConsent& operator=(ExplorerGlueConsent&&) = delete;

    [[nodiscard]] static ExplorerGlueBeginResult begin(
        std::unique_ptr<ExplorerTestSession> leader,
        std::unique_ptr<ExplorerTestSession> follower);

    // Repeatable live readiness preview. References remain owned by the caller;
    // formal begin independently re-inspects and replans to close the preview-
    // to-authority TOCTOU boundary.
    [[nodiscard]] static ExplorerGlueLayoutReadinessResult
    preview_layout_readiness(ExplorerTestSession& leader,
                             ExplorerTestSession& follower);
    [[nodiscard]] ExplorerGlueStepResult record_glue_prompt();
    [[nodiscard]] ExplorerGlueAuthorizeResult confirm_user_glue();
    [[nodiscard]] const ExplorerGlueFacts& facts() const noexcept;

private:
    struct Impl;
    explicit ExplorerGlueConsent(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

struct ExplorerGlueBeginResult {
    ExplorerGlueReason reason{ExplorerGlueReason::InvalidArgument};
    std::unique_ptr<ExplorerGlueConsent> consent;
    std::optional<ExplorerDiagnostic> diagnostic;
    ExplorerGlueFacts facts;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible && consent != nullptr;
    }
};

struct ExplorerGlueAuthorizeResult {
    ExplorerGlueReason reason{ExplorerGlueReason::GlueConsentRequired};
    std::unique_ptr<ExplorerGlueSession> session;
    std::optional<ExplorerDiagnostic> diagnostic;
    ExplorerGlueFacts facts;

    [[nodiscard]] bool succeeded() const noexcept {
        return reason == ExplorerGlueReason::Eligible && session != nullptr;
    }
};

// Strictly owner-thread-affine UAT aggregate. Its owning unique_ptr must not be
// transferred across threads. All normal terminal paths run on that owner and
// finish unhook + restore before returning; foreign-thread destruction takes a
// deliberate safe leak rather than freeing live hook/COM state.
class ExplorerGlueSession final {
public:
    ~ExplorerGlueSession() noexcept;

    ExplorerGlueSession(const ExplorerGlueSession&) = delete;
    ExplorerGlueSession& operator=(const ExplorerGlueSession&) = delete;
    ExplorerGlueSession(ExplorerGlueSession&&) = delete;
    ExplorerGlueSession& operator=(ExplorerGlueSession&&) = delete;

    [[nodiscard]] ExplorerGlueStepResult setup_test_layout();
    [[nodiscard]] ExplorerGlueStepResult arm();

    // Every terminal path stops/unhooks the event source and performs the safe
    // independent two-window restore before this call returns.
    [[nodiscard]] ExplorerGlueStepResult run_until_terminal(
        std::chrono::milliseconds timeout);
    [[nodiscard]] ExplorerGlueStepResult cancel_and_restore();

    [[nodiscard]] const ExplorerGlueFacts& facts() const noexcept;
    [[nodiscard]] const std::vector<ExplorerGlueOperationRecord>& operations()
        const noexcept;
    [[nodiscard]] const std::vector<ExplorerGlueTraceRecord>& trace()
        const noexcept;

private:
    struct Impl;
    explicit ExplorerGlueSession(std::unique_ptr<Impl> impl) noexcept;
    [[nodiscard]] ExplorerOperationResult execute_translation(
        ExplorerGlueWindowRole role,
        ExplorerGlueOperationPhase phase,
        std::uint64_t behavior_operation_generation,
        std::uint64_t source_leader_sequence,
        const ExplorerWindowSnapshot& expected_before,
        const core::geometry::Rect& target_visible);
    [[nodiscard]] static bool register_pending_before_native(
        void* context) noexcept;
    [[nodiscard]] ExplorerGlueStepResult process_event(
        const ExplorerGlueEvent& event,
        const std::optional<core::geometry::Rect>& observed_geometry =
            std::nullopt);
    [[nodiscard]] ExplorerGlueStepResult drain_event_source();
    [[nodiscard]] ExplorerGlueStepResult finish_and_restore(
        ExplorerGlueReason terminal_reason,
        ExplorerGlueStage terminal_stage);
    [[nodiscard]] ExplorerGlueStepResult setup_test_layout_impl();
    [[nodiscard]] ExplorerGlueStepResult arm_impl();
    [[nodiscard]] ExplorerGlueStepResult run_until_terminal_impl(
        std::chrono::milliseconds timeout);
    void record_trace(
        const ExplorerGlueEvent* event,
        const core::behavior::GlueDecision* decision,
        const std::optional<core::geometry::Rect>& visible_rect = std::nullopt,
        std::uint64_t behavior_operation_generation = 0U);
    std::unique_ptr<Impl> impl_;

    friend class ExplorerGlueConsent;
    friend class ExplorerGlueEventSource;
    friend class detail::ExplorerGlueSessionBridge;
    friend class detail::ExplorerGlueSessionDiagnostics;
};

// Deterministic R1-C2B UAT fixture layout only; this is not a Snap API.
[[nodiscard]] std::optional<ExplorerGlueLayoutPlan>
plan_explorer_glue_test_layout(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept;

} // namespace panebind::platform::windows::explorer
