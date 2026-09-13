#pragma once
#include "platform/windows/explorer/explorer_group_internal.h"
#include <atomic>
#include <vector>

namespace panebind::platform::windows::explorer {
enum class GroupEventKind { Start, Location, End, Destroy };
struct GroupEventReceipt final {
    std::size_t member_index{};
    std::uint64_t window_id{};
    std::uint64_t capability_generation{};
    HWND native_source{};
    GroupEventKind kind{GroupEventKind::Location};
    std::uint64_t sequence{};
    DWORD native_thread{};
    DWORD native_time{};
    std::int64_t callback_qpc{};
    CtrlSample ctrl;
};
struct GroupEventFacts final {
    std::uint64_t accepted{}, ignored{}, overflow{}, post_failure{};
    std::size_t max_depth{};
    bool poisoned{}, running{};
};
// Role-neutral ingress. Only the private group owner can bind its authorized
// set. Receipt storage and hook slots are fixed and allocation-free at ingress.
class ExplorerGroupEventSource final {
public:
    ~ExplorerGroupEventSource();
    ExplorerGroupEventSource(const ExplorerGroupEventSource&)=delete;
    ExplorerGroupEventSource& operator=(const ExplorerGroupEventSource&)=delete;
private:
    explicit ExplorerGroupEventSource(const std::array<detail::GroupMemberBinding,3>& members);
    bool start();
    bool stop() noexcept;
    std::vector<GroupEventReceipt> drain();
    bool healthy() const noexcept { return !poisoned_.load(); }
    bool lifecycle_pending(std::optional<std::size_t> active_member = std::nullopt) const noexcept;
    GroupEventFacts facts() const noexcept;
    std::uint64_t watermark() const noexcept { return sequence_; }
    void receive(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) noexcept;
    static void CALLBACK callback(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) noexcept;
    static constexpr UINT wake_message=WM_APP+0x46;
    struct Hook {HWINEVENTHOOK value{};DWORD pid{};DWORD first{};DWORD last{};};
    std::array<detail::GroupMemberBinding,3> members_;
    std::array<Hook,9> hooks_{};
    std::array<GroupEventReceipt,4096> queue_{};
    std::size_t head_{},size_{},hook_count_{},max_depth_{};
    DWORD owner_{};
    std::uint64_t sequence_{},ignored_{},overflow_{},post_failure_{};
    std::atomic<bool> poisoned_{};
    bool running_{},notified_{},in_callback_{};
    friend class ExplorerGroupSession;
#if defined(PANEBIND_EXPLORER_GROUP_TESTING)
    friend class GroupEventSourceTestAccess;
    bool synthetic_{};
    CtrlSample synthetic_ctrl_{};
#endif
};
} // namespace panebind::platform::windows::explorer
