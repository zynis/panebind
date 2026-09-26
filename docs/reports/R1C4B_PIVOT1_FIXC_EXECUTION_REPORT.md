# R1-C4B Pivot 1 Fix C — Structured END handoff diagnostics 执行报告

执行日期：2026-09-27（Asia/Shanghai；日志文件名使用 2026-09-26 UTC）。
分支 `codex/r1c4b-live-magnet`，起始 clean HEAD
`10d429706b8afb3be11f308370af89964a5a054a`，local/upstream 0/0。

## 结论与停止点

IMPLEMENTED / AUTOMATED TESTED：test-only 分项 preflight、exact-source
out-of-context WinEvent START/END matcher、双 END 首写 barrier、受约束 cleanup、
独立 validator、单次 runner 和分阶段 repetition runner。

实际先运行 Debug BottomResize×1；matching WinEvent END 后唯一阻断为
`CursorOutsideOwnedInputGuard`，零 native write。按任务书 §15，仅修正一次
test-only guard geometry，覆盖两套 planned trajectory +50px，再只运行
BottomResize×1。第二次仍是同一唯一 authority blocker，零写入，立即 STOP。

关键区别：cursor 坐标在 guard rect 内，但实际 `WindowFromPoint → GA_ROOT`
不属于 exact owned source 或 guard。**几何覆盖不等于 hit-test-root authority。**
修正后的覆盖由 validator 从实际 rect 和原始 START independently 重算通过，
不能据此把 root membership 改成 true。没有推定 z-order、occlusion、owner
或其他具体原因，也没有调整这些条件、扩大权限或继续重试。

WinEvent START/END 真实匹配，native EXIT 后 GUI clear、P/V exact；没有
matching END 后 native DRAG 或未归属 geometry change。这不是 END geometry
反例，也不是 writer failure。复合 `WinEventBarrier` 因 fresh authority 未通过
而 FAIL，整体 architecture **UNRESOLVED**，不是 REJECTED。

[Fix B 的单次 Move 正例](R1C4B_PIVOT1_FIXB_EXECUTION_REPORT.md) 保持有效，
不降级、不追认为 Fix C regression。C Move、smoke、formal、Release interaction、
Pure Magnet、Explorer、Human UAT 全部未运行。

```text
CURRENT_CONCURRENT_NATIVE_LOOP_ARCHITECTURE = REJECTED
OLD_CANCEL_RETURN_RECT_CONTRACT = FAILED_AND_SUPERSEDED
FIXC_STRUCTURED_HANDOFF_DIAGNOSTIC = PASS
FIXC_WINEVENT_END_BARRIER = FAIL
MATCHING_WINEVENT_START_END_WITNESS_OBSERVED = YES
WINEVENT_HOOK_LIFECYCLE = PASS
BOTTOM_RESIZE_INITIAL_FAILURE_CLASS = CursorOutsideOwnedInputGuard
NATIVE_END_DIAGNOSTIC_INITIAL_FAILURE_CLASS = MissingWinEventEnd
BOTTOM_RESIZE_RETRY_FAILURE_CLASS = CursorOutsideOwnedInputGuard
OWNED_RAW_INPUT_BACKGROUND = PASS
OWNED_NATIVE_CANCEL_MOVE = PASS_WITH_TERMINAL_SETTLEMENT
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
OWNED_HANDOFF_RECONCILIATION_MOVE = PASS
OWNED_HANDOFF_RECONCILIATION_RESIZE = NOT_RUN
OWNED_TAKEOVER_MOVE = PASS
OWNED_TAKEOVER_RESIZE = NOT_RUN
OWNED_TAKEOVER_PRE_RELEASE_CONTROL = PASS
FIXC_MOVE_REGRESSION = NOT_RUN
OWNED_NATIVE_DRAG_AFTER_WINEVENT_END = 0
OWNED_FREE_TAKEOVER_GATE = NOT_RUN
DEBUG_SMOKE = 0/5
RELEASE_SMOKE = 0/5
DEBUG_FORMAL = 0/20
RELEASE_FORMAL = 0/20
NATIVE_EXIT_TO_WINEVENT_END_P50_MS = 2.2124
NATIVE_EXIT_TO_WINEVENT_END_P95_MS = 2.4004
WINEVENT_END_TO_HANDOFF_WRITE_P50_MS = NOT_RUN
WINEVENT_END_TO_HANDOFF_WRITE_P95_MS = NOT_RUN
CLEANUP_INPUT_RELEASE = SKIPPED_NO_AUTHORITY
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

上述 Move PASS / pre-release PASS 仅继承旧 B 已观察的完整 Move。
`BOTTOM_RESIZE_INITIAL_FAILURE_CLASS` 指本轮**首次运行在两个 END 均到达后的
实际阻断**，不是最早 native-only 诊断阶段的优先级字段。
两次 metadata 的 `InitialFailureClass=MissingWinEventEnd` 对应尚未收到
异步 END 的 `phase=native_end`；其全部 failed checks 同时包含 cursor root。
最终 `phase=winevent_end` 的 `FinalFailureClass` 与唯一 failed check 均为
`CursorOutsideOwnedInputGuard`。没有把等待阶段误写成 END 丢失。

完整 Resize cancel gate 仍 UNKNOWN：机械 ENTER/SIZING/cancel/capture release/
terminal settlement/held EXIT 已观察，但没有 post-END Raw continuation、
takeover Raw UP 或完整 final acceptance。`native_drag_after_winevent_end=0`
只覆盖两个实际停止前缀，不证明未执行 takeover 的可靠性。

## 研究、实现与隔离范围

[Fix C research](../research/R1C4B_PIVOT1_FIXC_END_DIAGNOSTICS.md) gate PASS，
[architecture](../architecture/R1C4B_PIVOT1_FIXC_DIAGNOSTICS.md) 记录新 v3 合约，
[provenance](../research/SOURCE_PROVENANCE.md) 记录本轮实际 source/history/license
检查。AltSnap GPL reference-only；FancyZones MIT reference-only，均未复制、
翻译、适配或派生代码。研究 PASS 仅授权 owned probe，不是运行验收。

新增 CLI `--run-owned-end-diagnostics-test --gesture move|bottom-resize
--evidence-log NEW_FILE`，schema `r1c4b-takeover-owned/v3`；旧 v1/v2 CLI、
validator、fixtures、JSONL、metadata、报告和 verdict 保留原合同。
Core、产品 runtime、R0 observer、Fix 3 probe 未改。CMake/default CTest
只增加纯 C++ classifier test，不加入 Raw Input/SendInput/WinEvent GUI 实验。

每次 fresh source/guard/hidden receiver/hook；exact PID/UI-thread filter，
`WINEVENT_OUTOFCONTEXT`，无 SKIPOWNPROCESS/SKIPOWNTHREAD。callback 只记录
有界 envelope/QPC/sequence 并通知 owner，不 capture、pump、COM 或写窗口。
owner 匹配真实 START→END、hook/source/thread/gesture arm，再 fresh 读取
全部 proof/P/V，先完整 record 再决策。首次 native write 必须晚于两种 END；
这个写入条件有 synthetic coverage，本轮因 authority 阻断没有真实首写正例。

原始 DOWN pointer 和 ENTER P/V 冻结为 intent anchor，EXIT 只作 terminal
stability baseline。Bottom 只变 bottom；使用 checked frame bridge、立即/full
P/V strict exact，不用 corrected rect 累加、不加 DWM grace、sleep、polling、
timer retry 或 repeated SWP。原有有限输入轨迹 pacing 不作为事件或 writer 源。
没有 AttachThreadInput、WH_MOUSE_LL、global mouse hook、DLL injection 或
arbitrary-HWND authority。

两个 commits 分别为：

- `febec1d6aeba72914ab4dc62ea4b509a59285c49` — `test: add structured WinEvent end-barrier handoff diagnostics`
- `c8798537a78e2a4490748e00008f8908c4c8a5f4` — `test: cover owned gesture plans with a 50-pixel guard margin`

第二个仅改变新 v3 test guard geometry、其 evidence 与对应 coverage tests；
`WS_EX_NOACTIVATE`、show mode、z-order/owner/权限不变，旧 v1/v2 dimensions 不变。

## 本地真实证据与独立复核

原始 JSONL 和同 prefix `.metadata.json` 保留 ignored
`uat/r1c4b-end-diagnostics/`，不进入 Git。两次相同命令，各只运行一个手势：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-end-diagnostics.ps1 -Configuration Debug -Operation BottomResize
```

| 项目 | 首次 Debug BottomResize | 单次 guard 修正后 Debug BottomResize |
| --- | --- | --- |
| prefix | `20260926T180441083Z-Debug-BottomResize-8372500f52ef43f2aa38225205c43091` | `20260926T181314652Z-Debug-BottomResize-1e7b2ae497d54151a52f60c285730e04` |
| ExecutedHEAD | `febec1d6aeba72914ab4dc62ea4b509a59285c49` | `c8798537a78e2a4490748e00008f8908c4c8a5f4` |
| source HWND / PID / TID | 8259788 / 3040 / 24960 | 9638050 / 48872 / 53672 |
| guard HWND | 41225730 | 18354190 |
| receiver HWND / PID / TID | 33687596 / 18004 / 35580 | 9572526 / 12592 / 31988 |
| hook handle | 547229759 | 256446769 |
| actual guard P | `[1096,602,1976,1252]` | `[1076,582,1996,1242]` |
| final preflight cursor / actual root | `[1339,1083]` / 920508 | `[1339,1083]` / 33623098 |
| native ENTER / SIZING / EXIT | 1 / 2 WMSZ_BOTTOM / 1 | 1 / 2 WMSZ_BOTTOM / 1 |
| cancel / terminal settlement | 1 / 1 | 1 / 1 |
| Raw packets / movement / UP | 8 / 5 / 1（仅 preflight UP） | 8 / 5 / 1（仅 preflight UP） |
| native writes / continuation quanta | 0 / 0 | 0 / 0 |
| after-return / after-native-END / after-WinEvent-END DRAG | 0 / 0 / 0 | 0 / 0 / 0 |
| unowned geometry changes | 0 | 0 |
| JSONL rows / sequence | 116 / 连续 | 116 / 连续 |
| probe / runner exit | 2 / 2 | 2 / 2 |
| typed result | BLOCKED / UNRESOLVED | BLOCKED / UNRESOLVED |

JSONL SHA256 分别为：

```text
BE6DDEC768EEA317264D1E93C327E9081E7926C5379B223E04B06C200A3D0A3E
F97E10307079657629EBE9F4C77C558F3DC3E102BF94280C7506AADD0929ADF1
```

对应 Debug binary SHA256 分别为：

```text
ED6ADB510F2EFF91C8A214DAAC538272265328E76E0CD64A73843D7E51E71731
0F1654A0BBDAB4C64BB2C5232C7DFD72705C9FED923DB2A27E92A5E40B3389E9
```

两次 startup/shutdown 完整、JSONL 合法、receiver serial 1–10 连续；无已知
overflow/drop/post failure/receiver failure/log failure。明确的 preflight
失败保留，不能写成“无错误”。metadata 均 pre/post clean、HEAD 与 binary
hash 不变、`CurrentContractsVerified=true`、`ImplementationUnchanged=true`、
`SyntheticFixture=false`。独立重算与记录一致，不相信 harness verdict 自证。

共同 monitor65537、DPI192，work area `[0,0,3072,1824]`、virtual screen
`[0,0,3072,1920]`。original DOWN `[1339,1071]`，planned endpoint
`[1339,1191]`；START、terminal EXIT 和 matching-END fresh actual 的
P 均 `[1126,632,1766,1072]`，V 均 `[1137,632,1755,1061]`。
cancel return P bottom1084 / V bottom1073，随后 terminal restoration 回到
START；不把 terminal restoration 当作 cancel FAIL 或新 intent anchor。

修正后 guard 的 planned Move bounds `[1126,632,1946,1072]`、Bottom Resize
bounds `[1126,632,1766,1192]`，union 加50px恰为实际 guard rect。
validator 从原始 START 和180px Move /120px Bottom 计划独立计算，两套覆盖
均 true。旧 guard 也包含本次 cursor 和 Bottom path 的50px范围；因此没有
证据把旧失败根因定为尺寸不足。两次 root 均非 source/guard，而不是 cursor
坐标在 rect 之外。没有查询/操纵这些 foreign root 的窗口状态。

两次 #102 `phase=native_end` 均记录完整 proof，failure priority 为
`MissingWinEventEnd`，failed checks 两项；不写。#103 callback/#104 owner match
证实 exact source/thread/current hook/armed START→END；#107 重新 fresh capture，
仅 root membership false。identity/desktop/visible/foreground、GUI query 与
flags0、capture/menu/movesize0、held left/其他输入清、receiver/takeover health、
DPI/monitor、terminal/actual P/V availability 与 exact、native/unowned change
均符合要求。无 `DWM_VISIBLE_TERMINAL_LAG_CANDIDATE` 或 post-END geometry mismatch。

## Event ordering 与有限样本时序

QPC frequency 均 10,000,000Hz。native ENTER/EXIT 用 `receipt_qpc`，WinEvent
用 `callback_qpc`，cancel 用 API `returned_qpc`；不是相邻 record 的 QPC。

| 事件 QPC | 首次 | guard 修正后 |
| --- | --- | --- |
| native ENTER #68 | 1056077848150 | 1061213758722 |
| WinEvent START #70，callback seq1 | 1056077862081 | 1061213766071 |
| cancel return #98 | 1056079532755 | 1061215616431 |
| native EXIT #100，held/capture0 | 1056079598025 | 1061215674100 |
| native-only diagnostic #102 | 1056079621587 | 1061215693502 |
| matching WinEvent END #103，callback seq2 | 1056079622238 | 1061215694135 |
| fresh final handoff preflight #107 | 1056079650499 | 1061215717299 |
| first native write / takeover Raw UP | NOT_RUN / NOT_OBSERVED | NOT_RUN / NOT_OBSERVED |

native EXIT→WinEvent END 分别24,213ticks = **2.4213ms**，20,035ticks =
**2.0035ms**。n=2 合并诊断样本采用排序后线性插值分位数：p50=2.2124ms、
p95=2.40041ms（报告四位小数2.4004）。不是 formal repetition、稳定性结论
或 SLA。out-of-context callback 是异步 receipt，不是 OS internal exit
instant；不能预设两种 witness 的先后关系。没有 first write，故
WinEvent END→handoff write 的 p50/p95 均 NOT_RUN，不能填0。

## Cleanup 与输入安全

两个 #110–112 均在 acceptance 退出后单独记录 cleanup。root authority
仍不足，所以 `cleanup_skipped_no_authority`：没有尝试或发送 LEFTUP，
没有实际 cleanup Raw UP。final high bit=true、capture0、cursor
`[1339,1083]`、`PendingButton=true`、`acceptance_eligible=false`，
shutdown `cursor_restored=false`。这些不补成 cleanup PASS 或 gesture UP。

#113 同安装 UI thread Unhook 成功；#115 receiver 注销/销毁/停止；#116
source/guard 销毁、hook removed=true、external_windows_touched=false、
geometry writes0。只清理空白自建资源，没有关闭用户应用或丢弃数据。

已提示用户：若鼠标仍表现为按下，手动按放左键。其后只读主机检查分别在
`2026-09-26T18:07:09.3170117Z` 和 `2026-09-26T18:14:30.0743324Z`
看到 left high bit=false，`NoInputSent=true`。这只说明稍后的当前状态，
不重写日志，不补作 cleanup、Raw UP 或 Resize acceptance。

## 自动回归

Windows NT10.0.26200，C++20，VS18 MSBuild18.5.4+cb4e32d21，SDK10.0.26100.0。
cwd `D:\repository\panebind`，`$env:MSBUILDDISABLENODEREUSE='1'`；CMake/CTest
使用 VS bundled 绝对路径
`D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`。

```powershell
cmake.exe --build out/r1c4b-live-magnet-debug --config Debug --parallel 1 -- /nodeReuse:false
cmake.exe --build out/r1c4b-live-magnet-release --config Release --parallel 1 -- /nodeReuse:false
ctest.exe --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest.exe --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-end-diagnostics.ps1
```

最终 guard 修正后完整 Debug/Release build exit0；host CTest **29/29 +29/29
PASS**。新增 pure classifier **38 checks PASS**；新 PowerShell validator
**93 synthetic checks PASS**（初始87，加6个 guard coverage 检查，STOP后只读
离线复核仍93 PASS）。涵盖任务 A–P、late/wrong END、availability、完整
failure predicates、first-write/anchor/Bottom/full exact、Raw与cleanup隔离、
partial cleanup/final context 和实际 blocked prefix。Synthetic 不充当 Windows
empirical positive；没有运行 gated aggregate runner。

12项既有脚本逐项以 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
scripts/<name>` 默认离线参数执行，全部exit0：

| 脚本 | 结果 |
| --- | --- |
| `test-r1c2b-evidence-runner.ps1` | 26 stdout fixtures PASS |
| `test-r1c3a-evidence-runner.ps1` | 61 stdout fixtures PASS |
| `test-r1c3b-profile-runner.ps1` | 47 stdout fixtures PASS |
| `test-r1c3b-frame-profile-runner.ps1` | 41 checks PASS |
| `test-r1c3b-vdm-profile-runner.ps1` | 28 checks PASS |
| `test-r1c4a-capture-json.ps1` | Debug4 stages PASS |
| `test-r1c4a-evidence-runner.ps1` | 140 checks PASS |
| `test-r1c4b-evidence-runner.ps1` | 29 checks PASS |
| `test-r1c4b-postverify.ps1` | 14 checks PASS |
| `test-r1c4b-auto-owned-modal.ps1` | 144 synthetic checks PASS |
| `test-r1c4b-takeover-owned.ps1` | 142 synthetic checks PASS |
| `test-r1c4b-end-handoff.ps1` | 61 synthetic checks PASS |

最终 Release binary 已编译、未交互运行，SHA256
`1175C2557F5AC500243849252B68DC85B3C968B52A57E5C1CC6D0AD572F4258E`。

## 历史保全、Git 与下一轮边界

旧 B Move SHA256
`0AFEB1835FB133C55D3B2EBCD6BBD914B3925B8DD7E08C600A6BE892FB7B98BD`、
旧 B Resize
`0843D6F9F4BBA806ECB9E1F448390D508FC07E969D633B87E7A0CB0D6B6D5A09`、
旧 A formal
`F146A4235600F0FFFCD1E708238E5960FBEA2C7AD0597EECC3D569770D65B74E`
保持原证据与 verdict。新 C 分项失败不追认成旧 B 未记录的实际 failure class。

起始标准 `git fetch origin` PASS；origin URL 已核对为
`ssh://git@ssh.github.com:443/zynis/panebind.git`，secondary
`https://github.com/zynis/panebind.git` 同 repository。起始
`origin/main=81e40facf52ffdb96f76e4d737740167d485f4a7`，
`origin/main...HEAD=0 behind/20 ahead`，不是 secondary supplied ref。
本轮普通 commits、标准 push；无force/rebase/amend/reset/API object/ref
重建或Git HTTP/TLS/proxy配置修改。最终文档 commit SHA、push 远端核对、
clean status 和最终 divergence 见交付回执，避免报告自身自引用 SHA。
`uat/` 始终 ignored、不入 Git。

唯一当前阻断：matching END 后 fresh cursor hit-test root 不属于 exact
owned source/guard；已授权的一次 guard geometry 修正没有解除该条件。
后续若要调查 root membership，须另轮明确授权，不能本轮改 z-order、owner、
guard authority 或操纵 foreign root。Resize handoff/continuation、C Move
regression、重复门、Release interaction、Explorer authority、Pure Magnet、
mixed DPI/monitor、产品输入接管、visual flicker 均 NOT TESTED。STOP。
