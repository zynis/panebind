#pragma once

#include "core/model/window_snapshot.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iosfwd>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace panebind::platform::windows {

struct FieldError {
    std::string api;
    std::string field;
    std::string domain;
    std::uint64_t code{};
};

struct NativeRect {
    std::int64_t left{};
    std::int64_t top{};
    std::int64_t right{};
    std::int64_t bottom{};
};

struct WindowsWindowSnapshot {
    HWND window{};
    DWORD process_id{};
    DWORD window_thread_id{};
    std::optional<std::string> process_path;
    std::optional<std::string> process_name;
    std::optional<std::string> title;
    std::optional<std::string> class_name;
    std::optional<std::uint64_t> style;
    std::optional<std::uint64_t> extended_style;
    bool visible{};
    bool minimized{};
    bool maximized{};
    std::optional<DWORD> cloak_mask;
    std::optional<NativeRect> positioning_bounds;
    std::optional<NativeRect> visible_frame_bounds;
    HMONITOR monitor{};
    std::optional<std::string> monitor_device_name;
    std::optional<NativeRect> monitor_bounds;
    std::optional<NativeRect> monitor_work_area;
    std::optional<bool> monitor_primary;
    std::optional<UINT> window_dpi;
    std::optional<std::string> window_dpi_awareness;
    std::chrono::system_clock::time_point captured_at;
    bool dpi_geometry_context_valid{};
    bool identity_valid{};
    std::vector<FieldError> field_errors;
};

[[nodiscard]] core::model::NormalizedWindowSnapshot
to_normalized_window_snapshot(const WindowsWindowSnapshot& snapshot);

class WindowsObserver final {
public:
    explicit WindowsObserver(std::ostream& output);
    ~WindowsObserver();

    WindowsObserver(const WindowsObserver&) = delete;
    WindowsObserver& operator=(const WindowsObserver&) = delete;

    [[nodiscard]] int enumerate_only();
    [[nodiscard]] int observe(std::optional<std::chrono::seconds> duration);

private:
    struct RawWinEvent {
        HWINEVENTHOOK hook{};
        DWORD event{};
        HWND window{};
        LONG object_id{};
        LONG child_id{};
        DWORD event_thread_id{};
        DWORD native_event_time_ms{};
        DWORD callback_window_thread_id{};
        DWORD callback_process_id{};
        bool callback_root_matches{};
        std::uint64_t sequence{};
        std::chrono::system_clock::time_point received_at;
    };

    struct SnapshotResult {
        std::optional<WindowsWindowSnapshot> snapshot;
        std::string disposition;
        bool observed_snapshot{};
        bool default_candidate{};
        std::vector<FieldError> field_errors;
    };

    static void CALLBACK win_event_callback(HWINEVENTHOOK hook,
                                            DWORD event,
                                            HWND window,
                                            LONG object_id,
                                            LONG child_id,
                                            DWORD event_thread_id,
                                            DWORD event_time_ms);
    static BOOL CALLBACK enum_windows_callback(HWND window, LPARAM context);

    void receive_win_event(HWINEVENTHOOK hook,
                           DWORD event,
                           HWND window,
                           LONG object_id,
                           LONG child_id,
                           DWORD event_thread_id,
                           DWORD event_time_ms) noexcept;
    [[nodiscard]] bool initialize_owner_thread();
    [[nodiscard]] bool install_hooks();
    [[nodiscard]] bool uninstall_hooks();
    [[nodiscard]] bool enumerate_windows();
    void drain_event_queue();
    void emit_event(const RawWinEvent& event);
    void emit_census_snapshot(HWND window);
    void emit_overflow_diagnostic();
    void write_json_line(std::string_view record) noexcept;
    void emit_diagnostic(std::string_view name,
                         std::string_view disposition,
                         const std::vector<FieldError>& errors = {});
    [[nodiscard]] SnapshotResult inspect_window(
        HWND window,
        std::optional<DWORD> expected_process_id,
        std::optional<DWORD> expected_window_thread_id);
    [[nodiscard]] WindowsWindowSnapshot capture_snapshot(HWND window,
                                                         DWORD process_id,
                                                         DWORD window_thread_id);
    [[nodiscard]] std::uint64_t next_sequence() noexcept;
    [[nodiscard]] bool verify_thread_dpi_context();

    std::ostream& output_;
    DWORD owner_thread_id_{};
    DWORD own_process_id_{};
    HWINEVENTHOOK move_size_hook_{};
    HWINEVENTHOOK location_hook_{};
    bool thread_dpi_context_valid_{};
    std::string thread_dpi_awareness_;
    bool callback_target_active_{};
    std::atomic<std::uint64_t> next_sequence_{1U};
    std::mutex queue_mutex_;
    std::deque<RawWinEvent> event_queue_;
    std::atomic<bool> queue_notification_pending_{false};
    std::uint64_t dropped_event_count_{};
    std::uint64_t first_dropped_sequence_{};
    std::uint64_t last_dropped_sequence_{};
    std::uint64_t post_message_failure_count_{};
    DWORD last_post_message_error_{};
    bool output_failed_{};
};

} // namespace panebind::platform::windows
