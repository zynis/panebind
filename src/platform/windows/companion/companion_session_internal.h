#pragma once

#include "platform/windows/companion/companion_session.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace panebind::platform::windows::companion::detail {

struct CompanionLedgerEntry {
    protocol::SessionMarker session_authority{};
    protocol::LogicalWindowId logical_id{};
    std::uint64_t generation{};
    std::uintptr_t native_key{};
    std::uint32_t process_id{};
    std::uint32_t creator_thread_id{};
};

enum class CompanionLedgerIssueStatus {
    Succeeded,
    SessionRetired,
    InvalidIdentity,
    DuplicateLogicalId,
    DuplicateNativeKey,
    NonMonotonicGeneration,
    AuthorityExhausted,
};

struct CompanionLedgerIssueResult {
    CompanionLedgerIssueStatus status{
        CompanionLedgerIssueStatus::InvalidIdentity};
    std::optional<CompanionWindowToken> token;
};

class CompanionTokenLedger final {
public:
    explicit CompanionTokenLedger(protocol::SessionMarker session_authority) noexcept;
    CompanionTokenLedger(protocol::SessionMarker session_authority,
                         std::uint64_t controller_authority) noexcept;

    CompanionTokenLedger(const CompanionTokenLedger&) = delete;
    CompanionTokenLedger& operator=(const CompanionTokenLedger&) = delete;

    [[nodiscard]] CompanionLedgerIssueResult issue(
        protocol::LogicalWindowId logical_id,
        std::uint64_t generation,
        std::uintptr_t native_key,
        std::uint32_t process_id,
        std::uint32_t creator_thread_id);
    [[nodiscard]] bool retire(protocol::LogicalWindowId logical_id) noexcept;
    void retire_all() noexcept;
    [[nodiscard]] bool session_active() const noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] std::optional<CompanionLedgerEntry> resolve(
        const CompanionWindowToken& token) const noexcept;
    [[nodiscard]] std::vector<CompanionWindowToken> active_tokens() const;
    [[nodiscard]] std::vector<CompanionLedgerEntry> active_entries() const;

private:
    std::uint64_t controller_authority_{};
    protocol::SessionMarker session_authority_{};
    bool session_active_{true};
    std::map<protocol::LogicalWindowId, CompanionLedgerEntry> active_;
    std::map<std::uintptr_t, protocol::LogicalWindowId> logical_by_native_;
    std::map<protocol::LogicalWindowId, std::uint64_t> highest_generation_;
};

enum class CompanionBatchTokenValidation {
    Succeeded,
    Empty,
    Duplicate,
    Unknown,
};

[[nodiscard]] CompanionBatchTokenValidation validate_companion_batch_tokens(
    std::span<const CompanionWindowToken> tokens,
    const CompanionTokenLedger& ledger) noexcept;

[[nodiscard]] bool is_companion_top_level_provenance(
    bool root_is_self,
    bool has_child_style,
    bool has_owner) noexcept;

[[nodiscard]] bool is_valid_uncooperative_selection(
    std::uint32_t window_mask,
    std::int32_t offset_x,
    std::uint32_t requested_window_mask) noexcept;

} // namespace panebind::platform::windows::companion::detail
