#pragma once

#include "platform/windows/product/window_catalog.h"
#include "core/v1/visual_ownership.h"

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
    enum class Operation { Placement,Visual,Lower };
    struct VisualNativeFacts {
        bool style_attempted{},alpha_attempted{},alpha_succeeded{},zorder_attempted{},zorder_succeeded{};
        LONG_PTR style_previous{};
        DWORD style_error{},alpha_error{},zorder_error{};
    };
    // Frozen before an explicitly cross-display placement, not selected from
    // its result. Window DPI follows the target window's actual awareness;
    // the PMv2 display probe is not a substitute for GetDpiForWindow.
    struct DestinationDisplay {
        Display source,target;
        core::v1::Rect positioning;
        DPI_AWARENESS awareness{DPI_AWARENESS_INVALID};
        UINT window_dpi{};
        std::vector<Display> topology;
    };
    struct Receipt {
        // accepted means the requested target was actually postverified, or
        // was already exact without a native call. Admission alone is not
        // successful completion; the native facts below remain independent.
        bool accepted{}, native_attempted{}, native_succeeded{}, completion_known{true};
        bool identity_valid{}, geometry_exact{};
        // An issued native result and its exact geometry remain facts even
        // while normal-UP capture cleanup is not yet confirmed.
        bool post_context_checked{},post_context_valid{};
        std::string post_context_reason;
        DWORD error{};
        Snapshot before;
        std::optional<Snapshot> after;
        core::v1::Rect requested;
        std::uint64_t before_qpc{}, after_qpc{};
        std::uint64_t call_id{},post_observed_qpc{};
        std::string reason;
        Operation operation{Operation::Placement};
        core::v1::VisualState visual_before,visual_target;
        std::optional<core::v1::VisualState> visual_after;
        bool alpha_change{},top_change{},visual_exact{},z_bottom_observed{};
        HWND z_previous{},z_next{};
        VisualNativeFacts visual_native;
        std::optional<DestinationDisplay> destination;
    };

    // Called at the last pre-native boundary, after all potentially blocking
    // capture/COM work. The runtime owns its generation/retirement claim; this
    // function does not wait for, or pretend to cancel, an already-issued API.
    using Guard = std::function<bool()>;
    // Synchronous owner-thread handshake, before queue/wake and any native
    // visual mutation. The borrowed manual-reset event belongs to this exact
    // Work and may only be duplicated for synchronization. Never stored in Work.
    using BeforeIssue = std::function<bool(HANDLE borrowed_completion_event)>;

    struct Preparation {
        bool accepted{}, identity_valid{}, noop{};
        std::optional<core::v1::Rect> positioning;
        std::string reason;
    };
    // The exact bounded frame/continuity decision used by place() and batch().
    // No window query, COM, input resource or native placement occurs here.
    [[nodiscard]] static Preparation prepare(const Snapshot& expected,
        const Snapshot& actual, const core::v1::Rect& desired);
    struct DestinationPreparation {bool accepted{};std::optional<DestinationDisplay> proof;std::string reason;};
    // Exact predicates also used by the native adapter. No window operation or
    // monitor query occurs here. Same-display calls need no probe/proof.
    [[nodiscard]] static DestinationPreparation prepare_destination(const Snapshot&,
        const core::v1::Rect& positioning,HMONITOR planned,DPI_AWARENESS,std::span<const Display>);
    [[nodiscard]] static bool placement_context_matches(const Snapshot& before,const Snapshot& after,
        const std::optional<DestinationDisplay>&,std::span<const Display> current,DPI_AWARENESS,std::string& reason);
    struct VisualPreparation {bool accepted{},noop{};std::string reason;};
    // Shared admission used on GUI and native-worker sides. Cleanup may only
    // restore the exact original of a still-owned attribute, not set a new one.
    [[nodiscard]] static VisualPreparation prepare_visual(const core::v1::VisualState& before,
        const core::v1::VisualState& actual,const core::v1::VisualState& target,bool alpha_change,bool top_change,
        const std::optional<core::v1::VisualOwnership>& cleanup_owned=std::nullopt);
    // Actual stop-time decision. A claimed/issued call must keep its real
    // completion unknown; only never-issued admission can finish immediately.
    [[nodiscard]] static bool complete_unissued_on_stop(bool native_attempted,bool completion_known)noexcept;
    // The exact apply() classification: an unissued matching-UP withdrawal
    // waits for real terminal receipts, never manufactures NormalUp or makes
    // a failed identity/context/native result successful. Retain the lease
    // captured before batch(), not whichever pair a later DOWN publishes.
    [[nodiscard]] static bool preserves_terminal_receipt(const Receipt&,
        std::uint64_t generation,const GestureCaptureLease&,bool generation_allowed)noexcept;

    explicit WindowWriter(Catalog& catalog);
    ~WindowWriter();
    // Retire pending admission; an issued native call is not OS-cancellable.
    // False means it was still in flight at the bounded shutdown boundary.
    bool stop();
    [[nodiscard]] bool has_unconfirmed_native_call()const;
    // Includes admission which has not issued an API yet. Do not confuse this
    // queue/ordering barrier with proof of an already-issued native mutation.
    [[nodiscard]] bool has_pending_native_operation()const;
    // Caller owns the SYNCHRONIZE-only duplicate (CloseHandle). nullptr means
    // no unfinished pending Work, or duplication failure (GetLastError != 0).
    // This fixed-call completion receipt grants no admission/write authority.
    [[nodiscard]] HANDLE duplicate_pending_completion_event()const;
    void notify_on_completion(HWND);
    [[nodiscard]] std::vector<Receipt> take_delayed_completions(const core::v1::Settings&);
    [[nodiscard]] Receipt place(const Snapshot& expected,
        const core::v1::Rect& desired, const core::v1::Settings& settings,
        Guard guard,GestureCaptureLease capture={});
    [[nodiscard]] Receipt visual(const Snapshot&,const core::v1::VisualState& before,
        const core::v1::VisualState& target,bool alpha_change,bool top_change,
        const core::v1::Settings&,Guard,std::optional<core::v1::VisualOwnership> cleanup_owned=std::nullopt,
        BeforeIssue before_issue={},GestureCaptureLease capture={});
    [[nodiscard]] Receipt lower(const Snapshot&,const core::v1::Settings&,Guard);

    // Prevalidate every supplied identity before any placement. There is no
    // native transaction or rollback: return the actual completed prefix plus
    // the first rejection/failure, and do not issue the remaining placements.
    [[nodiscard]] std::vector<Receipt> batch(std::span<const Snapshot> identities,
        std::span<const core::v1::Placement> placements,
        const core::v1::Settings& settings, Guard guard,GestureCaptureLease capture={});

private:
    [[nodiscard]] Receipt place_with_destination(const Snapshot&,const core::v1::Rect&,
        const core::v1::Settings&,Guard,GestureCaptureLease,const std::optional<DestinationDisplay>* frozen);
    void observe_auxiliary(Receipt&,const core::v1::Settings&,const GestureCaptureLease& capture={});
    Catalog& catalog_;
    struct NativeExecutor;
    std::unique_ptr<NativeExecutor> executor_;
};

} // namespace panebind::platform::windows::product
