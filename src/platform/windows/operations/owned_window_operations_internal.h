#pragma once

#include "platform/windows/operations/owned_window_operations.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace panebind::platform::windows::operations::detail {

class MonotonicIdSource final {
public:
    explicit MonotonicIdSource(std::uint64_t first = 1U) noexcept;

    MonotonicIdSource(const MonotonicIdSource&) = delete;
    MonotonicIdSource& operator=(const MonotonicIdSource&) = delete;

    [[nodiscard]] std::uint64_t issue() noexcept;

private:
    std::atomic<std::uint64_t> next_;
};

struct LedgerEntry {
    std::uint64_t logical_id{};
    std::uint64_t generation{};
    std::uintptr_t native_key{};
    std::uint32_t process_id{};
    std::uint32_t creator_thread_id{};
};

enum class LedgerIssueStatus {
    Succeeded,
    InvalidNativeKey,
    DuplicateNativeKey,
    AuthorityExhausted,
    GenerationExhausted,
};

struct LedgerIssueResult {
    LedgerIssueStatus status{LedgerIssueStatus::InvalidNativeKey};
    std::optional<OwnedWindowToken> token;
};

class TokenLedger final {
public:
    TokenLedger() noexcept;
    TokenLedger(std::uint64_t next_logical_id,
                std::uint64_t next_generation) noexcept;

    [[nodiscard]] LedgerIssueResult issue(std::uintptr_t native_key,
                                          std::uint32_t process_id,
                                          std::uint32_t creator_thread_id);
    [[nodiscard]] bool retire_native(std::uintptr_t native_key) noexcept;
    [[nodiscard]] std::optional<LedgerEntry>
    resolve(const OwnedWindowToken& token) const noexcept;
    [[nodiscard]] std::optional<LedgerEntry>
    find_native(std::uintptr_t native_key) const noexcept;
    [[nodiscard]] std::vector<LedgerEntry> active_entries() const;

private:
    std::uint64_t authority_id_{};
    std::uint64_t next_logical_id_{1U};
    std::uint64_t next_generation_{1U};
    std::map<std::uint64_t, LedgerEntry> active_by_logical_id_;
    std::map<std::uintptr_t, std::uint64_t> logical_id_by_native_key_;
};

enum class BatchTokenValidation {
    Succeeded,
    Empty,
    Duplicate,
    Unknown,
};

[[nodiscard]] BatchTokenValidation validate_batch_tokens(
    std::span<const OwnedWindowToken> tokens,
    const TokenLedger& ledger) noexcept;

[[nodiscard]] bool is_independent_top_level_provenance(
    bool root_is_self,
    bool has_child_style,
    bool has_owner) noexcept;

enum class TranslationBridgeStatus {
    Succeeded,
    EmptyGeometry,
    ResizeRejected,
    ArithmeticOverflow,
    NativeCoordinateOutOfRange,
};

struct TranslationBridgeResult {
    TranslationBridgeStatus status{TranslationBridgeStatus::ArithmeticOverflow};
    core::geometry::Distance dx{};
    core::geometry::Distance dy{};
    std::optional<core::geometry::Rect> target_positioning_rect;
};

[[nodiscard]] TranslationBridgeResult bridge_visible_translation(
    const core::geometry::Rect& current_positioning_rect,
    const core::geometry::Rect& current_visible_rect,
    const core::geometry::Rect& target_visible_rect) noexcept;

} // namespace panebind::platform::windows::operations::detail
