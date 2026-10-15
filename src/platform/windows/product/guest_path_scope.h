#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <utility>

namespace panebind::platform::windows::product::guest {

// Compare two already-scoped Windows directory spellings, not a path prefix.
// canonical/weakly_canonical may retain an empty final component ("\").
// Removing that empty component must never strip a drive/share root or allow
// a parent, sibling, relative path, different RunId or different volume.
inline bool same_exact_directory(std::filesystem::path actual,
                                 std::filesystem::path expected) {
    if (!actual.is_absolute() || !expected.is_absolute()) return false;
    actual.make_preferred();
    expected.make_preferred();
    const auto trim = [](std::filesystem::path path) {
        while (path.has_relative_path() && path.filename().empty())
            path = path.parent_path();
        return path;
    };
    actual = trim(std::move(actual));
    expected = trim(std::move(expected));
    auto left = actual.begin(), right = expected.begin();
    for (; left != actual.end() && right != expected.end(); ++left, ++right) {
        const auto a = left->wstring(), b = right->wstring();
        if (CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) != CSTR_EQUAL)
            return false;
    }
    return left == actual.end() && right == expected.end();
}

} // namespace panebind::platform::windows::product::guest
