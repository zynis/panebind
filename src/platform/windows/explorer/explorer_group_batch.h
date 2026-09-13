#pragma once
#include "platform/windows/explorer/explorer_glue_input.h"
#include "core/geometry/geometry.h"
#include <span>

namespace panebind::platform::windows::explorer::detail {
class ExplorerGroupBridge;
class GroupBatchTestAccess;

// Internal mechanical executor, NOT an Explorer capability. Its only runtime
// caller must first resolve the private group seal and complete all preflight.
struct GroupNativeTarget final {
    HWND window{};
    core::geometry::Rect positioning;
};
enum class GroupBatchStage { Preflight, Begin, Defer, End, Postverify, Complete };
struct GroupNativeBatchResult final {
    GroupBatchStage stage{GroupBatchStage::Preflight};
    DWORD error{};
    std::size_t deferred{};
    bool native_commit_attempted{};
    bool succeeded{};
    std::int64_t native_start_qpc{};
    std::int64_t native_return_qpc{};
};

class GroupNativeApi final {
private:
    HDWP begin(int count) const noexcept { return BeginDeferWindowPos(count); }
    HDWP defer(HDWP batch, const GroupNativeTarget& target) const noexcept {
        return DeferWindowPos(batch, target.window, nullptr,
            static_cast<int>(target.positioning.left()), static_cast<int>(target.positioning.top()),
            0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    bool end(HDWP batch) const noexcept { return EndDeferWindowPos(batch) != FALSE; }
    DWORD error() const noexcept { return GetLastError(); }
    friend class GroupBatchExecutor;
};

// All-or-zero admission is deliberately distinct from native atomicity.
// No scope guard invokes End: a failed Defer must abandon the chain.
class GroupBatchExecutor final {
private:
template <class Api>
[[nodiscard]] static GroupNativeBatchResult run(
    std::span<const GroupNativeTarget> targets, std::span<const bool> member_preflight,
    bool all_pending_registered, Api& api) noexcept {
    GroupNativeBatchResult result;
    if (member_preflight.size()!=targets.size() || !all_pending_registered || targets.empty() || targets.size()>3)
        return result;
    for(bool eligible:member_preflight)if(!eligible)return result;
    result.native_start_qpc=glue_qpc_now();
    result.stage=GroupBatchStage::Begin;
    auto handle=api.begin(static_cast<int>(targets.size()));
    if (!handle) { result.error=api.error(); result.native_return_qpc=glue_qpc_now(); return result; }
    result.stage=GroupBatchStage::Defer;
    for (const auto& target:targets) {
        handle=api.defer(handle,target);
        if (!handle) { result.error=api.error(); result.native_return_qpc=glue_qpc_now(); return result; }
        ++result.deferred;
    }
    result.stage=GroupBatchStage::End;
    result.native_commit_attempted=true;
    result.succeeded=api.end(handle);
    result.error=result.succeeded?ERROR_SUCCESS:api.error();
    result.native_return_qpc=glue_qpc_now();
    if (result.succeeded) result.stage=GroupBatchStage::Postverify;
    return result;
}
    friend class ExplorerGroupBridge;
#if defined(PANEBIND_EXPLORER_GROUP_TESTING)
    friend class GroupBatchTestAccess;
#endif
};
} // namespace panebind::platform::windows::explorer::detail
