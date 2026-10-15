#include "platform/windows/product/window_writer.h"

#include <iostream>
#include <limits>
#include <string_view>

namespace p = panebind::platform::windows::product;
using panebind::core::v1::Rect;

namespace {
p::Snapshot fixture() {
    p::Snapshot value;
    // Opaque identities only: this test never queries or operates these values.
    value.hwnd = reinterpret_cast<HWND>(std::uintptr_t{0x1234});
    value.monitor = reinterpret_cast<HMONITOR>(std::uintptr_t{0x4321});
    value.pid = 20;
    value.tid = 30;
    value.born = 40;
    value.key = 50;
    value.visible = {100, 100, 600, 500};
    value.positioning = {92, 100, 608, 508};
    value.work = {0, 0, 1920, 1040};
    value.dpi = 96;
    value.exe = L"C:\\fixture\\owned-test.exe";
    value.window_class = L"PaneBind.Test.Writer";
    value.can_resize = true;
    return value;
}
} // namespace

int main() {
    int checks{}, failures{};
    const auto check = [&](bool condition, std::string_view name) {
        ++checks;
        if (!condition) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    const auto initial = fixture();
    check(p::WindowWriter::complete_unissued_on_stop(false,false),"stop completes and signals only never-issued pending admission");
    check(!p::WindowWriter::complete_unissued_on_stop(true,false),"stop cannot signal or claim completion of an issued unknown native call");
    check(!p::WindowWriter::complete_unissued_on_stop(false,true)&&!p::WindowWriter::complete_unissued_on_stop(true,true),
        "stop does not invent another completion for an already finished call");
    const Rect desired{180, 130, 730, 570};
    auto prepared = p::WindowWriter::prepare(initial, initial, desired);
    check(prepared.accepted && prepared.identity_valid && !prepared.noop, "G01 admission");
    check(prepared.positioning == Rect{172, 130, 738, 578}, "G01 independent positioning expectation");

    auto negative = initial;
    negative.visible = {-1000, -400, -500, 0};
    negative.positioning = {-1008, -400, -492, 8};
    prepared = p::WindowWriter::prepare(negative, negative, {-920, -370, -370, 70});
    check(prepared.accepted && prepared.positioning == Rect{-928, -370, -362, 78}, "negative screen coordinates");

    prepared = p::WindowWriter::prepare(initial, initial, initial.visible);
    check(prepared.accepted && prepared.noop && prepared.positioning == initial.positioning, "exact duplicate no-native preparation");
    auto changed = initial;
    changed.visible = {120, 100, 620, 500};
    changed.positioning = {112, 100, 628, 508};
    prepared = p::WindowWriter::prepare(initial, changed, desired);
    check(!prepared.accepted && prepared.identity_valid && prepared.reason == "unattributed_geometry_change", "external translation rejected");
    changed = initial;
    changed.positioning = {91, 100, 607, 508};
    prepared = p::WindowWriter::prepare(initial, changed, desired);
    check(!prepared.accepted && prepared.reason == "unattributed_geometry_change", "positioning-only continuity change rejected");

    changed = initial;
    ++changed.key;
    prepared = p::WindowWriter::prepare(initial, changed, desired);
    check(!prepared.accepted && !prepared.identity_valid, "recycled HWND generation rejected");
    changed = initial;
    changed.hwnd = reinterpret_cast<HWND>(std::uintptr_t{0x1235});
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "different HWND rejected");
    changed = initial;
    ++changed.pid;
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "different process rejected");
    changed = initial;
    ++changed.tid;
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "different window thread rejected");
    changed = initial;
    ++changed.born;
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "different process birth rejected");
    changed = initial;
    changed.exe = L"C:\\other\\owned-test.exe";
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "changed original executable identity rejected");
    changed = initial;
    changed.exe = L"c:\\FIXTURE\\OWNED-TEST.EXE";
    check(p::WindowWriter::prepare(initial, changed, desired).accepted, "Windows case-insensitive executable identity");

    changed = initial;
    changed.dpi = 144;
    prepared = p::WindowWriter::prepare(initial, changed, desired);
    check(!prepared.accepted && prepared.reason == "geometry_context_changed", "DPI changed before placement");
    changed = initial;
    changed.monitor = reinterpret_cast<HMONITOR>(std::uintptr_t{0x4322});
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "monitor changed before placement");
    changed = initial;
    changed.work = {0, 0, 1920, 1000};
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "work area changed before placement");
    changed = initial;
    changed.maximized = true;
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "window state changed before placement");
    changed = initial;
    changed.can_resize = false;
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "resize capability changed before placement");

    auto constrained = initial;
    constrained.limits.min_width = 300;
    check(!p::WindowWriter::prepare(constrained, constrained, {100, 100, 300, 500}).accepted, "minimum width enforced");
    constrained.limits.max_width = 600;
    check(!p::WindowWriter::prepare(constrained, constrained, {100, 100, 800, 500}).accepted, "maximum width enforced");
    constrained = initial;
    constrained.limits.min_height = 200;
    check(!p::WindowWriter::prepare(constrained, constrained, {100, 100, 600, 250}).accepted, "minimum height enforced");
    constrained.limits.max_height = 600;
    check(!p::WindowWriter::prepare(constrained, constrained, {100, 100, 600, 800}).accepted, "maximum height enforced");
    constrained = initial;
    constrained.limits.grid_width = 50;
    check(p::WindowWriter::prepare(constrained, constrained, desired).accepted, "legal width grid");
    check(!p::WindowWriter::prepare(constrained, constrained, {180, 130, 731, 570}).accepted, "width grid violation");
    constrained.limits.grid_height = 20;
    check(!p::WindowWriter::prepare(constrained, constrained, {180, 130, 730, 571}).accepted, "height grid violation");
    constrained = initial;
    constrained.limits.grid_width = 0;
    check(!p::WindowWriter::prepare(constrained, constrained, desired).accepted, "invalid zero grid rejected");
    constrained = initial;
    constrained.can_resize = false;
    check(!p::WindowWriter::prepare(constrained, constrained, desired).accepted, "fixed-size frame refuses resize");
    check(p::WindowWriter::prepare(constrained, constrained, {150, 130, 650, 530}).accepted, "fixed-size frame can translate");

    constexpr auto hi = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    prepared = p::WindowWriter::prepare(initial, initial, {hi - 10, 100, hi + 490, 500});
    check(!prepared.accepted && prepared.reason == "invalid_or_out_of_native_range_target", "visible coordinate exceeds native range");
    prepared = p::WindowWriter::prepare(initial, initial, {hi - 508, 100, hi, 500});
    check(!prepared.accepted && prepared.reason == "positioning_bridge_invalid_or_out_of_native_range", "frame offset overflows native positioning range");
    prepared = p::WindowWriter::prepare(initial, initial,
        {std::numeric_limits<std::int64_t>::min(), 0, std::numeric_limits<std::int64_t>::max(), 100});
    check(!prepared.accepted && !prepared.positioning, "64-bit extent overflow rejected before subtraction");
    check(!p::WindowWriter::prepare(initial, initial, {100, 100, 100, 500}).accepted, "zero width rejected");
    changed = initial;
    changed.positioning = {};
    check(!p::WindowWriter::prepare(initial, changed, desired).accepted, "invalid actual positioning frame rejected");

    namespace v=panebind::core::v1;
    v::VisualState original;auto applied=original;applied.layered=true;applied.alpha=190;applied.flags=LWA_ALPHA;
    auto visual=p::WindowWriter::prepare_visual(original,original,applied,true,false);
    check(visual.accepted&&!visual.noop,"actual shared visual alpha admission requires a native change");
    auto external=applied;external.alpha=120;
    check(!p::WindowWriter::prepare_visual(applied,external,original,true,false).accepted,"external alpha change is not silently rebased");
    v::VisualOwnership owned{original,applied,false,true};
    visual=p::WindowWriter::prepare_visual(applied,applied,original,true,false,owned);
    check(visual.accepted&&!visual.noop,"retired-input cleanup may restore only the still-owned original");
    check(!p::WindowWriter::prepare_visual(applied,applied,external,true,false,owned).accepted,"cleanup mode cannot set a different new alpha");
    check(!p::WindowWriter::prepare_visual(applied,external,original,true,false,owned).accepted,"cleanup mode cannot overwrite an externally replaced tuple");
    owned.changed_alpha=false;
    check(!p::WindowWriter::prepare_visual(applied,applied,original,true,false,owned).accepted,"cleanup mode rejects unowned alpha even with a caller guard");
    auto unknown=original;unknown.layered=true;unknown.attributes_known=false;
    auto top=unknown;top.topmost=true;
    check(p::WindowWriter::prepare_visual(unknown,unknown,top,false,true).accepted,"top-only operation preserves unreadable unrelated alpha");
    check(!p::WindowWriter::prepare_visual(unknown,unknown,applied,true,false).accepted,"unreadable alpha cannot acquire mutation authority");
    owned={unknown,top,true,false};
    check(p::WindowWriter::prepare_visual(top,top,unknown,false,true,owned).accepted,"exact owned top can be restored independently of unknown alpha");
    owned.original.topmost=true;
    check(!p::WindowWriter::prepare_visual(top,top,unknown,false,true,owned).accepted,"cleanup top target must equal its held original");
    check(p::WindowWriter::prepare_visual(applied,applied,applied,true,false).noop,"already-exact property receipt does not invent a native API call");

    std::cout << "v1-window-writer-preparation checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
