#pragma once

#include "platform/windows/explorer/explorer_group_internal.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"
#include <limits>

namespace panebind::platform::windows::explorer {
struct GroupLayoutReadiness final {
    bool ready{};
    struct Pair {
        std::size_t first{},second{};
        bool relation{};
        core::geometry::Edge first_edge{},second_edge{};
        std::int64_t signed_gap{},orthogonal_overlap{};
    };
    std::array<Pair,3> pairs{};
    std::size_t relation_count{};
    std::array<std::vector<std::size_t>,3> components;
    std::string_view reason{"invalid_geometry"};
};
[[nodiscard]] GroupLayoutReadiness group_layout_readiness(const detail::GroupSnapshots&) noexcept;

struct GroupReadinessActivity final {
    bool owner_thread{};
    std::uint64_t group_generation{},gesture_generation{};
    std::size_t native_apply_count{},hdwp_begin_count{},pending_count{};
    bool event_source_running{},leader_present{};
};
struct GroupReadinessPreview final {
    std::uint64_t attempt{};
    bool valid{},setup_check{};
    detail::GroupCaptureResult capture_result;
    std::optional<detail::GroupSnapshots> snapshots;
    GroupLayoutReadiness readiness;
    GroupReadinessActivity activity;
    std::array<std::array<std::int64_t,2>,3> member_sizes{};
    core::geometry::Rect work_area;
    std::int64_t capture_qpc{};
    std::string_view reason{"fixture_state_invalid"};
};

// Fixture bookkeeping only: no capability issuance, roles, native operations,
// hooks or input. The runtime supplies the existing complete read-only capture;
// deterministic tests supply fresh snapshots through the same capture boundary.
class GroupReadinessFixture final {
public:
    GroupReadinessFixture(std::uint64_t generation,detail::GroupSnapshots binding)
        :generation_(generation),binding_(std::move(binding)) {}

    template<class Capture>
    GroupReadinessPreview preview(const GroupReadinessActivity& activity,
                                  Capture&& capture,bool setup_check=false) {
        GroupReadinessPreview result;
        result.capture_result.failure_stage=detail::GroupCaptureStage::Context;
        result.capture_result.reason="fixture_state_invalid";
        result.activity=activity;
        result.setup_check=setup_check;
        if(!activity.owner_thread || failed_ || accepted_ || !generation_ ||
           activity.group_generation!=generation_ || activity.gesture_generation ||
           activity.native_apply_count || activity.hdwp_begin_count || activity.pending_count ||
           activity.event_source_running || activity.leader_present ||
           attempts_==std::numeric_limits<std::uint64_t>::max()) return result;
        result.attempt=++attempts_;
        result.capture_result=capture(); // ALWAYS fresh, never binding_ or last_.
        result.snapshots=result.capture_result.snapshots;
        result.capture_qpc=glue_qpc_now();
        if(!result.capture_result) {
            result.reason="member_validation_failed";
            failed_=!result.capture_result.recoverable();last_=result;return result;
        }
        for(std::size_t i=0;i<3;++i) {
            auto context=(*result.snapshots)[i];
            auto bound=binding_[i];
            context.visible_rect=bound.visible_rect={};
            context.positioning_rect=bound.positioning_rect={};
            if(context!=bound) {
                result.reason="readiness_member_context_changed";
                detail::GroupMemberCaptureResult failure;
                failure.reason=ExplorerEligibilityReason::TargetInvalidated;
                failure.invalidation="immutable_context_changed";
                detail::group_capture_failure(result.capture_result,i,detail::GroupCaptureStage::Context,
                    failure,"immutable_context_changed");
                failed_=true;last_=result;return result;
            }
        }
        std::size_t geometry_member{};
        try {
            for(std::size_t i=0;i<3;++i) {
                geometry_member=i;
                const auto& r=(*result.snapshots)[i].visible_rect;
                result.member_sizes[i]={core::movement::detail::checked_extent(r.right(),r.left()),
                    core::movement::detail::checked_extent(r.bottom(),r.top())};
                if(result.member_sizes[i][0]<=0 || result.member_sizes[i][1]<=0)
                    throw std::invalid_argument("nonpositive readiness extent");
            }
            result.work_area=(*result.snapshots)[0].monitor_work_area;
            result.readiness=group_layout_readiness(*result.snapshots);
            result.reason=result.readiness.reason;
            result.valid=true;
            if(!result.readiness.ready && result.reason!="disconnected_topology") {
                result.valid=false;failed_=true;
                detail::GroupMemberCaptureResult failure;failure.reason=ExplorerEligibilityReason::GeometryCaptureFailed;
                detail::group_capture_failure(result.capture_result,0,detail::GroupCaptureStage::Context,
                    failure,result.reason);
                result.capture_result.failed_member_index.reset(); // group-level computation, no invented member
            }
        } catch(const std::exception&) {
            result.reason="invalid_readiness_geometry";failed_=true;
            detail::GroupMemberCaptureResult failure;failure.reason=ExplorerEligibilityReason::ArithmeticOverflow;
            detail::group_capture_failure(result.capture_result,geometry_member,detail::GroupCaptureStage::Context,
                failure,result.reason);
        }
        last_=result;
        return result;
    }

    template<class Capture>
    GroupReadinessPreview prepare_setup(const GroupReadinessActivity& activity,Capture&& capture) {
        auto result=preview(activity,std::forward<Capture>(capture),true);
        if(result.valid && result.readiness.ready) accepted_=result;
        return result;
    }
    [[nodiscard]] const auto& accepted() const noexcept {return accepted_;}
    [[nodiscard]] const auto& last() const noexcept {return last_;}
private:
    std::uint64_t generation_{},attempts_{};
    detail::GroupSnapshots binding_;
    bool failed_{};
    std::optional<GroupReadinessPreview> last_,accepted_;
};
} // namespace panebind::platform::windows::explorer
