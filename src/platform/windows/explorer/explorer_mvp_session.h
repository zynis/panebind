#pragma once

#include "platform/windows/explorer/explorer_group_session.h"
#include "platform/windows/explorer/explorer_mvp_attribution.h"
#include "platform/windows/operations/gesture_input_shield.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace panebind::platform::windows::explorer {

enum class MvpEvidenceKind {
    Down, Start, IsolationReady, Cancel, NativeEnd, Handoff, Writer,
    RawUp, LegacyUp, IsolationGone, Rejected, Resource,
};

// Fixed, owner-STA facts for the entry's existing JSONL sink. Absence means
// UNKNOWN, not false. Resource callbacks never serialize or write to disk.
struct MvpEvidenceEvent final {
    MvpEvidenceKind kind{MvpEvidenceKind::Resource};
    std::uint64_t generation{}, raw_sequence{}, native_sequence{}, quantum{};
    std::size_t source_member{3};
    std::optional<MvpGestureRoute> route;
    std::string_view reason{"none"};
    std::optional<core::geometry::Point> cursor;
    std::optional<core::geometry::Rect> initial_visible, target_visible;
    std::optional<core::geometry::Rect> actual_visible, actual_positioning;
    std::optional<bool> attempted, succeeded, outcome_known, geometry_exact;
    std::optional<bool> post_context_exact, snapped, overlay_destroyed, hotkey_unregistered;
    std::optional<bool> receiver_destroyed, raw_registration_removed;
    std::optional<bool> winevent_unhooked, classes_unregistered;
    std::optional<std::uint32_t> win32_error;
    std::optional<std::uint32_t> isolation_timeout_ms;
    std::optional<std::uint32_t> shield_route_message;
    std::optional<std::uintptr_t> shield_route_wparam, shield_route_capture;
    std::optional<operations::GestureShieldCaptureFacts> shield_capture;
    std::optional<std::uint64_t> native_attempt_qpc, raw_up_qpc;
    std::optional<std::uint32_t> shield_setup_stage, shield_native_failure,
        shield_readback_failure, shield_created_exstyle,
        shield_initial_exstyle, shield_observed_exstyle;
    std::optional<bool> shield_topmost_retry_attempted;
    std::optional<operations::GestureShieldPlacementFacts> shield_initial_placement,
        shield_retry_placement;
    std::optional<operations::GestureShieldWindowPosFacts> shield_windowpos_changing,
        shield_windowpos_changed;
    std::uintptr_t overlay{};
};

// One consent-bound product entry: ordinary Move may be taken over only after
// real Raw/WinEvent attribution, an observed shield, native END and a fresh
// three-frame capture. Ctrl Move stays in the native leader/Glue route; Resize
// is native. The owner is the provisioning STA, including every Shell capture
// and source SetWindowPos. No arbitrary HWND admission is exposed.
class ExplorerMvpSession final {
public:
    using OwnedMembers = ExplorerGroupSession::OwnedMembers;

    [[nodiscard]] static std::unique_ptr<ExplorerMvpSession>
    create_after_explicit_consent(OwnedMembers members,
                                  std::function<bool()> environment_allowed);
    ~ExplorerMvpSession();
    ExplorerMvpSession(const ExplorerMvpSession&) = delete;
    ExplorerMvpSession& operator=(const ExplorerMvpSession&) = delete;

    // Call from the provisioning STA's normal message/console pump. Neither
    // method creates synthetic input or blocks waiting for a drag to finish.
    [[nodiscard]] bool pump();
    [[nodiscard]] std::optional<GroupProductStatus> status();
    [[nodiscard]] bool stop();
    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] std::string_view reason() const noexcept;
    [[nodiscard]] const std::array<detail::GroupMemberBinding, 3>& bindings() const noexcept;
    // Owner-STA drain; all returned events were produced on that STA from
    // observed adapter facts or actual writer receipts, never synthetic input.
    [[nodiscard]] std::vector<MvpEvidenceEvent> drain_evidence_events();

private:
    struct Impl;
    explicit ExplorerMvpSession(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::explorer
