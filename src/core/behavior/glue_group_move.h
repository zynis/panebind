#pragma once

#include "core/movement/move_plan.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace panebind::core::behavior {

// Logical authority facts only. Native consent issuance stays in the adapter.
struct GlueGroupMember final {
    model::WindowId id;
    std::uint64_t capability_generation{};
    friend bool operator==(const GlueGroupMember&, const GlueGroupMember&) = default;
};

enum class GlueGroupState { GroupReady, GestureActive, GestureCompleting, GroupPoisoned };
struct FollowerMoveBatchCommand final {
    std::uint64_t group_generation{};
    std::uint64_t gesture_generation{};
    std::uint64_t batch_generation{};
    std::uint64_t source_leader_sequence{};
    std::vector<movement::PlannedTranslation> followers;
};
struct GroupPendingExpectation final {
    GlueGroupMember member;
    std::uint64_t group_generation{};
    std::uint64_t gesture_generation{};
    std::uint64_t batch_generation{};
    std::uint64_t source_leader_sequence{};
    std::uint64_t registration_watermark{};
    geometry::Rect expected;
    bool exact_native_result{};
    bool feedback_observed{};
};
enum class GroupFeedbackResult { Acknowledged, Duplicate, Stale, Rejected };

// One role-neutral authorized set; the optional Gesture owns ALL transient
// roles and topology. No native handles, event constants or input APIs here.
class GlueGroupMoveCoordinator final {
public:
    GlueGroupMoveCoordinator(std::vector<GlueGroupMember> members,
                             std::uint64_t group_generation,
                             std::size_t pending_limit = 256)
        : members_(std::move(members)), group_generation_(group_generation),
          pending_limit_(pending_limit) {
        std::sort(members_.begin(), members_.end(), [](const auto& a, const auto& b) {
            return a.id.value() < b.id.value();
        });
        if (members_.empty() || group_generation == 0 || pending_limit == 0)
            throw std::invalid_argument("invalid group authority");
        for (std::size_t i = 0; i < members_.size(); ++i) {
            if (!members_[i].capability_generation ||
                (i && members_[i - 1].id == members_[i].id))
                throw std::invalid_argument("invalid group member");
        }
    }

    [[nodiscard]] GlueGroupState state() const noexcept { return state_; }
    [[nodiscard]] std::uint64_t group_generation() const noexcept { return group_generation_; }
    [[nodiscard]] std::uint64_t gesture_generation() const noexcept { return last_gesture_; }
    [[nodiscard]] std::string_view reason() const noexcept { return reason_; }
    [[nodiscard]] std::span<const GlueGroupMember> members() const noexcept { return members_; }
    [[nodiscard]] const model::WindowId* gesture_leader() const noexcept {
        return gesture_ ? &gesture_->leader : nullptr;
    }
    [[nodiscard]] std::span<const GroupPendingExpectation> pending() const noexcept {
        return gesture_ ? std::span<const GroupPendingExpectation>(gesture_->pending) :
                          std::span<const GroupPendingExpectation>{};
    }
    [[nodiscard]] std::size_t reconciled_missing() const noexcept { return reconciled_missing_; }
    [[nodiscard]] std::size_t acknowledged() const noexcept { return acknowledged_; }

    void abort(std::string_view reason) noexcept {
        state_ = GlueGroupState::GroupPoisoned;
        reason_ = reason;
        gesture_.reset();
    }

    // Caller supplies fresh snapshots of EXACTLY the authorized set. Unrelated
    // touching nodes cannot be smuggled into the graph, even when disconnected.
    [[nodiscard]] bool start(const GlueGroupMember& source, bool ctrl_at_start,
        std::uint64_t start_sequence, std::span<const topology::WindowGeometry> fresh,
        topology::AdjacencyOptions options = {}, std::size_t required_component = 0) {
        if (state_ != GlueGroupState::GroupReady) {
            abort("concurrent_start"); return false;
        }
        if (!member_matches(source) || !start_sequence || start_sequence <= last_end_) {
            abort("invalid_start_identity_or_sequence"); return false;
        }
        if (!ctrl_at_start) return false;
        if (!snapshots_authorized(fresh)) { abort("snapshot_authority_mismatch"); return false; }
        try {
            auto graph = topology::WindowAdjacencyGraph::build(fresh, options);
            auto component = graph.connected_component(source.id);
            if (required_component && component.size() != required_component) {
                abort("component_size_mismatch"); return false;
            }
            if (last_gesture_ == std::numeric_limits<std::uint64_t>::max()) {
                abort("gesture_generation_exhausted"); return false;
            }
            gesture_.emplace(source.id, std::move(graph), std::move(component), start_sequence, options);
            ++last_gesture_;
            state_ = GlueGroupState::GestureActive;
            return true;
        } catch (const std::exception&) { abort("invalid_start_geometry"); return false; }
    }

    [[nodiscard]] std::optional<FollowerMoveBatchCommand> plan(
        const GlueGroupMember& source, std::uint64_t source_sequence,
        const geometry::Rect& leader_visible) {
        if (state_ != GlueGroupState::GestureActive || !gesture_ ||
            !member_matches(source) || !(source.id == gesture_->leader)) return std::nullopt;
        auto& g = *gesture_;
        if (source_sequence <= g.last_source_sequence || g.unregistered || g.awaiting_result) {
            abort("sample_sequence_or_unfinished_batch"); return std::nullopt;
        }
        g.last_source_sequence = source_sequence;
        try {
            auto targets = g.translation.plan(leader_visible);
            if (!targets) { abort("resize_or_mixed"); return std::nullopt; }
            if (*targets == g.last_targets) return std::nullopt;
            if (g.batch_generation == std::numeric_limits<std::uint64_t>::max()) {
                abort("batch_generation_exhausted"); return std::nullopt;
            }
            FollowerMoveBatchCommand batch{group_generation_, last_gesture_,
                ++g.batch_generation, source_sequence, std::move(*targets)};
            g.unregistered = batch;
            return batch;
        } catch (const std::exception&) { abort("translation_overflow"); return std::nullopt; }
    }

    // Invoke only after ALL native preflights pass, before ANY native commit.
    [[nodiscard]] bool register_batch(const FollowerMoveBatchCommand& batch,
                                     std::uint64_t watermark) {
        if (!valid_batch(batch) || !gesture_->unregistered ||
            gesture_->unregistered->followers != batch.followers ||
            watermark < batch.source_leader_sequence) {
            abort("invalid_batch_registration"); return false;
        }
        auto& g = *gesture_;
        if (g.pending.size() + batch.followers.size() > pending_limit_) {
            abort("pending_capacity"); return false;
        }
        // Same-member repeated outstanding geometry is ambiguous, not a guess.
        for (const auto& target : batch.followers) {
            for (const auto& p : g.pending) {
                if (p.member.id == target.id && p.expected == target.target_visible_rect &&
                    !p.feedback_observed) { abort("ambiguous_pending_geometry"); return false; }
            }
        }
        for (const auto& target : batch.followers) {
            const auto* member = find_member(target.id);
            if (!member) { abort("unauthorized_target"); return false; }
            g.pending.push_back({*member, group_generation_, last_gesture_,
                batch.batch_generation, batch.source_leader_sequence, watermark,
                target.target_visible_rect, false, false});
        }
        g.unregistered.reset();
        g.awaiting_result = batch;
        return true;
    }

    // Result is per member. A failed native return or any mismatch poisons the
    // complete group; no further writes or automatic rollback are authorized.
    [[nodiscard]] bool postverify(const FollowerMoveBatchCommand& batch,
        bool native_succeeded, std::span<const movement::PlannedTranslation> actual) {
        if (!valid_batch(batch) || !gesture_->awaiting_result || !native_succeeded ||
            !std::equal(actual.begin(), actual.end(), batch.followers.begin(), batch.followers.end())) {
            abort("native_or_postverify_failure"); return false;
        }
        auto& g = *gesture_;
        for (auto& p : g.pending) if (p.batch_generation == batch.batch_generation)
            p.exact_native_result = true;
        g.last_targets = batch.followers;
        g.awaiting_result.reset();
        return true;
    }

    [[nodiscard]] GroupFeedbackResult feedback(const GlueGroupMember& member,
        std::uint64_t group, std::uint64_t gesture, std::uint64_t batch,
        std::uint64_t receipt_sequence, const geometry::Rect& actual) {
        if (group != group_generation_ || gesture != last_gesture_ || !gesture_ ||
            receipt_sequence <= gesture_->start_sequence) return GroupFeedbackResult::Stale;
        if (!member_matches(member) || member.id == gesture_->leader) {
            abort("wrong_feedback_member"); return GroupFeedbackResult::Rejected;
        }
        for (auto& p : gesture_->pending) {
            if (p.member == member && p.batch_generation == batch) {
                if (receipt_sequence <= p.registration_watermark) return GroupFeedbackResult::Stale;
                if (actual != p.expected || !p.exact_native_result) {
                    abort("feedback_geometry_or_result_mismatch"); return GroupFeedbackResult::Rejected;
                }
                if (p.feedback_observed) return GroupFeedbackResult::Duplicate;
                p.feedback_observed = true;
                ++acknowledged_;
                return GroupFeedbackResult::Acknowledged;
            }
        }
        abort("unknown_feedback_batch"); return GroupFeedbackResult::Rejected;
    }

    [[nodiscard]] bool finish(const GlueGroupMember& source, std::uint64_t end_sequence,
                             std::span<const topology::WindowGeometry> actual) {
        if (state_ != GlueGroupState::GestureActive || !gesture_ ||
            !member_matches(source) || source.id != gesture_->leader ||
            end_sequence < gesture_->last_source_sequence || end_sequence <= gesture_->start_sequence || !snapshots_authorized(actual) ||
            gesture_->unregistered || gesture_->awaiting_result) {
            abort("invalid_end"); return false;
        }
        state_ = GlueGroupState::GestureCompleting;
        const auto leader = std::find_if(actual.begin(), actual.end(), [&](const auto& s) {
            return s.id == source.id;
        });
        try {
            const auto expected = gesture_->translation.plan(leader->visible_rect);
            if (!expected) { abort("end_resize"); return false; }
            const auto final_graph = topology::WindowAdjacencyGraph::build(actual, gesture_->options);
            if (final_graph.connected_component(source.id) != gesture_->component) {
                abort("end_component_mismatch"); return false;
            }
            for (const auto& target : *expected) {
                const auto found = std::find_if(actual.begin(), actual.end(), [&](const auto& s) {
                    return s.id == target.id;
                });
                if (found == actual.end() || found->visible_rect != target.target_visible_rect) {
                    abort("end_geometry_mismatch"); return false;
                }
            }
            for (const auto& p : gesture_->pending) {
                if (!p.exact_native_result) { abort("end_unverified_operation"); return false; }
                if (!p.feedback_observed) ++reconciled_missing_;
            }
            last_end_ = end_sequence;
            gesture_.reset();
            state_ = GlueGroupState::GroupReady;
            return true;
        } catch (const std::exception&) { abort("invalid_end_geometry"); return false; }
    }

private:
    struct Gesture final {
        model::WindowId leader;
        topology::WindowAdjacencyGraph graph;
        std::vector<model::WindowId> component;
        movement::TranslationSession translation;
        topology::AdjacencyOptions options;
        std::uint64_t start_sequence{};
        std::uint64_t last_source_sequence{};
        std::uint64_t batch_generation{};
        std::vector<GroupPendingExpectation> pending;
        std::vector<movement::PlannedTranslation> last_targets;
        std::optional<FollowerMoveBatchCommand> unregistered;
        std::optional<FollowerMoveBatchCommand> awaiting_result;
        Gesture(model::WindowId source, topology::WindowAdjacencyGraph topology,
                std::vector<model::WindowId> members, std::uint64_t sequence, topology::AdjacencyOptions adjacency)
            : leader(std::move(source)), graph(std::move(topology)), component(std::move(members)),
              translation(graph, leader), options(adjacency), start_sequence(sequence), last_source_sequence(sequence) {
            for (const auto& w : graph.windows()) if (w.id == leader)
                last_targets = *translation.plan(w.visible_rect);
        }
    };
    [[nodiscard]] const GlueGroupMember* find_member(const model::WindowId& id) const noexcept {
        for (const auto& m : members_) if (m.id == id) return &m;
        return nullptr;
    }
    [[nodiscard]] bool member_matches(const GlueGroupMember& member) const noexcept {
        const auto* found = find_member(member.id);
        return found && *found == member;
    }
    [[nodiscard]] bool snapshots_authorized(std::span<const topology::WindowGeometry> fresh) const {
        if (fresh.size() != members_.size()) return false;
        for (const auto& m : members_) if (std::count_if(fresh.begin(), fresh.end(),
            [&](const auto& w) { return w.id == m.id; }) != 1) return false;
        return true;
    }
    [[nodiscard]] bool valid_batch(const FollowerMoveBatchCommand& b) const noexcept {
        return state_ == GlueGroupState::GestureActive && gesture_ &&
            b.group_generation == group_generation_ && b.gesture_generation == last_gesture_ &&
            b.batch_generation == gesture_->batch_generation &&
            b.source_leader_sequence == gesture_->last_source_sequence;
    }
    std::vector<GlueGroupMember> members_;
    std::uint64_t group_generation_{};
    std::uint64_t last_gesture_{};
    std::uint64_t last_end_{};
    std::size_t pending_limit_{};
    std::size_t reconciled_missing_{};
    std::size_t acknowledged_{};
    GlueGroupState state_{GlueGroupState::GroupReady};
    std::string_view reason_{"none"};
    std::optional<Gesture> gesture_;
};

} // namespace panebind::core::behavior
