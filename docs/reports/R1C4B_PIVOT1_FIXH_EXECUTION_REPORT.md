# R1-C4B Pivot 1 Fix H — Owned Stability Gate

日期：2026-09-28（Asia/Shanghai）。仅冻结 v5 契约的 owned 重复运行验收；
没有新产品功能、Explorer、Pure Magnet、真人 UAT 或 Recovery Index 更改。

最终接受决定：**NOT_PASSED**。100个实际normal手势及fresh完整v5验算通过；
新增辅助计数把Boolean=true重复计入UNKNOWN，机器summary的PASS不被接受。
冻结实现和原artifact未修复、未改写，retry0；详见下方FINAL_COUNTER_AUDIT。

## Git 起点与不可变契约

Branch `codex/r1c4b-live-magnet`；starting HEAD
`36e1e0f0312afdca5c6195999dce4335061c7d47`，worktree clean、upstream0/0。
标准 `git fetch origin` PASS，实际origin为
`ssh://git@ssh.github.com:443/zynis/panebind.git`，secondary为同仓库HTTPS；
origin/main=`81e40facf52ffdb96f76e4d737740167d485f4a7`，起点0 behind/32 ahead。
Fix G implementation=`5fc0ccb575b82940b78e4a8e4cbac916e12a2b4c`；该SHA到起点
只增加Fix G报告结果，没有native/helper/runner/validator变化。

本轮复用既有已核验的AltSnap/FancyZones research与官方合同（见
[Fix G报告](R1C4B_PIVOT1_FIXG_EXECUTION_REPORT.md)及
[provenance](../research/SOURCE_PROVENANCE.md)）；不设计新的窗口/输入行为，
不复制或派生外部代码，不重新解释历史失败或扩展权限。
native cleanup、ProductGestureAuthority、geometry writer、输入隔离、v5完整
normal验收、readonly v2、单次runner均保持冻结；本轮新增仅PowerShell编排/
独立复核/统计/打包与针对性离线测试。

固定基础SHA-256：

| 依赖 | SHA-256 |
|---|---|
| Fix E validator | 9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D |
| v5 normal primitives | 064B9C1EDAC92C7A7F011FDF7F8B01D15D339D0214CF21436F753E8876483D6E |
| Fix G inventory | 9FAA3A049D027079F9026C77B34213ECEBC8AC5C9D53C75708AC6C54A1F01A37 |
| frozen Fix F replay | 4444A4AEA058828DCFE8B42C31CF8F47AE20E4EF0BFBABED35A2A3BB5A3A47F6 |

起始实际重hash匹配。独立先决条件脚本从原G八项32文件按原5fc实施SHA与十九
身份清单复核，fresh v5完整分层verdict和v2 pre/post；旧11份D/E/F前后hash
核对，F prefix保持BLOCKED、历史null不补写。历史不进入Fix H任何分子/分母。
先决条件完整只读复核PASS：4 abort、4 normal均按实际5fc实施SHA重新独立验算，
32个run原文件和inventory/replay/F支持证据共36个文件绑定一致；旧11份历史
文件前后hash不变。D replay仍PASS，E blocked仍BLOCKED/SKIPPED_NO_AUTHORITY，
F原prefix仍VERIFIED_BLOCKED_BEFORE_INPUT；新增GUI/input均0。
核验artifact：`uat/r1c4b-fixh/fixg-prerequisites-precheckpoint-20260928.json`，
SHA256=`028B0AFAD5F70C25B64D13BC7B8E3154F0C2CE6F08077E076D46A60B8EB1D98A`。
本轮实际结果见下方；未以旧报告代替原始证据。

## 实现与停止规则

新入口 `scripts/run-r1c4b-owned-stability-gates.ps1` 没有count/retry/resume/
AllowSynthetic/live绕过开关。唯一child site调用冻结
`run-r1c4b-input-reliability.ps1 -Mode normal -ExpectedHEAD <frozenSHA>`。
不执行旧v4入口或native EXE，不添加单次/abort试跑。

计划在首个GUI/input前CreateNew落盘，包含batchId、版本、100个固定step及
预登记fresh RunId、implementation HEAD、两配置十九runtime/source/import/
binary hash和六个新aggregate/helper/statistics/package hash、停止条件。
每child前后重新检查clean HEAD、runtime/outer identity和plan bytes；新helper
不在旧十九清单中也不能中途修改。实际run仍在既有ignored fixg目录，新的plan/
append-only progress/inventory/statistics/summary/finalization在ignored fixh。

| group | phase | 配置 | 操作 | Planned |
|---:|---|---|---|---:|
| 1 | smoke | Debug | Move | 5 |
| 2 | smoke | Debug | BottomResize | 5 |
| 3 | smoke | Release | Move | 5 |
| 4 | smoke | Release | BottomResize | 5 |
| 5 | formal | Debug | Move | 20 |
| 6 | formal | Debug | BottomResize | 20 |
| 7 | formal | Release | Move | 20 |
| 8 | formal | Release | BottomResize | 20 |

四组smoke全部通过才开始formal；任一non-PASS立即停止所有剩余项、不retry。
attempt在child前记录；包括缺metadata的actual exit/output与已知文件/missing
声明，不猜未知启动数或继承上一项UP。loop全部结束仅为
PENDING_INDEPENDENT_SUMMARY；独立无输入汇总逐项fresh复核raw/metadata/v2/
identity、plan/progress/ordinal/nonce、fresh NormalProof measurement及统计。
软件/证据写入失败保持非零，不补跑GUI；独立finalization receipt保存收尾错误。

完整normal合同不降低：original anchor、Bottom非参与边、WinEvent END barrier、
Product/Test authority隔离、strict P/V、actual Raw continuation/UP、final exact/
UP/no pending、≤1 source write perquantum/handoff、post-END nativeDRAG0、
shield仅在实际需要时、所有自有资源有序退出。PID/HWND数值允许OS复用，RunId/
nonce不可重复，创建/销毁必须各自证明。

## 离线验证与统计口径

Windows10.0.26200、Windows PowerShell5.1.26100.9444、VS18 2026 x64，
CMake/CTest4.2.3-msvc3。Native未修改，本轮可复用并逐hash核对Fix G两配置
probe/environment binary；没有默认全量CTest或额外interactive测试。
实际已执行正向 `run-r1c4b-offline-tests.ps1 -Configuration Debug/Release`，
各24/24 PASS；旧numeric-runner98、冻结single runner363、新编排111、新统计107、
新boundary90、新包装9 checks通过；九个新增PowerShell文件parser零错误。
真实G normal测量只读兼容检查AVAILABLE、JSON roundtrip保持精确一致，不计入H。
新包装自测为synthetic文件/ZIP反例，不是经验UAT，不进入现场清单。
先决条件脚本离线调试曾发现`+`与`,`优先级将两个F支持路径拼成一条字符串，
在首个现场运行前修正括号；未改历史文件、未用重跑替代历史证据。

实际命令均由`powershell.exe -NoProfile -ExecutionPolicy Bypass -File`执行：

| script与参数 | 结果 |
|---|---|
| `scripts/run-r1c4b-offline-tests.ps1 -Configuration Debug` | 24/24 PASS |
| `scripts/run-r1c4b-offline-tests.ps1 -Configuration Release` | 24/24 PASS |
| `scripts/test-r1c4b-input-isolation-numeric-runner.ps1` | 98 checks PASS |
| `scripts/test-r1c4b-input-reliability-runner.ps1` | 363 checks PASS |
| `scripts/test-r1c4b-owned-stability-gates.ps1` | 111 checks PASS |
| `scripts/test-r1c4b-owned-stability-statistics.ps1` | 107 checks PASS |
| `scripts/test-r1c4b-owned-stability-boundaries.ps1` | 90 checks PASS |
| `scripts/package-r1c4b-owned-stability-evidence.ps1 -SelfTest` | 9 checks PASS，仅文件/ZIP |
| `scripts/verify-r1c4b-fixh-prerequisites.ps1 -ArtifactPath uat/r1c4b-fixh/fixg-prerequisites-precheckpoint-20260928.json` | 独立原证据PASS |

统计组COMPLETE/PARTIAL/empty/large-QPC的JSON roundtrip亦精确PASS；
独立只读审查未发现PS5.1 scope/字典传递或首失败停止blocker。

统计只消费fresh完整正常verdict.NormalProof及原rows，按smoke/formal×配置×
操作分为8组。七维为native EXIT→WinEvent END、END→isolation ready、
isolation ready→handoff write、END→handoff write、handoff duration、Raw→
owner quantum、Raw→native writer。前五为gesture粒度，后两为Raw/quantum粒度；
不把许多Raw样本当许多独立gesture。
QPC/sequence/tick差signed Int64，合法cross-stream负delta保留、不clamp；
每run按实际QPC frequency转decimal毫秒，再固定线性插值(index=(n-1)×p)计算
p50/p95。无SLA或临时阈值；空维返回null/NOT_AVAILABLE，部分组明确PARTIAL，
不填0冒充已通过。实际writes/quanta/handoff/shield placements/Raw/resources
计数独立绑定记录，并与NormalProof核对。

## 本轮现场与交付

先决条件和离线反例通过后，冻结01633ff实施checkpoint并执行唯一100项计划。
现场循环和原机器逐项汇总均100/100 normal PASS；额外独立control审查发现
UNKNOWN辅助计数矛盾，最终接受决定NOT_PASSED，不接受顶层机器PASS。
现场后不修改实现/计划/轨迹/时限/采样数，不修后重跑，不进入Explorer/产品阶段。
证据包按本batch显式plan/进度/inventory/统计/summary/finalization与实际attempt
四原文件、新先决条件的必要G/F引用打包；保留字节、绝对原路径不改写，ZIP
manifest另列archive相对映射。含失败及missing声明、不全量uat/EXE/截图，
完整run不拆分、尽量每包≤25MiB；不上传、不提交raw evidence到Git。

```text
FIXE_HISTORICAL_FORMAL = 13_PASS_THEN_BLOCKED_AT_14
FIXF_HISTORICAL_GUI = BLOCKED_BEFORE_INPUT
FIXG_HISTORICAL_BOUNDED = 4_ABORT_PASS_AND_4_NORMAL_PASS
FIXH_FROZEN_IMPLEMENTATION_SHA = 01633ff31776b42be9fc2383ed301d9884e80019
FIXG_PREREQUISITE_EVIDENCE_VERIFIED = YES
FIXH_V5_RUNNER_PATH = scripts/run-r1c4b-input-reliability.ps1
FIXH_PLAN_HASH = 8CF16B8E015891CD3C85B9703E4E105BB31C8EBFCCF19E18C16A2D1C1125576A
DEBUG_MOVE_SMOKE = 5/5
DEBUG_RESIZE_SMOKE = 5/5
RELEASE_MOVE_SMOKE = 5/5
RELEASE_RESIZE_SMOKE = 5/5
DEBUG_MOVE_FORMAL = 20/20
DEBUG_RESIZE_FORMAL = 20/20
RELEASE_MOVE_FORMAL = 20/20
RELEASE_RESIZE_FORMAL = 20/20
FIXH_BATCH_RESULT = INVALID_EVIDENCE
FIRST_UNEXPECTED_FAILURE = FINAL_COUNTER_AUDIT
RETRY_COUNT = 0
FINAL_BUTTON_STATE = OBSERVED_UP
FIXH_FINAL_INDEPENDENT_SUMMARY = NOT_PASSED_BY_COUNTER_AUDIT
MACHINE_SUMMARY_RESULT = PASS
MACHINE_SUMMARY_EXIT_CODE = 0
FIXH_V5_STABILITY_GATE = NOT_PASSED
OWNED_FREE_TAKEOVER_GATE = NOT_PASSED
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
EXPLORER_STAGE = NOT_READY
EXPLORER_EXECUTED = NO
POST_CANCEL_LEGACY_MOUSE_DELIVERY_RISK = NOT_PRODUCT_TESTED
VISUAL_TERMINAL_RESTORE_FLICKER = NOT_HUMAN_TESTED
R1C4B_HUMAN_UAT = NOT_READY
PRODUCT_INPUT_SHIELD = NONE
PRODUCT_RAW_INPUT = NOT_IMPLEMENTED
PRODUCT_SENDINPUT_DEPENDENCY = NONE
PRODUCT_GLOBAL_MOUSE_HOOK = NONE
PRODUCT_DLL_INJECTION = NONE
PRODUCT_POLLING = NONE
PR / MERGE / TAG / RELEASE = NO
```

## 实际批次、原 artifact 与最终接受决定

实施冻结提交：`01633ff31776b42be9fc2383ed301d9884e80019`，
subject `test: freeze v5 owned stability gate orchestration`；首个现场前worktree clean。
本轮native/C++、readonly、v5单次及原validator均NO CHANGES；
新增九个PS编排/统计/审计/打包/纯测试文件，现场后仅改本报告。

唯一现场命令：
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-owned-stability-gates.ps1`。
实际唯一child仍是`scripts/run-r1c4b-input-reliability.ps1`，
每项配置/操作依冻结计划、`-Mode normal -RunId <fresh> -ExpectedHEAD 01633ff31776b42be9fc2383ed301d9884e80019`。
没有其它单次、abort、retry、替补、resume或新GUI，probe共100次。
以下是实际owned自动现场/只读验算，不是人工观察或产品UAT。

BatchId=`c8e0d10d38c344879d4554be4e019742`。
原控制文件目录：`D:\repository\panebind\uat\r1c4b-fixh\20260927T174945663Z-c8e0d10d38c344879d4554be4e019742`。计划在首次GUI前落盘；
20 smoke全部逐项PASS后，才启动第21项formal。
100个RunId、100个nonce唯一；每次resource生命周期完整，
不要求数值HWND/PID永不复用。progress201条（header+100 start/finish）。

| 文件 | SHA256 |
|---|---|
| plan.json | 8CF16B8E015891CD3C85B9703E4E105BB31C8EBFCCF19E18C16A2D1C1125576A |
| progress.jsonl | 09CD57F8A96784652F5686DFF9892CF73700BDB583645F4B212627D4783E4EE4 |
| inventory.json | 465252DE89F6CFACC1D6640DFD2015498F4575A915955AF92B3C0DAD5C63EE18 |
| statistics.json | 061A1E0FC864D11DB00298D9794F24BD5FAAE0034303D421DF4E793537A57F6E |
| summary.json | 1F9FABF6C342EF55514E0564E19473A71C89B554955C92A366C531CB2FBB282D |
| finalization.json | 9B65284133AB9F7AB52B9769E6468A97062DCF3FC72918402FEEDB6D7CE16898 |
| prerequisites.json | 6DE0652D9F6FF996BED3F29D373D5F244514E3895058535F86AE2161B003EEA0 |

批次内前置复核再PASS：原G的4 abort+4 normal和原11历史文件不变。
额外只读control审计在现场后核原11文件hash，11/11仍一致；没有改写旧记录。

### 正常验收事实与计数矛盾分层

| phase | config | operation | Planned | Preflight | Probe | Target entered | Normal PASS | blocked/fail/invalid-native | NotRun | 缓存unknown（两项各） | 实际typed unknown（两项各） |
|---|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|
| smoke | Debug | Move | 5 | 5 | 5 | 5 | 5 | 0/0/0 | 0 | 5 | 0 |
| smoke | Debug | BottomResize | 5 | 5 | 5 | 5 | 5 | 0/0/0 | 0 | 5 | 0 |
| smoke | Release | Move | 5 | 5 | 5 | 5 | 5 | 0/0/0 | 0 | 5 | 0 |
| smoke | Release | BottomResize | 5 | 5 | 5 | 5 | 5 | 0/0/0 | 0 | 5 | 0 |
| formal | Debug | Move | 20 | 20 | 20 | 20 | 20 | 0/0/0 | 0 | 20 | 0 |
| formal | Debug | BottomResize | 20 | 20 | 20 | 20 | 20 | 0/0/0 | 0 | 20 | 0 |
| formal | Release | Move | 20 | 20 | 20 | 20 | 20 | 0/0/0 | 0 | 20 | 0 |
| formal | Release | BottomResize | 20 | 20 | 20 | 20 | 20 | 0/0/0 | 0 | 20 | 0 |

最终独立脚本重新读取100份raw/metadata/v2 pre/post并完整v5 normal验证，
100/100成功，fresh统计与原statistics一致。它原样返回
`BatchResult=PASS / IndependentSummaryResult=PASS / exit0`；
aggregate/finalization也原样记录exit0。无收尾写入错误。

但是，现场结束后的额外只读control审查发现唯一明确软件矛盾：
[model第103行](../../scripts/r1c4b-owned-stability-model.ps1#L103)和第104行
以布尔字段`-ceq 'UNKNOWN'`。PowerShell5.1实测
`$true -ceq 'UNKNOWN'`为true（右侧非空string被转换为bool）；
100个actual ProbeInvoked/TargetGestureEntered字段均是Boolean=true，
却重复进入两个UNKNOWN计数。缓存各100，实际严格string UNKNOWN各0。
首个受影响控制组是smoke/Debug/Move（5 known+5错误unknown），其余七组同因。

summary复用同一计数helper，虽然重读证据，却仍重算出同一错误，因而未发现
known/unknown互斥矛盾。这不是某个normal gesture失败，也没有观察到新的
geometry/raw-handoff架构反例；100项完整正常验收及七维统计事实不撤销。
但最终计数/接受链不可信，**不接受机器顶层PASS作为Seal**。

`FIRST_UNEXPECTED_FAILURE=FINAL_COUNTER_AUDIT`；
phase=final audit，configuration/operation=all eight groups，repetition=N/A，
native log失败sequence=N/A。原引用为inventory/summary的Counts[0..7]两个
unknown字段及冻结model103–104行；其hash均列在上表。
首次现场后没有修改实现/计划/时限/采样数，没有重验或GUI补跑。
最终接受分类是`INVALID_EVIDENCE`（仅计数/Seal链），owned gate NOT_PASSED。
后续如获另行授权，只能修复辅助审计并对这些不可变证据另行只读审计；
本轮不执行该修复，不把后续阶段前拉。

### 当前最终输入观察

`FINAL_BUTTON_STATE=OBSERVED_UP`，来源是第100项的readonly v2 post：
`D:\repository\panebind\uat\r1c4b-fixg\20260927T184812468Z-Release-BottomResize-normal-bcf50cc52ecc44788a67845f24179499\environment-post.json`。

SHA256=`187AE2E15CC06C219969F18D88E14E02F671FBF674A99ED89C7A3F870B2A5E12`。
真实button查询QPC范围1946221962282–1946221962574，frequency=10000000 Hz；
完整post context范围1946221835310–1946221973643。
此非原子观察对应attempt_finished UTC=`2026-09-27T18:48:45.5607548Z`
（北京时间2026-09-28 02:48:45.5607548；这是进度记录时间，不冒充原子按钮采样UTC）。
post全部必需输入UP、可靠context、GUI readiness READY；最后normal raw也证明
真实UP、final exact、无pending movement/write和资源收尾，不继承上一项UP。

## 实际七维统计

以下从原fresh NormalProof/rows重算，机器summary已完整复核一致；计数审计错误
不改写统计。各run实际QPC frequency均10000000 Hz，decimal转换和固定
index=(n-1)×p线性插值。前五维gesture样本；后两维Raw/owner/write样本。
例如formal每组20独立gesture、每个Raw维360样本，不能称360独立gesture。
无SLA、不因尾部较大调整阈值。各组COMPLETE，未混入旧或failed/abort数据。

### smoke / Debug / Move

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 5 | 2.6414 | 3.24186 |
| WinEventEndToIsolationReady | 5 | 7.3481 | 49.57424 |
| IsolationReadyToHandoffWrite | 5 | 0.7589 | 1.91942 |
| WinEventEndToHandoffWrite | 5 | 8.107 | 50.323 |
| HandoffWriteDuration | 5 | 4.0109 | 4.84842 |
| RawReceiptToOwnerQuantum | 90 | 2.93875 | 4.1898 |
| RawReceiptToNativeWrite | 90 | 2.98835 | 4.22804 |

### smoke / Debug / BottomResize

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 5 | 3.1896 | 4.90604 |
| WinEventEndToIsolationReady | 5 | 25.2721 | 26.5258 |
| IsolationReadyToHandoffWrite | 5 | 0.75 | 2.76552 |
| WinEventEndToHandoffWrite | 5 | 26.0221 | 27.99954 |
| HandoffWriteDuration | 5 | 6.0439 | 10.18468 |
| RawReceiptToOwnerQuantum | 90 | 2.8181 | 4.182225 |
| RawReceiptToNativeWrite | 90 | 2.849 | 4.216505 |

### smoke / Release / Move

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 5 | 2.7713 | 6.80898 |
| WinEventEndToIsolationReady | 5 | 3.6405 | 5.85946 |
| IsolationReadyToHandoffWrite | 5 | 0.2167 | 0.36166 |
| WinEventEndToHandoffWrite | 5 | 3.7928 | 6.22112 |
| HandoffWriteDuration | 5 | 4.4782 | 7.79868 |
| RawReceiptToOwnerQuantum | 90 | 2.9733 | 4.09673 |
| RawReceiptToNativeWrite | 90 | 2.9952 | 4.425565 |

### smoke / Release / BottomResize

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 5 | 1.8434 | 2.9687 |
| WinEventEndToIsolationReady | 5 | 13.1028 | 19.79828 |
| IsolationReadyToHandoffWrite | 5 | 0.0737 | 0.27518 |
| WinEventEndToHandoffWrite | 5 | 13.1765 | 20.04196 |
| HandoffWriteDuration | 5 | 10.9261 | 14.4352 |
| RawReceiptToOwnerQuantum | 90 | 2.60095 | 3.675745 |
| RawReceiptToNativeWrite | 90 | 2.65325 | 3.725285 |

### formal / Debug / Move

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 20 | 3.45555 | 4.804785 |
| WinEventEndToIsolationReady | 20 | 6.55545 | 33.39752 |
| IsolationReadyToHandoffWrite | 20 | 0.4544 | 4.702555 |
| WinEventEndToHandoffWrite | 20 | 7.32025 | 39.776595 |
| HandoffWriteDuration | 20 | 5.12925 | 9.2493 |
| RawReceiptToOwnerQuantum | 360 | 3.16395 | 4.51209 |
| RawReceiptToNativeWrite | 360 | 3.20395 | 4.954505 |

### formal / Debug / BottomResize

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 20 | 2.953 | 4.541445 |
| WinEventEndToIsolationReady | 20 | 22.9733 | 30.5347 |
| IsolationReadyToHandoffWrite | 20 | 0.5562 | 1.048975 |
| WinEventEndToHandoffWrite | 20 | 23.3392 | 30.915995 |
| HandoffWriteDuration | 20 | 9.72645 | 14.668905 |
| RawReceiptToOwnerQuantum | 360 | 2.97455 | 4.158075 |
| RawReceiptToNativeWrite | 360 | 3.022 | 4.26351 |

### formal / Release / Move

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 20 | 2.68745 | 4.94865 |
| WinEventEndToIsolationReady | 20 | 4.5845 | 33.503405 |
| IsolationReadyToHandoffWrite | 20 | 0.23485 | 2.33013 |
| WinEventEndToHandoffWrite | 20 | 4.8297 | 39.957475 |
| HandoffWriteDuration | 20 | 5.74635 | 13.221345 |
| RawReceiptToOwnerQuantum | 360 | 2.84085 | 4.337535 |
| RawReceiptToNativeWrite | 360 | 2.8657 | 4.361165 |

### formal / Release / BottomResize

完整成功组，COMPLETE。时间单位均为ms。

| metric | n | p50 (ms) | p95 (ms) |
|---|---:|---:|---:|
| NativeExitToWinEventEnd | 20 | 2.42725 | 4.299545 |
| WinEventEndToIsolationReady | 20 | 14.90105 | 42.68254 |
| IsolationReadyToHandoffWrite | 20 | 0.1384 | 0.33031 |
| WinEventEndToHandoffWrite | 20 | 14.98485 | 42.886525 |
| HandoffWriteDuration | 20 | 8.0382 | 13.72874 |
| RawReceiptToOwnerQuantum | 360 | 2.71475 | 3.7848 |
| RawReceiptToNativeWrite | 360 | 2.74065 | 3.8028 |

## 写入、Raw与资源收尾

| phase / config / op | source total/max | handoff total/max | post-END DRAG total/max | shield total/max | Raw total/max | continuation total/max | writes/quantum max |
|---|---:|---:|---:|---:|---:|---:|---:|
| smoke/Debug/Move | 95/19 | 5/1 | 0/0 | 0/0 | 135/27 | 90/18 | 1 |
| smoke/Debug/BottomResize | 95/19 | 5/1 | 0/0 | 100/20 | 135/27 | 90/18 | 1 |
| smoke/Release/Move | 95/19 | 5/1 | 0/0 | 0/0 | 135/27 | 90/18 | 1 |
| smoke/Release/BottomResize | 95/19 | 5/1 | 0/0 | 100/20 | 135/27 | 90/18 | 1 |
| formal/Debug/Move | 380/19 | 20/1 | 0/0 | 0/0 | 540/27 | 360/18 | 1 |
| formal/Debug/BottomResize | 380/19 | 20/1 | 0/0 | 400/20 | 540/27 | 360/18 | 1 |
| formal/Release/Move | 380/19 | 20/1 | 0/0 | 0/0 | 540/27 | 360/18 | 1 |
| formal/Release/BottomResize | 380/19 | 20/1 | 0/0 | 400/20 | 540/27 | 360/18 | 1 |

总计source writes1900（每gesture最大19，包含handoff与18 continuation），
handoff100（每gesture最大1），writes/quantum最大1，post-END native DRAG总0/最大0，
shield placements1000（每gesture最大20）、Raw packets2700、continuation quanta1800。
Shield维护不计入source geometry writer。每组source/guard实际verified absent、
receiver退出和hook移除数量都等于该组PASS数，合计各100。
50次Move全部ShieldNotNeeded，50次BottomResize的shield全部正确阶段创建并退出。
正常验证、pre/post和source/runtime身份逐项一致；retry0。
这些是本主机/冻结方案/当前条件的工程重复事实，不证明第三方窗口或长期零故障。

## 原字节证据包

打包命令使用冻结`scripts/package-r1c4b-owned-stability-evidence.ps1`，
显式传入上述目录的plan/progress/inventory/statistics/summary/prerequisites及
`-OutputDirectory uat/r1c4b-fixh/20260927T174945663Z-c8e0d10d38c344879d4554be4e019742/review-packages`，exit0。
打包和随后独立ZIP hash读取均PASS；443个原文件，400个本轮run文件+
36个必要G/F原文件+7个控制artifact。完整run不拆分，ZIP读取回算及原文件
打包前后hash一致；无missing、无EXE/截图/用户数据、没有全量uat或外部上传。

| 完整本地包路径 | bytes | SHA256 |
|---|---:|---|
| `D:\repository\panebind\uat\r1c4b-fixh\20260927T174945663Z-c8e0d10d38c344879d4554be4e019742\review-packages\fixh-evidence-001.zip` | 2214108 | 8CC702DCD26EA1A057D9624D757928658C7E805DD3BD1044E1304628A146AB3E |
| `D:\repository\panebind\uat\r1c4b-fixh\20260927T174945663Z-c8e0d10d38c344879d4554be4e019742\review-packages\fixh-evidence-002.zip` | 1886811 | DB3ACB784977282B978263650993C3339BCC005EE3B6235FDC35EF82EB3C59B1 |
| `D:\repository\panebind\uat\r1c4b-fixh\20260927T174945663Z-c8e0d10d38c344879d4554be4e019742\review-packages\fixh-evidence-003.zip` | 1529095 | 64BA71F932612D8D3920BD6FC785B19E41ABDEF723BE5D534CF9ACBC8E34D49E |

外部manifest：
`D:\repository\panebind\uat\r1c4b-fixh\20260927T174945663Z-c8e0d10d38c344879d4554be4e019742\review-packages\package-manifest.json`，
SHA256=`508E775B0BE44A3497C5B521C91120C3A84351AFD8FFBF0B63ED85417D169DC1`。
ZIP内有SHA256 manifest/README与原路径→archive映射，不改日志绝对路径。
**包内BatchResult=PASS只是未改写机器summary的复制；AcceptanceClaim=NONE，
不能代替本报告的NOT_PASSED接受决定。** 唯一软件失败对应的原错误控制文件亦完整保留。

## Git交付与停止

现场和独立汇总期间HEAD固定01633ff，worktree clean；随后仅本报告docs-only结果
提交。交付前标准`git fetch origin`再PASS，origin/main仍为
`81e40facf52ffdb96f76e4d737740167d485f4a7`，实施checkpoint相对它0 behind/33 ahead。
报告提交后预期0 behind/34 ahead，实际final SHA、push/ref核验、
clean/upstream0/0以最终Git handoff为准；不在报告提交内伪造自引用SHA。
原始artifact/ZIP全部ignored，未暂存或提交；所有冻结PS/native文件仍未修复或改变。
Recovery Index INCUBATE与路线图未改。PR/merge/tag/release/Explorer/PureMagnet/
humanUAT均NO。到此STOP。
