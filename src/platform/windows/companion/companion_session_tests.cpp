#include "platform/windows/companion/companion_session_internal.h"

#include "platform/windows/operations/owned_window_operations.h"
#include "platform/windows/operations/window_translation.h"

#include <windows.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

namespace companion = panebind::platform::windows::companion;
namespace detail = panebind::platform::windows::companion::detail;
namespace geometry = panebind::core::geometry;
namespace operations = panebind::platform::windows::operations;
namespace protocol = panebind::platform::windows::companion::protocol;

int failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] companion::CompanionWindowToken issued_token(
    detail::CompanionTokenLedger& ledger,
    const protocol::LogicalWindowId logical_id,
    const std::uint64_t generation,
    const std::uintptr_t native_key) {
    const auto result = ledger.issue(logical_id,
                                     generation,
                                     native_key,
                                     101U,
                                     202U);
    expect(result.status == detail::CompanionLedgerIssueStatus::Succeeded &&
               result.token.has_value(),
           "test token issuance succeeds");
    return *result.token;
}

void test_token_surface_is_opaque_and_separate() {
    static_assert(!std::is_default_constructible_v<
                  companion::CompanionWindowToken>);
    static_assert(!std::is_constructible_v<companion::CompanionWindowToken,
                                           HWND>);
    static_assert(!std::is_convertible_v<companion::CompanionWindowToken,
                                         HWND>);
    static_assert(!std::is_convertible_v<HWND,
                                         companion::CompanionWindowToken>);
    static_assert(!std::is_same_v<companion::CompanionWindowToken,
                                  operations::OwnedWindowToken>);
    static_assert(!std::is_constructible_v<operations::OwnedWindowToken,
                                           companion::CompanionWindowToken>);
    expect(true,
           "companion token has no HWND surface and does not generalize owned token");
}

void test_controller_and_session_authorities_do_not_alias() {
    constexpr protocol::SessionMarker first_session{1U, 11U};
    constexpr protocol::SessionMarker second_session{2U, 22U};

    detail::CompanionTokenLedger first{first_session, 1001U};
    detail::CompanionTokenLedger other_controller{first_session, 1002U};
    detail::CompanionTokenLedger second{second_session, 1001U};

    const auto first_token = issued_token(
        first, protocol::LogicalWindowId::A, 1U, 0x100U);
    const auto other_controller_token = issued_token(
        other_controller, protocol::LogicalWindowId::A, 1U, 0x200U);
    const auto second_token = issued_token(
        second, protocol::LogicalWindowId::A, 1U, 0x300U);

    expect(first_token.logical_id() == other_controller_token.logical_id() &&
               first_token.generation() == other_controller_token.generation() &&
               first_token.session_authority() ==
                   other_controller_token.session_authority(),
           "test reproduces equal session-local token fields");
    expect(first_token != other_controller_token,
           "private controller authority prevents registry alias");
    expect(first_token != second_token,
           "session authority prevents cross-launch alias");
    expect(first.resolve(first_token).has_value() &&
               !first.resolve(other_controller_token).has_value() &&
               !first.resolve(second_token).has_value(),
           "ledger resolves only its controller/session authority");
}

void test_generation_and_native_reuse_never_revive_old_token() {
    detail::CompanionTokenLedger ledger{{3U, 33U}, 2001U};
    expect(ledger.issue(protocol::LogicalWindowId::A,
                        0U,
                        0x100U,
                        101U,
                        202U)
                   .status == detail::CompanionLedgerIssueStatus::InvalidIdentity,
           "zero generation is rejected");

    const auto first = issued_token(
        ledger, protocol::LogicalWindowId::A, 1U, 0x100U);
    expect(ledger.retire(protocol::LogicalWindowId::A),
           "logical registration retires");
    expect(!ledger.resolve(first).has_value(),
           "retired token is stale immediately");
    expect(ledger.issue(protocol::LogicalWindowId::A,
                        1U,
                        0x100U,
                        101U,
                        202U)
                   .status ==
               detail::CompanionLedgerIssueStatus::NonMonotonicGeneration,
           "same generation cannot revive a retired token");

    const auto second = issued_token(
        ledger, protocol::LogicalWindowId::A, 2U, 0x100U);
    expect(second != first && second.generation() == 2U,
           "reused native value requires and receives a newer generation");
    expect(!ledger.resolve(first).has_value() &&
               ledger.resolve(second).has_value(),
           "native-value reuse never revives old capability");
}

void test_unique_registration_and_token_set_preflight() {
    detail::CompanionTokenLedger ledger{{4U, 44U}, 3001U};
    const auto a = issued_token(
        ledger, protocol::LogicalWindowId::A, 1U, 0x100U);
    const auto b = issued_token(
        ledger, protocol::LogicalWindowId::B, 1U, 0x200U);

    expect(ledger.issue(protocol::LogicalWindowId::A,
                        2U,
                        0x300U,
                        101U,
                        202U)
                   .status ==
               detail::CompanionLedgerIssueStatus::DuplicateLogicalId,
           "duplicate active logical ID is rejected");
    expect(ledger.issue(protocol::LogicalWindowId::C,
                        1U,
                        0x200U,
                        101U,
                        202U)
                   .status ==
               detail::CompanionLedgerIssueStatus::DuplicateNativeKey,
           "duplicate active native fact is rejected");

    const std::vector<companion::CompanionWindowToken> empty;
    const std::vector duplicate{a, a};
    const std::vector valid{b, a};
    expect(detail::validate_companion_batch_tokens(empty, ledger) ==
               detail::CompanionBatchTokenValidation::Empty,
           "empty batch fails deterministic preflight");
    expect(detail::validate_companion_batch_tokens(duplicate, ledger) ==
               detail::CompanionBatchTokenValidation::Duplicate,
           "duplicate token batch fails deterministic preflight");
    expect(detail::validate_companion_batch_tokens(valid, ledger) ==
               detail::CompanionBatchTokenValidation::Succeeded,
           "distinct active companion tokens pass preflight");

    expect(ledger.retire(protocol::LogicalWindowId::A),
           "test retires one batch member");
    const std::vector mixed{b, a};
    expect(detail::validate_companion_batch_tokens(mixed, ledger) ==
               detail::CompanionBatchTokenValidation::Unknown,
           "mixed valid/stale set fails before native apply");
}

void test_whole_session_exit_retires_every_token() {
    detail::CompanionTokenLedger ledger{{5U, 55U}, 4001U};
    const auto a = issued_token(
        ledger, protocol::LogicalWindowId::A, 1U, 0x100U);
    const auto b = issued_token(
        ledger, protocol::LogicalWindowId::B, 1U, 0x200U);
    const auto c = issued_token(
        ledger, protocol::LogicalWindowId::C, 1U, 0x300U);
    const auto d = issued_token(
        ledger, protocol::LogicalWindowId::D, 1U, 0x400U);
    expect(ledger.active_count() == protocol::kWindowCount &&
               ledger.active_tokens().size() == protocol::kWindowCount,
           "four handshake registrations are active");

    ledger.retire_all();
    expect(!ledger.session_active() && ledger.active_count() == 0U,
           "whole-process exit retires the complete session");
    expect(!ledger.resolve(a).has_value() && !ledger.resolve(b).has_value() &&
               !ledger.resolve(c).has_value() && !ledger.resolve(d).has_value(),
           "all pre-exit tokens are stale");
    expect(ledger.issue(protocol::LogicalWindowId::A,
                        2U,
                        0x500U,
                        101U,
                        202U)
                   .status == detail::CompanionLedgerIssueStatus::SessionRetired,
           "retired session cannot issue later registrations");
}

void test_second_session_reissues_ids_without_accepting_old_tokens() {
    detail::CompanionTokenLedger first{{6U, 66U}, 5001U};
    const auto old_a = issued_token(
        first, protocol::LogicalWindowId::A, 1U, 0x100U);
    first.retire_all();

    detail::CompanionTokenLedger second{{7U, 77U}, 5002U};
    const auto new_a = issued_token(
        second, protocol::LogicalWindowId::A, 1U, 0x100U);
    expect(old_a.logical_id() == new_a.logical_id() &&
               old_a.generation() == new_a.generation() && old_a != new_a,
           "new session may reissue A/1 without token alias");
    expect(!second.resolve(old_a).has_value() &&
               second.resolve(new_a).has_value(),
           "session 2 rejects the session 1 token");
}

void test_top_level_provenance_predicate() {
    expect(detail::is_companion_top_level_provenance(true, false, false),
           "independent root top-level shape passes");
    expect(!detail::is_companion_top_level_provenance(false, false, false),
           "non-root shape fails");
    expect(!detail::is_companion_top_level_provenance(true, true, false),
           "WS_CHILD shape fails");
    expect(!detail::is_companion_top_level_provenance(true, false, true),
           "owned top-level shape fails");
}

void test_uncooperative_selection_is_d_only_and_in_batch() {
    const std::uint32_t a =
        protocol::logical_window_bit(protocol::LogicalWindowId::A);
    const std::uint32_t d =
        protocol::logical_window_bit(protocol::LogicalWindowId::D);
    expect(detail::is_valid_uncooperative_selection(0U, 0, a),
           "cooperative operation needs no test-only mask");
    expect(detail::is_valid_uncooperative_selection(d, 5, d),
           "D-only deterministic offset passes when D is requested");
    expect(!detail::is_valid_uncooperative_selection(a, 5, a),
           "controller rejects A/B/C uncooperative masks");
    expect(!detail::is_valid_uncooperative_selection(d, 5, a),
           "controller rejects D offset when D is outside the request set");
    expect(!detail::is_valid_uncooperative_selection(0U, 5, d),
           "nonzero offset requires the D bit");
    expect(!detail::is_valid_uncooperative_selection(
               d, protocol::kMaximumTestOffset + 1, d),
           "controller rejects an out-of-bound test offset");
}

void test_marker_value_preserves_logical_id_and_generation() {
    const auto a1 = protocol::marker_property_value(
        protocol::LogicalWindowId::A, 1U);
    const auto d1 = protocol::marker_property_value(
        protocol::LogicalWindowId::D, 1U);
    const auto a257 = protocol::marker_property_value(
        protocol::LogicalWindowId::A, 257U);
    expect(a1 != 0U && a1 != d1 && a1 != a257,
           "marker value distinguishes logical ID and generations beyond eight bits");
    expect(protocol::marker_property_value(
               protocol::LogicalWindowId::A,
               (std::numeric_limits<std::uint64_t>::max)()) == 0U,
           "unrepresentable marker generation fails closed");
}

void test_shared_visible_to_positioning_translation() {
    const geometry::Rect positioning{90, 90, 510, 410};
    const geometry::Rect visible{100, 100, 500, 400};
    const auto translated =
        operations::window_translation::prepare_visible_translation(
            positioning,
            visible,
            geometry::Rect{220, 160, 620, 460});
    expect(translated.status == operations::window_translation::
                                    TranslationPreparationStatus::Succeeded &&
               translated.dx == 120 && translated.dy == 60 &&
               translated.target_positioning_rect ==
                   geometry::Rect{210, 150, 630, 470},
           "companion and owned adapters share the same visible-frame bridge");

    const auto resize =
        operations::window_translation::prepare_visible_translation(
            positioning,
            visible,
            geometry::Rect{220, 160, 621, 460});
    expect(resize.status == operations::window_translation::
                                TranslationPreparationStatus::ResizeRejected,
           "shared bridge rejects resize instead of converting it");

    constexpr auto minimum =
        std::numeric_limits<geometry::Coordinate>::min();
    constexpr auto maximum =
        std::numeric_limits<geometry::Coordinate>::max();
    const auto overflow =
        operations::window_translation::prepare_visible_translation(
            positioning,
            geometry::Rect{minimum, 0, minimum + 10, 10},
            geometry::Rect{maximum - 10, 0, maximum, 10});
    expect(overflow.status == operations::window_translation::
                                  TranslationPreparationStatus::ArithmeticOverflow,
           "shared bridge keeps checked arithmetic at extreme coordinates");
}

} // namespace

int main() {
    test_token_surface_is_opaque_and_separate();
    test_controller_and_session_authorities_do_not_alias();
    test_generation_and_native_reuse_never_revive_old_token();
    test_unique_registration_and_token_set_preflight();
    test_whole_session_exit_retires_every_token();
    test_second_session_reissues_ids_without_accepting_old_tokens();
    test_top_level_provenance_predicate();
    test_uncooperative_selection_is_d_only_and_in_batch();
    test_marker_value_preserves_logical_id_and_generation();
    test_shared_visible_to_positioning_translation();

    if (failures != 0) {
        std::cerr << failures << " companion session test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All companion session tests passed\n";
    return EXIT_SUCCESS;
}
