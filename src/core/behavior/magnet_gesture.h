#pragma once
#include "core/geometry/magnet_constraint_solver.h"
#include <array>

namespace panebind::core::behavior {
namespace mg=magnet;
struct MagnetMember {
    model::WindowId id;
    std::uint64_t capability{},consent{};
    geometry::Rect visible;
};
enum class MagnetRoute { Classifying, MagnetMove, MagnetResize, GlueMove, CtrlResizeUnsupported, Aborted };
inline std::string_view magnet_route_name(MagnetRoute r) noexcept {
    switch(r){case MagnetRoute::Classifying:return "CLASSIFYING";case MagnetRoute::MagnetMove:return "MAGNET_MOVE";
    case MagnetRoute::MagnetResize:return "MAGNET_RESIZE";case MagnetRoute::GlueMove:return "EXISTING_C4A_GLUE";
    case MagnetRoute::CtrlResizeUnsupported:return "C4C_UNSUPPORTED";case MagnetRoute::Aborted:return "ABORTED";}return "INVALID";
}
struct MagnetCorrection {
    std::uint64_t group{},gesture{},operation{},source_receipt{};
    std::size_t source{};
    model::WindowId source_id;
    std::uint64_t capability{},consent{};
    geometry::Rect raw;
    mg::MagnetProposal proposal;
};
struct MagnetExpectation {
    MagnetCorrection command;
    std::uint64_t watermark{},provider_tick{};
    bool postverified{},acknowledged{};
};
struct MagnetCounters {
    std::size_t meaningful{},solver_calls{},motion_suppressed{},proposals{},corrections{},acknowledged{},duplicates{},ambiguous{},missing{};
    std::size_t x_latches{},y_latches{},latch_releases{};
};
// Pure gesture state only. No capability issuance, native handle, input query,
// clock read or window operation. The adapter supplies validated raw samples.
class MagnetGestureCoordinator final {
public:
    bool start(std::uint64_t group,std::uint64_t gesture,std::size_t source,bool ctrl,
               std::uint64_t sequence,std::uint64_t qpc,std::uint64_t frequency,
               std::span<const MagnetMember> members) {
        if(active_||!group||!gesture||gesture<=gesture_||(group_&&group_!=group)||source>=3||members.size()!=3||
           !sequence||sequence<=last_sequence_||!qpc||!frequency)return false;
        for(std::size_t i=0;i<3;++i){
            if(!members[i].capability||!members[i].consent||!mg::detail::valid_rect(members[i].visible))return false;
            for(std::size_t j=0;j<i;++j)if(members[i].id==members[j].id)return false;
        }
        members_.assign(members.begin(),members.end());source_=source;ctrl_=ctrl;group_=group;gesture_=gesture;
        start_sequence_=last_sequence_=sequence;last_qpc_=qpc;frequency_=frequency;
        initial_=last_raw_=expected_=members[source].visible;active_=true;completed_=false;retry_raw_=false;issued_operations_=0;route_=MagnetRoute::Classifying;
        reason_="none";edges_={};counters_={};pending_.clear();unregistered_.reset();x_.reset();y_.reset();
        targets_.clear();for(std::size_t i=0;i<3;++i)if(i!=source)targets_.push_back({members[i].id,members[i].visible,false});
        return true;
    }
    bool validate_members(std::span<const MagnetMember> current) {
        if(current.size()!=3){abort("member_set_changed");return false;}
        for(std::size_t i=0;i<3;++i){
            if(current[i].id!=members_[i].id||current[i].capability!=members_[i].capability||current[i].consent!=members_[i].consent){abort("member_identity_changed");return false;}
            if(!ctrl_&&i!=source_&&current[i].visible!=members_[i].visible){abort("unexpected_target_geometry");return false;}
        }
        return true;
    }
    // Provider ticks are normalized monotonic timestamps by the adapter, not a
    // time-window heuristic. Equal/unordered ticks never earn an ACK.
    std::string_view feedback(bool location_receipt,const MagnetMember& member,std::uint64_t group,std::uint64_t gesture,
        std::uint64_t operation,std::uint64_t sequence,std::uint64_t provider_tick,const geometry::Rect& actual) {
        if(!location_receipt)return "wrong_feedback_event";
        if(!active_||member.id!=members_[source_].id||member.capability!=members_[source_].capability||
           member.consent!=members_[source_].consent||group!=group_||gesture!=gesture_)return "wrong_feedback_identity";
        const auto found=std::find_if(pending_.begin(),pending_.end(),[&](const auto& p){return p.command.operation==operation;});
        if(found==pending_.end())return "wrong_feedback_operation";
        if(sequence<=found->watermark||provider_tick<=found->provider_tick)return "unordered_feedback";
        if(!found->postverified||actual!=found->command.proposal.corrected)return "unexpected_feedback_geometry";
        last_feedback_operation_=operation;
        if(found->acknowledged){++counters_.duplicates;return "duplicate";}
        found->acknowledged=true;++counters_.acknowledged;return "acknowledged";
    }
    std::optional<MagnetCorrection> sample(std::uint64_t sequence,std::uint64_t provider_tick,
        std::uint64_t qpc,const geometry::Rect& raw,bool location_receipt=true) {
        last_sample_result_="ignored";
        last_feedback_operation_=0;
        try {
        if(!active_||route_==MagnetRoute::Aborted||route_==MagnetRoute::CtrlResizeUnsupported)return std::nullopt;
        if(sequence<=last_sequence_||qpc<last_qpc_||!mg::detail::valid_rect(raw)){abort("invalid_sample_order_or_geometry");return std::nullopt;}
        last_sequence_=sequence;
        if(unregistered_){abort("uncommitted_proposal");return std::nullopt;}
        if(!pending_.empty()&&raw==pending_.back().command.proposal.corrected) {
            if(!location_receipt){last_sample_result_="end_exact_reconciliation";return std::nullopt;}
            const auto& p=pending_.back();last_sample_result_=feedback(true,members_[source_],group_,gesture_,p.command.operation,sequence,provider_tick,raw);
            if(last_sample_result_!="acknowledged"&&last_sample_result_!="duplicate")++counters_.ambiguous;
            return std::nullopt; // never feed a known corrected rectangle back into solver
        }
        if(!pending_.empty()&&provider_tick<=pending_.back().provider_tick){++counters_.ambiguous;last_sample_result_="stale_or_ambiguous_native_sample";return std::nullopt;}
        if(raw==last_raw_&&!retry_raw_) {
            if(raw!=expected_){abort("raw_reassertion_after_correction");return std::nullopt;}
            last_sample_result_="unchanged";return std::nullopt;
        }
        retry_raw_=false;
        const auto change=movement::classify_geometry_change(initial_,raw);
        if(route_==MagnetRoute::Classifying) {
            if(change.kind==movement::GeometryChangeKind::Unchanged)return std::nullopt;
            if(change.kind==movement::GeometryChangeKind::Translation)route_=ctrl_?MagnetRoute::GlueMove:MagnetRoute::MagnetMove;
            else {
                const auto inferred=mg::infer_resize_edges(initial_,raw);
                if(!inferred){abort("ambiguous_resize");return std::nullopt;}
                edges_=*inferred;
                if(ctrl_){route_=MagnetRoute::CtrlResizeUnsupported;reason_="ctrl_resize_not_implemented";return std::nullopt;}
                route_=MagnetRoute::MagnetResize;
            }
        }
        if((route_==MagnetRoute::MagnetMove||route_==MagnetRoute::GlueMove)&&change.kind==movement::GeometryChangeKind::ResizeOrMixed) {
            abort("move_changed_size");return std::nullopt;
        }
        if(route_==MagnetRoute::MagnetResize&&!mg::matches_latched_resize(initial_,raw,edges_)) {
            abort("resize_edges_changed");return std::nullopt;
        }
        ++counters_.meaningful;expected_=raw;
        const mg::MotionSample motion{last_raw_,last_qpc_,qpc,frequency_,speed_limit};
        last_raw_=raw;last_qpc_=qpc;
        if(ctrl_){last_sample_result_="glue_route";return std::nullopt;}
        mg::MagnetOptions options;options.screen_edges=false;options.max_targets=2;
        mg::MagnetInput input{initial_,raw,route_==MagnetRoute::MagnetResize?mg::Interaction::Resize:mg::Interaction::Move,
            edges_,targets_,options,motion,x_,y_,route_==MagnetRoute::MagnetResize};
        ++counters_.solver_calls;const auto solved=mg::MagnetConstraintSolver::solve(input);
        if(solved.motion!=mg::MotionState::BelowThreshold){++counters_.motion_suppressed;clear_preferences();last_sample_result_="motion_suppressed";return std::nullopt;}
        update_preferences(solved.proposal?solved.proposal->selected_x:std::nullopt,
                           solved.proposal?solved.proposal->selected_y:std::nullopt);
        if(!solved.proposal){last_sample_result_=solved.reason;return std::nullopt;}
        ++counters_.proposals;
        if(solved.proposal->corrected==raw){last_sample_result_="exact_noop";return std::nullopt;}
        if(pending_.size()>=256||issued_operations_>=1024){abort("correction_capacity");return std::nullopt;}
        const auto& member=members_[source_];
        unregistered_=MagnetCorrection{group_,gesture_,++issued_operations_,sequence,source_,member.id,member.capability,member.consent,raw,*solved.proposal};
        last_sample_result_="proposal";return unregistered_;
        }catch(const std::overflow_error&){abort("geometry_overflow");return std::nullopt;}
    }
    bool register_pending(const MagnetCorrection& command,std::uint64_t watermark,std::uint64_t provider_tick) {
        if(!active_||ctrl_||!unregistered_||command.group!=group_||command.gesture!=gesture_||command.source!=source_||
           command.operation!=unregistered_->operation||command.source_receipt!=last_sequence_||watermark<last_sequence_||
           command.source_id!=members_[source_].id||command.capability!=members_[source_].capability||command.consent!=members_[source_].consent||
           command.raw!=unregistered_->raw||command.proposal.corrected!=unregistered_->proposal.corrected||
           command.proposal.edges!=unregistered_->proposal.edges||command.proposal.move_delta!=unregistered_->proposal.move_delta||
           command.proposal.selected_x!=unregistered_->proposal.selected_x||command.proposal.selected_y!=unregistered_->proposal.selected_y)return false;
        pending_.push_back({command,watermark,provider_tick,false,false});unregistered_.reset();return true;
    }
    void discard_superseded_proposal() {
        unregistered_.reset();clear_preferences();retry_raw_=true;last_sample_result_="raw_sample_superseded";
    }
    bool postverify(std::uint64_t operation,bool native_success,const geometry::Rect& actual,bool exact_placement) {
        if(!active_||ctrl_||(route_!=MagnetRoute::MagnetMove&&route_!=MagnetRoute::MagnetResize)||pending_.empty()||pending_.back().command.operation!=operation||pending_.back().postverified||
           !native_success||!exact_placement||actual!=pending_.back().command.proposal.corrected){abort("nonexact_native_correction");return false;}
        pending_.back().postverified=true;expected_=actual;++counters_.corrections;return true;
    }
    bool finish(std::uint64_t end_sequence,std::span<const MagnetMember> actual) {
        if(!active_||end_sequence<last_sequence_||unregistered_||route_==MagnetRoute::Aborted||route_==MagnetRoute::CtrlResizeUnsupported||
           !validate_members(actual)){abort(reason_=="none"?"invalid_end":reason_);return false;}
        if(!ctrl_&&actual[source_].visible!=expected_){abort("nonexact_final_geometry");return false;}
        for(const auto& p:pending_){if(!p.postverified){abort("unverified_pending_at_end");return false;}if(!p.acknowledged)++counters_.missing;}
        clear_preferences();active_=false;completed_=true;last_sequence_=end_sequence;return true;
    }
    void abort(std::string_view reason) {reason_=reason;if(route_!=MagnetRoute::CtrlResizeUnsupported)route_=MagnetRoute::Aborted;clear_preferences();unregistered_.reset();}
    bool active() const noexcept {return active_;}
    bool completed() const noexcept {return completed_;}
    bool ctrl() const noexcept {return ctrl_;}
    MagnetRoute route() const noexcept {return route_;}
    std::string_view reason() const noexcept {return reason_;}
    std::string_view last_sample_result() const noexcept {return last_sample_result_;}
    std::uint64_t last_feedback_operation() const noexcept {return last_feedback_operation_;}
    const auto& counters() const noexcept {return counters_;}
    const auto& pending() const noexcept {return pending_;}
    const auto& members() const noexcept {return members_;}
    const auto& initial() const noexcept {return initial_;}
    const auto& last_raw() const noexcept {return last_raw_;}
    const auto& edges() const noexcept {return edges_;}
    std::uint64_t generation() const noexcept {return gesture_;}
    std::size_t source() const noexcept {return source_;}
    static constexpr geometry::Distance speed_limit=2000,release_distance=16;
private:
    static bool same_preference(const mg::MagnetAxisPreference& p,const mg::SatisfiedConstraint& c) {
        return p.target==c.target&&p.kind==c.kind&&p.moving_edge==c.moving_edge&&p.target_edge==c.target_edge;
    }
    void clear_preferences(){counters_.latch_releases+=(x_?1:0)+(y_?1:0);x_.reset();y_.reset();}
    void update_preferences(const std::optional<mg::SatisfiedConstraint>& x,const std::optional<mg::SatisfiedConstraint>& y) {
        const auto update=[&](auto& old,const auto& selected,std::size_t& count){
            if(old&&(!selected||!same_preference(*old,*selected))){++counters_.latch_releases;old.reset();}
            if(selected&&!old){old=mg::MagnetAxisPreference{selected->target,selected->kind,selected->moving_edge,selected->target_edge,release_distance};++count;}
        };update(x_,x,counters_.x_latches);update(y_,y,counters_.y_latches);
    }
    std::uint64_t group_{},gesture_{},start_sequence_{},last_sequence_{},last_qpc_{},frequency_{},issued_operations_{},last_feedback_operation_{};
    std::size_t source_{};bool ctrl_{},active_{},completed_{},retry_raw_{};
    MagnetRoute route_{MagnetRoute::Classifying};mg::ParticipatingEdges edges_;
    geometry::Rect initial_,last_raw_,expected_;
    std::vector<MagnetMember> members_;std::vector<mg::MagnetTarget> targets_;
    std::vector<MagnetExpectation> pending_;std::optional<MagnetCorrection> unregistered_;
    std::optional<mg::MagnetAxisPreference> x_,y_;MagnetCounters counters_;
    std::string_view reason_{"none"},last_sample_result_{"none"};
};
} // namespace panebind::core::behavior
