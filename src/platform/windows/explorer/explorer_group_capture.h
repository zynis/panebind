#pragma once
#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_consent_validation.h"
#include <array>

namespace panebind::platform::windows::explorer::detail {
using GroupSnapshots=std::array<ExplorerWindowSnapshot,3>;
enum class GroupCaptureMode { Strict, PreAcceptMutableGeometry };
enum class GroupCaptureStage { None, Binding, NativeValidation, ReceiptHealth, Context };
enum class GroupCaptureDisposition { Succeeded, Recoverable, Fatal };
struct GroupCaptureObservation {
    bool browser_observed{};
    BrowserReadinessFacts browser;
    std::optional<std::uint64_t> navigation_epoch;
    std::optional<bool> canonical_identity_matches,anchor_hwnd_matches,location_exact;
    std::string_view browser_stream_reason{"not_observed"};
};
struct GroupCaptureDiagnostic {
    ExplorerDiagnosticDomain domain{ExplorerDiagnosticDomain::Adapter};
    std::uint64_t code{};
};
struct GroupMemberCaptureResult {
    ExplorerEligibilityReason reason{ExplorerEligibilityReason::Eligible};
    std::optional<ExplorerWindowSnapshot> snapshot;
    std::optional<GroupCaptureDiagnostic> diagnostic;
    std::string_view invalidation{"none"};
    GroupCaptureObservation observation;
    bool authority_proven{}; // diagnostic proof from COMPLETE native validation
};
struct GroupCaptureResult {
    GroupCaptureDisposition disposition{GroupCaptureDisposition::Fatal};
    std::optional<GroupSnapshots> snapshots;
    std::optional<std::size_t> failed_member_index;
    GroupCaptureStage failure_stage{GroupCaptureStage::Binding};
    std::optional<ExplorerEligibilityReason> eligibility_reason;
    std::optional<GroupCaptureDiagnostic> diagnostic;
    std::string_view glue_validation_invalidation{"none"};
    std::string_view reason{"capture_not_run"};
    std::array<GroupCaptureObservation,3> observations;
    bool succeeded() const noexcept {return disposition==GroupCaptureDisposition::Succeeded&&snapshots.has_value();}
    bool recoverable() const noexcept {return disposition==GroupCaptureDisposition::Recoverable;}
    explicit operator bool() const noexcept {return succeeded();}
    const GroupSnapshots& operator*() const {return snapshots.value();}
    GroupSnapshots& operator*() {return snapshots.value();}
    const GroupSnapshots* operator->() const {return &snapshots.value();}
};
inline std::string_view group_capture_stage_name(GroupCaptureStage s) noexcept {
    switch(s){case GroupCaptureStage::None:return "None";case GroupCaptureStage::Binding:return "Binding";
    case GroupCaptureStage::NativeValidation:return "NativeValidation";case GroupCaptureStage::ReceiptHealth:return "ReceiptHealth";
    case GroupCaptureStage::Context:return "Context";}return "Unknown";
}
inline std::string_view group_capture_eligibility_name(ExplorerEligibilityReason r) noexcept {
    switch(r) {
    case ExplorerEligibilityReason::Eligible:return "Eligible";
    case ExplorerEligibilityReason::InvalidTargetDirectory:return "InvalidTargetDirectory";
    case ExplorerEligibilityReason::TargetDirectoryNotEmpty:return "TargetDirectoryNotEmpty";
    case ExplorerEligibilityReason::ComApartmentUnavailable:return "ComApartmentUnavailable";
    case ExplorerEligibilityReason::InventoryUnavailable:return "InventoryUnavailable";
    case ExplorerEligibilityReason::InventoryUnstable:return "InventoryUnstable";
    case ExplorerEligibilityReason::BaselineWindowIdentityUnavailable:return "BaselineWindowIdentityUnavailable";
    case ExplorerEligibilityReason::ShellEventSubscriptionUnavailable:return "ShellEventSubscriptionUnavailable";
    case ExplorerEligibilityReason::BrowserEventSubscriptionUnavailable:return "BrowserEventSubscriptionUnavailable";
    case ExplorerEligibilityReason::ShellEventStreamInvalid:return "ShellEventStreamInvalid";
    case ExplorerEligibilityReason::RegistrationNotObserved:return "RegistrationNotObserved";
    case ExplorerEligibilityReason::RegistrationResolutionFailed:return "RegistrationResolutionFailed";
    case ExplorerEligibilityReason::RegistrationRevoked:return "RegistrationRevoked";
    case ExplorerEligibilityReason::CanonicalIdentityMismatch:return "CanonicalIdentityMismatch";
    case ExplorerEligibilityReason::SubscriptionGenerationMismatch:return "SubscriptionGenerationMismatch";
    case ExplorerEligibilityReason::ShellWindowCreationFailed:return "ShellWindowCreationFailed";
    case ExplorerEligibilityReason::ShellWindowHandleMissing:return "ShellWindowHandleMissing";
    case ExplorerEligibilityReason::PreexistingWindow:return "PreexistingWindow";
    case ExplorerEligibilityReason::ReusedExistingWindow:return "ReusedExistingWindow";
    case ExplorerEligibilityReason::AmbiguousCandidate:return "AmbiguousCandidate";
    case ExplorerEligibilityReason::BaselineChanged:return "BaselineChanged";
    case ExplorerEligibilityReason::LocationNotReady:return "LocationNotReady";
    case ExplorerEligibilityReason::LocationMismatch:return "LocationMismatch";
    case ExplorerEligibilityReason::WindowDestroyed:return "WindowDestroyed";
    case ExplorerEligibilityReason::ProcessOpenFailed:return "ProcessOpenFailed";
    case ExplorerEligibilityReason::ProcessExited:return "ProcessExited";
    case ExplorerEligibilityReason::WrongProcess:return "WrongProcess";
    case ExplorerEligibilityReason::WrongThread:return "WrongThread";
    case ExplorerEligibilityReason::WrongImage:return "WrongImage";
    case ExplorerEligibilityReason::WrongClass:return "WrongClass";
    case ExplorerEligibilityReason::NotTopLevel:return "NotTopLevel";
    case ExplorerEligibilityReason::ChildWindow:return "ChildWindow";
    case ExplorerEligibilityReason::OwnedWindow:return "OwnedWindow";
    case ExplorerEligibilityReason::Invisible:return "Invisible";
    case ExplorerEligibilityReason::Cloaked:return "Cloaked";
    case ExplorerEligibilityReason::Minimized:return "Minimized";
    case ExplorerEligibilityReason::Maximized:return "Maximized";
    case ExplorerEligibilityReason::WrongVirtualDesktop:return "WrongVirtualDesktop";
    case ExplorerEligibilityReason::SecurityQueryFailed:return "SecurityQueryFailed";
    case ExplorerEligibilityReason::UserMismatch:return "UserMismatch";
    case ExplorerEligibilityReason::SessionMismatch:return "SessionMismatch";
    case ExplorerEligibilityReason::IntegrityMismatch:return "IntegrityMismatch";
    case ExplorerEligibilityReason::Elevated:return "Elevated";
    case ExplorerEligibilityReason::UiAccess:return "UiAccess";
    case ExplorerEligibilityReason::AppContainer:return "AppContainer";
    case ExplorerEligibilityReason::GeometryCaptureFailed:return "GeometryCaptureFailed";
    case ExplorerEligibilityReason::DpiContextMismatch:return "DpiContextMismatch";
    case ExplorerEligibilityReason::MonitorUnavailable:return "MonitorUnavailable";
    case ExplorerEligibilityReason::MonitorChanged:return "MonitorChanged";
    case ExplorerEligibilityReason::DpiChanged:return "DpiChanged";
    case ExplorerEligibilityReason::UnsafeDelta:return "UnsafeDelta";
    case ExplorerEligibilityReason::StaleToken:return "StaleToken";
    case ExplorerEligibilityReason::OperationLimitReached:return "OperationLimitReached";
    case ExplorerEligibilityReason::OperationSequenceViolation:return "OperationSequenceViolation";
    case ExplorerEligibilityReason::AuthorityExhausted:return "AuthorityExhausted";
    case ExplorerEligibilityReason::GenerationExhausted:return "GenerationExhausted";
    case ExplorerEligibilityReason::EmptyGeometry:return "EmptyGeometry";
    case ExplorerEligibilityReason::ResizeRejected:return "ResizeRejected";
    case ExplorerEligibilityReason::ArithmeticOverflow:return "ArithmeticOverflow";
    case ExplorerEligibilityReason::NativeCoordinateOutOfRange:return "NativeCoordinateOutOfRange";
    case ExplorerEligibilityReason::NativeApplyFailed:return "NativeApplyFailed";
    case ExplorerEligibilityReason::PostVerificationFailed:return "PostVerificationFailed";
    case ExplorerEligibilityReason::TargetInvalidated:return "TargetInvalidated";
    case ExplorerEligibilityReason::SafeCleanupNotPerformed:return "SafeCleanupNotPerformed";
    case ExplorerEligibilityReason::TargetConsentRequired:return "TargetConsentRequired";
    case ExplorerEligibilityReason::MoveConsentRequired:return "MoveConsentRequired";
    case ExplorerEligibilityReason::ConsentGenerationMismatch:return "ConsentGenerationMismatch";
    case ExplorerEligibilityReason::ConsentDeclined:return "ConsentDeclined";
    case ExplorerEligibilityReason::TargetNotFound:return "TargetNotFound";
    }return "UnknownEligibility";
}
inline bool recoverable_preaccept(const GroupMemberCaptureResult& member) noexcept {
    return member.authority_proven&&member.invalidation=="none"&&
        (member.reason==ExplorerEligibilityReason::MonitorChanged||member.reason==ExplorerEligibilityReason::DpiChanged);
}
inline void group_capture_failure(GroupCaptureResult& result,std::size_t member,
    GroupCaptureStage stage,const GroupMemberCaptureResult& failure,std::string_view reason,
    GroupCaptureDisposition disposition=GroupCaptureDisposition::Fatal) {
    result.disposition=disposition;result.snapshots.reset();result.failed_member_index=member;
    result.failure_stage=stage;result.eligibility_reason=failure.reason;
    result.diagnostic=failure.diagnostic;result.glue_validation_invalidation=failure.invalidation;
    result.reason=reason;
}
// Fixed three-member plumbing shared with injected-boundary tests. This helper
// cannot mint authority or perform native writes. Only the private bridge binds
// these callbacks to its ledger and native validator.
template<class Binding,class Validate,class Health,class Retire>
GroupCaptureResult capture_group_members(GroupCaptureMode mode,Binding&& binding,
    Validate&& validate,Health&& health,Retire&& retire) {
    GroupCaptureResult result;
    GroupSnapshots snapshots;
    bool temporary{};
    for(std::size_t i=0;i<3;++i) {
        const auto binding_reason=binding(i);
        if(binding_reason!="none") {
            GroupMemberCaptureResult bad;bad.reason=ExplorerEligibilityReason::TargetInvalidated;bad.invalidation=binding_reason;
            group_capture_failure(result,i,GroupCaptureStage::Binding,bad,binding_reason);retire(i);return result;
        }
        auto live=validate(i);result.observations[i]=live.observation;
        if(live.reason!=ExplorerEligibilityReason::Eligible||!live.snapshot) {
            if(live.reason==ExplorerEligibilityReason::Eligible)live.reason=ExplorerEligibilityReason::GeometryCaptureFailed;
            const bool recoverable=mode==GroupCaptureMode::PreAcceptMutableGeometry&&recoverable_preaccept(live);
            if(!temporary||!recoverable)
                group_capture_failure(result,i,GroupCaptureStage::NativeValidation,live,
                    group_capture_eligibility_name(live.reason),recoverable?GroupCaptureDisposition::Recoverable:GroupCaptureDisposition::Fatal);
            if(!recoverable){retire(i);return result;}
            temporary=true; // continue: another member may have a fatal failure
        } else snapshots[i]=std::move(*live.snapshot);
    }
    for(std::size_t i=0;i<3;++i) {
        const auto binding_reason=binding(i);
        if(binding_reason!="none") {
            GroupMemberCaptureResult bad;bad.reason=ExplorerEligibilityReason::TargetInvalidated;bad.invalidation=binding_reason;
            group_capture_failure(result,i,GroupCaptureStage::Binding,bad,binding_reason);retire(i);return result;
        }
        const auto receipt=health(i);
        if(receipt.observation.browser_observed) {
            result.observations[i].browser_observed=true;result.observations[i].browser=receipt.observation.browser;
            result.observations[i].browser_stream_reason=receipt.observation.browser_stream_reason;
            result.observations[i].navigation_epoch=receipt.observation.navigation_epoch;
        }
        if(receipt.reason!=ExplorerEligibilityReason::Eligible) {
            group_capture_failure(result,i,GroupCaptureStage::ReceiptHealth,receipt,receipt.invalidation);retire(i);return result;
        }
    }
    if(temporary)return result;
    result.disposition=GroupCaptureDisposition::Succeeded;result.snapshots=std::move(snapshots);
    result.failure_stage=GroupCaptureStage::None;result.reason="none";return result;
}
} // namespace panebind::platform::windows::explorer::detail
