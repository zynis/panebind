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
#include <memory>
#include <optional>
#include <string>
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
    [[nodiscard]] std::vector<Snapshot> all(const core::v1::Settings& settings);
    [[nodiscard]] std::vector<Display> displays();
    [[nodiscard]] static bool same(const Snapshot& first, const Snapshot& second);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::product
