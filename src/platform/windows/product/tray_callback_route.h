#pragma once

#include <windows.h>
#include <shellapi.h>

namespace panebind::platform::windows::product {

enum class TrayCallbackRoute { Ignored, Menu, Settings };

// Decode the event already extracted from the v4 callback's LOWORD(lParam).
// Keyboard activation belongs to the same existing Settings route as Select.
[[nodiscard]] constexpr TrayCallbackRoute tray_callback_route(UINT event) noexcept {
    if(event==WM_CONTEXTMENU||event==WM_RBUTTONUP)return TrayCallbackRoute::Menu;
    if(event==NIN_SELECT||event==NIN_KEYSELECT||event==WM_LBUTTONDBLCLK)return TrayCallbackRoute::Settings;
    return TrayCallbackRoute::Ignored;
}

} // namespace panebind::platform::windows::product
