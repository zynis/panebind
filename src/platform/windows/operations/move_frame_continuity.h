#pragma once

#include "platform/windows/operations/live_move_writer.h"

#include <cstdint>
#include <mutex>
#include <optional>

namespace panebind::platform::windows::operations {

// Exact, post-handoff geometry is a validation baseline, never an input
// anchor for CursorMoveMagnetIntent. This object grants no HWND authority.
enum class MoveFrameObservation {
    NotArmed,
    Expected,
    OwnInFlight,
    Stale,
    ExternalChange,
};

enum class MoveSampleDecision {
    Process,
    DropStale,
    Reject,
};

// The owned Raw receiver uses this same classification before forwarding a
// geometry sample to the Move session. A stale frame is neither authority nor
// evidence of an external translation; unrelated context loss still rejects.
[[nodiscard]] constexpr MoveSampleDecision classify_move_sample(
    MoveFrameObservation observation, bool static_context_valid) noexcept {
    if (!static_context_valid) return MoveSampleDecision::Reject;
    switch (observation) {
    case MoveFrameObservation::Expected:
    case MoveFrameObservation::OwnInFlight:
        return MoveSampleDecision::Process;
    case MoveFrameObservation::Stale:
        return MoveSampleDecision::DropStale;
    case MoveFrameObservation::NotArmed:
    case MoveFrameObservation::ExternalChange:
        return MoveSampleDecision::Reject;
    }
    return MoveSampleDecision::Reject;
}

class MoveFrameContinuity final {
public:
    [[nodiscard]] bool arm(std::uint64_t generation,
                           const MoveFrameGeometry& handoff_actual);
    // Read immediately before a native geometry capture. A nonzero version
    // only orders this capture against our own committed/in-flight changes;
    // it does not grant permission to write.
    [[nodiscard]] std::uint64_t snapshot_version(std::uint64_t generation) const;
    [[nodiscard]] MoveFrameObservation observe(std::uint64_t generation,
                                               const MoveFrameGeometry& actual,
                                               std::uint64_t captured_version);
    // The caller captures `before` immediately before the native-attempt
    // boundary. A successful registration is not evidence that an API ran.
    [[nodiscard]] bool begin_attempt(std::uint64_t generation,
                                     std::uint64_t quantum,
                                     const MoveFrameGeometry& before,
                                     const MoveFrameGeometry& target);
    // Only for an attempt that never entered a native API.
    [[nodiscard]] bool abort_unissued(std::uint64_t generation,
                                      std::uint64_t quantum);
    // Called with the writer's actual receipt. Only an issued, successful,
    // exact postverified placement advances the baseline. A completion after
    // retire may be recorded but cannot restore write permission.
    [[nodiscard]] bool finish_attempt(std::uint64_t generation,
                                      std::uint64_t quantum,
                                      bool native_attempted,
                                      bool exact_verified,
                                      const std::optional<MoveFrameGeometry>& after);
    void retire(std::uint64_t generation);
    [[nodiscard]] bool may_write(std::uint64_t generation) const;

private:
    struct InFlight final {
        std::uint64_t quantum{};
        MoveFrameGeometry target;
    };

    mutable std::mutex mutex_;
    std::uint64_t generation_{};
    std::uint64_t version_{};
    std::uint64_t last_quantum_{};
    std::optional<MoveFrameGeometry> expected_;
    std::optional<InFlight> in_flight_;
    bool retired_{};
    bool invalid_{};
};

} // namespace panebind::platform::windows::operations
