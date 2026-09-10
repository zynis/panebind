#pragma once

#include "platform/windows/explorer/explorer_shell_inventory.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace panebind::platform::windows::explorer {

enum class ConsentValidationPhase : std::uint8_t { Setup, Start, Active, End, Restore, Invalidation, Count };
constexpr bool consent_fast_mode(bool private_bound_contract, bool active_phase) noexcept {
    return private_bound_contract && active_phase;
}
constexpr std::string_view consent_phase_name(ConsentValidationPhase phase) noexcept {
    constexpr std::array names{"setup", "start", "active", "end", "restore", "invalidation"};
    const auto i = static_cast<std::size_t>(phase);
    return i < names.size() ? names[i] : "invalid";
}

// No COM, pumping, allocation, authority issuance or callback work here.
// The final checkpoint rejects even a not-yet-resolved receipt.
constexpr std::string_view consent_browser_invalidation(
    const BrowserReadinessFacts& b, std::uint64_t navigation_epoch) noexcept {
    if (b.quit_count != 0 || b.latest_activity_was_quit) return "browser_quit";
    if (b.matching_navigate_complete_count != navigation_epoch) return "browser_navigation_changed";
    if (!b.subscribed || !b.accepting || b.unadvised || b.malformed_count ||
        b.overflow_count || b.wrong_thread_count || b.post_retirement_count ||
        b.identity_query_failure_count || b.unrelated_navigate_complete_count ||
        b.callback_sequence != b.latest_sequence) return "browser_stream_invalid";
    return "none";
}

struct ConsentValidationRecord {
    std::uint64_t span_id{}, quantum_id{}, operation_generation{}, source_receipt{};
    std::uintptr_t native_key{};
    std::uint64_t capability_generation{}, consent_generation{};
    std::uint64_t inventory_calls{};
    ConsentValidationPhase phase{};
    bool fast{}, follower{}, succeeded{}, role_bound{};
    std::string_view reason{"not_completed"};
};

struct ConsentFrameAnchorProof {
    bool token_generation_matches{}, canonical_identity_matches{};
    std::uintptr_t current_window{}, authorized_window{};
    bool location_exact{};
    BrowserReadinessFacts browser;
    std::uint64_t navigation_epoch{};
};
constexpr std::string_view consent_frame_anchor_invalidation(const ConsentFrameAnchorProof& p) noexcept {
    if (!p.token_generation_matches) return "token_generation_changed";
    if (!p.canonical_identity_matches) return "canonical_identity_changed";
    if (!p.authorized_window || p.current_window != p.authorized_window) return "hwnd_changed";
    const auto browser = consent_browser_invalidation(p.browser, p.navigation_epoch);
    if (browser != "none") return browser;
    return p.location_exact ? "none" : "location_changed";
}

// Evidence-only owner-STA audit. Mode/phase values never issue a capability.
// Native code uses private session state, not this diagnostic pointer, to
// authorize the fast path. Scope is created only by the Phase 2 executable.
class ConsentValidationAudit final {
public:
    static constexpr std::size_t capacity = 8192;
    ConsentValidationAudit() : records_(std::make_unique<ConsentValidationRecord[]>(capacity)) {}
    void record(ConsentValidationRecord value) noexcept {
        invalidation_observed = invalidation_observed || (!value.succeeded && value.phase != ConsentValidationPhase::Setup);
        if (size_ == capacity) { overflow = true; return; }
        records_[size_++] = value;
    }
    void inventory_request() noexcept {
        ++inventory_calls[static_cast<std::size_t>(phase)];
        ++total_inventory_calls;
    }
    [[nodiscard]] std::span<const ConsentValidationRecord> records() const noexcept { return {records_.get(), size_}; }
    ConsentValidationPhase phase{ConsentValidationPhase::Setup};
    std::array<std::uint64_t, static_cast<std::size_t>(ConsentValidationPhase::Count)> inventory_calls{};
    std::uint64_t total_inventory_calls{};
    std::uint64_t active_native_after_invalidation{};
    bool overflow{};
    bool invalidation_observed{};
private:
    std::unique_ptr<ConsentValidationRecord[]> records_;
    std::size_t size_{};
};
inline thread_local ConsentValidationAudit* consent_validation_audit = nullptr;
class ConsentValidationAuditScope final {
public:
    explicit ConsentValidationAuditScope(ConsentValidationAudit* value) noexcept
        : previous_(consent_validation_audit) { consent_validation_audit = value; }
    ~ConsentValidationAuditScope() { consent_validation_audit = previous_; }
private:
    ConsentValidationAudit* previous_;
};
class ConsentValidationPhaseScope final {
public:
    explicit ConsentValidationPhaseScope(ConsentValidationPhase phase) noexcept
        : audit_(consent_validation_audit), previous_(audit_ ? audit_->phase : phase) {
        if (audit_) audit_->phase = phase;
    }
    ~ConsentValidationPhaseScope() { if (audit_) audit_->phase = previous_; }
private:
    ConsentValidationAudit* audit_;
    ConsentValidationPhase previous_;
};
// Counts API requests, including failure, without double-counting the public
// overload forwarding to the IShellWindows overload.
class ConsentInventoryRequestScope final {
public:
    ConsentInventoryRequestScope() noexcept { if (depth_++ == 0 && consent_validation_audit) consent_validation_audit->inventory_request(); }
    ~ConsentInventoryRequestScope() { --depth_; }
private:
    inline static thread_local unsigned depth_{};
};
} // namespace panebind::platform::windows::explorer
