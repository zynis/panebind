#pragma once
#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_group_capture.h"
#include "platform/windows/explorer/explorer_group_batch.h"
#include "platform/windows/explorer/explorer_virtual_desktop_manager.h"
#include <array>
#include <algorithm>

namespace panebind::platform::windows::explorer {
class ExplorerGroupSession;
namespace detail {
class ExplorerGroupBridge;
struct GroupMemberBinding final {
    std::uint64_t window_id{}; // session-qualified logical id, never numeric HWND
    std::uint64_t capability_generation{};
    std::uint64_t consent_generation{};
    HWND window{};
    DWORD process_id{};
    DWORD thread_id{};
};
class ExplorerGroupSeal final {
public:
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] const auto& members() const noexcept { return members_; }
    [[nodiscard]] const auto& logical_order() const noexcept { return logical_order_; }
private:
    ExplorerGroupSeal(std::uint64_t generation, std::array<GroupMemberBinding,3> members)
        : generation_(generation), members_(members) {
        std::sort(logical_order_.begin(),logical_order_.end(),[&](auto a,auto b) {
            return std::to_string(members_[a].window_id)<std::to_string(members_[b].window_id);
        });
    }
    std::uint64_t generation_{};
    std::array<GroupMemberBinding,3> members_{};
    std::array<std::size_t,3> logical_order_{0,1,2};
    friend class ExplorerGroupBridge;
};
using GroupSessions=std::array<ExplorerTestSession*,3>;
struct GroupBatchReceipt final {
    GroupNativeBatchResult native;
    GroupSnapshots before;
    std::array<std::optional<ExplorerWindowSnapshot>,3> actual;
    std::array<std::optional<core::geometry::Rect>,3> targets;
    std::array<std::optional<core::geometry::Rect>,3> positioning_targets;
    std::array<bool,3> exact{};
    bool all_preflight{};
    bool all_pending_registered{};
    bool all_postverify{};
    std::int64_t postverify_qpc{};
    std::string_view reason{"preflight_failed"};
};
// Private capability bridge. No public HWND-admission or placement entry point.
class ExplorerGroupBridge final {
private:
    static std::optional<ExplorerGroupSeal> bind(GroupSessions sessions,
        ExplorerVirtualDesktopManager& manager, GroupSnapshots& original);
    static GroupCaptureResult capture(const ExplorerGroupSeal&, GroupSessions,
        GroupCaptureMode mode=GroupCaptureMode::Strict);
    static bool receipts_healthy(const ExplorerGroupSeal&, GroupSessions) noexcept;
    static void active(const ExplorerGroupSeal&,GroupSessions,bool) noexcept;
    static void retire(const ExplorerGroupSeal&,GroupSessions) noexcept;
    static GroupBatchReceipt apply(const ExplorerGroupSeal&,GroupSessions,
        const GroupSnapshots& expected,
        const std::array<std::optional<core::geometry::Rect>,3>& targets,
        std::uint64_t operation_generation, bool (*register_pending)(void*) noexcept, void* context);
    friend class ::panebind::platform::windows::explorer::ExplorerGroupSession;
};
} // namespace detail
} // namespace panebind::platform::windows::explorer
