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
    ExternalChange,
};

class MoveFrameContinuity final {
public:
    [[nodiscard]] bool arm(std::uint64_t generation,
                           const MoveFrameGeometry& handoff_actual);
    [[nodiscard]] MoveFrameObservation observe(std::uint64_t generation,
                                               const MoveFrameGeometry& actual);
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
    std::uint64_t last_quantum_{};
    std::optional<MoveFrameGeometry> expected_;
    std::optional<InFlight> in_flight_;
    bool retired_{};
    bool invalid_{};
};

} // namespace panebind::platform::windows::operations
