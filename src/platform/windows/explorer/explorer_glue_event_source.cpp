#include "platform/windows/explorer/explorer_glue_event_source.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <utility>

namespace panebind::platform::windows::explorer {
namespace {

constexpr DWORD kHookFlags =
    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
constexpr std::size_t kMaximumHookCount = 6U;

std::atomic<ExplorerGlueEventSource*> active_live_source{};
std::atomic<bool> live_hook_lifecycle_poisoned{};
std::atomic<std::uintptr_t> next_notification_cookie{1U};

[[nodiscard]] std::uintptr_t allocate_notification_cookie() noexcept {
    auto cookie = next_notification_cookie.fetch_add(1U,
                                                      std::memory_order_relaxed);
    if (cookie == 0U) {
        cookie = next_notification_cookie.fetch_add(1U,
                                                     std::memory_order_relaxed);
    }
    return cookie;
}

[[nodiscard]] bool is_supported_event(const DWORD event) noexcept {
    return event == EVENT_SYSTEM_MOVESIZESTART ||
           event == EVENT_OBJECT_LOCATIONCHANGE ||
           event == EVENT_SYSTEM_MOVESIZEEND ||
           event == EVENT_OBJECT_DESTROY;
}

[[nodiscard]] ExplorerGlueEventKind normalized_kind(
    const DWORD event) noexcept {
    if (event == EVENT_SYSTEM_MOVESIZESTART) {
        return ExplorerGlueEventKind::MoveResizeStarted;
    }
    if (event == EVENT_SYSTEM_MOVESIZEEND) {
        return ExplorerGlueEventKind::MoveResizeEnded;
    }
    if (event == EVENT_OBJECT_DESTROY) {
        return ExplorerGlueEventKind::TargetDestroyed;
    }
    return ExplorerGlueEventKind::GeometryChanged;
}

} // namespace

struct ExplorerGlueEventSource::RawReceipt final {
    HWINEVENTHOOK hook{};
    DWORD event{};
    HWND window{};
    LONG object_id{};
    LONG child_id{};
    DWORD event_thread{};
    DWORD event_time{};
    std::uint64_t sequence{};
};

struct ExplorerGlueEventSource::HookSlot final {
    enum class Kind : std::uint8_t {
        Lifecycle,
        Location,
        Destroy,
    };

    HWINEVENTHOOK hook{};
    DWORD process_id{};
    Kind kind{Kind::Lifecycle};
    bool installed{};
};

ExplorerGlueEventSource::ExplorerGlueEventSource(
    NativeTargetBinding leader,
    NativeTargetBinding follower,
    const std::size_t queue_capacity,
    const DeliveryMode mode,
    const bool synthetic_notification_succeeds)
    : leader_(leader),
      follower_(follower),
      owner_thread_id_(GetCurrentThreadId()),
      notification_cookie_(allocate_notification_cookie()),
      delivery_mode_(mode),
      synthetic_notification_succeeds_(synthetic_notification_succeeds),
      queue_(queue_capacity == 0U
                 ? nullptr
                 : std::make_unique<RawReceipt[]>(queue_capacity)),
      queue_capacity_(queue_capacity),
      hooks_(std::make_unique<HookSlot[]>(kMaximumHookCount)),
      hook_capacity_(kMaximumHookCount) {
    const bool bindings_valid =
        queue_capacity_ != 0U && leader_.window != nullptr &&
        follower_.window != nullptr && leader_.window != follower_.window &&
        leader_.process_id != 0U && follower_.process_id != 0U &&
        leader_.thread_id != 0U && follower_.thread_id != 0U &&
        leader_.window_id != 0U && follower_.window_id != 0U &&
        leader_.window_id != follower_.window_id &&
        leader_.capability_generation != 0U &&
        follower_.capability_generation != 0U &&
        leader_.role == ExplorerGlueWindowRole::Leader &&
        follower_.role == ExplorerGlueWindowRole::Follower;
    if (!bindings_valid) {
        mark_poison(ExplorerGlueEventSourcePoison::InvalidBinding);
        return;
    }

    unique_target_process_count_ =
        leader_.process_id == follower_.process_id ? 1U : 2U;
    if (delivery_mode_ == DeliveryMode::Synthetic) {
        install_synthetic_slots_for_process(leader_.process_id);
        if (follower_.process_id != leader_.process_id) {
            install_synthetic_slots_for_process(follower_.process_id);
        }
        running_ = true;
    }
}

ExplorerGlueEventSource::~ExplorerGlueEventSource() {
    if (delivery_mode_ == DeliveryMode::Live) {
        if (running_ || hook_count_ != 0U) {
            static_cast<void>(stop_live());
        }
    } else if (running_) {
        stop_synthetic(false);
    }
}

void ExplorerGlueEventSource::mark_poison(
    const ExplorerGlueEventSourcePoison poison) noexcept {
    if (poison == ExplorerGlueEventSourcePoison::None) {
        return;
    }

    auto expected = ExplorerGlueEventSourcePoison::None;
    static_cast<void>(poison_.compare_exchange_strong(
        expected,
        poison,
        std::memory_order_release,
        std::memory_order_relaxed));
}

bool ExplorerGlueEventSource::validate_binding(
    const NativeTargetBinding& binding) noexcept {
    DWORD process_id = 0U;
    const DWORD thread_id = GetWindowThreadProcessId(binding.window,
                                                     &process_id);
    if (thread_id == 0U || process_id != binding.process_id ||
        thread_id != binding.thread_id) {
        mark_poison(ExplorerGlueEventSourcePoison::TargetIdentityMismatch);
        return false;
    }
    if (GetAncestor(binding.window, GA_ROOT) != binding.window) {
        mark_poison(ExplorerGlueEventSourcePoison::TargetNotRoot);
        return false;
    }
    return true;
}

void ExplorerGlueEventSource::install_hooks_for_process(
    const DWORD process_id) {
    if (poison_.load(std::memory_order_acquire) !=
        ExplorerGlueEventSourcePoison::None) {
        return;
    }

    const auto add_hook = [this, process_id](const DWORD event_min,
                                             const DWORD event_max,
                                             const HookSlot::Kind kind) {
        if (hook_count_ >= hook_capacity_) {
            mark_poison(ExplorerGlueEventSourcePoison::HookInstallFailure);
            return;
        }

        const auto hook = SetWinEventHook(event_min,
                                          event_max,
                                          nullptr,
                                          &win_event_callback,
                                          process_id,
                                          0U,
                                          kHookFlags);
        if (hook == nullptr) {
            mark_poison(ExplorerGlueEventSourcePoison::HookInstallFailure);
            return;
        }

        hooks_[hook_count_] = {hook, process_id, kind, true};
        ++hook_count_;
        if (kind == HookSlot::Kind::Lifecycle) {
            ++lifecycle_hook_count_;
        } else if (kind == HookSlot::Kind::Location) {
            ++location_hook_count_;
        } else {
            ++destroy_hook_count_;
        }
    };

    add_hook(EVENT_SYSTEM_MOVESIZESTART,
             EVENT_SYSTEM_MOVESIZEEND,
             HookSlot::Kind::Lifecycle);
    add_hook(EVENT_OBJECT_LOCATIONCHANGE,
             EVENT_OBJECT_LOCATIONCHANGE,
             HookSlot::Kind::Location);
    add_hook(EVENT_OBJECT_DESTROY,
             EVENT_OBJECT_DESTROY,
             HookSlot::Kind::Destroy);
}

void ExplorerGlueEventSource::install_synthetic_slots_for_process(
    const DWORD process_id) {
    const auto add_slot = [this, process_id](const HookSlot::Kind kind) {
        const auto identity =
            (notification_cookie_ << 4U) + hook_count_ + 1U;
        hooks_[hook_count_] = {
            reinterpret_cast<HWINEVENTHOOK>(identity), process_id, kind};
        ++hook_count_;
        if (kind == HookSlot::Kind::Lifecycle) {
            ++lifecycle_hook_count_;
        } else if (kind == HookSlot::Kind::Location) {
            ++location_hook_count_;
        } else {
            ++destroy_hook_count_;
        }
    };
    add_slot(HookSlot::Kind::Lifecycle);
    add_slot(HookSlot::Kind::Location);
    add_slot(HookSlot::Kind::Destroy);
}

bool ExplorerGlueEventSource::start_live() {
    if (delivery_mode_ != DeliveryMode::Live || running_ || hook_count_ != 0U) {
        mark_poison(ExplorerGlueEventSourcePoison::SourceAlreadyActive);
        return false;
    }
    if (GetCurrentThreadId() != owner_thread_id_) {
        wrong_thread_count_.fetch_add(1U, std::memory_order_relaxed);
        mark_poison(ExplorerGlueEventSourcePoison::WrongOwnerThread);
        return false;
    }
    if (poison_.load(std::memory_order_acquire) !=
            ExplorerGlueEventSourcePoison::None ||
        live_hook_lifecycle_poisoned.load(std::memory_order_acquire)) {
        mark_poison(ExplorerGlueEventSourcePoison::HookInstallFailure);
        return false;
    }
    if (!validate_binding(leader_) || !validate_binding(follower_)) {
        return false;
    }

    MSG message{};
    static_cast<void>(PeekMessageW(&message, nullptr, 0U, 0U, PM_NOREMOVE));

    ExplorerGlueEventSource* expected = nullptr;
    if (!active_live_source.compare_exchange_strong(
            expected,
            this,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        mark_poison(ExplorerGlueEventSourcePoison::SourceAlreadyActive);
        return false;
    }

    running_ = true;
    install_hooks_for_process(leader_.process_id);
    if (follower_.process_id != leader_.process_id) {
        install_hooks_for_process(follower_.process_id);
    }

    if (poison_.load(std::memory_order_acquire) !=
        ExplorerGlueEventSourcePoison::None) {
        active_live_source.store(nullptr, std::memory_order_release);
        unhook_all();
        running_ = false;
        return false;
    }

    live_hooks_installed_ = true;
    return true;
}

void ExplorerGlueEventSource::unhook_all() noexcept {
    bool unhook_failed = false;
    for (std::size_t index = 0U; index < hook_count_; ++index) {
        if (hooks_[index].installed && hooks_[index].hook != nullptr &&
            !UnhookWinEvent(hooks_[index].hook)) {
            unhook_failed = true;
        }
        hooks_[index].installed = false;
    }
    live_hooks_installed_ = false;
    if (unhook_failed) {
        unhook_failure_observed_ = true;
        live_hook_lifecycle_poisoned.store(true, std::memory_order_release);
        mark_poison(ExplorerGlueEventSourcePoison::HookUnhookFailure);
    }
}

bool ExplorerGlueEventSource::stop_live() noexcept {
    if (delivery_mode_ != DeliveryMode::Live) {
        return false;
    }
    if (GetCurrentThreadId() != owner_thread_id_) {
        wrong_thread_count_.fetch_add(1U, std::memory_order_relaxed);
        mark_poison(ExplorerGlueEventSourcePoison::WrongOwnerThread);
        live_hook_lifecycle_poisoned.store(true, std::memory_order_release);
        ExplorerGlueEventSource* expected = this;
        static_cast<void>(active_live_source.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel,
            std::memory_order_acquire));
        running_ = false;
        live_hooks_installed_ = false;
        return false;
    }

    ExplorerGlueEventSource* expected = this;
    static_cast<void>(active_live_source.compare_exchange_strong(
        expected,
        nullptr,
        std::memory_order_acq_rel,
        std::memory_order_acquire));
    unhook_all();
    running_ = false;
    notification_pending_ = false;
    return !unhook_failure_observed_;
}

void ExplorerGlueEventSource::stop_synthetic(
    const bool inject_unhook_failure) noexcept {
    running_ = false;
    notification_pending_ = false;
    if (inject_unhook_failure) {
        unhook_failure_observed_ = true;
        mark_poison(ExplorerGlueEventSourcePoison::HookUnhookFailure);
    }
}

bool ExplorerGlueEventSource::post_owner_notification() noexcept {
    if (delivery_mode_ == DeliveryMode::Synthetic) {
        return synthetic_notification_succeeds_;
    }
    return PostThreadMessageW(owner_thread_id_,
                              notification_message(),
                              static_cast<WPARAM>(notification_cookie_),
                              0) != FALSE;
}

void CALLBACK ExplorerGlueEventSource::win_event_callback(
    const HWINEVENTHOOK hook,
    const DWORD event,
    const HWND window,
    const LONG object_id,
    const LONG child_id,
    const DWORD event_thread,
    const DWORD event_time) noexcept {
    auto* const source = active_live_source.load(std::memory_order_acquire);
    if (source != nullptr) {
        source->receive_raw_event(hook,
                                  event,
                                  window,
                                  object_id,
                                  child_id,
                                  event_thread,
                                  event_time);
    }
}

void ExplorerGlueEventSource::receive_raw_event(
    const HWINEVENTHOOK hook,
    const DWORD event,
    const HWND window,
    const LONG object_id,
    const LONG child_id,
    const DWORD event_thread,
    const DWORD event_time) noexcept {
    if (GetCurrentThreadId() != owner_thread_id_) {
        wrong_thread_count_.fetch_add(1U, std::memory_order_relaxed);
        mark_poison(ExplorerGlueEventSourcePoison::WrongOwnerThread);
        return;
    }
    if (callback_in_progress_ || drain_in_progress_) {
        ++reentrant_count_;
        mark_poison(ExplorerGlueEventSourcePoison::ReentrantCallback);
        return;
    }
    if (!running_ || poison_.load(std::memory_order_acquire) !=
                         ExplorerGlueEventSourcePoison::None) {
        ++post_poison_count_;
        return;
    }

    // Fixed envelope filtering is callback-safe: it performs no native query,
    // allocation, logging, or behavior work. Noise from another window in the
    // same Explorer process (or from an accessibility child) must not consume
    // the two-target bounded queue or poison the Glue session.
    if (!is_supported_event(event)) {
        ++ignored_event_count_;
        return;
    }
    if (object_id != OBJID_WINDOW || child_id != CHILDID_SELF) {
        ++ignored_object_child_count_;
        return;
    }
    if (window != leader_.window && window != follower_.window) {
        ++ignored_other_window_count_;
        return;
    }

    callback_in_progress_ = true;
    if (next_receipt_sequence_ == 0U) {
        mark_poison(ExplorerGlueEventSourcePoison::SequenceExhausted);
        callback_in_progress_ = false;
        return;
    }
    const std::uint64_t sequence = next_receipt_sequence_++;
    latest_receipt_sequence_ = sequence;
    if (queue_size_ >= queue_capacity_) {
        ++overflow_count_;
        mark_poison(ExplorerGlueEventSourcePoison::QueueOverflow);
        callback_in_progress_ = false;
        return;
    }

    const std::size_t tail = (queue_head_ + queue_size_) % queue_capacity_;
    queue_[tail] = {hook,
                    event,
                    window,
                    object_id,
                    child_id,
                    event_thread,
                    event_time,
                    sequence};
    ++queue_size_;
    max_queue_depth_ = std::max(max_queue_depth_, queue_size_);

    if (!notification_pending_) {
        notification_pending_ = true;
        if (!post_owner_notification()) {
            notification_pending_ = false;
            ++notification_failure_count_;
            mark_poison(ExplorerGlueEventSourcePoison::NotificationFailure);
        }
    }
    callback_in_progress_ = false;
}

const ExplorerGlueEventSource::NativeTargetBinding*
ExplorerGlueEventSource::find_binding(const HWND window) const noexcept {
    if (window == leader_.window) {
        return &leader_;
    }
    if (window == follower_.window) {
        return &follower_;
    }
    return nullptr;
}

const ExplorerGlueEventSource::HookSlot*
ExplorerGlueEventSource::find_hook_slot(
    const HWINEVENTHOOK hook) const noexcept {
    for (std::size_t index = 0U; index < hook_count_; ++index) {
        if (hooks_[index].hook == hook) {
            return &hooks_[index];
        }
    }
    return nullptr;
}

ExplorerGlueEventDrainResult ExplorerGlueEventSource::drain_owner_queue() {
    ExplorerGlueEventDrainResult result;
    if (GetCurrentThreadId() != owner_thread_id_) {
        wrong_thread_count_.fetch_add(1U, std::memory_order_relaxed);
        mark_poison(ExplorerGlueEventSourcePoison::WrongOwnerThread);
        result.facts = facts();
        return result;
    }
    if (drain_in_progress_ || callback_in_progress_) {
        ++reentrant_count_;
        mark_poison(ExplorerGlueEventSourcePoison::ReentrantDrain);
        result.facts = facts();
        return result;
    }

    drain_in_progress_ = true;
    notification_pending_ = false;
    result.events.reserve(queue_size_);

    while (queue_size_ != 0U) {
        const RawReceipt receipt = queue_[queue_head_];
        queue_head_ = (queue_head_ + 1U) % queue_capacity_;
        --queue_size_;

        if (!is_supported_event(receipt.event)) {
            ++ignored_event_count_;
            continue;
        }
        if (receipt.object_id != OBJID_WINDOW ||
            receipt.child_id != CHILDID_SELF) {
            ++ignored_object_child_count_;
            continue;
        }

        const auto* const hook_slot = find_hook_slot(receipt.hook);
        const HookSlot::Kind expected_hook_kind =
            receipt.event == EVENT_OBJECT_LOCATIONCHANGE
                ? HookSlot::Kind::Location
                : (receipt.event == EVENT_OBJECT_DESTROY
                       ? HookSlot::Kind::Destroy
                       : HookSlot::Kind::Lifecycle);
        if (hook_slot == nullptr || hook_slot->kind != expected_hook_kind) {
            mark_poison(
                ExplorerGlueEventSourcePoison::HookReceiptMismatch);
            continue;
        }

        const auto* const binding = find_binding(receipt.window);
        if (binding == nullptr) {
            ++ignored_other_window_count_;
            continue;
        }
        if (hook_slot->process_id != binding->process_id) {
            mark_poison(
                ExplorerGlueEventSourcePoison::HookReceiptMismatch);
            continue;
        }

        if (receipt.event == EVENT_OBJECT_DESTROY) {
            result.events.push_back(
                {ExplorerGlueEventKind::TargetDestroyed,
                 binding->role,
                 binding->window_id,
                 binding->capability_generation,
                 receipt.sequence,
                 receipt.event_thread,
                 receipt.event_time});
            ++accepted_count_;
            continue;
        }

        DWORD process_id = 0U;
        const DWORD thread_id =
            GetWindowThreadProcessId(receipt.window, &process_id);
        if (thread_id == 0U || process_id != binding->process_id ||
            thread_id != binding->thread_id) {
            mark_poison(
                ExplorerGlueEventSourcePoison::TargetIdentityMismatch);
            continue;
        }
        if (GetAncestor(receipt.window, GA_ROOT) != receipt.window) {
            mark_poison(ExplorerGlueEventSourcePoison::TargetNotRoot);
            continue;
        }

        result.events.push_back(
            {normalized_kind(receipt.event),
             binding->role,
             binding->window_id,
             binding->capability_generation,
             receipt.sequence,
             receipt.event_thread,
             receipt.event_time});
        ++accepted_count_;
    }

    drain_in_progress_ = false;
    result.facts = facts();
    return result;
}

ExplorerGlueEventSourceFacts ExplorerGlueEventSource::facts() const noexcept {
    return {
        poison_.load(std::memory_order_acquire),
        owner_thread_id_,
        kHookFlags,
        queue_capacity_,
        queue_size_,
        max_queue_depth_,
        unique_target_process_count_,
        lifecycle_hook_count_,
        location_hook_count_,
        destroy_hook_count_,
        latest_receipt_sequence_,
        accepted_count_,
        ignored_other_window_count_,
        ignored_object_child_count_,
        ignored_event_count_,
        overflow_count_,
        notification_failure_count_,
        wrong_thread_count_.load(std::memory_order_relaxed),
        reentrant_count_,
        post_poison_count_,
        running_,
        live_hooks_installed_,
    };
}

bool ExplorerGlueEventSource::owns_notification(
    const UINT message,
    const WPARAM cookie) const noexcept {
    return message == notification_message() &&
           cookie == static_cast<WPARAM>(notification_cookie_);
}

#if defined(PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING)

std::unique_ptr<ExplorerGlueEventSource>
detail::ExplorerGlueEventSourceTestAccess::create(
    const ExplorerGlueEventSourceTestBinding leader,
    const ExplorerGlueEventSourceTestBinding follower,
    const std::size_t queue_capacity,
    const bool notification_succeeds) {
    const ExplorerGlueEventSource::NativeTargetBinding native_leader{
        leader.window,
        leader.process_id,
        leader.thread_id,
        leader.window_id,
        leader.capability_generation,
        leader.role,
    };
    const ExplorerGlueEventSource::NativeTargetBinding native_follower{
        follower.window,
        follower.process_id,
        follower.thread_id,
        follower.window_id,
        follower.capability_generation,
        follower.role,
    };
    return std::unique_ptr<ExplorerGlueEventSource>(
        new ExplorerGlueEventSource(native_leader,
                                    native_follower,
                                    queue_capacity,
                                    ExplorerGlueEventSource::DeliveryMode::Synthetic,
                                    notification_succeeds));
}

void detail::ExplorerGlueEventSourceTestAccess::enqueue(
    ExplorerGlueEventSource& source,
    const ExplorerGlueSyntheticWinEvent& event) noexcept {
    const auto* const binding = source.find_binding(event.window);
    DWORD process_id = binding == nullptr ? 0U : binding->process_id;
    if (process_id == 0U) {
        static_cast<void>(GetWindowThreadProcessId(event.window, &process_id));
    }

    const ExplorerGlueEventSource::HookSlot::Kind kind =
        event.event == EVENT_OBJECT_LOCATIONCHANGE
            ? ExplorerGlueEventSource::HookSlot::Kind::Location
            : (event.event == EVENT_OBJECT_DESTROY
                   ? ExplorerGlueEventSource::HookSlot::Kind::Destroy
                   : ExplorerGlueEventSource::HookSlot::Kind::Lifecycle);
    HWINEVENTHOOK hook = nullptr;
    for (std::size_t index = 0U; index < source.hook_count_; ++index) {
        if (source.hooks_[index].process_id == process_id &&
            source.hooks_[index].kind == kind) {
            hook = source.hooks_[index].hook;
            break;
        }
    }
    if (!event.matching_hook_slot) {
        hook = reinterpret_cast<HWINEVENTHOOK>(
            std::numeric_limits<std::uintptr_t>::max());
    }

    source.receive_raw_event(hook,
                             event.event,
                             event.window,
                             event.object_id,
                             event.child_id,
                             event.event_thread,
                             event.event_time);
}

ExplorerGlueEventDrainResult
detail::ExplorerGlueEventSourceTestAccess::drain(
    ExplorerGlueEventSource& source) {
    return source.drain_owner_queue();
}

ExplorerGlueEventSourceFacts
detail::ExplorerGlueEventSourceTestAccess::facts(
    const ExplorerGlueEventSource& source) noexcept {
    return source.facts();
}

void detail::ExplorerGlueEventSourceTestAccess::stop(
    ExplorerGlueEventSource& source,
    const bool inject_unhook_failure) noexcept {
    source.stop_synthetic(inject_unhook_failure);
}

void detail::ExplorerGlueEventSourceTestAccess::simulate_reentrant_drain(
    ExplorerGlueEventSource& source) {
    source.drain_in_progress_ = true;
    static_cast<void>(source.drain_owner_queue());
    source.drain_in_progress_ = false;
}

#endif

} // namespace panebind::platform::windows::explorer
