#include "platform/windows/explorer/explorer_glue_session.h"

#include "core/geometry/checked_arithmetic.h"
#include "core/model/window_id.h"
#include "core/topology/window_adjacency.h"
#include "platform/windows/explorer/explorer_glue_event_source.h"
#include "platform/windows/explorer/explorer_glue_quantum.h"
#include "platform/windows/explorer/explorer_glue_session_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace panebind::platform::windows::explorer {
namespace {

constexpr std::size_t kPendingCapacity = 64U;
constexpr std::size_t kEventQueueCapacity = 512U;
constexpr std::size_t kTraceCapacity = 4096U;
constexpr std::size_t kNativeOperationCapacity = 512U;

class UniqueKernelHandle final {
public:
    explicit UniqueKernelHandle(HANDLE value = nullptr) noexcept
        : value_(value) {}
    ~UniqueKernelHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(CloseHandle(value_));
        }
    }
    UniqueKernelHandle(const UniqueKernelHandle&) = delete;
    UniqueKernelHandle& operator=(const UniqueKernelHandle&) = delete;
    [[nodiscard]] HANDLE get() const noexcept { return value_; }

private:
    HANDLE value_{};
};

std::atomic<std::uint64_t> next_glue_authority_id{1U};

[[nodiscard]] std::uint64_t allocate_glue_authority_id() noexcept {
    auto value = next_glue_authority_id.fetch_add(1U,
                                                   std::memory_order_relaxed);
    if (value == 0U ||
        value == std::numeric_limits<std::uint64_t>::max()) {
        return 0U;
    }
    return value;
}

[[nodiscard]] std::optional<core::geometry::Distance> extent(
    const core::geometry::Coordinate maximum,
    const core::geometry::Coordinate minimum) noexcept {
    const auto value = core::geometry::checked_difference(maximum, minimum);
    if (!value.has_value() || *value <= 0) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<core::geometry::Coordinate> add(
    const core::geometry::Coordinate value,
    const core::geometry::Distance delta) noexcept {
    return core::geometry::checked_add(value, delta);
}

[[nodiscard]] ExplorerDiagnostic glue_diagnostic(
    const std::uint64_t code,
    std::string api,
    std::string detail) {
    return {ExplorerDiagnosticDomain::Adapter,
            code,
            std::move(api),
            std::move(detail),
            {}};
}

[[nodiscard]] bool same_pair_monitor(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept {
    return leader.monitor_device_name == follower.monitor_device_name &&
           leader.monitor_rect == follower.monitor_rect &&
           leader.monitor_work_area == follower.monitor_work_area;
}

[[nodiscard]] bool same_pair_dpi(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept {
    return leader.dpi == follower.dpi;
}

[[nodiscard]] bool same_pair_monitor_and_dpi(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept {
    return same_pair_monitor(leader, follower) &&
           same_pair_dpi(leader, follower);
}

// Single checked source of truth for preview explanation and fixture planning.
// A missing required Size means that orientation overflowed checked arithmetic;
// it is reported as not fitting without preventing the other orientation.
[[nodiscard]] std::optional<ExplorerGlueLayoutReadiness>
measure_layout_readiness(const ExplorerWindowSnapshot& leader,
                         const ExplorerWindowSnapshot& follower) noexcept {
    if (!same_pair_monitor_and_dpi(leader, follower)) {
        return std::nullopt;
    }

    const auto available_width = extent(leader.monitor_work_area.right(),
                                        leader.monitor_work_area.left());
    const auto available_height = extent(leader.monitor_work_area.bottom(),
                                         leader.monitor_work_area.top());
    const auto leader_width =
        extent(leader.visible_rect.right(), leader.visible_rect.left());
    const auto leader_height =
        extent(leader.visible_rect.bottom(), leader.visible_rect.top());
    const auto follower_width =
        extent(follower.visible_rect.right(), follower.visible_rect.left());
    const auto follower_height =
        extent(follower.visible_rect.bottom(), follower.visible_rect.top());
    if (!available_width || !available_height || !leader_width ||
        !leader_height || !follower_width || !follower_height) {
        return std::nullopt;
    }

    const core::geometry::Size available{*available_width, *available_height};
    ExplorerGlueLayoutReadiness readiness;
    readiness.leader_visible_size = {*leader_width, *leader_height};
    readiness.follower_visible_size = {*follower_width, *follower_height};
    readiness.work_area_size = available;
    readiness.horizontal.available = available;
    readiness.vertical.available = available;

    const auto horizontal_width =
        core::geometry::checked_add(*leader_width, *follower_width);
    if (horizontal_width.has_value()) {
        readiness.horizontal.required = core::geometry::Size{
            *horizontal_width, std::max(*leader_height, *follower_height)};
        readiness.horizontal.fits =
            readiness.horizontal.required->width <= available.width &&
            readiness.horizontal.required->height <= available.height;
    }

    const auto vertical_height =
        core::geometry::checked_add(*leader_height, *follower_height);
    if (vertical_height.has_value()) {
        readiness.vertical.required = core::geometry::Size{
            std::max(*leader_width, *follower_width), *vertical_height};
        readiness.vertical.fits =
            readiness.vertical.required->width <= available.width &&
            readiness.vertical.required->height <= available.height;
    }

    if (readiness.horizontal.fits) {
        readiness.orientation = ExplorerGlueLayoutOrientation::Horizontal;
    } else if (readiness.vertical.fits) {
        readiness.orientation = ExplorerGlueLayoutOrientation::Vertical;
    }
    return readiness;
}

[[nodiscard]] bool same_non_geometry_snapshot(
    const ExplorerWindowSnapshot& left,
    const ExplorerWindowSnapshot& right) noexcept {
    return left.process_id == right.process_id &&
           left.thread_id == right.thread_id &&
           left.process_image_path == right.process_image_path &&
           left.window_class == right.window_class && left.dpi == right.dpi &&
           left.monitor_device_name == right.monitor_device_name &&
           left.monitor_rect == right.monitor_rect &&
           left.monitor_work_area == right.monitor_work_area &&
           left.controller_security == right.controller_security &&
           left.target_security == right.target_security &&
           left.root_top_level == right.root_top_level &&
           left.visible == right.visible && left.cloaked == right.cloaked &&
           left.minimized == right.minimized &&
           left.maximized == right.maximized &&
           left.on_current_virtual_desktop ==
               right.on_current_virtual_desktop &&
           left.exact_test_location == right.exact_test_location;
}

[[nodiscard]] std::optional<core::topology::WindowAdjacencyGraph>
build_exact_pair_graph(const ExplorerWindowSnapshot& leader,
                       const ExplorerWindowSnapshot& follower) {
    const std::array windows{
        core::topology::WindowGeometry{core::model::WindowId{"leader"},
                                       leader.visible_rect},
        core::topology::WindowGeometry{core::model::WindowId{"follower"},
                                       follower.visible_rect},
    };
    auto graph = core::topology::WindowAdjacencyGraph::build(
        std::span<const core::topology::WindowGeometry>{windows},
        core::topology::AdjacencyOptions{0});
    const auto component =
        graph.connected_component(core::model::WindowId{"leader"});
    if (graph.windows().size() != 2U || graph.relations().size() != 1U ||
        component.size() != 2U ||
        std::find(component.begin(),
                  component.end(),
                  core::model::WindowId{"follower"}) == component.end()) {
        return std::nullopt;
    }
    return graph;
}

[[nodiscard]] core::behavior::GlueWindowRole core_role(
    const ExplorerGlueWindowRole role) noexcept {
    return role == ExplorerGlueWindowRole::Leader
               ? core::behavior::GlueWindowRole::Leader
               : core::behavior::GlueWindowRole::Follower;
}

[[nodiscard]] core::behavior::GlueEventKind core_event_kind(
    const ExplorerGlueEventKind kind) noexcept {
    switch (kind) {
    case ExplorerGlueEventKind::MoveResizeStarted:
        return core::behavior::GlueEventKind::MoveResizeStarted;
    case ExplorerGlueEventKind::GeometryChanged:
        return core::behavior::GlueEventKind::GeometryChanged;
    case ExplorerGlueEventKind::MoveResizeEnded:
        return core::behavior::GlueEventKind::MoveResizeEnded;
    case ExplorerGlueEventKind::TargetDestroyed:
        break;
    }
    return core::behavior::GlueEventKind::GeometryChanged;
}

[[nodiscard]] core::behavior::FollowerOperationOutcome operation_outcome(
    const ExplorerOperationResult& operation) noexcept {
    if (operation.reason == ExplorerEligibilityReason::Eligible &&
        operation.receipt.has_value() && operation.receipt->actual.has_value()) {
        return core::behavior::FollowerOperationOutcome::Exact;
    }
    if (operation.reason == ExplorerEligibilityReason::NativeApplyFailed) {
        return core::behavior::FollowerOperationOutcome::NativeApplyFailed;
    }
    if (operation.reason ==
        ExplorerEligibilityReason::PostVerificationFailed) {
        return core::behavior::FollowerOperationOutcome::PostVerificationFailed;
    }
    return core::behavior::FollowerOperationOutcome::TargetInvalidated;
}

} // namespace

bool detail::evaluate_target_consent_prefix(
    const ExplorerGlueTargetConsentPrefixFacts& facts) noexcept {
    const auto& generation = facts.generations;
    return facts.authority_kind == ExplorerAuthorityKind::UserConsent &&
           facts.baseline_exclusion_complete && facts.unique_new_target &&
           facts.exact_target_location && facts.token_issued &&
           facts.token_active && generation.baseline_generation != 0U &&
           generation.baseline_generation <
               generation.target_prompt_generation &&
           generation.target_prompt_generation <
               generation.target_confirmation_generation &&
           generation.target_confirmation_generation <
               generation.eligibility_generation &&
           generation.eligibility_generation < generation.token_generation &&
           generation.move_prompt_generation == 0U &&
           generation.move_confirmation_generation == 0U &&
           !facts.move_authorized && !facts.primary_authority_consumed &&
           !facts.primary_guard_consumed && !facts.restore_guard_consumed &&
           !facts.glue_bound;
}

ExplorerGlueReason detail::evaluate_pair_authority(
    const ExplorerGluePairAuthorityFacts& facts) noexcept {
    if (!facts.owner_thread_matches) {
        return ExplorerGlueReason::WrongOwnerThread;
    }
    if (!facts.leader_prefix_valid || !facts.follower_prefix_valid) {
        return ExplorerGlueReason::TargetConsentIncomplete;
    }
    if (!facts.sessions_distinct || !facts.windows_distinct ||
        !facts.locations_distinct) {
        return ExplorerGlueReason::PairNotDistinct;
    }
    if (!facts.follower_baseline_excludes_leader) {
        return ExplorerGlueReason::FollowerBaselineMissingLeader;
    }
    if (!facts.same_monitor_and_dpi) {
        return ExplorerGlueReason::MonitorOrDpiMismatch;
    }
    return ExplorerGlueReason::Eligible;
}

ExplorerGlueLayoutReadinessResult detail::evaluate_layout_readiness_preview(
    ExplorerGluePairInspection inspection) {
    ExplorerGlueLayoutReadinessResult result;
    result.reason = inspection.reason;
    result.stage = ExplorerGlueStage::PairValidation;
    result.diagnostic = std::move(inspection.diagnostic);
    result.leader_target_consent_prefix_valid =
        inspection.leader_target_consent_prefix_valid;
    result.follower_target_consent_prefix_valid =
        inspection.follower_target_consent_prefix_valid;
    result.follower_baseline_excluded_leader =
        inspection.follower_baseline_excluded_leader;
    result.pair_distinct = inspection.pair_distinct;
    result.same_monitor = inspection.same_monitor;
    result.same_dpi = inspection.same_dpi;
    result.glue_authority_bound = inspection.glue_authority_bound;
    result.glue_authority_consumed = inspection.glue_authority_consumed;
    result.native_apply_attempted = inspection.native_apply_attempted;
    result.temporary_peer_exception_retained =
        inspection.temporary_peer_exception_retained;
    result.leader_snapshot = std::move(inspection.leader_snapshot);
    result.follower_snapshot = std::move(inspection.follower_snapshot);
    if (result.leader_snapshot.has_value() &&
        result.follower_snapshot.has_value()) {
        result.same_monitor = same_pair_monitor(*result.leader_snapshot,
                                                *result.follower_snapshot);
        result.same_dpi = same_pair_dpi(*result.leader_snapshot,
                                        *result.follower_snapshot);
    }

    if (result.glue_authority_bound || result.glue_authority_consumed ||
        result.native_apply_attempted ||
        result.temporary_peer_exception_retained) {
        result.reason = ExplorerGlueReason::AuthorityConsumed;
        result.diagnostic = glue_diagnostic(
            1611U,
            "Explorer Glue layout readiness side effects",
            "preview inspection retained authority, native-operation, or peer state");
        return result;
    }

    if (result.reason != ExplorerGlueReason::Eligible) {
        return result;
    }
    if (!result.leader_snapshot.has_value() ||
        !result.follower_snapshot.has_value()) {
        result.reason = ExplorerGlueReason::TargetChanged;
        result.diagnostic = glue_diagnostic(
            1608U,
            "Explorer Glue layout readiness",
            "live pair inspection did not retain both snapshots");
        return result;
    }
    if (!result.same_monitor || !result.same_dpi) {
        result.reason = ExplorerGlueReason::MonitorOrDpiMismatch;
        return result;
    }

    result.stage = ExplorerGlueStage::Layout;
    result.readiness = measure_layout_readiness(*result.leader_snapshot,
                                                *result.follower_snapshot);
    if (!result.readiness.has_value()) {
        result.reason = ExplorerGlueReason::UnsafeLayout;
        result.diagnostic = glue_diagnostic(
            1609U,
            "Explorer Glue layout readiness",
            "live geometry cannot produce representable layout dimensions");
        return result;
    }

    result.layout = plan_explorer_glue_test_layout(*result.leader_snapshot,
                                                   *result.follower_snapshot);
    if (!result.layout.has_value()) {
        result.reason = ExplorerGlueReason::UnsafeLayout;
        result.diagnostic = glue_diagnostic(
            1610U,
            "Explorer Glue layout readiness",
            "current sizes fit neither horizontal nor vertical adjacency");
        return result;
    }

    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

detail::ExplorerGlueLeaderStartGeometry
detail::evaluate_leader_start_geometry(
    const ExplorerWindowSnapshot& armed_layout,
    const ExplorerWindowSnapshot& live_at_batch_drain) {
    if (!same_non_geometry_snapshot(armed_layout, live_at_batch_drain)) {
        return ExplorerGlueLeaderStartGeometry::TargetInvalidated;
    }
    const auto visible_change = core::movement::classify_geometry_change(
        armed_layout.visible_rect, live_at_batch_drain.visible_rect);
    const auto positioning_change = core::movement::classify_geometry_change(
        armed_layout.positioning_rect, live_at_batch_drain.positioning_rect);
    if (visible_change.kind ==
            core::movement::GeometryChangeKind::ResizeOrMixed ||
        positioning_change.kind ==
            core::movement::GeometryChangeKind::ResizeOrMixed) {
        return ExplorerGlueLeaderStartGeometry::ResizeOrMixed;
    }
    if (visible_change.kind == core::movement::GeometryChangeKind::Unchanged &&
        positioning_change.kind ==
            core::movement::GeometryChangeKind::Unchanged) {
        return ExplorerGlueLeaderStartGeometry::ExactLayout;
    }
    if (visible_change.kind ==
            core::movement::GeometryChangeKind::Translation &&
        positioning_change.kind ==
            core::movement::GeometryChangeKind::Translation &&
        visible_change.delta == positioning_change.delta) {
        return ExplorerGlueLeaderStartGeometry::TranslatedSinceReceipt;
    }
    return ExplorerGlueLeaderStartGeometry::TargetInvalidated;
}

std::optional<ExplorerGlueLayoutPlan> plan_explorer_glue_test_layout(
    const ExplorerWindowSnapshot& leader,
    const ExplorerWindowSnapshot& follower) noexcept {
    const auto readiness = measure_layout_readiness(leader, follower);
    if (!readiness.has_value() || !readiness->orientation.has_value()) {
        return std::nullopt;
    }

    const auto& leader_size = readiness->leader_visible_size;
    const auto& follower_size = readiness->follower_visible_size;
    if (*readiness->orientation ==
        ExplorerGlueLayoutOrientation::Horizontal) {
        const auto& fit = readiness->horizontal;
        if (!fit.required.has_value()) {
            return std::nullopt;
        }
        const auto x_offset =
            (fit.available.width - fit.required->width) / 2;
        const auto y_offset =
            (fit.available.height - fit.required->height) / 2;
        const auto x = add(leader.monitor_work_area.left(), x_offset);
        const auto y = add(leader.monitor_work_area.top(), y_offset);
        if (!x || !y) {
            return std::nullopt;
        }
        const auto leader_right = add(*x, leader_size.width);
        const auto follower_right =
            leader_right ? add(*leader_right, follower_size.width) : std::nullopt;
        const auto leader_bottom = add(*y, leader_size.height);
        const auto follower_bottom = add(*y, follower_size.height);
        if (leader_right && follower_right && leader_bottom && follower_bottom) {
            return ExplorerGlueLayoutPlan{
                ExplorerGlueLayoutOrientation::Horizontal,
                {*x, *y, *leader_right, *leader_bottom},
                {*leader_right, *y, *follower_right, *follower_bottom}};
        }
    }

    const auto& fit = readiness->vertical;
    if (!fit.fits || !fit.required.has_value()) {
        return std::nullopt;
    }
    const auto x_offset = (fit.available.width - fit.required->width) / 2;
    const auto y_offset = (fit.available.height - fit.required->height) / 2;
    const auto x = add(leader.monitor_work_area.left(), x_offset);
    const auto y = add(leader.monitor_work_area.top(), y_offset);
    if (!x || !y) {
        return std::nullopt;
    }
    const auto leader_right = add(*x, leader_size.width);
    const auto follower_right = add(*x, follower_size.width);
    const auto leader_bottom = add(*y, leader_size.height);
    const auto follower_bottom =
        leader_bottom ? add(*leader_bottom, follower_size.height) : std::nullopt;
    if (!leader_right || !follower_right || !leader_bottom ||
        !follower_bottom) {
        return std::nullopt;
    }
    return ExplorerGlueLayoutPlan{
        ExplorerGlueLayoutOrientation::Vertical,
        {*x, *y, *leader_right, *leader_bottom},
        {*x, *leader_bottom, *follower_right, *follower_bottom}};
}

struct ExplorerGlueConsent::Impl final {
    DWORD owner_thread_id{};
    std::unique_ptr<ExplorerTestSession> leader;
    std::unique_ptr<ExplorerTestSession> follower;
    ExplorerWindowSnapshot leader_preview;
    ExplorerWindowSnapshot follower_preview;
    ExplorerGlueLayoutPlan layout;
    ExplorerGlueFacts facts;
    std::uint64_t next_generation{1U};
    bool terminal{};

    [[nodiscard]] std::uint64_t issue_generation() noexcept {
        if (next_generation == 0U ||
            next_generation == std::numeric_limits<std::uint64_t>::max()) {
            return 0U;
        }
        return next_generation++;
    }
};

struct ExplorerGlueSession::Impl final {
    struct PendingNativeOperation final {
        std::uint64_t behavior_operation_generation{};
        std::uint64_t source_leader_sequence{};
        core::geometry::Rect expected_visible;
        core::geometry::Rect expected_positioning;
        std::uint64_t feedback_after_sequence{};
        bool native_apply_attempted{};
        bool exact_result{};
    };

    struct PendingRegistration final {
        Impl* owner{};
        PendingNativeOperation pending;
        std::optional<std::size_t> registered_index;
    };

    Impl(detail::ExplorerGlueAuthoritySeal seal_value,
         detail::ExplorerGlueNativeBinding leader_binding_value,
         detail::ExplorerGlueNativeBinding follower_binding_value) noexcept
        : seal(seal_value),
          leader_binding(std::move(leader_binding_value)),
          follower_binding(std::move(follower_binding_value)),
          coordinator(core::behavior::GlueMoveConfig{kPendingCapacity}) {
        pending_native.reserve(kPendingCapacity);
        operations.reserve(kNativeOperationCapacity);
        trace.reserve(kTraceCapacity);
        receipts.reserve(kTraceCapacity);
        quanta.reserve(kTraceCapacity);
    }

    DWORD owner_thread_id{};
    std::unique_ptr<ExplorerTestSession> leader;
    std::unique_ptr<ExplorerTestSession> follower;
    detail::ExplorerGlueAuthoritySeal seal;
    detail::ExplorerGlueNativeBinding leader_binding;
    detail::ExplorerGlueNativeBinding follower_binding;
    ExplorerGlueFacts facts;
    core::behavior::GlueMoveCoordinator coordinator;
    std::unique_ptr<ExplorerGlueEventSource> event_source;
    std::vector<PendingNativeOperation> pending_native;
    std::optional<core::geometry::Rect> last_acknowledged_follower_visible;
    std::vector<ExplorerGlueOperationRecord> operations;
    std::vector<ExplorerGlueTraceRecord> trace;
    std::vector<ExplorerGlueReceiptRecord> receipts;
    std::vector<ExplorerGlueQuantumRecord> quanta;
    std::uint64_t current_quantum_id{};
    std::uint64_t current_sample_generation{};
    std::uint64_t next_permit_generation{1U};
    std::uint64_t next_trace_sequence{1U};
    bool setup_complete{};
    bool armed{};
    bool terminal{};
    bool cleanup_complete{};
    bool leader_native_attempted{};
    bool follower_native_attempted{};
};

ExplorerGlueConsent::ExplorerGlueConsent(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

ExplorerGlueConsent::~ExplorerGlueConsent() noexcept {
    if (impl_ != nullptr && GetCurrentThreadId() != impl_->owner_thread_id) {
        // Keep COM observation/session state on its owning apartment rather
        // than invoking teardown from a foreign thread. Production never
        // transfers this aggregate.
        static_cast<void>(impl_.release());
    }
}

ExplorerGlueLayoutReadinessResult
ExplorerGlueConsent::preview_layout_readiness(ExplorerTestSession& leader,
                                              ExplorerTestSession& follower) {
    return detail::evaluate_layout_readiness_preview(
        detail::ExplorerGlueSessionBridge::inspect_pair(
            leader, follower, false));
}

ExplorerGlueBeginResult ExplorerGlueConsent::begin(
    std::unique_ptr<ExplorerTestSession> leader,
    std::unique_ptr<ExplorerTestSession> follower) {
    ExplorerGlueBeginResult result;
    if (leader == nullptr || follower == nullptr) {
        result.diagnostic = glue_diagnostic(
            1600U, "ExplorerGlueConsent::begin", "both target sessions are required");
        return result;
    }

    auto inspection = detail::ExplorerGlueSessionBridge::inspect_pair(
        *leader, *follower, false);
    result.facts.leader_target_consent_prefix_valid =
        inspection.leader_target_consent_prefix_valid;
    result.facts.follower_target_consent_prefix_valid =
        inspection.follower_target_consent_prefix_valid;
    result.facts.follower_baseline_excluded_leader =
        inspection.follower_baseline_excluded_leader;
    result.facts.pair_distinct = inspection.pair_distinct;
    result.facts.same_monitor_and_dpi = inspection.same_monitor_and_dpi;
    result.facts.pending_capacity = kPendingCapacity;
    result.facts.event_queue_capacity = kEventQueueCapacity;
    if (!inspection.succeeded()) {
        result.reason = inspection.reason;
        result.diagnostic = std::move(inspection.diagnostic);
        return result;
    }

    const auto layout = plan_explorer_glue_test_layout(
        *inspection.leader_snapshot, *inspection.follower_snapshot);
    if (!layout.has_value()) {
        result.reason = ExplorerGlueReason::UnsafeLayout;
        result.diagnostic = glue_diagnostic(
            1601U,
            "R1-C2B test fixture layout",
            "the two current window sizes cannot form zero-gap adjacency inside one work area");
        return result;
    }

    auto impl = std::make_unique<Impl>();
    impl->owner_thread_id = GetCurrentThreadId();
    impl->leader = std::move(leader);
    impl->follower = std::move(follower);
    impl->leader_preview = *inspection.leader_snapshot;
    impl->follower_preview = *inspection.follower_snapshot;
    impl->layout = *layout;
    impl->facts = result.facts;
    impl->facts.layout = *layout;
    impl->facts.leader_original = impl->leader_preview;
    impl->facts.follower_original = impl->follower_preview;
    impl->facts.test_layout_planned = true;
    impl->facts.consent_generations.pair_preview_generation =
        impl->issue_generation();
    if (impl->facts.consent_generations.pair_preview_generation == 0U) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        return result;
    }

    result.reason = ExplorerGlueReason::Eligible;
    result.facts = impl->facts;
    result.consent = std::unique_ptr<ExplorerGlueConsent>(
        new ExplorerGlueConsent(std::move(impl)));
    return result;
}

ExplorerGlueStepResult ExplorerGlueConsent::record_glue_prompt() {
    ExplorerGlueStepResult result;
    result.stage = ExplorerGlueStage::Consent;
    if (impl_ == nullptr || impl_->terminal ||
        GetCurrentThreadId() != impl_->owner_thread_id ||
        impl_->facts.consent_generations.pair_preview_generation == 0U ||
        impl_->facts.consent_generations.prompt_generation != 0U) {
        result.reason = ExplorerGlueReason::GlueConsentRequired;
        result.diagnostic = glue_diagnostic(
            1602U, "Explorer Glue prompt", "Glue consent prompt is out of order");
        return result;
    }
    impl_->facts.consent_generations.prompt_generation =
        impl_->issue_generation();
    if (impl_->facts.consent_generations.prompt_generation == 0U) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        return result;
    }
    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

ExplorerGlueAuthorizeResult ExplorerGlueConsent::confirm_user_glue() {
    ExplorerGlueAuthorizeResult result;
    if (impl_ == nullptr || impl_->terminal ||
        GetCurrentThreadId() != impl_->owner_thread_id ||
        impl_->facts.consent_generations.prompt_generation == 0U) {
        result.reason = ExplorerGlueReason::GlueConsentRequired;
        result.diagnostic = glue_diagnostic(
            1603U,
            "Explorer Glue confirmation",
            "real-console Glue confirmation was not recorded in order");
        return result;
    }
    impl_->terminal = true;
    impl_->facts.consent_generations.confirmation_generation =
        impl_->issue_generation();
    impl_->facts.consent_generations.authority_generation =
        impl_->issue_generation();
    const auto& generations = impl_->facts.consent_generations;
    if (generations.confirmation_generation == 0U ||
        generations.authority_generation == 0U ||
        !(generations.pair_preview_generation < generations.prompt_generation &&
          generations.prompt_generation < generations.confirmation_generation &&
          generations.confirmation_generation < generations.authority_generation)) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        result.facts = impl_->facts;
        return result;
    }
    const auto authority_id = allocate_glue_authority_id();
    if (authority_id == 0U) {
        result.reason = ExplorerGlueReason::ConsentGenerationMismatch;
        result.facts = impl_->facts;
        return result;
    }

    const detail::ExplorerGlueAuthoritySeal seal{
        authority_id, generations.authority_generation};
    auto bound = detail::ExplorerGlueSessionBridge::bind_pair(
        seal,
        *impl_->leader,
        *impl_->follower,
        impl_->leader_preview,
        impl_->follower_preview);
    if (!bound.succeeded()) {
        result.reason = bound.reason;
        result.diagnostic = std::move(bound.diagnostic);
        result.facts = impl_->facts;
        return result;
    }

    auto session_impl = std::make_unique<ExplorerGlueSession::Impl>(
        seal, *bound.leader, *bound.follower);
    session_impl->owner_thread_id = impl_->owner_thread_id;
    session_impl->leader = std::move(impl_->leader);
    session_impl->follower = std::move(impl_->follower);
    session_impl->facts = impl_->facts;
    session_impl->facts.glue_authority_id = authority_id;
    session_impl->facts.glue_consent_confirmed = true;
    session_impl->facts.leader_original = *bound.leader_snapshot;
    session_impl->facts.follower_original = *bound.follower_snapshot;

    result.reason = ExplorerGlueReason::Eligible;
    result.facts = session_impl->facts;
    result.session = std::unique_ptr<ExplorerGlueSession>(
        new ExplorerGlueSession(std::move(session_impl)));
    return result;
}

const ExplorerGlueFacts& ExplorerGlueConsent::facts() const noexcept {
    return impl_->facts;
}

ExplorerGlueSession::ExplorerGlueSession(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

ExplorerGlueSession::~ExplorerGlueSession() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (GetCurrentThreadId() != impl_->owner_thread_id) {
        // The COM targets and WinEvent hooks are owner-thread-affine. Preserve
        // the complete aggregate rather than freeing callback/COM state from a
        // foreign thread. The production harness never takes this misuse path.
        static_cast<void>(impl_.release());
        return;
    }
    if (!impl_->cleanup_complete) {
        try {
            static_cast<void>(finish_and_restore(
                ExplorerGlueReason::BehaviorAborted,
                ExplorerGlueStage::Restore));
        } catch (...) {
            // Destruction cannot safely unwind after a partial native cleanup.
            // Retain all owner-affine state for process/thread teardown.
            static_cast<void>(impl_.release());
        }
    }
}

bool ExplorerGlueSession::register_pending_before_native(
    void* const context) noexcept {
    auto* const registration =
        static_cast<Impl::PendingRegistration*>(context);
    if (registration == nullptr || registration->owner == nullptr) {
        return false;
    }
    auto& owner = *registration->owner;
    if (owner.event_source == nullptr ||
        owner.pending_native.size() >= kPendingCapacity) {
        return false;
    }
    if (owner.event_source->facts().poison !=
        ExplorerGlueEventSourcePoison::None) {
        return false;
    }
    try {
        registration->pending.feedback_after_sequence =
            owner.event_source->facts().latest_receipt_sequence;
        owner.pending_native.push_back(registration->pending);
        registration->registered_index = owner.pending_native.size() - 1U;
        return true;
    } catch (...) {
        return false;
    }
}

ExplorerOperationResult ExplorerGlueSession::execute_translation(
    const ExplorerGlueWindowRole role,
    const ExplorerGlueOperationPhase phase,
    const std::uint64_t behavior_operation_generation,
    const std::uint64_t source_leader_sequence,
    const ExplorerWindowSnapshot& expected_before,
    const core::geometry::Rect& target_visible) {
    ExplorerOperationResult failure;
    failure.reason = ExplorerEligibilityReason::TargetInvalidated;
    failure.stage = phase == ExplorerGlueOperationPhase::Restore
                        ? ExplorerOperationStage::Restore
                        : ExplorerOperationStage::Preflight;
    failure.cleanup_operation = phase == ExplorerGlueOperationPhase::Restore;
    if (impl_ == nullptr || GetCurrentThreadId() != impl_->owner_thread_id ||
        impl_->operations.size() >= kNativeOperationCapacity ||
        impl_->next_permit_generation == 0U ||
        impl_->next_permit_generation ==
            std::numeric_limits<std::uint64_t>::max()) {
        failure.reason = ExplorerEligibilityReason::OperationLimitReached;
        failure.diagnostic = glue_diagnostic(
            1604U,
            "Explorer Glue operation",
            "owner, operation capacity, or permit generation is invalid");
        return failure;
    }

    auto& target = role == ExplorerGlueWindowRole::Leader ? *impl_->leader
                                                           : *impl_->follower;
    const std::uint64_t permit_generation =
        impl_->next_permit_generation++;
    const std::uint64_t effective_session_generation =
        impl_->coordinator.session_generation() != 0U
            ? impl_->coordinator.session_generation()
            : impl_->facts.consent_generations.authority_generation;
    const detail::ExplorerGlueOperationPermit permit{
        impl_->seal.authority_id(),
        impl_->seal.authority_generation(),
        effective_session_generation,
        permit_generation,
        phase,
        role};
    auto prepared = detail::ExplorerGlueSessionBridge::prepare_translation(
        impl_->seal, permit, target, expected_before, target_visible);
    if (!prepared.succeeded()) {
        failure.reason =
            prepared.reason == ExplorerGlueReason::UnsafeLayout
                ? ExplorerEligibilityReason::UnsafeDelta
                : (prepared.reason == ExplorerGlueReason::WrongOwnerThread
                       ? ExplorerEligibilityReason::WrongThread
                       : ExplorerEligibilityReason::TargetInvalidated);
        failure.diagnostic = std::move(prepared.diagnostic);
        return failure;
    }

    Impl::PendingRegistration pending_registration;
    pending_registration.owner = impl_.get();
    detail::ExplorerGlueSessionBridge::BeforeNativeApply before_native_apply =
        nullptr;
    void* before_native_apply_context = nullptr;
    if (phase == ExplorerGlueOperationPhase::ActiveFollower) {
        if (role != ExplorerGlueWindowRole::Follower ||
            impl_->pending_native.size() >= kPendingCapacity) {
            failure.reason = ExplorerEligibilityReason::OperationLimitReached;
            failure.diagnostic = glue_diagnostic(
                1605U,
                "Explorer Glue pending receipt",
                "active follower receipt capacity is exhausted");
            return failure;
        }
        pending_registration.pending =
            {behavior_operation_generation,
             source_leader_sequence,
             prepared.prepared->requested_visible_,
             prepared.prepared->requested_positioning_,
             0U,
             false,
             false};
        before_native_apply = &ExplorerGlueSession::register_pending_before_native;
        before_native_apply_context = &pending_registration;
    }

    auto operation = detail::ExplorerGlueSessionBridge::apply_prepared(
        impl_->seal,
        permit,
        target,
        *prepared.prepared,
        before_native_apply,
        before_native_apply_context);
    if (pending_registration.registered_index.has_value()) {
        auto& pending =
            impl_->pending_native[*pending_registration.registered_index];
        pending.native_apply_attempted = operation.native_apply_attempted;
        pending.exact_result = operation.succeeded();
    }
    if (role == ExplorerGlueWindowRole::Leader &&
        operation.native_apply_attempted) {
        impl_->leader_native_attempted = true;
    }
    if (role == ExplorerGlueWindowRole::Follower &&
        operation.native_apply_attempted) {
        impl_->follower_native_attempted = true;
        if (phase == ExplorerGlueOperationPhase::ActiveFollower) {
            ++impl_->facts.follower_native_apply_count;
        }
    }
    impl_->operations.push_back(
        {phase,
         role,
         behavior_operation_generation,
         source_leader_sequence,
         operation,
         phase == ExplorerGlueOperationPhase::ActiveFollower
             ? impl_->current_quantum_id : 0U,
         phase == ExplorerGlueOperationPhase::ActiveFollower
             ? impl_->current_sample_generation : 0U,
         pending_registration.registered_index.has_value()
             ? impl_->pending_native[*pending_registration.registered_index]
                   .feedback_after_sequence : 0U,
         phase == ExplorerGlueOperationPhase::ActiveFollower &&
                 impl_->event_source != nullptr
             ? impl_->event_source->facts().latest_receipt_sequence : 0U});
    return operation;
}

ExplorerGlueStepResult ExplorerGlueSession::setup_test_layout_impl() {
    ExplorerGlueStepResult result;
    result.stage = ExplorerGlueStage::Layout;
    if (impl_ == nullptr || impl_->terminal || impl_->setup_complete ||
        GetCurrentThreadId() != impl_->owner_thread_id ||
        !impl_->facts.glue_consent_confirmed || !impl_->facts.layout.has_value() ||
        !impl_->facts.leader_original.has_value() ||
        !impl_->facts.follower_original.has_value()) {
        result.reason = ExplorerGlueReason::AuthorityConsumed;
        result.diagnostic = glue_diagnostic(
            1606U,
            "Explorer Glue setup",
            "setup is unavailable, repeated, or used from the wrong owner");
        return result;
    }

    auto leader_before = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Leader, *impl_->leader);
    auto follower_before = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
    if (!leader_before.succeeded() || !follower_before.succeeded() ||
        *leader_before.snapshot != *impl_->facts.leader_original ||
        *follower_before.snapshot != *impl_->facts.follower_original ||
        !same_pair_monitor_and_dpi(*leader_before.snapshot,
                                   *follower_before.snapshot)) {
        result.reason = ExplorerGlueReason::TargetChanged;
        result.diagnostic = glue_diagnostic(
            1607U,
            "Explorer Glue setup preflight",
            "target facts changed after Glue authorization");
        return finish_and_restore(result.reason, result.stage);
    }

    const auto& plan = *impl_->facts.layout;
    if (leader_before.snapshot->visible_rect != plan.leader_target_visible) {
        const auto leader_result = execute_translation(
            ExplorerGlueWindowRole::Leader,
            ExplorerGlueOperationPhase::Setup,
            0U,
            0U,
            *leader_before.snapshot,
            plan.leader_target_visible);
        if (!leader_result.succeeded()) {
            return finish_and_restore(ExplorerGlueReason::LayoutOperationFailed,
                                      result.stage);
        }
    }

    follower_before = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
    if (!follower_before.succeeded()) {
        return finish_and_restore(ExplorerGlueReason::TargetChanged,
                                  result.stage);
    }
    if (follower_before.snapshot->visible_rect != plan.follower_target_visible) {
        const auto follower_result = execute_translation(
            ExplorerGlueWindowRole::Follower,
            ExplorerGlueOperationPhase::Setup,
            0U,
            0U,
            *follower_before.snapshot,
            plan.follower_target_visible);
        if (!follower_result.succeeded()) {
            return finish_and_restore(ExplorerGlueReason::LayoutOperationFailed,
                                      result.stage);
        }
    }

    const auto leader_after = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Leader, *impl_->leader);
    const auto follower_after = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
    if (!leader_after.succeeded() || !follower_after.succeeded() ||
        leader_after.snapshot->visible_rect != plan.leader_target_visible ||
        follower_after.snapshot->visible_rect != plan.follower_target_visible ||
        !same_pair_monitor_and_dpi(*leader_after.snapshot,
                                   *follower_after.snapshot) ||
        !build_exact_pair_graph(*leader_after.snapshot,
                                *follower_after.snapshot)
             .has_value()) {
        return finish_and_restore(ExplorerGlueReason::TopologyInvalid,
                                  ExplorerGlueStage::Topology);
    }

    impl_->facts.leader_layout = *leader_after.snapshot;
    impl_->facts.follower_layout = *follower_after.snapshot;
    impl_->facts.test_layout_exact = true;
    impl_->facts.topology_exact_two_window_component = true;
    impl_->setup_complete = true;
    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

ExplorerGlueStepResult ExplorerGlueSession::arm_impl() {
    ExplorerGlueStepResult result;
    result.stage = ExplorerGlueStage::EventSource;
    if (impl_ == nullptr || impl_->terminal || !impl_->setup_complete ||
        impl_->armed || GetCurrentThreadId() != impl_->owner_thread_id ||
        !impl_->facts.leader_layout.has_value() ||
        !impl_->facts.follower_layout.has_value()) {
        result.reason = ExplorerGlueReason::AuthorityConsumed;
        return result;
    }

    const auto leader_live = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Leader, *impl_->leader);
    const auto follower_live = detail::ExplorerGlueSessionBridge::capture(
        impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
    if (!leader_live.succeeded() || !follower_live.succeeded() ||
        *leader_live.snapshot != *impl_->facts.leader_layout ||
        *follower_live.snapshot != *impl_->facts.follower_layout) {
        return finish_and_restore(ExplorerGlueReason::TargetChanged,
                                  result.stage);
    }
    const auto graph = build_exact_pair_graph(*leader_live.snapshot,
                                              *follower_live.snapshot);
    if (!graph.has_value()) {
        return finish_and_restore(ExplorerGlueReason::TopologyInvalid,
                                  ExplorerGlueStage::Topology);
    }
    const auto armed = impl_->coordinator.arm(
        *graph,
        core::model::WindowId{"leader"},
        core::model::WindowId{"follower"});
    if (armed.kind != core::behavior::GlueDecisionKind::Armed) {
        return finish_and_restore(ExplorerGlueReason::BehaviorAborted,
                                  ExplorerGlueStage::Topology);
    }
    impl_->facts.glue_session_generation =
        impl_->coordinator.session_generation();
    record_trace(nullptr, &armed);

    const ExplorerGlueEventSource::NativeTargetBinding leader_binding{
        reinterpret_cast<HWND>(impl_->leader_binding.native_key_),
        impl_->leader_binding.process_id_,
        impl_->leader_binding.thread_id_,
        1U,
        impl_->leader_binding.capability_generation_,
        ExplorerGlueWindowRole::Leader};
    const ExplorerGlueEventSource::NativeTargetBinding follower_binding{
        reinterpret_cast<HWND>(impl_->follower_binding.native_key_),
        impl_->follower_binding.process_id_,
        impl_->follower_binding.thread_id_,
        2U,
        impl_->follower_binding.capability_generation_,
        ExplorerGlueWindowRole::Follower};
    impl_->event_source = std::unique_ptr<ExplorerGlueEventSource>(
        new ExplorerGlueEventSource(leader_binding,
                                    follower_binding,
                                    kEventQueueCapacity,
                                    ExplorerGlueEventSource::DeliveryMode::Live,
                                    true));
    if (!impl_->event_source->start_live()) {
        return finish_and_restore(ExplorerGlueReason::EventSourceFailed,
                                  result.stage);
    }
    impl_->armed = true;
    impl_->facts.event_source_armed = true;
    result.reason = ExplorerGlueReason::Eligible;
    return result;
}

void ExplorerGlueSession::record_trace(
    const ExplorerGlueEvent* const event,
    const core::behavior::GlueDecision* const decision,
    const std::optional<core::geometry::Rect>& visible_rect,
    const std::uint64_t behavior_operation_generation) {
    if (impl_ == nullptr || impl_->trace.size() >= kTraceCapacity ||
        impl_->next_trace_sequence == 0U ||
        impl_->next_trace_sequence ==
            std::numeric_limits<std::uint64_t>::max()) {
        if (impl_ != nullptr && impl_->coordinator.session_generation() != 0U) {
            static_cast<void>(impl_->coordinator.report_queue_overflow(
                impl_->coordinator.session_generation()));
            impl_->terminal = true;
        }
        return;
    }
    ExplorerGlueTraceRecord record;
    record.trace_sequence = impl_->next_trace_sequence++;
    record.glue_session_generation = impl_->coordinator.session_generation();
    record.visible_rect = visible_rect;
    // Event inputs reference the quantum sample. Operation-result trace rows
    // retain actual receipt geometry in visible_rect, not a second sample.
    record.sampled_visible_rect = event != nullptr ? visible_rect : std::nullopt;
    record.processing_quantum_id = impl_->current_quantum_id;
    record.sampled_geometry_generation = impl_->current_sample_generation;
    record.behavior_operation_generation = behavior_operation_generation;
    if (event != nullptr) {
        record.event_sequence = event->receipt_sequence;
        record.native_event_timestamp_ms = event->native_event_time;
        record.role = event->role;
        if (event->kind != ExplorerGlueEventKind::TargetDestroyed) {
            record.event_kind = core_event_kind(event->kind);
        }
    }
    if (decision != nullptr) {
        record.decision_kind = decision->kind;
        record.abort_reason = decision->abort_reason;
        if (decision->command.has_value()) {
            record.behavior_operation_generation =
                decision->command->operation_generation;
            record.visible_rect = decision->command->target_visible_rect;
        }
    }
    impl_->trace.push_back(std::move(record));
}

ExplorerGlueStepResult ExplorerGlueSession::process_event(
    const ExplorerGlueEvent& event,
    const ExplorerWindowSnapshot& sampled_leader,
    const ExplorerWindowSnapshot& sampled_follower) {
    ExplorerGlueStepResult result;
    result.reason = ExplorerGlueReason::Eligible;
    result.stage = ExplorerGlueStage::ActiveSession;
    if (impl_ == nullptr || !impl_->armed || impl_->terminal) {
        result.reason = ExplorerGlueReason::AuthorityConsumed;
        return result;
    }

    const auto& expected_binding =
        event.role == ExplorerGlueWindowRole::Leader ? impl_->leader_binding
                                                      : impl_->follower_binding;
    const std::uint64_t expected_window_id =
        event.role == ExplorerGlueWindowRole::Leader ? 1U : 2U;
    if (event.window_id != expected_window_id ||
        event.capability_generation !=
            expected_binding.capability_generation_) {
        const auto aborted = impl_->coordinator.report_target_invalidated(
            impl_->coordinator.session_generation());
        record_trace(&event, &aborted);
        result.reason = ExplorerGlueReason::BehaviorAborted;
        return result;
    }
    if (event.kind == ExplorerGlueEventKind::TargetDestroyed) {
        const auto aborted = impl_->coordinator.report_target_invalidated(
            impl_->coordinator.session_generation());
        record_trace(&event, &aborted);
        result.reason = ExplorerGlueReason::BehaviorAborted;
        return result;
    }

    // The quantum already fully revalidated both targets. These are explicitly
    // processing-time samples; do not recapture once per historical receipt.
    ExplorerCaptureResult leader_live{
        ExplorerEligibilityReason::Eligible, ExplorerOperationStage::Preflight,
        sampled_leader, std::nullopt};
    ExplorerCaptureResult follower_live{
        ExplorerEligibilityReason::Eligible, ExplorerOperationStage::Preflight,
        sampled_follower, std::nullopt};
    if (!leader_live.succeeded() || !follower_live.succeeded() ||
        !same_pair_monitor_and_dpi(*leader_live.snapshot,
                                   *follower_live.snapshot)) {
        const auto aborted = impl_->coordinator.report_target_invalidated(
            impl_->coordinator.session_generation());
        record_trace(&event, &aborted);
        result.reason = ExplorerGlueReason::BehaviorAborted;
        return result;
    }

    if (event.kind == ExplorerGlueEventKind::MoveResizeStarted &&
        event.role == ExplorerGlueWindowRole::Leader) {
        if (!impl_->facts.leader_layout.has_value() ||
            !impl_->facts.follower_layout.has_value() ||
            *follower_live.snapshot != *impl_->facts.follower_layout ||
            !build_exact_pair_graph(*impl_->facts.leader_layout,
                                    *impl_->facts.follower_layout)
                 .has_value()) {
            const auto aborted = impl_->coordinator.report_target_invalidated(
                impl_->coordinator.session_generation());
            record_trace(&event, &aborted);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            return result;
        }
        const auto start_geometry = detail::evaluate_leader_start_geometry(
            *impl_->facts.leader_layout, *leader_live.snapshot);
        if (start_geometry ==
            detail::ExplorerGlueLeaderStartGeometry::ResizeOrMixed) {
            const auto aborted =
                impl_->coordinator.report_leader_resize_or_mixed(
                    impl_->coordinator.session_generation());
            record_trace(&event, &aborted, leader_live.snapshot->visible_rect);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            return result;
        }
        if (start_geometry ==
            detail::ExplorerGlueLeaderStartGeometry::TargetInvalidated) {
            const auto aborted = impl_->coordinator.report_target_invalidated(
                impl_->coordinator.session_generation());
            record_trace(&event, &aborted, leader_live.snapshot->visible_rect);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            return result;
        }
    }

    std::optional<core::geometry::Rect> event_geometry;
    if (event.kind == ExplorerGlueEventKind::GeometryChanged ||
        event.kind == ExplorerGlueEventKind::MoveResizeEnded) {
        event_geometry = event.role == ExplorerGlueWindowRole::Leader
                             ? sampled_leader.visible_rect
                             : sampled_follower.visible_rect;
    }
    if (event.role == ExplorerGlueWindowRole::Follower &&
        event.kind == ExplorerGlueEventKind::GeometryChanged) {
        const auto pending = std::find_if(
            impl_->pending_native.begin(),
            impl_->pending_native.end(),
            [&](const Impl::PendingNativeOperation& operation) {
                return operation.expected_visible == *event_geometry;
            });
        if (pending != impl_->pending_native.end() &&
            !detail::follower_feedback_after_registration(
                event.receipt_sequence,
                pending->feedback_after_sequence)) {
            const auto aborted =
                impl_->coordinator.report_unexpected_follower_feedback(
                    impl_->coordinator.session_generation());
            record_trace(&event, &aborted, event_geometry);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            return result;
        }
    }
    const core::behavior::GlueEventReceipt receipt{
        impl_->coordinator.session_generation(),
        event.receipt_sequence,
        core_role(event.role),
        core_event_kind(event.kind),
        event_geometry};
    auto decision = impl_->coordinator.on_event(receipt);
    record_trace(&event, &decision, event_geometry);
    if (impl_->terminal) {
        result.reason = ExplorerGlueReason::EventQueueOverflow;
        return result;
    }

    if (event.role == ExplorerGlueWindowRole::Follower &&
        event.kind == ExplorerGlueEventKind::GeometryChanged &&
        (decision.kind ==
             core::behavior::GlueDecisionKind::FeedbackAcknowledged ||
         decision.kind == core::behavior::GlueDecisionKind::
                               DuplicateFeedbackSuppressed)) {
        impl_->last_acknowledged_follower_visible = event_geometry;
        const auto found = std::find_if(
            impl_->pending_native.begin(),
            impl_->pending_native.end(),
            [&](const Impl::PendingNativeOperation& pending) {
                return pending.expected_visible == *event_geometry;
            });
        if (found != impl_->pending_native.end() &&
            decision.kind ==
                core::behavior::GlueDecisionKind::FeedbackAcknowledged) {
            impl_->pending_native.erase(found);
        }
    }

    if (decision.command.has_value()) {
        if (impl_->operations.size() >= kNativeOperationCapacity - 2U) {
            const auto aborted = impl_->coordinator.report_queue_overflow(
                impl_->coordinator.session_generation());
            record_trace(nullptr, &aborted);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            return result;
        }
        const auto operation = execute_translation(
            ExplorerGlueWindowRole::Follower,
            ExplorerGlueOperationPhase::ActiveFollower,
            decision.command->operation_generation,
            decision.command->source_leader_sequence,
            *follower_live.snapshot,
            decision.command->target_visible_rect);
        const std::optional<core::geometry::Rect> actual =
            operation.receipt.has_value() &&
                    operation.receipt->actual.has_value()
                ? std::optional<core::geometry::Rect>{
                      operation.receipt->actual->visible_rect}
                : std::nullopt;
        auto operation_decision = impl_->coordinator.on_operation_result(
            {decision.command->session_generation,
             decision.command->operation_generation,
             decision.command->follower_id,
             operation_outcome(operation),
             actual});
        record_trace(nullptr,
                     &operation_decision,
                     actual,
                     decision.command->operation_generation);
        if (operation_decision.kind ==
            core::behavior::GlueDecisionKind::FeedbackAcknowledged) {
            const auto found = std::find_if(
                impl_->pending_native.begin(),
                impl_->pending_native.end(),
                [&](const Impl::PendingNativeOperation& pending) {
                    return pending.behavior_operation_generation ==
                           decision.command->operation_generation;
                });
            if (found != impl_->pending_native.end()) {
                impl_->pending_native.erase(found);
            }
        }
        decision = std::move(operation_decision);
    }

    if (decision.kind == core::behavior::GlueDecisionKind::Completing) {
        leader_live = detail::ExplorerGlueSessionBridge::capture(
            impl_->seal, ExplorerGlueWindowRole::Leader, *impl_->leader);
        follower_live = detail::ExplorerGlueSessionBridge::capture(
            impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
        if (!leader_live.succeeded() || !follower_live.succeeded()) {
            decision = impl_->coordinator.report_target_invalidated(
                impl_->coordinator.session_generation());
            record_trace(nullptr, &decision);
        } else {
            impl_->facts.leader_final = *leader_live.snapshot;
            impl_->facts.follower_final = *follower_live.snapshot;
            decision = impl_->coordinator.complete(
                {impl_->coordinator.session_generation(),
                 leader_live.snapshot->visible_rect,
                 follower_live.snapshot->visible_rect});
            record_trace(nullptr, &decision);
            if (decision.kind ==
                core::behavior::GlueDecisionKind::Completed) {
                impl_->pending_native.clear();
            }
        }
    }

    const auto& stats = impl_->coordinator.stats();
    impl_->facts.behavior_state = impl_->coordinator.state();
    impl_->facts.behavior_abort_reason = impl_->coordinator.abort_reason();
    impl_->facts.leader_start_count = stats.leader_start_count;
    impl_->facts.leader_location_count = stats.leader_location_count;
    impl_->facts.leader_end_count = stats.leader_end_count;
    impl_->facts.follower_noop_count = stats.follower_noop_count;
    impl_->facts.follower_feedback_count = stats.follower_feedback_count;
    impl_->facts.max_pending_depth = stats.max_pending_depth;
    impl_->facts.suppressed_feedback_count = stats.suppressed_feedback_count;
    impl_->facts.duplicate_feedback_count = stats.duplicate_feedback_count;
    impl_->facts.missing_feedback_count = stats.missing_feedback_count;
    impl_->facts.reconciled_feedback_count = stats.reconciled_feedback_count;
    impl_->facts.unexpected_feedback_count = stats.unexpected_feedback_count;
    if (impl_->coordinator.state() == core::behavior::GlueMoveState::Aborted) {
        result.reason = ExplorerGlueReason::BehaviorAborted;
    }
    return result;
}

ExplorerGlueStepResult ExplorerGlueSession::drain_event_source() {
    ExplorerGlueStepResult result;
    result.reason = ExplorerGlueReason::Eligible;
    result.stage = ExplorerGlueStage::EventSource;
    if (impl_ == nullptr || impl_->event_source == nullptr) {
        result.reason = ExplorerGlueReason::EventSourceFailed;
        return result;
    }

    // One current queue batch per processing quantum. Newly delivered receipts
    // belong to the next quantum, giving the owner loop an explicit boundary.
    {
        auto drained = impl_->event_source->drain_owner_queue();
        impl_->facts.max_event_queue_depth =
            std::max(impl_->facts.max_event_queue_depth,
                     drained.facts.max_queue_depth);
        impl_->facts.accepted_event_count = drained.facts.accepted_count;
        impl_->facts.ignored_event_count =
            drained.facts.ignored_event_count +
            drained.facts.ignored_object_child_count +
            drained.facts.ignored_other_window_count;
        if (drained.facts.poison != ExplorerGlueEventSourcePoison::None) {
            if (drained.facts.poison ==
                ExplorerGlueEventSourcePoison::QueueOverflow) {
                static_cast<void>(impl_->coordinator.report_queue_overflow(
                    impl_->coordinator.session_generation()));
                result.reason = ExplorerGlueReason::EventQueueOverflow;
            } else if (drained.facts.poison ==
                           ExplorerGlueEventSourcePoison::
                               TargetIdentityMismatch ||
                       drained.facts.poison ==
                           ExplorerGlueEventSourcePoison::TargetNotRoot) {
                static_cast<void>(impl_->coordinator.report_target_invalidated(
                    impl_->coordinator.session_generation()));
                result.reason = ExplorerGlueReason::BehaviorAborted;
            } else {
                static_cast<void>(
                    impl_->coordinator.report_event_source_failure(
                        impl_->coordinator.session_generation()));
                result.reason = ExplorerGlueReason::EventSourceFailed;
            }
            impl_->facts.behavior_state = impl_->coordinator.state();
            impl_->facts.behavior_abort_reason =
                impl_->coordinator.abort_reason();
            return result;
        }

        if (drained.events.empty()) {
            return result;
        }

        if (impl_->receipts.size() + drained.events.size() > kTraceCapacity ||
            impl_->quanta.size() >= kTraceCapacity) {
            static_cast<void>(impl_->coordinator.report_queue_overflow(
                impl_->coordinator.session_generation()));
            result.reason = ExplorerGlueReason::EventQueueOverflow;
            return result;
        }
        const auto end = std::find_if(drained.events.begin(), drained.events.end(),
            [](const ExplorerGlueEvent& event) {
                return event.role == ExplorerGlueWindowRole::Leader &&
                       event.kind == ExplorerGlueEventKind::MoveResizeEnded;
            });
        const auto active_count = end == drained.events.end()
            ? drained.events.size()
            : static_cast<std::size_t>(end - drained.events.begin()) + 1U;
        const auto selected = detail::select_glue_quantum_events(
            std::span{drained.events}.first(active_count));
        ExplorerGlueQuantumRecord quantum;
        quantum.processing_quantum_id = ++impl_->current_quantum_id;
        quantum.first_receipt_sequence = drained.events.front().receipt_sequence;
        quantum.last_receipt_sequence = drained.events.back().receipt_sequence;
        quantum.receipt_count = drained.events.size();
        for (std::size_t index = 0; index < drained.events.size(); ++index) {
            const auto& event = drained.events[index];
            const bool discarded = index >= active_count;
            const bool location = event.kind == ExplorerGlueEventKind::GeometryChanged;
            if (event.role == ExplorerGlueWindowRole::Leader) {
                quantum.leader_location_count += location && !discarded ? 1U : 0U;
                quantum.contains_leader_end = quantum.contains_leader_end ||
                    event.kind == ExplorerGlueEventKind::MoveResizeEnded;
                if (location && !discarded && selected[index]) {
                    quantum.selected_leader_sequence = event.receipt_sequence;
                }
            } else {
                quantum.follower_location_count += location && !discarded ? 1U : 0U;
            }
            impl_->receipts.push_back({event.receipt_sequence,
                quantum.processing_quantum_id, event.role,
                event.kind == ExplorerGlueEventKind::TargetDestroyed
                    ? std::nullopt
                    : std::optional{core_event_kind(event.kind)},
                event.native_event_time,
                !discarded && !selected[index], discarded});
        }
        if (quantum.leader_location_count != 0U) {
            quantum.sampled_geometry_generation = quantum.processing_quantum_id;
        }
        impl_->current_sample_generation = quantum.sampled_geometry_generation;
        impl_->quanta.push_back(quantum);

        // Do not query a destroyed HWND. A destroy receipt is already bound to
        // the exact target by the event source and terminates the batch before
        // any earlier receipt can cause another native operation.
        const auto destroyed = std::find_if(
            drained.events.begin(),
            drained.events.end(),
            [](const ExplorerGlueEvent& event) {
                return event.kind == ExplorerGlueEventKind::TargetDestroyed;
            });
        if (destroyed != drained.events.end()) {
            const auto aborted = impl_->coordinator.report_target_invalidated(
                impl_->coordinator.session_generation());
            record_trace(&*destroyed, &aborted);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            result.stage = ExplorerGlueStage::ActiveSession;
            return result;
        }

        // Exactly one live pair sample at this processing quantum. Coalesced
        // Leader receipts keep source metadata only; they never enter Core as
        // multiple copies of this geometry. Follower attribution still uses the
        // pre-apply sample and the existing pending watermark protections.
        const auto batch_leader = detail::ExplorerGlueSessionBridge::capture(
            impl_->seal, ExplorerGlueWindowRole::Leader, *impl_->leader);
        const auto batch_follower = detail::ExplorerGlueSessionBridge::capture(
            impl_->seal, ExplorerGlueWindowRole::Follower, *impl_->follower);
        if (!batch_leader.succeeded() || !batch_follower.succeeded() ||
            !same_pair_monitor_and_dpi(*batch_leader.snapshot,
                                       *batch_follower.snapshot)) {
            const auto aborted = impl_->coordinator.report_target_invalidated(
                impl_->coordinator.session_generation());
            record_trace(nullptr, &aborted);
            result.reason = ExplorerGlueReason::BehaviorAborted;
            result.stage = ExplorerGlueStage::ActiveSession;
            return result;
        }
        impl_->quanta.back().leader_visible_rect = batch_leader.snapshot->visible_rect;
        impl_->quanta.back().follower_visible_rect = batch_follower.snapshot->visible_rect;

        for (std::size_t index = 0U; index < active_count; ++index) {
            const auto& event = drained.events[index];
            if (!selected[index]) {
                continue;
            }
            if (event.role == ExplorerGlueWindowRole::Leader &&
                event.kind == ExplorerGlueEventKind::GeometryChanged) {
                for (auto later = drained.events.begin() +
                                  static_cast<std::ptrdiff_t>(index + 1U);
                     later != drained.events.begin() +
                                  static_cast<std::ptrdiff_t>(active_count); ++later) {
                    if (later->role != ExplorerGlueWindowRole::Follower) {
                        continue;
                    }
                    if (later->kind !=
                        ExplorerGlueEventKind::GeometryChanged) {
                        const core::behavior::GlueEventReceipt receipt{
                            impl_->coordinator.session_generation(),
                            later->receipt_sequence,
                            core::behavior::GlueWindowRole::Follower,
                            core_event_kind(later->kind),
                            std::nullopt};
                        const auto aborted =
                            impl_->coordinator.on_event(receipt);
                        record_trace(&*later, &aborted);
                        result.reason = ExplorerGlueReason::BehaviorAborted;
                        result.stage = ExplorerGlueStage::ActiveSession;
                        return result;
                    }

                    const auto prior = std::find_if(
                        impl_->pending_native.begin(),
                        impl_->pending_native.end(),
                        [&](const Impl::PendingNativeOperation& pending) {
                            return pending.expected_visible ==
                                   batch_follower.snapshot->visible_rect;
                        });
                    const bool has_prior =
                        prior != impl_->pending_native.end();
                    const bool duplicate =
                        impl_->last_acknowledged_follower_visible.has_value() &&
                        *impl_->last_acknowledged_follower_visible ==
                            batch_follower.snapshot->visible_rect;
                    const bool attributable =
                        detail::follower_receipt_precedes_new_native_apply(
                            later->receipt_sequence,
                            has_prior,
                            has_prior ? prior->feedback_after_sequence : 0U,
                            has_prior && prior->native_apply_attempted,
                            has_prior && prior->exact_result,
                            duplicate);
                    if (!attributable) {
                        const auto aborted = impl_->coordinator
                                                 .report_unexpected_follower_feedback(
                                                     impl_->coordinator
                                                         .session_generation());
                        record_trace(&*later,
                                     &aborted,
                                     batch_follower.snapshot->visible_rect);
                        result.reason = ExplorerGlueReason::BehaviorAborted;
                        result.stage = ExplorerGlueStage::ActiveSession;
                        return result;
                    }
                }
            }
            const auto handled = process_event(event, *batch_leader.snapshot,
                                               *batch_follower.snapshot);
            if (!handled.succeeded()) {
                return handled;
            }
            if (impl_->coordinator.state() ==
                    core::behavior::GlueMoveState::Completed ||
                impl_->coordinator.state() ==
                    core::behavior::GlueMoveState::Aborted) {
                return result;
            }
        }
    }
    return result;
}

ExplorerGlueStepResult ExplorerGlueSession::finish_and_restore(
    const ExplorerGlueReason terminal_reason,
    const ExplorerGlueStage terminal_stage) {
    ExplorerGlueStepResult result;
    result.reason = terminal_reason;
    result.stage = terminal_stage;
    if (impl_ == nullptr) {
        return result;
    }
    if (impl_->cleanup_complete) {
        return result;
    }
    if (GetCurrentThreadId() != impl_->owner_thread_id) {
        result.reason = ExplorerGlueReason::WrongOwnerThread;
        return result;
    }
    impl_->terminal = true;

    bool event_lifecycle_clean = true;
    if (impl_->event_source != nullptr) {
        event_lifecycle_clean = impl_->event_source->stop_live();
        try {
            const auto discarded = impl_->event_source->drain_owner_queue();
            // Postverify may have delivered additional callbacks after the
            // final active quantum drained. Preserve that tail in its own
            // inactive quantum rather than losing receipt/watermark evidence.
            if (!discarded.events.empty()) {
                if (impl_->quanta.size() >= kTraceCapacity ||
                    impl_->receipts.size() + discarded.events.size() >
                        kTraceCapacity) {
                    event_lifecycle_clean = false;
                } else {
                    ExplorerGlueQuantumRecord tail;
                    tail.processing_quantum_id = ++impl_->current_quantum_id;
                    tail.first_receipt_sequence =
                        discarded.events.front().receipt_sequence;
                    tail.last_receipt_sequence =
                        discarded.events.back().receipt_sequence;
                    tail.receipt_count = discarded.events.size();
                    tail.inactive_discard = true;
                    for (const auto& event : discarded.events) {
                        impl_->receipts.push_back({event.receipt_sequence,
                            tail.processing_quantum_id, event.role,
                            event.kind == ExplorerGlueEventKind::TargetDestroyed
                                ? std::nullopt
                                : std::optional{core_event_kind(event.kind)},
                            event.native_event_time, false, true});
                    }
                    impl_->quanta.push_back(tail);
                }
            }
            impl_->facts.max_event_queue_depth =
                std::max(impl_->facts.max_event_queue_depth,
                         discarded.facts.max_queue_depth);
            impl_->facts.accepted_event_count =
                discarded.facts.accepted_count;
            impl_->facts.ignored_event_count =
                discarded.facts.ignored_event_count +
                discarded.facts.ignored_object_child_count +
                discarded.facts.ignored_other_window_count;
        } catch (...) {
            event_lifecycle_clean = false;
        }
        impl_->facts.event_source_stopped = true;
        impl_->facts.event_source_lifecycle_clean = event_lifecycle_clean;
    } else {
        impl_->facts.event_source_stopped = true;
        impl_->facts.event_source_lifecycle_clean = true;
    }

    const auto restore_role = [&](const ExplorerGlueWindowRole role,
                                  const ExplorerWindowSnapshot& original,
                                  bool& attempted,
                                  bool& exact,
                                  std::optional<ExplorerWindowSnapshot>& restored) {
        try {
            auto& target = role == ExplorerGlueWindowRole::Leader
                               ? *impl_->leader
                               : *impl_->follower;
            const auto current = detail::ExplorerGlueSessionBridge::capture(
                impl_->seal, role, target);
            if (!current.succeeded()) {
                return;
            }
            if (current.snapshot->visible_rect == original.visible_rect &&
                current.snapshot->positioning_rect ==
                    original.positioning_rect) {
                exact = true;
                restored = *current.snapshot;
                return;
            }
            const auto operation = execute_translation(
                role,
                ExplorerGlueOperationPhase::Restore,
                0U,
                0U,
                *current.snapshot,
                original.visible_rect);
            attempted = operation.native_apply_attempted;
            exact = operation.succeeded() && operation.receipt.has_value() &&
                    operation.receipt->actual.has_value() &&
                    operation.receipt->actual->visible_rect ==
                        original.visible_rect &&
                    operation.receipt->actual->positioning_rect ==
                        original.positioning_rect;
            if (operation.receipt.has_value() &&
                operation.receipt->actual.has_value()) {
                restored = *operation.receipt->actual;
            }
        } catch (...) {
            exact = false;
        }
    };

    if (impl_->facts.follower_original.has_value()) {
        restore_role(ExplorerGlueWindowRole::Follower,
                     *impl_->facts.follower_original,
                     impl_->facts.follower_restore_attempted,
                     impl_->facts.follower_restored_exact,
                     impl_->facts.follower_restored);
    }
    if (impl_->facts.leader_original.has_value()) {
        restore_role(ExplorerGlueWindowRole::Leader,
                     *impl_->facts.leader_original,
                     impl_->facts.leader_restore_attempted,
                     impl_->facts.leader_restored_exact,
                     impl_->facts.leader_restored);
    }

    impl_->pending_native.clear();
    detail::ExplorerGlueSessionBridge::release_pair(
        impl_->seal, *impl_->leader, *impl_->follower);
    impl_->cleanup_complete = true;
    const auto& stats = impl_->coordinator.stats();
    impl_->facts.behavior_state = impl_->coordinator.state();
    impl_->facts.behavior_abort_reason = impl_->coordinator.abort_reason();
    impl_->facts.leader_start_count = stats.leader_start_count;
    impl_->facts.leader_location_count = stats.leader_location_count;
    impl_->facts.leader_end_count = stats.leader_end_count;
    impl_->facts.follower_noop_count = stats.follower_noop_count;
    impl_->facts.follower_feedback_count = stats.follower_feedback_count;
    impl_->facts.max_pending_depth = stats.max_pending_depth;
    impl_->facts.suppressed_feedback_count = stats.suppressed_feedback_count;
    impl_->facts.duplicate_feedback_count = stats.duplicate_feedback_count;
    impl_->facts.missing_feedback_count = stats.missing_feedback_count;
    impl_->facts.reconciled_feedback_count = stats.reconciled_feedback_count;
    impl_->facts.unexpected_feedback_count = stats.unexpected_feedback_count;
    impl_->facts.user_windows_close_attempted = false;
    if (!event_lifecycle_clean) {
        result.reason = ExplorerGlueReason::CleanupLifecycleFailed;
        result.stage = ExplorerGlueStage::EventSource;
    } else if (!impl_->facts.leader_restored_exact ||
               !impl_->facts.follower_restored_exact) {
        result.reason = ExplorerGlueReason::RestoreFailed;
        result.stage = ExplorerGlueStage::Restore;
    }
    return result;
}

ExplorerGlueStepResult ExplorerGlueSession::run_until_terminal_impl(
    const std::chrono::milliseconds timeout) {
    ExplorerGlueStepResult result;
    result.stage = ExplorerGlueStage::ActiveSession;
    if (impl_ == nullptr || !impl_->armed || impl_->terminal ||
        GetCurrentThreadId() != impl_->owner_thread_id ||
        timeout <= std::chrono::milliseconds::zero() ||
        timeout > std::chrono::hours{24}) {
        result.reason = ExplorerGlueReason::InvalidArgument;
        return result;
    }

    UniqueKernelHandle timer{CreateWaitableTimerW(nullptr, TRUE, nullptr)};
    if (timer.get() == nullptr) {
        return finish_and_restore(ExplorerGlueReason::EventSourceFailed,
                                  result.stage);
    }
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(timeout.count()) * 10'000LL;
    if (!SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE)) {
        return finish_and_restore(ExplorerGlueReason::EventSourceFailed,
                                  result.stage);
    }

    ExplorerGlueReason terminal_reason = ExplorerGlueReason::Eligible;
    ExplorerGlueStage terminal_stage = ExplorerGlueStage::Completion;
    const HANDLE wait_handle = timer.get();
    for (;;) {
        if (WaitForSingleObject(wait_handle, 0U) == WAIT_OBJECT_0) {
            static_cast<void>(impl_->coordinator.report_timeout(
                impl_->coordinator.session_generation()));
            terminal_reason = ExplorerGlueReason::TimedOut;
            terminal_stage = ExplorerGlueStage::ActiveSession;
            break;
        }
        const auto drained = drain_event_source();
        if (!drained.succeeded()) {
            terminal_reason = drained.reason;
            terminal_stage = drained.stage;
            break;
        }
        if (impl_->coordinator.state() ==
            core::behavior::GlueMoveState::Completed) {
            terminal_reason = ExplorerGlueReason::Eligible;
            break;
        }
        if (impl_->coordinator.state() ==
            core::behavior::GlueMoveState::Aborted) {
            terminal_reason = ExplorerGlueReason::BehaviorAborted;
            terminal_stage = ExplorerGlueStage::ActiveSession;
            break;
        }

        // COM/native validation may reenter and deliver receipts after the
        // quantum's initial drain. Consume those at a new sample boundary
        // without waiting for another edge-triggered notification.
        if (impl_->event_source->facts().queue_depth != 0U) {
            continue;
        }

        const DWORD wait = MsgWaitForMultipleObjectsEx(
            1U,
            &wait_handle,
            INFINITE,
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0) {
            static_cast<void>(impl_->coordinator.report_timeout(
                impl_->coordinator.session_generation()));
            terminal_reason = ExplorerGlueReason::TimedOut;
            terminal_stage = ExplorerGlueStage::ActiveSession;
            break;
        }
        if (wait == WAIT_FAILED || wait != WAIT_OBJECT_0 + 1U) {
            static_cast<void>(impl_->coordinator.report_event_source_failure(
                impl_->coordinator.session_generation()));
            terminal_reason = ExplorerGlueReason::EventSourceFailed;
            terminal_stage = ExplorerGlueStage::EventSource;
            break;
        }

        bool quit_seen = false;
        detail::pump_glue_message_quantum(
            [&]() {
                MSG message{};
                const bool retrieved =
                    PeekMessageW(&message, nullptr, 0U, 0U, PM_REMOVE) != FALSE;
                if (!retrieved) {
                    return false; // PeekMessage may still have delivered callbacks.
                }
                if (message.message == WM_QUIT) {
                    quit_seen = true;
                    return false;
                }
                if (!impl_->event_source->owns_notification(message.message,
                                                            message.wParam)) {
                    static_cast<void>(TranslateMessage(&message));
                    static_cast<void>(DispatchMessageW(&message));
                }
                return true;
            },
            [&]() {
                const auto source = impl_->event_source->facts();
                return source.queue_depth != 0U ||
                       source.poison != ExplorerGlueEventSourcePoison::None;
            });
        if (quit_seen) {
            static_cast<void>(impl_->coordinator.report_event_source_failure(
                impl_->coordinator.session_generation()));
            terminal_reason = ExplorerGlueReason::EventSourceFailed;
            terminal_stage = ExplorerGlueStage::EventSource;
            break;
        }
    }

    static_cast<void>(CancelWaitableTimer(timer.get()));
    return finish_and_restore(terminal_reason, terminal_stage);
}

ExplorerGlueStepResult ExplorerGlueSession::setup_test_layout() {
    try {
        return setup_test_layout_impl();
    } catch (...) {
        try {
            return finish_and_restore(ExplorerGlueReason::LayoutOperationFailed,
                                      ExplorerGlueStage::Layout);
        } catch (...) {
            static_cast<void>(impl_.release());
            return {ExplorerGlueReason::CleanupLifecycleFailed,
                    ExplorerGlueStage::Restore,
                    std::nullopt};
        }
    }
}

ExplorerGlueStepResult ExplorerGlueSession::arm() {
    try {
        return arm_impl();
    } catch (...) {
        try {
            return finish_and_restore(ExplorerGlueReason::EventSourceFailed,
                                      ExplorerGlueStage::EventSource);
        } catch (...) {
            static_cast<void>(impl_.release());
            return {ExplorerGlueReason::CleanupLifecycleFailed,
                    ExplorerGlueStage::Restore,
                    std::nullopt};
        }
    }
}

ExplorerGlueStepResult ExplorerGlueSession::run_until_terminal(
    const std::chrono::milliseconds timeout) {
    try {
        return run_until_terminal_impl(timeout);
    } catch (...) {
        try {
            return finish_and_restore(ExplorerGlueReason::BehaviorAborted,
                                      ExplorerGlueStage::ActiveSession);
        } catch (...) {
            static_cast<void>(impl_.release());
            return {ExplorerGlueReason::CleanupLifecycleFailed,
                    ExplorerGlueStage::Restore,
                    std::nullopt};
        }
    }
}

ExplorerGlueStepResult ExplorerGlueSession::cancel_and_restore() {
    try {
        return finish_and_restore(ExplorerGlueReason::BehaviorAborted,
                                  ExplorerGlueStage::Restore);
    } catch (...) {
        static_cast<void>(impl_.release());
        return {ExplorerGlueReason::CleanupLifecycleFailed,
                ExplorerGlueStage::Restore,
                std::nullopt};
    }
}

const ExplorerGlueFacts& ExplorerGlueSession::facts() const noexcept {
    static const ExplorerGlueFacts unavailable;
    return impl_ != nullptr ? impl_->facts : unavailable;
}

const std::vector<ExplorerGlueOperationRecord>&
ExplorerGlueSession::operations() const noexcept {
    static const std::vector<ExplorerGlueOperationRecord> unavailable;
    return impl_ != nullptr ? impl_->operations : unavailable;
}

const std::vector<ExplorerGlueTraceRecord>&
ExplorerGlueSession::trace() const noexcept {
    static const std::vector<ExplorerGlueTraceRecord> unavailable;
    return impl_ != nullptr ? impl_->trace : unavailable;
}

const std::vector<ExplorerGlueReceiptRecord>&
ExplorerGlueSession::receipts() const noexcept {
    static const std::vector<ExplorerGlueReceiptRecord> unavailable;
    return impl_ != nullptr ? impl_->receipts : unavailable;
}

const std::vector<ExplorerGlueQuantumRecord>&
ExplorerGlueSession::quanta() const noexcept {
    static const std::vector<ExplorerGlueQuantumRecord> unavailable;
    return impl_ != nullptr ? impl_->quanta : unavailable;
}

detail::ExplorerGlueDiagnostics detail::ExplorerGlueSessionDiagnostics::read(
    const ExplorerGlueSession& session) noexcept {
    ExplorerGlueDiagnostics result;
    if (session.impl_ == nullptr) {
        return result;
    }
    const auto make = [](const ExplorerGlueNativeBinding& binding) {
        return ExplorerGlueDiagnosticIdentity{binding.native_key_,
                                              binding.process_id_,
                                              binding.thread_id_,
                                              binding.capability_generation_,
                                              binding.role_};
    };
    result.leader = make(session.impl_->leader_binding);
    result.follower = make(session.impl_->follower_binding);
    return result;
}

} // namespace panebind::platform::windows::explorer
