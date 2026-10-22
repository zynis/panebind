# Source Provenance Register

## V1 readable drag-alpha limitation — 2026-10-22

Re-read this repository's exact-edge feedback review and the pinned AltSnap
reference-only hollow-preview / FancyZones overlay-ownership and teardown
history (same pins below, including PR48473/48569). Reuse PaneBind's existing
nonactivating hit-transparent preview window rather than adding another input
route or a copied upstream notification implementation. No external code was
copied, adapted or translated; existing license/attribution decisions stand.

Actually rechecked Microsoft's [GetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlayeredwindowattributes),
[extended styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
and [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos).
Unreadable original per-window alpha is not permission to overwrite it;
nonactivation style/NOACTIVATE and text properties do not prove actual visible
pixels, foreground preservation or input transparency. The product therefore
keeps a generation-bound limitation distinct from write authority, uses the
existing small system-colour caption, and clears it with actual termination.
Pending/unissued mutations are not reported as completed native failures.

The `6274` guest really uploaded an opaque per-pixel surface and read unknown
constant alpha, but failed the TEST's combined geometry/foreground preparation
before product input. Its pre-layered DWM margins must not become a new false
oracle: the corrected fixture separately checks unchanged HWND positioning,
actual post-layering visible bounds, exact foreground/hit, and freezes that
baseline before DOWN. The raw failure remains FAIL; no product fallback or
visible-hint native PASS is inferred. New tests require a real Move/UP plus
actual nonactivating hint geometry, text, screen-region BMP and termination
hide. BMP creation is not by itself a readability or human-UAT verdict.

The same existing caption also supplies an idle notice: no operation/gesture
is active, NOACTIVATE positioning preserves the foreground, and one serial-
checked four-second timer hides it without becoming a polling input source.
Busy notices are explicitly not shown/queued; a new gesture retires the old
notice. The public no-history Undo is the real rejection used for guest
window/pixel/expiry and subsequent-gesture validation, still pending at design.

For the remaining explicit session/queue fault contract, reused the existing
pinned owner-dispatch/termination research above and actually checked
[WM_COPYDATA](https://learn.microsoft.com/en-us/windows/win32/dataxchg/wm-copydata),
[PostMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew)
and [WM_WTSSESSION_CHANGE](https://learn.microsoft.com/en-us/windows/win32/termserv/wm-wtssession-change).
Borrowed COPYDATA bytes must be copied; deferred processing does not inherit
native completion or authority from message delivery. A narrow guest Run/
source/generation-bound one-shot fault enters the existing push failure branch
only after real Move progress, labelled injected rather than a naturally full
queue or a silently removed OS hook. TEST session notifications carry the
actual session ID but do not prove actual OS lock/unlock. Neither test fabricates
UP, changes host security/queue quotas, or replaces real recovery observations.

## V1 native-arranged restore and unsupported drag alpha — 2026-10-22

Actually re-read the existing AltSnap review's native-restore section at the
same GPL reference-only pin recorded below, and [issue7](https://github.com/RamonUnch/AltSnap/issues/7).
Its current-vs-normal-placement heuristic is not adopted. Re-read the existing
FancyZones review's owner-thread event handoff and keyboard snap/restore test
history at the same recorded MIT pin; no new upstream implementation or code
reuse is claimed. Native OS arrangement and product-owned layout history are
different facts, as the actual `8b55` native-WinLeft failure demonstrates.

Actually checked Microsoft's [IsWindowArranged](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindowarranged),
[GetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowplacement)
and [WINDOWPLACEMENT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement).
The documented arranged state may coexist with SW_SHOWNORMAL, but not minimized
or maximized. The page directs dynamic lookup in User32 and carries a
prerelease caveat; absent lookup/contradictory observations remain unknown,
not evidence of ordinary or arranged state. Workspace normal-position data
must not be substituted for screen geometry. Reuse the existing bounded
restore request, real normal-state observation and original-DOWN intent;
new platform/state behavior is not empirically PASS from these documents.

For the two remaining drag-alpha scopes, actually checked [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
and [Window Class Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles).
WS_EX_LAYERED is not supported for CS_OWNDC/CS_CLASSDC. The new guest-only alias
is registered as an exact new self-owned class, not by altering another class.
Documentation does not establish a particular API error/result or safe product
fallback: actual native receipts, unchanged attributes, Move/UP and visible
limitation feedback still need observation. No GPL/MIT source copied, adapted
or translated; no new external code or attribution obligation. Applicable
subsystems: production native-restore state, drag visual ownership/feedback,
and only the exact disposable-guest tests for those existing V1 contracts.

The actual `e1a2` CS_OWNDC source nevertheless accepted the candidate's alpha
mutation and restored it after Move/UP; that recorded positive native result
is not overwritten by the documentation or relabelled an unsupported failure.
For the remaining real unreadable-alpha case, actually checked Microsoft's
[GetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlayeredwindowattributes)
remarks and [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow).
The former explicitly excludes attributes set by the latter. The guest fixture
therefore uses one ordinary self-owned per-pixel opaque surface, with bounded
self-owned GDI resources, and requires actual upload, unreadable attributes,
geometry and hit observations before testing the product fallback. It never
fills a failed API result or unknown alpha tuple, nor mistakes a notice log
for an actually visible limitation. This test's runtime outcome remains pending
until its new run; the earlier expectation-of-failure remains historical FAIL.

## V1 maximized-entry repair and one Lower contrast — 2026-10-22

Reused the existing mature/reference-only AltSnap GPL-3.0-or-later pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d` and FancyZones MIT pin
`19c4d805321db86f3634e6968e14dbf25cbba14a` reviews and their recorded history.
Actually re-read `ALTSNAP_REVIEW.md` and
[AltSnap issue681](https://github.com/RamonUnch/AltSnap/issues/681): visible
application geometry and unfiltered native window order are different facts.
The pinned hooks.c GitHub HTML opened, but its Lower implementation was not
available in the parsed view; no new claim of reading/adapting ActionLower.
No upstream code copied, translated or adapted; no new attribution obligation.

Official Microsoft references actually rechecked: [IsZoomed](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iszoomed),
[SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture),
[ShowWindowAsync](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindowasync),
[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos),
[GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow)
and [Window Features](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features).
IsZoomed is a state observation, not an identity test; ShowWindowAsync starts
work rather than proving normal-state completion. The existing consumed-DOWN
path must preserve its exact callback/UP/foreground/capture checks while making
the already-designed plain maximized restoration reachable. Native-first and
historical default requests continue to reject maximized entry.

The `9197` actual consumed maximized DOWN was rejected before shield admission;
the unchanged `11ae` failure remains under its original identity. The separate
`4362` Lower API actually completed but raw-next was non-null. Unfiltered Z-order
can contain hidden/system windows; neither that handle nor a later observation
proves the earlier semantic outcome. The single `761a` exact-owned passive
WINDOWPOS/bounded-tail contrast actually observed source -> current exact
GetShellWindow -> raw NULL with reciprocal links. It also showed that delivered
WINDOWPOS insertAfter need not retain the API's HWND_BOTTOM pseudo-handle.
Its original overall FAIL remains. The production correction only accepts two
stable finite observations of raw NULL or that immediate exact Shell terminal;
it skips no hidden/system/ordinary node and makes no global atomicity claim.
This is not a new z-order writer, permission grant or empirical PASS for the
new candidate. Applicable subsystems: V1 input adapter,
shared shield admission, restore integration and window-command postverification.

## V1 native application initialization contrast — 2026-10-21

Actual `fe0e2b7191134dc7a3dad634b8b0c983` ran with the PaneBind product
not started: Terminal's screen showed launch status `0xd0000022`, its exact
root closed, while Edge's click focus succeeded without the offline page's
DOM effect and its normal root close was not confirmed. Later `dfdd` paused
enhancements but retained hooks; it is not a product-absent control. Neither
record proves a unique system cause. Existing Terminal drag-band/issue8795 /
PR11680 research is reused; no new movement or product input mechanism enters.

Inspected mature official Microsoft Terminal tag `v1.25.2733.0`:
[ConptyConnection.cpp](https://raw.githubusercontent.com/microsoft/terminal/v1.25.2733.0/src/cascadia/TerminalConnection/ConptyConnection.cpp),
[winconpty.cpp](https://raw.githubusercontent.com/microsoft/terminal/v1.25.2733.0/src/winconpty/winconpty.cpp)
and [LICENSE](https://raw.githubusercontent.com/microsoft/terminal/v1.25.2733.0/LICENSE)
(MIT). The connection creates a pseudoconsole before launching the client; its
catch formats failures from multiple initialization stages into the same launch
message. The displayed status alone does not identify the failing API or prove
that cmd ran. A direct no-console cmd receipt therefore distinguishes a narrower
launch path, not a strict all-variables-equal Terminal or pseudoconsole control.
No upstream implementation copied, adapted, translated or redistributed; no new
dependency, license or attribution obligation. A failed attempt to fetch another
source file is not counted as an inspected source.

Rechecked official live [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
[creation flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags),
[GetWindowTextW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowtextw)
and [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw).
The child uses the caller's security context and an explicitly supplied working
directory; `CREATE_NO_WINDOW` supplies no console handle. A cross-process caption
read is not remote edit-control text. A zero timed-message result with unchanged
zero LastError is generic failure, not proof of a specific timeout cause. The
minimal TEST-only addition retains the actual API results, child SID/session /
Medium observations and a new RunId receipt. Edge must first expose the known
local page title before an actual client click is attempted. It changes no Edge
startup/security switches, product code, token, environment or host settings.
Native initialization/client/close results still require the guarded guest run;
static wiring checks and a shell receipt do not award an application PASS.

## V1 finite guest schedule: timer signal is not a QPC deadline — 2026-10-17

Actual `e7e07758db0e4a3a9f3f32be28aa0a8c` completed 20 mixed action slots,
but its final timer returned before the recorded QPC target: row3275 target
10194506397, row3276 end-resource sample10194431804 at frequency10000000.
The original I11 elapsed check failed; it is not relabelled as 900 seconds.
The old TEST scheduler did not recheck QPC after a timer signal. This is the
observed clock discrepancy, not proof of a particular hypervisor or OS defect.

Inspected official [SetWaitableTimer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer)
and [QueryPerformanceCounter](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter):
timer precision depends on the hardware clock, period0 is one-shot and setting
a manual-reset timer resets its signaled state. An interval measured in QPC
must be checked with QPC, rather than inferred from another clock's signal.
The minimal TEST change retains event waits and STA dispatch, permits only one
bounded rearm for at most32ms measured early return (additional wait at most64ms),
then requires a real fresh QPC at/after the original target. It does not alter
the strict 900-second assertion, product timers, input handling or any authority.
Official live documentation; no invented revision, external code copied or
adapted, dependency, licensing or attribution change. New runtime is pending.

## V1 guest evidence reader: handle-scoped rotating log snapshot — 2026-10-17

Actual Release Run `f54dccc34fbe47ad9fa296c11bb11880` reached two owned core
rounds in one product process, then driver row4524 recorded ordinary attributes
followed by `filesystem::file_size` Windows/system error5 for the `.1` log.
The next row aborted I11; this is not a completed 900-second observation.
The precise origin of that access denial remains UNKNOWN. No permission
error is reclassified as proven rotation, skipped or accepted as evidence.

Inspected Microsoft's official [CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew),
[GetFileSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex)
and [ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)
contracts: shared-delete access permits rename/delete while a reader holds its
handle; OPEN_REPARSE_POINT allows checking the opened object without following
a reparse point; size and bytes can be obtained from that same opened handle.
Official live documentation is not pinned to an invented source revision.
The minimal TEST reader replaces separate path attributes/size/ifstream opens
with one bounded, validated handle. It keeps the 2MiB bound, exact sequence
and launch identity checks, two pair snapshots and real failure verdicts.
It changes no product logging, input route, geometry or authority. Host offline
file tests are file-I/O evidence only, not guest interaction or product PASS.
Independently authored; no external implementation copied/adapted, no new
dependency, licensing or attribution obligation.

## V1 application follow-up: real drag band and Chromium startup failure — 2026-10-17

Run `1acb8c6bd6184c5bbf615497bf6bb57a` retained the actual Terminal screen
(`2081C3B55CEC30362CD48BA99AA4AB7B1440A738E7FFB4988EB971530630F5A3`)
and child inventory: the old point `[153,172]` was outside its actual
`DRAG_BAR_WINDOW_CLASS` band `[632,151,1048,211)`. The existing fixed Terminal
revision / issue8795 / PR11680 review below already establishes that an external
root HTCAPTION does not prove native dragging. Reused that evidence; reread
`IslandWindow.cpp` at `dc8ae365847d41f62d1ebca93815bb00959bc12f` (MIT,
reference only) and Microsoft's [FindWindowExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-findwindowexw)
contract (direct children, not recursive). A TEST-only point candidate may use
one unique real child band, excluding actual DWM buttons. Actual hit, source
identity, foreground, native START and all product authority gates remain.
The screen also shows cmd startup `0xd0000022`; moving its frame would not
prove that Terminal's shell/client works. New point selection is pending runtime.

The same run's Code log actually reports GPU child launch error49 and a fatal
unusable GPU process, even with the official `--disable-gpu` option. Inspected
the official version chain: [VS Code1.142.0 .npmrc](https://raw.githubusercontent.com/microsoft/vscode/1.142.0/.npmrc)
pins Electron43.7.7; [Electron DEPS](https://raw.githubusercontent.com/electron/electron/v43.7.7/DEPS)
pins Chromium150.0.7871.250. At that tag,
[sandbox_types.h](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/sandbox/win/src/sandbox_types.h)
defines49 as `SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN`; [sandbox_policy_base.cc](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/sandbox/win/src/sandbox_policy_base.cc)
returns it when preparing the restricted primary token fails. Also inspected
that tag's GPU host/mode handling and Electron's `DisableHardwareAcceleration`:
software rendering does not guarantee absence of a GPU/Viz child sandbox.
The lower-level failed API/errno remains UNKNOWN; do not identify this as a
Win32 error49, hardware-driver defect, UIPI or a PaneBind input failure.
No evidence-backed additional software flag within the current security
boundaries was found. Do not disable sandboxing or alter token/ACL/system
configuration to manufacture an application PASS.

Upstream classification: mature official production projects. VS Code/Electron
source MIT, Chromium BSD-style with its source LICENSE; no code copied/adapted,
translated or redistributed into PaneBind, no new dependency/attribution due.
Official live API documentation has no immutable revision claim. Historical
failures keep their original product/driver identity; Edge's actual blank pixels
do not prove the same startup cause as Code.

## MVP1 guest command-only contrast — 2026-10-12

Scope: only the guest entry's console dependency becomes a single RunId/configuration
named pipe; no movement, eligibility, foreground, capture, shield or writer design
is changed. Independently authored; external code copied/adapted **NO/NO**.

- AltSnap / mature maintained / fixed `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`,
  GPL-3.0-or-later, reference-only: re-read `hooks.c` 694–737 worker dispatch and
  `License.txt`; PR609 history commit `7f4afe59076b70980f71af202f63609ca3ac5745`
  patch inspected. A filtered worker is not a Shell STA template; none reused.
- PowerToys/FancyZones / mature production / fixed
  `19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT, reference-only: re-read
  `FancyZones/FancyZonesApp.cpp` routing and root `LICENSE`; PR48569/
  `dd26d86580168d2e368701f7b0c4d629dc9cd9ac` destroy/abort diff re-inspected.
  Invalidation/abort is not successful completion. No implementation reused.
- Microsoft Learn / official live documentation / read 2026-10-12 / Learn terms,
  no immutable revision claimed: [CreateNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea),
  [ConnectNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-connectnamedpipe),
  [GetNamedPipeClientProcessId](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeclientprocessid),
  [CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex),
  [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex),
  [creation flags](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags).
  Facts: local-only first-instance overlapped pipe, exact live PID/SID/session;
  connected-before-Connect race; cancel request is not completion; keep pending
  OVERLAPPED storage alive; event wait retains owner STA pumping.
  CREATE_NO_WINDOW is not SW_HIDE and cannot combine with NEW_CONSOLE/DETACHED.
  No samples copied/adapted; research links retained, no code attribution due.

The existing [STA research](R1C4A_STA_CONSOLE_WAIT.md) informs the owner pump,
not pipe equivalence. Private no-input IPC tests do not prove Move/UP/cleanup
or human UAT. A passing contrast cannot identify the historical UNKNOWN
ConsoleWindowClass owner or establish a system root cause.

## MVP1 guest 自动驱动的自有 console 对照 — 2026-10-06

实际查阅 Microsoft Learn [GetConsoleWindow](https://learn.microsoft.com/en-us/windows/console/getconsolewindow)、
[GetConsoleProcessList](https://learn.microsoft.com/en-us/windows/console/getconsoleprocesslist)、
[ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)。
Official primary documentation，live pages 无 immutable commit；site terms、仅链接/释义，
code copied/adapted NO，issues/PRs N/A。GetConsoleWindow 属于调用进程当前关联 console，
pseudoconsole HWND 不等于可见终端；进程列表返回不足容量时不能把缓冲区当完整归属证明。
ShowWindow 返回旧可见状态，不是成功布尔；隐藏/非激活恢复都须真实回读。
用途仅 test-created guest driver/product 已确属的独立 console 对照，不是产品抢前台或
放宽 source/capture 权限，也不控制未知 console 或宿主窗口。复用本轮既有 AltSnap /
FancyZones reference-only 研究，不新增或派生第三方产品代码。

追加读取 [STARTUPINFOW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-startupinfow)：
STARTF_USESHOWWINDOW 才使 wShowWindow 生效，console 新进程的外观由创建参数约束。
同上述日期/官方来源/许可与 no-copy 分类；仅 test driver 传自己的新进程创建参数，
不是发现或控制另一个 ConsoleWindowClass HWND 的依据。实际对照发现查询句柄与
失败 foreground 不同，保留该未知，不宣称 pseudoconsole 根因已经证明。

## MVP1 exact three-root High guest test exception — 2026-10-06

BASE `2bc40fe33c12a9110e27ff9a1c930804ddcf3323`。本轮用户明确批准仅专用
可丢弃 Sandbox、同 SID / 非零交互 session / exact High 的三个分别确认新
Explorer root；普通 Medium/non-elevated 合同不改。不改账户、token、UAC
或宿主设置。既有 8af40e... 实际 High/elevated/no-linked 证据及九项 owned
正常 UP / 独立清理证据保留，不外推为跨进程结论。

本轮是上述权限范围传递，不更换 input/capture/window movement 机制。
复用已核验 AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`
（成熟维护，GPL-3.0-or-later，仅参考）movement/finish/state/history，以及
FancyZones `19c4d805321db86f3634e6968e14dbf25cbba14a`（成熟生产，MIT，仅
参考）窗口 eligibility、Abort/End、PR48569/history dd26d865 研究；不声称
本轮重新检查全部上游或继承其测试结果。项目链接和模块/history 见下节。
未检查新的外部项目，code copied/adapted/translated/derived NO。

实际补读 official live primary contracts（Microsoft site terms、链接/转述，
无代码示例复用、无 immutable revision）：
[Sandbox LogonCommand](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file)、
[TOKEN_INFORMATION_CLASS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-token_information_class)、
[GetTokenInformation](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation)。
SID、session、integrity、elevation 是分开的真实 token 查询事实；账户名或
管理员身份不能替代这些值。例外附着 exact consent session/RunId/根窗/确认
及 token 代际，每次 native validation 重新读取原事实，不把 medium 改 true。
只读查询不调整权限。guest 保护是防误运行而非防伪安全边界。

同轮真实 RunId `73e4aeb0f5f746d3bc8f05f1390ba6d0` 发现 Browser sink 把
DISPID108 计为 malformed（2次），其余订阅/导航/身份/队列事实正常。
最初怀疑 CommandStateChange，并实际补读其官方页 cc136546，但 SDK 数值
核对否定此猜测（未提交/部署）。随后实际补读
[ProgressChange](https://learn.microsoft.com/en-us/previous-versions/cc136550(v=vs.85))：
DISPID_PROGRESSCHANGE，两个输入 Long Progress / ProgressMax，Progress 可为-1。
仅补实际108通知的严格形状接收，保留其他未知/错误形状拒绝；不证明导航
或窗口权限，不推进导航 epoch / callback 身份凭据。Microsoft site terms，
review2026-10-06，live contract，无外部代码复制/改编。

同轮标题栏 query 对照实读
[WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)、
[WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint)、
[GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)。
WM_NCHITTEST 使用屏幕坐标，对实际指定窗口查询；返回 HTCLIENT 不能直接
当作 HTCAPTION。WindowFromPoint 及实际 root/PID/TID 回读只用于区分 child
查询对象，不授予窗口写入权限；不能从空白截图推断未观察的 native Move。
Microsoft live primary docs，review2026-10-06，无外部代码复用。

本轮复核成员实际追加 inspected（不是 PaneBind 平台观察）：AltSnap 同 pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d` 的 hooks.c590–616/5190–5226、
unfuck.h1538–1556，history `9fe1e8c44e8081b3a13650eeca2d51a25f56b4aa`
caption-button 误归因防御；GPL-3.0-or-later reference-only。FancyZones 同 pin
`19c4d805321db86f3634e6968e14dbf25cbba14a` 完整 WindowMouseSnap.cpp/LICENSE，
MIT reference-only，cursor-size heuristic 不是本候选 DOWN/root HTCAPTION 证明。
教训仅为区分实际 point HWND 与身份 root，不复制上游回退或策略代码。
code copied/adapted/derived NO；保留已有 repository 链接，无新增代码 attribution。

同轮实际 Resize Run `674b726caa6d4b9da0e50aefef9fbd29` 进一步观察到
DISPID105/CommandStateChange 两次被旧sink拒绝，其他身份/导航/队列事实正常。
采用已实读 [CommandStateChange](https://learn.microsoft.com/en-us/previous-versions/cc136546(v=vs.85))
Long Command/VARIANT_BOOL Enable 合同，按实际SDK105核验后只接此通知。
这是具体实测缺口修复；没有补未观察的其他DISPID或宽泛忽略未知事件。
Microsoft live primary，review2026-10-06，无外部代码复用。

## MVP1 post-END input-queue association — 2026-10-04

BASE `0e7a4a8240e2317381ef772674c7d31a47a9b81e`。本轮人类明确授权仅在
可丢弃 guest，把本代 shield 资源线程关联至已核实 source HWND 的实际线程。
上一轮未关联的失败证据和许可判断保持原样，不追溯改为 PASS。

复用已核验 AltSnap pin `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`
（mature maintained，GPL-3.0-or-later，reference-only）的既有 movement/
STATE_UP/finish/history 研究；本轮 GitHub hooks.c 页面实际可读的前段包含 GPL
header 和状态定义，issue572 body 可读，034d58/8a5c history 网络页失败，
不声称重新读到这些 diff，也不外推其 hook 路径为本候选证明。
随后研究复核通过只读 raw 请求实读同 pin `hooks.c` 的 FinishMovementNow
5239–5310、`License.txt` GPLv3 opening，以及上述 034d58/8a5c 的完整 `.patch`。
仅在 `hooks.c` / `unfuck.h` / `altsnap.c` 中搜索 AttachThreadInput、SetCapture、
ReleaseCapture 未命中；不宣称全库无使用，也没有复制其实现。
实读 PowerToys/FancyZones pin `19c4d805321db86f3634e6968e14dbf25cbba14a`
（mature production，MIT，reference-only）LICENSE、完整 WindowMouseSnap.cpp
和 history `dd26d86580168d2e368701f7b0c4d629dc9cd9ac` 的 rendered diff：
Abort 只清资源，不当成成功 End placement；保留失效退出，不继承其现场结论。
Repository links: [AltSnap](https://github.com/RamonUnch/AltSnap),
[PowerToys](https://github.com/microsoft/PowerToys)。所有 upstream code copied /
adapted / translated / derived: NO；没有新增代码 attribution，保留研究链接。

实际读取 official primary live pages（无 immutable 文档版本，Microsoft site
terms，链接/转述，无示例代码复用）：
[AttachThreadInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput)、
[SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)、
[ReleaseCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-releasecapture)、
[GetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getthreaddesktop)、
[GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo)，
以及 Microsoft Raymond Chen 的
[input queue association hang warning](https://devblogs.microsoft.com/oldnewthing/20080801-00/?p=21393/)。
关联要求有队列、同 desktop；共享 focus/key/input ordering，相关键状态会重置。
键状态改变不是 Raw/legacy UP。资料不保证 source 停泵时 release/detach 独立
完成，必须在 own source 有界停泵的 guest 场景检验。
同时实读 [GetUserObjectInformationW UOI_IO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectinformationw)：
对具名 resource/source desktop 查询实际 input-desktop 属性，并核对同非零 session、
真实 source foreground；不只依赖两个 desktop 恰巧同名 Default。

设计只改这一主要变量：真实 END 后关联，保留到真实 UP 或 abort；自有 release
→ exact-pair detach → 撤罩。只解除本代真正建立的关联，失败不进入下一代。
本轮验证前状态为 IMPLEMENTED 候选 / runtime UNKNOWN，不宣称清理或功能 PASS。

owned 现场 Gate 通过后的 Explorer driver 桌面修正，实际补读 Microsoft Learn
[WTSQuerySessionInformationW](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsquerysessioninformationw)、
[WTS_CONNECTSTATE_CLASS](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ne-wtsapi32-wts_connectstate_class)、
[WTSGetActiveConsoleSessionId](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-wtsgetactiveconsolesessionid)。
适用仅 guest test driver：当前会话 WTSActive 与实际 input desktop 检查，不能
把物理 console session 等同所有可交互 guest 会话；未知仍拒绝。官方 live 页、
同日 review、无代码示例复制/适配；没有修改系统会话/权限/远程配置。

本轮 owned 连续手势的实际反例又触发对既有
[GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)
和 [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input) 的 scoped
官方补读（同日 live pages，无代码复用）：合法 post-handoff Raw event 作为
驱动信号，owner 处理时只读一次当前 cursor/当前 QPC；receiver 历史 cursor
单独保留，不声称它与具体合成输入存在因果一一对应。缺当前 cursor 或权限事实
仍拒绝，不轮询、不更换 DOWN/窗口锚点、不增加输入来源或产品 SendInput。

Explorer guest 的实际 `Elevated` 拒绝及 driver 192 DPI 坐标反例之后，同日补读
Microsoft Learn [Sandbox LogonCommand/account contract](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file)、
[TOKEN_INFORMATION_CLASS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-token_information_class)、
[GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)、
[SetThreadDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext)。
Official live primary pages，无 immutable revision；Microsoft site terms，链接/转述，
sample/code copied/adapted NO。适用范围仅 guest 启动身份只读诊断与 test-driver
实际线程 PMV2；未检查新的开源项目。Sandbox 默认 WDAGUtilityAccount 的管理员
身份不能证明具体 token；只查询 token，不改变它或 UAC/账户。WindowRect 可被
DPI 虚拟化，DWM bounds 不作同样调整；用 actual caller context 防止混用，而非
修改显示比例或扩大跨屏/混合 DPI 功能。产品 medium/non-elevated Gate 不放宽。

2026-10-03 NormalUp 两种唯一 UP 形式均失败后，只读补查
[AttachThreadInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput)
（live official page）。它共享 input/focus/key states，重置 GetKeyState/
GetKeyboardState 相关状态，要求两线程都有消息队列且同 desktop。
仅列为待人类新增范围决策的 guest 候选；未调用、实现、复用代码或实测，
不将该共享影响推断为当前 SetCapture 许可，不声明它能解决 legacy UP。

## MVP1 post-END self capture — 2026-10-03

适用：guest-only GestureInputShield capture 生命周期及 legacy UP 收尾。
复用既有 AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`
（GPL-3.0-or-later、成熟维护、reference-only）源模块与 history 研究，不声称
本轮重新读取其全库。重新实读 FancyZones `WindowMouseSnap.cpp`，pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`（MIT、成熟生产、reference-only）
及 history `dd26d86580168d2e368701f7b0c4d629dc9cd9ac` 的 abort/资源退出边界；
它不是 PaneBind capture 功能证明。AltSnap history 400eeb 网络页失败，不算新实读。
没有复制、适配、翻译或派生 upstream code。

实读 Microsoft Learn [SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)、
[GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)、
[ReleaseCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-releasecapture)、
[WM_CAPTURECHANGED](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-capturechanged)。
Live official pages，无 immutable revision、无示例复用。原始返回是前 capture
HWND；GetCapture NULL 只代表本线程；背景 capture 的可见区域及键盘加速键限制
需 guest 实测；自己 ReleaseCapture 也产生 capturechanged，禁止在该消息中抢回。
保留链接，无新增代码 attribution 义务。授权不等于经验 PASS。

第一批 capture 回读成功而 legacy UP 缺失后，补读
[MOUSEINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput)
及 [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)。
dwFlags 可组合移动与一次按钮状态转换；测试 driver 对当前同坐标只发一次组合
MOVE/ABSOLUTE/VIRTUALDESK/LEFTUP，不补第二次 UP。API 插入成功不等于 legacy
投递，必须继续观察真实 Raw/legacy 消息。产品仍完全不依赖 SendInput。

## R1-C4B Architecture Pivot 1 Fix B — 2026-09-26

Starting HEAD: `e381b998d9e3eded051fbc859893633e8633ebee`.
Applicable subsystem: true native END barrier, original-intent anchors,
source-only owned handoff, checked frame bridge and exact readback. See
[Fix B END-barrier research](R1C4B_PIVOT1_FIXB_END_BARRIER.md).
Old Fix A records/logs/verdicts are preserved under their old return-baseline
contract; Fix B does not accept them retroactively as a takeover result.

Official Microsoft Learn primary live pages actually read in Fix B; no
immutable document revision asserted; Microsoft site terms; links/paraphrases
only, no platform examples copied/adapted:

- [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
- [WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)
- [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
- [Event Constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
- [DwmGetWindowAttribute](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute)
- [DWMWINDOWATTRIBUTE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)
- [GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)
- [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)

Lessons: send-handler completion is not modal-loop END; actual owned
WM_EXITSIZEMOVE and future foreign EVENT_SYSTEM_MOVESIZEEND are distinct witness
paths. User-authorized pre-END terminal settlement does not reset the intent
anchor. Current Raw-triggered cursor is not packet-time trajectory. P/V are
different coordinate domains; the official DWM query contract does not publish
a synchronous SetWindowPos visibility guarantee or a PaneBind retry policy.

Existing local prior-art/provenance/history records were read and reused,
not new upstream inspections: AltSnap mature/GPL-3.0-or-later reference-only
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; FancyZones mature/MIT reference-only
`19c4d805321db86f3634e6968e14dbf25cbba14a`, exact modules/history and retrieval
limits remain the Pivot 1 record. PaneBind's own checked frame arithmetic,
postverify diagnostic/classifier/tests and Explorer bridge were inspected;
no magnet_postverify_policy.* existed at this starting baseline, so no false
async-policy inspection/reuse claim is made. No external project added.
Code copied/adapted/translated: NO/NO/NO. Attribution required: retain research
citations; no new code obligation. Research PASS applies only to the independent
strict-exact owned probe, not product readiness, native behavior, full repetition,
Explorer authority or subjective flicker. WH_MOUSE_LL remains research-only.

Fix A additional read-only environment diagnosis, 2026-09-26: Microsoft Learn
[WTSINFOEX_LEVEL1_W](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ns-wtsapi32-wtsinfoex_level1_w)
and [WTSINFOEXW](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ns-wtsapi32-wtsinfoexw)
were actually read. Live official pages, Microsoft site terms; no samples copied
or adapted. SessionState Active does not imply unlocked; on this Windows
10.0.26200 host SessionFlags 0 denotes LOCK, 1 UNLOCK. The documented reversed
Windows 7/Server 2008 R2 behavior is not applied to this host. Applicable scope:
test-only desktop safety prerequisite, not input/cancel architecture evidence.

## R1-C4B Architecture Pivot 1 Fix A — 2026-09-26

Starting HEAD: `94242959568b28d782d1a89a6f78c95c2dd8b098`.
Scope: remove an unsupported owned-test local-activation prerequisite; preserve
original logs and strict global source / receiver background evidence. Source,
interpretation and superseded design are recorded in
[Pivot 1 input contracts](R1C4B_PIVOT1_INPUT_CONTRACTS.md).

Microsoft Learn official primary live pages actually re-read in Fix A; no
immutable document revision asserted; Microsoft Learn terms, paraphrase and
links only, no samples copied/adapted:

- [GetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getactivewindow)
- [GetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getforegroundwindow)
- [SetActiveWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setactivewindow)
- [SetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus)
- [GetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getfocus)
- [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices)
- [RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice)
- [Raw Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input)

Lessons / applicable subsystem: GetActiveWindow/GetFocus are calling-queue
diagnostics, GetForegroundWindow is the global foreground fact;
SetActiveWindow(NULL) has no documented clear-active contract; documented
SetFocus(NULL) is irrelevant to INPUTSINK eligibility. Exact registered separate
receiver PID, foreground source identity, actual RIM_INPUTSINK movement and UP,
input correlation and final fences establish owned-test background evidence.
Fresh activation/focus callbacks are diagnostic rather than a second mandatory
foreground proof. No local-state reset or foreground-policy bypass is retained.

The prior-art source/license/history gate is reused from the original Pivot 1
record below and [its focused research](R1C4B_PIVOT1_PRIOR_ART.md); that local
record was read in Fix A. This is **not a new upstream source/history inspection**
claim. AltSnap remains GPL-3.0-or-later reference-only at
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; FancyZones remains MIT reference-only
at `19c4d805321db86f3634e6968e14dbf25cbba14a`, with exact inspected modules/history
and retrieval limitations in the original records. No new external project,
GPL-derived code, translated control flow or MIT sample code enters PaneBind.
WH_MOUSE_LL stays the existing technically possible research-only candidate,
not a new implementation. Code copied/adapted/translated: NO/NO/NO.
Attribution required: research citations retained, no new code obligation.
Contract-research PASS permits only a test-owned probe; cancellation/takeover
and product PASS require independent new original evidence. The three original
BLOCKED logs are immutable and are not accepted retroactively.

## R1-C4B Architecture Pivot 1 — 2026-09-26

Base: `070b05a8f44c8b08503b3643f838a83703495987`. Scope, exact links and
inspection limits: [Pivot 1 prior art](R1C4B_PIVOT1_PRIOR_ART.md) and
[Pivot 1 input contracts](R1C4B_PIVOT1_INPUT_CONTRACTS.md). All external sources are reference-only;
code copied/adapted/translated NO/NO/NO. No new code attribution obligation;
research citations retained. This is a source/history/contract review, not an
upstream behavior run or a PaneBind cancellation/takeover acceptance result.

AltSnap, https://github.com/RamonUnch/AltSnap, mature maintained,
GPL-3.0-or-later **REFERENCE ONLY**, locally verified exact pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: License.txt, hooks.c GPL header,
WorkerThread, LetWindowKickBack, MoveResizeWindowNow_, LowLevelMouseProc,
HookMouse and NO_HOOK_LL polling branch. Read issue 572 and PR 609; actual local
commit diffs `7f4afe59076b70980f71af202f63609ca3ac5745`,
`df25d36c6369bb13aa02ec83974e625fc7922c35`,
`8a5c422928d37d34e63cb8b99d4a74e14cb955f0` (altsnap.c DLL lifetime).
Lessons: custom input/movement and native-loop concurrent correction differ;
coalesced worker movement can bound work; synthetic lifecycle can interact with
other managers; own-process callback DLL is not proof of foreign injection.
Its polling/Sleep paths and GPL implementation are not implementation templates.
Applicable subsystem: bounded input owner and cancel/takeover probe research.
No new AltDrag checkout/source inspection this round; historical comparisons
remain their original records, not a new Pivot 1 inspection claim.

PowerToys/FancyZones, https://github.com/microsoft/PowerToys, mature production,
MIT **REFERENCE ONLY**, immutable pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`: actual pinned WindowMouseSnap.cpp,
DraggingState.cpp and root LICENSE read through source URLs. Read PR 48569 and
actual rendered merge diff `dd26d86580168d2e368701f7b0c4d629dc9cd9ac`; PR 49985
and actual rendered metadata/test-guidance hunks of merge
`d68980a81bb8de144bdec998a114e948bf68c563` (not all product hunks).
Pinned FancyZones.cpp, KeyState.h and KeyboardInput.cpp requests returned cache
miss; these modules were **not re-inspected in Pivot 1**. Lessons: abort is not
a completed drop; destroy/invalidation and input delivery proof must survive
coalescing; drag-end zone Snap is not live cancel/takeover authority. Applicable
subsystem: probe lifecycle and reliable original evidence. No code copied or
adapted; future code reuse needs a separate license/provenance decision.

Microsoft Learn live official documents, no immutable revision asserted,
Microsoft site terms; facts paraphrased, no examples copied/adapted: Raw Input
Overview, RegisterRawInputDevices, RAWINPUTDEVICE, WM_INPUT, RAWMOUSE,
GetRawInputData, RAWINPUTHEADER, WM_CANCELMODE, WM_CAPTURECHANGED,
WM_EXITSIZEMOVE, GetCapture, GetGUIThreadInfo, SendMessageTimeoutW, GetCursorPos,
GetAsyncKeyState, SendInput, MOUSEINPUT, LowLevelMouseProc, Using Raw Input,
Window Features (message-only windows), HID Architecture (mouse TLC). Exact links and
bounded interpretation are in the two focused research records. Additional
Microsoft Old New Thing primary explanation read:
[2014-02-13 input queue](https://devblogs.microsoft.com/oldnewthing/20140213-00/?p=1773/).
Lessons: INPUTSINK supports background event delivery; raw coordinates are not
accelerated cursor coordinates; same-thread capture must not be confused with
foreign capture; cancel return value does not prove EXIT; WH_MOUSE_LL itself is
non-injected; SendInput and system Raw Input Thread do not establish a published
guarantee of application WM_INPUT. Applicable subsystem: original owned input
continuity/cancel observations and explicit fail-closed architecture gates.

Pivot 1 additional primary message-contract review (2026-09-26):
GetMessageW, SendMessageW and About Messages and Message Queues were actually
read for the bounded existing sample3 cancel-observation stimulus; exact URLs
and facts/inference boundary are recorded in R1C4B_PIVOT1_PROBE.md. No platform
sample code copied/adapted. These contracts do not prove a native loop's internal
implementation or cancellation success; the observation still needs real EXIT
and post-return no-drag/no-geometry-change evidence.

Pivot 1 owned-bootstrap review (2026-09-26): ShowWindow, GetActiveWindow,
GetFocus, SetActiveWindow, SetFocus and SetForegroundWindow official live pages
were actually read. Exact URLs and second attempt's unchanged BLOCKED evidence
are in the input-contract record. Lesson: calling-queue activation/focus and
global foreground are distinct. The historical one-time local reset and fresh
click-callback gate were withdrawn by Fix A: neither is an INPUTSINK prerequisite
or a second foreground-authority requirement.
No examples copied/adapted; no new attribution obligation. Old Fix 3 behavior
and evidence remain unchanged.

MsgWaitForMultipleObjects and MsgWaitForMultipleObjectsEx official live pages
were actually read for bounded UI/receiver message waits; exact URLs and the
old-message versus MWMO_INPUTAVAILABLE distinction are in the input-contract
record. No examples copied/adapted. Waiting on actual messages/events is not
evidence that the unexecuted cancellation or future writer already works.

## R1-C4B Fix 3 verified foreground test — 2026-09-26

Base: `167be83db73f62bf6cc0e0691ab88697fc08871c`. See
[Fix 3 focused research](R1C4B_FIX3_VERIFIED_FOREGROUND.md) for exact links,
inspection limits, contract interpretation and the test-only research gate.

AltSnap, https://github.com/RamonUnch/AltSnap, mature maintained,
GPL-3.0-or-later **REFERENCE ONLY**, exact locally verified pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: License.txt, hooks.c GPL header,
ReallySetForegroundWindow / SetForegroundWindowL, ActionLower topmost handling,
CreatePinWindow. Actual local history/diffs inspected:
`3c69ef9e650c8cf66031550183038980e7109d81`,
`865a51619975fe02fe9d13549d3e88abdcd6256f`,
`400eebf04dc651f76b2c1148c63fee7a4b039d8c`; merge
`bdb881019397195a09da9e00a844612253b67030` identifies PR 756. PR web discussion
was not retrievable; no discussion-review claim. Lessons: its synthetic Ctrl
foreground workaround is prohibited here; indicator visibility and activation
must be separated. No GPL implementation copied, translated or adapted.

PowerToys/FancyZones, https://github.com/microsoft/PowerToys, mature production,
MIT **REFERENCE ONLY**, exact pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`: actual pinned WindowMouseSnap.cpp
and root LICENSE read; PR 48569 and merge diff
`dd26d86580168d2e368701f7b0c4d629dc9cd9ac` read. Lessons: drag cleanup must not
steal focused-app input; destroy/abort is not successful END; highlight/end-zone
placement is not proof of live modal correction authority. Pinned WindowUtils,
ZonesOverlay and WorkArea retrieval failed with cache-miss and these modules
were **not re-inspected in Fix 3**. No code copied, translated or adapted; no new
code attribution obligation. Any future MIT reuse needs a separate decision.

Microsoft Learn live pages, no immutable revision, site terms, facts paraphrased:
SetForegroundWindow, SetWindowPos, GetClientRect, ClientToScreen, WindowFromPoint,
GetAncestor, WM_NCHITTEST, GetGUIThreadInfo, GUITHREADINFO, SendInput, MOUSEINPUT,
WM_MOUSEACTIVATE, WM_ACTIVATE and WM_SETFOCUS. Exact links are in the focused
research record. Applicable subsystem: isolated test activation input, fresh
identity/input-state proof, event-driven bounded confirmation and original
evidence. The research gate permits the explicitly authorized test-only click;
it does not assert observed success or product foreground authority. Product
SendInput dependency and foreground force remain NONE. Actual run outcomes are
reserved for [Fix 3 execution report](../reports/R1C4B_FIX3_EXECUTION_REPORT.md).

Fix 3 alternative-architecture follow-up (same review date/base), research only:
[A/B/C alternatives](R1C4B_FIX3_ALTERNATIVES.md). Re-inspected local AltSnap at
the same exact pin: hooks.c LetWindowKickBack, MoveResizeWindowNow_,
LowLevelMouseProc and WH_MOUSE_LL setup; actual history diff
`df25d36c6369bb13aa02ec83974e625fc7922c35`. The synthetic sizing/placement and
custom hook input pipeline are not authority over an ongoing native Explorer
drag; no GPL code copied or adapted. Reused this round's actually inspected
FancyZones pin/WindowMouseSnap/LICENSE/PR 48569/merge diff: end placement and
abort are separate; no claim of a new source inspection or empirical FZ run.
Additional Microsoft Learn live contracts actually read: WM_MOVING, WM_SIZING,
SetWindowSubclass, SetWindowLongPtrW, SetWindowsHookExW, Hooks Overview,
SendMessageW, WM_CANCELMODE, SetCapture, ReleaseCapture, WM_CAPTURECHANGED,
WM_EXITSIZEMOVE and Event Constants. SetWindowPos contract reused from this
round's official review. Facts paraphrased under site terms; exact links in the
alternatives record; no documentation sample copied/adapted. Applicable
subsystem: architecture feasibility and product-interaction decision only.
A requires recipient cooperation not established for Explorer under current
constraints; cancellation does not establish B input ownership; C is a possible
post-END model with different UX and requires an explicit product decision.
All alternatives remain unimplemented/unobserved; no new code attribution duty.

## R1-C4B Fix 2 self-driving test — 2026-09-16

AltSnap, https://github.com/RamonUnch/AltSnap, mature GPL-3.0-or-later,
REFERENCE ONLY, pin 5c86416ad21e4b72844a998a746bd3bb0bee5f5d: License.txt,
hooks.c LetWindowKickBack / MoveResizeWindowNow_, history
df25d36c6369bb13aa02ec83974e625fc7922c35 / PR 739. Lesson: synthesized
notifications and actual modal interaction are not interchangeable.
PowerToys/FancyZones, https://github.com/microsoft/PowerToys, mature MIT,
REFERENCE ONLY, pin 19c4d805321db86f3634e6968e14dbf25cbba14a: LICENSE,
WindowMouseSnap.cpp, PR 48569 / dd26d86580168d2e368701f7b0c4d629dc9cd9ac.
Lesson: lifecycle/abort must be explicit; prior art is not owned/Explorer proof.
No code copied, translated or adapted; no new code attribution obligation.
Microsoft Learn live documents read this date (site terms, no pinned revision):
SendInput, MOUSEINPUT, OpenInputDesktop, WTSINFOEX_LEVEL1_W, WM_NCHITTEST,
SetForegroundWindow, ShowWindow, SetWaitableTimer. Applicable subsystem:
test-only input, desktop/foreground safety and evidence; no product reuse.
Exact links and independent test contract:
[Fix 2 research](R1C4B_FIX2_SELF_DRIVING_PROBE.md).

## R1-C4B Fix 1 modal authority — 2026-09-16

See [modal authority research](R1C4B_FIX1_MODAL_AUTHORITY.md). AltSnap,
https://github.com/RamonUnch/AltSnap, mature GPL-3.0-or-later REFERENCE ONLY,
exact commit 5c86416ad21e4b72844a998a746bd3bb0bee5f5d: License.txt,
hooks.c LetWindowKickBack / MoveResizeWindowNow_, PR 739 and historical
df25d36c6369bb13aa02ec83974e625fc7922c35 inspected. Lesson: recipient message
result, ordinary placement and real modal-loop ownership are distinct.
PowerToys/FancyZones, https://github.com/microsoft/PowerToys, mature MIT,
exact commit 19c4d805321db86f3634e6968e14dbf25cbba14a: LICENSE,
WindowMouseSnap.cpp, PR 48569 / dd26d86580168d2e368701f7b0c4d629dc9cd9ac.
Lesson: abort is not completion; its zone interaction is not evidence of live
Explorer RECT authority. Both projects reference-only; no code copied,
translated or adapted, no new code attribution obligation.
Microsoft Learn live API documents (site terms, no immutable version claimed):
WM_MOVING, WM_SIZING, SetWindowPos, WM_WINDOWPOSCHANGING, GetWindowRect,
DwmGetWindowAttribute / DWMWINDOWATTRIBUTE, WM_ENTERSIZEMOVE,
WM_EXITSIZEMOVE and SendInput. Links/interpretation in the research record.
Applicable subsystem: diagnostic placement evidence and owned native modal
probe; no platform documentation samples copied/adapted into implementation.

## R1-C4B live integration — 2026-09-15

See [focused integration research](R1C4B_LIVE_MAGNET_INTEGRATION_RESEARCH.md).
AltSnap mature GPL-3.0-or-later, REFERENCE ONLY, pin
5c86416ad21e4b72844a998a746bd3bb0bee5f5d: License.txt, hooks.c MoveSnap and
LetWindowKickBack, PR 723, commits 524332458d1b01244f0fe211bb09d5a32174f911
and df25d36c6369bb13aa02ec83974e625fc7922c35 / PR 739 history. Lesson: app
constraints may alter resize; no message synthesis or source reuse adopted.
PowerToys/FancyZones mature MIT reference-only, pin
19c4d805321db86f3634e6968e14dbf25cbba14a: WindowMouseSnap.cpp, LICENSE,
PR 48569 / dd26d86580168d2e368701f7b0c4d629dc9cd9ac. Lesson: lifecycle abort
is distinct from completion. Microsoft Learn SetWindowPos, WM_WINDOWPOSCHANGING,
WM_SIZING are live official references, no immutable version claimed. Existing
AquaSnap proprietary behavior research is reused without binary inspection.
Applicable subsystems: source correction, resize bridge, feedback/lifecycle.
No external code copied, adapted or translated; no new code attribution duty.

## R1-C4A Fix 5 — 2026-09-15

[Scoped research](R1C4A_FIX5_PREACCEPT_VALIDATION.md) records inspected modules,
history and official links. AltSnap mature GPL-3.0-or-later reference-only at
5c86416ad21e4b72844a998a746bd3bb0bee5f5d: hooks.c filtering, License.txt and
397b84b2db114de979d428c3cec0cddec2a339ae history. PowerToys/FancyZones mature
MIT reference-only at 19c4d805321db86f3634e6968e14dbf25cbba14a:
WindowMouseSnap.cpp, LICENSE and PR 48569. Lessons: eligibility versus identity,
bounded observations, fatal lifecycle abort. Microsoft Learn DWebBrowserEvents2
and four geometry event pages (live, no immutable revision claimed), installed
Windows SDK 10.0.26100.0 ExDisp.Idl/ExDispid.h establish signatures only.
No external code copied/adapted/translated; no new code attribution obligation.
Subsystems: browser receipt classification, pre-accept validation diagnostics.

## R1-C4 architecture correction — 2026-09-15

See [behavior research](R1C4_AQUASNAP_BEHAVIOR_REFERENCE.md) for exact sources,
versions, inspected modules/history and lessons. AquaSnap/Nurgo is a mature
proprietary commercial behavior reference: official product, configuration,
v1.16.0 release and EULA pages only, no repository/commit or internals claimed.
AltSnap GPL-3.0-or-later at 5c86416ad21e4b72844a998a746bd3bb0bee5f5d,
hooks.c MoveSnap/ResizeSnap, issue 681, PR 682 and allocation-fix commit 397b84b:
REFERENCE ONLY. PowerToys/FancyZones MIT at
19c4d805321db86f3634e6968e14dbf25cbba14a, WindowMouseSnap.cpp, LICENSE and
PR 48569: REFERENCE ONLY. Microsoft Learn event constants/GetWindowRect are
live official platform references, no immutable version claimed. Applicable
subsystems: constraint geometry, candidate eligibility, interaction lifetime.
No external code copied/adapted/translated; no new code attribution obligation.
Research links retained. Lock affordance is PANE_BIND_ORIGINAL_UX, not upstream.

## R1-C4A Fix 3 terminal ownership review — 2026-09-15

Microsoft Learn live SetConsoleMode, ReadConsoleInputEx, Process Creation Flags
and STARTUPINFOW pages, Console Virtual Terminal Sequences input table (DEL
Backspace / ESC Escape); Microsoft site terms, no immutable revision claimed.
VS Code official Terminal Basics copy/paste and mouse-selection sections:
live documentation (page dated 2026-09-09), mature product behavior reference,
Microsoft documentation terms; no VS Code implementation code inspected.
Lesson: NOWAIT record reading does not require taking over cooked-input or
frontend selection/shortcut policy. Applicable subsystem: C4A human console
input and isolated automated mode fixtures. Code copied/adapted NO/NO; no new
code attribution required, source links retained in
[the Fix 3 decision](R1C4A_TERMINAL_MODE_PRESERVATION.md).
Fix 2's mandatory AltSnap/FancyZones review remains unchanged, reference-only.

## R1-C4A Fix 2 STA console wait review — 2026-09-14

Reference-only; copied/adapted code NO/NO. Exact sources and decisions:
[STA console research](R1C4A_STA_CONSOLE_WAIT.md).

| Source / maturity | Revision and verified license | Actual inspection / history / lesson |
| --- | --- | --- |
| AltSnap / mature maintained | 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; License.txt GPL-3.0-or-later | hooks.c 694-737; PR 609 history, 7f4afe59076b70980f71af202f63609ca3ac5745 diff; a worker's filtered dispatch is not an owner Shell STA pump; no worker/hook code reused |
| PowerToys/FancyZones / mature production | 19c4d805321db86f3634e6968e14dbf25cbba14a; root LICENSE MIT | main.cpp apartment-init call, FancyZonesApp.cpp routing; PR 48569 / dd26d86580168d2e368701f7b0c4d629dc9cd9ac diff; preserve destroy/invalidation delivery; no apartment equivalence claimed |
| Microsoft Learn / official platform contracts | Live pages read 2026-09-14; Microsoft Learn terms, no immutable SHA | Processes/Threads/Apartments, STA, MsgWaitForMultipleObjects/Ex, CoWaitForMultipleHandles, PeekMessageW, ReadConsoleInput/Ex, PeekConsoleInput, low-level console input, KEY_EVENT_RECORD, SetConsoleMode; non-pumping STA hazard, all-message dispatch and NOWAIT input; no sample code reused |

Applicable subsystem: C4A human-input waits only. Attribution: research links
retained; no new code attribution obligation from reference-only inspection.
GitHub APIs read source/history metadata only; no Git objects or refs rebuilt.

## R1-C4A batch and transient-role review — 2026-09-13

All reference-only; code copied/adapted NO/NO; no new code attribution required.
Source links retained in [R1C4A research](R1C4A_BATCH_PLACEMENT.md).

| Project / maturity | Verified revision / license | Actual inspection, history, lesson and subsystem |
| --- | --- | --- |
| AltSnap / mature maintained | 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; hooks.c and License.txt GPL-3.0-or-later | hooks.c 827-977, local path history, PR 609 and 7f4afe59076b70980f71af202f63609ca3ac5745 diff; deferred placement versus selection and owner coalescing; no global selection, GPL math, hooks or sleeping copied into C4A |
| Microsoft PowerToys/FancyZones / mature production | 19c4d805321db86f3634e6968e14dbf25cbba14a; root LICENSE MIT | DraggingState.cpp; PR 48569 and dd26d86580168d2e368701f7b0c4d629dc9cd9ac FancyZones.cpp/WindowMouseSnap.cpp diff; transient-state cleanup and abort-not-end on destroy; role/lifecycle design only |
| Microsoft Learn / official API contracts | Live pages read 2026-09-13; Microsoft Learn terms, no immutable SHA asserted | BeginDeferWindowPos, DeferWindowPos, EndDeferWindowPos, WinEventProc, out-of-context hooks, GetAsyncKeyState; replacement HDWP, common parent, abandon on Defer failure, no rollback guarantee, asynchronous receipt limitations and unchanged Ctrl latch; no sample code reused |

GitHub API was used only to read historical diffs, never to reconstruct Git
objects or refs. No upstream runtime was executed; upstream reports are not
PaneBind observations. AltDrag historical evidence remains the prior record;
no additional AltDrag source inspected this round.

## R1-C3B Phase 1 targeted hot-path review (2026-09-09)

All sources below are reference only; code copied/adapted: NO/NO. No upstream
program was run. Existing local fixed-HEAD checkouts and official GitHub
issue/PR metadata/diffs were read; no API-derived Git objects or refs were made.

| Project / classification | Revision and verified license | Actual inspection / lesson / applicable subsystem |
| --- | --- | --- |
| AltSnap / mature active | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`; hooks.c header + License.txt, GPL-3.0-or-later | hooks.c worker 695–735, placement wrapper 872–878, cadence 5442–5481, configuration 6677–6697; issue #160/comments, PR #609, commits `7f4afe59076b70980f71af202f63609ca3ac5745` and `7d4c7deb17437a7d5d350fd8074f6285444f4c51`; measure cadence/queue/work boundaries, do not copy timers/hooks/rate algorithms |
| AltDrag / mature historical | `e2740d605b0336a3b391fec26794718864b19521`; hooks.c GPL-3.0-or-later header and existing LICENSE provenance | hooks.c 1113–1128, path-scoped history, issue #38; raw-input cadence differs from placement, visible versus positioning bounds cannot be swapped |
| PowerToys/FancyZones / mature production | `19c4d805321db86f3634e6968e14dbf25cbba14a`; root LICENSE, MIT | FancyZonesApp.cpp lifecycle forwarding, FancyZones.cpp owner/start/update/end/destroy, WindowMouseSnap.cpp update/end, WindowUtils.cpp DWM frame adjustment; #12135, #18568, #18057; PR #18106 / `22786a6bdcbbc6eaeb417a6c6f1f15bb6fb0a550` default-layout repeat-loop fix; PR #48569 / `dd26d86580168d2e368701f7b0c4d629dc9cd9ac` destroy abort; no analogous PaneBind root cause inferred |
| AquaSnap/AquaGlue / mature proprietary UX reference | Live official help read 2026-09-09; proprietary/public-documentation facts only, no source SHA | Official AquaGlue configuration behavior; no internal implementation or smoothness-equivalence claim |
| Microsoft Learn / official platform contract | Live pages read 2026-09-09; Microsoft Learn terms, no sample code reuse | SetWinEventHook, PostThreadMessageW, MsgWaitForMultipleObjectsEx, PeekMessageW, DispatchMessageW, SetWindowPos, GetWindowRect, DwmGetWindowAttribute/DWMWINDOWATTRIBUTE, GetWindowThreadProcessId, GetAncestor, GetDpiForWindow, MonitorFromWindow, GetMonitorInfoW, QPC/QPF/high-resolution timing guidance, Begin/Defer/EndDeferWindowPos; preserve asynchronous delivery, geometry/identity checks and timing units |

Repositories: <https://github.com/RamonUnch/AltSnap>,
<https://github.com/stefansundin/altdrag>, <https://github.com/microsoft/PowerToys>.
No implementation attribution obligation is introduced by reference-only
inspection; research citations are retained. Any future external code reuse
requires separate authorization/license/provenance; GPL remains non-derivable.
Exact source/history/official URLs, conclusions, test obligations and research
gate: [R1C3B_SMOOTHNESS_RESEARCH.md](R1C3B_SMOOTHNESS_RESEARCH.md).

Review date for R0 entries: 2026-08-24.

This register covers external projects and documentation actually inspected for
PaneBind. A research citation does not authorize code reuse. No external code
was copied or adapted during R0. Any future reuse requires a new, explicit
license-compatibility decision before code enters the repository.

## AltSnap

```text
Project: AltSnap
Classification: Mature, active prior-art reference
Repository: https://github.com/RamonUnch/AltSnap
Commit / Tag / SHA reviewed: 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; describe 1.68-48-g5c86416; nearest release tag 1.68 -> 20307468b832b40998e1e65e96ab6eb28293ee62
License: GNU GPL v3-or-later per source headers; License.txt contains GPLv3; GitHub SPDX metadata reports GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: README.md; License.txt; Makefile; AltSnap.exe.manifest; AltSnap.txt; AltSnap.dni; altsnap.c; hooks.c; hooks.h; snap.c; zones.c; unfuck.h; nanolibc.h; relevant git history
Issues/PRs inspected: issues #7, #63, #160, #388, #413, #414, #452, #532, #538, #545, #644, #681; issue #618 attempted but unavailable (404); PRs #415, #682, #723, #748
What was learned: current non-injected low-level hook design; removal of AltDrag HookWindows; move/resize pipeline; unified monitor/window edge candidates; occlusion-aware edge filtering; min/max and application-adjusted resize behavior; positioning/visible-frame distinction; blacklist policy; worker/coalescing history; unresolved mixed-DPI and input reliability risks
Applicable PaneBind subsystem: future Windows input research; Windows window/monitor/frame adapter; normalized snapshots/events; pure geometry candidate model; filtering policy; reliability test matrix
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R0 does not authorize copying,
translation, mechanical restructuring, or adaptation of AltSnap code into the
MIT codebase. See [the detailed review](ALTSNAP_REVIEW.md).

## AltDrag

```text
Project: AltDrag
Classification: Mature historical comparison source
Repository: https://github.com/stefansundin/altdrag
Commit / Tag / SHA reviewed: e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later per source headers; repository LICENSE is GPLv3; GitHub SPDX metadata reports GPL-3.0
Date reviewed: 2026-08-24
Files/modules inspected: LICENSE; altdrag.c; hooks.c; hookwindows_x64.c; build.bat; AltDrag.ini; repository tree/metadata
Issues/PRs inspected: none directly; AltSnap issue #63 and the current AltSnap README were inspected for fork history
What was learned: optional HookWindows used a global WH_CALLWNDPROC hook, per-process DLL initialization/subclassing, and a second-bitness helper; AltSnap removed this injected design before its first public source revision
Applicable PaneBind subsystem: hook/input architecture and no-injection threat boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. The historical source was inspected
only to verify the before/after architecture described in the AltSnap review.

## Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit / Tag / SHA reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a (detached main SHA; no exact tag)
License: MIT; https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE
Date reviewed: 2026-08-24
Files/modules inspected: src/modules/fancyzones/FancyZones/FancyZonesApp.{h,cpp}; FancyZones/main.cpp; FancyZonesModuleInterface/dllmain.cpp; FancyZonesLib/FancyZones.{h,cpp}; FancyZonesWindowProcessing.{h,cpp}; WindowUtils.{h,cpp}; WorkArea.{h,cpp}; WorkAreaConfiguration.{h,cpp}; Layout.{h,cpp}; FancyZonesDataTypes.{h,cpp}; MonitorUtils.{h,cpp}; EditorParameters.{h,cpp}; VirtualDesktop.{h,cpp}; AppliedLayouts, AppZoneHistory, CustomLayouts and related data modules; Settings; OnThreadExecutor; ZonesOverlay; project files; native unit tests; legacy and .Next UI tests; editor unit/UI tests; fuzz tests; doc/devdocs/modules/fancyzones.md; root LICENSE
Issues/PRs inspected: issues #1685, #43363, #44058, #49016, #49019; PRs #44440, #48473, #48569, #49433; path-scoped git history through the reviewed SHA
Official documentation inspected: Microsoft Learn FancyZones product documentation and SetWinEventHook documentation
What was learned: use out-of-context event ingress and serialized owner-thread dispatch; scope noisy location events when product semantics permit; cancel state before topology replacement; model monitor/work-area/virtual-desktop identity explicitly; label DPI coordinate spaces; use reason-bearing filters; separate canonical layout data from caches; test teardown, missing events, and topology/DPI transitions; FancyZonesLib is Windows-specific and is not a platform-neutral core template
Applicable PaneBind subsystem: Windows event adapter; window snapshot/filtering; monitor/work-area/DPI adapter; normalized event model; core geometry boundary; lifecycle and test strategy
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any future copied or substantially reused MIT material must preserve the PowerToys MIT notice and be approved/recorded separately before entering PaneBind
```

License decision: **REFERENCE-ONLY FOR R0**. MIT compatibility is known, but
R0 independently implements only the documented platform boundary; no
PowerToys code was introduced. See [the detailed review](FANCYZONES_REVIEW.md).

## Microsoft Learn Windows desktop documentation

```text
Source: Microsoft Learn — Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A — live platform documentation; individual page metadata varies
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-24
Pages inspected: EnumWindows; SetWinEventHook; WINEVENTPROC; UnhookWinEvent; Event Constants; Object Identifiers; Guarding Against Reentrancy; GetAncestor; IsWindow; IsWindowVisible; Extended Window Styles; IsIconic; IsZoomed; GetWindowLongPtrW; GetWindowThreadProcessId; GetWindowTextW; GetClassNameW; QueryFullProcessImageNameW; OpenProcess; CloseHandle; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE; setting default DPI awareness; DPI_AWARENESS_CONTEXT; SetProcessDpiAwarenessContext; GetThreadDpiAwarenessContext; AreDpiAwarenessContextsEqual; GetDpiForWindow; GetWindowDpiAwarenessContext; MonitorFromWindow; GetMonitorInfoW; MONITORINFO
Issues/PRs inspected: N/A
What was learned: narrow out-of-context event registration; same-thread hook lifetime and reentrancy; event-specific object filtering; HWND lifetime races; visibility/cloak/tool/minimized distinctions; minimum-rights process metadata; positioning versus visible-frame bounds; PMv2 manifest semantics; target-window DPI semantics; signed virtual-screen monitor/work-area coordinates
Applicable PaneBind subsystem: Windows enumeration/events/filtering/snapshot/geometry/DPI/monitor/process-metadata adapter and normalized event mapping
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; inline official-document citations retained
```

Exact page URLs and claim-level citations are retained in
[R0_WINDOWS_EVENT_MODEL.md](R0_WINDOWS_EVENT_MODEL.md).

## R1-A targeted adjacency/topology review

Review date: 2026-08-25.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d (1.68-48-g5c86416); AltDrag e2740d605b0336a3b391fec26794718864b19521 (v1.1-8-ge2740d6)
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-A: AltSnap hooks.c (ShouldSnapTo, EnumWindowsProc, EnumTouchingWindows, MoveSnap, ResizeSnap, ResizeTouchingWindows); unfuck.h (visible-frame helpers, IsEqualT, IsInRangeT, SegT, AreRectsTouchingT); relevant AltDrag hooks.c snap scan
Issues/PR/history inspected for R1-A: AltSnap issues #59, #95, #347, #413, #445, #507, #566, #620, #681; PRs #415, #682, #723; commits 1dd26c9, 5fdb078, 5fb8307, fa5c70a, 3555048, 8b774a1, 90d0a06; AltDrag issues #1, #7, #32, #38; PRs #99, #134, #136, #187; commit 0055ed1
What was learned: visible-frame snapping; explicit pixel tolerance; mutable signed per-axis threshold risks; tolerance-expanded perpendicular candidate checks; sticky resize's historical move from edge-only matching to inclusive segment contact; immediate-neighbor-only behavior; no connected-component graph; unresolved frame-gap and mixed-DPI behavior
Applicable PaneBind subsystem: visible-geometry adjacency relation; deterministic graph/component solver; translation-session reliability tests; Windows eligibility remains outside core
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R1-A uses independently specified
mathematics and tests. It does not copy, translate, mechanically rewrite, or
adapt AltSnap/AltDrag implementation code.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-A
Files/modules/tests inspected for R1-A: FancyZonesLib Layout, Zone, WorkArea, WorkAreaConfiguration, FancyZones, MonitorUtils, FancyZonesDataTypes, FancyZonesWindowProcessing, WindowUtils, EditorParameters; AppliedLayouts and CustomLayouts stores; Layout/WorkArea/Zone/WindowProcessing/WorkAreaId unit tests
Issues/PR/history inspected for R1-A: issues #1167, #4962, #28626, #43363, #43386, #44058, #49016; PRs #13703, #19077, #19312, #27005, #28556, #44440, #48473, #49433, #49985; relevant merge commits recorded in R1_ADJACENCY_TOPOLOGY_RESEARCH.md
What was learned: WorkArea/native identity and operations must not become platform-neutral core; cursor sensitivity is not edge tolerance; topology replacement must abort stale consumers; canonical layout refresh prevents mixed-generation state; visual zone targets and positioning operation rectangles are separate contracts; DPI coordinate contexts must be unified before core; reason-bearing Windows eligibility remains caller policy; PaneBind mixed-DPI/multi-monitor runtime behavior remains NOT TESTED
Applicable PaneBind subsystem: immutable geometry snapshot boundary; future topology generation/lifetime contract; visible versus operation geometry separation; graph caller eligibility
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any future reuse requires separate approval and PowerToys MIT notice preservation
```

### Microsoft Learn geometry documentation

```text
Sources: GetWindowRect; DwmGetWindowAttribute / DWMWA_EXTENDED_FRAME_BOUNDS; RECT; MONITORINFO; FancyZones product documentation
Publisher: Microsoft Learn
Version / SHA: N/A; live documentation reviewed 2026-08-25
Terms: Microsoft Learn Terms of Use; facts paraphrased and cited only
What was learned: exclusive right/bottom rectangle convention; GetWindowRect DPI virtualization and invisible borders; visible DWM frame distinction; signed virtual-screen monitor/work-area coordinates; documented FancyZones mixed-DPI limitations
Applicable PaneBind subsystem: adapter-to-core visible geometry contract and signed coordinate test domain
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; inline citations retained
```

Detailed R1-A findings and decisions are in
[R1_ADJACENCY_TOPOLOGY_RESEARCH.md](R1_ADJACENCY_TOPOLOGY_RESEARCH.md).

## R1-B targeted Windows operations review

Review date: 2026-08-25.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d (1.68-48-g5c86416); AltDrag e2740d605b0336a3b391fec26794718864b19521 (v1.1-8-ge2740d6)
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-B: AltSnap hooks.c, unfuck.h, License.txt; AltDrag hooks.c, altdrag.c, LICENSE; pinned source history
Issues/PR/history inspected for R1-B: AltSnap issues #76, #160, #374, #572, #719; PRs #77, #573, #723; commits 1b64b08fb1db262b6f0a180b022243956c8a016e, a84f6b1084aa7346a9c847837843f5b221565864, c69135e1efbb9f97bccb8f86c713d319e6e0e835; historical AltDrag HookWindows path
What was learned: move-only SetWindowPos flags; sticky-resize Defer chain without structured error/post-verification; visible/positioning frame corrections; application-adjusted resize; weak IsWindow-only lifetime checks; synthetic interactive messages/WinEvents; async and NOSENDCHANGING application risks; requested geometry can be resisted
Applicable PaneBind subsystem: R1-B Windows owned-window operations, translation bridge, lifetime capability, batch failure model, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. R1-B independently specifies and
implements its capability, bridge, result, and batch contracts. No source was
copied, translated, mechanically rewritten, or structurally adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-B
Files/modules inspected for R1-B: FancyZonesLib WorkArea, WindowUtils, FancyZones, MonitorUtils, WorkAreaConfiguration, LayoutAssignedWindows, WindowDrag, WindowMouseSnap; FancyZones/FancyZonesApp; editor/FancyZonesEditor/Utils/NativeMethods.cs; FancyZones.UITests.Next/Utils/FancyZonesTestHelper.cs; common/Display/dpi_aware.cpp; root LICENSE
Issues/PR/history inspected for R1-B: issues #1685, #19440, #49016; PRs #17553, #21565, #28688, #44440, #48473, #48569, #49985; commits 75e966ce1981de26bb7c34f151b7939b88988233, 6d9d4a7112e3db72258989139907619b6bb65729, 3299ecfece9128cfd6791bacee1351b1c9a32c73, 6c2a99dfd6a12ad98feeda0acbc663aa84865676, ae9f241ef13737dab6f861767bbfdfca72b78475, dd26d86580168d2e368701f7b0c4d629dc9cd9ac, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: target placement uses SetWindowPlacement rather than a Defer batch; visible targets require frame adjustment; event callbacks dispatch to an owner thread; destroy/topology changes abort stale consumers; raw HWND state remains weaker than PaneBind's owned token; mixed-DPI failures require explicit coordinate context; placement failures are log-only and not a PaneBind result model
Applicable PaneBind subsystem: R1-B Windows owned-window operations, geometry bridge, lifetime/topology invalidation, DPI/monitor transitions, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future copied or substantially reused MIT material requires separate approval and preservation of the Microsoft MIT notice
```

An exact search of the pinned FancyZones tree found no
`BeginDeferWindowPos`, `DeferWindowPos`, or `EndDeferWindowPos` use. It is not a
batch atomicity or rollback precedent.

### Microsoft Learn Windows operations documentation

```text
Source: Microsoft Learn - Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live platform documentation
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-25
Pages inspected for R1-B: SetWindowPos; BeginDeferWindowPos; DeferWindowPos; EndDeferWindowPos; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE; IsWindow; GetWindowThreadProcessId; GetClassNameW; SetPropW/GetPropW/RemovePropW; CreateWindowExW; DestroyWindow; WM_NCCREATE; WM_CREATE; WM_DESTROY; WM_NCDESTROY; WM_WINDOWPOSCHANGING; WM_WINDOWPOSCHANGED; WM_MOVE; WM_SIZE; GetWindowDpiAwarenessContext; AreDpiAwarenessContextsEqual; GetDpiForWindow; PMv2/default DPI-awareness guidance; GetLastError
Issues/PRs inspected: N/A
What was learned: newest HDWP must be carried forward; a failed Defer chain must be abandoned without End; End has no documented rollback/transaction guarantee; visible DWM frame and positioning Window Rect differ; HWND checks are point-in-time and values recycle; creation/destruction message lifetime; PMv2 manifest and context comparison; WINDOWPOS message/application-adjustment behavior
Applicable PaneBind subsystem: owned capability validation, translation bridge, batch application, failure receipts, post-verification, DPI/monitor facts, and harness instrumentation
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level official-document citations retained
```

Detailed findings and independently selected R1-B contracts are in
[R1B_WINDOWS_OPERATIONS_RESEARCH.md](R1B_WINDOWS_OPERATIONS_RESEARCH.md).

## R1-C1 targeted companion-process review

Review date: 2026-08-26.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C1: AltSnap hooks.c, unfuck.h, README.md, altsnap.c; AltDrag hooks.c, altdrag.c, HookWindows implementation; pinned history
Issues/PR/history inspected for R1-C1: AltSnap issues #374, #572, #719; PR #573, #723; initial source/history removing AltDrag HookWindows; prior reviewed move/resize and lifetime history
What was learned: mature cross-process/cross-thread SetWindowPos and Defer behavior exists, but uses global discovery/raw HWND and point-in-time IsWindow rather than launch authority; transient process opens do not prevent PID reuse; requested geometry can be application-adjusted; explicit synthetic START/END prevents natural feedback attribution; injected subclassing is a rejected historical architecture
Applicable PaneBind subsystem: R1-C1 companion capability, process/window lifetime, cross-process placement, post-verification, feedback evidence, and no-injection boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. No implementation structure,
control flow, or code was translated or adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C1
Files/modules inspected for R1-C1: FancyZonesLib FancyZones, WorkArea, WindowUtils, MonitorUtils, LayoutAssignedWindows; FancyZones/FancyZonesApp; runner/editor launch and process-wait paths; common process/DPI utilities; UI test helpers; root LICENSE
Issues/PR/history inspected for R1-C1: issues #5947, #21849; PRs #44440, #48473, #48569, #49985; commits dd26d86580168d2e368701f7b0c4d629dc9cd9ac, ae9f241ef13737dab6f861767bbfdfca72b78475, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: production cross-process placement/event processing exists but global raw HWND state is not a launch capability; target placement is per-window SetWindowPlacement and provides no Defer precedent; selected component launch paths retain process handles but do not form a window-registration handshake; destroy/topology changes abort stale consumers; elevation and mixed-DPI remain explicit risks
Applicable PaneBind subsystem: R1-C1 process launch/lifetime, external window identity, destroy cancellation, integrity/DPI risk, post-verification, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future copied or substantially reused MIT material requires separate approval and preservation of the Microsoft MIT notice
```

### Microsoft Learn process, IPC, window, and security documentation

```text
Source: Microsoft Learn - Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live platform documentation
Terms: Microsoft Learn Terms of Use; facts were cited and paraphrased only
Date reviewed: 2026-08-26
Pages inspected for R1-C1: CreateProcessW; PROCESS_INFORMATION; process creation flags; WaitForInputIdle; WaitForSingleObject/WaitForMultipleObjects; GetExitCodeProcess; TerminateProcess; process termination; CreatePipe; anonymous pipe operations/security/inheritance; SetHandleInformation; Initialize/Update/DeleteProcThreadAttributeList; PROC_THREAD_ATTRIBUTE_HANDLE_LIST; ReadFile/WriteFile/CancelSynchronousIo; OpenProcessToken; GetTokenInformation; mandatory integrity control/UIPI; SetWindowPos; Begin/Defer/EndDeferWindowPos; IsWindow; GetWindowThreadProcessId; GetClassNameW; GetAncestor; GetWindowLongPtrW; GetWindow; SetPropW/GetPropW/RemovePropW; DestroyWindow; WM_WINDOWPOSCHANGING/CHANGED; SetWinEventHook; event constants
Issues/PRs inspected: N/A
What was learned: retained process HANDLE versus reusable PID; CreateProcess readiness limitations; WaitForInputIdle is not a handshake; two stream-framed anonymous pipes and restricted handle inheritance; bounded synchronous I/O/cleanup; per-use PID/TID/class/root/owner/marker validation; same-integrity baseline; cross-thread synchronous placement; application-adjusted WINDOWPOS; no native rollback guarantee; target-thread destruction; out-of-context feedback limitations
Applicable PaneBind subsystem: companion launch/IPC/session authority, process/window lifetime, token resolution, native translation operations, structured receipts, integrity/DPI facts, cleanup, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level official-document citations retained
```

Detailed findings and R1-C1 decisions are in
[R1C1_COMPANION_PROCESS_RESEARCH.md](R1C1_COMPANION_PROCESS_RESEARCH.md).

## R1-C2A targeted Explorer eligibility review

Review date: 2026-08-26; provisioning-recovery supplement reviewed
2026-08-27; user-consented recovery supplement reviewed 2026-08-27.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C2A: AltSnap window discovery/filtering/blacklist/process/frame/move paths and history; AltDrag historical discovery/injection paths
Issues/PR/history inspected for R1-C2A: existing reviewed Explorer/filter/visible-frame/DPI/application-adjustment issues and commits; R1-C1 movement/injection history
What was learned: mature generic Explorer movement uses raw discovery and reason filters but provides no new-window provenance, Shell location authority, canonical image/session identity, or safe exact-window cleanup; injection remains rejected
Applicable PaneBind subsystem: Explorer-specific eligibility reasons, geometry/post-verification risks, and no-injection/no-global-authority boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code; citations retained
```

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production and UI-test reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C2A
Files/modules inspected for R1-C2A: FancyZonesTestHelper Explorer launch/baseline/cleanup; FancyZonesWindowProcessing; WindowUtils; event/lifetime/DPI paths; root LICENSE
Issues/PR/history inspected for R1-C2A: existing reviewed destroy, DPI, elevation, filtering, placement, and topology hardening history
What was learned: HWND baseline set-delta is a useful candidate signal but upstream helper does not prove exact Shell location/image/integrity or reject multiple candidates, and its close-all cleanup is unsafe for PaneBind; production eligibility remains raw-HWND policy rather than capability
Applicable PaneBind subsystem: new-versus-preexisting candidate model, Explorer allowlist reasons, runtime test safety, and prohibited cleanup behavior
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; future MIT reuse requires separate approval/notice
```

### Microsoft Learn Shell, Win32, file, and security documentation

```text
Source: Microsoft Learn Windows Shell/Win32 documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live documentation
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Date reviewed: 2026-08-26
Recovery supplement date reviewed: 2026-08-27
User-consented recovery supplement date reviewed: 2026-08-27
Pages inspected for R1-C2A: Developing with Windows Explorer; IShellWindows; DShellWindowsEvents; DShellWindowsEvents.WindowRegistered; DShellWindowsEvents.WindowRevoked; IShellWindows::FindWindowSW; ShellWindowFindWindowOptions; IConnectionPointContainer::FindConnectionPoint; IConnectionPoint::Advise/Unadvise; Architecture of Connectable Objects; IDispatch::Invoke/DISPPARAMS; IUnknown::QueryInterface and Rules for Implementing QueryInterface; IWebBrowser2 Navigate2/HWND/Quit; DWebBrowserEvents2 and NavigateComplete2; ShellExecute/ShellExecuteEx; SHELLEXECUTEINFO; cmd start; SHCreateItemFromParsingName; SHGetIDListFromObject; PathCreateFromUrlW; QueryFullProcessImageNameW; GetWindowsDirectoryW; CreateFileW; GetFileInformationByHandleEx/FILE_ID_INFO; IsWindow; GetWindowThreadProcessId; GetClassNameW; GetAncestor; GetWindowLongPtrW; GetWindow; IsWindowVisible; DwmGetWindowAttribute/DWMWA_CLOAKED/EXTENDED_FRAME_BOUNDS; IsIconic; IsZoomed; IVirtualDesktopManager; OpenProcess/OpenProcessToken/GetTokenInformation; SetWindowPos; GetWindowRect; MonitorFromWindow/GetMonitorInfoW/GetDpiForWindow; Single-Threaded Apartments; CoInitializeEx; MsgWaitForMultipleObjectsEx; BCryptHash and SHA-256 CNG guidance
Issues/PRs inspected: N/A
What was learned: official ShellBrowserWindow creation and ShellWindows inventory; Shell registration/revocation cookies through the DShellWindowsEvents connection point; separation of the Advise connection token from a Shell registration cookie; FindWindowSW cookie resolution with NEEDDISPATCH; canonical IUnknown object identity; no documented total ordering among registration, navigation, visibility, Quit, revocation, and HWND lifetime; NavigateComplete2 frame/asynchrony limitations; documented ShellExecute folder reuse when separate-process folder launch is disabled; ShellExecuteEx hProcess is neither guaranteed nor proof of a unique new HWND; cmd start `/separate` is a 16-bit memory-space option unsupported on 64-bit platforms, not an Explorer-frame contract; no reviewed current Microsoft contract makes explorer.exe `/n` or `/separate` a Windows 11 new-HWND guarantee; tab/frame ambiguity; baseline HWND exclusion independent of positive target location authority; filesystem URL/file identity; process image and kernel-handle lifetime; fail-closed state/security/desktop allowlist; pure single translation and exact post-verification
Applicable PaneBind subsystem: Explorer target isolation, read-only inventory, user-consented capability issuance, live eligibility, process/location identity, safe operation/restore, user-owned close, and feedback evidence
Code copied: NO
Code adapted: NO
Attribution required: NO for code reuse; claim-level citations retained
```

Recovery-specific official URLs:

- <https://learn.microsoft.com/en-us/windows/win32/shell/developing-with-windows-explorer>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nn-exdisp-ishellwindows>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowregistered>
- <https://learn.microsoft.com/en-us/windows/win32/shell/dshellwindowsevents-windowrevoked>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/nf-exdisp-ishellwindows-findwindowsw>
- <https://learn.microsoft.com/en-us/windows/win32/api/exdisp/ne-exdisp-shellwindowfindwindowoptions>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpointcontainer-findconnectionpoint>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-advise>
- <https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iconnectionpoint-unadvise>
- <https://learn.microsoft.com/en-us/windows/win32/com/architecture-of-connectable-objects>
- <https://learn.microsoft.com/en-us/windows/win32/api/oaidl/nf-oaidl-idispatch-invoke>
- <https://learn.microsoft.com/en-us/windows/win32/api/oaidl/ns-oaidl-dispparams>
- <https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface>
- <https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface%28refiid_void%29>
- <https://learn.microsoft.com/en-us/previous-versions/aa752134%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/mt725310%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/aa752140%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768285%28v%3Dvs.85%29>
- <https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw>
- <https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow>
- <https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/start>

### Microsoft Windows SDK Shell automation declarations

```text
Source: Installed Microsoft Windows SDK
Publisher: Microsoft
Version reviewed: 10.0.26100.0
License/terms: installed Windows SDK terms; declarations inspected for facts only
Date reviewed: 2026-08-27
Files inspected: um/ExDisp.idl; um/ExDisp.h; um/ExDispid.h; um/ocidl.h
Issues/PR/history inspected: N/A; versioned installed SDK declarations
What was learned: exact IWebBrowser2/IShellWindows ABI signatures; ShellWindowFindWindowOptions values; DIID_DShellWindowsEvents and DIID_DWebBrowserEvents2; WindowRegistered/WindowRevoked and NavigateComplete2 dispinterface signatures; DISPIDs 200/201/252; ShellWindows and ShellBrowserWindow source-interface declarations; connection-point method ABI
Applicable PaneBind subsystem: Explorer provisioning lease, Shell registration sink, cookie correlation, navigation readiness, sink lifetime, and exact-object cleanup
Code copied: NO
Code adapted: NO
Attribution required: NO; no SDK implementation or declaration was copied into PaneBind
```

Detailed findings and R1-C2A decisions are in
[R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md](R1C2A_EXPLORER_ELIGIBILITY_RESEARCH.md).

## R1-C2B targeted Explorer Glue feedback review

Review date: 2026-09-03.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License: GNU GPL v3-or-later; REFERENCE ONLY
Files/modules inspected for R1-C2B: AltSnap hooks.c movement lifecycle, worker queue, sticky resize, disabled WinEvent experiment; AltDrag hooks.c lifecycle messages and historical HookWindows injection
Issues/PR/history inspected for R1-C2B: AltSnap PRs #564, #573, #580, #609; issues #507, #572, #575, #620, #725; commits 2ed9fe3dff49b260c25ce9abbd71f541dbfc1ca0, 1b64b08fb1db262b6f0a180b022243956c8a016e, 45ec7b4343ea4b8c6342cb1974933756367b022b, 7f4afe59076b70980f71af202f63609ca3ac5745, fa5c70a5029f418cac56007053fe67024cbeef86; AltDrag commits 4d90661a757976fdaa0ac31d028f5d9313ea3114, c46747bb184a72f08876a265b9024ddb59b4c073, f877c42afb8cf05eb98ec533834e775112aeea3f, 3d1fa0b22582c32631c5f3ef455b3cc76eb8338c; AltDrag lifecycle compatibility and HookWindows history; AltSnap's later removal of the inherited HookWindows injection design
What was learned: AltSnap explicitly synthesizes START/END and caused FancyZones cross-tool feedback; START timing and END edge cases can create false lifecycle; one worker/coalescing is useful architecture evidence but no generation-bound feedback ledger exists; sticky resize is not Glue Move; AltDrag injection/subclassing and synthetic lifecycle compatibility are rejected
Applicable PaneBind subsystem: event provenance limits, explicit state transitions, callback/owner separation, exact follower feedback attribution, bounded ledger, no-recursion stress, and no-injection boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for code because no GPL code entered PaneBind; research citations retained
```

License decision: **GPL REFERENCE-ONLY**. No implementation structure,
algorithm, control flow, or code was translated or adapted.

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License: MIT; reference-only in R1-C2B
Files/modules inspected for R1-C2B: FancyZones/FancyZonesApp hook lifecycle; FancyZonesLib FancyZones owner dispatch; WindowMouseSnap/work-area lifetime; WindowUtils placement; UI test helper; root LICENSE
Issues/PR/history inspected for R1-C2B: PRs #44440, #48473, #48569, #49433, #49985; issue #49016; commits 6c2a99dfd6a12ad98feeda0acbc663aa84865676, ae9f241ef13737dab6f861767bbfdfca72b78475, dd26d86580168d2e368701f7b0c4d629dc9cd9ac, 37d8729ac3eec734f4d000079145d6fcb40db3a5, d68980a81bb8de144bdec998a114e948bf68c563
What was learned: callback-to-owner dispatch and lifecycle-scoped hooks are mature patterns; process skipping is not feedback suppression; pinned code has no bounded receipt queue or expected-operation ledger; destroy/topology replacement must abort and terminate consumers before state replacement; stale generation and mixed-DPI history require whole-session invalidation
Applicable PaneBind subsystem: additive Glue WinEvent source, owner-thread state machine, bounded backpressure, frozen topology, invalidation, setup/cleanup isolation, and DPI/monitor safety
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; any future reuse requires separate approval and preservation of Microsoft MIT notice
```

### Microsoft Learn WinEvent, identity, geometry, and placement documentation

```text
Source: Microsoft Learn Windows desktop/Win32 API documentation
Publisher/repository: Microsoft; https://learn.microsoft.com/en-us/windows/win32/
Version / SHA: N/A - live documentation
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Date reviewed: 2026-09-03
Pages inspected for R1-C2B: SetWinEventHook; UnhookWinEvent; Out-of-Context Hook Functions; Guarding Against Reentrancy; WinEventProc; NotifyWinEvent; Event Constants; SetWindowPos; WM_WINDOWPOSCHANGING; GetWindowThreadProcessId; IsWindow; PROCESS_INFORMATION; GetWindowRect; DwmGetWindowAttribute; DWMWINDOWATTRIBUTE / DWMWA_EXTENDED_FRAME_BOUNDS
Issues/PRs inspected: N/A
What was learned: out-of-context delivery is queued, asynchronous, sequential to the installing message-loop thread, and reentrancy-sensitive; process/thread filters are noise reduction, not authority; unhook is same-thread; native timestamps have no sufficient correlation contract; START/END can be synthetically notified; LOCATION conflates position/shape/size; SetWindowPos success is not exact geometry; ASYNC placement is unsuitable for immediate verification; HWND/PID/TID are point-in-time facts; visible and positioning rectangles remain distinct non-atomic reads
Applicable PaneBind subsystem: hook lifecycle, minimal callback receipt, owner-thread drain, exact identity/generation filter, pending operation registration, feedback reconciliation, synchronous placement, post-verification, and restore
Code copied: NO
Code adapted: NO
Attribution required: NO for code; claim-level official-document citations retained
```

Official URLs reviewed:

- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-notifywinevent>
- <https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos>
- <https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow>
- <https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information>
- <https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect>
- <https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmgetwindowattribute>
- <https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute>

Detailed findings and independently selected R1-C2B contracts are in
[R1C2B_EXPLORER_GLUE_RESEARCH.md](R1C2B_EXPLORER_GLUE_RESEARCH.md).

## R1-C2B UAT Fix 2 targeted processing-cadence review

Review date: 2026-09-05. This is an additive reference-only inspection; no
upstream behavior was run or relabeled as a PaneBind empirical result.

### AltSnap / AltDrag

```text
Projects: AltSnap; AltDrag
Classification: Mature active behavioral prior art; mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License verified: AltSnap hooks.c header and License.txt; AltDrag hooks.c header and LICENSE; GPL-3.0-or-later, REFERENCE ONLY
Files/modules actually reinspected: AltSnap hooks.c WorkerThread and movement work coalescing; AltDrag hooks.c lifecycle message compatibility paths
Issues/PR/history actually reinspected: AltSnap PR #609 and local commit diff 7f4afe59076b70980f71af202f63609ca3ac5745; path-scoped hooks.c history; existing AltDrag historical comparison retained
What was learned: one serialized behavior owner and coalescing consecutive movement work are mature patterns; their code does not supply PaneBind historical WinEvent geometry, bounded processing fairness or operation receipts
Applicable PaneBind subsystem: private Glue owner scheduling, explicit processing-time sample semantics and small callback boundary
Code copied: NO
Code adapted: NO
Attribution required: NO for implementation; research citations retained, GPL implementation structure/control flow must not be translated or adapted
```

Pinned source and history links:

- [AltSnap WorkerThread](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c#L693-L735)
- [AltSnap worker PR #609](https://github.com/RamonUnch/AltSnap/pull/609)
- [AltSnap worker commit](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)
- [AltDrag pinned hooks.c](https://github.com/stefansundin/altdrag/blob/e2740d605b0336a3b391fec26794718864b19521/hooks.c)

### Microsoft PowerToys / FancyZones

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License verified: pinned root LICENSE, MIT; reference-only in Fix 2
Files/modules actually reinspected: FancyZones/FancyZonesApp.cpp hook subscription and forwarding; FancyZonesLib/FancyZones.cpp owner lifecycle/location/destroy dispatch
Issues/PR/history actually reinspected: PR #48569 and immutable commit dd26d86580168d2e368701f7b0c4d629dc9cd9ac through GitHub rendered discussion/diff
What was learned: callbacks forward to an owner, and destroy invalidation must abort rather than finalize movement; lifecycle/invalidation boundaries must survive scheduling/coalescing
Applicable PaneBind subsystem: bounded Glue owner turn and explicit lifecycle barriers; not a native follower feedback matching algorithm
Code copied: NO
Code adapted: NO
Attribution required: NO for this reference-only review; any later code reuse requires separate authorization/provenance and Microsoft MIT notice preservation
```

Inspection limitation: historical `git show` in the existing partial clone
triggered promisor fetch, which failed with GitHub connection resets/timeouts;
that synchronization was stopped. Pinned HEAD source/license were already
available locally. The PR and commit diff were independently inspected as web
sources; no objects were reconstructed, refs moved, or fetch simulated.

- [FancyZones pinned hook wrapper](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZones/FancyZonesApp.cpp#L96-L181)
- [FancyZones pinned owner](https://github.com/microsoft/PowerToys/blob/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/FancyZones.cpp#L961-L997)
- [Destroy-abort PR #48569](https://github.com/microsoft/PowerToys/pull/48569)
- [Destroy-abort immutable diff](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)

### Microsoft Learn message delivery, live geometry and COM documentation

```text
Source: Microsoft Learn Windows desktop/Win32 documentation
Publisher: Microsoft
Revision: live pages reviewed 2026-09-05; no immutable SHA asserted
Terms: Microsoft Learn Terms of Use; facts paraphrased/cited only
Pages actually inspected: SetWinEventHook; Out-of-Context Hook Functions; WinEventProc; GetWindowRect; PeekMessageW; MsgWaitForMultipleObjectsEx; Guarding Against Reentrancy in Hook Functions; Single-Threaded Apartments
Issues/PRs inspected: N/A
What was learned: WinEvent envelope has no geometry; late window query is not event-time history; PeekMessage dispatches nonqueued/internal events before visible MSG retrieval; one MSG is not one callback; MWMO_INPUTAVAILABLE preserves wake opportunity for seen queued input; STA cross-process COM and message pumping permit reentrancy
Applicable PaneBind subsystem: event-driven owner fairness; raw receipts versus processing-time samples; bounded notification rearm; removal of explicit nested Shell waiting only in private Glue path; no callback capture and no polling
Code copied: NO
Code adapted: NO
Attribution required: NO for code; official links retained beside claims
```

- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Out-of-Context Hook Functions](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)
- [WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
- [PeekMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagew)
- [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
- [Guarding Against Reentrancy](https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions)
- [Single-Threaded Apartments](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments)

Fix 2 independent decisions, limits and research gate are recorded in
[R1C2B_EXPLORER_GLUE_RESEARCH.md](R1C2B_EXPLORER_GLUE_RESEARCH.md).

## R1-C3A Ctrl + Move activation source review — 2026-09-09

### AltSnap / AltDrag modifier, lifecycle and cadence review

```text
Projects: AltSnap; AltDrag
Classification: AltSnap mature maintained movement reference; AltDrag mature historical comparison
Repositories: https://github.com/RamonUnch/AltSnap ; https://github.com/stefansundin/altdrag
Commits reviewed: AltSnap 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; AltDrag e2740d605b0336a3b391fec26794718864b19521
License verified: hooks.c headers and AltSnap License.txt / AltDrag LICENSE; GPL-3.0-or-later, REFERENCE ONLY
Date reviewed: 2026-09-09
Files/modules actually inspected: AltSnap altsnap.c hook loading; hooks.c IsHotkeyDown, IsCtrlDown, LowLevelKeyboardProc, LowLevelMouseProc, WorkerThread, NotifySizeMoveStaEnd, FinishMovementNow, touching-window/StickyResize paths and performance defaults/configuration; AltDrag hooks.c modifier checks, movement update counter, enter/exit messages, movement timer and CallWndProc/subclassing path; local path history
Issues/PRs actually inspected: AltSnap issue #572; PRs #537, #609, #689, #695; AltDrag v1.1 release history
Commit diffs actually inspected: AltSnap 6890cabfe93f5526491d146626f4874727ace8fc, 7f4afe59076b70980f71af202f63609ca3ac5745, ce72d731563eda654ee2efbd336aa2c2e48525d3, 84249cc38d6aa159ee95a945819969cc98100a57, f4c7ab3c1a1d02451f1cde577a2f92e617ae5f55; AltDrag f614a2b6a1c89f804c79be19b86185dd5cfb158b
What was learned: tracked modifier state differs from async current state; Ctrl repeat/release/AltGr history matters; a later async sample cannot blindly replace an earlier state fact; movement work may be count/timestamp throttled and consecutive worker work coalesced; explicit lifecycle compatibility can interact with other managers; synthetic input, injected hooks and timers are not PaneBind implementation templates
Applicable PaneBind subsystem: Explorer START activation boundary, callback-versus-owner evidence, one-session latch and invalidation tests, bounded timing measurements without cadence tuning
Code copied: NO
Code adapted: NO
Attribution required: NO for code; research citations retained; GPL code copying, translation and derivative implementation remain prohibited
```

Exact source anchors, history links and the distinction between source facts
and PaneBind inferences are in
[R1C3A_CTRL_MOVE_ACTIVATION_RESEARCH.md](R1C3A_CTRL_MOVE_ACTIVATION_RESEARCH.md).

### PowerToys / FancyZones modifier and drag lifetime review

```text
Project: Microsoft PowerToys / FancyZones
Classification: Mature production reference
Repository: https://github.com/microsoft/PowerToys
Commit reviewed: 19c4d805321db86f3634e6968e14dbf25cbba14a
License verified: root LICENSE, MIT; REFERENCE ONLY for R1-C3A
Date reviewed: 2026-09-09
Files/modules actually inspected: FancyZones/FancyZonesApp.cpp; FancyZonesLib/FancyZones.cpp; WindowMouseSnap.cpp; DraggingState.{cpp,h}; KeyState.h; GenericKeyHook.h; KeyboardInput.cpp; root LICENSE
Issues/PRs actually inspected: PR #49985 and immutable commit d68980a81bb8de144bdec998a114e948bf68c563; PR #48569 and immutable commit dd26d86580168d2e368701f7b0c4d629dc9cd9ac; issue #49016 (topology context, not modifier evidence)
What was learned: WinEvent ingress and owner dispatch are separate; dynamic Shift state uses Raw Input while Ctrl state uses async initialization plus a low-level hook; swallowing Shift can suppress the module's own input update; mode-switch ordering can erase first highlight; destroyed windows must abort without zoning and clear drag state; this broader input design does not demonstrate a hook requirement for START-latched Ctrl sampling
Applicable PaneBind subsystem: input/activation separation, authoritative receipt facts versus processing-time diagnostics, invalidation cleanup, first-event and negative-path tests
Code copied: NO
Code adapted: NO
Attribution required: NO for reference-only review; any future reuse requires a separate explicit decision and MIT notice preservation
```

The fixed source/license files were available locally. A historical local diff
triggered a promisor fetch that failed to connect to GitHub HTTPS/443; that Git
synchronization was stopped. Historical conclusions use independently read
GitHub PR/immutable commit pages, not reconstructed Git objects. No remote ref
was changed and no API transport fallback was used.

Reference gate scope: source/license/history inspection PASS. This is not a
human Ctrl+Move run, a smoothness result, a keyboard-origin authenticity claim,
or authorization to change R1-C2B processing cadence.

### R1-C3A public behavior and official input/timing contracts

```text
Review date: 2026-09-09
Project: AquaSnap / AquaGlue, Nurgo Software
Classification: Mature commercial behavior reference, closed-source
Revision: live official product/help pages; v1.10.0 announcement dated 2014-12-02; no source SHA asserted
License/terms: proprietary product, copyrighted public documentation; behavior reference only
Inspected: AquaGlue configuration, AquaSnap product, official v1.10.0 announcement
History: official announcement introducing Ctrl movement/resizing groups
Lesson/subsystem: documented adjacent-group Ctrl+Move/Resize UX; no internal API or latch claim
Copied/adapted code: NO / NO
Attribution: source links retained; no code attribution obligation from reference-only facts

Source: Microsoft Learn / Win32, official platform contracts
Revision: live pages read 2026-09-09; no immutable revision asserted
Terms: Microsoft Learn Terms of Use; cited/paraphrased, no sample code copied
Inspected: GetAsyncKeyState, GetKeyState, GetKeyboardState, SetWinEventHook, event constants, LowLevelKeyboardProc/WH_KEYBOARD_LL, Raw Input, RegisterRawInputDevices, RegisterHotKey, QueryPerformanceCounter, QueryPerformanceFrequency, acquiring high-resolution time stamps
Issues/PRs: N/A (API contracts)
Lesson/subsystem: callback-delivery high-bit sampling, async/queue/UIPI limits, bounded same-owner monotonic timing
Copied/adapted code: NO / NO
Attribution: official links retained, no code reuse
```

Exact official URLs, API comparison, independently written owned probe and
implementation gate are in [R1C3A research](R1C3A_CTRL_MOVE_ACTIVATION_RESEARCH.md).

## R1-C3B Phase 2 consent-bound proof review — 2026-09-09/10

Result: BLOCKED by an unproved same-frame Shell-entry invariant. No external
code copied, adapted, translated or mechanically derived; no attribution-bearing
code enters PaneBind. Draft implementation was withdrawn, not shipped.

| Inspected reference | Exact revision / license / maturity | Scope and history | Lesson / subsystem |
| --- | --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap) | 5c86416ad21e4b72844a998a746bd3bb0bee5f5d; License.txt GPL-3.0-or-later; mature maintained movement reference, reference-only | hooks.c 695-735 and license; PR 609 and listed commits 7f4afe59076b70980f71af202f63609ca3ac5745, 7d4c7deb17437a7d5d350fd8074f6285444f4c51 | Worker/coalescing history is not permission to move Shell COM across apartments or remove safety checks. |
| [PowerToys/FancyZones](https://github.com/microsoft/PowerToys) | 19c4d805321db86f3634e6968e14dbf25cbba14a; root LICENSE MIT; mature production reference, reference-only | FancyZones.cpp abort/destroy paths, root license; PR 48569 / dd26d86580168d2e368701f7b0c4d629dc9cd9ac and PR 18106 / 22786a6bdcbbc6eaeb417a6c6f1f15bb6fb0a550 | Retain lifecycle invalidation; repeated-work optimization requires a proved validity interval. |
| Microsoft Learn (official API contracts) | Live pages read 2026-09-09/10; no immutable revision asserted; Microsoft Learn terms, paraphrase/link only | QueryInterface identity rules, single-threaded apartments, NavigateComplete2, IConnectionPoint::Unadvise, HWND property | Canonical object is not exclusive frame identity; outgoing COM may reenter; target navigation stream is not a global inventory epoch. No samples reused. |

Historical AltDrag review remains the Phase 1 record; no additional AltDrag
source was inspected for Phase 2. Official and immutable research links,
countermodel and rejected fallback are in
[R1C3B_PHASE2_VALIDATION_CONTRACT.md](R1C3B_PHASE2_VALIDATION_CONTRACT.md).
Required attribution: research links retained; no code attribution obligation
from these reference-only inspections. Existing license boundaries unchanged.

## Human Root frame-authority amendment — 2026-09-10

Same verified local pins/licenses: AltSnap
5c86416ad21e4b72844a998a746bd3bb0bee5f5d (GPL-3.0-or-later, reference only),
hooks.c placement 860-890; FancyZones
19c4d805321db86f3634e6968e14dbf25cbba14a (MIT, reference only),
FancyZones.cpp 961-1002 owner HWND dispatch/destroy-abort. Prior PR 609,
48569 and 18106 history remains the scoped historical evidence, not a source
of copied algorithms. Classifications and attribution boundaries above remain.

Additional official pages actually read: Microsoft Learn IWebBrowser2 HWND,
IShellWindows::Register, DShellWindowsEvents::WindowRegistered/WindowRevoked.
Live pages, no immutable version claimed, Microsoft Learn terms. Exact links
are in [the decision](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md).
Lesson: a frame, a Shell collection cookie and a PaneBind capability are
different authority levels; membership notifications are not a global validity
epoch. Tabbed-IE wording is a frame design input, not empirical Windows 11
Explorer tab implementation evidence. No code samples copied or adapted;
research links retained, no new code attribution obligation.

Human Root, not upstream source, authorizes removing post-issuance entry
cardinality from frame authority. All implementation is independently written
against PaneBind's private ledger/permit and the explicit amended contract.

## R1-C3B Phase 3 VDM lifetime review — 2026-09-12

AltSnap: mature reference, GPL-3.0-or-later, REFERENCE ONLY. Verified local
5c86416ad21e4b72844a998a746bd3bb0bee5f5d and License.txt; hooks.c 3040-3087
enumeration/filtering and path history 62adaf14503417a4fdbff61e023950208436c77d,
96ea072af389362f79b66c8286289f9837c689ef. No VDM lifetime mechanism inferred
from those filters. No copying, translation or adaptation; no code attribution.

PowerToys/FancyZones: mature production reference, MIT, REFERENCE ONLY. Read
VirtualDesktop.cpp/.h and LICENSE at 19c4d805321db86f3634e6968e14dbf25cbba14a
through pinned source URLs (old temporary checkout no longer present). Source
retains a manager across calls; its singleton/registry/failure policies were
not adopted. Read path history for PR 29059 / 78a94aecb965a7c10ded273a1227da3e2259b341,
PR 28556 / 890b7f4286a95ced04d7da140b474f90fd4351ed, PR 18805 /
f5f8861eac976384273523be335572315a54566c; inspected 29059's VirtualDesktop.cpp
diff. Issue 49019 is an upstream stale-registry-state report, not PaneBind UAT.
No code copied/adapted; research links retained; no code attribution obligation.

Microsoft Learn / Old New Thing: official live API documents read 2026-09-12,
no immutable revision asserted, Microsoft site terms. IsWindowOnCurrentVirtualDesktop,
CoGetApartmentType, STA, CoUninitialize and the 2017-10-02 current-desktop
example distinguish repeated fresh queries from interface lifetime. Samples,
global pointers and permissive fallbacks were not reused. Exact links and the
independent owner-STA design/test plan are in
[R1C3B_PHASE3_VDM_LIFETIME.md](R1C3B_PHASE3_VDM_LIFETIME.md).

Scope: read-only VDM service lifetime and evidence. No Git API payload was used
to reconstruct local Git objects/refs, and no third-party window was controlled.

## R1-C4B Architecture Pivot 1 Fix C — 2026-09-27

Starting HEAD: `10d429706b8afb3be11f308370af89964a5a054a`.
Applicable subsystem: test-only structured handoff diagnostics, matched
out-of-context MOVESIZE START/END witness, hook lifetime, owned input cleanup.
See [Fix C END diagnostics research](R1C4B_PIVOT1_FIXC_END_DIAGNOSTICS.md).
Old Fix A/B records and evidence are unchanged; the accepted Fix B observed
Move PASS is not downgraded by Resize's pre-write compound blocker.

AltSnap, mature upstream; existing pinned local checkout HEAD
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`, clean at read time; GPL-3.0-or-later,
REFERENCE ONLY. This round actually re-read `hooks.c` license header and
`License.txt` opening, `NotifySizeMoveStaEnd`, the conditional `EVENT_HOOK`
`HandleWinEvent` / `PinWindowProc`, pin teardown and movement-end notifications.
The pin hook is an experimental conditional path, not a claim of production
reliability. Actually read [issue 572](https://github.com/RamonUnch/AltSnap/issues/572)
and local `altsnap.c` history diff at
[8a5c422928d37d34e63cb8b99d4a74e14cb955f0](https://github.com/RamonUnch/AltSnap/commit/8a5c422928d37d34e63cb8b99d4a74e14cb955f0)
(do not free hooks DLL when disabling). Source:
[pinned repository](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d).
Lessons: callback code/resource ownership and synthetic lifecycle compatibility
require care; upstream synthetic WM/NotifyWinEvent is not true PaneBind OS END.
No GPL code, timer/thunk architecture or control flow copied/adapted/translated.

PowerToys/FancyZones, mature production reference; existing pin
`19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT, REFERENCE ONLY. This round
actually read pinned
[LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE)
and [WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)
in full, [PR 48569](https://github.com/microsoft/PowerToys/pull/48569) body and
[dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
rendered merge diff for destroy-event routing, drag Abort/reset and cleanup.
No claim of re-reading all FancyZonesApp/FancyZones implementation files in
this round. Lessons: normal END versus abort/resource cleanup must remain
distinct; upstream manual reports/tests are not PaneBind evidence. No source
copied/adapted/translated. Required attribution for both upstream projects:
retain research citations; no new code attribution obligation.

Microsoft Learn primary live pages actually read in Fix C; no immutable
document revision asserted, Microsoft site terms, links/paraphrases only:

- [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
- [Event Constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
- [WinEventProc](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wineventproc)
- [UnhookWinEvent](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent)
- [Out-of-Context Hook Functions](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)
- [Guarding Against Reentrancy](https://learn.microsoft.com/en-us/windows/win32/winauto/guarding-against-reentrancy-in-hook-functions)
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)

Lessons: asynchronous END receipt is not internal OS END time or a DWM
synchronous guarantee; callback only envelopes and must avoid reentrancy;
installer thread pumps and unhooks; both real native END and exact matched
WinEvent END precede a fresh preflight/first write. High-bit key state alone
does not confer desktop/input authority, SendInput is not HWND-addressed or a
WM_INPUT delivery guarantee, and safe cleanup UP never substitutes acceptance.
Research PASS is limited to the instrumented owned-test design/synthetic gate;
new empirical END/Resize/repetition/product/Explorer behavior remains NOT TESTED.
No new project, external code reuse, GUI execution or old-evidence rewriting.

## R1-C4B Architecture Pivot 1 Fix D — 2026-09-27

Starting HEAD: `e4ecdeaf4311e4adc95fcd94ff967df4c0f1bc83`.
Subsystem: separated product gesture authority / automated synthetic input
isolation, test-only post-END nonactivating shield, hit-test/lifetime/cleanup.
See [Fix D input-isolation research](R1C4B_PIVOT1_FIXD_INPUT_ISOLATION.md).
Old A/B/C docs, raw logs and verdicts remain unchanged. Two existing C JSONL
were read-only parsed (116 rows each), native/callback/fresh facts checked and
SHA256 rechecked; no GUI observations or new takeover acceptance asserted.

AltSnap, mature maintained GPL-3.0-or-later, REFERENCE ONLY. Existing local pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d` verified; this round actually read
hooks.c GPL header and License.txt opening, scoped ScrollPointedWindow,
PinWindowProc/CreatePinWindow and pin/transparent-window teardown. Read local
path history and actual complete hooks.c diff at
[400eebf04dc651f76b2c1148c63fee7a4b039d8c](https://github.com/RamonUnch/AltSnap/commit/400eebf04dc651f76b2c1148c63fee7a4b039d8c),
plus [issue 572](https://github.com/RamonUnch/AltSnap/issues/572) body.
Source: [pinned repository](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d).
Lessons: indicator activation/hit-test/lifetime are distinct; NOACTIVATE-removal
fallback, foreign point-window mutations, timer/thunk/owner-topmost policies
are not suitable for this test shield. Upstream cross-tool reports are not
PaneBind empirical evidence. No GPL code/control flow copied/adapted/translated.

PowerToys/FancyZones, mature production MIT, REFERENCE ONLY, existing immutable
pin `19c4d805321db86f3634e6968e14dbf25cbba14a`. Actually re-read full
[LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE)
and [WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp),
[PR 48569](https://github.com/microsoft/PowerToys/pull/48569) body and actual
[dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
rendered history diff (destroy routing, Abort/reset, cleanup). Candidate raw
ZoneWindow.cpp/ZonesOverlay.cpp paths were not retrievable; no inspection or
overlay implementation claim. Lessons: abort and success END teardown differ;
overlay visibility is not input isolation; upstream manual tests do not prove
PaneBind behavior. No source/examples/control flow copied/adapted/translated.
Attribution for both projects: retain research links; no new code obligation.

Microsoft Learn live official primary applicable content actually read this
round; no immutable revision asserted, Microsoft site terms, paraphrases/links
only, no examples reused:

- [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)
- [CreateWindowExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw)
- [WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint)
- [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- [Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles)
- [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)
- [WM_MOUSEACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate)
- [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect)
- [PtInRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-ptinrect)
- [DestroyWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow)
- [IsWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow)
- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)

Lessons: SendInput has no HWND target/delivery guarantee; nonactivation style
is not a substitute for real foreground/focus proof; rect coverage/topmost are
not actual root ownership; hidden/disabled/static/transparent targets cannot
be assumed isolating. Shield stays outside current source P with half-open
boundaries and event-triggered maintenance; no timer/retry source writes.
Same creating-thread exact-generation teardown is required. Research PASS is
only the bounded test-owned design; real shield/gates and future legacy mouse
delivery to underlying windows remain NOT TESTED. No product shield/input
implementation or external code reuse authorized.

## R1-C4B Architecture Pivot 1 Fix F — 2026-09-27

BASE `a20f32b667db601207f5675653bbc55520bf9314`；subsystem: test-only abort
failure snapshot / DOWN ledger / owner quiescence / owned-native cleanup UP。
See [Fix F abort cleanup research](R1C4B_PIVOT1_FIXF_ABORT_CLEANUP.md)。
研究门仅 bounded independently instrumented design，非 cleanup empirical PASS。
旧 D/E ignored evidence六文件实际重hash匹配，失败117行只读复核；未GUI/input/
CTest/build/Git写，未改历史verdict或原artifact。

AltSnap：mature maintained reference，GPL-3.0-or-later，REFERENCE ONLY。
本轮核验本地 clean HEAD `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`，
实际读取 `License.txt` opening、`hooks.c` GPL header，以及 scoped
FinishMovementNow/Async、STATE_UP forwarding/block bookkeeping、GRAB_TIMER
synthetic button-UP。完整读取本地 history diff
[034d58bf140552fa520e078d3c832735c2fe708a](https://github.com/RamonUnch/AltSnap/commit/034d58bf140552fa520e078d3c832735c2fe708a)
（long-click button-UP ordering），
[8a5c422928d37d34e63cb8b99d4a74e14cb955f0](https://github.com/RamonUnch/AltSnap/commit/8a5c422928d37d34e63cb8b99d4a74e14cb955f0)
（disable不free hooks DLL），以及
[issue572 body](https://github.com/RamonUnch/AltSnap/issues/572)。
Repository/pin: [RamonUnch/AltSnap](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d)。
Lessons: pending input/work/lifetime必须分别终结；synthetic/global-hook方案不是
本轮安全权限证明，跨工具报告不识别本机干扰actor。GPL实现/结构/控制流未复制、
翻译、适配或机械派生；Code copied=NO，Code adapted=NO。Required attribution:
保留研究链接，无新增代码义务；任何未来复用须先独立许可决定。

PowerToys/FancyZones：mature production reference，MIT，REFERENCE ONLY。
实际读取 pin `19c4d805321db86f3634e6968e14dbf25cbba14a` 的完整
[LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE)
与完整
[WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)，
[PR48569 body/commit discussion](https://github.com/microsoft/PowerToys/pull/48569)
和 [dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
rendered destroy-dispatch/Abort/reset/tagging diff。
Repository: [microsoft/PowerToys](https://github.com/microsoft/PowerToys)。
Lessons: abort不应snap失效HWND；资源cleanup、输入终结和成功placement不同；
upstreammanual测试不是PaneBind输入证明。Code copied=NO，Code adapted=NO，
未复制示例/控制流；Required attribution:研究链接保留，无新增代码义务，未来
显式MIT复用须保留license/notice。未声称本轮重读全部FancyZonesApp或其它模块。

Microsoft Learn / Win32：official platform contracts，live pages实际读取适用
段落，review date2026-09-27；无immutable revision；Microsoft网站terms，
paraphrase/link only，Code/sample copied=NO，adapted=NO。Issues/PRs=N/A。
实际URLs：

- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
- [SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture)
- [GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)
- [GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo)
- [GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo)
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
- [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)
- [RAWINPUTHEADER](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputheader)
- [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input)
- [GetWindowThreadProcessId](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid)
- [GetWindowLongPtrW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw)
- [OpenInputDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openinputdesktop)
- [GetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getthreaddesktop)
- [GetUserObjectInformationW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectinformationw)
- [GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)
- [WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint)
- [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)
- [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
- [Event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)
- [Out-of-context hooks](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)

Lessons/subsystem: capture与actualsource root只是fresh接收前提，非原子SendInput
target transaction；GetCapture为thread-local，GUI2允许仅独立cleanup候选，不
放宽Product；Rawtransition与DOWN command账本必须关联，tag/device非actor认证；
async0可能context失效；native/WinEvent END与cleanup Raw必须scope隔离并实际观察。
Required attribution: official研究链接保留，无示例或代码复用。误拼RAWMOUSE URL
不可访问，未作为实际inspected来源或证据；使用上列有效primary页。

Fix F current-input checker补充实读（2026-09-27）：
[OpenProcessToken](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocesstoken)、
[GetTokenInformation](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation)、
[TOKEN_MANDATORY_LABEL](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-token_mandatory_label)。
Exact source: Microsoft Learn上述API页；用途为只读caller/foreground IL上下文
验证，TOKEN_QUERY及PROCESS_QUERY_LIMITED_INFORMATION，不读取用户内容，
不调整token/权限或Windows设置。Copied/adapted code: NO；official links保留。

## R1-C4B Architecture Pivot 1 Fix G — 2026-09-27

BASE `7d026c2933acef865b23edd9587098d612098a31`。仅 test-only bootstrap
prefix classification、runner finalization、readonly foreground GUI readiness；
不重新设计 product handoff、geometry writer 或 owned-native abort cleanup。
详见 [Fix G execution report](../reports/R1C4B_PIVOT1_FIXG_EXECUTION_REPORT.md)。

复用上节已经核验的 pinned research，而非声称本轮重新检查整个上游：
AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`（mature maintained，
GPL-3.0-or-later，reference-only；FinishMovementNow/Async、STATE_UP、
GRAB_TIMER，history `034d58bf140552fa520e078d3c832735c2fe708a` /
`8a5c422928d37d34e63cb8b99d4a74e14cb955f0`、issue572）；FancyZones
`19c4d805321db86f3634e6968e14dbf25cbba14a`（mature production，MIT，
reference-only；WindowMouseSnap.cpp、PR48569、history
`dd26d86580168d2e368701f7b0c4d629dc9cd9ac`）。沿用 abort/成功 placement/
资源终结分层，不将上游报告当成 PaneBind 现场观察。
Code copied/adapted/translated/derived: NO。Required attribution: 保留上述
研究链接；未引入外部代码或新的代码 notice 义务。

本轮实际补读 Microsoft Learn 五份 official primary contract 的适用内容：

- [GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo)
- [GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)
- [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
- [GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo)
- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)

Review date 2026-09-27；live pages，无 immutable revision；Microsoft site
terms，paraphrase/link only，issues/PRs N/A，sample/code copied/adapted NO。
用途与结论：GetCapture 的 NULL 只属于调用线程，不能证明 foreign capture
clear；显式 foreground TID 的 GetGUIThreadInfo 与前后 tuple 是有序而非
全桌面原子观察，前台失活可能令句柄失效。按钮 UP、flags=0 不能代替 capture
字段；查询失败保留 null/error。SetForegroundWindow 拒绝不唯一归因；SendInput
没有目标 HWND，禁止为通过测试干预第三方 capture。无新枚举器/foreign title/
path/content 读取，无 hook、输入或窗口操作扩权。
## V1 production mainline — 2026-10-15

Additional official platform checks on 2026-10-15: `WM_GETMINMAXINFO`,
`MINMAXINFO`, `SendMessageTimeoutW`, `GetUserObjectInformationW` (`UOI_IO`),
`GetGUIThreadInfo`, and `GetLayeredWindowAttributes` were read on Microsoft
Learn. The production GUI queries declared min/max track bounds with a bounded
OS-marshalled system message, outside the input callback; unknown/hung replies
reject eligibility. Arbitrary application sizing grids are not inferred from
this message. Input-desktop checks use actual `UOI_IO` plus active-session facts,
not desktop names alone. These are original adapters, not imported source.

- https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-getminmaxinfo
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-minmaxinfo
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectinformationw
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlayeredwindowattributes

Scope: user-approved [V1 contract](../PANEBIND_V1_DEVELOPMENT_GUIDE.md), not a
rewrite of the older observer/Explorer experiment permissions or verdicts.
Original PaneBind implementation; external code copied/adapted/translated: NO.

- AltSnap (mature maintained), revision 5c86416ad21e4b72844a998a746bd3bb0bee5f5d,
  GPL-3.0-or-later, reference-only: existing ALTSNAP_REVIEW and hook/worker
  provenance reused; pinned hooks.c and License.txt actually re-read, plus
  [PR609 worker history](https://github.com/RamonUnch/AltSnap/pull/609).
  Lesson: keep callback work separate from placement and own complete input
  pairs; neither its GPL implementation nor polling fallback is reused.
- PowerToys/FancyZones (mature production), revision 19c4d805321db86f3634e6968e14dbf25cbba14a,
  root LICENSE MIT, reference-only: existing FANCYZONES_REVIEW reused;
  pinned FancyZonesApp.cpp/LICENSE and actual
  [destroy-abort diff](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
  read again. Event ingress, STA owner dispatch, lifecycle invalidation and
  abort-not-commit are applicable lessons; FancyZones is not evidence that
  PaneBind's newly selected mouse route works.
- Official Microsoft contracts actually read, paraphrase only:
  [LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc),
  [SetWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw),
  [RegisterHotKey](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey),
  [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos),
  [current virtual desktop](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-iswindowoncurrentvirtualdesktop).
  Own-process LL callback, dedicated message loop, bounded handoff, negative-code
  passthrough, possible silent hook removal, real registration errors, no F12,
  actual frame postverify and explicit desktop eligibility are preserved.
  Existing geometry/frame/Resize research is reused; no foreign WM_GETMINMAXINFO
  response is presumed to reveal every application constraint.
- Guest-only Medium launch research: official
  [CreateRestrictedToken](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken),
  [DuplicateTokenEx](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-duplicatetokenex),
  [SetTokenInformation](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-settokeninformation),
  [token information](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-token_information_class),
  [CreateProcessWithTokenW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithtokenw),
  [CreateProcessAsUserW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessasuserw),
  [MIC](https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control).
  Use an already-linked standard token if available, otherwise a newly created
  LUA-restricted child token; verify actual SID/session/Medium/non-elevated
  facts before resuming that guest child. Do not modify the original token,
  host accounts, UAC or policy, or infer Medium from a requested flag.
  These are design/API facts, not yet a native test result.
- The V1 seam preview uses a generation/zone-specific, one-shot
  [SetTimer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer)
  / [KillTimer](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-killtimer)
  wait, read 2026-10-15. It never samples the cursor or calls the geometry writer.
  Queued timer messages survive cancellation; a never-reused identifier filters
  obsolete generations. This is not a resident polling input source.
- V1 visual recovery (original code, no external implementation): official
  [named-pipe security](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights),
  [client PID](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeclientprocessid),
  [server PID](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeserverprocessid),
  [layered attributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes),
  [window style mutation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw),
  and [message/event wait](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)
  were actually read on 2026-10-15. A same-executable/Medium/logon-session helper
  acknowledges prior/planned property tuples before mutation and independently
  waits on parent exit. It does not hook input, inject code, reclaim capture or
  infer unknown alpha attributes. Unknown first-layered intermediate state and
  failed bounded cleanup remain failures, not cleanup PASS.
- V1 native worker reuses the inspected AltSnap worker-dispatch lesson only;
  no source copied/adapted. One native call runs outside the GUI/input owners.
  A finite wait may retire later requests but cannot cancel an issued native
  call. Delayed completion records use the same call ID and never restore
  authority. Current guest evidence, not this design, must prove exit behavior.
- Additional original V1 event and timing integration (2026-10-15): official
  [WM_POWERBROADCAST](https://learn.microsoft.com/en-us/windows/win32/power/wm-powerbroadcast),
  [PBT_APMSUSPEND](https://learn.microsoft.com/en-us/windows/win32/power/pbt-apmsuspend)
  and [PBT_APMRESUMEAUTOMATIC](https://learn.microsoft.com/en-us/windows/win32/power/pbt-apmresumeautomatic)
  were read. Suspend retires the current gesture; automatic resume does not
  establish user presence and fresh input-desktop admission remains required.
  Official QPC/QPF and acquiring-high-resolution-timestamps guidance was also
  checked for bounded callback and hook-observed-to-postverify distributions.
  Same-machine software QPC observations are not physical display latency;
  cross-thread one-tick quantization and missing clocks are reported. No
  external implementation was copied or adapted. No sleep/resume or timing
  performance PASS is inferred from these API checks.
- The first V1 Medium native run (`87cf12b7490747669ac7ce34e7059da1`,
  `7878e573d2f6f8c09dafe75230dade48ceae366a`) observed one exact source
  placement but the test's independent `GetCursorPos` check then failed.
  The existing active route filtered every `WM_MOUSEMOVE`. Re-read official
  LowLevelMouseProc, GetCursorPos and SendInput contracts: insertion is not
  delivery and suppression blocks the rest of the hook/target route; those
  pages alone do not establish why that cursor check failed. The next bounded
  comparison observes motion but forwards it to `CallNextHookEx`, while still
  owning the matching DOWN/UP pair. No injected product input, capture or
  native-loop correction is introduced. Actual cursor/geometry/UP evidence,
  not this hypothesis, determines that candidate's verdict.
- V1 second-instance command dispatch: official
  [WM_COPYDATA](https://learn.microsoft.com/en-us/windows/win32/dataxchg/wm-copydata)
  and SendMessage contracts were read 2026-10-15 after run
  `12dca72858ad4f2088f24e724eec8d5c` completed the owned historical gesture
  sequence but rejected the synchronously dispatched Center command. The
  receiver now validates/copies the action enum and posts it to its own STA
  queue; no borrowed pointer survives WM_COPYDATA. This avoids synchronous
  cross-process message context for COM admission. The original failure did
  not record a COM HRESULT, so that specific system cause remains an inference,
  not an observed HRESULT. Delivery is reported separately from actual geometry.
- Internal compatibility-test binary, not PaneBind source reuse: Microsoft
  Visual Studio Code official Windows x64 ZIP 1.142.0, upstream commit
  `6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1`, reviewed 2026-10-15.
  [Microsoft product terms](https://code.visualstudio.com/license) permit
  internal application development/testing; this binary is not relabelled as
  the MIT Code-OSS source distribution and is not shipped in the PaneBind user
  package. Official [portable ZIP documentation](https://code.visualstudio.com/docs/setup/portable)
  and update metadata were read. ZIP SHA256
  `ACE1DA7D1432D43C1AB4B2BBF643F6494F6D25DEC07494976755EAC522CD50D9`
  matches official metadata; Code.exe version/signature were statically checked
  (valid Microsoft signature), without running it on the host. Preserve the
  entire archive's license/ThirdPartyNotices in disposable-guest extraction.
  Source modules copied/adapted/translated: NONE. The fresh portable data
  directory is test state, not a user profile migration or a product dependency.
  Asset availability/hash validation does not establish app compatibility.
- Explicit settings activation (original V1 code, 2026-10-15): read official
  [AllowSetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-allowsetforegroundwindow)
  and [SetForegroundWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow).
  A second instance opening settings transfers permission only to the existing
  product PID; the product requests activation only for its own settings UI.
  Windows may deny activation. No gesture restores foreign foreground, no
  ASFW_ANY, focus lock timeout setting or AttachThreadInput is used. Run
  `36577e33b07f4006bc9dd789b5f36c57` observed the exact settings-window check
  fail but did not identify which conjunction failed; the patch adds individual
  root/tab/foreground/focus facts. No root cause or UI PASS is inferred here.
- Bounded visual mutation / recovery ordering (original V1 code, 2026-10-15):
  re-read official SetWindowLongPtr / SetWindowPos synchronous-message and
  SWP_ASYNCWINDOWPOS contracts; style, alpha and z-order changes now share the
  existing single native executor rather than blocking the GUI/input owner.
  The recovery ordering review inspected official
  [DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle),
  [CompareObjectHandles](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-compareobjecthandles)
  and [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject).
  The exact worker completion event is duplicated with SYNCHRONIZE-only access
  to the verified same-image/Medium helper before publishing visual work. The
  synchronous acknowledgement callback does not escape into worker lifetime.
  Recovery waits for that real event; timeout/unreadable alpha is not successful
  cleanup or external replacement. No external implementation was copied or
  adapted. Event/preparation tests are not native window/hung-application PASS.
- Settings page hit routing (2026-10-15, original code): official
  [tab display-area documentation](https://learn.microsoft.com/en-us/windows/win32/controls/tab-controls),
  [tabbed dialog layout](https://learn.microsoft.com/en-us/windows/win32/controls/create-a-tabbed-dialog-box)
  and SetWindowPos were inspected. The selected child page is raised above
  its sibling tab control with NOACTIVATE; no foreign z-order is changed.
  Release run `269b8f1f2941478990f9d769d3b1d9c1` proved actual settings
  foreground/focus but rejected the first checkbox's hit before sending its
  click. The old rejection did not record the actual hit HWND/class. New
  evidence records both target and hit; the layout fix remains a candidate
  until actual clicks pass. No copied/adapted sample implementation.
- Exact-edge feedback (2026-10-15, original V1 code): reused the previously
  inspected FancyZones overlay teardown / coordinate history and AltSnap
  hollow-preview lessons above (reference only). Re-read official
  [extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles),
  [SetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes)
  and [FillRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-fillrect).
  Thin member outlines are distinct from thick, exact-postverified selected
  magnet edges / actual shared contacts. Existing nonactivating, hit-transparent
  overlay semantics are unchanged. No external code copied or adapted; native
  visual acceptance remains dependent on the matching guest run.
- Candidate occlusion policy (2026-10-15, original V1 code): revisited the
  already pinned AltSnap occlusion findings (#681/#682) above, reference only,
  plus official [EnumWindows](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumwindows),
  [GetTopWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-gettopwindow)
  and [GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow).
  Enumeration order is not assumed to prove Z order. Two finite native Z-chain
  observations and unchanged frame reads permit conservative single-frame
  containment filtering; partial coverage / unknown order is not a pixel-level
  visibility assertion. No copied/adapted GPL code or general compositor added.
- VS Code test profile (2026-10-15): read official
  [portable mode](https://code.visualstudio.com/docs/setup/portable), which
  specifies that a program-local `data` folder overrides explicit user-data /
  extensions command-line directories. New guest bundles no longer create that
  competing bootstrap-owned folder; the Medium fixture uses its own fresh CLI
  profile. Run `8cc920c8b1584531b248e602b672d186` observed only hidden Code roots
  and one `0x80070005` child exit. The conflicting profile setup is corrected,
  but that exit's root cause / Code compatibility is not presumed proved.
- Exact foreground-console GUI observation (2026-10-15, original V1 code):
  revisited the pinned reference-only GUI/capture research above and official
  [GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo)
  / [GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo).
  An owner without a GUI queue can fail; thread ID zero queries the actual
  foreground queue. Only an exact foreground `ConsoleWindowClass` may use a
  successful zero-ID query with matching actual active HWND, unchanged native
  PID/TID and matching foreground observations. Nonempty capture/move/menu and
  unknown results remain rejected; ordered reads are not an atomic desktop
  snapshot. Run `3e5d9c491c5642efb7ca02ce82d6db55` observed owner-query failure
  on the bound cmd frame. The narrow fallback is a candidate pending native
  validation, not evidence of compatibility. No copied/adapted source code,
  input attachment, foreground control or new window-operation authority.
- Click-versus-drag candidate retirement (2026-10-15, original V1 code): read
  official [mouse input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-mouse-input),
  [DragDetect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dragdetect)
  and [GetSystemMetrics](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics).
  The system defines separate double-click and held-button drag tolerances.
  PaneBind retires its click candidate monotonically once held motion exceeds
  the actual system drag tolerance; it does not call blocking/capturing
  DragDetect. Genuine unmoved clicks retain the existing double-click routes.
  Run `aae67076ccda4bdb9300c9015108231a` observed a real 80-pixel trial drag,
  then an unclaimed same-point START after a short interval; its logs did not
  directly observe the callback's `clicks=2`. The source counterexample explains
  that possible pass-through without claiming Windows' internal recognizer
  has the same state machine. No external code copied/adapted.
- Owned batch-loss test (2026-10-15, original test code): read official
  [DestroyWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow),
  [WM_WINDOWPOSCHANGING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)
  and [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos).
  The exact empty test window destroys only itself, on its owner thread,
  during the second actual batch sizing callback. No API-result override is
  added. Outer FALSE and outer TRUE with subsequent identity loss are separate
  observed outcomes, not a documented guarantee that destruction returns FALSE.
  The separate one-shot guest input actor likewise must actually exit and leave
  the button held before testing the unchanged 30-second product deadline;
  neither an exit code nor test cleanup UP is product termination evidence.
  No external source code copied/adapted; native cases remain unexecuted until
  their matching new-build guest receipts are reviewed.
- Failure-time cursor observation (2026-10-15): read official
  [GetClipCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipcursor).
  Its successful rectangle is the current confinement, or screen dimensions
  when unconfined; it is read-only. A single failed test cursor observation now
  includes that rectangle and actual foreground class/PID/TID and GUI facts.
  It never calls ClipCursor, retakes foreground or releases foreign capture.
  Run `92b2b41ea7f946bebca7a519a3c79831` had a real FocusLost terminal event
  followed by unsuccessful exact cursor observation. This does not establish
  which external/system component caused either observation. No external code.
- Hook observations versus test input bookkeeping (2026-10-15, original V1
  code): read official
  [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate),
  [LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)
  and [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).
  SendInput's return counts inserted events, not product/target delivery. The
  low-level mouse callback observes an event before target-queue posting and
  may suppress its onward delivery. No inspected contract guarantees that a
  consumed DOWN makes another thread's async button high bit true; async zero
  also has documented access/desktop failure meanings. Actual normalized
  DOWN/UP receipts and driver send/QPC bookkeeping are recorded separately,
  with async SHORT values retained only as readings. Run
  `8453f79385e64ca29c2a73eeb4bc305c` observed one real nonexact Call42 and live
  generation authority at its completion; the later combined test assertion
  did not record each operand and does not prove why it failed. This repairs
  a test-evidence assumption, not a claimed Windows/product root cause.
  Previously pinned AltSnap/FancyZones research remains reused; no new
  upstream source inspection or copied/adapted implementation is claimed.
- Aborted-pair resynchronization (2026-10-15, original V1 decision): read
  official [MSLLHOOKSTRUCT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msllhookstruct)
  and [GetMessageTime](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagetime).
  The hook packet contains a message timestamp, not a gesture identity; native
  timestamp differences must account for wrap rather than JSONL line order.
  Only an already aborted/revoked pair may retire on a current same-button
  actual DOWN. New preparation and routing remain mandatory, and insufficient
  preparation leaves the whole new pair native. An explicitly older UP cannot
  complete/revoke the current pair; it is not broadly consumed as an unknown
  event. A later-injected UP carrying a new timestamp cannot be proven to belong
  to an old gesture. This does not synthesize UP or explain the absent callback
  in prior guest evidence. Pinned prior-art findings are reused without a new
  source-inspection claim; no external implementation copied or adapted.
- Revocation dispatch/hover follow-up (2026-10-15, original V1 integration):
  the preceding timestamp references remain applicable. GUI hard revocation
  sets authority to zero before its queued owner abort is dispatched. Pair
  retirement therefore uses that actual zero authority, not the later abort
  flag; a live authorized pair remains ineligible. MOVE while awaiting the
  revoked pair's UP also requests normal GUI preparation, whose active-gesture
  guard and fresh-identity checks remain unchanged. Neither action creates an
  UP, restores an old generation, or establishes why a previous callback was
  absent. No additional API, input source, upstream code or retry loop added.

### V1 bounded missing-UP review — 2026-10-16

- AltSnap, mature maintained, repository <https://github.com/RamonUnch/AltSnap>,
  exact local revision `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`;
  `License.txt` and source headers: GPL-3.0-or-later, **reference only**.
  Re-read `hooks.c` timer dispatch around 5715–5726 and the actual bodies and
  discussion of [#532](https://github.com/RamonUnch/AltSnap/issues/532),
  [#545](https://github.com/RamonUnch/AltSnap/issues/545), and worker
  [PR #609](https://github.com/RamonUnch/AltSnap/pull/609).
  These report intermittent interaction/button-state failures and historical
  worker/rehook mitigations, not a demonstrated cause of PaneBind's missing
  callback. Its five-second rehook polling is **not adopted**.
- Microsoft PowerToys/FancyZones, mature production, repository
  <https://github.com/microsoft/PowerToys>; existing source/license pin
  `19c4d805321db86f3634e6968e14dbf25cbba14a`, root license MIT, reference only.
  Re-read [#50130](https://github.com/microsoft/PowerToys/issues/50130),
  [PR #49985](https://github.com/microsoft/PowerToys/pull/49985), and its
  history page for `d68980a81bb8de144bdec998a114e948bf68c563`.
  The issue discussion attributes a keyboard-state regression to suppressing
  the same input needed by its own Raw Input handler; this is a warning not to
  presume independent LL/Raw delivery. The large commit page did not expose
  its cited source hunk to this inspection; no hunk-level verification or
  source reuse is claimed.
- Official pages actually read:
  [LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc),
  [CallNextHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-callnexthookex),
  [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput),
  [Raw Input overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input),
  [RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices),
  [GetRegisteredRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getregisteredrawinputdevices),
  [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input),
  [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse).
  Existing callback measurements exclude downstream `CallNextHookEx` time and
  time before callback entry; they cannot establish either timeout or UIPI as
  this failure's cause. Historical MVP guest logs actually contain SendInput
  followed by Raw UP, but did not use this V1 consumed-DOWN route. A bounded,
  fixed-atomic mouse observer in the Medium guest fixture is diagnostic only:
  no synthetic receipt, generation authority, product termination, rehooking,
  or geometry behavior is added. Zero Raw observations remain UNKNOWN, not a
  product failure. External implementation copied/adapted/translated: **NO**.

### V1 multiline settings keyboard navigation — 2026-10-16

- Reused the existing pinned AltSnap/FancyZones UI/input research; no new
  upstream implementation was inspected or reused for this focused repair.
- Official pages actually read:
  [IsDialogMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-isdialogmessagew),
  [WM_GETDLGCODE](https://learn.microsoft.com/en-us/windows/win32/dlgbox/wm-getdlgcode),
  [About Edit Controls](https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls),
  and [Edit Control Styles](https://learn.microsoft.com/en-us/windows/win32/controls/edit-control-styles).
  Multiline EDIT reports DLGC_WANTALLKEYS; IsDialogMessage consults this
  contract before handling dialog navigation. PaneBind's edits have a custom
  scrolling-page parent, not a dialog procedure. After Add Rule explicitly
  focuses the rules edit, the prior code delegated neither Tab nor Escape to
  the settings-root navigation path. Run `d815b5dcbfe84ecc9fe69516b137b9b1`
  observed a real successful rule notice/Enter close followed by unmoved Tab
  and unsuccessful Escape; no actual save is inferred from that failure.
- Original PaneBind repair: only the three owned multiline mapping/rule edits
  yield Tab and Escape through WM_GETDLGCODE to the existing IsDialogMessage
  path. Enter/newline and ordinary edit behavior are unchanged. Existing real
  Tab-to-save input remains; a real Escape-from-rules-edit check is added to
  the guarded guest case. Compilation is not native delivery evidence; that
  new implementation still requires a new-identity guest run. External code
  copied, adapted, translated or mechanically rewritten: **NO**.

### V1 owned native-Move test sequencing — 2026-10-16

- Reused the existing pinned AltSnap/FancyZones input research; no additional
  upstream implementation or new external code was inspected or reused.
- Official pages actually read:
  [WM_ENTERSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-entersizemove),
  [WM_NCLBUTTONDOWN](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nclbuttondown),
  and [DragDetect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dragdetect).
  WM_ENTERSIZEMOVE reports actual modal-loop entry; nonclient DOWN is posted
  and DefWindowProc decides the applicable action. DragDetect documents the
  configurable system drag rectangle, but does not prove that a particular
  DefWindowProc implementation used that function in the failed run.
- Run `3b0011d2ece5475db6782f96434703e3`, source
  `f796d26e9678ac729f2a35abe50a0fbae4bc3d29`, recorded one DOWN at fixture
  sequence 263 followed by a 3.5-second START wait failure at 264. The driver
  never sent its planned motion before that wait. This proves a missing test
  stimulus, not the platform cause of the missing START or later empty client
  rectangle. The original driver failure remains unchanged evidence.
- Original guest-only repair: acquire the anchor after exact foreground, then
  recheck actual hit/HTCAPTION, original geometry and idle GUI facts; send a
  bounded real displacement beyond the recorded drag metric before waiting
  for actual START, followed by real destination motion, exact geometry, one
  UP and actual END. Both exclusion and paused-tray checks still require zero
  product gesture starts and placements. Failure cleanup is one unsent UP
  attempt plus bounded actual GUI-idle observation; its send is not delivery,
  NormalUp or a functional PASS. No production input behavior changed.
  External code copied, adapted, translated or mechanically rewritten: **NO**.

### V1 current-route Raw observation follow-up — 2026-10-16

- Actual `4dee2fc` / Run `43b7fa0483384156ab1a4f5a8dfc2057` registered
  and read back the fixture-only INPUTSINK mouse target. Five unmapped
  client button pairs each had actual client DOWN/UP delivery and matching
  per-button Raw counter increases. Two ordinary generation-1 Moves in
  different product instances also observed both LL UP and Raw UP with normal
  termination. These are software-input guest observations, not hardware or
  product Raw-receiver evidence, and do not explain the older missing LL UP.
- The same observer is now enabled after the identical exact-guest guards
  for Full as well as IndependentEntry. The existing Full gestures and
  single-UP assertions remain unchanged. This allows one discriminating
  comparison with an actual missing-UP interval, rather than repeating an
  uninstrumented failing batch or assuming Raw independence from hook
  suppression. No new product API, authority, synthetic UP or polling is
  added. Source/generation attribution remains UNKNOWN; an interval counter
  is not itself permission to end a product gesture.
- Official Raw Input and upstream input references are those actually read
  and recorded above; no additional external implementation was inspected,
  copied, adapted, translated or mechanically rewritten for this change.

### V1 actual multiline-key navigation follow-up — 2026-10-16

- Run `43b7fa0483384156ab1a4f5a8dfc2057`, source
  `4dee2fcfccef7d293702bc5df9a957cd07705ee0`, retained the earlier targeted
  WM_GETDLGCODE adjustment, but actual mouse-edit input at sequence270 still
  led to Tab-to-save and later Escape failures. Its original records do not
  include the WM_GETDLGCODE arguments or per-Tab focus transitions, so they
  do not establish the precise native query/dispatch responsible. Earlier
  rules-edit success is not extrapolated to the mouse editor.
- Official pages actually read:
  [GetNextDlgTabItem](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getnextdlgtabitem),
  [SetFocus](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus),
  [WM_GETDLGCODE](https://learn.microsoft.com/en-us/windows/win32/dlgbox/wm-getdlgcode),
  and [About Edit Controls](https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls).
  The first API searches actual visible/enabled tab stops and recursively
  searches WS_EX_CONTROLPARENT children. SetFocus is used only within this
  same UI thread and already-active settings window, never to recover foreign
  focus or foreground. The old adjustment only negotiated EDIT's requested
  keys; it did not explicitly navigate the root from an actual queued key.
- Original minimal follow-up: the normal STA translate path handles only
  actual WM_KEYDOWN Tab/Escape when the exact current focus is one of the
  three settings multiline edits. Plain/Shift-Tab follows the actual root
  tab order; plain Escape cancels the draft. Ctrl-Tab retains page switching,
  and Enter/newlines and ordinary editing remain native. The guest still
  sends actual keys and requires real focus change, save and window exit;
  one failure-only readback records focus/class and bounded dialog-code
  queries without pretending those queries prove key delivery. New native
  revalidation remains required. Existing pinned prior art is reused;
  external code copied/adapted/translated/mechanically rewritten: **NO**.

### V1 caption/system-button exclusion follow-up — 2026-10-16

- Actual guest `fdfe8a79403342259401aad1b6a45b7c`, source
  `783078111ebee3d17cdaa80244180e0f4b4f155b`, retained a fresh explicit
  WM_NCHITTEST result HTCAPTION at `[151,89]`, followed by the same source's
  actual WM_NCLBUTTONDOWN wParam HTMINBUTTON and no SC_MOVE. The original
  record contains no NC lParam or DWM button bounds; it does not establish
  identical message coordinates, a DPI root cause, or a DWM fix PASS.
- Re-read mature GPL-3.0-or-later AltSnap (reference-only), repository
  <https://github.com/RamonUnch/AltSnap>, exact commit
  `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`: `hooks.c` caption classification
  and `unfuck.h` caption-bounds query; inspected actual path history commit
  `9fe1e8c44e8081b3a13650eeca2d51a25f56b4aa` (2022-07-23, caption detection
  repair). Lesson only: a positive caption query needs independent system
  button exclusion. Re-read mature MIT PowerToys/FancyZones, repository
  <https://github.com/microsoft/PowerToys>, exact commit
  `19c4d805321db86f3634e6968e14dbf25cbba14a`, `WindowMouseSnap.cpp`: its
  move-size/cursor heuristic is not proof of PaneBind's pre-DOWN caption
  classification. No external code copied, adapted, translated or derived;
  no new attribution obligation. PaneBind's raw-rectangle decision is an
  original implementation from the official coordinate contract.
- Actually re-read Microsoft live primary documentation (no immutable
  revision; Microsoft site terms; links/paraphrase only, no sample reuse):
  [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest),
  [WM_NCLBUTTONDOWN](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nclbuttondown),
  [DWMWINDOWATTRIBUTE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute),
  [DwmDefWindowProc](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmdefwindowproc),
  [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect),
  [mixed-mode DPI](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-improvements-for-desktop-applications),
  [GetWindowDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowdpiawarenesscontext),
  [LogicalToPhysicalPointForPerMonitorDPI](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-logicaltophysicalpointforpermonitordpi).
  Button bounds are window-relative; GetWindowRect may be virtualized, and
  visible-frame bounds are not a substitute positioning origin. A missing or
  invalid DWM reply is not evidence that standard buttons are absent. Custom
  frames' DwmDefWindowProc guidance is for their own message handling, not
  authority to call a foreign procedure or change its message semantics.
- Product preparation and guest caption selection now use the same small
  pure exclusion decision, with checked signed arithmetic and raw-edge
  validation. Standard-button windows with unknown bounds fail closed;
  explicitly button-less custom styles retain their existing caption query
  path. The test observer records actual NC signed coordinates, receiver and
  window DPI contexts and at-message DWM facts without altering messages.
  Snapshot/query contexts are labelled separately from the NC sender.
  Implementation/offline checks do not award native interaction PASS.

### V1 bounded native-file-dialog driver readiness — 2026-10-16

- Run `b4eeef9c5c1d41f6bcd90ef6461f4240` source
  `2eb186ca6bf095c7df98d8d1f5a5708259600ae5`: exact native export dialog
  title/owner/PID/TID passed at sequence355, then immediate filename-child
  enumeration failed at356. This does not establish a product select_file
  defect or prove whether the child was absent, late or differently arranged.
- Actually read Microsoft's
  [Open and Save As dialog boxes](https://learn.microsoft.com/en-us/windows/win32/dlgbox/open-and-save-as-dialog-boxes),
  [Windows dialog shortcuts](https://support.microsoft.com/en-us/accessibility/windows/keyboard-shortcuts-in-windows),
  and [Visio Open/Save dialog shortcuts](https://support.microsoft.com/en-us/accessibility/visio/keyboard-shortcuts-for-visio).
  The control-ID documentation distinguishes edt1 and cmb13; the shortcut
  source describes Alt+N for the filename field, not a guarantee for every
  localized/custom dialog. Actual focus and the resulting file remain tested.
- Original guest-only repair waits a bounded1500ms for a unique documented
  filename edit; otherwise sends Alt+N once and requires actual editable
  EDIT focus under the already-confirmed foreground modal, same PID/TID.
  Real Unicode input/readback/Enter and subsequent file parsing remain; no
  WM_SETTEXT, direct save API or successful-send delivery inference. Failure
  retains its original verdict, records at most128 class/ID/style/ancestor
  entries without text if unresolved, and only attempts real Escape on that
  same exact foreground file modal before verifying return to own settings.
  Existing pinned prior art reused; no external implementation copied,
  adapted, translated or mechanically rewritten. New runtime test pending.
- Native filename readback follow-up (2026-10-16): run
  `fdfe8a79403342259401aad1b6a45b7c` source
  `783078111ebee3d17cdaa80244180e0f4b4f155b` identified the actual editable
  filename focus via Alt+N (sequence356), but its single immediate text read
  failed the exact-path check361; local real Escape returned to own settings
  at362. The record does not prove a unique system cause. Actually re-read
  [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw)
  and [WM_GETTEXT](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-gettext).
  A successful input insertion is not evidence that queued Unicode input has
  already updated the edit. The same guest driver now waits at most1500ms
  for the actual exact text, with each read bounded100ms and preceded by
  precise modal/focus/PID/TID/foreground/editable checks. It sends the path
  once, never retries typing, and failure records only copied length/read
  error rather than text. The original failure and local-cancel contract
  remain. This is test-driver sequencing, not a product save change or a
  claimed native PASS. No external implementation copied or adapted.

### V1 exact readonly-legend navigation correction — 2026-10-16

- Actual run `3e478afcf62d4535a7030cf3b98d8451`, source
  `14b6603746c2d97a2e11ec152b7f327c11a0a4d8`: the mapped mouse editor was
  HWND524982 (sequence264), but both failure snapshots271/283 showed focus
  HWND524978, class EDIT, and bounded WM_GETDLGCODE Tab/Escape returns141.
  Source inspection found a fourth multiline tab stop after mouse_text:
  the readonly public action legend. Its HWND was only a discarded local;
  both the dialog-code override and actual-key navigation enumerated only
  hotkey_text/mouse_text/rules_text. Thus moving out of the mouse editor did
  not prove the following readonly edit could yield Tab/Escape. This is a
  concrete omitted product control, not a demonstrated system input cause.
- Reused the official GetNextDlgTabItem, SetFocus, WM_GETDLGCODE and edit
  control contracts actually read in the preceding entry. Original repair:
  retain this exact fourth HWND and include it in both existing bounded
  navigation decisions; no style-based generic control framework or foreign
  focus operation. The existing GUI case now actually Tabs from mouse_text
  to the readonly legend, compares only its fixed public heading, and sends
  real Escape to cancel the modified draft. M09's existing real Tab-to-Save
  and Enter checks continue to exercise navigation through the legend.
- A failure-only snapshot adds ES_READONLY/ES_MULTILINE and a boolean match
  against at most24 characters of that fixed public heading. It records no
  custom editor text. Original failures remain unchanged; compiling this
  change does not award runtime PASS. Existing pinned prior art is reused;
  external code copied/adapted/translated/mechanically rewritten: **NO**.

### V1 notification registration receipt — 2026-10-16

- Actual source `783078111ebee3d17cdaa80244180e0f4b4f155b`, RunId
  `fdfe8a79403342259401aad1b6a45b7c`, fixture sequence420 returned E_FAIL
  from the exact own icon's Shell_NotifyIconGetRect. It does not establish
  whether NIM_ADD itself succeeded. The production registration call had
  discarded its BOOL result. Actually read Microsoft's
  [Shell_NotifyIconW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw)
  and [notification-area guidance](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area).
  Record the actual ADD and (only after successful ADD) SETVERSION BOOLs,
  without treating icon registration as actual reachability or menu delivery.
  No timer/retry, foreign shell manipulation, focus operation or inferred
  LastError contract was added. Existing pinned architecture references are
  unchanged; external code copied or adapted: **NO**. Runtime remains pending.

### V1 rules-editor native line breaks — 2026-10-16

- Run `945c28c08a4a43d4b6801b15706aa33d`, source
  `3a65a41e7e4c4382ddde4a343b7df30d301a4857`, actually completed the excluded
  native Move at fixture532/534/536 without a product writer. Restoring the
  original rules through real input then failed exact text readback at545;
  the old driver appended a CRLF even to `rule_count=0` and injected CR and
  LF separately. The evidence did not record the received string, so a
  unique runtime cause is not established.
- Actually read Microsoft's [Edit Control Text Operations](https://learn.microsoft.com/en-us/windows/win32/controls/edit-controls-text-operations)
  and [About Edit Controls](https://learn.microsoft.com/en-us/windows/win32/controls/about-edit-controls).
  A standard multiline edit represents a user hard line break as CRLF;
  ES_WANTRETURN permits Enter to insert it. Original test-driver correction:
  join rule rows with CRLF but no redundant final break, and use actual
  Enter once between rows rather than Unicode CR followed by Unicode LF.
  Retain exact foreground/focus/identity checks, exact readback, real Save
  and parsed-rule/next-Move assertions. No WM_SETTEXT or normalized-readback
  substitute. Bounded failure facts record character and CR/LF counts,
  never rule content. New runtime validation remains pending; prior raw
  failure is unchanged. Existing pinned architecture references reused;
  external code copied/adapted: **NO**.

### V1 Raw/LL termination feasibility closure — 2026-10-16

Run `e4a9f65c5034440f9822babf794956db`, source
`59326ce35a4f380343bdf78353087cb30d595be4`, fixture1207 observed a Raw left-UP
increment in the single-test-UP interval while generation23 had no LL-UP or
NormalUp; its Raw source/generation remained UNKNOWN. Reused the existing
mature/reference-only AltSnap GPL-3.0-or-later pin
`5c86416ad21e4b72844a998a746bd3bb0bee5f5d` and PowerToys/FancyZones MIT pin
`19c4d805321db86f3634e6968e14dbf25cbba14a` source/history registrations above;
actually re-read [AltSnap #532](https://github.com/RamonUnch/AltSnap/issues/532)
and [PowerToys #50130](https://github.com/microsoft/PowerToys/issues/50130).
The latter discusses history `d68980a81bb8de144bdec998a114e948bf68c563` /
PR49985, but the direct KeyboardInput.cpp/commit-patch retrieval failed this
time; no newly verified source hunk or mouse-pairing precedent is claimed.
Actually read Microsoft live primary contracts:
[MSLLHOOKSTRUCT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-msllhookstruct),
[RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse),
[RAWINPUTHEADER](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputheader),
[GetMessageTime](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessagetime),
[WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input),
[RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices),
[Raw Input overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input),
and [message queues](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues).
They do not specify a shared LL/Raw event ID or equality of the two message
timestamps; Raw has no source HWND/generation, LL has no device handle, and
zero Raw hDevice can be legitimate. Therefore interval-UP/current-generation
binding cannot authorize NormalUp/dock. A possible bounded, immutable-DOWN-
paired Raw abort remains a candidate only: ambiguity/staleness must not revoke
a newer generation, and it would not close the normal-termination gate.
No Raw product implementation or new empirical PASS follows from this review.

### V1 same-candidate UP chain discrimination — 2026-10-16

The continuation reuses the pinned AltSnap/FancyZones input and issue-history
reviews above (reference only; no upstream code copied). Re-read Microsoft's
[LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc),
[CallNextHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-callnexthookex),
[Hooks Overview](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-hooks),
and [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).
The documented chain puts a newly installed hook first; returning nonzero can
prevent downstream hooks from receiving that event. SendInput's insertion
count alone does not prove a callback, delivery, or product termination.

Decision: one short-lived, passive, guest-test-only hook may observe the
already-planned single UP ahead of the running product. It runs a dedicated
message loop, records bounded atomic native packet facts and CallNextHookEx's
actual return, always preserves that return, then unregisters. No disk IO,
window operation, input filtering, additional UP, product permission or
NormalUp receipt is supplied by this observation. This distinguishes a missing
guest hook-chain observation from an event seen at the head but absent in the
product; neither outcome by itself proves the system root cause. The initial
implementation is not an empirical result; actual RunIds must be recorded in
the acceptance table after execution.
Microsoft site terms; links/paraphrase only. External implementation copied,
adapted, translated or mechanically rewritten: **NO**; no new attribution.

### V1 official test applications and ordinary guest precheck — 2026-10-16

The author actually read Microsoft's [WinGet Sandbox bootstrap](https://learn.microsoft.com/en-us/windows/package-manager/winget/#install-winget-on-windows-sandbox),
[download / skip-license](https://learn.microsoft.com/en-us/windows/package-manager/winget/download)
and [Sandbox FAQ](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-faq).
The FAQ distinguishes absent inbox apps from installing a captured Appx. A
separate, disposable preparation guest may download the exact official Notepad
Store ID with networking enabled; formal product tests remain offline. This is
test-environment preparation, not an empirical application PASS or permission
to redistribute Microsoft software. WinGet v1.29.380's original signed x64
packages and notices are pinned in the local preparation receipt; source MIT,
binary/application terms remain Microsoft's. No external code is copied.

The ordinary-user feasibility precheck follows the actually read official
[NetUserAdd](https://learn.microsoft.com/en-us/windows/win32/api/lmaccess/nf-lmaccess-netuseradd),
[CreateProcessWithLogonW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithlogonw),
[GetUserObjectSecurity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectsecurity)
and [AccessCheck](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-accesscheck)
contracts. In the exact disposable guest only, it creates one unique temporary
standard account, checks the real credential token and existing station,
desktop and mapped-directory DACL access, then removes only that exact account.
It does not modify an ACL, token/session, policy, existing user or host account,
launch a GUI, send input, or establish a working ordinary Shell. DACL allowance
alone does not prove mandatory-integrity or actual child-process access.
Denied/unknown results remain environment observations, not product failures.
Existing AltSnap/FancyZones references are reused; no implementation copied,
adapted, translated or mechanically rewritten and no new attribution required.

Run b49's fresh passive head hook also missed generation26's single UP while
the independent Raw interval count advanced. Neither proves an OS cause or
source/generation attribution. The same product passed the rapid, single
DOWN/MOVE/UP insertion in8189. For one bounded driver comparison, actually read
[MOUSEINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput)
and reused the serial, non-interleaved SendInput contract above. The guarded
guest driver sends one final absolute MOVE at its actually read cursor point
then exactly one UP in one call. No UP replay, delay-based success, product
change or Raw substitute is introduced; both real head/product UP and normal
termination remain mandatory. This is an unproven test-input variant, not a
product fix or claimed root cause. External code copied/adapted: **NO**.

The next ordinary-desktop precheck follows the separately inspected
[CreateProcessWithTokenW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithtokenw)
NULL-desktop contract: the API can grant the specified user's inherited
station/desktop access. This is an explicit disposable-guest environment
change, not a read-only operation. One exact newly created standard token is
used, never a re-logon or altered host token; only a short-lived blank Win32
child is launched. Query/cleanup handles are held before the call. Actual ACL
deltas and only this new account/logon SID's recorded ACEs are checked and
removed without restoring an old whole DACL. Failure/unknown cleanup blocks
further input. A visible GUI with current-VD query is a precheck only, never
an Explorer/Shell/tray or product functional PASS. No external code copied.

For the c81 official Notepad download's `0x8a15003b`, actually inspected fixed
WinGet tag v1.29.380
[HttpClientHelper.cpp](https://raw.githubusercontent.com/microsoft/winget-cli/v1.29.380/src/AppInstallerCommonCore/HttpClientHelper.cpp)
and Microsoft's [troubleshooting/log contract](https://learn.microsoft.com/en-us/windows/package-manager/winget/troubleshooting).
That code maps both HTTP400 and some native-code-less HTTP exceptions to this
value; the agreement's region text alone does not establish a geographic
cause. One further bounded preparation run records the actual region without
changing it and retains the new tool's exact verbose logs to distinguish these
outcomes. No proxy/TLS/signature relaxation, third-party Store downloader or
application-code reuse is authorized or introduced.

The e0 preparation's actual verbose trace now shows `Market=ZZ` returning
HTTP400; its user GeoID is World39070. A separate read-only host query observed
configured GeoID244, not a claim about physical location. Read Microsoft's
[Set-WinHomeLocation](https://learn.microsoft.com/en-us/powershell/module/international/set-winhomelocation)
contract (current-user setting). In one final, newly identified disposable
preparation guest only, fixed manifest fields permit World39070 to mirror
that configured244 for the exact official download, followed by restoration
and actual readback after the tool exits. No host/account policy, language,
proxy/TLS or security setting changes. Unknown initial/current values or
unconfirmed tool exit refuse alteration/restoration and retain failure. This
is bounded environment repair under guide2.1(4), not a functional PASS.

### V1 exact guest configuration-save rejection — 2026-10-16

Read Microsoft's [ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew),
[SetFileAttributesW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileattributesw)
and [CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)
contracts for a test-only, reversible READONLY fault on the exact new Run's
settings.ini. Keep all sharing flags on the metadata identity handle; do not
manufacture an unrelated sharing violation. Attribute setup alone is not a
save-failure result. Require actual GUI rejection, unchanged file identity and
bytes, retained draft/live policy, guarded attribute restoration and subsequent
save/restart behavior. ReplaceFile failure can have partial effects, so unknown
identity/bytes prohibit blind restoration; a native error not exported by the
product remains unobserved. This is not a DACL-permission experiment or a new
product input mechanism. Existing pinned architecture references are reused;
Microsoft documentation is linked/paraphrased only. External code copied or
adapted: **NO**. Runtime outcome must be recorded under its actual RunId.

### V1 second input candidate: consumed-DOWN capture feasibility — 2026-10-16

This is a bounded second architecture hypothesis under guide sections 2/5,
not a diagnosis of D08 or a claim that capture repairs an unobserved LL UP.
Reused the pinned AltSnap GPL reference-only and FancyZones MIT architecture,
worker, input-loss issue/PR/history reviews registered above; re-read their
local review and the existing Raw/LL feasibility closure. No upstream source
was copied, adapted, translated or mechanically rewritten.

Actually re-read Microsoft's [SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture),
[AttachThreadInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput)
and [WM_LBUTTONUP](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-lbuttonup)
contracts. SetCapture's return is the previous HWND, not success; background
capture is limited and cannot be presumed desktop-wide. ATI shares input
state, resets key state and is not a promise of independent cleanup while a
source is unresponsive. Only actual captured legacy UP can support this new
candidate's normal completion; key-state changes and unowned Raw counts cannot.

Re-read original owned Run 050736d561fe4b3c941e9f194b3f51aa normal-repeat
(SHA256 6A85F9ED355D0CBB599134AADEA84BC056293F8826D5A5F1E7A155B40F8B5650)
and source-pause
(FEC238A0ADFEA4564FD61ED653CF8F835318E751EC22B0A640BAE9850887C2F4).
Their native-END route supplies historical release/detach/withdrawal evidence,
not proof for a consumed DOWN which never entered a native modal loop.

Decision: keep the old START/cancel/END contract unchanged. Add a separate,
explicit guest-guarded consumed-DOWN resource entry, requiring an actual
immutable exact-owned LL receipt, fresh idle source/foreground/context and
generation-specific overlay/capture lifetime. The hook only compares prepared
snapshot/native packet fields, publishes and returns; no synchronous hit test,
GUI query, disk IO, COM or geometry in the callback. The feasibility probe does
not start V1 InputService, so it cannot compete for the stop hotkey or turn a
second producer into its SPSC queue. First prove actual legacy UP and a next
gesture; only then evaluate exits and cross-process product integration.
Implementation, build and old evidence do not grant this new hypothesis PASS.

Bounded admission correction (2026-10-16): actual Run
`f63d6bc443534f5c94ca637a60b412ae`, source
`6685a08d8ff3ade1f6714603eee1edf5beb4f619`, recorded an exact consumed LL DOWN,
current generation, no observed UP/revocation and valid idle context, while the
single `GetAsyncKeyState(VK_LBUTTON)` result was zero. Capture/ATI/writer were
not attempted. This identifies the rejected gate, not a system root cause.
Actually re-read Microsoft's
[LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc),
[GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)
and SetCapture pages above. A processed hook can prevent target delivery;
async zero is not an attributed actual UP, and the docs do not promise the
consumed DOWN updates that queried state. In the explicit second candidate
only, use the actual immutable DOWN receipt plus live generation-specific
revocation/UP/deadline facts for entry; retain actual other-button/context
checks. Do not claim physical hold from test-driver insertion. Any UP observed
after this active DOWN, including ambiguous/same-millisecond time, revokes new
capture authority; that observation alone cannot award NormalUp. The old
native-handoff route retains its physical-left requirement. No code copied or
adapted; actual captured legacy-UP and independent-exit gates remain unproven.

Subsequent scoped evidence (2026-10-16): Run
`f4814a3b2fc24894975adbce91a5721f`, source
`37e5ffce1290f3c8e07056083e42666859eebe0d`, actually observed two successive
consumed-DOWN owned resource lifetimes with captured legacy UP, own capture
release and input-thread detach. Its separate early-UP, actual stop-hotkey and
5-second owned-source pause / actual 3-second resource-deadline cases are
indexed in `V1_ACCEPTANCE.json`. No Move writer was started. These facts permit
integration of this second candidate, not a V1 Move/Explorer/Medium or default
30-second-deadline PASS. The production integration retains the same resource;
only an actual CaptureReady may promote pending authority. Resource receipts
return through the sole input owner, and normal completion waits for actual
legacy UP plus confirmed release/detach/removal. A separate actual registered
input-owner stop hotkey avoids claiming a duplicate registration succeeded.
No external implementation was copied or adapted.

The former terminal-motion capability was found by source inspection to allow
a new continuous placement after UP. That is not completion of an issued call
and conflicts with guide section 5.2. Remove that dispatch authority; retain
actual issued-call receipts independently and the guide's explicitly discrete
release-to-commit docking command. Prior rapid-test outcomes retain their
original binary, inputs and assertions, not a retroactive stronger verdict.

### V1 official installation-media test assets — 2026-10-16

The user supplied the existing Windows 11 Enterprise Evaluation 26200.6584
zh-CN ISO and authorized read-only inspection for missing test applications.
Only task-local copies were extracted; no ISO mount, host app installation or
virtual-machine start was performed. The retained asset inventory identifies
Notepad 11.2312.18.0, Terminal 1.18.10301.0, their resource packages and three
matching x64 dependencies. These are the actual media versions, not a claim
that they are the latest Store releases. Original WIM reference-count warnings
remain preserved.

The disposable local archive reader was obtained from the official
[7-Zip 26.04 release](https://github.com/ip7z/7zip/releases/tag/26.04)
(`ip7z/7zip`, reviewed 2026-10-16). Its `7z2604-x64.exe` digest is
`D54BF805F9F3704D1E8DB2FA3498AE7EF2DF0312B40B558E7C71C734430A665D`.
Inspected the release asset and bundled License.txt, not implementation code
or project issues/PRs. The bundled license is LGPL-2.1-or-later for the main
tool, with documented BSD-2/BSD-3 and unRAR restrictions in 7z.dll. The reader
and its license remain local together. It is an external development tool
only, not linked, copied or adapted into PaneBind; no tool is redistributed
in the product package. Applicable subsystem: local official-media inspection.
Windows SDK `IAppxFactory::CreateValidatedBlockMapReader` actually returned
S_OK for each of the ten extracted package/blockmap/signature pairs, with
separate per-file block checks. Notepad's EXE itself is NotSigned; package
verification is not relabelled as an EXE Authenticode signature. No unsigned
registration bypass or repackaged purported original MSIX is used. The assets
remain local test inputs under their existing Windows license, are not part of
PaneBind's MIT source/portable distribution, and still require actual guest
registration, activation and functional validation.

Actual registration follow-up: Run `e1fc79d4e1ca4c63b0572da1b4ef9150`
attempted the eight real manifests in its disposable guest; all failed. The
retained `appx-activity.txt` for the first VCLibs and Notepad operations records
event 709, "the manifest is not in the package root", and event 404 with native
`0x80070057`. The command reported wrapper `0x80073CF9`; its .NET exception
HResult is a separate value, not a replacement native error. These copied
directories had not been staged/registered in the guest package repository.
No activation occurred. Do not repeat this WIM-directory Register operation
or call it proof that Sandbox cannot run all packaged applications. Microsoft's
[Add-AppxPackage contract](https://learn.microsoft.com/en-us/powershell/module/appx/add-appxpackage)
distinguishes an existing/staged installation from development files, and the
[deployment troubleshooting reference](https://learn.microsoft.com/en-us/windows/win32/appxpkg/troubleshooting)
requires the associated deployment log. Notepad remains blocked on obtaining
an original, normally deployable signed package; no fake-signature repacking,
Developer Mode change or classic-Notepad substitution was performed.

Official acquisition follow-up reviewed 2026-10-16: Microsoft's
[Store Web Installer documentation](https://learn.microsoft.com/en-us/windows/apps/distribute-through-store/how-to-use-store-web-installer-for-distribution)
and [Notepad Store listing, product ID 9MSMLRH6LZF3](https://apps.microsoft.com/detail/9msmlrh6lzf3)
describe an anonymously obtainable online bootstrap installer, not a complete
offline MSIX and dependency set. The inspected listing did not supply an
original deployable package URL. Software revision/commit: **N/A**; no original
installer binary was obtained or version verified. Only the official
documentation/listing was inspected; no new installer or implementation code
was downloaded, copied, adapted or run. This inspection grants no Microsoft
software redistribution license and does not resolve the missing original
signed deployable Notepad test package. Applicable subsystem: isolated test
asset acquisition; no product behavior or acceptance result changes.

### V1 official Windows Terminal portable test asset — 2026-10-16

Inspected Microsoft's official, mature production application distribution,
[`microsoft/terminal` v1.25.2733.0](https://github.com/microsoft/terminal/releases/tag/v1.25.2733.0),
annotated tag `643609c3f9e022c9fb44366a43f5ebb8567f9722`, resolving to source
commit `dc8ae365847d41f62d1ebca93815bb00959bc12f`. GitHub reports that tag as
unsigned; `target_commitish=main` is not the exact source identity. Inspected
release metadata, fixed-tag LICENSE (MIT), archive layout/NOTICE and executable
version/signature, not Terminal implementation modules, issues or PRs. No
Terminal behavior algorithm or implementation code was copied/adapted.

The official x64 ZIP `Microsoft.WindowsTerminal_1.25.2733.0_x64.zip` is
12,127,272 bytes, SHA-256
`BF3EF2012F6C44D8340A4C58125ACC9498D19B580F9890DC043CDF831852E796`,
matching the GitHub release asset digest. Its 63 entries expand to 34,870,043
bytes. Actual `WindowsTerminal.exe` version is `1.25.2609.30003`, SHA-256
`1CBE52914F7437A4F064327383F1B4C45FE2EB8E92D4F8DAE0AD7EFCB13A20A1`;
Authenticode returned Valid with Microsoft Corporation as signer. This binary
signature is distinct from the unsigned source tag. The initial download
wrapper did not yield a final process receipt; a separate read-only verifier
checked the unchanged complete ZIP and exited zero. Neither action executed
or installed Terminal on the development host.

The official [distribution documentation](https://learn.microsoft.com/en-us/windows/terminal/distributions)
supports unpackaged ZIP/offline operation and a local `.portable` marker;
[CLI documentation](https://learn.microsoft.com/en-us/windows/terminal/command-line-arguments)
defines `--window new`, `new-tab`, `--title` and `--suppressApplicationTitle`.
The test adapter uses new per-Run/configuration guest-local directories and
retains actual Medium, baseline-exclusion, image, class, retained process
birth/tree and root-window checks. Specifying the portable asset does not
permit a failed preparation to fall back to the old ConsoleWindowClass test.
Applicable subsystem: isolated compatibility test application provisioning,
not product input or window authority. Original copyright/license/NOTICE are
retained with the internal asset; it is not included in PaneBind's source or
portable product distribution. Guest activation and functional validation for
this new asset are **NOT_RUN**, not inferred from the signature or ZIP checks.

### V1 one-shot test actor asynchronous-key-state contract — 2026-10-17

Rechecked Microsoft's official
[GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate),
[AttachThreadInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-attachthreadinput)
and [GetKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeystate)
contracts for Run `62591991e03f406ba66ced2fff997329`'s independent guest test
actor. Async high-bit state is not a matching UP receipt: zero can also result
from documented desktop/UIPI/foreground-thread access failures. The association
documentation explicitly identifies resets of GetKeyState/GetKeyboardState;
it does not establish that association caused this observed Async result.
The driver retains the actual SHORT/high-bit diagnostic, without claiming a
physical press or release. Its one TEST MOVE instead requires its single DOWN
insertion ledger, actual matching START/Ready and parent no-UP/no-END snapshot,
plus fresh exact process/window/capture/foreground facts. Native movement,
actual termination and cleanup remain separate required observations. This
changes neither product input authority nor NormalUp evidence. Revision/commit:
N/A (official API documentation, not inspected implementation code); no
external code was downloaded, copied or adapted, and no new attribution or
software redistribution right is implied.

### V1 Terminal external hit-test mismatch — 2026-10-17

Inspected Microsoft Terminal at `dc8ae365847d41f62d1ebca93815bb00959bc12f`
(MIT; LICENSE blob `017b9885a46ad4c7f4367ad2949fb66fb56f977f`), specifically
`src/cascadia/WindowsTerminal/NonClientIslandWindow.cpp` (blob
`07eba616f1db5a36c293af3edfeca498d64b638c`, root hit testing and XAML layout)
and `IslandWindow.cpp` (blob `e627bd44957545a58fbe09d0d132f6467c5bd572`,
initialization, child layout and app-initialized handling). Also inspected
[issue 8795](https://github.com/microsoft/terminal/issues/8795) and
[PR 11680](https://github.com/microsoft/terminal/pull/11680) (merged
`f2ebb21`; follow-up cherry-pick `ccc7468`): the latter explicitly does not
resolve the external WindowFromPoint/WM_NCHITTEST discrepancy because XAML's
input site handles real hit testing before the root. Confirmed the message's
screen-coordinate/result contract against Microsoft's
[WM_NCHITTEST documentation](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest).

Applicable subsystem: V1 application compatibility and guarded TEST client
input. Run `4e9eb4a7e1494054b2791c3797e78a37` actually found the exact Terminal
root at the center point but its external hit-test returned HTCAPTION. The
inspected root code can return Caption for client space; this does not prove
that the observed application content was absent or uninitialized. Keep the
existing client-click rejection and obtain a minimal guest screenshot/child
observation before choosing an adapter change. No code copied, adapted,
translated or mechanically rewritten; no additional dependency or attribution
obligation is introduced by this read-only review. Historical results remain
bound to their original product and driver.

For the single guest-driver visual observation, confirmed the bounded GDI
capture and child enumeration contracts against Microsoft's
[Capturing an Image](https://learn.microsoft.com/en-us/windows/win32/gdi/capturing-an-image)
and [EnumChildWindows](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumchildwindows).
The implementation reuses PaneBind's own Settings capture pattern, not the
documentation sample code; it records actual pixels only and grants no input
authority or application PASS. No additional external code or dependency.

### V1 ordinary-Move entry replacement and bounded application follow-up — 2026-10-17

The real fast-DOWN/MOVE/UP failures in Runs `39443acd23034e8fb7bd4eb8d1b2071a`
and `cb0b735bf4cd4ac0b46374df1bff0ab7` invalidate the assumption that consuming
ordinary DOWN before asynchronous capture readiness always preserves a short
native drag. Do not resume after-UP continuous writing to conceal that loss.
Replace that plain, non-maximized, unmodified Move entry inside the guarded
guest candidate with the already-researched native START/isolation/one cancel/
actual END entry. This is a replacement of the failed entry, not a simultaneous
writer or a new fallback experiment tree. The shared shield, capture lifecycle,
single input owner and writer remain; other mapped/Ctrl/Resize/maximized entry
paths are not silently declared repaired. Short input that ends before takeover
must remain native-only, and still needs actual geometry/UP evidence.

Reused the exact AltSnap and FancyZones source/history records above
(`5c86416ad21e4b72844a998a746bd3bb0bee5f5d`, GPL reference-only;
`19c4d805321db86f3634e6968e14dbf25cbba14a`, MIT reference-only), including
FancyZones' posted START/END handling and its non-START edge cases. Reread the
existing native-handoff implementation and Microsoft's
[event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants),
[WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)
and [SendMessageTimeoutW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw).
A successful cancel call is not an END receipt; WinEvents and mouse packets are
separate observations. Keep bounded native messaging on the GUI adapter, never
the LL owner, and keep original DOWN intent separate from actual END geometry.
No new platform API, external code reuse or license obligation is introduced.
At this record the replacement is under implementation, not a runtime PASS.

For the unchanged official Code 1.142.0 test asset, inspected the official
[CLI](https://code.visualstudio.com/docs/configure/command-line#_advanced-cli-options),
[1.40 software-renderer note](https://code.visualstudio.com/updates/v1_40#_disable-gpu-acceleration)
and [blank-UI FAQ](https://code.visualstudio.com/docs/supporting/faq#_vs-code-is-blank).
The guest driver may append the supported `--disable-gpu` option for one bounded
startup candidate. Existing `0x80070005` child exit has no recorded process type
and is not proven to be GPU-related. No `--no-sandbox`, token/security changes,
host installation or persistent Code configuration is authorized by this test;
the next actual run must report its own product/driver identities and cannot
claim strict single-variable causality across a changed PaneBind binary.

### V1 official Notepad web-installer acquisition — 2026-10-17

Actually inspected the official [Notepad Store page](https://apps.microsoft.com/detail/9msmlrh6lzf3?hl=en-US&gl=US),
its referenced `constants-B1r-ZMtw.js` and `psi-service-CIANOCrg.js`, and Microsoft's
[Store Web Installer documentation](https://learn.microsoft.com/en-us/windows/apps/distribute-through-store/how-to-use-store-web-installer-for-distribution).
The page identifies product `9MSMLRH6LZF3` and family
`Microsoft.WindowsNotepad_8wekyb3d8bbwe`; the actual scripts construct
`https://get.microsoft.com/installer/download/9MSMLRH6LZF3`. The GET returned
813,088 bytes, SHA-256 `97C2D2730D66E872CA46507437597AF9EB35B9345870B22D3353980D349BB1AB`,
with `Windows Notepad Installer.exe` in Content-Disposition. Its Authenticode
signature is Valid/Microsoft Corporation, certificate thumbprint
`0F7E3531DB084A3D45759670B7699E53985E8742`, PE version `22609.1501.4.0`.
The generic binary metadata/plain string inspection does not independently
prove its internal product binding; the observed official URL/response chain
is the acquisition evidence, not a claim to have verified an embedded payload.

This proprietary Microsoft test application is not copied/adapted into
PaneBind or included in its portable product. The downloaded file is an online
stub, not an offline MSIX; Microsoft describes installation through the Store
API/backend and prerequisite checks. It has NOT been executed. Current guest
Networking=Disable cannot supply that route. A separately approved disposable
networked preparation guest would be required; no host network/account/policy
change or installation was made. Existing media-registration failures remain
unchanged. Local original acquisition evidence, page/script hashes and byte
offsets: `uat/v1-assets/d7b77c31807f42b5827ea50a14ddd095/acquisition-evidence.txt`.
Applicable subsystem: isolated application compatibility test preparation;
no external source-code reuse or new product attribution obligation.

The bounded official offline follow-up inspected Microsoft's
[winget download contract](https://learn.microsoft.com/en-us/windows/package-manager/winget/download)
and reread the Web Installer page. The Store-packaged download contract requires
Entra authentication; `--skip-license` removes the offline-license generation
requirement, not documented proof that Notepad's package is anonymously available.
No additional download, installation, account or guest-network change was made.
The already signed stub still needs Store-backend access; a legal original
deployable package plus dependencies or a separately approved networked guest
remains the specific application-preparation gap, not a PaneBind/Home limitation.

### V1 exact application-input and driver-exit follow-up — 2026-10-17

Run `128f2aeb03494807becac60d68067cbb` retained real application failures with
product82d/driver22d. Terminal's external HTCAPTION at the chosen point did not
produce a real native START; Edge's actual HTCLIENT test with enhancements
paused did not produce the page's expected DOM effect; Code with the documented
software-renderer flag still had no accepted visible root. None establishes a
GPU, security-policy, Console or input-system root cause. Reuse the bounded
Terminal GDI observation already researched above for those exact test-created,
bound Terminal/Edge roots after the existing test UP and real GUI-idle guard.
No additional input, foreground activation or product authority is supplied.

For Code, reread [Electron's official switches](https://www.electronjs.org/docs/latest/api/command-line-switches)
and the existing Microsoft Code CLI records. A single next test launch may add
`--enable-logging=file --log-file=...` inside its new, already Medium-write-tested
UserData directory. Read only that exact startup file under the existing
8 KiB/file, 32 KiB/total test-log bounds. No stdout/stderr inheritance,
environment change, `--no-sandbox`, token/ACL/policy change or host install.
An absent file remains UNKNOWN. This is not a repair claim or external code reuse.

### V1 production resource admission versus exact guest guard — 2026-10-17

Reused the pinned AltSnap/FancyZones review records, V1 guide sections 2/4/5,
and the existing consumed-DOWN/native-first capture research above. This change
introduces no new input API or external code: it connects the already implemented
GestureInputShield/native-END/capture lifecycle to the ordinary production
entry, rather than leaving that entry on the older resource-less LL-UP route.
Resource admission is separate from Catalog/Prepared target eligibility, enabled
settings, per-generation authority and the existing real foreground, identity,
capture, geometry and UP receipts. It grants none of those window facts.

The actual owner query must succeed. An ordinary user must satisfy the existing
interactive Medium/non-elevated, active session, WinSta0/Default checks; High,
inactive and unknown owners are rejected. WDAGUtilityAccount continues through
the unchanged exact RunId/marker/manifest/path/binary-hash guard and cannot fall
back to production admission after a failed guest proof. Initialization failure
leaves enhancements paused instead of silently selecting the superseded route.
The same selector is checked in the existing offline guest-path target. This is
production wiring, not permission for host automated input or a claim of an
ordinary full-desktop runtime PASS. Current guest results retain their original
product binary identity; the changed product requires its own affected runtime
checks. Ordinary full-desktop and multi-monitor acceptance remain NOT_RUN or
environment-blocked until actually available. No new attribution is required.

The subsequent bounded source review found an actual queue-publication gap:
CaptureReady can arm a generation before its input owner queues the historical
DOWN. A real LL MOVE can then precede DOWN in the GUI queue and be discarded
without a gesture. The shared product now keeps one input-owner POD sample and
publishes DOWN before that sample, only under still-live same-generation
authority. Consumed-DOWN motion may wait for resources; native-first motion is
retained only after real CaptureReady, never borrowed from before native END.
UP/abort retires the slot without granting terminal motion permission; an
issued native completion remains separate. The existing input-route tests use
this production slot. This is a source-ordering repair, not a proven explanation
for the retained missing-UP failures or a native runtime PASS. No new input API,
polling, resource-thread disk IO or upstream code is introduced.

### V1 explicit display-transition and adjacent-command wiring — 2026-10-17

Reused the pinned AltSnap/FancyZones monitor/DPI review and existing product
Catalog/geometry bridge. Rechecked Microsoft's [MonitorFromRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromrect),
[GetMonitorInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmonitorinfow),
[GetDpiForWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdpiforwindow)
and [WM_DPICHANGED](https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged)
contracts, plus [MonitorFromWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromwindow),
[GetWindowDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowdpiawarenesscontext)
and [GetAwarenessFromDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getawarenessfromdpiawarenesscontext).
Native monitor selection uses the largest intersection with window
bounds; a window's DPI depends on its awareness, and its response to a DPI
transition may change its frame. These facts do not justify accepting arbitrary
post-call geometry or a topology change as success. Bind an intended native
destination before issue and still require actual identity, exact geometry and
fresh display/GUI facts; a second corrective placement is not introduced.

The adjacent-display command previously followed unspecified enumeration order.
PaneBind's independent policy selects the nearest rectangle gap, then contact,
centre proximity and signed bounds; it is not a cycle-through-all-screens
promise. Indistinguishable duplicate monitor rectangles are rejected rather
than picking a physical target by enumeration order. Existing pure layout tests use independent three-screen/negative-space
expectations. The guest command oracle remains an explicitly two-display test,
not a call back into the production selector. These are source/model checks,
not empirical multi-monitor or mixed-DPI PASS. No external code was copied or
adapted and no licensing/attribution obligation changes.

Also inspected the driver's retained Catalog lifetime against its existing
same-STA contract and Microsoft's [STA rules](https://learn.microsoft.com/en-us/windows/win32/com/single-threaded-apartments),
[CoUninitialize](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-couninitialize)
and [UnhookWinEvent](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwinevent).
An unconfirmed app close retained the driver-created Catalog past that STA's
CoUninitialize, allowing State destruction on the other fixture thread. Release
this read-only observer on its original driver STA before CoUninitialize;
preserve the app-close FAIL. Original 128f shutdown row158 does not prove process
exit, nor does this source defect prove the observed missing-wrapper-receipt root
cause. Actual next process exit and wrapper receipts remain required.

### V1 bounded pre-UP termination observation — 2026-10-17

Run `b1a3f530aa934059ae9950b3e35da025` preserved three exact shared-edge
placements followed by FocusLost and actual resource removal before the single
test UP. Both the passive test hook and product LL callback later observed that
UP on the retired generation; this is neither missing LL delivery nor NormalUp.
The existing record cannot distinguish the foreground WinEvent, capture-start
WinEvent and application GUI rejection paths. Add only their bounded origin and
actual GUI facts, transferred through the existing queue for GUI-owner logging.
Revoke before optional observations; do not change admission or reclaim capture.

Rechecked Microsoft's [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook)
and [GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture):
out-of-context notifications are asynchronous, while a thread-local null capture
does not establish another queue's state. No specific WinEvent is inferred as
this failure's cause. Reuse the previously reviewed event/resource design;
no new input mechanism or copied/adapted upstream implementation is introduced.

### V1 classified native-first Ctrl/Resize continuation — 2026-10-17

Reuse the same AltSnap/FancyZones pins and inspected START/END history recorded
in the native-first entry review above, not a new upstream implementation.
Rechecked Microsoft's [event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants),
[WM_NCLBUTTONDOWN](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nclbuttondown)
and [WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode).
MOVESIZE notifications cover both kinds of native interaction; they do not
classify Move versus Resize. Keep the actual DOWN hit region/modifiers, actual
source capture/move-size state and matched native START/END as separate facts.

The consumed-DOWN Ctrl/Resize route can retire its queued motion on a real UP
before the GUI consumes it, after it has already suppressed the native drag.
Extend the existing native-first candidate only to classified left single-click
plain/Ctrl Move/Resize, with unchanged mapped/maximized/bypass semantics and no
new API. Native Resize may change only its locked active edges; all inactive
edges, frame offsets, identity and display/state remain independently checked.
Freeze the original Ctrl group before the native prefix and reject changed
followers; source END geometry is B for continuity, never the A intent anchor.
The historical gate's plain-Move begin remains strict; V1 uses an explicit
platform-neutral operation classification rather than fabricating a titlebar.

Tests must distinguish a genuinely preserved short native drag from enhanced
Glue/Resize. No UP causes a new continuous call or replay, and an early native
UP does not establish product NormalUp. This functional replacement does not
identify or close b1a3's intermittent pre-UP FocusLost; f706 did not reproduce
that same failure. No external code, injection, polling, foreground recovery or
foreign-capture operation is added. Runtime acceptance remains pending.

### V1 exact-owned native passthrough evidence — 2026-10-17

The 212/c1f fast Resize reached its exact native geometry and modal END, but
neither LL observer saw that UP. Retain FAIL: generation-zero Raw input and
modal END alone are not source-UP delivery. Microsoft's [GetMsgProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc)
documents observation before a retrieved queue message returns, including the
PM_REMOVE distinction. [SetWindowsHookExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
allows a same-process, nonzero thread hook with a null module; this is not a
cross-process injection. [GetMessageExtraInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessageextrainfo)
returns queue-associated information, not a promised preserved input marker.

Add one bounded, test-only observer to the exact owned source UI thread. It
never changes MSG or the downstream return and never enters the product EXE.
Only an actually observed, unchanged, removed, marked source UP returning
before that source's actual modal END may supplement the native-only test.
Unknown facts remain failure; no queue observation grants writer authority,
product NormalUp, enhanced Glue/Resize or cross-process compatibility. Retain
four one-use callback records through driver process lifetime for unhook races.
Existing LL observation also records its fixed DOWN/MOVE/UP CallNext phases;
it does not diagnose the missing callback from absence alone. No upstream
implementation was copied or adapted; existing production prior-art pins and
license decisions are unchanged.

### V1 Code GPU launch error49: exact-source follow-up — 2026-10-20 (Asia/Shanghai)

Finite, read-only follow-up to the earlier Chromium-startup entry above. This
review interprets retained `744fff40a030404ebafe9e2879190179`,
`7c6c3d233155473ea2a3b08c57d58d3e` and
`fe0e2b7191134dc7a3dad634b8b0c983` evidence; it is not a new application run,
input experiment, runtime-version probe or repair. All three exact-profile
startup-log receipts contain GPU launch error49 and the fatal unusable-GPU
message despite `--disable-gpu`. The fe0 case recorded this without a running
PaneBind product. Original failures, driver identities and verdicts are kept.

Inspected primary sources and license/provenance boundaries:

- **Microsoft VS Code / mature production source**, exact commit
  `6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1`, version `1.142.0`:
  [`.npmrc`](https://raw.githubusercontent.com/microsoft/vscode/6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1/.npmrc)
  pins Electron `43.7.7` and Microsoft build ID `15553055`.
  [`src/main.ts`](https://raw.githubusercontent.com/microsoft/vscode/6d62b1d5dace93a33b3a6d3eca1317bd5e15bcb1/src/main.ts)
  carries the MIT source notice. Also read the existing official ZIP's exact
  `6d62b1d5da/resources/app/package.json` and `product.json` without extraction
  or execution; they identify version 1.142.0 and the same product commit.
  The official [FAQ](https://code.visualstudio.com/docs/supporting/faq)
  documents `--disable-gpu` for hardware-acceleration problems; it is live
  documentation, not an immutable source revision or proof of this failure's
  cause. The Microsoft-distributed test binary remains under the separately
  reviewed Microsoft product terms, not the Code-OSS source license.
- **Electron / mature production**, exact tag `v43.7.7`:
  [`DEPS`](https://raw.githubusercontent.com/electron/electron/v43.7.7/DEPS)
  and the [official release record](https://releases.electronjs.org/release/v43.7.7)
  bind Chromium `150.0.7871.250`.
  [`docs/api/command-line-switches.md`](https://raw.githubusercontent.com/electron/electron/v43.7.7/docs/api/command-line-switches.md)
  documents file logging, Windows child-process stderr limitations, supported
  logging controls and the explicit security effect of `--no-sandbox`.
  [License](https://raw.githubusercontent.com/electron/electron/v43.7.7/LICENSE): MIT.
  These are the matched build-configuration versions, not a new empirical
  observation of the tested process's `process.versions`.
- **Chromium / mature production**, exact tag `150.0.7871.250`,
  [BSD-3-Clause license](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/LICENSE):
  inspected [`sandbox/win/src/sandbox_types.h`](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/sandbox/win/src/sandbox_types.h),
  [`sandbox_policy_base.cc`](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/sandbox/win/src/sandbox_policy_base.cc)
  and `restricted_token_utils.cc`; `content/browser/child_process_launcher_helper_win.cc`
  and `child_process_launcher.cc`; [`content/browser/gpu/gpu_process_host.cc`](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/content/browser/gpu/gpu_process_host.cc),
  `gpu_data_manager_impl_private.cc`; [`content/browser/compositor/viz_process_transport_factory.cc`](https://raw.githubusercontent.com/chromium/chromium/150.0.7871.250/content/browser/compositor/viz_process_transport_factory.cc);
  `content/public/common/content_switches.cc` and `gpu/config/gpu_switches.cc`.
  Initial main-branch `content/public/common/result_codes.h` and sandbox-enum
  navigation did not bind the tested version; the error interpretation uses
  the pinned sandbox/launcher sources above.

At the pinned Chromium tag, result49 is
`SBOX_ERROR_CANNOT_CREATE_RESTRICTED_TOKEN`. `PolicyBase::MakeTokens` returns it
when preparing the restricted primary token fails; the Windows launcher passes
the sandbox launch result through to `GpuProcessHost::OnProcessLaunchFailed`.
It is not a Win32 error49 or a graphics-driver error code. The separate retained
child exit `0x80070005` does not by itself identify that child's role or prove
the lower-level API/GetLastError responsible for result49. Those facts remain
UNKNOWN; no specific privilege, High Shell, UIPI, ACL or host-setting cause is
inferred.

The matched GPU-mode code permits software GL/display composition after hardware
acceleration is disabled. The Viz transport still needs a GPU process for its
compositor thread, so `--disable-gpu` does not promise absence of a sandboxed
GPU/Viz child. No evidence-backed functional startup option was found in this
bounded review that avoids the observed token failure while preserving process
sandboxing, the existing token/permissions and host settings. The inspected
`--in-process-gpu` switch instead runs GPU work in a browser-process thread and
changes isolation; it is not a security-neutral candidate. Neither it nor
`--no-sandbox`/`--disable-gpu-sandbox` was enabled or proposed as an accepted
compatibility result. This is not proof that every possible supported route is
impossible, nor a reason to retry unchanged profiles or stack graphics flags.

Applicable subsystem: isolated official-application readiness and test-driver
failure classification only. Reference-only; external code copied, translated
or adapted **NO/NO/NO**. No upstream source entered PaneBind, no new dependency
or attribution obligation was created, and the existing archive notices remain
intact. No repository source, guest, token, ACL, sandbox setting or host setting
was changed by this review; functional compatibility and ordinary Medium
full-desktop acceptance still require their own actual evidence.

### V1 guest native logon identity alternative — 2026-10-20 (Asia/Shanghai)

Primary Win32 documentation inspected: [CreateProcessAsUserW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessasuserw)
and [CreateProcessWithLogonW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createprocesswithlogonw).
These are official API contract pages, not an inspected implementation or an
immutable upstream commit. The first explicitly recommends CPWL after error1314;
the second retains interactive-logon/desktop permissions, new credential logon,
profile and environment semantics. No permission, DACL, privilege or service
change is implied by that recommendation. This justifies one explicitly selected
Run-bound guest native `--build-identity` contrast, not an automatic fallback,
application input or proof of a complete Medium desktop. New LogonSID, profile,
exact image and actual output must be observed separately from the old CPAS
baseline. Documentation examples were not copied or adapted; the existing
PaneBind test launcher is extended with original P/Invoke declarations. No
external dependency/source code entered the product. Actual guest result remains
NOT_RUN until separately recorded.

### V1 disposable-guest High Contrast preparation — 2026-10-22 (Asia/Shanghai)

Applicable subsystem: the TEST-only `HighContrastOnly` fixture, not a product
theme setter or a host configuration route. Existing AltSnap reference-only
pin `5c86416ad21e4b72844a998a746bd3bb0bee5f5d` (GPL-3.0-or-later) and FancyZones
pin `19c4d805321db86f3634e6968e14dbf25cbba14a` (MIT) were reviewed through the
existing repository research for the UI/event boundary; no additional upstream
High Contrast implementation, issue or commit was inspected in this change.
No code was copied, translated or adapted, and no attribution obligation or
dependency is added. Existing native keyboard-navigation research is reused.

Primary API contracts actually read on this date:

- [HIGHCONTRASTW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-highcontrastw):
  GET fills the structure; copy the system-owned scheme and release its buffer
  with `LocalFree`. SET supplies all members. Do not combine the no-theme-change
  option with toggling High Contrast.
- [SystemParametersInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow):
  use the real API result/error and structure size. Sending a change notification
  is distinct from updating the profile; mode and color scheme are also distinct.
- [High contrast parameter](https://learn.microsoft.com/en-us/windows/win32/winauto/high-contrast-parameter):
  query at initialization/system-color change and use paired system text and
  background colors rather than assuming configured colors stay readable.

The fixture first requires its exact disposable-guest/RunId scope and an idle
product, saves actual flags/scheme/colors, uses `SPIF_SENDCHANGE` without profile
updates, and verifies exact restoration on success or failure. Unavailable or
unknown original state blocks this scene; there is no registry/theme fallback.
Native Tab/caret observations and two bounded GUI BMPs support later review,
not an inference of visual readability, animation behavior, 200% DPI or full
M10-F05 acceptance. Existing notice logs do not prove a visible notice. No HC
API, synthetic input or native experiment was executed on the developer host;
guest execution and screenshot review remain NOT_RUN at implementation time.

### V1 explicit guest callback-owner stop — 2026-10-22

Scope: one exact disposable-guest/RunId/Medium, already-active owned source;
complete the guide 5.2 callback-stop fault injection independently of the
successful injected queue-failure run `28e3f8ddc91f4c8da1328df09a75017d`.
Reuse the already-inspected AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`
(GPL-3.0-or-later, reference only) and FancyZones/PowerToys
`19c4d805321db86f3634e6968e14dbf25cbba14a` (MIT, reference only) lifecycle,
bounded callback and failure/rebuild lessons recorded above. No new upstream
implementation inspection, code copy, adaptation or license change is claimed.
In particular, do not adopt periodic rehooking or infer OS hook presence from
the public service-health Boolean.

Official platform contracts actually read for this small addition:

- [PostThreadMessageW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postthreadmessagew):
  successful posting does not prove the owner processed the message; the queue
  must exist and thread messages require explicit handling in its message pump.
- [UnhookWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex):
  preserve actual BOOL/error and handle identity. A returned success alone does
  not prove another concurrently running callback or the owner thread exited.
- [Terminating a Thread](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-thread)
  and [WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject):
  let the existing owner return normally, never use TerminateThread. Observe the
  actual thread object's signaled state/exit code separately, with a zero-time
  GUI observation after a one-shot wait notification, not a resident poll.
- [RegisterWaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-registerwaitforsingleobject)
  and [UnregisterWaitEx](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-unregisterwaitex):
  WT_EXECUTEONLYONCE still requires explicit cancellation; cancel on the GUI,
  never blocking from the same wait callback, before closing its thread handle.
- [GetExitCodeThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodethread):
  query only after the real thread wait is signaled; preserve BOOL/error/code,
  rather than treating STILL_ACTIVE or a posted completion as actual exit.

The GUI reuses the full immutable guest proof before sending one owner-stop
request. The owner rechecks generation/source/UP/current capture, retires
authority and exits its real loop with InputUnavailable (new value 9, old
values unchanged). The independent shield thread performs existing bounded
cleanup. Record actual mouse/WinEvent/hotkey/Raw/receiver teardown and actual
thread exit; failed unhook is not resources-stopped. The short-lived wait
callback only notifies the GUI; stop cancels the wait before joining/closing
its handle. The TEST driver must prove one original cleanup UP in the guest
and exact client, then public Pause/Resume and same-instance Move/Resize.
This is explicit injected owner shutdown, not natural queue exhaustion,
OS-silent hook removal, human UAT or host execution. Native results remain
NOT_RUN until matching guest evidence is collected. No external code reused.

### V1 isolated Medium desktop environment feasibility — 2026-10-22

This is a single TEST environment observation inside a disposable task guest,
not a new product input route or a relaxation of the product's Default-desktop
eligibility. Reuse the existing AltSnap/FancyZones desktop/lifecycle research
above; no new upstream source, issue, PR or commit inspection is claimed.

Actually inspected: Microsoft Sysinternals [Desktops v2.01 documentation](https://learn.microsoft.com/en-us/sysinternals/downloads/desktops),
published 2021-10-12. Classification: mature platform utility, documentation-only
reference; its public page lists Windows 7–10 and Server 2008–2022, not a Windows
11 Sandbox compatibility guarantee. No binary was downloaded or executed, no
source was available or copied/adapted, and no redistribution/license decision
is made. The useful documented distinction is separate Win32 desktop objects
and separate Explorer shells, not Task View virtual desktops. Closing a desktop
handle does not establish that its windows/processes have exited.

Official API contracts read: [CreateDesktopW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createdesktopw)
and [EnumDesktopWindows](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdesktopwindows).
Use a fresh RunId name, flags zero, inherited security descriptor and the same
verified Medium identity; an existing object must not be adopted. Request only
the observation/window rights needed for this guest. A single Explorer launch
with an explicit new desktop is not proof of a new Medium shell: actual desktop,
HWND/PID/birth/path/SID/session/integrity and Shell/Tray must be observed. Do not
replace/control the Default High shell, alter ACL/token/account/UAC settings,
or infer an interactive desktop from off-screen enumeration. No product or
synthetic input is needed for this initial feasibility observation. Failure only
blocks this environment route; guest disposal is not product cleanup evidence.

The actual c199 guest observation subsequently enumerated a visible Progman
and Shell_TrayWnd belonging to the exact new Medium child. Its auxiliary
GetThreadDesktop query failed with error 5; the old TEST's short-circuit then
skipped GetWindowRect. The default zero RECT is not a native geometry result.
The TEST correction separates actual rectangle calls, complete enumeration
membership on the still-verified created handle, and unknown thread-name
queries. A contradictory successfully read name must still reject. Read-only
Shell existence never grants product input or writing authority.

Also read the official [WaitForInputIdle](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-waitforinputidle),
[SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook),
[MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex),
[SwitchDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-switchdesktop),
[OpenInputDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openinputdesktop),
and [desktop access rights](https://learn.microsoft.com/en-us/windows/win32/winstation/desktop-security-and-access-rights).
First idle is not complete multithreaded Shell readiness. Use notifications and
one total deadline, not repeated idle calls or timer polling. The next isolated
environment variable is one activation of the new Run-owned desktop and an
independent bounded return to the original Default desktop in that same guest.
It requires normally granted READOBJECTS/SWITCHDESKTOP handles, exact names and
actual input-desktop/IO readback; no ACL/token/account changes, product start,
synthetic input, focus/capture control, or host desktop switch. A timeout cannot
cancel an already issued native API or establish cleanup success. The product's
existing Default eligibility remains unchanged. This is TEST-only preparation
under guide 2.1(4), not a new accepted product route. No external code reused.

The actual229e run verified the two Medium roots, one real activation and an
independent return to Default. It did not run product/input/tray interactions.
The next distinct TEST-only UI precondition uses the existing exact private
notification resource on that active desktop. The separate UI-only suite may
then launch the unchanged ordinary product and interact only with its verified
UI; the product's Default-only enhancement guard is not relaxed.

Also actually read [Shell_NotifyIconW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw),
[SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
and [GetUserObjectInformationW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectinformationw).
Record each ADD/SETVERSION/GetRect/DELETE result; raw GetLastError alone is not
the notification API's documented root cause. UI input is TEST-only paired
insertion, not proof of delivery. Require actual receiving-input UOI_IO, the
exact Run desktop, process/window identity and observed menu/control outcome.
Retire its short-lived input lease before the independent desktop return;
an already-issued insertion is not cancellable and must complete before that
switch, otherwise cleanup remains unknown. The original environment deadline
is5seconds/outer10; the explicit UI-only case is at most20seconds of work with
30seconds before independent return/outer35. No permanent polling, account,
token, DACL or host-desktop change. No external implementation code reused.

Actual2b49: the new exact Medium desktop and independent return succeeded,
but the own notification probe's NIM_ADD returned FALSE; raw error2147500037
is preserved, not assigned a unique system cause. No product/UI input followed.
Do not repeat that unchanged notification route or infer B27 from cleanup.
The independent next environment variable is the already reviewed official
Terminal1.25.2733.0 asset on the activated Run desktop, with the product truly
absent. Reuse the existing Terminal provenance and native preflight history:
pin the same ZIP/EXE, use its existing prepared guest directory, observe only
the new exact Medium child and complete held-desktop membership, check one
bounded WM_NULL, then request one normal WM_CLOSE and observe the bound root
disappear. No client input, product operation, Appx registration, shell setting,
token or permission change. A successful root close is not all-descendant exit
or a product compatibility PASS. This is TEST-only, with a15second independent
return/20second outer completion budget; unknowns remain unknown. No new
external source code or upstream version is copied/adapted.

Actualabb1 verified the exact new Medium modern Terminal root, native response
and a single normal root close on the activated Run desktop. It did not send
client input or start PaneBind. The two observations process-Wait=258 and a
later GetExitCode=0 are non-atomic; do not infer all processes exited.

The next TEST client case uses only this exact newly created target and the
existing independent return/input lease. Recheck actual HTCLIENT, original
source/client geometry, exact foreground/focus and idle capture/menu state;
then insert paired synthetic input and verify a unique command-produced nonce
file. No product route or eligibility is granted by those test observations.
Read-only [IsWindowOnCurrentVirtualDesktop](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-iswindowoncurrentvirtualdesktop)
HRESULT/BOOL and source/foreground GUI facts are separate from Win32 desktop
membership. A failed HRESULT remains unknown, not a fabricated true result.

Official [ReadDirectoryChangesW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw),
[CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)
and [GetOverlappedResult](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult)
were inspected for this one nonce completion: aligned retained buffer, one
OVERLAPPED event, at most four actual notifications, no fixed file poll.
Cancellation success does not imply completion; an unconfirmed native pending
buffer remains alive and cleanup UNKNOWN. An unsupported mapped watch may
choose the already-owned local Run directory before the sole input command,
not switch the oracle after a failed command. No external code reused.

The independent frozen-product UI observation does not repeat the failed own
notification probe. Observe the actual product UID1 registration/rectangle
and fresh exact Shell hit before any tray click. Its normal GUI is separately
eligible for exact own UI input; neither a failed probe nor its cleanup proves
the product icon's outcome. Product Default-only enhancement guards remain
unchanged until actual source GUI/virtual-desktop/context facts support a
separate properly verified adapter. No host permission or desktop is changed.

### V1 exact Medium input prerequisites — 2026-10-22 follow-up

Actual0072 observed both source-owner and foreground GUI facts and a successful
current-virtual-desktop query, but sent no input because the TEST root hit
oracle required HTCLIENT. Re-read Terminal tag `v1.25.2733.0` (the already pinned
MIT revision above), `NonClientIslandWindow.cpp` root hit testing and issue8795:
an external root HTCAPTION result cannot establish that its XAML client is
unusable. The separate next subcase verifies keyboard command semantics only
under an already exact foreground/focus; it does not reinterpret HTCAPTION,
claim a mouse-client click, or remove product hit/permission gates. No external
implementation code copied or adapted.

Actual630b created the frozen ordinary Medium product GUI. Its first paired
Esc SendInput inserted zero with raw error5. This does not uniquely identify
UIPI or a platform root cause. Re-read the official
[SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput),
[SetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddesktop),
[CreateDesktopW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createdesktopw),
[desktop access rights](https://learn.microsoft.com/en-us/windows/win32/winstation/desktop-security-and-access-rights)
and [thread desktop connection](https://learn.microsoft.com/en-us/windows/win32/winstation/thread-connection-to-a-desktop)
contracts. The assigned desktop handle constrains subsequent thread operations;
ordinary new-process desktop connection attempts MAXIMUM_ALLOWED. Neither
SendInput nor Shell_NotifyIcon documentation identifies a missing bit in195.
One bounded TEST comparison requests the original minimum195 plus
MAXIMUM_ALLOWED only when creating the exact fresh Run object for UI/client
input. The existing token and inherited descriptor still decide the grant;
its effective mask is unknown. No ACL, owner, token, account, host setting or
other desktop is changed. No bit-by-bit retries or security bypass is planned;
failure remains failure and cleanup is independently observed. API reference
revision N/A; no documentation implementation/sample code reused.

The next product candidate explicitly carries one immutable, exact-manifest
Run desktop context through Catalog, InputService, native writer, the shared
GestureInputShield and visual-recovery helper. This is not a rename of old
owned/Explorer tokens. Reused the existing AltSnap review's bounded candidate
enumeration and FancyZones review's reason-bearing eligibility, virtual-
desktop uncertainty and teardown-history conclusions at their registered
revisions above; no new upstream code reuse. Actual0072 establishes that this
particular Medium source's owner GUI and VDM can be queried successfully.
Two complete memberships on the held exact Run desktop plus stable HWND/PID/
TID/birth form a distinct adapter evidence source when foreign GetThreadDesktop
is unknown; a known contradictory source name or input-state still rejects.
Unknown query flags remain unknown in receipts. Medium/SID/session, active
unlocked WinSta0 owner/input UOI_IO, original VDM S_OK/TRUE, foreground, GUI,
generation, actual END and capture remain separate mandatory gates. No target
eligibility arises from a read-only desktop membership alone. Ordinary-host
and historical adapters retain their Default route; the candidate needs fresh
native verification and earns no compatibility/feature PASS from compilation.

Actual6d20 retained a real zero-area keyboard-focus proxy of class
Windows.UI.Input.InputSite.WindowClass; the positive-area TEST requirement
prevented any input. Re-read the pinned Terminal `IslandWindow.cpp` CoreInput
keyboard-routing explanation, Microsoft's
[GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo)
and [keyboard focus model](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input).
The next keyboard-only check requires the same actually queried focus identity
and unchanged actual rectangle, but not mouse-hit geometry for that proxy.
It does not claim that zero area is mandatory, that Terminal content is ready,
or that input was delivered. Only the actual new command nonce can prove
that subcase. Source/client geometry, DPI, foreground, keys and lease checks
remain; no click, activation, message injection or extra retry is introduced.
This is an independent PaneBind TEST oracle repair; no upstream code reused.

### V1 bounded tray and native-client failure discrimination — 2026-10-22

Actual26fd inserted the exact UID1 right-button pair; its later observation
found no product popup and no GUI menu owner. This does not prove the tray
callback never arrived or a menu never briefly existed. Re-read Microsoft's
[Shell_NotifyIconW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shell_notifyiconw),
[TrackPopupMenu](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu)
and [GetMenuBarInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmenubarinfo).
The existing ADD/version4 callback decoding and popup OBJID_CLIENT/0 query
match those contracts; a MODIFY does not require another SETVERSION. No
definite product or Windows root cause follows from the available facts.
The next candidate adds GUI-owner-only, fixed-limit ingress and original
menu API receipts without changing native call order/parameters, input,
foreground policy or retries. An attempted call, actual return, delivered
message and visible/selected menu remain different facts. API references have
no pinned implementation revision; no sample/source code was reused.

Actuale657 inserted the sole keyboard nonce command and Enter but did not
observe the nonce. Buffered JSONL emit ticks are not native observation times:
the recorded input and final unhook QPCs span about3.4seconds. The existing
event-driven wait now records its actual budget, native return and exit branch
once, without extending its deadline, replaying input or adding file polling.
The result remains unknown until actual command receipt; missing nonce is not
by itself proof of a Terminal initialization or product defect. These changes
reuse the already documented event/teardown design and do not copy upstream
implementation code.

### 2026-10-22 — Exact guest shell initialization versus client input

Actual ab827 records a complete three-second nonce wait with an accepted
context but no command receipt while the product was absent. This does not
establish whether the Terminal shell initialized. The reviewed official
[cmd command contract](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/cmd)
defines `/k` execution/retention, `/d` AutoRun suppression and file redirection.
The narrow TEST contrast uses an independently authored, Run-owned bootstrap
to write a separate shell-ready value; it never writes the later client nonce.
Only an actual ready observation within the existing initialization budget
permits rechecking the unchanged input guards and attempting the original
single keyboard command. The existing notification owner and nonce deadline
are retained. File reservation, initialization, API insertion and client
semantic success remain separate facts; none is pre-awarded PASS. This is
test preparation, not a product dependency or a change to another application,
account or security setting. The prior AltSnap/FancyZones research is reused;
no external implementation or sample code was copied or adapted.
