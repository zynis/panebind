#include "core/v1/timing_histogram.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace v=panebind::core::v1;
int failures{},checks{};
void expect(bool value,const char* message){++checks;if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
int main(){
    v::TimingHistogram empty;
    expect(!empty.snapshot().percentile_upper_us(99),"empty observations do not invent p99 zero");
    expect(!empty.snapshot().percentile_upper_us(0)&&!empty.snapshot().percentile_upper_us(101),"percentile range checked");
    for(std::size_t i=0;i<v::timing_upper_us.size();++i){
        v::TimingHistogram exact;exact.observe_us(v::timing_upper_us[i]);
        const auto actual=exact.snapshot();
        expect(actual.samples==1&&actual.buckets[i]==1&&actual.percentile_upper_us(99)==v::timing_upper_us[i],"inclusive exact bucket boundary");
        v::TimingHistogram after;after.observe_us(v::timing_upper_us[i]+1);
        const auto next=after.snapshot();
        expect(next.samples==1&&next.buckets[i+1]==1,"boundary plus one advances exactly one bucket");
    }
    v::TimingHistogram overflow;overflow.observe_us(30000001);
    expect(overflow.snapshot().samples==1&&!overflow.snapshot().percentile_upper_us(95),"over-range sample retained without finite percentile fiction");
    expect(overflow.snapshot().maximum_us==30000001,"over-range maximum retained");
    v::TimingHistogram distribution;
    for(unsigned i=0;i<95;++i)distribution.observe_us(20);
    for(unsigned i=0;i<4;++i)distribution.observe_us(500);
    distribution.observe_us(1000);
    expect(distribution.snapshot().percentile_upper_us(95)==20,"p95 nearest rank independent expected");
    expect(distribution.snapshot().percentile_upper_us(99)==500,"p99 nearest rank independent expected");
    expect(distribution.snapshot().percentile_upper_us(100)==1000,"p100 includes actual final bucket");
    v::TimingHistogram qpc;
    expect(qpc.observe_qpc(100,100,10000000),"zero duration valid when both actual stamps present");
    expect(qpc.observe_qpc(100,101,10000000)&&qpc.snapshot().buckets[1]==1,"fractional microsecond rounded conservatively upward");
    expect(qpc.observe_qpc(100,10100,10000000)&&qpc.snapshot().buckets[11]==1,"one millisecond QPC conversion");
    expect(qpc.observe_qpc(100,10101,10000000)&&qpc.snapshot().buckets[12]==1,"over one millisecond not rounded down into passing bucket");
    expect(!qpc.observe_qpc(0,100,10000000)&&!qpc.observe_qpc(100,0,10000000)&&!qpc.observe_qpc(100,101,0),"missing clocks discarded not zero-latency samples");
    expect(!qpc.observe_qpc(101,100,10000000),"reversed cross-thread stamps rejected not unsigned wrapped");
    expect(!qpc.observe_qpc(1,std::numeric_limits<std::uint64_t>::max(),1),"conversion overflow safely discarded");
    const auto result=qpc.snapshot();
    expect(result.samples==4&&result.discarded==5&&result.missing_clock==3&&result.reversed_clock==1&&result.conversion_overflow==1,"valid and reasoned discard accounting");
    expect(result.maximum_us==1001,"maximum uses conservative converted duration");
    expect(qpc.snapshot().samples==result.samples,"snapshot is read-only and restart need not clear process statistics");
    v::TimingSnapshot saturated;saturated.samples=1;saturated.buckets[0]=1;saturated.saturated=true;
    expect(!saturated.percentile_upper_us(95),"saturated counters cannot assert percentile");
    v::PlacementTiming placements;
    placements.observe_native(1,true,{100,std::nullopt},200,10000000);
    expect(placements.hook_snapshot().samples==1&&placements.legacy_dispatch_snapshot().samples==0,
        "hook-only native observation does not invent a legacy sample");
    placements.observe_native(2,true,{0,300},500,10000000);
    expect(placements.hook_snapshot().samples==1&&placements.hook_snapshot().missing_clock==1&&
        placements.legacy_dispatch_snapshot().samples==1&&placements.legacy_dispatch_snapshot().maximum_us==20,
        "actual legacy dispatch has an independent duration and preserves missing hook origin");
    placements.observe_native(3,true,{0,std::uint64_t{0}},600,10000000);
    placements.observe_native(4,true,{0,std::nullopt},600,10000000);
    expect(placements.hook_snapshot().missing_clock==3&&placements.legacy_dispatch_snapshot().samples==1&&
        placements.legacy_dispatch_snapshot().missing_clock==1,
        "applicable legacy zero is missing while public no-hook input is outside the legacy domain");
    placements.observe_native(5,false,{0,700},0,10000000);
    expect(placements.unfinished_calls()==1&&placements.unfinished_legacy_dispatch_calls()==1&&
        placements.legacy_dispatch_snapshot().samples==1,"issued unknown completion is not a zero-duration sample");
    const auto still_unknown=placements.observe_delayed(5,true,false,900,10000000);
    expect(still_unknown.legacy_up_dispatch_qpc==700&&placements.unfinished_calls()==1&&
        placements.legacy_dispatch_snapshot().samples==1,"a delayed observation without completion retains its original clock and unknown call");
    const auto completed=placements.observe_delayed(5,true,true,1000,10000000);
    expect(completed.hook_observed_qpc==0&&completed.legacy_up_dispatch_qpc==700&&
        placements.unfinished_calls()==0&&placements.unfinished_legacy_dispatch_calls()==0&&
        placements.hook_snapshot().missing_clock==4&&placements.legacy_dispatch_snapshot().samples==2&&
        placements.legacy_dispatch_snapshot().maximum_us==30,"final delayed completion uses the issued legacy origin exactly once");
    placements.observe_native(6,false,{1100,std::nullopt},0,10000000);
    placements.observe_native(7,false,{0,std::uint64_t{0}},0,10000000);
    expect(placements.unfinished_calls()==2&&placements.unfinished_legacy_dispatch_calls()==1&&
        placements.hook_snapshot().samples==1&&placements.legacy_dispatch_snapshot().samples==2,
        "final unfinished hook and applicable-missing legacy calls remain unknown without fabricated samples");
    const auto unissued=placements.observe_delayed(8,false,true,1200,10000000);
    expect(!unissued.hook_observed_qpc&&!unissued.legacy_up_dispatch_qpc&&
        placements.hook_snapshot().samples==1&&placements.hook_snapshot().missing_clock==4&&
        placements.legacy_dispatch_snapshot().samples==2&&placements.legacy_dispatch_snapshot().missing_clock==1&&
        placements.unfinished_calls()==2,
        "unissued delayed admission does not invent a native sample or a missing clock");
    const auto contradictory=placements.observe_delayed(7,false,true,1300,10000000);
    expect(contradictory.legacy_up_dispatch_qpc==std::uint64_t{0}&&
        placements.unfinished_calls()==2&&placements.unfinished_legacy_dispatch_calls()==1&&
        placements.hook_snapshot().missing_clock==4&&placements.legacy_dispatch_snapshot().missing_clock==1,
        "unissued delayed fact cannot erase an already-issued unknown or its applicable-missing origin");
    const auto resolved=placements.observe_delayed(7,true,true,1400,10000000);
    expect(resolved.legacy_up_dispatch_qpc==std::uint64_t{0}&&
        placements.unfinished_calls()==1&&placements.unfinished_legacy_dispatch_calls()==0&&
        placements.hook_snapshot().missing_clock==5&&placements.legacy_dispatch_snapshot().missing_clock==2&&
        placements.hook_snapshot().samples==1&&placements.legacy_dispatch_snapshot().samples==2,
        "subsequent actual issued completion consumes its retained origin exactly once");
    if(failures)return EXIT_FAILURE;
    std::cout<<"V1 timing "<<checks<<" checks passed\n";return EXIT_SUCCESS;
}
