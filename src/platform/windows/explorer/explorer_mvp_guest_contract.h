#pragma once

#include <windows.h>
#include <wtsapi32.h>

#include <algorithm>
#include <array>
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

inline constexpr wchar_t guest_marker[] = L"C:\\PaneBindMVP1\\Input\\run-id.txt";

inline bool mvp1_interactive_default_desktop() noexcept {
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!input) return false;
    wchar_t input_name[256]{}, current_name[256]{};
    DWORD bytes{};
    BOOL input_io = FALSE, current_io = FALSE;
    const bool same =
        GetUserObjectInformationW(input, UOI_NAME, input_name,
                                  sizeof(input_name), &bytes) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()),
                                  UOI_NAME, current_name,
                                  sizeof(current_name), &bytes) &&
        std::wstring_view(input_name) == current_name &&
        std::wstring_view(input_name) == L"Default" &&
        GetUserObjectInformationW(input, UOI_IO, &input_io, sizeof(input_io), &bytes) &&
        GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_IO,
                                  &current_io, sizeof(current_io), &bytes) &&
        input_io != FALSE && current_io != FALSE;
    CloseDesktop(input);
    if (!same) return false;

    LPWSTR data{};
    DWORD size{};
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                     WTS_CURRENT_SESSION, WTSSessionInfoEx,
                                     &data, &size)) return false;
    bool active = false;
    if (data && size >= sizeof(WTSINFOEXW)) {
        const auto& info = *reinterpret_cast<const WTSINFOEXW*>(data);
        active = info.Level == 1 &&
            info.Data.WTSInfoExLevel1.SessionState == WTSActive &&
            info.Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
    }
    WTSFreeMemory(data);
    return active;
}

// An accidental-host-run guard, not a security boundary against a forged
// guest. It runs before opening evidence or creating any GUI / Raw resource.
inline bool mvp1_guest_run_authorized(std::wstring_view run_id,
                            std::wstring_view evidence_log) noexcept {
    if (run_id.size() != 32 ||
        !std::all_of(run_id.begin(), run_id.end(), [](wchar_t ch) {
            return (ch >= L'0' && ch <= L'9') ||
                   (ch >= L'a' && ch <= L'f');
        })) return false;
    wchar_t user[256]{};
    DWORD user_length = static_cast<DWORD>(std::size(user));
    if (!GetUserNameW(user, &user_length) ||
        std::wstring_view(user) != L"WDAGUtilityAccount") return false;
    DWORD session{};
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session) ||
        session == 0 || !mvp1_interactive_default_desktop()) return false;

    wchar_t executable[MAX_PATH]{};
    const DWORD executable_length = GetModuleFileNameW(
        nullptr, executable, static_cast<DWORD>(std::size(executable)));
    if (executable_length == 0 || executable_length >= std::size(executable) ||
        !explorer::exact_mvp1_guest_launch_artifacts(
            std::wstring_view(executable, executable_length), run_id, evidence_log))
        return false;

    const auto marker_matches = [&](const wchar_t* path) noexcept {
        const HANDLE marker = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (marker == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER size{};
        std::array<char, 32> bytes{};
        DWORD read{};
        const bool valid = GetFileSizeEx(marker, &size) &&
            size.QuadPart == static_cast<LONGLONG>(bytes.size()) &&
            ReadFile(marker, bytes.data(), static_cast<DWORD>(bytes.size()),
                     &read, nullptr) && read == bytes.size() &&
            std::equal(bytes.begin(), bytes.end(), run_id.begin(),
                [](char lhs, wchar_t rhs) {
                    return static_cast<unsigned char>(lhs) == rhs;
                });
        CloseHandle(marker);
        return valid;
    };
    return marker_matches(guest_marker) &&
        marker_matches(L"C:\\PaneBindMVP1\\Output\\run-id.txt");
}

} // namespace panebind::platform::windows::explorer
