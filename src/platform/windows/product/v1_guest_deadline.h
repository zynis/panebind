#pragma once

// TEST-only one-shot deadline decision. Timer signalling is not elapsed-time
// evidence; the caller supplies a fresh QPC observation after each signal.
#include <cmath>
#include <cstdint>
#include <limits>

namespace panebind::platform::windows::product::guest_test {
enum class DeadlineSignal { Reached, RearmOnce, TooEarly, SecondEarly, Invalid };
struct DeadlineSignalFact {
    DeadlineSignal decision{DeadlineSignal::Invalid};
    std::uint64_t remaining_qpc{},rearm_due_100ns{};
};
inline DeadlineSignalFact deadline_after_signal(std::uint64_t target,std::uint64_t observed,
    std::uint64_t frequency,bool already_rearmed){
    if(!target||!observed||!frequency||frequency>std::numeric_limits<std::uint64_t>::max()/64)return {};
    if(observed>=target)return {DeadlineSignal::Reached,0,0};
    const auto remaining=target-observed;
    if(already_rearmed)return {DeadlineSignal::SecondEarly,remaining,0};
    // Integer floor means a candidate genuinely exceeds 32 ms if it is one
    // tick above this bound. No second sampling loop or periodic timer.
    if(remaining>(frequency*32)/1000)return {DeadlineSignal::TooEarly,remaining,0};
    const auto until=static_cast<std::uint64_t>(std::ceil(
        static_cast<long double>(remaining)*10000000.0L/static_cast<long double>(frequency)));
    if(!until||until>320000)return {DeadlineSignal::TooEarly,remaining,0};
    return {DeadlineSignal::RearmOnce,remaining,until+320000};
}
}
