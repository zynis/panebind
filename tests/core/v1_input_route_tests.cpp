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
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures==0?0:1;
}
