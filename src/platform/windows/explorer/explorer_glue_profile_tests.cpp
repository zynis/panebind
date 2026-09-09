#include "platform/windows/explorer/explorer_glue_profile.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace e = panebind::platform::windows::explorer;
namespace {
int failures{};
void expect(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
struct Inputs { std::int64_t tick{100}; e::GlueProfileQueue queue; };
std::int64_t clock_value(void* p) noexcept { return ++static_cast<Inputs*>(p)->tick; }
e::GlueProfileQueue queue_value(void* p) noexcept { return static_cast<Inputs*>(p)->queue; }

void test_scopes_and_error_preservation() {
    Inputs inputs;
    e::ExplorerGlueProfiler profile(clock_value, &inputs);
    profile.bind_queue(queue_value, &inputs);
    SetLastError(321);
    {
        e::GlueProfileContextScope context(&profile, {2, 3, 4, e::ExplorerGlueWindowRole::Follower});
        e::GlueProfileScope operation(&profile, e::GlueProfileStage::Operation);
        {
            e::GlueProfileScope native(&profile, e::GlueProfileStage::NativePlacement);
            inputs.queue.latest = 5;
        }
        e::GlueProfileScope post(&profile, e::GlueProfileStage::Postverify);
    }
    const auto spans = profile.spans();
    expect(profile.valid() && spans.size() == 3 && profile.clock_reads() == 6,
           "nested spans are bounded and use measured monotonic endpoints");
    expect(GetLastError() == 321, "profiling preserves native LastError");
    expect(spans[1].parent_id == spans[0].id && spans[2].parent_id == spans[0].id &&
        spans[0].context.quantum_id == 2 && spans[0].context.operation_generation == 3 &&
        spans[0].context.source_receipt == 4, "stage/operation/quantum correlation");
    const auto exclusive = (spans[0].end.qpc - spans[0].begin.qpc) -
        (spans[1].end.qpc - spans[1].begin.qpc) - (spans[2].end.qpc - spans[2].begin.qpc);
    expect(exclusive == 3 && spans[1].end.receipt_watermark == 5,
        "exclusive parent duration subtracts only nonoverlapping immediate children");
    expect(profile.context().quantum_id == 0, "context restored, not leaked to later operation");
}
void test_failure_is_diagnostic() {
    Inputs inputs;
    e::ExplorerGlueProfiler profile(clock_value, &inputs, 1);
    {
        e::GlueProfileScope parent(&profile, e::GlueProfileStage::Operation);
        e::GlueProfileScope overflow(&profile, e::GlueProfileStage::NativePlacement);
    }
    expect(profile.overflow() && !profile.valid() && profile.spans().size() == 1,
        "overflow cannot silently overwrite or claim timing PASS");
    e::ExplorerGlueProfiler backwards(clock_value, &inputs);
    {
        e::GlueProfileScope operation(&backwards, e::GlueProfileStage::Operation);
        inputs.tick = -10;
    }
    expect(backwards.invalid() && !backwards.valid(), "negative/backwards QPC invalidates profile");
    inputs.tick = 100;
    e::ExplorerGlueProfiler nesting(clock_value, &inputs);
    const auto outer = nesting.begin(e::GlueProfileStage::Operation);
    const auto inner = nesting.begin(e::GlueProfileStage::NativePlacement);
    nesting.end(outer);
    nesting.end(inner);
    nesting.end(outer);
    expect(nesting.invalid(), "out-of-order scope close remains invalid");
    e::GlueProfileScope disabled(nullptr, e::GlueProfileStage::Quantum);
    disabled.next(e::GlueProfileStage::NativePlacement);
    disabled.end();
    expect(!profile.spans().empty(), "disabled scope needs no profiler or buffer");
}
void test_quantum_backlog_and_notifications() {
    Inputs inputs;
    inputs.queue = {3, 3, 3};
    e::ExplorerGlueProfiler profile(clock_value, &inputs);
    profile.bind_queue(queue_value, &inputs);
    const auto n = profile.notification(1, 99);
    profile.posted(n, true);
    {
        e::GlueProfileQuantumScope quantum(&profile, 1);
        auto* q = quantum.record();
        q->first_receipt = 1; q->last_receipt = 3; q->receipt_count = 3;
        q->leader_locations = 2; q->coalesced_leader_locations = 1;
        inputs.queue.depth = 0;
        q->drain_complete = profile.point(); q->queue_after_drain = profile.queue();
        {
            e::GlueProfileScope native(&profile, e::GlueProfileStage::NativePlacement);
            inputs.queue = {2, 3, 5};
        }
        {
            e::GlueProfileScope post(&profile, e::GlueProfileStage::Postverify);
            inputs.queue = {5, 5, 8};
        }
    }
    {
        e::GlueProfileQuantumScope quantum(&profile, 2);
        const auto* q = quantum.record();
        expect(q->queue_before.depth == 5 && q->previous_quantum_id == 1 &&
            q->arrived_during_previous_quantum == 5 &&
            q->arrived_during_previous_native == 2 &&
            q->arrived_during_previous_postverify == 3,
            "backlog by delivered watermarks, native and postverify intervals distinct");
    }
    expect(profile.notifications().size() == 1 && profile.notifications()[0].dispatch_qpc == 0,
        "direct drain does not invent a private-message dispatch");
    profile.dispatched(n);
    expect(profile.valid() && profile.notifications()[0].dispatch_qpc > profile.notifications()[0].post_qpc,
        "actual later dispatch correlates to original notification");
    profile.dispatched(n);
    expect(!profile.valid(), "duplicate dispatch is invalid profiling evidence");
}
void report_perturbation() {
    constexpr std::size_t iterations = 2000;
    std::atomic<std::uint64_t> work{};
    const auto run = [&](e::ExplorerGlueProfiler* p) {
        const auto begin = e::glue_qpc_now();
        for (std::size_t i = 0; i < iterations; ++i) {
            e::GlueProfileScope scope(p, e::GlueProfileStage::Operation);
            e::GlueProfileScope stages(p, e::GlueProfileStage::TokenLedger);
            work.fetch_add(1, std::memory_order_relaxed);
            stages.next(e::GlueProfileStage::NativeIdentity);
            work.fetch_add(1, std::memory_order_relaxed);
            stages.next(e::GlueProfileStage::ReceiptFinalize);
        }
        return e::glue_qpc_now() - begin;
    };
    const auto off = run(nullptr);
    e::ExplorerGlueProfiler profile;
    const auto on = run(&profile);
    expect(profile.valid() && work == iterations * 4 && profile.spans().size() == iterations * 4,
        "same deterministic work/output with profiling OFF and ON; no timing SLA");
    std::cout << "profiling_perturbation iterations=" << iterations << " off_ticks=" << off
        << " on_ticks=" << on << " qpc_frequency=" << e::glue_qpc_frequency()
        << " profile_qpc_reads=" << profile.clock_reads()
        << " scope=CPU_fixture_only real_Explorer=NOT_TESTED\n";
}
}
int main() {
    test_scopes_and_error_preservation();
    test_failure_is_diagnostic();
    test_quantum_backlog_and_notifications();
    report_perturbation();
    std::cout << "Glue profile tests " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
