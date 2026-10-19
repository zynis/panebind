#pragma once

#include "core/v1/settings.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace panebind::platform::windows::product {

struct Snapshot {
    HWND hwnd{};
    DWORD pid{}, tid{};
    std::uint64_t born{}, key{};
    core::v1::Rect visible, positioning, work;
    UINT dpi{};
    HMONITOR monitor{};
    std::wstring exe, window_class;
    core::v1::Limits limits;
    bool maximized{}, can_resize{}, minimized{};
    std::uint32_t feature_mask{0xffffffffU};
    // A WM_GETMINMAXINFO response does not reveal an arbitrary app grid.
    // Unknown grid is never relabelled as a measured one-pixel size policy.
    bool declared_track_limits_observed{},application_grid_known{};
};

struct Display {
    HMONITOR monitor{};
    core::v1::Rect work, bounds;
    UINT dpi{};
};

enum class GuiContextMode { Unknown, OwnerThread, ForegroundConsole };
[[nodiscard]] constexpr const char* gui_context_mode_name(GuiContextMode mode) noexcept {
    switch(mode){case GuiContextMode::OwnerThread:return "owner_thread";
        case GuiContextMode::ForegroundConsole:return "foreground_console";default:return "unknown";}
}
struct GuiContext {
    GUITHREADINFO actual{sizeof(GUITHREADINFO)};
    GuiContextMode mode{GuiContextMode::Unknown};
    HWND foreground_before{},foreground_after{};
    DWORD owner_error{},foreground_error{};
    bool owner_succeeded{},foreground_succeeded{};
    [[nodiscard]] bool known()const noexcept{return mode!=GuiContextMode::Unknown;}
    [[nodiscard]] bool idle()const noexcept{return known()&&!actual.hwndCapture&&!actual.hwndMoveSize&&!actual.hwndMenuOwner;}
};
// Read-only facts, never window-operation authority. For an exact foreground
// ConsoleWindowClass whose reported owner has no GUI queue, GTI(0) must really
// succeed and identify this same active root between matching foreground reads.
// Background console followers and every unknown result remain rejected.
[[nodiscard]] GuiContext observe_gui_context(const Snapshot&);

// Platform-only, one actual CaptureReady generation. Identity is immutable
// after publication. These independent facts never grant window authority:
// InputService still owns the final generation/stop admission guard.
struct GestureCaptureContext {
    std::uint64_t generation{};
    Snapshot source;
    HWND overlay{};
    DWORD owner_tid{},owner_pid{};
    // A matched real LL/Raw UP revokes issue permission, but is not proof of
    // queued legacy UP or successful resource cleanup / NormalUp completion.
    mutable std::atomic<bool> matching_up_observed{},normal_up_observed{},own_release_confirmed{},association_detached{};
};
using GestureCaptureLease=std::shared_ptr<const GestureCaptureContext>;
inline constexpr char capture_matching_up_retired_reason[]="capture_matching_UP_retired_before_native_call";
enum class GestureGuiDisposition { Allowed,MatchingUpRetired,Rejected };
// A rejected admission is not failed completion. Use the exact same lease
// which produced the context decision; a newer pair cannot explain old input.
inline GestureGuiDisposition gesture_gui_disposition(bool context_allowed,std::string_view reason,
    std::uint64_t generation,const GestureCaptureLease& capture,bool generation_allowed)noexcept{
    if(context_allowed)return GestureGuiDisposition::Allowed;
    if(reason==capture_matching_up_retired_reason&&generation&&capture&&capture->generation==generation&&
       !generation_allowed&&capture->matching_up_observed.load(std::memory_order_acquire))
        return GestureGuiDisposition::MatchingUpRetired;
    return GestureGuiDisposition::Rejected;
}
enum class GeometryContextPhase { BeforeNativeCall,IssuedCompletion };
// Without a lease this retains the existing idle-only policy. With a lease,
// every new call needs ACTUAL own capture; idle is not a fallback. A target on
// the source's same PID/TID sees the same GUI queue, but still needs its own
// independent Catalog/plan admission. No other target gets capture authority.
[[nodiscard]] bool geometry_gui_context_allowed(const Snapshot&,const GuiContext&,
    const GestureCaptureLease&,GeometryContextPhase,std::string* why=nullptr);

// Product admission is deliberately independent of owned/Explorer test tokens.
// Construct, use and destroy on the same already-initialized UI STA. Its normal
// message pump must keep delivering lifecycle WinEvents; get() never dispatches
// arbitrary application messages or waits on a foreign window procedure.
class Catalog final {
public:
    Catalog();
    ~Catalog();
    Catalog(const Catalog&) = delete;
    Catalog& operator=(const Catalog&) = delete;

    [[nodiscard]] std::optional<Snapshot> get(HWND window,
        const core::v1::Settings& settings, std::string* why = nullptr,
        bool include_minimized_for_restore = false, bool query_track_limits = true);
    // Magnet/group candidates share one conservative frame-rectangle policy:
    // with complete enumeration and unchanged bounded actual Z/frame reads,
    // omit a rear frame wholly contained by one eligible front frame. Partial
    // coverage remains; unknown Z makes no occlusion claim. This is not pixel
    // visibility. Truncated enumeration returns no invented complete group.
    [[nodiscard]] std::vector<Snapshot> all(const core::v1::Settings& settings);
    [[nodiscard]] std::vector<Display> displays();
    [[nodiscard]] static bool same(const Snapshot& first, const Snapshot& second);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::product
