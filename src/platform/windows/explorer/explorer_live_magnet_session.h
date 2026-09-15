#pragma once
#include "platform/windows/explorer/explorer_group_session.h"
#include "core/behavior/magnet_gesture.h"
namespace panebind::platform::windows::explorer {
struct LiveMagnetOperation {
    core::behavior::MagnetCorrection command;
    GroupEventReceipt trigger;
    std::uint64_t watermark{},provider_tick{};
    std::int64_t owner_qpc{};
    detail::MagnetNativeReceipt native;
};
struct LiveMagnetSample {
    std::uint64_t gesture{},receipt{},provider_tick{},feedback_operation{};
    std::size_t source{};
    core::geometry::Rect raw;
    std::string_view result;
    std::int64_t callback_qpc{},owner_qpc{};
};
struct LiveMagnetGesture {
    std::uint64_t generation{},start_receipt{},end_receipt{};
    std::size_t source{},raw_receipts{};
    bool ctrl{},completed{};
    core::behavior::MagnetRoute route{core::behavior::MagnetRoute::Classifying};
    core::magnet::ParticipatingEdges edges;
    core::behavior::MagnetCounters counters;
    detail::GroupSnapshots initial,final;
    GroupLayoutReadiness topology;
    std::string_view reason{"none"};
};
struct LiveMagnetQuantum {
    std::uint64_t id{},first{},last{},gesture{};
    std::size_t receipts{},coalesced{},solver_calls{},corrections{};
    std::int64_t owner_qpc{},end_qpc{};
    std::uint64_t inventory_active{},manager_creates_active{};
};
class ExplorerLiveMagnetSession final {
public:
    // Separate explicit live consent, never implied by the old C4A text.
    enum class Consent { Confirmed };
    static std::unique_ptr<ExplorerLiveMagnetSession> create(ExplorerGroupSession::OwnedMembers,Consent);
    ~ExplorerLiveMagnetSession();
    bool pump() noexcept;
    bool accept_topology();
    bool restore();
    bool healthy() const noexcept;
    bool idle() const noexcept {return !active_source_;}
    bool accepted() const noexcept {return accepted_.has_value();}
    std::string_view reason() const noexcept {return reason_;}
    const auto& group() const noexcept {return *group_;}
    const auto& current() const noexcept {return current_;}
    const auto& accepted_baseline() const noexcept {return accepted_;}
    const auto& gestures() const noexcept {return gestures_;}
    const auto& operations() const noexcept {return operations_;}
    const auto& samples() const noexcept {return samples_;}
    const auto& quanta() const noexcept {return quanta_;}
    const auto& receipts() const noexcept {return receipts_;}
    const auto& last_capture() const noexcept {return last_capture_;}
    const auto& audit() const noexcept {return *audit_;}
private:
    explicit ExplorerLiveMagnetSession(std::unique_ptr<ExplorerGroupSession>);
    bool process(std::span<const GroupEventReceipt>);
    bool stop(std::string_view);
    std::vector<core::behavior::MagnetMember> members(const detail::GroupSnapshots&) const;
    static bool register_pending(void*) noexcept;
    std::unique_ptr<ExplorerGroupSession> group_;
    std::unique_ptr<ConsentValidationAudit> audit_;
    core::behavior::MagnetGestureCoordinator model_;
    detail::GroupSnapshots current_;
    detail::GroupCaptureResult last_capture_;
    std::optional<detail::GroupSnapshots> accepted_;
    std::optional<std::size_t> active_source_;
    std::vector<LiveMagnetGesture> gestures_;
    std::vector<LiveMagnetOperation> operations_;
    std::vector<LiveMagnetSample> samples_;
    std::vector<LiveMagnetQuantum> quanta_;
    std::vector<GroupEventReceipt> receipts_;
    std::uint64_t generation_{},last_sequence_{};
    bool failed_{},processing_{};
    std::string_view reason_{"none"};
};
}
