# R1-C3B Human Validation — Interaction Timing & Smoothness Baseline

审查日期：2026-09-13。Human validation：PASS / SEALED；frozen replay 与 pre-merge
自动回归全部 PASS。集成仍需普通 merge 和 final main regression。Scope 为 two-Explorer、same-monitor /
same-DPI、Debug optimized Ctrl+Move，不是通用产品认证。

正式 Human contract 只有一次 Phase 3 Debug UAT。未新增 Release 真人 UAT，
未放宽 wrapper 的 ValidateSet('Debug')，未绕过 wrapper。Release 仅自动回归。
本轮不修改 runtime/tests/scripts/CMake，不继续性能开发、不新增 Phase 4。

## 1. Source、证据与主观验收

- Starting branch：codex/r1c3b-smoothness-profile。
- Starting HEAD：5e6c79b6b0f39f0b929369e5fa0e727ecbf292f0。
- UAT runtime：201f3c25cee49f7b2ea7af67063cfaf7df2d5725。
- Starting origin/main：d901094404f04772e1d641842df76d13c551a734。
- Origin SSH-over-443 freshness PASS，工作树初始 clean，local/upstream 0/0。

| Phase | Human evidence prefix | 人工体验 | 解释 |
| --- | --- | --- | --- |
| 1 | 20260909T143539511Z | C | 明显卡顿；Shell inventory 主热点 |
| 2 | 20260910T161142993Z | C+ | 明显优于 C，但未达到 B |
| 3 | 20260912T173654895Z | 介于 A / B | 用户明确给出的正式 subjective acceptance |

允许的结论：在已验证的 two-Explorer Debug 场景中，主观跟随体验显著改善，
达到 A/B 之间。不是 perfect、AquaGlue-equivalent、production-ready 或 FPS 保证。
评分来自用户实际操作，本轮未代替用户输入/拖动、没有重跑真人 UAT。

Phase 3 原始数据只在 ignored uat/r1c3b/，不提交 Git：

| 文件 | Bytes | SHA256 |
| --- | ---: | --- |
| 20260912T173654895Z-glue-harness.jsonl | 11503948 | 4DB121AA9AC9598B33A68722D5D4BB08187A95DBCEF65C59AE1734B0208FE133 |
| 20260912T173654895Z-glue-observer.stdout.jsonl | 7935657 | 23022B9C712FDC9CC508301681D676D294A31814787B130E316F7E89AC51980E |
| 20260912T173654895Z-glue-observer.stderr.log | 0 | 空文件 |

Harness：24893 records；唯一 startup/summary/shutdown，890 event receipts，
199 processing quanta，21343 profile spans，1137 validation records。
Observer：7983 records，sequence 连续；hook registration、hook shutdown、
observer shutdown 完整，无 queue/post/overflow failure，stderr 为空。
Startup 明确 Debug、interactive console、synthetic_input=false、external Observer
enabled、frame-authority v1、VDM reuse enabled、virtual_desktop_result_cache=false。

只读审计检查 JSON/sequence、generation/source/quantum/ACK 关联、177 条实际
geometry chain、initial-relative 目标、validation freshness 和 timing 定义。
仓库 frozen Phase 3 validator 同时做完整离线 replay；结果在本报告回归节收口。
本环境技能文件不可读，未把未加载技能当作已执行审查；使用 repo validator 与
独立 raw-evidence 交叉检查，不改验收标准或运行代码。

## 2. Phase 1 root cause 与 Phase 2 契约修订

Phase 1：Shell inventory 占 operation-local exclusive 82.93%，16/16 operation
最大阶段；重复的 global selection validation 拉长 quanta，形成后续 queue backlog。
Notification->dispatch 的大数不等于 owner 实际等待，因为 ready queue 可先于
notification dispatch 被 drain。没有据此重写 message loop。

Phase 2 经 Human Root 明确修订：initial selection uniqueness 仍 required；
发证后 native authority 是 exact authorized top-level frame，由原 canonical
Shell anchor、nonce location、generation、native identity 和 eligibility 约束。
Same-frame additional entry 不代表新 native authority；different HWND 永不继承
capability；anchor 导航、quit、rehost/identity/stream failure 仍 fail closed。
旧 single-entry proof 的停止与反例保留，未改写为与旧规则等价。
真实 multi-tab UAT 仍 NOT TESTED。见[正式决策](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md)。

## 3. Phase 3 VDM reuse 与 correctness

Phase 2 的第二热点为 VDM family 47.4440%，52/60 operation 最大。Phase 3 只
复用私有 owner-STA service instance；每个需要 desktop eligibility 的 validation
仍以当前 authorized HWND 实时调用 IsWindowOnCurrentVirtualDesktop。没有缓存
true 结果、active recreate/retry、global COM singleton 或跨线程 interface 共享。
Release 在对应 CoUninitialize 前，manager 与两侧 frame authority 分离。

| Correctness 项 | Phase 3 实际结果 |
| --- | --- |
| Ctrl activation | PASS，1 次；callback delivery 时 Ctrl down |
| Leader START / LOCATION / END | 1 / 711 / 1 |
| Leader processing quanta / distinct samples | 183 / 175 |
| Follower native applies / distinct targets | 177 / 175 |
| Applies before END receipt | 177 |
| Follower LOCATION internal / Observer | 177 / 177 |
| Suppressed / duplicate / missing / reconciled | 177 / 0 / 0 / 0 |
| Recursive / unexpected | 0 / 0 |
| Native operations overall | 181：2 setup + 177 active + 2 restore |
| All active operation receipts | exact，identity/location/size/monitor/DPI stable |
| Final geometry / restore | EXACT / EXACT |
| Max pending / max queue | 2 / 13 |
| Hooks / queue / evidence | lifecycle clean，overflow/drop/post failure 无已知异常 |

两个目标均 explorer.exe，PID 27968，Leader TID 12772，Follower TID 5976；
native frame keys 2228926 / 5902030，独立 capability/consent generations。
均 CabinetWClass，DISPLAY1，DPI 192，medium integrity、无 elevation/UIAccess/
AppContainer。此 positive evidence 不等于其他权限环境通过真人验证。

Final visible rectangles：Leader (36,895,1274,1766)，Follower
(1274,895,2512,1766)，仍精确相邻。原位置分别恢复为
(118,864,1356,1735)、(167,913,1405,1784)，positioning rect 也精确恢复。
Visible/positioning 不混用，不以 SetWindowPos success 代替实际 postverify。

## 4. 177 applies / 175 targets：NON_BLOCKING

Raw evidence 恰有两个历史目标被各访问两次，均非连续重复写入。以下均为
active_follower，源 event_kind=geometry_changed，source 是该 quantum 的
selected Leader receipt，非 coalesced/discarded receipt，quantum 不含 END。

| 重访目标 visible rect | Operation generations | Quantum IDs | Leader source receipts | 各自 ACK receipts |
| --- | --- | --- | --- | --- |
| (1273,893,2511,1764) | 95 -> 176 | 106 -> 196 | 420 -> 884 | 423 -> 887 |
| (1253,608,2491,1479) | 148 -> 150 | 162 -> 165 | 765 -> 776 | 774 -> 781 |

第一组：Leader 的 processing-time sample 在 Q106 与 Q196 都为
(35,893,1273,1764)，但期间已经移动到其他位置。Gen176 的 Follower before
是 (1273,891,2511,1762)，因此该次确实向下平移 2 px，非同位置空写。
Gen95 后的 Gen96 则到 (1275,895,2513,1766)，证明这不是连续重复 target。

第二组：Gen148 到 y=608；Gen149 明确到 y=601（Q163 / source773）；Gen150
又回 y=608，其 before 是 (1253,601,2491,1472)，实际向下 7 px。Q164 不产生
新的 Leader move command；Gen150 的 Q165 是新 sample/source，不是旧 snapshot
被重复当作新的 operation。

独立检查全部 177 operation：generation 连续且各有唯一 Leader source；每个
before 等于上一条 actual；每个 requested 不等于其 before；每个 actual 精确
等于 requested；所有 target 均等于 initial Follower rectangle 加本 quantum 的
Leader total delta。无 accumulated drift、recursion、unexpected write。

因此多出的两次 apply 是合法的非连续历史坐标回访，不来自 START/END、missing
feedback reconciliation 或重复 snapshot 误用。Coalescing 只选择新的 source/sample：
Q106/196 分别有 3/2 个 Leader LOCATION，Q162/165 分别有 7/1 个；它不要求全程
坐标从不再次出现。183 Leader processing quanta 中另有 6 个合法 no-op，未造成
native apply，故 183 - 6 = 177；全程 distinct sample/target 为 175。

duplicate feedback=0 不矛盾：它统计同一 pending operation 的重复 feedback，
或没有 pending match 时重复最近 ACK 的 geometry，不是“某坐标在全部历史中
是否出现过”。旧 operation 已 ACK/移出 pending，新 generation 在不同当前位置
重新申请目标，各获得一次匹配 ACK；四条对应 reconciliation record 的 disposition
都是 acknowledged_self_feedback，command_trace_match_count=1。
这里的 reconciliation record 是关联审计记录，不代表执行了 missing-feedback
reconciliation；后者实际计数为 0。

时间先后依据 receipt sequence、quantum、operation generation 和 QPC；
recorded_at 是 buffered evidence 序列化时间，不当作每次移动实际发生时间。

## 5. Timing / queue / 三阶段 progression

以下均是 CPU-side timing，不是 DWM presentation、帧率或 display latency。
Percentile 不能相加。Activation callback->decision：147.0973 ms。

| Phase 3 interval | p50 ms | p95 ms |
| --- | ---: | ---: |
| Apply interval | 31.86005 | 85.2135 |
| Receipt->owner | 12.5053 | 56.2575 |
| Owner->native start | 19.1302 | 50.5662 |
| Native call | 6.2261 | 10.1005 |
| Native start->postverify | 12.8993 | 22.3711 |
| Native API return->postverify only | 7.0367 | 12.8744 |
| Receipt->exact postverify | 35.9236 | 72.5316 |
| Quantum span inclusive | 29.6881 | 68.0201 |

原 runner 的 native-to-postverify 名称实际从 native start 起算，包含 native call；
上表单独列 return->postverify，避免把两个口径混为一谈。

| 指标 | Phase 1 | Phase 2 | Phase 3 |
| --- | ---: | ---: | ---: |
| Human | C | C+ | A/B |
| Follower applies | 16 | 60 | 177 |
| Max queue depth | 51 | 23 | 13 |
| Quantum p50 ms | 218.5243 | 81.4957 | 29.6881 |
| Quantum p95 ms | 428.0875 | 145.457 | 68.0201 |
| Apply interval p50 ms | 304.4571 | 100.2221 | 31.86005 |
| Apply interval p95 ms | 532.0436 | 205.6511 | 85.2135 |
| Receipt->owner p50 ms | 161.0284 | 46.8883 | 12.5053 |
| Owner->native p50 ms | 239.39925 | 69.0361 | 19.1302 |
| Receipt->postverify p50 ms | 340.71445 | 96.4764 | 35.9236 |
| Active inventory calls | 86 | 0 | 0 |

Phase 3 共 199 个 quanta，528 个 Leader LOCATION 被 coalesce，最大 queue 13。
原始 drag 不可完全复现，因此不以 711/raw LOCATION 或 177 applies 推算性能倍率。
三阶段结论来自 per-operation/per-quantum timing、queue 与明确的用户主观验收。

## 6. Inventory / VDM / 热点

| Phase | Global inventory | Manager create | VDM queries | Manager release |
| --- | ---: | ---: | ---: | ---: |
| Setup | 27 | 9 | 21 | 8 |
| START | 2 | 0 | 2 | 0 |
| Active | 0 | 0 | 1102 | 0 |
| END | 4 | 0 | 4 | 0 |
| Restore | 8 | 0 | 8 | 1 |
| Invalidation | 0 | 0 | 0 | 0 |

Retained manager create/release=1/1，setup 另外有 8 次原 ephemeral acquisition。
1137 个 validation 全部成功且每个恰有一次 fresh query。Active 查询 1102 可分解
为 197 个 steady quanta × 2 captures，加 177 operations × 4 checks（Follower
prepare、独立 Leader witness、Follower immediate、Follower postverify）。
VIRTUAL_DESKTOP_RESULT_CACHE=NO，CHECK_FREQUENCY_CHANGED=NO。
Active shell_inventory exclusive=0；initial/setup/START/END/restore inventory
仍存在，没有宣称删除所有全局 inventory。

| Largest steady-active stage | Operations |
| --- | ---: |
| Native placement | 80 / 177 |
| Shell observation | 52 / 177 |
| Shell location | 39 / 177 |
| Virtual desktop query | 6 / 177 |
| Shell inventory | 0 / 177 |

VDM family operation-local exclusive share 为 16.89557433%，使用不重叠 parent/
child exclusive 成本口径。没有剩余单一热点要求立即继续优化；结合 A/B acceptance，
R1C3B_FURTHER_PERFORMANCE_OPTIMIZATION=STOPPED。没有 Phase 4。

## 7. Remaining limitations / future polish

NOT TESTED：mixed DPI、cross monitor、真实 multi-tab UAT、other apps、
elevation/UIAccess 真人场景、长期运行/resource SLA，以及 display-present/FPS。
NOT IMPLEMENTED：Z-order grouping、3+ windows、Glue Resize、Snap integration、
dynamic Ctrl-release detach。

未来仅 backlog，当前不实现：

- Normal-band Z-order：Leader highest member，Followers contiguous below；
  当前 SWP_NOZORDER 保持。
- Glue component MUST be bounded；product maximum NOT DECIDED，未来测试 2/4/8。
- N-window 优先研究一次 quantum 产生完整 Follower target set，再通过
  BeginDeferWindowPos / DeferWindowPos × N / EndDeferWindowPos 处理。
- Mixed-DPI/multi-monitor 独立研究；shell location/observation、native placement、
  message loop 只保留 future polish，不在本轮优化。

## 8. Runtime freeze / regression / integration

UAT runtime 与起始 review HEAD 的以下 Git object 完全相等：

| Path | Frozen object |
| --- | --- |
| src tree | 662628716e97b11380f785a7d3a4b2e5c5bb2ab7 |
| scripts tree | 49f31a2d2cc2fbf1cc50c0afb663a2d649171ec9 |
| tests tree | f524d0c68658a47e30109a77b07e6edb2053100c |
| CMakeLists.txt blob | dd20c1ee67d72a8dc9ac8634afd6ef7783cc9c12 |

本轮仅 docs-only（包括 README）。完整 frozen offline replay exit 0，Ctrl /
realtime / frame-fast / VDM lifetime-freshness / timing / external Observer
evidence gates 全部 PASS。其审计结果与本报告的独立 chain、ACK、geometry 和
exclusive hotspot 重算一致。

Pre-merge 自动回归：Debug build + CTest 14/14 PASS；Release build + CTest
14/14 PASS；Owned Debug/Release、Companion Debug/Release 四项 self-test PASS，
failures 0；runner fixtures 26 + 61 + 47 + 41 + 28 全部 PASS。已覆盖当前
C2A/C2B/C3A/C3B 的 C++ 与 evidence regression。未重跑任何真人 UAT。

复现命令（cmake/ctest 使用 VS bundled CMake/bin；现有构建目录）：

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c3b-phase3-vdm-profile.ps1 -ValidateEvidencePrefix uat/r1c3b/20260912T173654895Z -ValidationHarnessExitCode 0 -ValidationObserverExitCode 0
cmake --build out/r1c3b-debug --config Debug --parallel
ctest --test-dir out/r1c3b-debug -C Debug --output-on-failure
cmake --build out/r1c3b-release --config Release --parallel
ctest --test-dir out/r1c3b-release -C Release --output-on-failure
.\out\r1c3b-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c3b-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c3b-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c3b-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-frame-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
~~~

环境：Windows 10.0.26200，VS 18 x64、Windows SDK 10.0.26100.0，CMake 4.2.3-msvc3。
最终 merge SHA、PR URL、final main regression 与 clean/divergence 由最终交接
记录；不为填写自引用 SHA 而在 main 开发。任何 runtime/tests/scripts/CMake 变化
立即阻断集成，要求 HUMAN_REVALIDATION_REQUIRED=YES。
PR 使用普通 merge commit，不 squash/rebase/force；不打 tag/release，不开始 R1C4。

~~~text
R1C3B_HUMAN_VALIDATION = PASS
R1C3B_PHASE1_HUMAN_SMOOTHNESS = C
R1C3B_PHASE2_HUMAN_SMOOTHNESS = C_PLUS
R1C3B_PHASE3_HUMAN_SMOOTHNESS = A_B
R1C3B_CTRL_ACTIVATION_GATE = PASS
R1C3B_REALTIME_FOLLOW_GATE = PASS
R1C3B_FEEDBACK_SUPPRESSION_GATE = PASS
R1C3B_ACTIVE_GLOBAL_INVENTORY = ZERO
R1C3B_ACTIVE_VDM_CREATE = ZERO
R1C3B_VIRTUAL_DESKTOP_FRESH_QUERY = PRESERVED
R1C3B_SMOOTHNESS_RUNTIME_GATE = PASS
R1C3B_FURTHER_PERFORMANCE_OPTIMIZATION = STOPPED
HUMAN_REVALIDATION_REQUIRED = NO
R1C4 = NOT STARTED
~~~
