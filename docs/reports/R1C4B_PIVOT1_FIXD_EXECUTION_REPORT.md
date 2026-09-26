# R1-C4B Pivot 1 Fix D — 执行结果与首失败停止

## 结论与证据等级

本轮完成 **IMPLEMENTED / AUTOMATED TESTED** 的两类 proof 分离、独立 v4
test-owned probe、post-END shield、独立证据 validator 与严格顺序 runner。
第一项真实 Debug BottomResize 执行后，runner 返回 **INVALID_EVIDENCE**：
validator 第204行在实际64位 QPC 上发生 Int32 overflow。遵守 first failure
STOP，未重跑、未补丁后追认 PASS，未运行 Fix D Move、重复门或 Explorer。

Probe exit0 / `CAPTURED_NOT_ACCEPTED` 不是 acceptance。原始日志中确有完整
Resize/Raw UP/资源销毁记录，但完整独立验收未完成；不能据此把 architecture
置为 VALID，也没有证据把它置为 END-barrier 或 takeover-writer 被反例拒绝。
当前 architecture **UNRESOLVED**。旧 Fix B Move PASS 保留其原轮次范围。

`PASS` 的 Product / isolation / shield 子检查不等于完整 Resize gate PASS。
它们的范围是 **DIAGNOSTIC_ONLY**：在原 validator 中断之前已执行的原有
子函数，以及独立只读核查；不是修改或替代原 metadata 的正式结论。
下方 `FAIL` 的 handoff、takeover 与 free gate 表示 **验收链失败
EVIDENCE_VALIDATION_FAILURE**，不表示观察到 geometry 或 native-authority 反例。
Cancel 完整复合 verdict 未计算，标 UNKNOWN；它的 native lifecycle 事实单独保留。
**本轮没有 MANUALLY OBSERVED / Human UAT 验收。**

## 起点与实现范围

- Branch：`codex/r1c4b-live-magnet`。
- Starting HEAD：`e4ecdeaf4311e4adc95fcd94ff967df4c0f1bc83`，起点 clean、upstream0/0。
- 起始标准 `git fetch origin` PASS；`origin/main` 为
  `81e40facf52ffdb96f76e4d737740167d485f4a7`，起点0 behind /23 ahead。
- 实际执行 implementation SHA：`a67d3049c1027e8b2b82f8e122041b71ff5043ca`；
  前后 HEAD、clean 状态与 binary hash 相同。
- 普通实现提交：`test: separate product authority from owned input isolation`。
- 新模型/测试仅在 Windows test adapter 下；无 `src/core/`、product runtime、
  R0 observer 或旧 A/B/C validator/header/tests/report 修改。
- 完整实现语义见 [architecture](../architecture/R1C4B_PIVOT1_FIXD_INPUT_ISOLATION.md)，
  先行证据见 [research](../research/R1C4B_PIVOT1_FIXD_INPUT_ISOLATION.md)。

Product proof 没有 cursor-root predicate；cursor sampling 只作为 writer 计算
能力。TestIsolation 独立检查 fresh exact source/shield identity、PID/TID/nonce、
实际 WindowFromPoint→GA_ROOT、生命周期、foreground 与非交叠。每个 post-END
synthetic MOVE/UP 和 source writer 单独 fresh proof；失败不把 test predicate
归为 product authority failure。

Shield 为创建线程的 `WS_POPUP / WS_EX_NOACTIVATE`，仅在双 END + Product PASS
后创建，`HWND_TOPMOST` + flags80（NOACTIVATE|SHOWWINDOW）。固定有限路径的
横向 margin50、下方 margin50；top 裁到当前 source bottom，非 fullscreen、
不覆盖 source caption/client。不改 foreign z-order，也不把 source post-END
设为 topmost。每个 source strict postverify 后一次 shield maintenance；
无 timer、poll、source retry。Move 全部点属于 source 时不创建 shield。

v4 不注入 saved-cursor restore MOVE：saved point 可能属于 foreign root，
不为恢复目的豁免隔离。记录 `cursor_restore_skipped`，cursor 留在终点；旧 v1–v3
语义保留。Source work 投递检查 fresh identity/retirement，v4 无 stale-HWND
WM_CLOSE fallback；failure cleanup 仅在独立 fresh 安全 proof 下最多一次 UP，
cleanup Raw UP 永远不算 acceptance。已退役 fixture 不再注入。

## Prior art gate

先检查实际 source/license/history/issue/PR 与官方 API 文档，再模型、测试、实现。
AltSnap `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`（GPL reference-only），
包括 PinWindow/nonactivation/cleanup 与相关 history/issue；FancyZones
`19c4d805321db86f3634e6968e14dbf25cbba14a`（MIT reference-only），包括
WindowMouseSnap 与 PR48569 的 teardown/Abort 修补。官方资料覆盖 SendInput、
WindowFromPoint、GA_ROOT、CreateWindowEx、SetWindowPos、NOACTIVATE、DestroyWindow、
WinEvent 与 Raw Input。实际 inspected sources / exact pins / license / history
见研究文件及 `SOURCE_PROVENANCE.md` 本轮追加记录。

Research gate PASS 仅覆盖 bounded owned design。未复制、改写或适配外部代码；
GPL source 不进入 PaneBind。有限 hit-test proof 不是 race-free、HWND-addressed
SendInput transaction，也不建立产品 latency SLA。

## Fix C 只读重分类与历史保全

两份原 C JSONL 均通过原历史 envelope/validator 后，用新 product 字段规则只读
重分类：ProductHandoffAuthority PASS、TestInputIsolation BLOCKED、matching
WinEvent witness PASS、post-END geometry stability PASS、Takeover NOT_RUN、
architecture UNRESOLVED。原 `HistoricalVerdict=BLOCKED` 不改；原 compound
WinEventBarrier FAIL 不解释为 END witness 缺失，也不追认为 Resize PASS。

| 历史日志（ignored `uat/` 相对路径） | 不变 SHA256 |
| --- | --- |
| `r1c4b-end-diagnostics/20260926T180441083Z-Debug-BottomResize-8372500f52ef43f2aa38225205c43091.jsonl` | `BE6DDEC768EEA317264D1E93C327E9081E7926C5379B223E04B06C200A3D0A3E` |
| `r1c4b-end-diagnostics/20260926T181314652Z-Debug-BottomResize-1e7b2ae497d54151a52f60c285730e04.jsonl` | `F97E10307079657629EBE9F4C77C558F3DC3E102BF94280C7506AADD0929ADF1` |
| `r1c4b-end-handoff/20260926T155824435Z-Debug-Move-af08cb9bdc0f41e0891a635377ab76ab.jsonl` | `0AFEB1835FB133C55D3B2EBCD6BBD914B3925B8DD7E08C600A6BE892FB7B98BD` |
| `r1c4b-end-handoff/20260926T155921544Z-Debug-BottomResize-f785fe7f41c74cd9b28b19b5ed460601.jsonl` | `0843D6F9F4BBA806ECB9E1F448390D508FC07E969D633B87E7A0CB0D6B6D5A09` |

本轮前后复算相同；旧 A formal hash/结论沿用原报告，不重写任何旧 evidence、
metadata/report/verdict。新 D 实际结果不扩展旧 B Move PASS 的覆盖。

## 唯一实际交互执行与 preserved verdict

精确命令（仅执行一次）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-input-isolation.ps1 -Configuration Debug -Operation BottomResize
```

证据目录 ignored `uat/r1c4b-input-isolation/`；新 prefix：
`20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b`。

- JSONL SHA256：`BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F`。
- 原 metadata SHA256：`D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09`。
- Debug binary SHA256：`43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D`。
- Release binary（编译/回归通过，交互 NOT_RUN）：
  `9755812C25B99B90767844829458CB51F1FA8BB99E9A6B7FDDA213E84A984195`。
- Source HWND76615376 / PID45656 / TID33956；shield HWND22876276，同 PID/TID；
  exact source/shield nonce197157086769502。Guard HWND6359968 不充当 post-END shield。
- 独立 receiver PID49320 / HWND25495546；actual Raw input receipt来自该receiver。
- QPC frequency10000000；460条合法 JSONL、sequence1–460连续，startup/shutdown完整。
- ProbeExitCode0；shutdown `CAPTURED_NOT_ACCEPTED`；runner exit1，
  `Result=INVALID_EVIDENCE`、`CurrentContractsVerified=false`，
  `ImplementationUnchanged=true`。原 metadata 保留，不重写成 typed PASS。

只读重跑同一 validator 仍在 `Test-IsolationSourceWriters:204` 抛出：

```text
Cannot convert argument "val2", with value: "1134816073663",
for "Max" to type "System.Int32" ... too large or too small for an Int32.
```

根因是 `$lastReturn=0` 的 Int32 accumulator 与未显式定宽的
`[Math]::Max($lastReturn,$r.native_return_qpc)` 重载选择。Synthetic fixtures
使用较小 QPC，87项通过未覆盖此真实数值范围。未修改 validator、未在内存替换
函数去制造完整 PASS、未重跑 input；这是 evidence pipeline blocker，不是 Git
transport、WinEvent witness、source writer 或 native geometry 的已证实反例。

## 实际 raw facts（诊断，不替代完整 gate）

- native ENTER1 / WMSZ_BOTTOM DRAG2 / cancel1 / native EXIT1；capture clear。
  Cancel API return仍保留 bottom1084，EXIT terminal settlement回到 bottom1072。
- Exact source MOVESIZESTART/END 各1；native EXIT receipt1134815925811，
  WinEvent END callback1134815941919；post-END native DRAG0。
- phase `native_end` Product 未具备 WinEvent END；匹配 END 后 phase `winevent_end`
  的 Product proof全部满足，虽 cursor `[1339,1083]` 的 root100535132为 foreign。
  该 foreign root只记录数字，不查询 title/path/contents/UI tree。
- Shield creation在 Product seq108/双END之后；首次 P `[1289,1072,1390,1242]`。
  setup current+18remaining点全部19个 fresh observations，ready seq131。
- source writer19：handoff1 + Raw continuation18；每次最多一次 source SWP。
  shield placement20：initial1 +每次 source result后一次，actor/target独立。
- original DOWN `[1339,1071]` / ENTER P `[1126,632,1766,1072]`、
  V `[1137,632,1755,1061]`；final cursor `[1339,1191]`，full dy120。
  最终 P `[1126,632,1766,1192]`、V `[1137,632,1755,1181]`，left/top/right不变。
- first handoff native write晚于 native END、WinEvent END与isolation-ready；
  raw rows记录所有19次 source strict immediate/full exact。
- Raw continuation18、gesture actual Raw UP1、terminal pending motion/write=false，
  final left high bit=false。27个 Raw包还包含前置背景输入证明，不能把全27算续接。
- 共77个隔离快照（setup19、writer19、synthetic19、shield position20）在只读
  独立身份/root/非交叠核算中未发现矛盾；19/19 writer 原始锚点数学与原
  immediate/full postverify 子函数均通过。完整 aggregate仍未通过。
- `source_final_acceptance` seq454在actualUP之后；shield destroyed seq455晚于它，
  exact identity + creating thread、Destroy success/IsWindow=false。
- 无 receiver_error/winevent_error/blocked、无已记录 queue overflow/drop、
  无 shield activation；source/guard/receiver/shield/hook均记录清理成功。
  `external_windows_touched=false`。正常路径无需 failure cleanup UP。
- 没有 saved-cursor restore synthetic MOVE；鼠标留在终点，按钮已松开。

上述事实是只读原始记录核查；native自己的 `exact`、`source_final_acceptance`
命名与 probe0不能替代完整独立验收。后续 Move/重复门保持 NOT_RUN。

## Timing（单次失败证据的只读诊断统计，非 SLA）

按实际 QPC/frequency 转 ms；p50/p95 对排序样本线性插值。n1的两个分位相同，
不表示批次性能稳定。未形成 gate inventory，未对 unexecuted batch 填0ms。

| Metric | n | p50 ms | p95 ms |
| --- | ---: | ---: | ---: |
| native EXIT → WinEvent END | 1 | 1.6108 | 1.6108 |
| WinEvent END → isolation ready | 1 | 7.8311 | 7.8311 |
| isolation ready → handoff native write | 1 | 1.9120 | 1.9120 |
| WinEvent END → handoff native write | 1 | 9.7431 | 9.7431 |
| handoff native duration | 1 | 3.4313 | 3.4313 |
| Raw receipt → owner quantum | 18 | 2.0215 | 3.2383 |
| Raw receipt → native writer | 18 | 2.04625 | 3.276855 |

## Automated tests / 环境

Windows NT10.0.26200、MSVC VS18 Community、MSBuild18.5.4、SDK10.0.26100.0、
C++20 / CMake，Debug/Release。采用 VS bundled CMake/CTest；无 Git HTTP/TLS/proxy
持久配置改动。`MSBUILDDISABLENODEREUSE=1`、`--parallel 1 -- /nodeReuse:false`。

```powershell
cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 1 -- /nodeReuse:false
cmake --build out/r1c4b-live-magnet-release --config Release --parallel 1 -- /nodeReuse:false
ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-input-isolation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-input-isolation-runner.ps1
```

完整双配置 build PASS；最终 standalone-probe 生命周期修补后重编译 PASS，
最终双配置 CTest各30/30 PASS。新纯模型47checks PASS；完整 wire/synthetic
87checks PASS，root独立复跑；runner离线133checks PASS。Synthetic均显式标记，
默认拒绝充当真实 evidence，且不运行 GUI/input。

现有 offline PowerShell regressions全部 exit0（同 `powershell.exe -NoProfile
-ExecutionPolicy Bypass -File scripts/<name>.ps1` 命令形式）：

| 脚本 | 结果 |
| --- | --- |
| `test-r1c2b-evidence-runner` | 26 checks PASS |
| `test-r1c3a-evidence-runner` | 61 checks PASS |
| `test-r1c3b-profile-runner` | 47 checks PASS |
| `test-r1c3b-frame-profile-runner` | 41 checks PASS |
| `test-r1c3b-vdm-profile-runner` | 28 checks PASS |
| `test-r1c4a-capture-json` | 4 synthetic serialization stages PASS |
| `test-r1c4a-evidence-runner` | 140 checks PASS |
| `test-r1c4b-evidence-runner` | 29 checks PASS |
| `test-r1c4b-postverify` | 14 checks PASS |
| `test-r1c4b-auto-owned-modal` | 144 checks PASS |
| `test-r1c4b-takeover-owned` | 142 checks PASS |
| `test-r1c4b-end-handoff` | 61 checks PASS |
| `test-r1c4b-end-diagnostics` | 93 checks PASS |

这些自动测试 PASS 不抹去 actual validator overflow。`git diff --check` PASS。
旧文件未改，ignored evidence不进入 Git。

## Round gate 与 STOP

```text
FIXD_PRODUCT_AUTHORITY_SEPARATED = PASS
FIXD_TEST_INPUT_ISOLATION_SEPARATED = PASS
FIXC_RECLASSIFIED_PRODUCT_HANDOFF_AUTHORITY = PASS
FIXC_RECLASSIFIED_TEST_INPUT_ISOLATION = BLOCKED
POST_END_INPUT_SHIELD = PASS
BOTTOM_RESIZE_PRODUCT_GESTURE_AUTHORITY = PASS
BOTTOM_RESIZE_TEST_INPUT_ISOLATION = PASS
OWNED_NATIVE_CANCEL_RESIZE = UNKNOWN
OWNED_HANDOFF_RECONCILIATION_RESIZE = FAIL
OWNED_TAKEOVER_RESIZE = FAIL
OWNED_TAKEOVER_MOVE = NOT_RUN
OWNED_NATIVE_DRAG_AFTER_WINEVENT_END = 0
OWNED_FREE_TAKEOVER_GATE = FAIL
DEBUG_SMOKE = 0/5
RELEASE_SMOKE = 0/5
DEBUG_FORMAL = 0/20
RELEASE_FORMAL = 0/20
POST_CANCEL_LEGACY_MOUSE_DELIVERY_RISK = NOT_PRODUCT_TESTED
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
EXPLORER_STAGE = NOT_RUN
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_INPUT_SHIELD = NONE
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

此处0/5、0/20均为 NOT_RUN，不是运行后失败五/二十次。Fix D Move NOT_RUN不
撤销旧 Fix B `OWNED_TAKEOVER_MOVE=PASS`。本轮第一项验收失败后无第二次 input。

下一轮如获授权，先修补 validator 的64位 QPC accumulator/重载并添加大值
fixture与原真实日志只读回归，再决定新的实际 gate；不得静默改本轮旧 metadata。
本轮不实施该 follow-up，不顺手修改 native/runtime，不继续下一阶段。

仍 NOT TESTED：真实产品 physical legacy movement/up 在 capture释放后对
underlying windows的影响（不能用shield掩盖）、无shield的产品Raw接管、
Explorer/Pure Magnet、其他apps、mixed DPI/monitor、visual restore flicker、
idle CPU/20MB目标、failure cleanup实际故障场景、完整重复稳定性。
本次没有验证 event coalescing 压力行为。

最终普通文档提交 SHA、standard push/ls-remote核对、clean status与divergence见
交付回执，避免报告自身 SHA 自引用。origin/secondary identity已核对；不改remote、
不 forge tracking refs、不API传输、不force/amend/rebase/reset。完成后STOP。
