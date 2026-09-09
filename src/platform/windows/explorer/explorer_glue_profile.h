#pragma once

#include "platform/windows/explorer/explorer_glue_input.h"
#include "platform/windows/explorer/explorer_glue_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace panebind::platform::windows::explorer {

enum class GlueProfileStage : std::uint8_t {
    Quantum, Drain, LeaderCapture, FollowerCapture, FullValidation,
    TokenLedger, ShellObservation, ShellInventory, ShellLocation,
    NativeIdentity, ProcessImage, WindowStructure, WindowState, VirtualDesktop,
    ProcessSecurity, PositioningBounds, VisibleFrame, MonitorDpi, EligibilityFinalize,
    EventPolicy, ActivationPolicy, StartGeometry, CoreDecision, FeedbackPolicy,
    Operation, OperationPrepare, PrepareValidation, PositioningPlan,
    ImmediateValidation, PendingRegistration, NativePlacement, Postverify,
    PostverifyValidation, ExactComparison, ReceiptFinalize, OperationResultPolicy,
    EndLeaderCapture, EndFollowerCapture, EndReconciliation, Count
};

[[nodiscard]] constexpr std::string_view profile_stage_name(GlueProfileStage stage) noexcept {
    constexpr std::array names{
        "quantum", "drain", "leader_capture", "follower_capture", "full_validation",
        "token_ledger", "shell_observation", "shell_inventory", "shell_location",
        "native_identity", "process_image", "window_structure", "window_state", "virtual_desktop",
        "process_security", "positioning_bounds", "visible_frame", "monitor_dpi", "eligibility_finalize",
        "event_policy", "activation_policy", "start_geometry", "core_decision", "feedback_policy",
        "operation", "operation_prepare", "prepare_validation", "positioning_plan",
        "immediate_validation", "pending_registration", "native_placement", "postverify",
        "postverify_validation", "exact_comparison", "receipt_finalize", "operation_result_policy",
        "end_leader_capture", "end_follower_capture", "end_reconciliation"};
    const auto index = static_cast<std::size_t>(stage);
    return index < names.size() ? names[index] : "invalid";
}

struct GlueProfileContext {
    std::uint64_t quantum_id{};
    std::uint64_t operation_generation{};
    std::uint64_t source_receipt{};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
};
struct GlueProfilePoint { std::int64_t qpc{}; std::uint64_t receipt_watermark{}; };
struct GlueProfileQueue { std::size_t depth{}; std::size_t maximum{}; std::uint64_t latest{}; };
struct GlueProfileSpan {
    std::uint64_t id{};
    std::uint64_t parent_id{};
    GlueProfileStage stage{};
    GlueProfileContext context;
    GlueProfilePoint begin;
    GlueProfilePoint end;
};
struct GlueProfileNotification {
    std::uint64_t id{};
    std::uint64_t trigger_receipt{};
    std::int64_t callback_qpc{};
    std::int64_t post_qpc{};
    std::int64_t dispatch_qpc{}; // zero = NOT OBSERVED, not a synthetic dispatch
    bool posted{};
};
struct GlueProfileQuantum {
    std::uint64_t id{};
    std::uint64_t first_receipt{};
    std::uint64_t last_receipt{};
    std::uint64_t receipt_count{};
    std::uint64_t leader_locations{};
    std::uint64_t follower_locations{};
    std::uint64_t coalesced_leader_locations{};
    std::int64_t first_callback_qpc{};
    GlueProfilePoint begin;
    GlueProfilePoint drain_complete;
    GlueProfilePoint end;
    GlueProfileQueue queue_before;
    GlueProfileQueue queue_after_drain;
    GlueProfileQueue queue_end;
    std::uint64_t arrived_during_quantum{};
    std::uint64_t arrived_during_native{};
    std::uint64_t arrived_during_postverify{};
    std::uint64_t previous_quantum_id{};
    std::uint64_t arrived_during_previous_quantum{};
    std::uint64_t arrived_during_previous_native{};
    std::uint64_t arrived_during_previous_postverify{};
};

// Owner-affine, opt-in diagnostic storage. No operation authority. All memory
// is allocated before arming, never in a callback or active profiling scope.
class ExplorerGlueProfiler final {
public:
    static constexpr std::size_t span_capacity = 32768;
    static constexpr std::size_t quantum_capacity = 4096;
    static constexpr std::size_t notification_capacity = 4096;
    using Clock = std::int64_t (*)(void*) noexcept;
    using ReadQueue = GlueProfileQueue (*)(void*) noexcept;
    explicit ExplorerGlueProfiler(Clock clock = nullptr, void* clock_context = nullptr,
                                  std::size_t limit = span_capacity)
        : spans_(std::make_unique<GlueProfileSpan[]>(span_capacity)),
          quanta_(std::make_unique<GlueProfileQuantum[]>(quantum_capacity)),
          notifications_(std::make_unique<GlueProfileNotification[]>(notification_capacity)),
          clock_(clock), clock_context_(clock_context), limit_(limit) {
        if (limit_ == 0 || limit_ > span_capacity) { limit_ = 0; invalid_ = true; }
    }
    void bind_queue(ReadQueue read, void* context) noexcept { read_queue_ = read; queue_context_ = context; }
    [[nodiscard]] GlueProfileQueue queue() const noexcept {
        return read_queue_ ? read_queue_(queue_context_) : GlueProfileQueue{};
    }
    [[nodiscard]] GlueProfilePoint point() noexcept {
        const DWORD error = GetLastError();
        const auto tick = clock_ ? clock_(clock_context_) : glue_qpc_now();
        const auto watermark = queue().latest;
        SetLastError(error);
        ++clock_reads_;
        if (tick <= 0 || tick < last_qpc_) invalid_ = true;
        last_qpc_ = tick;
        return {tick, watermark};
    }
    [[nodiscard]] std::uint64_t begin(GlueProfileStage stage) noexcept {
        if (stage >= GlueProfileStage::Count) { invalid_ = true; return 0; }
        if (size_ == limit_ || depth_ == stack_.size()) { overflow_ = true; return 0; }
        const auto id = static_cast<std::uint64_t>(++size_);
        spans_[size_ - 1] = {id, depth_ ? stack_[depth_ - 1] : 0, stage, context_, point(), {}};
        stack_[depth_++] = id;
        return id;
    }
    void end(std::uint64_t id, GlueProfilePoint at) noexcept {
        if (!id) return;
        if (id > size_ || depth_ == 0 || stack_[depth_ - 1] != id) { invalid_ = true; return; }
        auto& span = spans_[static_cast<std::size_t>(id - 1)];
        if (at.qpc < span.begin.qpc || at.receipt_watermark < span.begin.receipt_watermark) invalid_ = true;
        span.end = at;
        --depth_;
        if (quantum_size_ && span.context.quantum_id == quanta_[quantum_size_ - 1].id) {
            const auto arrived = at.receipt_watermark >= span.begin.receipt_watermark
                ? at.receipt_watermark - span.begin.receipt_watermark : 0;
            if (span.stage == GlueProfileStage::NativePlacement) quanta_[quantum_size_ - 1].arrived_during_native += arrived;
            if (span.stage == GlueProfileStage::Postverify) quanta_[quantum_size_ - 1].arrived_during_postverify += arrived;
        }
    }
    void end(std::uint64_t id) noexcept { if (id) end(id, point()); }
    [[nodiscard]] std::size_t begin_quantum(std::uint64_t id) noexcept {
        if (!id || (quantum_size_ && id <= quanta_[quantum_size_ - 1].id)) invalid_ = true;
        if (quantum_size_ == quantum_capacity) { overflow_ = true; return quantum_capacity; }
        const auto index = quantum_size_++;
        auto& q = quanta_[index];
        q.id = id; q.begin = point(); q.queue_before = queue();
        if (index) {
            const auto& previous = quanta_[index - 1];
            q.previous_quantum_id = previous.id;
            q.arrived_during_previous_quantum = previous.arrived_during_quantum;
            q.arrived_during_previous_native = previous.arrived_during_native;
            q.arrived_during_previous_postverify = previous.arrived_during_postverify;
        }
        return index;
    }
    [[nodiscard]] GlueProfileQuantum* quantum(std::size_t index) noexcept {
        return index < quantum_size_ ? &quanta_[index] : nullptr;
    }
    void end_quantum(std::size_t index) noexcept {
        if (auto* q = quantum(index)) {
            q->end = point(); q->queue_end = queue();
            if (q->end.receipt_watermark < q->begin.receipt_watermark) invalid_ = true;
            else q->arrived_during_quantum = q->end.receipt_watermark - q->begin.receipt_watermark;
        }
    }
    [[nodiscard]] std::uint64_t notification(std::uint64_t trigger, std::int64_t callback_qpc) noexcept {
        if (notification_size_ == notification_capacity) { overflow_ = true; return 0; }
        const auto id = static_cast<std::uint64_t>(++notification_size_);
        notifications_[notification_size_ - 1] = {id, trigger, callback_qpc, point().qpc, 0, false};
        if (!trigger || callback_qpc <= 0 || notifications_[notification_size_ - 1].post_qpc < callback_qpc) invalid_ = true;
        return id;
    }
    void posted(std::uint64_t id, bool succeeded) noexcept {
        if (!succeeded) invalid_ = true;
        if (id && id <= notification_size_) notifications_[static_cast<std::size_t>(id - 1)].posted = succeeded;
    }
    void dispatched(std::uint64_t id) noexcept {
        if (!id || id > notification_size_) { invalid_ = true; return; }
        auto& n = notifications_[static_cast<std::size_t>(id - 1)];
        if (n.dispatch_qpc || !n.posted) invalid_ = true;
        n.dispatch_qpc = point().qpc;
        if (n.dispatch_qpc < n.post_qpc) invalid_ = true;
    }
    [[nodiscard]] GlueProfileContext context() const noexcept { return context_; }
    void context(GlueProfileContext value) noexcept { context_ = value; }
    [[nodiscard]] bool valid() const noexcept { return !invalid_ && !overflow_ && depth_ == 0; }
    [[nodiscard]] bool invalid() const noexcept { return invalid_ || depth_ != 0; }
    [[nodiscard]] bool overflow() const noexcept { return overflow_; }
    [[nodiscard]] std::uint64_t clock_reads() const noexcept { return clock_reads_; }
    [[nodiscard]] std::span<const GlueProfileSpan> spans() const noexcept { return {spans_.get(), size_}; }
    [[nodiscard]] std::span<const GlueProfileQuantum> quanta() const noexcept { return {quanta_.get(), quantum_size_}; }
    [[nodiscard]] std::span<const GlueProfileNotification> notifications() const noexcept { return {notifications_.get(), notification_size_}; }
private:
    std::unique_ptr<GlueProfileSpan[]> spans_;
    std::unique_ptr<GlueProfileQuantum[]> quanta_;
    std::unique_ptr<GlueProfileNotification[]> notifications_;
    std::array<std::uint64_t, 32> stack_{};
    Clock clock_{}; void* clock_context_{};
    ReadQueue read_queue_{}; void* queue_context_{};
    std::size_t limit_{}; std::size_t size_{}; std::size_t depth_{};
    std::size_t quantum_size_{}; std::size_t notification_size_{};
    std::int64_t last_qpc_{}; std::uint64_t clock_reads_{};
    GlueProfileContext context_;
    bool invalid_{}; bool overflow_{};
};

class GlueProfileScope final {
public:
    GlueProfileScope(ExplorerGlueProfiler* p, GlueProfileStage stage) noexcept
        : profiler_(p), id_(p ? p->begin(stage) : 0) {}
    ~GlueProfileScope() noexcept { end(); }
    GlueProfileScope(const GlueProfileScope&) = delete;
    GlueProfileScope& operator=(const GlueProfileScope&) = delete;
    void end() noexcept { if (profiler_ && id_) profiler_->end(id_); id_ = 0; }
    void next(GlueProfileStage stage) noexcept { end(); id_ = profiler_ ? profiler_->begin(stage) : 0; }
private:
    ExplorerGlueProfiler* profiler_{}; std::uint64_t id_{};
};
class GlueProfileContextScope final {
public:
    GlueProfileContextScope(ExplorerGlueProfiler* p, GlueProfileContext value) noexcept
        : profiler_(p), previous_(p ? p->context() : GlueProfileContext{}) { if (p) p->context(value); }
    ~GlueProfileContextScope() noexcept { if (profiler_) profiler_->context(previous_); }
private:
    ExplorerGlueProfiler* profiler_; GlueProfileContext previous_;
};
class GlueProfileQuantumScope final {
public:
    GlueProfileQuantumScope(ExplorerGlueProfiler* p, std::uint64_t id) noexcept
        : profiler_(p), context_(p, {id, 0, 0, ExplorerGlueWindowRole::Leader}),
          index_(p ? p->begin_quantum(id) : ExplorerGlueProfiler::quantum_capacity),
          scope_(p, GlueProfileStage::Quantum) {}
    ~GlueProfileQuantumScope() noexcept { scope_.end(); if (profiler_) profiler_->end_quantum(index_); }
    [[nodiscard]] GlueProfileQuantum* record() noexcept { return profiler_ ? profiler_->quantum(index_) : nullptr; }
private:
    ExplorerGlueProfiler* profiler_; GlueProfileContextScope context_;
    std::size_t index_; GlueProfileScope scope_;
};

} // namespace panebind::platform::windows::explorer
