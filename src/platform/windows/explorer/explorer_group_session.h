#pragma once
#include "platform/windows/explorer/explorer_group_event_source.h"
#include "platform/windows/explorer/explorer_group_readiness.h"
#include "core/behavior/glue_group_move.h"
#include <chrono>

namespace panebind::platform::windows::explorer {
struct GroupQuantumRecord final {
    std::uint64_t id{},gesture{},first_sequence{},last_sequence{};
    std::size_t receipts{},coalesced{};
    std::int64_t owner_qpc{},end_qpc{};
};
struct GroupOperationRecord final {
    std::uint64_t group{},gesture{},batch{},source_sequence{},watermark{};
    std::size_t source_member{3};
    DWORD pre_native_tick{};
    std::int64_t receipt_qpc{},owner_qpc{};
    core::geometry::Rect source_visible;
    detail::GroupBatchReceipt receipt;
};
struct GroupFeedbackRecord final {
    std::size_t member{};
    std::uint64_t gesture{},batch{},sequence{};
    std::string_view result{"unattributed"};
    core::geometry::Rect observed_visible;
};
struct GroupGestureRecord final {
    std::uint64_t group{},gesture{},start_sequence{},end_sequence{};
    std::size_t source_member{3},starts{},locations{},ends{},batches{};
    CtrlSample callback_ctrl,owner_ctrl;
    std::int64_t callback_qpc{},decision_qpc{};
    detail::GroupSnapshots initial,final;
    bool exact{},roles_cleared{},pending_empty{},group_ready{};
};
class ExplorerGroupSession final {
public:
    using OwnedMembers=std::array<std::unique_ptr<ExplorerTestSession>,3>;
    // Call only after the harness' real console group-consent confirmation.
    // Takes existing separately consented sessions, never an HWND selector.
    static std::unique_ptr<ExplorerGroupSession> create(OwnedMembers members);
    ~ExplorerGroupSession();
    ExplorerGroupSession(const ExplorerGroupSession&)=delete;
    ExplorerGroupSession& operator=(const ExplorerGroupSession&)=delete;
    [[nodiscard]] GroupReadinessPreview preview_readiness();
    [[nodiscard]] const auto& last_readiness_preview() const noexcept {return readiness_fixture_->last();}
    [[nodiscard]] const auto& accepted_readiness() const noexcept {return readiness_fixture_->accepted();}
    bool setup();
    bool run_gesture(std::size_t expected_member,std::chrono::seconds timeout);
    bool restore();
    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] std::string_view reason() const noexcept {return reason_;}
    [[nodiscard]] const auto& bindings() const noexcept {return seal_->members();}
    [[nodiscard]] const auto& binding_snapshots() const noexcept {return binding_snapshots_;}
    [[nodiscard]] const auto& gestures() const noexcept {return gestures_;}
    [[nodiscard]] const auto& operations() const noexcept {return operations_;}
    [[nodiscard]] const auto& quanta() const noexcept {return quanta_;}
    [[nodiscard]] const auto& receipts() const noexcept {return receipts_;}
    [[nodiscard]] const auto& feedback() const noexcept {return feedback_;}
    [[nodiscard]] GroupEventFacts event_facts() const noexcept {return source_?source_->facts():GroupEventFacts{};}
    [[nodiscard]] std::uint64_t generation() const noexcept {return seal_->generation();}
    [[nodiscard]] std::uint64_t vdm_queries() const noexcept {return desktop_->query_count();}
    [[nodiscard]] std::size_t reconciled_missing() const noexcept {return model_->reconciled_missing();}
private:
    explicit ExplorerGroupSession(OwnedMembers members);
    detail::GroupSessions sessions() const noexcept;
    GroupReadinessActivity readiness_activity() const noexcept;
    std::vector<core::topology::WindowGeometry> geometry(const detail::GroupSnapshots&) const;
    void poison(std::string_view) noexcept;
    bool quantum(std::span<const GroupEventReceipt>,const detail::GroupSnapshots* validated_sample=nullptr);
    bool move_batch(const GroupEventReceipt&,const detail::GroupSnapshots&,std::int64_t owner_qpc);
    bool attribute(const GroupEventReceipt&,const detail::GroupSnapshots&);
    bool apply_targets(const std::array<std::optional<core::geometry::Rect>,3>&,
                       const detail::GroupSnapshots&,GroupOperationRecord&,
                       const core::behavior::FollowerMoveBatchCommand*);
    static bool register_pending(void*) noexcept;
    DWORD owner_{};
    OwnedMembers members_;
    std::unique_ptr<ExplorerVirtualDesktopManager> desktop_;
    std::optional<detail::ExplorerGroupSeal> seal_;
    std::unique_ptr<ExplorerGroupEventSource> source_;
    std::unique_ptr<core::behavior::GlueGroupMoveCoordinator> model_;
    std::vector<core::behavior::GlueGroupMember> logical_members_;
    detail::GroupSnapshots binding_snapshots_,original_,current_;
    std::optional<GroupReadinessFixture> readiness_fixture_;
    std::vector<GroupGestureRecord> gestures_;
    std::vector<GroupOperationRecord> operations_;
    std::vector<GroupQuantumRecord> quanta_;
    std::vector<GroupEventReceipt> receipts_;
    std::vector<GroupFeedbackRecord> feedback_;
    std::optional<std::size_t> active_member_,plain_member_;
    DWORD start_native_time_{};
    std::uint64_t native_generation_{};
    bool setup_done_{},restored_{},poisoned_{};
    std::string_view reason_{"none"};
    friend class ExplorerLiveMagnetSession;
};
} // namespace panebind::platform::windows::explorer
