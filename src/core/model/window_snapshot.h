#pragma once

#include "core/events/window_event.h"
#include "core/geometry/geometry.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace panebind::core::model {

class ProcessId final {
public:
    ProcessId() = delete;

    explicit ProcessId(std::uint32_t value) : value_(value) {
        if (value_ == 0) {
            throw std::invalid_argument{"ProcessId must not be zero"};
        }
    }

    [[nodiscard]] std::uint32_t value() const noexcept { return value_; }

    friend bool operator==(const ProcessId&, const ProcessId&) = default;

private:
    std::uint32_t value_;
};

enum class WindowState {
    Normal,
    Minimized,
    Maximized,
};

struct NormalizedWindowSnapshot {
    NormalizedWindowSnapshot(events::WindowId window_id, ProcessId owner_process_id)
        : id(std::move(window_id)), process_id(owner_process_id) {}

    const events::WindowId id;
    const ProcessId process_id;
    // Normalized text is UTF-8. Adapters must transcode native encodings.
    std::optional<std::string> process_name;
    std::optional<std::string> title;
    std::optional<bool> visible;
    std::optional<WindowState> state;
    std::optional<geometry::Rect> positioning_bounds;
    std::optional<geometry::Rect> visible_frame_bounds;
    // Display identifiers are opaque UTF-8 adapter values.
    std::optional<std::string> display_id;
    std::optional<geometry::Rect> display_work_area;
    // Effective scale reported for the target window. This is deliberately not
    // named monitor/display scale: some platforms report an application-aware
    // value that can differ from the physical display scale.
    std::optional<double> effective_window_scale;
};

} // namespace panebind::core::model
