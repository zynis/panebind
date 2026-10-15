#pragma once

#include "core/v1/settings.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <functional>
#include <memory>
#include <optional>

namespace panebind::platform::windows::product {

// The caller owns the application's normal STA message loop. Edits are local
// drafts until save validates and the production callback confirms persistence.
class SettingsWindow final {
public:
    SettingsWindow();
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    void show(HWND owner, const core::v1::Settings& settings,
              std::function<bool(const core::v1::Settings&)> save,
              std::function<void()> diagnostic,
              std::function<std::optional<core::v1::Rule>()> current_application = {});
    void close();
    [[nodiscard]] HWND handle() const;
    bool translate(MSG& message);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::product
