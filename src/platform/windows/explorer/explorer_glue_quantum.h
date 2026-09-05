#pragma once

#include "platform/windows/explorer/explorer_glue_event_source.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace panebind::platform::windows::explorer::detail {

// Preserve lifecycle barriers and receipt order. Coalescing is scoped to one
// owner quantum, never across wakes. The selected receipt is a trigger to read
// current geometry, not an assertion about that receipt's historical geometry.
[[nodiscard]] inline std::vector<bool> select_glue_quantum_events(
    std::span<const ExplorerGlueEvent> events) {
    std::vector<bool> selected(events.size(), true);
    std::optional<std::size_t> latest_leader;
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.kind != ExplorerGlueEventKind::GeometryChanged) {
            latest_leader.reset();
        } else if (event.role == ExplorerGlueWindowRole::Leader) {
            if (latest_leader.has_value()) {
                selected[*latest_leader] = false;
            }
            latest_leader = index;
        }
    }
    return selected;
}

inline constexpr std::size_t kGlueMessageQuantum = 8U;

// pump_one may deliver internal/nonqueued WinEvents even when returning false.
// Recheck target receipts after every call, before attempting another message.
template <typename PumpOne, typename ReceiptReady>
void pump_glue_message_quantum(PumpOne&& pump_one, ReceiptReady&& ready) {
    for (std::size_t count = 0; count < kGlueMessageQuantum; ++count) {
        if (ready()) {
            return;
        }
        const bool retrieved = pump_one();
        if (ready() || !retrieved) {
            return;
        }
    }
}

} // namespace panebind::platform::windows::explorer::detail
