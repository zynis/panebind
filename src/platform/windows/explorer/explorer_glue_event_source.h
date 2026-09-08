#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "platform/windows/explorer/explorer_glue_types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace panebind::platform::windows::explorer {

class ExplorerGlueSession;
#if defined(PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING)
namespace detail {
class ExplorerGlueEventSourceTestAccess;
}
#endif

enum class ExplorerGlueEventKind : std::uint8_t {
    MoveResizeStarted,
    GeometryChanged,
    MoveResizeEnded,
    TargetDestroyed,
};

enum class ExplorerGlueEventSourcePoison : std::uint8_t {
    None,
    InvalidBinding,
    SourceAlreadyActive,
    HookInstallFailure,
    HookUnhookFailure,
    HookReceiptMismatch,
    QueueOverflow,
    NotificationFailure,
    ReentrantCallback,
    ReentrantDrain,
    WrongOwnerThread,
    TargetIdentityMismatch,
    TargetNotRoot,
    SequenceExhausted,
};

struct ExplorerGlueEvent final {
    ExplorerGlueEventKind kind{ExplorerGlueEventKind::GeometryChanged};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
    std::uint64_t window_id{};
    std::uint64_t capability_generation{};
    std::uint64_t receipt_sequence{};
    DWORD native_event_thread{};
    DWORD native_event_time{};
};

struct ExplorerGlueEventSourceFacts final {
    ExplorerGlueEventSourcePoison poison{
        ExplorerGlueEventSourcePoison::None};
    DWORD owner_thread_id{};
    DWORD hook_flags{};
    std::size_t queue_capacity{};
    std::size_t queue_depth{};
    std::size_t max_queue_depth{};
    std::size_t unique_target_process_count{};
    std::size_t lifecycle_hook_count{};
    std::size_t location_hook_count{};
    std::size_t destroy_hook_count{};
    std::uint64_t latest_receipt_sequence{};
    std::uint64_t accepted_count{};
    std::uint64_t ignored_other_window_count{};
    std::uint64_t ignored_object_child_count{};
    std::uint64_t ignored_event_count{};
    std::uint64_t overflow_count{};
    std::uint64_t notification_failure_count{};
    std::uint64_t wrong_thread_count{};
    std::uint64_t reentrant_count{};
    std::uint64_t post_poison_count{};
    bool running{};
    bool live_hooks_installed{};
};

struct ExplorerGlueEventDrainResult final {
    std::vector<ExplorerGlueEvent> events;
    ExplorerGlueEventSourceFacts facts;
};

// This source has no caller-facing HWND constructor or factory. Only the
// ExplorerGlueSession authority can bind its two already-authorized Explorer
// targets. The callback records fixed receipts; all identity checks and event
// interpretation happen when the owner thread drains the queue.
class ExplorerGlueEventSource final {
public:
    ~ExplorerGlueEventSource();

    ExplorerGlueEventSource(const ExplorerGlueEventSource&) = delete;
    ExplorerGlueEventSource& operator=(const ExplorerGlueEventSource&) = delete;
    ExplorerGlueEventSource(ExplorerGlueEventSource&&) = delete;
    ExplorerGlueEventSource& operator=(ExplorerGlueEventSource&&) = delete;

private:
    struct NativeTargetBinding final {
        HWND window{};
        DWORD process_id{};
        DWORD thread_id{};
        std::uint64_t window_id{};
        std::uint64_t capability_generation{};
        ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
    };

    enum class DeliveryMode : std::uint8_t {
        Live,
        Synthetic,
    };

    struct RawReceipt;
    struct HookSlot;

    ExplorerGlueEventSource(NativeTargetBinding leader,
                            NativeTargetBinding follower,
                            std::size_t queue_capacity,
                            DeliveryMode mode,
                            bool synthetic_notification_succeeds);

    [[nodiscard]] bool start_live();
    [[nodiscard]] bool stop_live() noexcept;
    [[nodiscard]] ExplorerGlueEventDrainResult drain_owner_queue();
    [[nodiscard]] ExplorerGlueEventSourceFacts facts() const noexcept;
    [[nodiscard]] bool owns_notification(UINT message, WPARAM cookie) const
        noexcept;
    [[nodiscard]] static constexpr UINT notification_message() noexcept {
        return WM_APP + 0x42U;
    }

    void receive_raw_event(HWINEVENTHOOK hook,
                           DWORD event,
                           HWND window,
                           LONG object_id,
                           LONG child_id,
                           DWORD event_thread,
                           DWORD event_time) noexcept;
    void mark_poison(ExplorerGlueEventSourcePoison poison) noexcept;
    [[nodiscard]] bool post_owner_notification() noexcept;
    [[nodiscard]] bool validate_binding(const NativeTargetBinding& binding)
        noexcept;
    [[nodiscard]] const NativeTargetBinding* find_binding(HWND window) const
        noexcept;
    [[nodiscard]] const HookSlot* find_hook_slot(HWINEVENTHOOK hook) const
        noexcept;
    void install_hooks_for_process(DWORD process_id);
    void install_synthetic_slots_for_process(DWORD process_id);
    void unhook_all() noexcept;
    void stop_synthetic(bool inject_unhook_failure) noexcept;

    static void CALLBACK win_event_callback(HWINEVENTHOOK hook,
                                             DWORD event,
                                             HWND window,
                                             LONG object_id,
                                             LONG child_id,
                                             DWORD event_thread,
                                             DWORD event_time) noexcept;

    NativeTargetBinding leader_{};
    NativeTargetBinding follower_{};
    DWORD owner_thread_id_{};
    std::uintptr_t notification_cookie_{};
    DeliveryMode delivery_mode_{DeliveryMode::Live};
    bool synthetic_notification_succeeds_{true};
    bool running_{};
    bool live_hooks_installed_{};
    bool notification_pending_{};
    bool callback_in_progress_{};
    bool drain_in_progress_{};
    bool unhook_failure_observed_{};
    std::atomic<ExplorerGlueEventSourcePoison> poison_{
        ExplorerGlueEventSourcePoison::None};
    std::unique_ptr<RawReceipt[]> queue_;
    std::size_t queue_capacity_{};
    std::size_t queue_head_{};
    std::size_t queue_size_{};
    std::size_t max_queue_depth_{};
    std::unique_ptr<HookSlot[]> hooks_;
    std::size_t hook_capacity_{};
    std::size_t hook_count_{};
    std::size_t unique_target_process_count_{};
    std::size_t lifecycle_hook_count_{};
    std::size_t location_hook_count_{};
    std::size_t destroy_hook_count_{};
    std::uint64_t next_receipt_sequence_{1U};
    std::uint64_t latest_receipt_sequence_{};
    std::uint64_t accepted_count_{};
    std::uint64_t ignored_other_window_count_{};
    std::uint64_t ignored_object_child_count_{};
    std::uint64_t ignored_event_count_{};
    std::uint64_t overflow_count_{};
    std::uint64_t notification_failure_count_{};
    std::atomic<std::uint64_t> wrong_thread_count_{};
    std::uint64_t reentrant_count_{};
    std::uint64_t post_poison_count_{};

    friend class ExplorerGlueSession;
#if defined(PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING)
    friend class detail::ExplorerGlueEventSourceTestAccess;
#endif
};

#if defined(PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING)

namespace detail {

struct ExplorerGlueEventSourceTestBinding final {
    HWND window{};
    DWORD process_id{};
    DWORD thread_id{};
    std::uint64_t window_id{};
    std::uint64_t capability_generation{};
    ExplorerGlueWindowRole role{ExplorerGlueWindowRole::Leader};
};

struct ExplorerGlueSyntheticWinEvent final {
    DWORD event{};
    HWND window{};
    LONG object_id{OBJID_WINDOW};
    LONG child_id{CHILDID_SELF};
    DWORD event_thread{};
    DWORD event_time{};
    bool matching_hook_slot{true};
};

// This seam does not exist in production builds. Tests compile the source in a
// separate target with PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING and never
// install live hooks.
class ExplorerGlueEventSourceTestAccess final {
public:
    [[nodiscard]] static std::unique_ptr<ExplorerGlueEventSource> create(
        ExplorerGlueEventSourceTestBinding leader,
        ExplorerGlueEventSourceTestBinding follower,
        std::size_t queue_capacity,
        bool notification_succeeds = true);
    static void enqueue(ExplorerGlueEventSource& source,
                        const ExplorerGlueSyntheticWinEvent& event) noexcept;
    [[nodiscard]] static ExplorerGlueEventDrainResult drain(
        ExplorerGlueEventSource& source);
    [[nodiscard]] static ExplorerGlueEventSourceFacts facts(
        const ExplorerGlueEventSource& source) noexcept;
    static void stop(ExplorerGlueEventSource& source,
                     bool inject_unhook_failure = false) noexcept;
    static void simulate_reentrant_drain(ExplorerGlueEventSource& source);
    [[nodiscard]] static bool notification_pending(
        const ExplorerGlueEventSource& source) noexcept {
        return source.notification_pending_;
    }
    static void set_notification_succeeds(ExplorerGlueEventSource& source,
                                         const bool succeeds) noexcept {
        source.synthetic_notification_succeeds_ = succeeds;
    }
};

} // namespace detail

#endif

} // namespace panebind::platform::windows::explorer
