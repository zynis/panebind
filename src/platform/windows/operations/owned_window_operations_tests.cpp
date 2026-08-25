#include "platform/windows/operations/owned_window_operations_internal.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace geometry = panebind::core::geometry;
namespace operations = panebind::platform::windows::operations;
namespace detail = panebind::platform::windows::operations::detail;

namespace {

int failures = 0;

void expect(const bool condition, const std::string_view description) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

[[nodiscard]] operations::OwnedWindowToken issued_token(
    detail::TokenLedger& ledger,
    const std::uintptr_t native_key,
    const std::uint32_t process_id = 100U,
    const std::uint32_t thread_id = 200U) {
    const auto result = ledger.issue(native_key, process_id, thread_id);
    if (!result.token.has_value()) {
        std::cerr << "FAIL: test token issuance failed\n";
        std::exit(EXIT_FAILURE);
    }
    return *result.token;
}

void test_token_is_opaque() {
    static_assert(!std::is_default_constructible_v<operations::OwnedWindowToken>);
    static_assert(!std::is_constructible_v<operations::OwnedWindowToken,
                                           std::uint64_t,
                                           std::uint64_t>);
    static_assert(!std::is_constructible_v<operations::OwnedWindowToken,
                                           std::uint64_t,
                                           std::uint64_t,
                                           std::uint64_t>);
    static_assert(!std::is_convertible_v<operations::OwnedWindowToken, HWND>);
    static_assert(!std::is_convertible_v<HWND, operations::OwnedWindowToken>);
    expect(true, "OwnedWindowToken has no public native-handle construction");
}

void test_process_monotonic_id_does_not_wrap() {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    detail::MonotonicIdSource near_exhaustion{maximum - 1U};
    expect(near_exhaustion.issue() == maximum - 1U,
           "monotonic source issues the last safely incrementable value");
    expect(near_exhaustion.issue() == 0U && near_exhaustion.issue() == 0U,
           "monotonic source reports permanent exhaustion without wrapping");

    detail::MonotonicIdSource already_exhausted{0U};
    expect(already_exhausted.issue() == 0U,
           "zero sentinel remains exhausted");
}

void test_registry_authorities_do_not_alias() {
    detail::TokenLedger first_ledger;
    detail::TokenLedger second_ledger;
    const auto first_token = issued_token(first_ledger, 0x100U);
    const auto second_token = issued_token(second_ledger, 0x200U);

    expect(first_token.logical_id() == second_token.logical_id() &&
               first_token.generation() == second_token.generation(),
           "test reproduces equal local id/generation across two ledgers");
    expect(first_token != second_token,
           "private authority identity distinguishes otherwise equal tokens");
    expect(first_ledger.resolve(first_token).has_value() &&
               second_ledger.resolve(second_token).has_value(),
           "each ledger resolves its locally issued first token");
    expect(!first_ledger.resolve(second_token).has_value() &&
               !second_ledger.resolve(first_token).has_value(),
           "both cross-ledger token resolutions are rejected");
}

void test_token_ledger_generation_and_stale_rejection() {
    detail::TokenLedger ledger;
    expect(ledger.issue(0U, 1U, 2U).status ==
               detail::LedgerIssueStatus::InvalidNativeKey,
           "ledger rejects a null native key");

    const auto first = issued_token(ledger, 0x100U);
    const auto first_entry = ledger.resolve(first);
    expect(first_entry.has_value() && first_entry->native_key == 0x100U &&
               first_entry->process_id == 100U &&
               first_entry->creator_thread_id == 200U,
           "issued token resolves to its recorded native identity");
    expect(ledger.issue(0x100U, 100U, 200U).status ==
               detail::LedgerIssueStatus::DuplicateNativeKey,
           "ledger rejects duplicate active registration");

    expect(ledger.retire_native(0x100U),
           "invalidation retires the active native registration");
    expect(!ledger.resolve(first).has_value(),
           "retired token is immediately stale");
    expect(!ledger.retire_native(0x100U),
           "repeated invalidation reports no active registration");

    const auto second = issued_token(ledger, 0x100U);
    expect(second != first && second.generation() > first.generation(),
           "reused native value receives a distinct token generation");
    expect(!ledger.resolve(first).has_value() && ledger.resolve(second).has_value(),
           "native-value reuse never revives the old token");

    detail::TokenLedger exhausted{
        std::numeric_limits<std::uint64_t>::max(), 1U};
    expect(exhausted.issue(0x200U, 1U, 2U).status ==
               detail::LedgerIssueStatus::GenerationExhausted,
           "logical identifier exhaustion fails without wrapping");
    detail::TokenLedger generation_exhausted{
        1U, std::numeric_limits<std::uint64_t>::max()};
    expect(generation_exhausted.issue(0x200U, 1U, 2U).status ==
               detail::LedgerIssueStatus::GenerationExhausted,
           "generation exhaustion fails without wrapping");
}

void test_batch_token_preflight() {
    detail::TokenLedger ledger;
    const auto first = issued_token(ledger, 0x100U);
    const auto second = issued_token(ledger, 0x200U);

    const std::vector<operations::OwnedWindowToken> empty;
    expect(detail::validate_batch_tokens(empty, ledger) ==
               detail::BatchTokenValidation::Empty,
           "empty batch fails before native apply");

    const std::vector duplicate{first, first};
    expect(detail::validate_batch_tokens(duplicate, ledger) ==
               detail::BatchTokenValidation::Duplicate,
           "duplicate capability fails before native apply");

    const std::vector valid{second, first};
    expect(detail::validate_batch_tokens(valid, ledger) ==
               detail::BatchTokenValidation::Succeeded,
           "distinct active capabilities pass ledger preflight");

    expect(ledger.retire_native(0x100U),
           "test setup retires one batch member");
    const std::vector invalid_member{second, first};
    expect(detail::validate_batch_tokens(invalid_member, ledger) ==
               detail::BatchTokenValidation::Unknown,
           "mixed valid/stale batch fails before native apply");

    detail::TokenLedger other_ledger;
    const std::vector foreign{second};
    expect(detail::validate_batch_tokens(foreign, other_ledger) ==
               detail::BatchTokenValidation::Unknown,
           "token from another ledger is not a capability in this registry");
}

void test_operation_batch_ids_are_process_unique() {
    operations::OwnedWindowRegistry first_registry;
    operations::OwnedWindowRegistry second_registry;
    operations::OwnedWindowOperations first_operations{first_registry};
    operations::OwnedWindowOperations second_operations{second_registry};
    const std::vector<operations::WindowTranslationRequest> empty;

    const auto first = first_operations.apply(empty);
    const auto second = second_operations.apply(empty);
    expect(first.status == operations::OperationStatus::InvalidRequest &&
               second.status == operations::OperationStatus::InvalidRequest,
           "empty operations remain deterministic preflight failures");
    expect(first.operation_batch_id != 0U &&
               second.operation_batch_id != 0U &&
               first.operation_batch_id != second.operation_batch_id,
           "operation batch ids do not duplicate across registries");
}

void test_independent_top_level_provenance_predicate() {
    expect(detail::is_independent_top_level_provenance(true, false, false),
           "self-rooted non-child unowned window passes provenance shape");
    expect(!detail::is_independent_top_level_provenance(false, false, false),
           "non-root window is rejected by capability provenance");
    expect(!detail::is_independent_top_level_provenance(true, true, false),
           "WS_CHILD window is rejected by capability provenance");
    expect(!detail::is_independent_top_level_provenance(true, false, true),
           "owned top-level window is rejected by capability provenance");
    expect(!detail::is_independent_top_level_provenance(false, true, true),
           "combined provenance violations remain rejected");
}

void test_visible_to_positioning_translation_bridge() {
    const geometry::Rect positioning{90, 90, 510, 410};
    const geometry::Rect visible{100, 100, 500, 400};
    const geometry::Rect target_visible{220, 160, 620, 460};
    const auto result = detail::bridge_visible_translation(
        positioning, visible, target_visible);
    expect(result.status == detail::TranslationBridgeStatus::Succeeded &&
               result.dx == 120 && result.dy == 60 &&
               result.target_positioning_rect ==
                   geometry::Rect{210, 150, 630, 470},
           "visible delta translates positioning rect without conflating frames");
    expect(result.target_positioning_rect->size() == positioning.size(),
           "translation bridge preserves positioning size");

    const auto negative = detail::bridge_visible_translation(
        positioning,
        visible,
        geometry::Rect{-20, 40, 380, 340});
    expect(negative.status == detail::TranslationBridgeStatus::Succeeded &&
               negative.dx == -120 && negative.dy == -60 &&
               negative.target_positioning_rect ==
                   geometry::Rect{-30, 30, 390, 350},
           "translation bridge supports checked negative deltas");

    const auto unchanged = detail::bridge_visible_translation(
        positioning, visible, visible);
    expect(unchanged.status == detail::TranslationBridgeStatus::Succeeded &&
               unchanged.dx == 0 && unchanged.dy == 0 &&
               unchanged.target_positioning_rect == positioning,
           "unchanged target remains a valid zero translation");
}

void test_bridge_rejects_resize_empty_and_overflow() {
    const geometry::Rect positioning{90, 90, 510, 410};
    const geometry::Rect visible{100, 100, 500, 400};

    expect(detail::bridge_visible_translation(
               positioning, visible, geometry::Rect{100, 100, 501, 400})
               .status == detail::TranslationBridgeStatus::ResizeRejected,
           "visible width change is rejected instead of becoming native resize");
    expect(detail::bridge_visible_translation(
               positioning, visible, geometry::Rect{100, 100, 500, 401})
               .status == detail::TranslationBridgeStatus::ResizeRejected,
           "visible height change is rejected instead of becoming native resize");
    expect(detail::bridge_visible_translation(
               positioning,
               geometry::Rect{100, 100, 100, 400},
               geometry::Rect{200, 100, 200, 400})
               .status == detail::TranslationBridgeStatus::EmptyGeometry,
           "empty visible geometry fails preflight");

    constexpr auto minimum = std::numeric_limits<geometry::Coordinate>::min();
    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    expect(detail::bridge_visible_translation(
               positioning,
               geometry::Rect{minimum, 0, minimum + 10, 10},
               geometry::Rect{maximum - 10, 0, maximum, 10})
               .status == detail::TranslationBridgeStatus::ArithmeticOverflow,
           "unrepresentable visible delta fails checked arithmetic");
    expect(detail::bridge_visible_translation(
               geometry::Rect{maximum - 10, 0, maximum, 10},
               geometry::Rect{0, 0, 10, 10},
               geometry::Rect{1, 0, 11, 10})
               .status == detail::TranslationBridgeStatus::ArithmeticOverflow,
           "overflowing positioning translation fails without partial target");

    constexpr auto native_maximum = std::numeric_limits<int>::max();
    expect(detail::bridge_visible_translation(
               geometry::Rect{native_maximum - 10, 0, native_maximum, 10},
               geometry::Rect{0, 0, 10, 10},
               geometry::Rect{1, 0, 11, 10})
               .status ==
               detail::TranslationBridgeStatus::NativeCoordinateOutOfRange,
           "target outside Win32 int coordinate domain fails preflight");
    expect(detail::bridge_visible_translation(
               positioning,
               visible,
               geometry::Rect{minimum, 0, maximum, 10})
               .status == detail::TranslationBridgeStatus::ArithmeticOverflow,
           "unrepresentable target extent is rejected without signed overflow");
}

} // namespace

int main() {
    test_token_is_opaque();
    test_process_monotonic_id_does_not_wrap();
    test_registry_authorities_do_not_alias();
    test_token_ledger_generation_and_stale_rejection();
    test_batch_token_preflight();
    test_operation_batch_ids_are_process_unique();
    test_independent_top_level_provenance_predicate();
    test_visible_to_positioning_translation_bridge();
    test_bridge_rejects_resize_empty_and_overflow();

    if (failures != 0) {
        std::cerr << failures << " owned-window operations test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All owned-window operations tests passed\n";
    return EXIT_SUCCESS;
}
