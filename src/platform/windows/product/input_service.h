#pragma once
#include "window_catalog.h"
#include "core/v1/timing_histogram.h"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace panebind::platform::windows::product {
constexpr UINT input_message=WM_APP+41, prepare_message=WM_APP+42, catalog_message=WM_APP+43;
struct Prepared {
    Snapshot source;
    POINT point{};
    DWORD stamp{};
    int region{-1};
    std::uint64_t config_generation{};
    // Prepared on the GUI owner, only for this exact hit-test region.
    // Index: (button-1)*32 + (clicks-1)*16 + locked modifiers.
    std::array<core::v1::Action,160> mappings{};
};
enum class DownRoute { Pass, Gesture, Mapped };
struct DownDecision { DownRoute route{DownRoute::Pass}; core::v1::Action action{core::v1::Action::None}; };
inline void prepare_mappings(Prepared& prepared,const core::v1::Settings& settings) {
    prepared.mappings.fill(core::v1::Action::None);
    for(const auto& mapping:settings.mouse_bindings)
        if(mapping.region==prepared.region&&mapping.button>=1&&mapping.button<=5&&
            mapping.clicks>=1&&mapping.clicks<=2&&mapping.modifiers<=15)
            prepared.mappings[static_cast<std::size_t>((mapping.button-1)*32+(mapping.clicks-1)*16)+mapping.modifiers]=mapping.action;
}
// The callback uses only this bounded precomputed table; no configuration
// searches or native queries are hidden in the routing predicate.
inline DownDecision route_down(const Prepared& prepared,int button,int clicks,unsigned mods) {
    if(prepared.region<0||prepared.region>8||button<1||button>5||clicks<1||clicks>2||mods>15)return {};
    const auto action=prepared.mappings[static_cast<std::size_t>((button-1)*32+(clicks-1)*16)+mods];
    if(action!=core::v1::Action::None)return {DownRoute::Mapped,action};
    // Shift bypasses enhanced dragging, not an explicitly configured Shift
    // double-click. Unmapped extra buttons and native double-clicks pass.
    if(button!=1||(mods&8u))return {};
    // Corners have a contextual two-edge stretch route; they are not mapped
    // to StretchAll (which would also alter the two opposite edges).
    if(clicks==2&&prepared.region>=5)return {DownRoute::Gesture,core::v1::Action::None};
    if((mods&4u)||clicks==2)return {};
    return {DownRoute::Gesture,core::v1::Action::None};
}
// A press that became a drag cannot become the first click of a double-click,
// even if the cursor subsequently returns to its DOWN point. Track native
// passthrough presses too; consumption is unrelated to click eligibility.
struct ClickCandidate {
    POINT anchor{};
    int button{};
    bool eligible{},pressing{};
    void begin(POINT point,int pressed_button,bool candidate){
        anchor=point;button=pressed_button;eligible=candidate;pressing=true;
    }
    void motion(POINT point,std::int64_t drag_x,std::int64_t drag_y){
        if(!eligible||!pressing)return;
        const auto dx=static_cast<std::int64_t>(point.x)-anchor.x;
        const auto dy=static_cast<std::int64_t>(point.y)-anchor.y;
        if(drag_x<0||drag_y<0||dx < -drag_x||dx > drag_x||dy < -drag_y||dy > drag_y)eligible=false;
    }
    void release(int released_button){if(pressing&&released_button==button)pressing=false;}
};
inline bool native_event_is_current(DWORD event_stamp,DWORD down_stamp) {
    return static_cast<DWORD>(event_stamp-down_stamp)<0x80000000u;
}
// A new actual DOWN is a resynchronization boundary only for an already
// aborted, fully revoked pair. It is not a substitute UP or renewed authority.
inline bool retire_aborted_pair_for_down(bool consumed,bool aborted,bool authority_retired,
                                        int paired_button,int incoming_button,DWORD old_down,DWORD new_down){
    return consumed&&aborted&&authority_retired&&paired_button>=1&&paired_button<=5&&
        incoming_button==paired_button&&native_event_is_current(new_down,old_down);
}
inline bool button_up_matches_current_pair(bool consumed,int paired_button,int incoming_button,DWORD down_stamp,DWORD up_stamp){
    return consumed&&paired_button>=1&&paired_button<=5&&incoming_button==paired_button&&
        native_event_is_current(up_stamp,down_stamp);
}
inline bool operation_permission(std::uint64_t generation,std::uint64_t active,std::uint64_t normal_up,
                                 bool available,bool allow_completion){
    return available&&(!generation||active==generation||(allow_completion&&normal_up==generation));
}
// Actual matching-UP decision used by the callback. Native foreground events
// may still be queued: current facts, not their eventual delivery, determine
// whether this generation can receive a NormalUp completion capability.
inline core::v1::End matching_up_end(bool aborted,core::v1::End observed_end,
                                   bool source_current,bool foreground_matches){
    if(observed_end!=core::v1::End::NormalUp)return observed_end;
    if(!source_current||aborted)return core::v1::End::SourceLost;
    if(!foreground_matches)return core::v1::End::FocusLost;
    return core::v1::End::NormalUp;
}
enum class InputKind { Down, Move, Up, Abort, Mapped };
struct InputEvent {
    InputKind kind{};
    POINT point{};
    std::uint64_t generation{}, tick{};
    unsigned modifiers{};
    int button{1}, clicks{1};
    core::v1::End end{core::v1::End::NormalUp};
    std::shared_ptr<const Prepared> prepared;
    core::v1::Action action{core::v1::Action::None};
    // Actual QPC at the beginning of this mouse-hook observation. Zero means
    // no such observation (e.g. WinEvent abort), never an invented timestamp.
    std::uint64_t hook_observed_qpc{};
};
struct MouseUpCallbackSnapshot {
    bool known{};
    std::uint64_t callback_count{},hook_observed_qpc{},generation_at_entry{};
    int button{};
    bool consumed_before{},aborted_before{},matching_branch_taken{},push_attempted{},push_succeeded{};
};
// One hook owner publishes; the GUI makes at most three coherent reads. Every
// payload word is atomic too: an inconsistent observation is UNKNOWN, never a
// mixed packet or a C++ data race. There are no waits/retries on the publisher.
class MouseUpCallbackObservation {
public:
    static constexpr bool coherent(std::uint64_t before,std::uint64_t after){return !(before&1u)&&before==after;}
    void publish(const MouseUpCallbackSnapshot& value){
        sequence_.fetch_add(1);
        count_.store(value.callback_count);qpc_.store(value.hook_observed_qpc);generation_.store(value.generation_at_entry);
        const auto flags=static_cast<std::uint64_t>(value.button)&255u;
        details_.store(flags|(static_cast<std::uint64_t>(value.consumed_before)<<8)|
            (static_cast<std::uint64_t>(value.aborted_before)<<9)|(static_cast<std::uint64_t>(value.matching_branch_taken)<<10)|
            (static_cast<std::uint64_t>(value.push_attempted)<<11)|(static_cast<std::uint64_t>(value.push_succeeded)<<12));
        sequence_.fetch_add(1);
    }
    MouseUpCallbackSnapshot snapshot()const{
        for(unsigned attempt=0;attempt<3;++attempt){
            const auto before=sequence_.load();if(before&1u)continue;
            MouseUpCallbackSnapshot out;out.callback_count=count_.load();out.hook_observed_qpc=qpc_.load();
            out.generation_at_entry=generation_.load();const auto flags=details_.load();
            if(!coherent(before,sequence_.load()))continue;
            out.known=true;out.button=static_cast<int>(flags&255u);out.consumed_before=(flags&(1u<<8))!=0;
            out.aborted_before=(flags&(1u<<9))!=0;out.matching_branch_taken=(flags&(1u<<10))!=0;
            out.push_attempted=(flags&(1u<<11))!=0;out.push_succeeded=(flags&(1u<<12))!=0;return out;
        }
        return {};
    }
private:
    // Sequentially consistent operations retain a single order across the
    // sequence and payload words; a relaxed multiword read is not substituted.
    std::atomic<std::uint64_t> sequence_{},count_{},qpc_{},generation_{},details_{};
};
// Dedicated message owner. No COM, file IO, cross-process messaging or geometry
// in hook callbacks. The GUI prepares identity/hit-test snapshots beforehand.
class InputService {
public:
    bool start(HWND owner);
    void stop();
    ~InputService(){stop();}
    void prepare(std::shared_ptr<const Prepared> value){prepared_.store(std::move(value));}
    bool pop(InputEvent&);
    POINT latest()const;
    void prepared_point(){prepare_pending_=false;}
    void pause(bool value);
    bool allowed(std::uint64_t g)const {return g && authorized_->load()==g && !paused_;}
    void revoke(core::v1::End reason=core::v1::End::SourceLost){
        if(reason!=core::v1::End::NormalUp)*completion_=0;
        const auto retired=authorized_->exchange(0);const auto id=thread_id_.load();
        if(retired&&id)PostThreadMessageW(id,WM_APP+2,static_cast<WPARAM>(retired),static_cast<LPARAM>(reason));
    }
    // Safe to retain at a bounded native worker boundary, even if the UI
    // retires/destroys this service. Only shared atomics are captured.
    void finish_completion(std::uint64_t generation){completion_->compare_exchange_strong(generation,0);}
    std::function<bool()> operation_guard(std::uint64_t generation,bool normal_up_completion=false,HWND source=nullptr)const {
        return [authority=authorized_,completion=completion_,available=available_,generation,normal_up_completion,source]{
            return (!generation||(source&&GetForegroundWindow()==source))&&
                operation_permission(generation,authority->load(),completion->load(),available->load(),normal_up_completion);
        };
    }
    bool healthy()const {return healthy_;}
    std::uint64_t max_callback_ticks()const{return max_ticks_;}
    // Join stop() before final reporting. A live relaxed snapshot is bounded
    // and race-free but not an atomic view of simultaneous callback updates.
    core::v1::TimingSnapshot callback_timing()const{return callback_timing_.snapshot();}
    std::uint64_t counter_frequency()const{return counter_frequency_;}
    std::uint64_t emergency_hotkey_messages()const{return emergency_hotkey_messages_;}
    MouseUpCallbackSnapshot mouse_up_callbacks()const{return up_observation_.snapshot();}
private:
    static LRESULT CALLBACK mouse(int,WPARAM,LPARAM);
    static void CALLBACK event(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD);
    static LRESULT CALLBACK receiver(HWND,UINT,WPARAM,LPARAM);
    void run();
    bool push(InputEvent);
    void abort(core::v1::End);
    static InputService* instance_;
    HWND owner_{},receiver_{};
    std::atomic<DWORD> thread_id_{};
    HANDLE ready_{};
    std::thread thread_;
    std::atomic<bool> paused_{false},healthy_{false},prepare_pending_{false},stop_requested_{false};
    std::shared_ptr<std::atomic<std::uint64_t>> authorized_{std::make_shared<std::atomic<std::uint64_t>>(0)};
    std::shared_ptr<std::atomic<std::uint64_t>> completion_{std::make_shared<std::atomic<std::uint64_t>>(0)};
    std::shared_ptr<std::atomic<bool>> available_{std::make_shared<std::atomic<bool>>(false)};
    std::atomic<std::uint64_t> point_{0},max_ticks_{0},emergency_hotkey_messages_{0};
    std::uint64_t counter_frequency_{};
    std::uint64_t up_callback_count_{};MouseUpCallbackObservation up_observation_;
    core::v1::TimingHistogram callback_timing_;
    std::atomic<std::shared_ptr<const Prepared>> prepared_;
    std::array<InputEvent,256> queue_{};
    std::atomic<unsigned> read_{0},write_{0};
    bool consumed_down_{};
    bool aborted_{};
    int button_{};
    std::uint64_t generation_{},next_generation_{};
    unsigned modifiers_{};
    DWORD last_down_{};
    DWORD active_down_stamp_{};
    int last_region_{-1};
    int last_button_{};
    unsigned last_modifiers_{};
    std::uint64_t last_source_key_{},deadline_{};
    ClickCandidate click_candidate_;
    UINT double_click_time_{};
    int double_click_x_{},double_click_y_{};
    std::int64_t drag_x_{},drag_y_{};
    std::shared_ptr<const Prepared> active_prepared_;
    core::v1::Action active_action_{core::v1::Action::None};
    core::v1::End active_end_{core::v1::End::NormalUp};
    HHOOK hook_{};
    std::array<HWINEVENTHOOK,5> events_{};
};
}
