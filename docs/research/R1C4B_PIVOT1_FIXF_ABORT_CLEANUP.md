# R1-C4B Pivot 1 Fix F — owned native abort cleanup 研究

日期：2026-09-27；BASE `a20f32b667db601207f5675653bbc55520bf9314`。
范围仅 test-only probe 的失败诊断、输入 ledger、writer quiescence 与
`TestAbortCleanupAuthority`，不是产品接管或稳定性新阶段。

`RESEARCH_GATE = PASS`，**仅允许先定义并测试独立模型，再实现有界、
fail-closed 的自有 native capture cleanup 实验**。这不是 cleanup 实测 PASS，
也不保证任意干扰下自动释放。新分支的实际 UP/Raw/EXIT/WinEvent/最终状态、
native modal loop 中的 owner ack 是否完成，均 `NOT TESTED`。
任一所需事实无法可靠取得则 NO INPUT；不得采用宽 fallback 补齐成功。

研究只读原证据和本地源码、读取 pinned upstream/官方文档；没有 GUI、
SendInput、CTest、build、Git 写入或旧 evidence 改写。本文与 provenance 是
本研究唯一文件写入。实施门还须 main 完整亲读并确认，后续真实执行由 main
按用户预登记最多八次顺序控制。

## 1. Prior art → history/license → 官方契约

两项目均 reference-only，未复制、翻译、机械改写或适配其实现/控制流。
成熟度是其实际用途与维护历史分类，不以 star 数证明可靠性。

| 参考 | 本轮实际读取的 pin / license / scope | 可用教训与不可推导事项 |
| --- | --- | --- |
| [AltSnap](https://github.com/RamonUnch/AltSnap/tree/5c86416ad21e4b72844a998a746bd3bb0bee5f5d)，成熟维护参考 | 本地 HEAD `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`、clean；`License.txt` 开头和 `hooks.c` GPL-3.0-or-later header；`FinishMovementNow/Async`、STATE_UP/forward/block bookkeeping、long-click grab timer 的 synthetic UP scoped source | 终结、尚未消费工作、button forwarding/block 状态需关联；其定时器、LL hook、synthetic lifecycle 和广泛窗口控制不是本轮模板。源码含 UP 操作不等于证明 PaneBind 的接收权。 |
| AltSnap 实际历史 | 本地完整 `hooks.c` diff [034d58bf140552fa520e078d3c832735c2fe708a](https://github.com/RamonUnch/AltSnap/commit/034d58bf140552fa520e078d3c832735c2fe708a)；完整 `altsnap.c` diff [8a5c422928d37d34e63cb8b99d4a74e14cb955f0](https://github.com/RamonUnch/AltSnap/commit/8a5c422928d37d34e63cb8b99d4a74e14cb955f0)；[issue 572](https://github.com/RamonUnch/AltSnap/issues/572) body | button-UP block 的处理顺序会影响 long-click 路径；disable 与 hooks DLL lifetime 不可混同。issue 是跨工具 resize/zone interaction 的上游报告，不是本机输入源或 cleanup 实证。 |
| [PowerToys/FancyZones](https://github.com/microsoft/PowerToys/tree/19c4d805321db86f3634e6968e14dbf25cbba14a)，成熟生产参考 | pin `19c4d805321db86f3634e6968e14dbf25cbba14a` 的 [MIT LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE) 和完整 [WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp) | `Abort` 与正常 `MoveSizeEnd` 的 placement 分开；overlay/highlight/transparency cleanup 不证明系统按钮已 up，也不证明 global SendInput 的 HWND 接收者。 |
| FancyZones 实际历史 | [PR 48569](https://github.com/microsoft/PowerToys/pull/48569) body/commit discussion；[dd26d86580168d2e368701f7b0c4d629dc9cd9ac](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac) rendered destroy-dispatch/Abort/reset/tagging diff | 窗口销毁时不能仍按正常 END snap；receipt、owner dispatch、状态退休需真实接通。其 manual test 不作为 PaneBind evidence，未沿用吞键或全局 hook 实现。 |

本轮实际读取的 Microsoft Learn live primary 适用段落如下；没有 immutable
文档 revision 或示例代码复用声明：

- [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)：插入输入流、受 UIPI 限制，不接受 HWND；返回插入数不是目标收到、按钮已更新或一定产生 WM_INPUT。数组内 serial insertion 不使此前 fresh query 与此单次调用成为原子事务。
- [SetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture) / [GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)：foreground/capture 影响路由，但其它点击仍可能改变 foreground；GetCapture 只回答当前线程，driver 的 NULL 不能证明 UI capture 清空。本轮不主动 SetCapture/ReleaseCapture。
- [GetGUIThreadInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getguithreadinfo) / [GUITHREADINFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-guithreadinfo)：查询 exact source UI TID；必须核对查询成功、capture/menu/move-size HWND，`GUI_INMOVESIZE=2` 表示 move/size loop。查询或 activation transition 不可靠时不可默认 clear。
- [GetAsyncKeyState](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getasynckeystate)：high bit 仅当前状态，low bit 不可靠；无效 desktop/UIPI 也可能返回 0；鼠标 physical/logical mapping 不可混淆。当前 high bit 和 tag 不证明 DOWN 归属；Raw DOWN 的 receipt 亦可先于 async state 更新。
- [RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse) / [RAWINPUTHEADER](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputheader) / [WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input)：button flags 是 transition，不是累计 held state；extra information 不是认证。hDevice 有无不能唯一识别 actor，precision touchpad 的 hDevice 还可能为 0。background receiver 必须真实 RIM_INPUTSINK，按注册/packet/序列/错误记录证明，不冒充 source 接收窗口。
- [GetWindowThreadProcessId](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid) / [GetWindowLongPtrW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw)：PID/TID 与实际 owned userdata nonce 联合约束本 run 的 source generation；不能只看 numeric HWND 或复制 expected nonce。只读本测试自建 HWND，不读 foreign title/path/content。
- [OpenInputDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-openinputdesktop) / [GetThreadDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getthreaddesktop) / [GetUserObjectInformationW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getuserobjectinformationw) / [GetCursorPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcursorpos)：input desktop、thread desktop、UOI_IO/成功/error 等是独立上下文事实；名称 Default 或 OpenInputDesktop 成功不能单独证明 connected/unlocked。沿用严格 desktop/session gate；不 SwitchDesktop/SetThreadDesktop 绕过失败，async 0 不作恢复证明。
- [WindowFromPoint](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint) / [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor)：当前 actual cursor 的实际 GA_ROOT 必须 exact source；位置落在旧计划或 source rect 内不是替代。hidden/disabled/static hit-test 有公开限制。
- [WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove) / [event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants) / [out-of-context hooks](https://learn.microsoft.com/en-us/windows/win32/winauto/out-of-context-hook-functions)：真实 native loop 结束与异步匹配 WinEvent END 是两条观察。callback 仍 bounded record-only；不能自行 Post/Notify END，不能从资源 Destroy 推导系统按钮释放，亦不假设两个流的固定到达顺序。

## 2. 原 117 行本机证据：只读 empirical observation

本轮重新 hash 六个原文件，精确匹配 brief；路径均位于 ignored
`uat/r1c4b-input-isolation/`，没有改写：

| 文件（既有 prefix） | SHA256 |
| --- | --- |
| 原 D `20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.jsonl` | `BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F` |
| 原 D 同 prefix `.metadata.json` | `D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09` |
| 原 E 同 prefix `.revalidation.json` | `27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F` |
| 失败 `20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f.jsonl` | `DACA83DC3AA9C5B43E7295B67E16E242526E240E5429C6C949A93C59C6CD03F8` |
| 失败同 prefix `.metadata.json` | `3F733C50D15DAFADA70D538EFFA33899ECB8C6EC32F25BFC0D28F69B89D53D16` |
| inventory `20260927T094931145Z-fixe-gates-c37c0342d5594a399685b57ceef982f2.json` | `C97A4C6D02F9B091773E9744B2025DEC0274B3A7F148444B2D67C07209D6257C` |

失败 source `HWND3806562/PID27332/TID28532/nonce118469386212298`。
下表 QPC 是日志的 receipt/API QPC，不将父 log 次序冒充系统发生次序：

| 原 row | 实际事实 |
| --- | --- |
| #66 INPUT LEFTDOWN | sent1；API `[1629130753193,1629130767075]`；point `[1339,641]` |
| #64 Raw DOWN | receiver serial7/QPC `1629130755527`，button flags1、tag match；**receipt 在上述 API 区间内且父记录早于 #66**，此 packet 的 async left=false 不否定 transition |
| #67/#76 | actual native nonclient DOWN at `[1339,641]`；native ENTER receipt `1629131242623`，父 record QPC `1629131243066`；ENTER 的 owner GetCapture 为0，不能替代后来 fresh GUI capture |
| #83 | sample1 completed QPC `1629131686091`，planned/observed `[1348,641]` |
| #84/#86/#92/#96/#100 | Raw serial9–13/QPC `1629131766881,1629131845458,1629131910719,1629132084472,1629132192318`；cursor `[1347,641]→[1343,641]→[1326,639]→[1313,639]→[1276,639]`；均 buttons0/tag false/device_handle_present true |
| #104 | `BLOCKED_BY_INPUT_INTERFERENCE`；没有该失败 fence 的完整子谓词日志，first_failed_predicate 仍 UNKNOWN |
| #105 | cleanup snapshot：source=FG=root=capture=hwndMoveSize；GUIflags2、left=true、eligible=false |
| #107 | Raw serial14/QPC `1629132298663`；cursor `[1258,639]`，同样 buttons0/tag false/device present |
| #111/#115/#117 | 最后 cleanup left=true、mode/capture仍 owned，SKIPPED_NO_AUTHORITY；capture0 随后仅属 teardown；source/guard/receiver/hook销毁，不代表 UP 已观察 |

117 行里 native ENTER1/DRAG7、cancel/EXIT/WinEvent END/source writers/shield 均0。
native gesture DOWN 之后没有新的 Raw button transition，唯一旧 Raw UP 是 preflight
serial5，不能结清后来 serial7 的 DOWN。这是已记录流中的事实，不是对未观察
输入或当前按钮状态的全知证明。movement 轨迹偏离已观察；来源/actor UNKNOWN。
本轮不事后将旧 cleanup SKIPPED 改 PASS，也不复造旧失败 fence 的子条件。

基线实际 scoped source 复核：`fence()` 在记录前 require，故拒绝现场缺字段；
`capture_cleanup_input/perform_cleanup_input` 要求 move_size_clear/gui_mode_clear，
所以在 source 自有 GUIflags2/capture loop 中必然拒绝。这个 static gap 由旧失败
状态支持，不等于现有 cleanup 宽分支满足新归属合同。

## 3. 独立 PaneBind 模型：三种权限，非宽放行

| Authority | 新 F 能改变什么 | 不能改变什么 |
| --- | --- | --- |
| ProductGestureAuthority | 无变化 | END、WinEvent、capture/menu/mode clear、原 anchor/P/V/health 等既有产品与 writer gate 不放宽 |
| TestSyntheticInputIsolation | 正常 post-END source/shield 合同保留 | 不提前造 shield，不用 root 会员替代产品权限 |
| TestAbortCleanupAuthority | 新的 owned-native-active、单次 UP 独立分支 | 不授权 handoff、source writer、MOVE/DOWN、foreground/capture 改动或 foreign 操作 |

### DOWN ledger 与 provenance

本 run 的 command intent、实际 API sent count/error/start/return、receiver arm
epoch/watermark、实际 Raw DOWN serial/QPC/flags/tag，以及 actual native DOWN/ENTER
必须可独立关联。由于实际 Raw receipt 可先于 API return/父 INPUT record，允许
bounded provisional intent，**只有 sent1、实际 receipt、native 来源引用全部确认后**
才能成为 cleanup 的 pending-owned-DOWN；不能仅 driver `down=true`。
preflight/bootstrap 输入使用不同用途/epoch，不用其旧 UP 清账本 gesture。

从 gesture DOWN 到 initial/API boundary 的 receiver 流必须保持有效注册、连续
serial、合法 QPC、读取/传输/容量无已知 loss/error。所有按钮 transition 都应
进入 ledger，不只是匹配 test tag 的 packet；任何无对应本测试 command 的
left DOWN/UP、其它按钮转换、重复/矛盾 transition、未知/失效接收状态均令归属
不可靠，NO INPUT。tag 是关联，不是来源认证；同 tag 的未匹配转换也不能忽略。
单独的纯 movement 偏差可以与 pending DOWN 的 button provenance 区分，但不能
忽略其造成的 current root/foreground/capture 变化。

### Writer retirement / quiescence

先退休 acceptance ingress/hand-off readiness，新 Raw 不能 publish 接管命令；
取消尚未开始的 owner work。由 source UI owner 的有界事件驱动 ack 证明
active operation=0、pending motion/write 清空，记录 retired scope 与 ack 的 QPC/
sequence，再 fresh capture cleanup authority。若 source write 已在执行，必须等其
已记录 return/postverify 与真实 owner ack，不得边写边 UP。

不能将持 mutex、driver 设置一个 bool、PostMessage 返回成功或 timeout当 quiescence。
WM_APP 在 native modal loop 内是否实际调度无本研究的必达保证；ack 不到则 NO UP。
等待时不能持 owner/Raw callback 完成 ack 所必需的锁，不能在 WndProc/WinEvent
回调里等待自身事件或作 synthetic input。只 bounded deadline，不 polling 到成功。
fixture lifetime 与 retired takeover scope 分开：source、receiver、hook 继续活着
供 cleanup 观察，source 已 retired/destroyed/stop 后仍禁止 UP 或 stale HWND 消息。

### Initial + API-boundary authority

两次均记录实际 query start/end/success/error，不称原子 snapshot。需要：

- exact run/gesture/source HWND+实际 PID/TID+userdata nonce，active fixture；
- 可靠 input desktop/session、visible source、receiver/log health；
- global FG exact source，GUI query 指向 source UI TID；capture==source、
  hwndMoveSize==source、实际 native ENTER 未被结束，GUI_INMOVESIZE set；
- menu owner0、menu flags clear、无其它不允许 GUI mode；
- current actual cursor 查询成功，WindowFromPoint→GA_ROOT exact source；
- pending-owned DOWN ledger 可信、current left=true、其它按钮/修饰键 clear；
- 无 foreign capture transfer、身份/desktop/receiver/ledger失效；真实 quiescence。

这里不要求 current cursor 回旧 planned point；也不注入 MOVE把它拉回去。
boundary 前若 ledger/nonce/FG/root/capture/mode/button/context改变，即 NO INPUT，
不刷新到“终于合格”。纯 movement 若仍可同时证明两次实际接收条件，可保持
候选资格；不能用 planned rect/owner-group/root-owner 假装 exact source。
post-END 分支继续用原严格 source/shield isolation，不能借 active-native 扩展
豁免其 mode-clear 或丢失 END 前提。

## 4. 单 UP、独立终结与失效结果

满足全部 fresh authority 后只调用一次 LEFTUP（no MOVE/no DOWN，专用 cleanup
tag/scope，acceptance_eligible=false）。不得先 cancel、ReleaseCapture、Destroy、
改 FG/Z-order 或向窗口 Post WM_LBUTTONUP；这些改变接收条件或并非系统 UP。

唯一 API 与 receiver watermark/start QPC、真实 cleanup Raw UP serial/QPC/tag/
RIM_INPUTSINK、native EXIT、matching 本 START 的 WinEvent END、final GUI capture0/
MoveSize0/menu/mode clear、有效 desktop 下 async left=false、writer/pending0、
exact generation resource teardown逐个独立验收。实际 Raw/EXIT/WinEvent 的到达
顺序不得硬猜；receipt/QPC/references要可复算，callback只记录/通知。
单项发送成功不够，旧 preflight UP或晚到旧 packet也不够。

cleanup 开始后 actual Raw、native EXIT、WinEvent END、native geometry变化全部
标记 observation purpose/epoch。这些是真实事实但**不能**打开产品handoff，不能
作为正常Takeover END/RawUP/strict geometry acceptance，也不能被日志分类静默删掉。
retired acceptance 中晚到的 movement/UP不会补成功；cleanup final P/V只作诊断，
不修改 original anchor 或 accepted正常算法。

结果独立：gesture仍BLOCKED；完整 cleanup证据可PASS；authority不满足而未API是
SKIPPED_NO_AUTHORITY；已尝试但 sent0/缺RawUP/缺END/最终状态不可靠为FAILED或
UNCONFIRMED，保留 pending状态，禁止重发。NOT_NEEDED必须有可靠初/末的实际 up
上下文与 ledger，不能把失效desktop的0或已Destroy视作恢复。

SendInput无HWND与query→API race不能消除；序列连续也不证明操作系统/硬件不存在
未观察输入。此模型只支持受控、不假设恶意tag spoofing的 test-owned host 实验；
并不声称外来事件可唯一归因、所有第三方抢占都能自动恢复或产品物理输入安全。
若实现或 host 实测不能提供上述真实接收/终结证据，停止后续输入，设计/GUI阶段
标BLOCKED；不得以更宽UP/cancel/move fallback凑PASS。

## 5. Tests → implementation → empirical 的仍欠缺门

先 pure/synthetic 反例：active-owned positive；偏离planned但root仍source；
foreign/root/FG/capture/nonce/mode/menu/desktop/receiver失败；重复/foreign button
transition和丢包；provisional DOWN未sent1；API boundary变更；无pending/已release；
quiescence ack缺失/activewriter；仅sent1无RawUP/EXIT/WinEND/finalfalse；cleanup
UP/END污染acceptance；UNKNOWN字段默认PASS；旧v4套新cleanup规则；Int64时钟。
fence failure先记录同次已求值谓词、first/all failures，读不到或未求值明确UNKNOWN/
NOT_EVALUATED；原成功判定、±1容差、firstfailure、产品/几何算法保持。

显式新 cleanup contract/schema opt-in；旧 v4/Fix E validator仍按旧规则，
原 replay artifact仍绑定原9FFB...validator。新兼容回归仅新结果，不改旧hash。
`PRODUCT_HANDOFF_SEMANTIC_DIFF=NONE`、`GEOMETRY_WRITER_SEMANTIC_DIFF=NONE`；
`CLEANUP_CONTRACT_CHANGE=OWNED_NATIVE_ABORT_EXTENSION`，整轮不得宣称semantic NONE。

默认CTest含旧 owned geometry GUI 已造成历史phase1违规；本轮只能先用本机
show-only实际命令+源码副作用分类，正向offline入口，未知/零选取失败，禁止偷偷
退回完整CTest。研究没有运行CTest或以名称猜测试无副作用。

实现/离线门和当前有效输入状态read-only gate全部PASS后，才按brief固定八次
（Debug abort Move/Resize、Debug normal Move/Resize、Release同序）执行；
每项fresh、最多一次，无retry/stability batch。controlled abort不假装复现未知
外来输入源；expected abort=gesture BLOCKED_BY_TEST_FAULT、cleanup PASS、
fixture PASS_EXPECTED_ABORT、Takeover NOT_RUN。额外干扰/清理不全/状态未知立即STOP。

本研究 `FIXF_OWNED_NATIVE_CLEANUP_EMPIRICAL=NOT_TESTED`；原formal 13 PASS后14th
BLOCKED历史不改，新formal NOT_RUN，OwnedFree未通过，architecture UNRESOLVED。
Explorer/产品Raw/产品shield/全局hook/DLL/polling/真人视觉验收均不前移。

## Read-only current-input gate补充

本轮独立console工具不创建HWND/receiver/hook、不调用SendInput、不移动cursor。
前后有序检查Default input/thread desktop、WinSta0、WTSActive/unlocked和相同
foreground HWND/PID/TID；只读取TokenIntegrityLevel（不读取进程路径或用户内容），
要求caller/foreground integrity已知且caller不低于foreground，前后tuple相同。
OpenInputDesktop的READOBJECTS|HOOKCONTROL访问成功不是注册hook。任一失败、
swapped mouse或上下文变化均BLOCKED；无效上下文的async0不能作为UP。
11个固定button/modifier只读high bit，逐项记录时间；仍非原子快照，不能排除
采样间短暂切换，也不构成后续GUI的长期授权。

2026-09-27实际补读的official APIs：
[OpenProcessToken](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocesstoken)、
[GetTokenInformation](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation)、
[TOKEN_MANDATORY_LABEL](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-token_mandatory_label)。
仅本轮只读诊断权限验证，无代码复制/改编或权限提升操作。
