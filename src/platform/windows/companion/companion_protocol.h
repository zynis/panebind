#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace panebind::platform::windows::companion::protocol {

// R1-C1 uses a small, versioned, little-endian protocol over two inherited
// anonymous-pipe handles. Native HWND values below are launch-handshake facts;
// they are not durable identities or public operation capabilities.
inline constexpr std::uint32_t kFrameMagic = 0x31434250U; // "PBC1"
inline constexpr std::uint16_t kProtocolVersion = 1U;
inline constexpr std::uint32_t kWindowCount = 4U;
inline constexpr std::uint32_t kAllWindowMask = 0x0fU;
inline constexpr std::size_t kMaximumEvidenceRecords = 512U;
inline constexpr std::size_t kMaximumPayloadBytes = 64U * 1024U;
inline constexpr std::int32_t kMaximumTestOffset = 64;
inline constexpr std::uint64_t kInitialWindowGeneration = 1U;
inline constexpr wchar_t kWindowClassName[] =
    L"PaneBind.R1C1.CompanionWindow";
inline constexpr wchar_t kSessionPropertyPrefix[] =
    L"PaneBind.R1C1.Session.";

struct SessionMarker {
    std::uint64_t high{};
    std::uint64_t low{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return high != 0U || low != 0U;
    }

    friend constexpr bool operator==(const SessionMarker&,
                                     const SessionMarker&) noexcept = default;
};

enum class LogicalWindowId : std::uint32_t {
    A = 1U,
    B = 2U,
    C = 3U,
    D = 4U,
};

[[nodiscard]] constexpr bool is_valid_logical_window_id(
    const std::uint32_t value) noexcept {
    return value >= static_cast<std::uint32_t>(LogicalWindowId::A) &&
           value <= static_cast<std::uint32_t>(LogicalWindowId::D);
}

[[nodiscard]] constexpr std::uint32_t logical_window_bit(
    const LogicalWindowId logical_id) noexcept {
    return 1U << (static_cast<std::uint32_t>(logical_id) - 1U);
}

// The session-specific property name supplies launch authority. The nonzero
// value is only an additional logical-id/generation consistency fact.
[[nodiscard]] constexpr std::uint64_t marker_property_value(
    const LogicalWindowId logical_id,
    const std::uint64_t generation) noexcept {
    constexpr std::uint64_t maximum_generation =
        (std::numeric_limits<std::uint64_t>::max)() >> 3U;
    return generation <= maximum_generation
               ? (generation << 3U) |
                     static_cast<std::uint32_t>(logical_id)
               : 0U;
}

[[nodiscard]] inline std::wstring session_property_name(
    const SessionMarker marker) {
    constexpr wchar_t hexadecimal[] = L"0123456789abcdef";
    std::wstring result{kSessionPropertyPrefix};
    result.reserve(result.size() + 32U);
    const auto append = [&result, &hexadecimal](const std::uint64_t value) {
        for (int shift = 60; shift >= 0; shift -= 4) {
            result.push_back(hexadecimal[(value >> shift) & 0x0fU]);
        }
    };
    append(marker.high);
    append(marker.low);
    return result;
}

enum class FrameKind : std::uint16_t {
    Handshake = 1U,
    SetStep = 2U,
    DestroyWindow = 3U,
    QueryEvidence = 4U,
    Shutdown = 5U,
    CommandResult = 6U,
    Evidence = 7U,
    FatalError = 8U,
};

enum class CommandStatus : std::uint16_t {
    Succeeded = 0U,
    InvalidFrame = 1U,
    InvalidPayload = 2U,
    InvalidWindow = 3U,
    UiThreadUnavailable = 4U,
    NativeFailure = 5U,
    SessionMismatch = 6U,
    UnsupportedCommand = 7U,
};

enum class EvidenceMessage : std::uint32_t {
    WindowPosChanging = 1U,
    WindowPosChanged = 2U,
    Move = 3U,
    Size = 4U,
    NonClientDestroy = 5U,
};

#pragma pack(push, 1)

struct FrameHeader {
    std::uint32_t magic{kFrameMagic};
    std::uint16_t version{kProtocolVersion};
    FrameKind kind{};
    std::uint32_t payload_size{};
    std::uint32_t flags{};
    SessionMarker session{};
    std::uint64_t request_id{};
};

struct WindowRegistrationFact {
    LogicalWindowId logical_id{};
    std::uint32_t reserved{};
    std::uint64_t generation{};
    std::uint64_t native_window{};
    std::uint32_t process_id{};
    std::uint32_t ui_thread_id{};
    std::uint64_t marker_value{};
};

struct HandshakePayload {
    std::uint32_t target_process_id{};
    std::uint32_t target_ui_thread_id{};
    std::uint32_t window_count{};
    std::uint32_t reserved{};
    std::array<WindowRegistrationFact, kWindowCount> windows{};
};

struct SetStepPayload {
    std::uint32_t uncooperative_window_mask{};
    std::int32_t d_window_x_offset{};
    std::uint64_t step_id{};
};

struct DestroyWindowPayload {
    LogicalWindowId logical_id{};
    std::uint32_t reserved{};
};

struct CommandResultPayload {
    FrameKind command{};
    CommandStatus status{};
    std::uint32_t native_error{};
    std::uint32_t target_process_id{};
    std::uint32_t target_ui_thread_id{};
    LogicalWindowId logical_id{};
    std::uint32_t reserved{};
    std::uint64_t step_id{};
};

struct EvidenceRecord {
    std::uint64_t sequence{};
    std::uint64_t step_id{};
    std::uint64_t tick_ms{};
    LogicalWindowId logical_id{};
    EvidenceMessage message{};
    std::uint64_t generation{};
    std::uint64_t native_window{};
    std::uint32_t target_process_id{};
    std::uint32_t target_ui_thread_id{};
    std::int32_t proposed_x{};
    std::int32_t effective_x{};
    std::int32_t y{};
    std::int32_t width{};
    std::int32_t height{};
    std::uint32_t flags{};
    std::int32_t applied_test_offset_x{};
    std::uint32_t reserved{};
    std::uint64_t message_wparam{};
};

struct EvidencePayload {
    std::uint32_t record_count{};
    std::uint32_t overflowed{};
    std::uint64_t dropped_record_count{};
    std::uint64_t next_sequence{};
    std::uint32_t target_process_id{};
    std::uint32_t target_ui_thread_id{};
    std::array<EvidenceRecord, kMaximumEvidenceRecords> records{};
};

#pragma pack(pop)

static_assert(sizeof(SessionMarker) == 16U);
static_assert(sizeof(FrameHeader) == 40U);
static_assert(sizeof(WindowRegistrationFact) == 40U);
static_assert(sizeof(HandshakePayload) == 176U);
static_assert(sizeof(SetStepPayload) == 16U);
static_assert(sizeof(DestroyWindowPayload) == 8U);
static_assert(sizeof(CommandResultPayload) == 32U);
static_assert(sizeof(EvidenceRecord) == 96U);
static_assert(sizeof(EvidencePayload) == 49'184U);
static_assert(sizeof(EvidencePayload) <= kMaximumPayloadBytes);

static_assert(std::is_trivially_copyable_v<FrameHeader>);
static_assert(std::is_trivially_copyable_v<HandshakePayload>);
static_assert(std::is_trivially_copyable_v<SetStepPayload>);
static_assert(std::is_trivially_copyable_v<DestroyWindowPayload>);
static_assert(std::is_trivially_copyable_v<CommandResultPayload>);
static_assert(std::is_trivially_copyable_v<EvidencePayload>);

} // namespace panebind::platform::windows::companion::protocol
