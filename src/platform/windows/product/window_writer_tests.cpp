#include "platform/windows/product/window_writer.h"

#include <array>
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
    using Lower=p::WindowWriter::LowerZOrderKind;
    p::WindowWriter::LowerZOrderSample z;z.known=true;z.source=initial.hwnd;z.source_pid=initial.pid;z.source_tid=initial.tid;
    z.previous=reinterpret_cast<HWND>(std::uintptr_t{0x2345});z.shell=reinterpret_cast<HWND>(std::uintptr_t{0x3456});
    check(p::WindowWriter::lower_z_order_completion(initial,z,z)==Lower::RawBottom,
        "stable complete raw NULL tail retains the original Lower completion policy");
    z.next=z.shell;z.shell_known=true;z.shell_pid=60;z.shell_tid=70;z.shell_previous=z.source;
    check(p::WindowWriter::lower_z_order_completion(initial,z,z)==Lower::ShellDesktopTail,
        "actual shared Lower postverify accepts only source then exact current shell then raw NULL");
    auto bad_z=z;bad_z.known=false;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "unknown source observation is not a successful native Lower completion");
    bad_z=z;bad_z.shell_known=false;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "unknown shell identity cannot grant the desktop-tail exception");
    bad_z=z;bad_z.shell=nullptr;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "unavailable GetShellWindow cannot authorize a non-NULL ordinary tail");
    bad_z=z;bad_z.next=reinterpret_cast<HWND>(std::uintptr_t{0x4567});
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "an ordinary or hidden root after source is never skipped toward the shell");
    bad_z=z;bad_z.shell_next=reinterpret_cast<HWND>(std::uintptr_t{0x4567});
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "an additional node after the actual shell is not an exact terminal desktop tail");
    bad_z=z;bad_z.shell_next=z.source;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "a source-shell cycle cannot manufacture complete tail evidence");
    bad_z=z;bad_z.next=z.source;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "a source self-cycle cannot manufacture raw bottom evidence");
    bad_z=z;bad_z.shell_previous=z.previous;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "shell reciprocal previous must identify this exact source");
    bad_z=z;++bad_z.source_pid;
    check(p::WindowWriter::lower_z_order_completion(initial,bad_z,bad_z)==Lower::Unconfirmed,
        "lower Z-order proof does not replace exact source identity checks");
    bad_z=z;++bad_z.shell_tid;
    check(p::WindowWriter::lower_z_order_completion(initial,z,bad_z)==Lower::Unconfirmed,
        "shell identity changed across the two bounded samples rejects completion");
    bad_z=z;bad_z.previous=nullptr;
    check(p::WindowWriter::lower_z_order_completion(initial,z,bad_z)==Lower::Unconfirmed,
        "source link changes across observations are not a stable global Z snapshot");
    bad_z=z;bad_z.shell=reinterpret_cast<HWND>(std::uintptr_t{0x4567});bad_z.next=bad_z.shell;
    check(p::WindowWriter::lower_z_order_completion(initial,z,bad_z)==Lower::Unconfirmed,
        "a replacement actual shell is not silently accepted as the original observation");
    p::WindowWriter::Receipt lower_notice;lower_notice.operation=p::WindowWriter::Operation::Lower;
    lower_notice.native_attempted=true;lower_notice.native_succeeded=true;lower_notice.completion_known=true;
    check(std::wstring_view(p::WindowWriter::lower_failure_notice(lower_notice))==
          L"置后原生调用已结束，但窗口后验未确认；请查看诊断记录。",
        "completed API success with failed postverify never claims it is still waiting for the call");
    lower_notice.native_succeeded=false;
    check(std::wstring_view(p::WindowWriter::lower_failure_notice(lower_notice))==L"置后原生调用已失败；请查看诊断记录。",
        "known native failure is reported separately from unknown completion");
    lower_notice.completion_known=false;
    check(std::wstring_view(p::WindowWriter::lower_failure_notice(lower_notice))==
          L"置后操作尚未取得真实完成回执；后续操作等待该操作结束。",
        "only genuinely unknown completion retains the wait-for-operation notice");
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

    // The production adapter binds this proof from MonitorFromRect(native),
    // before issuing the call. These opaque fixtures exercise that same
    // admission/postverify policy, never a simulated successful Win32 call.
    const p::Display primary{initial.monitor,initial.work,{0,0,1920,1080},96};
    const p::Display left{reinterpret_cast<HMONITOR>(std::uintptr_t{0x4322}),
        {-1920,-40,0,1000},{-1920,-80,0,1000},96};
    const std::array displays{primary,left};
    const Rect cross_native{-1008,100,-492,508};
    auto destination=p::WindowWriter::prepare_destination(initial,cross_native,left.monitor,DPI_AWARENESS_PER_MONITOR_AWARE,displays);
    check(destination.accepted&&destination.proof&&destination.proof->window_dpi==96&&destination.proof->positioning==cross_native,
        "explicit native target binds same-DPI negative-coordinate destination before call");
    auto after=initial;after.visible={-1000,100,-500,500};after.positioning=cross_native;after.monitor=left.monitor;after.work=left.work;
    std::string context_reason;
    check(p::WindowWriter::placement_context_matches(initial,after,destination.proof,displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "own exact same-DPI cross-display placement is not an external context failure");
    check(!p::WindowWriter::placement_context_matches(initial,after,{},displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "unbound cross-display result is still rejected");
    auto topology=displays;topology[1].work={-1920,-40,0,960};
    check(!p::WindowWriter::placement_context_matches(initial,after,destination.proof,topology,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "destination taskbar/work change after binding rejects completion");
    topology=displays;topology[0].bounds={0,0,1600,1080};
    check(!p::WindowWriter::placement_context_matches(initial,after,destination.proof,topology,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "other display topology change is not attributed to own placement");
    check(!p::WindowWriter::placement_context_matches(initial,after,destination.proof,std::span<const p::Display>{displays}.first(1),DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "display removal rejects the frozen destination proof");
    auto wrong_after=after;wrong_after.monitor=primary.monitor;
    check(!p::WindowWriter::placement_context_matches(initial,wrong_after,destination.proof,displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "completion on a different monitor cannot select its own new baseline");
    wrong_after=after;wrong_after.dpi=144;
    check(!p::WindowWriter::placement_context_matches(initial,wrong_after,destination.proof,displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "unexpected window DPI still rejects completion");
    wrong_after=after;++wrong_after.born;
    check(!p::WindowWriter::placement_context_matches(initial,wrong_after,destination.proof,displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "destination proof grants no recycled identity authority");
    wrong_after=after;wrong_after.maximized=true;
    check(!p::WindowWriter::placement_context_matches(initial,wrong_after,destination.proof,displays,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "own move does not authorize a simultaneous state change");
    check(!p::WindowWriter::prepare_destination(initial,cross_native,nullptr,DPI_AWARENESS_PER_MONITOR_AWARE,displays).accepted,
        "off-topology native destination is rejected before issue");
    check(!p::WindowWriter::prepare_destination(initial,cross_native,left.monitor,DPI_AWARENESS_INVALID,displays).accepted,
        "unknown window awareness cannot authorize a cross-display DPI change");
    const auto same=p::WindowWriter::prepare_destination(initial,initial.positioning,initial.monitor,DPI_AWARENESS_INVALID,{});
    check(same.accepted&&!same.proof,"same-monitor quantum needs no display probe or transition exception");
    auto mixed=displays;mixed[1].dpi=144;
    destination=p::WindowWriter::prepare_destination(initial,cross_native,left.monitor,DPI_AWARENESS_PER_MONITOR_AWARE,mixed);
    after.dpi=144;
    check(destination.accepted&&p::WindowWriter::placement_context_matches(initial,after,destination.proof,mixed,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "PM window expected DPI uses the explicitly measured destination display");
    destination=p::WindowWriter::prepare_destination(initial,cross_native,left.monitor,DPI_AWARENESS_SYSTEM_AWARE,mixed);
    after.dpi=96;
    check(destination.accepted&&destination.proof->window_dpi==96&&p::WindowWriter::placement_context_matches(initial,after,destination.proof,mixed,DPI_AWARENESS_SYSTEM_AWARE,context_reason),
        "system-aware window retains actual window DPI rather than monitor DPI");
    check(!p::WindowWriter::placement_context_matches(initial,after,destination.proof,mixed,DPI_AWARENESS_PER_MONITOR_AWARE,context_reason),
        "window awareness change after the native call is rejected");
    check(p::WindowWriter::prepare(after,after,{-980,120,-480,520}).accepted,
        "next quantum uses confirmed actual geometry/context without changing original intent anchors");
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
