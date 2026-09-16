#include "platform/windows/operations/magnet_postverify_diagnostic.h"
#include <iostream>
namespace o=panebind::platform::windows::operations;
using panebind::core::geometry::Rect;
int main(){
    int checks{},failures{};const auto check=[&](bool ok){++checks;if(!ok)++failures;};
    o::MagnetPostverifyDiagnostic baseline;baseline.capture_succeeded=baseline.native_success=baseline.other_members_exact=baseline.receipt_health=baseline.source_context_exact=true;
    baseline.requested_visible={1279,651,2366,1492};baseline.requested_positioning={1268,651,2377,1503};
    baseline.actual_visible=baseline.requested_visible;baseline.actual_positioning=baseline.requested_positioning;
    const auto classify=[&](auto d,auto expected){d.classify();check(d.failure_class==expected);return d;};
    using F=o::MagnetPostverifyFailure;
    auto d=classify(baseline,F::None);check(d.visible_exact&&d.positioning_exact);
    d=baseline;d.native_success=false;d.win32_error=5;classify(d,F::NativeCallFailed);
    d=baseline;d.capture_succeeded=false;d.actual_visible.reset();d.actual_positioning.reset();d=classify(d,F::CaptureFailed);check(!d.visible_edge_delta&&!d.positioning_edge_delta);
    d=baseline;d.actual_visible=Rect{1284,653,2371,1494};d=classify(d,F::VisibleMismatch);check(d.positioning_exact&&!d.visible_exact);
    d=baseline;d.actual_positioning=Rect{1273,653,2382,1505};classify(d,F::PositioningMismatch);
    d.actual_visible=Rect{1284,653,2371,1494};d=classify(d,F::BothGeometryMismatch);
    check(d.visible_edge_delta==std::array<std::int64_t,4>{5,2,5,2});check(d.positioning_edge_delta==d.visible_edge_delta);
    const auto json=o::magnet_postverify_json(d);check(json.find("BothGeometryMismatch")!=std::string::npos);check(json.find("capture_not_run")==std::string::npos);check(json.find("[5,2,5,2]")!=std::string::npos);
    d=baseline;d.other_members_exact=false;classify(d,F::OtherMemberChanged);
    d=baseline;d.receipt_health=false;classify(d,F::ReceiptHealthFailed);
    d=baseline;d.source_context_exact=false;classify(d,F::SourceContextChanged);
    std::cout<<"magnet-postverify checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
