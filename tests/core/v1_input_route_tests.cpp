#include "platform/windows/product/input_service.h"

#include <iostream>

namespace product = panebind::platform::windows::product;
namespace v1 = panebind::core::v1;
namespace {
int checks{}, failures{};
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
// Uses the exact table builder and route predicate called by the production
// GUI adapter / low-level callback. It never starts InputService, installs a
// hook, synthesizes input, queries foreground, or opens a user window.
int main() {
    product::Prepared prepared;
    auto settings = v1::default_settings();
    prepared.region = 0;
    product::prepare_mappings(prepared, settings);
    check(product::route_down(prepared,1,1,0).route == product::DownRoute::Gesture,
          "plain title left DOWN selects gesture");
    check(product::route_down(prepared,1,1,2).route == product::DownRoute::Gesture,
          "Ctrl title left DOWN retains group route at dispatcher");
    check(product::route_down(prepared,1,1,4).route == product::DownRoute::Pass &&
          product::route_down(prepared,1,1,6).route == product::DownRoute::Pass,
          "Shift and Ctrl Shift ordinary move bypass");
    product::ClickCandidate click;
    click.begin({498,296},1,true);
    click.motion({578,296},4,4);
    check(!click.eligible&&click.pressing,"production press tracking retires the 80 pixel drag before its UP");
    click.motion({498,296},4,4);click.release(1);
    check(!click.eligible&&!click.pressing,"return to DOWN point and matching UP cannot revive dragged click candidate");
    click.begin({-100,-200},1,true);click.motion({-104,-196},4,4);
    check(click.eligible&&click.pressing,"system drag distances are inclusive on both signed axes");
    click.release(2);
    check(click.pressing,"unrelated button UP cannot stop click candidate tracking");
    click.release(1);click.motion({10000,20000},4,4);
    check(click.eligible&&!click.pressing,"matching UP preserves undragged first click and later hover cannot retire it");
    click.begin({0,0},1,true);click.motion({0,-5},4,4);
    check(!click.eligible,"vertical movement beyond drag distance retires first click");
    click.begin({LONG_MIN,LONG_MAX},1,true);click.motion({LONG_MAX,LONG_MIN},4,4);
    check(!click.eligible,"full signed physical coordinate difference cannot overflow click motion check");
    click.begin({50,50},1,true);click.motion({54,54},4,4);click.release(1);
    const auto unchanged_double=product::route_down(prepared,1,click.eligible?2:1,0);
    check(unchanged_double.route==product::DownRoute::Mapped&&unchanged_double.action==v1::Action::Maximize,
          "true undragged title double click retains existing mapped action");
    check(product::route_down(prepared,1,1,4).route==product::DownRoute::Pass,
          "passthrough press fixture uses unchanged native bypass route");
    click.begin({10,10},1,true);click.motion({15,10},4,4);click.release(1);
    check(!click.eligible&&!click.pressing,"same production tracker retires a passthrough drag independently of consumption");
    click.begin({10,10},1,false);click.motion({10,10},4,4);click.release(1);
    check(!click.eligible,"second click does not seed a spurious triple click candidate");
    const auto title_double = product::route_down(prepared,1,2,0);
    check(title_double.route == product::DownRoute::Mapped && title_double.action == v1::Action::Maximize,
          "default title double click is explicit command not native evidence");
    for (int button=2; button<=5; ++button)
        check(product::route_down(prepared,button,1,0).route == product::DownRoute::Pass &&
              product::route_down(prepared,button,2,0).route == product::DownRoute::Pass,
              "unmapped extra button singles and doubles pass through");
    prepared.region = 1;
    product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,2,0).action == v1::Action::StretchLeft,
          "left border default single-axis stretch");
    check(product::route_down(prepared,1,2,4).action == v1::Action::StretchHorizontal,
          "Shift border double click maps before drag bypass");
    prepared.region = 3;
    product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,2,4).action == v1::Action::StretchVertical,
          "top border Shift double maps vertical not horizontal");
    for (int region=5;region<=8;++region) {
        prepared.region=region;product::prepare_mappings(prepared,settings);
        const auto corner=product::route_down(prepared,1,2,0);
        check(corner.route==product::DownRoute::Gesture && corner.action==v1::Action::None,
              "corner double click is contextual two-edge route not StretchAll");
    }
    settings.mouse_bindings.clear();
    // Full five-button/one-two click/modifier index coverage on one exact
    // region; changing region must remove every previous mapping.
    for(int button=1;button<=5;++button)
        for(int clicks=1;clicks<=2;++clicks)
            for(unsigned mods=0;mods<16;++mods)
                settings.mouse_bindings.push_back({2,button,clicks,mods,
                    clicks==1?v1::Action::Center:v1::Action::Restore});
    check(v1::validate(settings).empty(),"complete single-region mapping fixture valid");
    prepared.region=2;product::prepare_mappings(prepared,settings);
    for(int button=1;button<=5;++button)
        for(int clicks=1;clicks<=2;++clicks)
            for(unsigned mods=0;mods<16;++mods){
                const auto decision=product::route_down(prepared,button,clicks,mods);
                check(decision.route==product::DownRoute::Mapped &&
                    decision.action==(clicks==1?v1::Action::Center:v1::Action::Restore),
                    "actual builder and callback index agree for five keys two clicks sixteen modifier masks");
            }
    prepared.region=0;product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,5,2,15).route==product::DownRoute::Pass,
          "region change clears stale mapping table");
    prepared.region=-1;product::prepare_mappings(prepared,settings);
    check(product::route_down(prepared,1,1,0).route==product::DownRoute::Pass,
          "system buttons and unsupported client region cannot acquire gesture");
    prepared.region=9;
    check(product::route_down(prepared,1,1,0).route==product::DownRoute::Pass,
          "unknown tenth region rejected");
    prepared.region=0;
    check(product::route_down(prepared,0,1,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,6,1,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,1,3,0).route==product::DownRoute::Pass &&
          product::route_down(prepared,1,1,16).route==product::DownRoute::Pass,
          "out-of-range route inputs do not index table");
    check(!product::native_event_is_current(99,100)&&product::native_event_is_current(100,100)&&
          product::native_event_is_current(101,100),"previous gesture native event cannot invalidate new DOWN");
    check(product::native_event_is_current(2,0xfffffff0u)&&!product::native_event_is_current(0xfffffff0u,2),
          "native 32 bit timestamp fence handles system tick rollover");
    // This is the production guard predicate, including the formerly unsafe
    // normal-UP completion route. Follow the actual receive/dispatch order.
    std::uint64_t active=1,completion=0;
    check(product::operation_permission(1,active,completion,true,false),"g1 DOWN owns active writes");
    active=0;completion=1;
    check(!product::operation_permission(1,active,completion,true,false)&&
          product::operation_permission(1,active,completion,true,true),"actual normal UP permits only its distinct completion route");
    completion=0;active=2;
    check(!product::operation_permission(1,active,completion,true,true),"new DOWN retires g1 completion even when its captured allow_completion boolean was true");
    active=0;completion=2;
    check(!product::operation_permission(1,active,completion,true,true)&&product::operation_permission(2,active,completion,true,true),
          "g2 normal UP never revives g1");
    completion=0;
    check(!product::operation_permission(2,active,completion,true,true),"abort retires completion without pretending the native call was cancelled");
    check(!product::operation_permission(2,0,2,false,true),"pause or stop independently rejects same-generation completion");
    check(product::matching_up_end(false,v1::End::NormalUp,true,true)==v1::End::NormalUp,
          "actual foreground and current source generation permit matching normal UP");
    check(product::matching_up_end(false,v1::End::NormalUp,true,false)==v1::End::FocusLost,
          "UP arriving before queued foreground WinEvent still records actual focus loss");
    check(product::matching_up_end(false,v1::End::NormalUp,false,true)==v1::End::SourceLost&&
          product::matching_up_end(false,v1::End::NormalUp,false,false)==v1::End::SourceLost,
          "destroyed replaced or explicitly retired source generation cannot receive normal completion");
    check(product::matching_up_end(true,v1::End::FocusLost,true,true)==v1::End::FocusLost&&
          product::matching_up_end(true,v1::End::Deadline,true,true)==v1::End::Deadline&&
          product::matching_up_end(true,v1::End::Paused,true,true)==v1::End::Paused,
          "foreground return after an observed abort cannot rewrite failure as NormalUp");
    check(product::matching_up_end(true,v1::End::NormalUp,true,true)!=v1::End::NormalUp,
          "already aborted admission cannot mint a completion capability even if its old end was normal");
    active=5;completion=0;
    const auto up_end=product::matching_up_end(false,v1::End::NormalUp,true,false);
    completion=up_end==v1::End::NormalUp?active:0;active=0;
    check(!product::operation_permission(5,active,completion,true,true),
          "production matching-UP failure leaves both active writes and discrete completion revoked");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures==0?0:1;
}
