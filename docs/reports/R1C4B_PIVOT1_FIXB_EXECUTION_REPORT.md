# R1-C4B Pivot 1 Fix B — END-barrier owned handoff 执行报告

执行日期：2026-09-26，交付整理跨至 2026-09-27（Asia/Shanghai）。
分支 `codex/r1c4b-live-magnet`；起始 clean HEAD
`e381b998d9e3eded051fbc859893633e8633ebee`。
研究及 implementation checkpoint：
`d9640c746a5f7bdcc4827124c187b295fe82bfd6`
（`test: add end-barrier owned takeover probe`）。

## 结论与停止点

Debug Move 单次完整 PASS；随后 Debug Bottom Resize 在首次 handoff 前
fail-closed，零 takeover write。其原因记录为
`handoff_terminal_stability_failed`，driver 记录
`handoff_completion_failed`。该复合检查失败发生在 fresh authority 和
actual snapshot 的日志行之前，无法从证据辨别具体哪项条件失败。
整体候选为 **UNRESOLVED**，不是已经证明的 native END 后 reassertion、
Raw failure 或 writer failure。按 STOP 边界，不改 native、不重试交互，
不开始 Debug/Release repetition、Pure Magnet、Explorer 或 Human UAT。

旧 Fix A 报告、JSONL、metadata、validator/verdict 保持不变。
`REJECTED_AT_CANCEL_STAGE` 仍是 `RESULT_UNDER_OLD_RETURN_BASELINE_CONTRACT`；
本轮正式 `SUPERSEDED_ARCHITECTURE_GATE = WAIT_FOR_END_BARRIER`，
而不是追认旧 run 为新 PASS。并发 native-loop correction 仍 REJECTED。

```text
CURRENT_CONCURRENT_NATIVE_LOOP_ARCHITECTURE = REJECTED
OLD_CANCEL_RETURN_RECT_CONTRACT = FAILED_AND_SUPERSEDED
END_BARRIER_HANDOFF_CONTRACT = UNRESOLVED
OWNED_RAW_INPUT_BACKGROUND = PASS
OWNED_NATIVE_CANCEL_MOVE = PASS_WITH_TERMINAL_SETTLEMENT
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
OWNED_HANDOFF_RECONCILIATION_MOVE = PASS
OWNED_HANDOFF_RECONCILIATION_RESIZE = NOT_RUN
OWNED_TAKEOVER_MOVE = PASS
OWNED_TAKEOVER_RESIZE = NOT_RUN
OWNED_NATIVE_DRAG_AFTER_END = 0
OWNED_TAKEOVER_PRE_RELEASE_CONTROL = PASS
TAKEOVER_TARGET_USES_FULL_GESTURE_DELTA = YES
TAKEOVER_TARGET_USES_EXIT_RECT_AS_INTENT_ANCHOR = NO
DEBUG_REPETITIONS = 0/20
RELEASE_REPETITIONS = 0/20
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
EXPLORER_STAGE = NOT_RUN
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

`PRE_RELEASE_CONTROL = PASS` 和 full-delta YES 仅限实际完成的单次 Move，
不是 Resize 或 repetition acceptance。Resize 机械 cancellation 的
ENTER/SIZING/capture release/held EXIT 已观察，但缺少 post-END Raw continuation
及完整后续生命周期，因此其完整 cancel gate UNKNOWN；不能由该前置阻断
倒推出 cancel FAIL。初始两个 diagnostic runs 不计入后续 20/20 repetition。

## 研究与实现范围

见 [Fix B 研究](../research/R1C4B_PIVOT1_FIXB_END_BARRIER.md)、
[probe 架构](../architecture/R1C4B_PIVOT1_PROBE.md) 和
[provenance](../research/SOURCE_PROVENANCE.md)。沿用已检查的 pinned
AltSnap/FancyZones source/license/history，并实际审阅官方 END/cancel/cursor
合约；没有新 external code 复制、翻译、适配。研究 gate PASS 仅授权 owned
probe，不等于运行结果或产品授权。

IMPLEMENTED：独立 test EXE 的 opt-in v2 `end_barrier_v1` 模式、单次 owned
runner、独立严格 validator 和 synthetic fixtures。产品 runtime、Core、
R0 observer、旧 Fix 3 probe、CMake/default CTest 及旧 Fix A validator 均未修改。
不存在 input hook、AttachThreadInput、DLL injection、产品 SendInput 或
resident polling。测试 timer 只 pacing 有限 SendInput 轨迹，不唤醒 writer。

真实 DOWN pointer 与 ENTER P/V 冻结为 intent anchor；EXIT actual P/V 只作为
stability baseline。queued owner handoff 在真实 END 后 fresh 验证，最多一次
SetWindowPos。此后实际 background WM_INPUT movement 经有界 mailbox 唤醒
UI owner，事件触发 GetCursorPos 一次、原始 anchor 全量计算、最多一次写入。
Raw UP terminal 优先清 pending motion；owner commit 与 parent UP publication
序列化，但验收独立使用 receiver 实际 UP QPC，不能靠迟到的 parent 日志
隐藏在 UP 后开始或完成的写入。现有 checked frame bridge 和立即/full P/V
strict exact 保持，无 DWM grace、补写 retry 或 corrected-rect 累积。

## 真实本地证据

原始日志及 metadata 都在 ignored `uat/r1c4b-end-handoff/`，未提交 Git。
两次均使用 checkpoint `d9640c7...`，`WorktreeDirty=false`、HEAD 与 binary
前后完全一致。Debug binary SHA256：
`FC4D33BF82E2B1DEBC82D4EE66B5113510E4AF0CACA07E5FCE20C4650BADFB8F`。

| 项目 | Debug Move×1 | Debug Bottom Resize×1 |
| --- | --- | --- |
| 日志 prefix | `20260926T155824435Z-Debug-Move-af08cb9bdc0f41e0891a635377ab76ab` | `20260926T155921544Z-Debug-BottomResize-f785fe7f41c74cd9b28b19b5ed460601` |
| JSONL rows | 371 | 104 |
| source HWND / PID / TID | 12128084 / 52500 / 49400 | 4788190 / 39180 / 51492 |
| receiver HWND / PID | 7143864 / 42416 | 61476132 / 12004 |
| native ENTER / DRAG / EXIT | 1 / 2 WM_MOVING / 1 | 1 / 2 WMSZ_BOTTOM WM_SIZING / 1 |
| cancel calls / terminal settlement | 1 / 1 | 1 / 1 |
| Raw packets / movement / UP | 27 / 23 / 2 | 8 / 5 / 1（仅 preflight UP） |
| handoff / continuation native writes | 1 / 18 | 0 / 0 |
| after-return native DRAG / after-END DRAG | 0 / 0 | 0 / 0 |
| 已记录 after-END unowned geometry delta | 0 | 0（不替代缺失 fresh proof） |
| probe / runner exit | 0 / 0 | 2 / 2 |
| result | PASS（单次 Move） | BLOCKED / UNRESOLVED |

Move JSONL SHA256：
`0AFEB1835FB133C55D3B2EBCD6BBD914B3925B8DD7E08C600A6BE892FB7B98BD`。
Resize JSONL SHA256：
`0843D6F9F4BBA806ECB9E1F448390D508FC07E969D633B87E7A0CB0D6B6D5A09`。
两项均 JSONL 合法、sequence 连续、startup/shutdown 完整、receiver 正常注销
并销毁；未见 queue overflow、drop、post failure、receiver error 或 log limit。
Resize 的明确 owner 前置失败必须保留，不能写成“无错误”。

共用 monitor `65537`、DPI 192、work area `[0,0,3072,1824]`，
virtual screen `[0,0,3072,1920]`。START P `[1126,632,1766,1072]`、
V `[1137,632,1755,1061]`。

### Move 实际生命周期与 geometry

DOWN `[1339,641]` → ENTER#75 → WM_MOVING×2 → 单 cancel#93 →
return#96：P `[1144,632,1784,1072]`、V `[1155,632,1773,1061]` →
terminal restoration#97 → held/capture0 EXIT#98 回到 START P/V →
fresh handoff#100，current cursor `[1357,641]`，**完整 delta `[18,0]`** →
唯一 handoff#101–103 exact 到原始 START+delta，而不是 EXIT+post-END delta →
18 actual Raw movement owner quanta / 单写 strict exact → 实际 Raw UP#359 →
owner terminal#362 →
final P `[1306,632,1946,1072]`、V `[1317,632,1935,1061]`。
final cursor `[1519,641]`、完整 delta `[180,0]`，pending write/motion false。
独立 final driver fence 证实已 release、GUI clear；最后 native return 比真实
receiver UP 早 41.6763 ms。cursor restore、自建 source/guard/receiver cleanup
完整，外部窗口 geometry/control 没有触及。

独立审计发现 18 continuation movement 中 11 个 receiver Raw callback 的
cursor snapshot 仍为旧位置；实际 owner current cursor 及其后 driver sample
正确。未把 Raw snapshot 当作历史 geometry authority，也未把 normalized
Raw delta 积分；单次正例不消除更广泛输入时序风险。

### Resize 实际停止前缀与证据缺口

DOWN `[1339,1071]` → ENTER#68 → SIZING#77/#84 → sample2
cursor `[1339,1083]`，P bottom1084 / V bottom1073 → cancel#93 →
capture0#95 → return#96 → restoration#97（初始 P/V） →
真实 held/capture0 EXIT#98 → bounded wait success#99 →
复合 fresh proof/stability check 失败#100 → driver blocked#101。
没有 `handoff_begin`、`writer_begin`、post-END Raw movement、takeover UP 或
完整 final geometry。#102 的同 P/V 通知不是 reassertion。

`handoff_terminal_stability_failed` 同时覆盖 context、capture 与 geometry
比较；失败时没有记录分项 proof/actual。因此不能猜是 cursor hit root、
foreground、GUI state、DWM visible lag 或 actual rect 变化，也不能宣告
`REJECTED_AT_END_BARRIER` / `REJECTED_AT_TAKEOVER_WRITER`。
后续需要先取得这次前置失败的可解释分项证据，须另轮明确授权；本轮未顺手修复。

清理仅销毁空白自建 source/guard、移除 receiver 注册并关闭自建 child。
本 prefix 没有合格 cleanup fence/release、gesture Raw UP 或最终 false-button
fence；`PendingButton=true`、`cursor_restored=false` 必须保留。
authority 未知时没有向未知窗口盲发 UP。已提示用户；之后单独主机只读
GetAsyncKeyState(VK_LBUTTON) high bit 为 false，且未发送输入。
这只是当前状态诊断，**不补作 Resize UP/cleanup acceptance**。

## 初始样本时序统计（无 SLA）

QPC frequency 两次均 10,000,000 Hz。下表按 operation 分开；每格只有
一个真实观测，故 p50=p95=该单点，不表示 latency 稳定性或 repetition PASS。
settlement 使用 POSITION_CHANGED 行的记录 QPC（非独立 callback entry 时间）；
END 使用 callback receipt QPC；
handoff→Raw 使用 handoff native return→下一实际 receiver movement QPC。

| 指标（ms） | Move n=1：p50 / p95 | Resize n=1：p50 / p95 |
| --- | --- | --- |
| cancel return→terminal POSITION_CHANGED | 1.2722 / 1.2722 | 1.9778 / 1.9778 |
| cancel return→EXIT | 2.7127 / 2.7127 | 3.5464 / 3.5464 |
| EXIT→handoff native start | 2.2640 / 2.2640 | NOT_RUN |
| handoff native duration | 2.3426 / 2.3426 | NOT_RUN |
| handoff native return→next Raw movement | 37.9318 / 37.9318 | NOT_RUN |

native→restoration→reconciliation 是否实际呈现中间 DWM frame/视觉闪烁，
自动 geometry/QPC 不能回答；`NOT_HUMAN_TESTED`，不填 PASS。

## 自动回归

Windows NT 10.0.26200；C++20 / VS18 MSBuild18.5.4，Windows SDK10.0.26100.0。
以下命令 cwd 均为 `D:\repository\panebind`，
`$env:MSBUILDDISABLENODEREUSE='1'`；CMake/CTest 使用 VS bundled 绝对路径
`D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`。

```powershell
cmake.exe --build out/r1c4b-live-magnet-debug --config Debug --parallel 1 -- /nodeReuse:false
cmake.exe --build out/r1c4b-live-magnet-release --config Release --parallel 1 -- /nodeReuse:false
ctest.exe --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest.exe --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-end-handoff.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-end-handoff.ps1 -Configuration Debug -Operation Move
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-end-handoff.ps1 -Configuration Debug -Operation BottomResize
```

两配置完整 build PASS；主机 CTest Debug **28/28**、Release **28/28** PASS。
默认 CTest 未加入 Raw Input/SendInput 新实验。新 validator **61 synthetic
checks PASS**，覆盖完整 Move/BottomResize wire positives、用户 A–H 正/反例矩阵、
pre-END write、wrong original anchor、multiple handoff、post-END native/unowned
change、strict diagnostic、Raw/input provenance、context failure 和合法 blocked
prefix。Synthetic 只验证模型/解码，不当作 Windows empirical evidence。

11项旧脚本均以 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
scripts/<name>` 默认离线参数执行、exit0：

| 脚本 | 汇总 |
| --- | --- |
| `test-r1c2b-evidence-runner.ps1` | 26 stdout fixture PASS |
| `test-r1c3a-evidence-runner.ps1` | 61 stdout fixture PASS |
| `test-r1c3b-profile-runner.ps1` | 47 stdout fixture PASS |
| `test-r1c3b-frame-profile-runner.ps1` | 41 checks |
| `test-r1c3b-vdm-profile-runner.ps1` | 28 checks |
| `test-r1c4a-capture-json.ps1` | Debug 4 stages |
| `test-r1c4a-evidence-runner.ps1` | 140 checks |
| `test-r1c4b-evidence-runner.ps1` | 29 checks |
| `test-r1c4b-postverify.ps1` | 14 checks |
| `test-r1c4b-auto-owned-modal.ps1` | 144 synthetic checks |
| `test-r1c4b-takeover-owned.ps1` | 142 synthetic checks |

Release probe 已编译但未交互运行；binary SHA256
`2F9BC77F6397C78828E76AC893E5E8406DEB1448D3CA9F01F33FC86AD127FB53`。

## 历史保全与 Git 交付

旧 Fix A report/validator/fixtures、Fix 3 source staged/unstaged diff 均为空；
重新只读 hash 旧 evidence 与既有报告一致：Fix A 正式 run
`F146A4235600F0FFFCD1E708238E5960FBEA2C7AD0597EECC3D569770D65B74E`；
原 Pivot 三次 `9D5D08...` / `0ECE70...` / `0F3EC5...`，Fix A 环境前缀
`F53037...`、input interference `15085F...`，Fix 3 `1CCC89...` 均未变化。

开始时普通标准 `git fetch origin` PASS，origin 使用已验证的
`ssh://git@ssh.github.com:443/zynis/panebind.git`；
`origin/main = 81e40facf52ffdb96f76e4d737740167d485f4a7`，当时
main→HEAD 为 0 behind / 18 ahead，local/upstream 0/0。
无 Git API transport、force/rebase/amend/reset、HTTP/TLS/proxy 配置修改。
implementation checkpoint 后仅新增本报告及当前入口说明；最终文档 commit
与标准 push 后的完整 HEAD、remote ref、clean status/divergence 见交付回执，
避免在报告自身 commit 中伪造自引用 SHA。`uat/` 始终不入 Git。

STOP：Resize handoff 复合前置证据缺口仍在；20/20、Release interaction、
Explorer consent/security/END witness、Pure Magnet、混合 DPI/monitor、产品
输入 authority 及 visual restoration 均 NOT TESTED。没有要求 Human
替代自动 gate，也没有开始后续轮次。
