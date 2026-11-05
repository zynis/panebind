#pragma once

#include "core/v1/settings_editor.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <optional>

namespace panebind::platform::windows::product {

// Modal native forms edit a copy; only OK publishes to the settings draft.
// IDs are stable for the isolated GUI driver, not an alternate product API.
bool edit_settings_mapping(HWND owner, core::v1::Settings& draft, int kind,
                           std::optional<std::size_t> index,
                           std::optional<core::v1::Rule> suggested_rule = {});
std::wstring settings_key_label(unsigned key);

} // namespace panebind::platform::windows::product
