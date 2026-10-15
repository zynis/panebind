#pragma once
#include "window_catalog.h"
#include <array>
#include <atomic>
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
inline bool native_event_is_current(DWORD event_stamp,DWORD down_stamp) {
    return static_cast<DWORD>(event_stamp-down_stamp)<0x80000000u;
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
    bool allowed(std::uint64_t g)const {return g && authorized_.load()==g && !paused_;}
    void revoke(){authorized_=0;}
    bool healthy()const {return healthy_;}
    std::uint64_t max_callback_ticks()const{return max_ticks_;}
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
    std::atomic<std::uint64_t> authorized_{0},point_{0},max_ticks_{0};
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
    POINT last_down_point_{};
    int last_region_{-1};
    int last_button_{};
    unsigned last_modifiers_{};
    std::uint64_t last_source_key_{},deadline_{};
    bool have_last_down_{};
    UINT double_click_time_{};
    int double_click_x_{},double_click_y_{};
    std::shared_ptr<const Prepared> active_prepared_;
    core::v1::Action active_action_{core::v1::Action::None};
    core::v1::End active_end_{core::v1::End::NormalUp};
    HHOOK hook_{};
    std::array<HWINEVENTHOOK,5> events_{};
};
}
