#include "platform/windows/product/window_writer.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/operations/window_rect_adjustment.h"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_set>

namespace panebind::platform::windows::product {
namespace {

using core::v1::Limits;
using core::v1::Rect;

std::uint64_t now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) && value.QuadPart > 0 ?
        static_cast<std::uint64_t>(value.QuadPart) : 0;
}

bool native_rect(const Rect& value) noexcept {
    using core::geometry::checked_difference;
    const auto width = checked_difference(value.right(), value.left());
    const auto height = checked_difference(value.bottom(), value.top());
    if (!width || !height || *width <= 0 || *height <= 0) return false;
    constexpr auto lo = std::numeric_limits<std::int32_t>::min();
    constexpr auto hi = std::numeric_limits<std::int32_t>::max();
    for (const auto coordinate : std::array{value.left(), value.top(), value.right(),
        value.bottom(), *width, *height}) {
        if (coordinate < lo || coordinate > hi) return false;
    }
    return true;
}

bool legal_size(const Rect& value, const Limits& limits) noexcept {
    if (limits.min_width <= 0 || limits.min_height <= 0 ||
        limits.max_width < limits.min_width || limits.max_height < limits.min_height ||
        limits.grid_width <= 0 || limits.grid_height <= 0) return false;
    return value.width() >= limits.min_width && value.width() <= limits.max_width &&
        value.height() >= limits.min_height && value.height() <= limits.max_height &&
        value.width() % limits.grid_width == 0 && value.height() % limits.grid_height == 0;
}

bool consistent(const Snapshot& expected, const Snapshot& current, std::string& reason) {
    if (!Catalog::same(expected, current)) { reason = "window_identity_or_generation_changed"; return false; }
    if (expected.visible != current.visible || expected.positioning != current.positioning) {
        reason = "unattributed_geometry_change"; return false;
    }
    if (expected.dpi != current.dpi || expected.monitor != current.monitor ||
        expected.work != current.work || expected.maximized != current.maximized ||
        expected.can_resize != current.can_resize || expected.minimized != current.minimized) {
        reason = "geometry_context_changed"; return false;
    }
    return true;
}

bool target_allowed(const Snapshot& expected, const Snapshot& current,
    const Rect& desired, std::string& reason) {
    if (!native_rect(desired)) { reason = "invalid_or_out_of_native_range_target"; return false; }
    const bool resizing = desired.size() != current.visible.size();
    if(current.minimized){reason="restore_minimized_window_before_geometry_command";return false;}
    if (resizing && (!expected.can_resize || !current.can_resize)) {
        reason = "window_does_not_support_resize"; return false;
    }
    if (!legal_size(desired, expected.limits) || !legal_size(desired, current.limits)) {
        reason = "target_violates_size_constraints"; return false;
    }
    if (current.maximized && desired != current.visible) {
        reason = "restore_maximized_window_before_geometry_command"; return false;
    }
    return true;
}

bool allowed_by_guard(const WindowWriter::Guard& guard, std::string& reason) {
    if (!guard) { reason = "missing_operation_guard"; return false; }
    try {
        if (guard()) return true;
        reason = "operation_revoked_before_native_attempt";
    } catch (...) { reason = "operation_guard_failed"; }
    return false;
}

WindowWriter::Receipt rejected(const Snapshot& expected, const Rect& desired, std::string reason) {
    WindowWriter::Receipt result;
    result.before = expected;
    result.requested = desired;
    result.reason = std::move(reason);
    return result;
}

} // namespace

WindowWriter::WindowWriter(Catalog& catalog) noexcept : catalog_(catalog) {}

WindowWriter::Preparation WindowWriter::prepare(const Snapshot& expected,
    const Snapshot& actual, const Rect& desired) {
    Preparation result;
    result.identity_valid = Catalog::same(expected, actual);
    if (!native_rect(expected.visible) || !native_rect(expected.positioning) ||
        !native_rect(actual.visible) || !native_rect(actual.positioning)) {
        result.reason = "invalid_or_out_of_native_range_frame";
        return result;
    }
    if (!consistent(expected, actual, result.reason) ||
        !target_allowed(expected, actual, desired, result.reason)) return result;
    const auto converted = operations::prepare_visible_rect_adjustment(actual.positioning, actual.visible, desired);
    if (converted.status != operations::RectAdjustmentStatus::Succeeded || !converted.positioning) {
        result.reason = "positioning_bridge_invalid_or_out_of_native_range";
        return result;
    }
    result.positioning = converted.positioning;
    result.noop = actual.visible == desired && actual.positioning == *result.positioning;
    result.accepted = true;
    result.reason = result.noop ? "already_exact_no_native_call" : "prepared";
    return result;
}

WindowWriter::Receipt WindowWriter::place(const Snapshot& expected,
    const Rect& desired, const core::v1::Settings& settings, Guard guard) {
    Receipt result = rejected(expected, desired, "not_attempted");
    if (!native_rect(desired)) {
        result.reason = "invalid_or_out_of_native_range_target";
        return result;
    }
    std::string capture_reason;
    const auto before = catalog_.get(expected.hwnd, settings, &capture_reason);
    if (!before) {
        result.reason = "preflight_" + capture_reason;
        return result;
    }
    result.before = *before;
    const auto prepared = prepare(expected, *before, desired);
    result.identity_valid = prepared.identity_valid;
    if (!prepared.accepted || !prepared.positioning) { result.reason = prepared.reason; return result; }
    if (prepared.noop) {
        // This permission check does not create an API attempt. Even an exact
        // no-op may not be credited after the caller has revoked its operation.
        if (!allowed_by_guard(guard, result.reason)) return result;
        result.after = *before;
        result.accepted = result.geometry_exact = true;
        result.reason = "already_exact_no_native_call";
        return result;
    }

    const auto& native = *prepared.positioning;
    result.before_qpc = now();
    if (!result.before_qpc) { result.reason = "native_attempt_clock_unavailable"; return result; }
    if (!allowed_by_guard(guard, result.reason)) return result;
    // No capture, COM, lock, dispatch or authority work between the final
    // runtime claim and this call. It is not an atomic OS cancellation API;
    // a later UP retires subsequent requests, not this issued native operation.
    result.native_attempted = true;
    const BOOL success = SetWindowPos(before->hwnd, nullptr,
        static_cast<int>(native.left()), static_cast<int>(native.top()),
        static_cast<int>(native.width()), static_cast<int>(native.height()),
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    result.error = success ? ERROR_SUCCESS : GetLastError();
    result.native_succeeded = success != FALSE;
    result.after_qpc = now();

    // This is observation of an issued call, not a new permission request.
    // In particular, do not call guard again: a matching normal UP must not
    // turn an otherwise exact completion into a false authority failure.
    result.after = catalog_.get(before->hwnd, settings, &capture_reason);
    result.identity_valid = result.after && Catalog::same(*before, *result.after);
    result.geometry_exact = result.identity_valid && result.after->visible == desired &&
        result.after->positioning == native;
    if (!result.native_succeeded) result.reason = "native_placement_failed";
    else if (!result.after) result.reason = "postverify_" + capture_reason;
    else if (!result.identity_valid) result.reason = "identity_changed_after_native_attempt";
    else if (!result.geometry_exact) result.reason = "native_result_not_exact_or_app_constrained";
    else if (!result.after_qpc) result.reason = "native_completion_clock_unavailable";
    else { result.accepted = true; result.reason = "exact_native_completion"; }
    return result;
}

std::vector<WindowWriter::Receipt> WindowWriter::batch(std::span<const Snapshot> identities,
    std::span<const core::v1::Placement> placements,
    const core::v1::Settings& settings, Guard guard) {
    std::vector<Receipt> result;
    // Bound the product batch, independently of the untrusted native desktop.
    if (identities.empty() || identities.size() > 256 || placements.empty() ||
        placements.size() > identities.size()) {
        result.push_back(rejected({}, {}, "invalid_or_unbounded_batch"));
        return result;
    }
    std::unordered_set<core::v1::Key> members, targets;
    std::unordered_set<HWND> windows;
    for (const auto& expected : identities) {
        if (!expected.key || !expected.hwnd || !members.insert(expected.key).second ||
            !windows.insert(expected.hwnd).second) {
            result.push_back(rejected(expected, expected.visible, "duplicate_or_invalid_batch_identity"));
            return result;
        }
    }
    for (const auto& placement : placements) {
        if (!members.contains(placement.id) || !targets.insert(placement.id).second) {
            result.push_back(rejected({}, placement.visible, "duplicate_or_out_of_scope_batch_target"));
            return result;
        }
    }
    // Complete ALL source/participant preflights before the first native call.
    // Fresh geometry cannot silently become a new baseline for a frozen plan.
    for (const auto& expected : identities) {
        std::string reason;
        const auto current = catalog_.get(expected.hwnd, settings, &reason);
        if (!current || !consistent(expected, *current, reason)) {
            result.push_back(rejected(expected, expected.visible, "batch_preflight_" + reason));
            return result;
        }
        const auto placement = std::find_if(placements.begin(), placements.end(),
            [&](const auto& target) { return target.id == expected.key; });
        if (placement != placements.end()) {
            const auto prepared = prepare(expected, *current, placement->visible);
            if (!prepared.accepted) {
                result.push_back(rejected(expected, placement->visible, "batch_preflight_" + prepared.reason));
                return result;
            }
        }
    }
    result.reserve(placements.size());
    for (const auto& placement : placements) {
        const auto expected = std::find_if(identities.begin(), identities.end(),
            [&](const auto& member) { return member.key == placement.id; });
        result.push_back(place(*expected, placement.visible, settings, guard));
        if (!result.back().accepted) break;
    }
    return result;
}

} // namespace panebind::platform::windows::product
