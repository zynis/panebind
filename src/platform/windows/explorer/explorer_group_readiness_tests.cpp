#include "platform/windows/explorer/explorer_group_readiness.h"
#include "platform/windows/operations/window_translation.h"
#include "core/behavior/glue_group_move.h"
#include <iostream>

namespace e=panebind::platform::windows::explorer;
namespace w=panebind::platform::windows::operations::window_translation;
using panebind::core::geometry::Rect;
namespace {
int failures{};
void check(bool condition,const char* name) {
    if(!condition){++failures;std::cerr<<"FAIL "<<name<<'\n';}
}
e::detail::GroupSnapshots snapshots(bool large) {
    e::detail::GroupSnapshots result;
    for(std::size_t i=0;i<3;++i) {
        auto& s=result[i];
        const auto offset=static_cast<std::int64_t>(i)*49;
        const auto width=large?1839:1300;const auto height=large?1026:700;
        s.visible_rect={11+offset,offset,11+offset+width,offset+height};
        s.positioning_rect={offset,offset,22+offset+width,11+offset+height};
        s.dpi=192;s.monitor_device_name=L"DISPLAY1";s.monitor_work_area={0,0,3072,1824};
        s.process_id=90;s.thread_id=200+static_cast<std::uint32_t>(i);
        s.process_image_path=L"C:\\Windows\\explorer.exe";s.window_class=L"CabinetWClass";
        s.visible=true;s.root_top_level=true;s.on_current_virtual_desktop=true;s.exact_test_location=true;
    }
    return result;
}
const e::GroupReadinessActivity idle{true,64,0,0,0,0,false,false};
}
int main() {
    { // A: immediate fit, independent setup capture, no baseline change.
        auto live=snapshots(false);e::GroupReadinessFixture fixture{64,live};int captures{};
        auto capture=[&]()->std::optional<e::detail::GroupSnapshots>{++captures;return live;};
        const auto p=fixture.preview(idle,capture);
        check(p.valid&&p.readiness.ready&&!p.setup_check&&p.attempt==1,"A preview fits");
        check(!fixture.accepted(),"preview alone never freezes baseline");
        const auto setup=fixture.prepare_setup(idle,capture);
        check(setup.valid&&setup.readiness.ready&&setup.setup_check&&captures==2,"A setup fresh capture");
        check(fixture.accepted()->snapshots==live,"A accepted equals current geometry");
        check(!fixture.preview(idle,capture).valid&&captures==2,"accepted fixture cannot re-preview/rebase again");
    }
    { // B: binding geometry is oversized; HUMAN resize is represented by input.
        auto binding=snapshots(true),live=binding;e::GroupReadinessFixture fixture{64,binding};int captures{};
        auto capture=[&]()->std::optional<e::detail::GroupSnapshots>{++captures;return live;};
        const auto p=fixture.preview(idle,capture);
        check(p.valid&&!p.readiness.ready&&p.readiness.width_deficit==606&&p.readiness.height_deficit==228,"B actual failed-fixture deficits");
        check(p.member_sizes[0][0]==1839&&p.required_width==3678&&p.required_height==2052,"B dimensions and requirement");
        check(!fixture.accepted()&&!p.activity.native_apply_count&&!p.activity.hdwp_begin_count,"B zero writes before acceptance");
        live=snapshots(false);
        check(fixture.preview(idle,capture).readiness.ready,"B live re-preview sees human resize");
        const auto accepted=fixture.prepare_setup(idle,capture);
        check(captures==3&&accepted.snapshots==live&&accepted.snapshots!=binding,"B accepts resized fresh geometry");
        for(std::size_t i=0;i<3;++i) {
            const auto& baseline=(*accepted.snapshots)[i];
            const auto setup=w::prepare_visible_translation(baseline.positioning_rect,baseline.visible_rect,accepted.readiness.targets[i]);
            check(setup.status==w::TranslationPreparationStatus::Succeeded,"B setup remains pure translation");
            const auto restore=w::prepare_visible_translation(*setup.target_positioning_rect,accepted.readiness.targets[i],baseline.visible_rect);
            check(restore.status==w::TranslationPreparationStatus::Succeeded&&restore.target_positioning_rect==baseline.positioning_rect,"B restore accepted positioning and size");
            check(baseline.visible_rect.right()-baseline.visible_rect.left()==1300,"B never restores old 1839 width");
        }
    }
    { // C: FIT preview cannot authorize stale setup geometry.
        auto live=snapshots(false);e::GroupReadinessFixture fixture{64,live};int captures{};
        auto capture=[&]()->std::optional<e::detail::GroupSnapshots>{++captures;return live;};
        check(fixture.preview(idle,capture).readiness.ready,"C initial fit");
        live=snapshots(true);
        const auto setup=fixture.prepare_setup(idle,capture);
        check(setup.valid&&!setup.readiness.ready&&!fixture.accepted()&&captures==2,"C setup TOCTOU rejects stale FIT");
        check(setup.activity.native_apply_count==0&&setup.activity.hdwp_begin_count==0,"C no setup native entry");
        live=snapshots(false);
        check(fixture.preview(idle,capture).readiness.ready&&fixture.prepare_setup(idle,capture).readiness.ready,"C returns to readiness loop");
    }
    { // D: a failed complete member validator is terminal for this fixture.
        e::GroupReadinessFixture fixture{64,snapshots(false)};int captures{};
        auto invalid=[&]()->std::optional<e::detail::GroupSnapshots>{++captures;return std::nullopt;};
        check(!fixture.preview(idle,invalid).valid,"D invalid member fails closed");
        auto later=[&]()->std::optional<e::detail::GroupSnapshots>{++captures;return snapshots(false);};
        check(!fixture.prepare_setup(idle,later).valid&&captures==1&&!fixture.accepted(),"D no retry/reissue after invalidation");
    }
    for(int failure=0;failure<5;++failure) {
        auto binding=snapshots(false),live=binding;e::GroupReadinessFixture fixture{64,binding};
        if(failure==0)live[2].dpi=96;
        if(failure==1)live[1].monitor_device_name=L"DISPLAY2";
        if(failure==2)live[0].exact_test_location=false;
        if(failure==3)++live[2].process_id;
        if(failure==4)live[1].on_current_virtual_desktop=false;
        check(!fixture.preview(idle,[&]{return std::optional{live};}).valid,"D nongeometry context cannot be rebased");
    }
    { // E: repeated previews cannot change group/gesture/roles/pending/source.
        namespace b=panebind::core::behavior;namespace m=panebind::core::model;
        b::GlueGroupMoveCoordinator model{{{m::WindowId{"A"},1},{m::WindowId{"B"},2},{m::WindowId{"C"},3}},64};
        e::GroupReadinessFixture fixture{64,snapshots(false)};int captures{};
        for(std::uint64_t i=1;i<=5;++i) {
            const auto p=fixture.preview(idle,[&]{++captures;return std::optional{snapshots(i%2==0)};});
            check(p.valid&&p.attempt==i&&p.activity.group_generation==64&&!p.activity.gesture_generation,"E monotonic preview, stable authority");
            check(!p.activity.native_apply_count&&!p.activity.hdwp_begin_count&&!p.activity.event_source_running,"E zero native/hook side effects");
            check(model.state()==b::GlueGroupState::GroupReady&&model.gesture_generation()==0&&!model.gesture_leader()&&model.pending().empty(),"E no gesture or role assigned");
        }
        check(captures==5&&!fixture.accepted(),"E each preview is fresh and not accepted");
        auto wrong_thread=idle;wrong_thread.owner_thread=false;
        check(!fixture.preview(wrong_thread,[&]{++captures;return std::optional{snapshots(false)};}).valid&&captures==5,"owner-thread-only preview");
    }
    std::cout<<"group-readiness failures="<<failures<<'\n';return failures?1:0;
}
