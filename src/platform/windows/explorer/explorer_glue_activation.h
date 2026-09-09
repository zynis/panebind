#pragma once

#include "platform/windows/explorer/explorer_glue_event_source.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace panebind::platform::windows::explorer {

// Policy facts only. These cannot mint an Explorer capability/native permit.
struct ExplorerGlueActivationPair final {
    std::uint64_t authority_id{};
    std::uint64_t authority_generation{};
    std::uint64_t leader_window_id{};
    std::uint64_t leader_capability_generation{};
    std::uint64_t follower_window_id{};
    std::uint64_t follower_capability_generation{};
    std::uint64_t leader_consent_generation{};
    std::uint64_t follower_consent_generation{};
    std::uint64_t glue_session_generation{};
    [[nodiscard]] bool operator==(const ExplorerGlueActivationPair&) const = default;
    [[nodiscard]] bool valid() const noexcept {
        return authority_id && authority_generation && leader_window_id &&
            follower_window_id && leader_window_id != follower_window_id &&
            leader_capability_generation && follower_capability_generation &&
            leader_consent_generation && follower_consent_generation &&
            glue_session_generation;
    }
};

enum class ExplorerGlueActivationReason : std::uint8_t {
    Activated, CtrlNotDownAtStart, WrongWindow, StalePairAuthority,
    WrongTargetGeneration, DuplicateStart, InvalidStart, MissingCtrlSample,
    InvalidTiming, EvidenceOverflow,
};

[[nodiscard]] constexpr std::string_view activation_reason_name(
    const ExplorerGlueActivationReason reason) noexcept {
    switch (reason) {
    case ExplorerGlueActivationReason::Activated: return "ctrl_at_leader_start";
    case ExplorerGlueActivationReason::CtrlNotDownAtStart: return "CTRL_NOT_DOWN_AT_START";
    case ExplorerGlueActivationReason::WrongWindow: return "wrong_window";
    case ExplorerGlueActivationReason::StalePairAuthority: return "stale_pair_authority";
    case ExplorerGlueActivationReason::WrongTargetGeneration: return "wrong_target_generation";
    case ExplorerGlueActivationReason::DuplicateStart: return "double_start";
    case ExplorerGlueActivationReason::InvalidStart: return "invalid_start";
    case ExplorerGlueActivationReason::MissingCtrlSample: return "missing_ctrl_sample";
    case ExplorerGlueActivationReason::InvalidTiming: return "invalid_timing";
    case ExplorerGlueActivationReason::EvidenceOverflow: return "activation_evidence_overflow";
    }
    return "invalid";
}

struct ExplorerGlueActivationAttempt final {
    std::uint64_t attempt_generation{};
    std::uint64_t activation_generation{};
    ExplorerGlueActivationPair pair;
    ExplorerGlueEvent event;
    CtrlSample callback_ctrl;
    CtrlSample owner_ctrl;
    std::int64_t callback_qpc{};
    std::int64_t owner_processing_qpc{};
    std::int64_t decision_qpc{};
    bool activated{};
    ExplorerGlueActivationReason reason{ExplorerGlueActivationReason::InvalidStart};
};

// One fixture/pair, one START-latched interaction. Not a general input monitor.
class ExplorerGlueActivationController final {
public:
    static constexpr std::size_t capacity = 8;
    explicit ExplorerGlueActivationController(ExplorerGlueActivationPair pair)
        : pair_(pair) {}

    [[nodiscard]] ExplorerGlueActivationAttempt evaluate_start(
        const ExplorerGlueEvent& event, const CtrlSample owner,
        const ExplorerGlueActivationPair& current_pair, const bool live_pair_valid,
        const std::int64_t owner_qpc, const std::int64_t decision_qpc) noexcept {
        ExplorerGlueActivationAttempt attempt;
        // No unbounded counter wrap after the explicit evidence cap is hit.
        if (attempt_count_ <= capacity) ++attempt_count_;
        attempt.attempt_generation = attempt_count_;
        attempt.pair = current_pair;
        attempt.event = event;
        attempt.callback_ctrl = event.ctrl_callback;
        attempt.owner_ctrl = owner;
        attempt.callback_qpc = event.callback_qpc;
        attempt.owner_processing_qpc = owner_qpc;
        attempt.decision_qpc = decision_qpc;
        if (stored_ == capacity) {
            overflowed_ = poisoned_ = true;
            active_ = false;
            attempt.reason = ExplorerGlueActivationReason::EvidenceOverflow;
            return attempt;
        }
        if (event.kind != ExplorerGlueEventKind::MoveResizeStarted ||
            event.receipt_sequence == 0) {
            attempt.reason = ExplorerGlueActivationReason::InvalidStart;
        } else if (!pair_.valid() || !live_pair_valid || current_pair != pair_) {
            attempt.reason = ExplorerGlueActivationReason::StalePairAuthority;
        } else if (event.role != ExplorerGlueWindowRole::Leader ||
                   event.window_id != pair_.leader_window_id) {
            attempt.reason = ExplorerGlueActivationReason::WrongWindow;
        } else if (event.capability_generation != pair_.leader_capability_generation) {
            attempt.reason = ExplorerGlueActivationReason::WrongTargetGeneration;
        } else if (start_evaluated_ || poisoned_) {
            attempt.reason = ExplorerGlueActivationReason::DuplicateStart;
        } else {
            start_evaluated_ = true;
            if (!event.ctrl_callback.available || !owner.available) {
                attempt.reason = ExplorerGlueActivationReason::MissingCtrlSample;
            } else if (event.callback_qpc <= 0 || owner_qpc < event.callback_qpc ||
                       decision_qpc < owner_qpc) {
                attempt.reason = ExplorerGlueActivationReason::InvalidTiming;
            } else if (!event.ctrl_callback.ctrl) {
                rejected_ = true;
                attempt.reason = ExplorerGlueActivationReason::CtrlNotDownAtStart;
            } else {
                active_ = attempt.activated = true;
                generation_ = attempt.activation_generation = attempt.attempt_generation;
                attempt.reason = ExplorerGlueActivationReason::Activated;
            }
        }
        if (attempt.reason != ExplorerGlueActivationReason::Activated &&
            attempt.reason != ExplorerGlueActivationReason::CtrlNotDownAtStart &&
            attempt.reason != ExplorerGlueActivationReason::WrongWindow) {
            poisoned_ = true;
            active_ = false;
        }
        records_[stored_++] = attempt;
        return attempt;
    }
    [[nodiscard]] bool is_active() const noexcept { return active_ && !poisoned_; }
    // Live owner stamps immediately AFTER evaluate_start returns. Tests can
    // inject deterministic completion ticks without an OS clock dependency.
    [[nodiscard]] bool stamp_last_decision_complete(std::int64_t tick) noexcept {
        if (stored_ == 0 || overflowed_ || tick <= 0 ||
            tick < records_[stored_ - 1].decision_qpc) {
            poisoned_ = true;
            active_ = false;
            return false;
        }
        records_[stored_ - 1].decision_qpc = tick;
        return true;
    }
    [[nodiscard]] std::uint64_t activation_generation() const noexcept { return generation_; }
    [[nodiscard]] bool rejected_at_start() const noexcept { return rejected_; }
    [[nodiscard]] bool poisoned() const noexcept { return poisoned_; }
    [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
    [[nodiscard]] bool matches_activation(std::uint64_t generation,
        const ExplorerGlueActivationPair& current_pair) const noexcept {
        return is_active() && generation != 0 && generation == generation_ &&
            current_pair == pair_;
    }
    [[nodiscard]] std::span<const ExplorerGlueActivationAttempt> attempts() const noexcept {
        return {records_.data(), stored_};
    }
    void finish() noexcept { active_ = false; }

private:
    ExplorerGlueActivationPair pair_;
    std::array<ExplorerGlueActivationAttempt, capacity> records_{};
    std::size_t stored_{};
    std::uint64_t attempt_count_{};
    std::uint64_t generation_{};
    bool start_evaluated_{};
    bool active_{};
    bool rejected_{};
    bool poisoned_{};
    bool overflowed_{};
};

} // namespace panebind::platform::windows::explorer
