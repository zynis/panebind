# R1-C4B Pivot 1 Fix D — Product authority / test input isolation

日期：2026-09-27；起始提交：`e4ecdeaf4311e4adc95fcd94ff967df4c0f1bc83`；分支：`codex/r1c4b-live-magnet`。

`RESEARCH_GATE = PASS`：仅支持本轮独立 test-owned probe 拆分 authority/isolation，并实现受约束、事件驱动的 post-END shield 与对应离线测试。shield 的真实命中、非激活、完整 Resize takeover、重复 gate 均 `NOT TESTED`，必须新证据验收；不是产品输入设计实测 PASS。研究未运行 GUI/SendInput，未改 native/core/product，未改旧 A/B/C 文档、日志、metadata 或 verdict。

## 1. 实际 source / license / history 检查

既有 pin 沿用，但下列范围在 Fix D 重新实际读取，不把旧 inspection 冒充新 inspection；没有新外部项目或代码复用。

| 项目 | 本轮实际阅读 | 研究结论及边界 |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d)，成熟维护项目，GPL-3.0-or-later | 本地 HEAD `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`；hooks.c license header / License.txt 开头；ScrollPointedWindow、PinWindowProc / CreatePinWindow、pin 和 transparent-window teardown 的 scoped source | 非激活显示与命中/生命周期是不同职责。其 pointed-window 功能会操作其它窗口，不导入本 probe；pin timer/thunk/owner topmost 操作也不导入。 |
| AltSnap history | 本地 path history；[400eebf04dc651f76b2c1148c63fee7a4b039d8c](https://github.com/RamonUnch/AltSnap/commit/400eebf04dc651f76b2c1148c63fee7a4b039d8c) 实际完整 hooks.c diff；[issue #572](https://github.com/RamonUnch/AltSnap/issues/572) body | NOACTIVATE 指示器是实际维护决策，但其移除 NOACTIVATE 后重试 fallback 本轮禁止采用；失败必须 fail closed。issue 的跨工具 resize/snapping interaction 不是 PaneBind 观察，也不证明 shield。 |
| [PowerToys/FancyZones](https://github.com/microsoft/PowerToys/tree/19c4d805321db86f3634e6968e14dbf25cbba14a)，成熟生产参考，MIT | pin `19c4d805321db86f3634e6968e14dbf25cbba14a` 的 [LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE) 与 [WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp) 全文 | Start/Update/End/Abort、overlay/highlight/transparency cleanup 显式分离；其 restore/transparency/snapping 策略不是测试隔离或 native takeover 证明。 |
| FancyZones history | [PR #48569](https://github.com/microsoft/PowerToys/pull/48569) body；[dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac) 实际 rendered diff：destroy subscription/routing、drag Abort/reset、WindowMouseSnap cleanup | destroyed target 应 abort/teardown，不应按成功 End 做 placement；upstream manual tests 不是 PaneBind 实证。 |

两项目均 REFERENCE ONLY，未复制、翻译、改名重写、机械改写或适配 GPL/MIT implementation/control flow。保留研究链接，无新增代码 attribution obligation。尝试的 FancyZones ZoneWindow.cpp / ZonesOverlay.cpp raw paths 未取回，未声称 inspection 或从这些文件推导行为；上述可实际读取的 pinned lifecycle source/history 已满足本轮有限研究范围。

## 2. 官方平台合约：实际读取的 primary pages

以下 Microsoft Learn live pages 的适用内容本轮实际读取；不声称 immutable revision，未复制示例。

- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)：插入全局输入流，不指定目标 HWND；有 UIPI / existing-key-state 限制。返回插入数不等于目标收到，亦无必定产生 WM_INPUT 或同步更新 Raw/cursor/async state 的保证。因此 root membership 是 synthetic 测试隔离 fence，不是 source gesture authority。
- [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)：HWND_TOPMOST 是 above non-topmost，不保证压过其它 topmost；SWP_NOACTIVATE 不激活，SWP_SHOWWINDOW 显示，SWP_NOZORDER 会使 insert-after 无效。只操作 exact self-created shield；API success 仍须实际 foreground、style/geometry、hit-test readback。owner/owned-window 的 Z-order 副作用需要明确，不能借 shield 改 source 成 topmost。
- [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)：WS_EX_NOACTIVATE 顶层窗口不会因点击成为 foreground，但仍可被 explicit activation API 激活，不是禁止一切 focus/activation 的绝对保证。WS_EX_TRANSPARENT 的公开定义涉及绘制次序，不能当作输入隔离保证。
- [Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles) / [CreateWindowExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexw)：WS_POPUP 与 WS_CHILD 不可组合；popup 使用屏幕坐标，parent 参数可表示 owner；creation 会调用该窗口 WndProc。shield 使用专用自建窗口类，不用 STATIC text、message-only、disabled 或透明穿透方案。
- [WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint)：hidden / disabled window 会被跳过；static text 返回其下面的窗口。rect containment 不能替代 actual root proof。API 无 timeout 参数；本轮 bounded 指有限点数和无 retry，不宣称单次调用的延迟 SLA。
- [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)：GA_ROOT 只沿 parent 链；GA_ROOTOWNER 还沿 owner 链。应保留 GA_ROOT 判别 exact source / exact shield，不能用 root-owner 将 owned popup 伪装成 source。
- [WM_NCHITTEST](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-nchittest)：鼠标移动/按钮或 WindowFromPoint 可触发 hit test；HTTRANSPARENT 会转交同线程 underlying windows。专用 shield 应可真实命中，不返回穿透结果，不借 callback 查询 foreign 内容。
- [WM_MOUSEACTIVATE](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mouseactivate)：MA_NOACTIVATE 保留鼠标消息但不激活；与丢弃消息的 MA_NOACTIVATEANDEAT 不同。shield 不能请求 activation/focus/capture；记录真实 WM_ACTIVATE / WM_SETFOCUS 异常并 fail closed，不能以样式值代替观察。
- [GetWindowRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect) / [PtInRect](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-ptinrect)：right/bottom 是 exclusive；touching 边界可无面积交叠。P 包含 invisible resize borders，并可能 DPI virtualized；DWM V 不同。shield 非交叠裁剪使用 current positioning P，冻结 DPI/monitor/context，不混用 V 作为 source hit-test 排除边界。
- [DestroyWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow) / [IsWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-iswindow)：必须由创建窗口的线程 Destroy；owner 销毁会自动销毁 owned windows，但本轮仍需 explicit checked shield teardown / IsWindow=false 证据。HWND 可回收，IsWindow 不替代 exact self-created identity/generation。
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)：用 high bit，不能信 low bit；desktop/UIPI 条件失败也返回0。物理鼠标 button mapping 应核对；不能以 authority 失效时的0证明 release 或 NOT_NEEDED。

真实 END witness / Raw receiver 的既有官方边界沿用 [Fix C END research](R1C4B_PIVOT1_FIXC_END_DIAGNOSTICS.md) 和 [input contracts](R1C4B_PIVOT1_INPUT_CONTRACTS.md)，未声称本轮新增 product input observation。

## 3. 两份 Fix C 原始 empirical 事实：新分类不改历史

本轮只读完整 parse 两份116行 JSONL，核对 native EXIT #100、实际 WinEvent callback / matching #70–71 / #103–104、fresh final #107，并重新 SHA256；没有改日志或把旧 contract verdict 改为 PASS。报告背景见 [Fix C execution report](../reports/R1C4B_PIVOT1_FIXC_EXECUTION_REPORT.md)。

| 事实 | 初次 C | 单次 guard 修正后 C |
| --- | --- | --- |
| 日志 prefix（ignored uat/r1c4b-end-diagnostics/） | `20260926T180441083Z-Debug-BottomResize-8372500f52ef43f2aa38225205c43091` | `20260926T181314652Z-Debug-BottomResize-1e7b2ae497d54151a52f60c285730e04` |
| SHA256 | `BE6DDEC768EEA317264D1E93C327E9081E7926C5379B223E04B06C200A3D0A3E` | `F97E10307079657629EBE9F4C77C558F3DC3E102BF94280C7506AADD0929ADF1` |
| terminal/fresh P；V | `[1126,632,1766,1072]`；`[1137,632,1755,1061]` | 相同，均 exact |
| current cursor；actual root | `[1339,1083]`；`920508` | `[1339,1083]`；`33623098` |
| native / matched WinEvent END、GUI clear、held、identity/health/context | 分项均满足 | 分项均满足 |
| post-END native DRAG / unowned geometry changes / source writes | `0 / 0 / 0` | `0 / 0 / 0` |
| 唯一旧 fresh preflight failure | CursorOutsideOwnedInputGuard | CursorOutsideOwnedInputGuard |

cursor y1083 在 terminal P bottom1072 之外，却在两套 guard P 之内；仍 root 非 owned。重新按分层事实 review：`WINEVENT_END_WITNESS = PASS`、`POST_END_GEOMETRY_STABILITY = PASS`；去除 root membership 后的 ProductHandoffAuthority facts 满足，TestSyntheticInputIsolation `BLOCKED`。正式新 offline classifier 必须独立验证原始 envelope/fields，不信任旧 failure_class；该 review 不追认任何 handoff/reconciliation/Resize takeover PASS（全部 source writes0），不降级 Fix B 已观察的 Move PASS。

## 4. 两类 proof 的精确分工

`ProductGestureAuthority` 不读取 cursor root membership：exact source PID/TID/identity、desktop/visibility、global foreground exactsource、两个 matched END、GUI query/capture/menu/move-size/flags clear、held left/no Raw UP、receiver/takeover health、无 foreign capture transfer、冻结 DPI/monitor、terminal/actual P/V 可用且 stable、零 post-END native drag / unowned geometry changes 必须独立满足。cursor 获取成功仍是 target 计算能力前置，但“cursor 下必须是 source”不是产品接管权限。

`TestSyntheticInputIsolation`：记录当前/destination POINT、WindowFromPoint result / GA_ROOT、source/shield exact identity、shield nonce/generation、lifecycle、enabled/visible/noactivate/topmost facts、每点 verdict。foreign root 只留 numeric HWND 与 owned=false，不读 title、PID/path/process contents、UI tree 或 user data。isolation failure = `TestInputIsolationUnavailable`，不扩大成 product authority 或 END failure；失败 NO INPUT / NO SOURCE WRITE。

两个 verdict 都 PASS 才允许此 automated writer；不得重新合成一个掩盖区分的 healthy 字段。harness PASS 字段不构成 validator 自证，原始 facts 与不同对象 operation 需要可独立重算。

## 5. Post-END shield 与持续非交叠

shield 不存在于产品。只在本 gesture 真实 native END + matched WinEvent END + ProductGestureAuthority PASS 后，由 source UI owner 创建/配置 exact self-created WS_POPUP / WS_EX_NOACTIVATE shield；不能在 native 开始前 topmost 干扰 HTBOTTOM、capture、SIZING 或 terminal settlement。已接受的 pre-native foreground bootstrap 可以保留其临时 self-source visibility 行为，但 native 前必须恢复 topmost；与本轮新增 post-END shield 是不同阶段，不授权新 source Z-order 操作。

Bottom shield 仅覆盖 terminal source 之外的 remaining planned cursor corridor；不 fullscreen、不盖 caption、不将 source 改成 topmost。owner 对 shield 使用 HWND_TOPMOST + SWP_NOACTIVATE | SWP_SHOWWINDOW（不含 NOZORDER）；检查 return/error、geometry/style/lifecycle、GetForegroundWindow()==source、source identity/context 未变、shield 无 activation/focus。任一失败 STOP，不去改 foreign Z-order、SetForegroundWindow(shield)、SetFocus、AttachThreadInput 或重复 placement 试到成功。

持续非交叠不是仅 setup 检查：每次 source writer strict exact readback 后，由同一 owner 的该次事件处理调整 shield，一次 checked reposition，随后再证明 isolation ready；不 timer/poll/retry。下边 corridor 采用 `shield.top = max(terminal_source_P.bottom, current_actual_source_P.bottom)`，横向只围 planned cursor x±50，下边 planned max y+50+1（exclusive bound），checked arithmetic / monitor-workarea / positive extent 必须成功。若没有剩余 outside 点，可明确 NOT_NEEDED；不可猜测或扩屏。

这里的上方50px margin 必须被 source 非交叠边界裁剪，不能声称 full symmetric margin 或覆盖 source client 上方。source/shield 以 half-open P bounds 判断无面积交叠；边界touch case 和 current actual P availability/DPI 都要测。原 source 经 writer 扩入旧 fixed shield 的区域必须即时退出 shield，避免覆盖新增 source client；移动 shield 是 test isolation maintenance，不是 source geometry correction，也不是第二次 source writer。

setup 后一次有界枚举：当前 cursor + 全18个 remaining planned points，逐点 WindowFromPoint-only、不发输入。point 在 current source P 内应 root==source，否则 root==exact shield；NULL/foreign/错误identity/未覆盖点全部失败。每个 synthetic MOVE / LEFTUP 的 API 前重新验证 destination 与 fresh source/shield lifecycle、无 foreign capture/menu/move-size、foreground/desktop/input fences，不能复用 setup 的 root 快照。每次 source write 后已维护的 corridor 与下一 destination 再验证；points 变化由事件驱动，不以定时观察替代。

WindowFromPoint proof 是 point-in-time fence，不是把全局 SendInput 变成 HWND-addressed transaction：别的窗口仍可能在 proof→SendInput 间改变。所有 fresh fences、异常后立即 STOP、无 new input 的 fail-closed 证据必须保留；真实 fixture只能证明本 host 这些 run 的隔离，而非 universally race-free 产品保证。

## 6. Intent、writer、Raw UP 与 teardown

shield 不提供 source gesture/geometry/Relation/Magnet authority，不成为 anchor。保持 original native START P/V 与 original HTBOTTOM DOWN pointer，owner Raw-triggered GetCursorPos → full dy → original visible bottom+dy（left/top/right冻结）→既有 checked visible→positioning bridge →最多一次 source SWP → strict exact P/V。不得用 EXIT rect / shield rect / previous corrected rect 累加。

顺序：两个 END → ProductAuthority PASS → shield setup / all-point isolation PASS → first source reconciliation write → 每次 Raw owner quantum / source exact / shield维护 → next fresh isolated synthetic input。first source native_start_qpc 必须严格晚于 matching WinEvent END callback QPC，且晚于 isolation-ready。shield operations 独立 object/operation type，不计 source takeover write，但不能隐去其线程、时间、目标与 native success。

至少18个 remaining actual RIM_INPUTSINK movement、每 event 最多一次 source writer，Raw UP 必须真实；最后 source exact、left high bit false、无 pending movement/write。正常 teardown 在实际 Raw UP + final source acceptance 之后，由创建线程 Destroy exact shield，记录 checked success / IsWindow=false；不能只依 parent 自动销毁推定已验收 teardown。所有失败路径也仅销毁确切 own resources。

失败 cleanup 不得借 product authority 去输入 foreign root：独立 fresh TestSyntheticInputIsolation 与完整 source/desktop/foreground/GUI/input authority 能再证明，才 exactly one test LEFTUP。没有 authority 或未证明 test-owned pending DOWN，NO INPUT / SKIPPED_NO_AUTHORITY；attempted SendInput count0 是 FAILED，不是 skipped；cleanup UP 及 cleanup Raw 永不算 acceptance。NOT_NEEDED 仍需可靠初/末无 held state，不用失效 desktop 下的 async0 伪证。失败时先退休 acceptance、保留 shield/receiver 完成有限独立 cleanup 观察，然后同 owner teardown，不能先露出 underlying foreign window 再补 UP。

## 7. Tests / empirical gate / future risk

必须先 pure/synthetic 测试 brief A–K，以及 current source 扩大后 shield 仍非交叠、touch边界/裁剪margin、无 source write before isolation、foreign/null point、nonce/PID/TID stale、topmost但hit-test仍非owned、shield activation/focus、placement/teardown error、failed cleanup 不充 acceptance、raw coalescing / current cursor 语义。validator 从 raw events、两proof、planned geometry、object identity、source/shield operation、Raw lifecycle 与 teardown 独立计算。

真实执行严格 Debug BottomResize×1；完整 PASS 后 Debug Move×1（cursor 全部在 source 时 shield可NOT_NEEDED）；再 Debug5/5 + Release5/5，全 PASS 后 Debug20/20 + Release20/20。每次 fresh source/receiver/hook/shield；first failure STOP，保留 first evidence，禁止 retry-until-pass。记录 native EXIT→WinEvent END→isolation ready→handoff writer，handoff duration，Raw receipt→owner writer 的 p50/p95；未执行量不填0，无 SLA。

`POST_CANCEL_LEGACY_MOUSE_DELIVERY_TO_UNDERLYING_WINDOWS = NOT YET PRODUCT-TESTED`：真实产品未来不注入 MOVE/LEFTUP、不能以 automated shield 掩盖 capture release 后物理 legacy delivery 对 underlying windows 的副作用。此风险留待独立 Explorer/product stage；不阻止当前 owned geometry-authority probe，但 owned gate 绝不解决或证明它。当前 PRODUCT_INPUT_SHIELD=NONE、PRODUCT_RAW_INPUT=NOT_IMPLEMENTED、PRODUCT_SENDINPUT_DEPENDENCY/global mouse hook/DLL/polling 均 NONE。NO Magnet / Explorer / Human UAT；完整 owned gate 后 STOP，Explorer 至多 READY_FOR_NEXT_STAGE。
