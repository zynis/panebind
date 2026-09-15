#include "platform/windows/explorer/explorer_group_readiness.h"
#include "platform/windows/explorer/explorer_group_capture_json.h"
#include "platform/windows/explorer/explorer_session_internal.h"
#include <iostream>
namespace e=panebind::platform::windows::explorer;
namespace d=e::detail;
using panebind::core::geometry::Rect;
namespace {
int failures{},checks{};
void check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
d::EligibilityModelFacts eligible() {
    d::EligibilityModelFacts f;
    f.window_exists=f.process_alive=f.process_id_stable=f.thread_id_stable=f.image_matches=f.class_allowed=true;
    f.root_is_self=f.visible=f.current_virtual_desktop=f.security_query_succeeded=f.same_user=f.same_session=true;
    f.same_integrity=f.medium_integrity=f.location_exact=f.geometry_available=f.dpi_context_supported=true;
    f.monitor_available=f.monitor_stable=f.dpi_stable=f.consent_frame_bound=true;return f;
}
struct Boundary {
    d::GroupSnapshots snapshots;
    std::array<d::EligibilityModelFacts,3> facts{eligible(),eligible(),eligible()};
    std::array<e::BrowserReadinessFacts,3> browser;
    std::array<bool,3> retired{};
    std::array<bool,3> canonical{true,true,true};
    std::array<std::uint64_t,3> capability{11,22,33},consent{5,6,7};
    std::array<std::string_view,3> binding_failure{"none","none","none"};
    int native_reads{},retire_calls{};bool late_binding_failure{};
    Boundary(){
        const std::array<Rect,3> rects{{{0,0,120,180},{120,40,220,180},{20,180,260,270}}};
        for(std::size_t i=0;i<3;++i){auto& s=snapshots[i];s.visible_rect=s.positioning_rect=rects[i];
            s.process_id=9;s.thread_id=static_cast<std::uint32_t>(10+i);s.dpi=192;s.monitor_device_name=L"DISPLAY1";
            s.monitor_rect=s.monitor_work_area={0,0,3072,1824};s.visible=s.root_top_level=s.on_current_virtual_desktop=s.exact_test_location=true;
            browser[i].subscribed=browser[i].accepting=true;
        }
    }
    d::GroupCaptureResult capture(d::GroupCaptureMode mode=d::GroupCaptureMode::PreAcceptMutableGeometry){
        const auto binding=[&](std::size_t i)->std::string_view {
            if(retired[i])return "stale_token";
            if(capability[i]!=(i+1)*11)return "capability_generation_changed";
            if(consent[i]!=i+5)return "consent_generation_changed";
            if(late_binding_failure&&native_reads>=3&&i==1)return "hwnd_changed";
            return binding_failure[i];
        };
        const auto native=[&](std::size_t i){
            ++native_reads;d::GroupMemberCaptureResult r;
            r.reason=d::evaluate_consent_live_eligibility({true,true,true,facts[i]});
            if(!canonical[i]){r.reason=e::ExplorerEligibilityReason::CanonicalIdentityMismatch;r.invalidation="canonical_identity_changed";}
            r.observation.canonical_identity_matches=canonical[i];r.observation.anchor_hwnd_matches=facts[i].window_exists;
            r.observation.location_exact=facts[i].location_exact;
            r.authority_proven=canonical[i]&&(r.reason==e::ExplorerEligibilityReason::Eligible||
                r.reason==e::ExplorerEligibilityReason::MonitorChanged||r.reason==e::ExplorerEligibilityReason::DpiChanged);
            if(r.reason==e::ExplorerEligibilityReason::Eligible)r.snapshot=snapshots[i];
            else r.diagnostic=d::GroupCaptureDiagnostic{e::ExplorerDiagnosticDomain::Adapter,1104};
            return r;
        };
        const auto receipt=[&](std::size_t i){
            d::GroupMemberCaptureResult r;r.observation.browser_observed=true;r.observation.browser=browser[i];
            r.observation.navigation_epoch=0;
            r.invalidation=e::consent_browser_invalidation(browser[i],0);r.observation.browser_stream_reason=r.invalidation;
            if(r.invalidation!="none")r.reason=e::ExplorerEligibilityReason::ShellEventStreamInvalid;return r;
        };
        return d::capture_group_members(mode,binding,native,receipt,[&](std::size_t i){retired[i]=true;++retire_calls;});
    }
};
const e::GroupReadinessActivity idle{true,64,0,0,0,0,false,false};
}
int main(int argc,char** argv){
    if(argc==2&&std::string_view{argv[1]}=="--diagnostic-samples") {
        Boundary binding;binding.binding_failure[1]="binding_mismatch";
        std::cout<<d::group_capture_json(binding.capture())<<'\n';
        Boundary native;native.facts[1].monitor_stable=false;
        std::cout<<d::group_capture_json(native.capture())<<'\n';
        Boundary receipt;receipt.browser[2].malformed_count=1;receipt.browser[2].last_malformed_dispid=9999;
        std::cout<<d::group_capture_json(receipt.capture())<<'\n';
        Boundary context;e::GroupReadinessFixture fixture{64,context.snapshots};context.snapshots[2].process_id=999;
        std::cout<<d::group_capture_json(fixture.preview(idle,[&]{return context.capture();}).capture_result)<<'\n';
        return 0; // synthetic serializer samples, not a human UAT log
    }
    {Boundary b;e::GroupReadinessFixture fixture{64,b.snapshots};const auto caps=b.capability,consent=b.consent;
        auto capture=[&]{return b.capture();};
        check(fixture.preview(idle,capture).valid,"initial capture");
        for(int n=0;n<8;++n){
            for(auto& s:b.snapshots){
                if(n%2==0){s.visible_rect=panebind::core::movement::translate_rect(s.visible_rect,{5,7});}
                else s.visible_rect={s.visible_rect.left(),s.visible_rect.top(),s.visible_rect.right()+3,s.visible_rect.bottom()+4};
                s.positioning_rect=s.visible_rect;
            }
            const auto p=fixture.preview(idle,capture);
            check(p.valid&&p.snapshots==b.snapshots,"A/B/C fresh Move/Resize geometry");
            check(b.retire_calls==0&&b.capability==caps&&b.consent==consent,"A/B/C authority retained");
            check(p.activity.group_generation==64&&p.activity.gesture_generation==0&&p.activity.native_apply_count==0&&
                p.activity.hdwp_begin_count==0&&!p.activity.event_source_running&&!p.activity.leader_present&&!p.activity.pending_count,"C no runtime side effects");
        }
        const auto p=fixture.preview(idle,capture);check(!p.readiness.ready&&!fixture.accepted()&&!b.retire_calls,"D disconnected is not retirement");
        Boundary connected;b.snapshots=connected.snapshots;
        for(auto& s:b.snapshots)s.visible_rect=s.positioning_rect=panebind::core::movement::translate_rect(s.visible_rect,{30,40});
        const auto accepted=fixture.prepare_setup(idle,capture);
        check(accepted.valid&&accepted.readiness.ready&&fixture.accepted()->snapshots==b.snapshots,"accepted fresh baseline after manual adjustments");
    }
    for(int fault=0;fault<14;++fault){Boundary b;std::size_t member=1;
        switch(fault){
        case 0:b.facts[member].location_exact=false;break;
        case 1:b.facts[member].window_exists=false;break;
        case 2:b.facts[member].process_id_stable=false;break;
        case 3:b.facts[member].thread_id_stable=false;break;
        case 4:b.browser[member].quit_count=1;break;
        case 5:b.facts[member].security_query_succeeded=false;break;
        case 6:b.facts[member].elevated=true;break;
        case 7:b.canonical[member]=false;break;
        case 8:b.capability[member]++;break;
        case 9:b.consent[member]++;break;
        case 10:b.facts[member].current_virtual_desktop=false;break;
        case 11:b.facts[member].class_allowed=false;break;
        case 12:b.facts[member].image_matches=false;break;
        case 13:b.browser[member].malformed_count=1;break;
        }
        const auto r=b.capture();check(!r&&!r.recoverable()&&r.failed_member_index==member&&b.retired[member],"E-H/J authority failure fatal and retired");
        const auto json=d::group_capture_json(r);
        check(json.find("\"failed_member_index\":1")!=std::string::npos&&json.find("\"failure_stage\":\"None\"")==std::string::npos,"member and stage serialized");
    }
    for(bool dpi:{false,true}){Boundary b;e::GroupReadinessFixture fixture{64,b.snapshots};auto capture=[&]{return b.capture();};
        if(dpi)b.facts[1].dpi_stable=false;else b.facts[1].monitor_stable=false;
        const auto p=fixture.preview(idle,capture);
        check(!p.valid&&p.capture_result.recoverable()&&!b.retire_calls&&!fixture.accepted(),"temporary monitor/DPI no retirement");
        const auto json=d::group_capture_json(p.capture_result);
        check(json.find(dpi?"DpiChanged":"MonitorChanged")!=std::string::npos,"exact native diagnostic reason");
        b.facts[1]=eligible();check(fixture.preview(idle,capture).valid&&fixture.prepare_setup(idle,capture).readiness.ready,"return original monitor/DPI can accept");
    }
    {Boundary b;b.facts[0].monitor_stable=false;b.facts[2].same_user=false;const auto r=b.capture();
        check(!r.recoverable()&&r.failed_member_index==2&&b.retired[2],"later fatal outranks temporary monitor");}
    {Boundary b;b.facts[1].monitor_stable=false;b.facts[1].security_query_succeeded=false;const auto r=b.capture();
        check(!r.recoverable()&&r.eligibility_reason==e::ExplorerEligibilityReason::SecurityQueryFailed,"monitor mismatch cannot hide security failure");}
    {Boundary b;b.facts[1].monitor_stable=false;const auto r=b.capture(d::GroupCaptureMode::Strict);
        check(!r.recoverable()&&b.retired[1],"strict mode does not allow monitor drift");}
    {Boundary b;b.browser[2].geometry_event_count=1000;b.browser[2].last_geometry_dispid=267;
        check(b.capture().succeeded()&&!b.retire_calls,"I benign geometry facts do not invalidate browser");}
    {Boundary b;b.late_binding_failure=true;const auto r=b.capture();check(!r&&r.failure_stage==d::GroupCaptureStage::Binding&&b.retired[1],"recheck binding after COM validation");}
    {Boundary b;b.browser[2].malformed_count=1;b.browser[2].last_malformed_dispid=9999;const auto r=b.capture();const auto json=d::group_capture_json(r);
        check(r.failure_stage==d::GroupCaptureStage::ReceiptHealth&&json.find("browser_stream_invalid")!=std::string::npos&&json.find("9999")!=std::string::npos,"receipt diagnostic includes exact stream facts");}
    {Boundary b;e::GroupReadinessFixture fixture{64,b.snapshots};b.snapshots[2].process_id=999;
        auto p=fixture.preview(idle,[&]{return b.capture();});
        check(!p.valid&&p.capture_result.failure_stage==d::GroupCaptureStage::Context&&p.capture_result.failed_member_index==2,"immutable fixture context names member");}
    {d::GroupCaptureResult r;r.reason="C:\\private\\file";r.glue_validation_invalidation="clipboard secret";
        auto json=d::group_capture_json(r);check(json.find("private")==std::string::npos&&json.find("clipboard")==std::string::npos,"diagnostics cannot leak free text");}
    std::cout<<"group-capture checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
