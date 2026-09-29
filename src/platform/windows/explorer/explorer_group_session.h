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
struct GroupProductStatus final {
    detail::GroupSnapshots snapshots;
    GroupLayoutReadiness topology;
    GroupEventFacts events;
    bool gesture_active{},glue_active{};
};

// This is only the product route gate. The adapter must separately prove the
// exact DOWN/START operation and retain native authority before any takeover.
[[nodiscard]] inline bool product_ctrl_glue_eligible(
    std::optional<std::size_t> approved_member, std::size_t event_member,
    const CtrlSample& ctrl, const GroupLayoutReadiness& topology) noexcept {
    return approved_member && *approved_member == event_member && event_member < 3 &&
           ctrl.available && ctrl.ctrl && topology.ready;
}
[[nodiscard]] inline bool product_receipts_match(
    std::span<const GroupEventReceipt> actual,
    std::span<const GroupEventReceipt> offered) noexcept {
    if(actual.size()!=offered.size())return false;
    for(std::size_t i=0;i<actual.size();++i) {
        const auto& a=actual[i];const auto& b=offered[i];
        if(a.member_index!=b.member_index || a.window_id!=b.window_id ||
           a.capability_generation!=b.capability_generation || a.native_source!=b.native_source ||
           a.kind!=b.kind || a.sequence!=b.sequence || a.native_thread!=b.native_thread ||
           a.native_time!=b.native_time || a.callback_qpc!=b.callback_qpc ||
           a.ctrl.available!=b.ctrl.available || a.ctrl.ctrl!=b.ctrl.ctrl ||
           a.ctrl.left!=b.ctrl.left || a.ctrl.right!=b.ctrl.right)return false;
    }
    return true;
}
// A product Ctrl START uses the adapter's verified DOWN-time three-member
// capture, never the already-advanced geometry captured after draining a batch
// containing START + LOCATION. This check does not itself prove DOWN ownership.
[[nodiscard]] inline bool product_ctrl_start_baseline_matches(
    const detail::GroupSnapshots& down, const detail::GroupSnapshots& now,
    std::size_t source) noexcept {
    if(source>=3)return false;
    try {
        for(std::size_t i=0;i<3;++i) {
            auto old=down[i],current=now[i];
            old.visible_rect=current.visible_rect={};
            old.positioning_rect=current.positioning_rect={};
            if(old!=current)return false;
            if(i!=source) {
                if(down[i].visible_rect!=now[i].visible_rect ||
                   down[i].positioning_rect!=now[i].positioning_rect)return false;
                continue;
            }
            const auto visible=core::movement::classify_geometry_change(
                down[i].visible_rect,now[i].visible_rect);
            const auto positioning=core::movement::classify_geometry_change(
                down[i].positioning_rect,now[i].positioning_rect);
            if(visible.kind==core::movement::GeometryChangeKind::ResizeOrMixed ||
               positioning.kind==core::movement::GeometryChangeKind::ResizeOrMixed)return false;
            if(core::movement::detail::checked_extent(now[i].visible_rect.left(),down[i].visible_rect.left())!=
                   core::movement::detail::checked_extent(now[i].positioning_rect.left(),down[i].positioning_rect.left()) ||
               core::movement::detail::checked_extent(now[i].visible_rect.top(),down[i].visible_rect.top())!=
                   core::movement::detail::checked_extent(now[i].positioning_rect.top(),down[i].positioning_rect.top()))return false;
        }
        return true;
    } catch(const std::exception&) {return false;}
}
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
    // Product mode shares the exact three-member capability and event source,
    // but does not require the old UAT's accepted, connected restore layout.
    bool start_product_monitoring();
    // The controller must classify the observed START before permitting any
    // Ctrl follower placement in this batch. Exactly one drained batch may
    // await processing; a second drain cannot replace it.
    [[nodiscard]] std::optional<std::vector<GroupEventReceipt>> drain_product_events();
    bool process_product_events(std::span<const GroupEventReceipt> events,
                                std::optional<std::size_t> ctrl_glue_member,
                                const detail::GroupSnapshots* verified_down_baseline=nullptr);
    // Compatibility convenience only; the MVP controller uses the split API.
    [[nodiscard]] std::optional<std::vector<GroupEventReceipt>> pump_product_events(
        std::optional<std::size_t> ctrl_glue_member,
        const detail::GroupSnapshots* verified_down_baseline=nullptr);
    [[nodiscard]] std::optional<GroupProductStatus> refresh_product_status();
    bool stop_product_monitoring() noexcept;
    [[nodiscard]] bool product_running() const noexcept {return product_mode_ && source_ && source_->facts().running;}
    // Owner-STA-only. A queued other-member LOCATION or any lifecycle receipt
    // is a conflict for a source-only product placement.
    [[nodiscard]] bool product_move_conflict_pending(std::size_t source) const noexcept;
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
    bool quantum(std::span<const GroupEventReceipt>,const detail::GroupSnapshots* validated_sample=nullptr,
                 std::optional<std::size_t> product_ctrl_glue_member=std::nullopt,
                 const detail::GroupSnapshots* product_down_baseline=nullptr);
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
    std::vector<GroupEventReceipt> product_pending_events_;
    bool product_batch_pending_{};
    std::optional<std::size_t> active_member_,plain_member_;
    DWORD start_native_time_{};
    std::uint64_t native_generation_{};
    bool setup_done_{},restored_{},poisoned_{},product_mode_{};
    std::string_view reason_{"none"};
    friend class ExplorerLiveMagnetSession;
    friend class ExplorerMvpSession;
};
} // namespace panebind::platform::windows::explorer
