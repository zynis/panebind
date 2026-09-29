#include "platform/windows/operations/move_frame_continuity.h"

namespace panebind::platform::windows::operations {
namespace {

bool same_frame(const MoveFrameGeometry& a,
                const MoveFrameGeometry& b) noexcept {
    return a.positioning == b.positioning && a.visible == b.visible;
}

bool valid_frame(const MoveFrameGeometry& frame) noexcept {
    return !frame.positioning.empty() && !frame.visible.empty();
}

bool same_sizes(const MoveFrameGeometry& a,
                const MoveFrameGeometry& b) noexcept {
    return a.positioning.size() == b.positioning.size() &&
           a.visible.size() == b.visible.size();
}

bool own_in_flight_frame(const MoveFrameGeometry& actual,
                         const MoveFrameGeometry& expected,
                         const MoveFrameGeometry& target) noexcept {
    // Win32 positioning and DWM visible bounds are sampled separately, so a
    // callback can see either component before or after this exact write.
    // No arbitrary intermediate rectangle or translated same-size frame is
    // made authoritative here. The full target must still pass postverify.
    return (actual.positioning == expected.positioning ||
            actual.positioning == target.positioning) &&
           (actual.visible == expected.visible ||
            actual.visible == target.visible);
}

} // namespace

bool MoveFrameContinuity::arm(std::uint64_t generation,
                              const MoveFrameGeometry& handoff_actual) {
    std::lock_guard lock{mutex_};
    if (generation == 0 || generation_ != 0 || !valid_frame(handoff_actual))
        return false;
    generation_ = generation;
    expected_ = handoff_actual;
    return true;
}

MoveFrameObservation MoveFrameContinuity::observe(
    std::uint64_t generation, const MoveFrameGeometry& actual) {
    std::lock_guard lock{mutex_};
    if (!expected_ || generation != generation_)
        return MoveFrameObservation::NotArmed;
    if (invalid_) return MoveFrameObservation::ExternalChange;
    if (same_frame(actual, *expected_)) return MoveFrameObservation::Expected;
    if (in_flight_ && own_in_flight_frame(actual, *expected_, in_flight_->target))
        return MoveFrameObservation::OwnInFlight;
    invalid_ = true;
    return MoveFrameObservation::ExternalChange;
}

bool MoveFrameContinuity::begin_attempt(
    std::uint64_t generation, std::uint64_t quantum,
    const MoveFrameGeometry& before, const MoveFrameGeometry& target) {
    std::lock_guard lock{mutex_};
    if (!expected_ || generation != generation_ || retired_ || invalid_ ||
        in_flight_ || quantum == 0 || quantum <= last_quantum_)
        return false;
    if (!same_frame(before, *expected_)) {
        invalid_ = true;
        return false;
    }
    if (!valid_frame(target) || !same_sizes(target, *expected_)) {
        invalid_ = true;
        return false;
    }
    in_flight_ = InFlight{quantum, target};
    last_quantum_ = quantum;
    return true;
}

bool MoveFrameContinuity::abort_unissued(std::uint64_t generation,
                                         std::uint64_t quantum) {
    std::lock_guard lock{mutex_};
    if (generation != generation_ || !in_flight_ ||
        in_flight_->quantum != quantum)
        return false;
    in_flight_.reset();
    return true;
}

bool MoveFrameContinuity::finish_attempt(
    std::uint64_t generation, std::uint64_t quantum, bool native_attempted,
    bool exact_verified, const std::optional<MoveFrameGeometry>& after) {
    std::lock_guard lock{mutex_};
    if (generation != generation_ || !in_flight_ ||
        in_flight_->quantum != quantum)
        return false;
    if (!native_attempted) {
        in_flight_.reset();
        return false;
    }
    const bool commit = !invalid_ && exact_verified && after &&
        same_frame(*after, in_flight_->target);
    if (commit) expected_ = *after;
    else invalid_ = true;
    in_flight_.reset();
    return commit;
}

void MoveFrameContinuity::retire(std::uint64_t generation) {
    std::lock_guard lock{mutex_};
    if (generation_ != 0 && generation == generation_) retired_ = true;
}

bool MoveFrameContinuity::may_write(std::uint64_t generation) const {
    std::lock_guard lock{mutex_};
    return expected_ && generation == generation_ && !retired_ &&
           !invalid_ && !in_flight_;
}

} // namespace panebind::platform::windows::operations
