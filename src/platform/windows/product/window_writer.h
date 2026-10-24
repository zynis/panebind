#pragma once

#include "platform/windows/product/window_catalog.h"
#include "core/v1/visual_ownership.h"

#include <array>
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
    // Explicitly supplied only by the live Resize dispatcher. A callback is
    // one application proposal, never a global grid or a new permission token.
    struct ResizeRequest {core::v1::Edges edges;bool coupled{};};
    struct ResizeQueryFacts {
        bool attempted{},context_revalidated{};
        UINT edge{};LRESULT api_return{};DWORD error{};DWORD_PTR message_result{};
        RECT requested_native{},returned_native{};
        std::uint64_t before_qpc{},after_qpc{};
    };
    struct ResizePreparation {
        bool accepted{},adjusted{};
        std::optional<core::v1::Rect> visible;
        std::string reason;
    };
    struct VisualNativeFacts {
        bool style_attempted{},alpha_attempted{},alpha_succeeded{},zorder_attempted{},zorder_succeeded{};
        LONG_PTR style_previous{};
        DWORD style_error{},alpha_error{},zorder_error{};
    };
    // Two bounded, read-only observations. A desktop-tail exception is only
    // the exact current GetShellWindow immediately after this source, then
    // raw NULL; no hidden/system/class-name filtering or skipped node.
    struct LowerZOrderSample {
        bool known{},shell_known{};
        HWND source{},previous{},next{},shell{},shell_previous{},shell_next{};
        DWORD source_pid{},source_tid{},shell_pid{},shell_tid{};
        bool operator==(const LowerZOrderSample&)const=default;
    };
    enum class LowerZOrderKind { Unconfirmed,RawBottom,ShellDesktopTail };
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
        // Request policy is not evidence that the message was delivered. A
        // successful size-preserving issued Move can retain its real pre-call
        // bounds without synchronously querying them again after the call.
        std::optional<bool> post_track_limits_query_requested;
        bool post_track_limits_carried_from_preflight{};
        DWORD error{};
        Snapshot before;
        std::optional<Snapshot> after;
        core::v1::Rect requested;
        std::uint64_t before_qpc{}, after_qpc{};
        std::uint64_t call_id{},preflight_started_qpc{},geometry_observed_qpc{},post_observed_qpc{};
        UINT native_position_flags{};
        std::string reason;
        Operation operation{Operation::Placement};
        core::v1::VisualState visual_before,visual_target;
        std::optional<core::v1::VisualState> visual_after;
        bool alpha_change{},top_change{},visual_exact{},z_bottom_observed{};
        HWND z_previous{},z_next{};
        // z_bottom_observed retains the literal raw-next-is-NULL fact.
        // z_lower_observed additionally accepts the exact stable shell tail.
        bool z_lower_observed{};
        LowerZOrderKind z_lower_kind{LowerZOrderKind::Unconfirmed};
        std::array<LowerZOrderSample,2> z_lower_samples{};
        std::uint64_t z_lower_begin_qpc{},z_lower_end_qpc{};
        VisualNativeFacts visual_native;
        std::optional<DestinationDisplay> destination;
        // These API facts are separate from SetWindowPos/native_attempted.
        // requested is the final target; this retains the pre-callback R.
        std::optional<ResizeQueryFacts> resize_query;
        std::optional<core::v1::Rect> resize_original_requested;
    };

    // Used by actual immediate, delayed and auxiliary post-observation paths.
    // The first clock retains the old geometry-capture boundary; only a fully
    // returned context observer publishes the NF02 completion clock. Neither
    // timing fact grants admission or turns a rejected completion into success.
    template<class Observer>
    static void observe_post_context(Receipt& receipt,Observer&& observer,std::uint64_t (*clock)()) {
        receipt.geometry_observed_qpc=clock?clock():0;
        receipt.post_observed_qpc=0;
        observer();
        receipt.post_observed_qpc=clock?clock():0;
    }

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
    [[nodiscard]] static UINT resize_message_edge(core::v1::Edges)noexcept;
    [[nodiscard]] static bool resize_request_scope_allowed(const ResizeRequest&,std::size_t placement_count)noexcept;
    // Actual batch policy uses this exact pure decision after the real API.
    // Raw RECT ordering is checked before constructing a normalizing Rect.
    [[nodiscard]] static ResizePreparation prepare_resize_query(const Snapshot& expected,
        const Snapshot& actual,const core::v1::Rect& requested,const ResizeRequest&,const ResizeQueryFacts&);
    // Pure flag selection used at the actual Placement callsite. The worker
    // still independently checks fresh identity/frame/capture and retirement.
    // Equal native dimensions retain size; genuine Resize stays unchanged.
    [[nodiscard]] static UINT placement_flags(const Snapshot& fresh,
        const core::v1::Rect& positioning)noexcept;
    [[nodiscard]] static bool placement_post_needs_track_limits(const Receipt&,
        const core::v1::Rect& native)noexcept;
    static void retain_placement_preflight_limits(Receipt&);
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
    [[nodiscard]] static LowerZOrderKind lower_z_order_completion(const Snapshot&,
        const LowerZOrderSample&,const LowerZOrderSample&)noexcept;
    [[nodiscard]] static const wchar_t* lower_failure_notice(const Receipt&)noexcept;
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
        const core::v1::Settings& settings, Guard guard,GestureCaptureLease capture={},
        std::optional<ResizeRequest> resize=std::nullopt);

private:
    [[nodiscard]] Receipt place_with_destination(const Snapshot&,const core::v1::Rect&,
        const core::v1::Settings&,Guard,GestureCaptureLease,const std::optional<DestinationDisplay>* frozen);
    void observe_auxiliary(Receipt&,const core::v1::Settings&,const GestureCaptureLease& capture={});
    Catalog& catalog_;
    struct NativeExecutor;
    std::unique_ptr<NativeExecutor> executor_;
};

} // namespace panebind::platform::windows::product
