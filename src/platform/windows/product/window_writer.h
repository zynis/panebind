#pragma once

#include "platform/windows/product/window_catalog.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace panebind::platform::windows::product {
inline constexpr UINT native_completion_message=WM_APP+46;

class WindowWriter final {
public:
    struct Receipt {
        // accepted means the requested target was actually postverified, or
        // was already exact without a native call. Admission alone is not
        // successful completion; the native facts below remain independent.
        bool accepted{}, native_attempted{}, native_succeeded{}, completion_known{true};
        bool identity_valid{}, geometry_exact{};
        DWORD error{};
        Snapshot before;
        std::optional<Snapshot> after;
        core::v1::Rect requested;
        std::uint64_t before_qpc{}, after_qpc{};
        std::uint64_t call_id{},post_observed_qpc{};
        std::string reason;
    };

    // Called at the last pre-native boundary, after all potentially blocking
    // capture/COM work. The runtime owns its generation/retirement claim; this
    // function does not wait for, or pretend to cancel, an already-issued API.
    using Guard = std::function<bool()>;

    struct Preparation {
        bool accepted{}, identity_valid{}, noop{};
        std::optional<core::v1::Rect> positioning;
        std::string reason;
    };
    // The exact bounded frame/continuity decision used by place() and batch().
    // No window query, COM, input resource or native placement occurs here.
    [[nodiscard]] static Preparation prepare(const Snapshot& expected,
        const Snapshot& actual, const core::v1::Rect& desired);

    explicit WindowWriter(Catalog& catalog);
    ~WindowWriter();
    // Retire pending admission; an issued native call is not OS-cancellable.
    // False means it was still in flight at the bounded shutdown boundary.
    bool stop();
    [[nodiscard]] bool has_unconfirmed_native_call()const;
    void notify_on_completion(HWND);
    [[nodiscard]] std::vector<Receipt> take_delayed_completions(const core::v1::Settings&);
    [[nodiscard]] Receipt place(const Snapshot& expected,
        const core::v1::Rect& desired, const core::v1::Settings& settings,
        Guard guard);

    // Prevalidate every supplied identity before any placement. There is no
    // native transaction or rollback: return the actual completed prefix plus
    // the first rejection/failure, and do not issue the remaining placements.
    [[nodiscard]] std::vector<Receipt> batch(std::span<const Snapshot> identities,
        std::span<const core::v1::Placement> placements,
        const core::v1::Settings& settings, Guard guard);

private:
    Catalog& catalog_;
    struct NativeExecutor;
    std::unique_ptr<NativeExecutor> executor_;
};

} // namespace panebind::platform::windows::product
