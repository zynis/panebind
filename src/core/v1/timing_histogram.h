#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>

namespace panebind::core::v1 {

// Fixed inclusive upper bounds in microseconds, plus one >30-second bucket.
// Percentiles are conservative BUCKET UPPER BOUNDS, not exact observations.
// No sample buffer grows with event rate; no timing result controls behavior.
inline constexpr std::array<std::uint64_t, 25> timing_upper_us{
    0,1,2,5,10,20,50,100,200,500,999,1000,2000,5000,10000,20000,
    49999,50000,99999,100000,200000,500000,1000000,5000000,30000000};

struct TimingSnapshot {
    std::array<std::uint64_t, timing_upper_us.size()+1> buckets{};
    std::uint64_t samples{}, discarded{}, missing_clock{}, reversed_clock{}, conversion_overflow{};
    std::uint64_t maximum_us{};
    bool saturated{};

    [[nodiscard]] std::optional<std::uint64_t> percentile_upper_us(unsigned percentile) const noexcept {
        if(!samples||!percentile||percentile>100||saturated)return std::nullopt;
        // ceil(samples * percentile / 100), without multiplying a large total.
        const auto rank=(samples/100)*percentile+((samples%100)*percentile+99)/100;
        std::uint64_t cumulative{};
        for(std::size_t i=0;i<buckets.size();++i){
            if(buckets[i]>std::numeric_limits<std::uint64_t>::max()-cumulative)return std::nullopt;
            cumulative+=buckets[i];
            if(cumulative>=rank)return i<timing_upper_us.size()?
                std::optional<std::uint64_t>{timing_upper_us[i]}:std::nullopt;
        }
        return std::nullopt;
    }
};

// Exactly ONE producer per instance, any number of snapshot readers. Relaxed
// atomics keep live snapshots data-race-free without a producer-side lock/CAS
// retry. A live snapshot is approximate; join the producer before final output.
// Saturation is explicit and disables percentile claims rather than wrapping.
class TimingHistogram final {
public:
    bool observe_qpc(std::uint64_t begin,std::uint64_t end,std::uint64_t frequency) noexcept {
        if(!begin||!end||!frequency){increment(missing_clock_);return false;}
        if(end<begin){increment(reversed_clock_);return false;}
        constexpr std::uint64_t scale=1000000;
        constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
        const auto ticks=end-begin,whole=ticks/frequency,remainder=ticks%frequency;
        if(whole>maximum/scale||remainder>maximum/scale){increment(conversion_overflow_);return false;}
        const auto numerator=remainder*scale;
        const auto fraction=numerator/frequency+(numerator%frequency?1u:0u);
        const auto base=whole*scale;
        if(fraction>maximum-base){increment(conversion_overflow_);return false;}
        observe_us(base+fraction);return true;
    }

    void observe_us(std::uint64_t microseconds) noexcept {
        std::size_t bucket{};
        while(bucket<timing_upper_us.size()&&microseconds>timing_upper_us[bucket])++bucket;
        increment(buckets_[bucket]);
        if(microseconds>maximum_us_.load(std::memory_order_relaxed))maximum_us_.store(microseconds,std::memory_order_relaxed);
    }

    [[nodiscard]] TimingSnapshot snapshot() const noexcept {
        TimingSnapshot result;
        for(std::size_t i=0;i<buckets_.size();++i){
            result.buckets[i]=buckets_[i].load(std::memory_order_relaxed);
            add(result.samples,result.buckets[i],result.saturated);
        }
        result.missing_clock=missing_clock_.load(std::memory_order_relaxed);
        result.reversed_clock=reversed_clock_.load(std::memory_order_relaxed);
        result.conversion_overflow=conversion_overflow_.load(std::memory_order_relaxed);
        add(result.discarded,result.missing_clock,result.saturated);
        add(result.discarded,result.reversed_clock,result.saturated);
        add(result.discarded,result.conversion_overflow,result.saturated);
        result.maximum_us=maximum_us_.load(std::memory_order_relaxed);
        result.saturated=result.saturated||saturated_.load(std::memory_order_relaxed);
        return result;
    }
private:
    static void add(std::uint64_t& total,std::uint64_t value,bool& saturated) noexcept {
        constexpr auto maximum=std::numeric_limits<std::uint64_t>::max();
        if(value>maximum-total){total=maximum;saturated=true;}else total+=value;
    }
    void increment(std::atomic<std::uint64_t>& counter) noexcept {
        const auto old=counter.load(std::memory_order_relaxed);
        if(old==std::numeric_limits<std::uint64_t>::max())saturated_.store(true,std::memory_order_relaxed);
        else counter.store(old+1,std::memory_order_relaxed);
    }
    std::array<std::atomic<std::uint64_t>,timing_upper_us.size()+1> buckets_{};
    std::atomic<std::uint64_t> missing_clock_{},reversed_clock_{},conversion_overflow_{},maximum_us_{};
    std::atomic<bool> saturated_{};
};

// These are distinct observation domains, not interchangeable input origins.
// A legacy dispatch is a queued UP handler entry, never a hook/physical clock.
// nullopt means this native placement is not in that domain; an engaged zero
// means it is applicable but its actual clock is missing.
struct PlacementTimingOrigins {
    std::uint64_t hook_observed_qpc{};
    std::optional<std::uint64_t> legacy_up_dispatch_qpc;
};

// GUI-owner accounting for the existing immediate/issued/delayed lifecycle.
// No clock, completion, or histogram result grants writer authority.
class PlacementTiming final {
public:
    void observe_native(std::uint64_t call_id,bool completion_known,PlacementTimingOrigins origins,
                        std::uint64_t post_qpc,std::uint64_t frequency) {
        if(completion_known)observe(origins,post_qpc,frequency);
        else pending_[call_id]=origins;
    }
    PlacementTimingOrigins observe_delayed(std::uint64_t call_id,bool completion_known,
                                          std::uint64_t post_qpc,std::uint64_t frequency) {
        const auto found=pending_.find(call_id);
        const auto origins=found==pending_.end()?PlacementTimingOrigins{}:found->second;
        if(completion_known){
            observe(origins,post_qpc,frequency);
            if(found!=pending_.end())pending_.erase(found);
        }
        return origins;
    }
    [[nodiscard]] TimingSnapshot hook_snapshot()const noexcept{return hook_.snapshot();}
    [[nodiscard]] TimingSnapshot legacy_dispatch_snapshot()const noexcept{return legacy_dispatch_.snapshot();}
    [[nodiscard]] std::size_t unfinished_calls()const noexcept{return pending_.size();}
    [[nodiscard]] std::size_t unfinished_legacy_dispatch_calls()const noexcept {
        std::size_t count{};for(const auto& item:pending_)if(item.second.legacy_up_dispatch_qpc.has_value())++count;
        return count;
    }
private:
    void observe(const PlacementTimingOrigins& origins,std::uint64_t post_qpc,std::uint64_t frequency)noexcept {
        // Preserve the old all-native hook missing-clock population. A legacy
        // clock does not fill its zero or alter its percentile denominator.
        hook_.observe_qpc(origins.hook_observed_qpc,post_qpc,frequency);
        if(origins.legacy_up_dispatch_qpc)
            legacy_dispatch_.observe_qpc(*origins.legacy_up_dispatch_qpc,post_qpc,frequency);
    }
    TimingHistogram hook_,legacy_dispatch_;
    std::map<std::uint64_t,PlacementTimingOrigins> pending_;
};

} // namespace panebind::core::v1
