#pragma once

#include "core/geometry/geometry.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>

namespace panebind::platform::windows::operations {

// One writer is bound to one already-authorized source and one gesture
// generation. This is a scheduling boundary, not an HWND capability issuer.
struct MoveFrameGeometry final {
    core::geometry::Rect positioning;
    core::geometry::Rect visible;
};

struct MoveWriteRequest final {
    std::uint64_t generation{};
    std::uint64_t quantum{};
    core::geometry::Rect target_visible;
    bool snapped{};
};

struct MoveNativePlacement final {
    bool attempted{};
    bool succeeded{};
    bool outcome_known{true};
};

enum class MoveAttemptStartOrder {
    NotStarted,
    ClaimedBeforeRevoke,
    RejectedAfterRevoke,
};

// The adapter evaluates these from the real bound source/participants. A
// matching Raw UP revokes *new* calls, but it is benign only while the stable
// identity/geometry context is still valid. ContextInvalid always wins.
struct MovePreflightFacts final {
    bool context_valid{};
    bool gesture_active{};
    bool matching_raw_up{};
};

enum class MovePreflightVerdict {
    Allowed,
    NormalUpRevoked,
    ContextInvalid,
};

[[nodiscard]] MovePreflightVerdict classify_move_preflight(
    const MovePreflightFacts& facts) noexcept;

// Post-verification concerns the already-issued native call. None of these
// facts is the current button state, overlay presence, or writer armed bit.
struct MovePostContextFacts final {
    bool source_identity_valid{};
    bool participant_identity_valid{};
    bool geometry_context_valid{};
    bool captured_after_matches{};
};

[[nodiscard]] bool move_post_context_valid(
    const MovePostContextFacts& facts) noexcept;

struct MoveWriteCallbacks final {
    // Each callback runs only on the dedicated writer thread. The adapter must
    // own and revalidate its existing exact capability, never accept an
    // arbitrary HWND supplied through this writer.
    std::function<std::optional<MoveFrameGeometry>()> capture;
    // context_valid is independently checked even when a matching Raw UP
    // already retired the gesture. A missing/foreign UP is not normal revoke.
    std::function<MovePreflightVerdict(const MoveFrameGeometry&)> fresh_authority;
    std::function<MoveNativePlacement(const MoveFrameGeometry& before,
                                      const core::geometry::Rect& target_visible,
                                      const core::geometry::Rect& expected_positioning,
                                      std::uint64_t quantum)> place;
    // Read-only exact identity/context check after native return and capture.
    // The adapter checks all relevant capability, monitor/DPI, and other
    // participating-window facts; rectangle equality alone is not authority.
    std::function<bool(const MoveFrameGeometry& before,
                       const MoveFrameGeometry& after)> post_context;
};

struct MoveWriteReceipt final {
    std::uint64_t generation{};
    std::uint64_t quantum{};
    core::geometry::Rect target_visible;
    bool snapped{};
    // Only a dispatch reservation. The native API may not yet have been
    // entered; native_attempted is supplied by the capability adapter.
    bool dispatch_reserved{};
    bool native_attempted{};
    bool native_succeeded{};
    bool native_outcome_known{true};
    MoveAttemptStartOrder attempt_start_order{MoveAttemptStartOrder::NotStarted};
    bool exact{};
    bool geometry_exact{};
    bool post_context_exact{};
    bool retired_during_dispatch{};
    std::optional<MoveFrameGeometry> before;
    std::optional<MoveFrameGeometry> after;
    std::optional<core::geometry::Rect> expected_positioning;
    std::string_view reason{"none"};
};

// The owned adapter uses this exact decision for its failure counter. A
// normally revoked, unissued request is not a placement failure; an issued
// call or a real context failure cannot be hidden by a later Raw UP.
[[nodiscard]] bool move_write_receipt_failed(
    const MoveWriteReceipt& receipt, bool continuity_committed) noexcept;

struct MoveRetireFacts final {
    bool retired_now{};
    bool pending_discarded{};
    bool dispatch_reserved{};
    std::uint64_t reserved_quantum{};
    bool attempt_started_before_revoke{};
};

// The receiver thread may only call offer()/retire(); it must never call
// wait_and_execute() because capture/place may synchronously block in Win32.
// The owner must join its writer thread before destroying this object or the
// callbacks' capability/session. retire() never waits for an in-flight call.
class LiveMoveWriter final {
public:
    LiveMoveWriter(std::uint64_t generation, MoveWriteCallbacks callbacks);
    LiveMoveWriter(const LiveMoveWriter&) = delete;
    LiveMoveWriter& operator=(const LiveMoveWriter&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    // Advisory read during adapter preflight. It is not the final native
    // boundary: begin_native_attempt() must still be called immediately
    // before the API because Raw UP can race between this read and the call.
    [[nodiscard]] bool active(std::uint64_t generation) const noexcept;
    // The final adapter-side gate, called immediately before entering the
    // native API. This claim is serialized with retire(); a successful claim
    // is an attempt-start witness, NOT proof that the API has returned or even
    // been entered. The callback must report actual native_attempted honestly.
    // Do not perform further validation or blocking work between this claim
    // and the native call. The writer clears the in-flight witness when the
    // place callback returns, without waiting in retire().
    [[nodiscard]] bool begin_native_attempt(std::uint64_t generation) noexcept;
    [[nodiscard]] bool offer(const MoveWriteRequest& request);
    [[nodiscard]] MoveRetireFacts retire(std::uint64_t generation) noexcept;

    // Returns nullopt only after retirement. Wait is event-driven; no cursor
    // polling, timer quantum or fixed-frequency worker loop is introduced.
    [[nodiscard]] std::optional<MoveWriteReceipt> wait_and_execute();
    // For a source-window creator thread driven by posted work messages. This
    // never blocks its message pump when a stale/duplicate notice arrives.
    [[nodiscard]] std::optional<MoveWriteReceipt> try_execute();

private:
    [[nodiscard]] std::optional<MoveWriteReceipt> pop_and_execute(bool wait);
    [[nodiscard]] MoveWriteReceipt execute(MoveWriteRequest request);

    const std::uint64_t generation_;
    const MoveWriteCallbacks callbacks_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<MoveWriteRequest> pending_;
    std::uint64_t last_offered_quantum_{};
    std::uint64_t last_started_quantum_{};
    std::uint64_t reserved_quantum_{};
    bool attempt_claimed_{};
    bool attempt_rejected_{};
    bool retired_{};
    bool worker_active_{};
};

} // namespace panebind::platform::windows::operations
