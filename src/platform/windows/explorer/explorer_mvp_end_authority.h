#pragma once

#include <cstdint>

namespace panebind::platform::windows::explorer {

// Facts observed after processing the batch containing the native END.
// The two conflict inputs are supplied separately to the production predicate
// so that its polarity is directly exercised by the offline test.
struct MvpEndAuthorityFacts final {
    bool exact_handoff_context{};
    bool gui_after_end{};
    bool source_foreground{};
    bool physical_left_held{};
    bool cursor_available{};
    bool overlay_valid{};
    bool input_desktop_active{};
    bool raw_up_seen{};
};

[[nodiscard]] constexpr bool mvp_end_authority_ready(
    const MvpEndAuthorityFacts& facts, bool processed_batch_conflict,
    bool pending_group_conflict) noexcept {
    return facts.exact_handoff_context && facts.gui_after_end &&
        facts.source_foreground && facts.physical_left_held &&
        facts.cursor_available && facts.overlay_valid &&
        facts.input_desktop_active && !processed_batch_conflict &&
        !pending_group_conflict &&
        !facts.raw_up_seen;
}

// Samples drained in the END owner's quantum have no cross-queue ordering
// guarantee. Only a later Raw packet can drive the post-END writer.
[[nodiscard]] constexpr bool mvp_raw_continuation_after_handoff(
    std::uint64_t sample_sequence, std::uint64_t down_packet_sequence,
    std::uint64_t handoff_watermark) noexcept {
    return sample_sequence > down_packet_sequence &&
        sample_sequence > handoff_watermark;
}

} // namespace panebind::platform::windows::explorer
