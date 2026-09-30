#pragma once

#include <windows.h>

#include <climits>
#include <string_view>

namespace panebind::platform::windows::explorer {

inline bool exact_mvp1_guest_launch_artifacts(std::wstring_view executable,
                                               std::wstring_view run_id,
                                               std::wstring_view evidence_log) noexcept {
    constexpr std::wstring_view release_exe =
        L"C:\\PaneBindMVP1\\Input\\panebind-explorer-mvp1.exe";
    constexpr std::wstring_view debug_exe =
        L"C:\\PaneBindMVP1\\Input\\Debug\\panebind-explorer-mvp1.exe";
    constexpr std::wstring_view output = L"C:\\PaneBindMVP1\\Output\\";
    constexpr std::wstring_view release_suffix = L"-explorer-mvp1.jsonl";
    constexpr std::wstring_view debug_suffix = L"-explorer-mvp1-debug.jsonl";
    const auto equal = [](std::wstring_view lhs, std::wstring_view rhs) noexcept {
        return lhs.size() <= INT_MAX && rhs.size() <= INT_MAX &&
            CompareStringOrdinal(lhs.data(), static_cast<int>(lhs.size()),
                                 rhs.data(), static_cast<int>(rhs.size()), TRUE) == CSTR_EQUAL;
    };
    const auto log_matches = [&](std::wstring_view suffix) noexcept {
        if (evidence_log.size() != output.size() + run_id.size() + suffix.size())
            return false;
        return equal(evidence_log.substr(0, output.size()), output) &&
            equal(evidence_log.substr(output.size(), run_id.size()), run_id) &&
            equal(evidence_log.substr(output.size() + run_id.size()), suffix);
    };
    return (equal(executable, release_exe) && log_matches(release_suffix)) ||
           (equal(executable, debug_exe) && log_matches(debug_suffix));
}

} // namespace panebind::platform::windows::explorer
