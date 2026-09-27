# R1-C4B Pivot 1 Fix H — Owned Stability Gate

日期：2026-09-28（Asia/Shanghai）。仅冻结 v5 契约的 owned 重复运行验收；
没有新产品功能、Explorer、Pure Magnet、真人 UAT 或 Recovery Index 更改。

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
本轮现场结论尚待以下结果补充；未以旧报告代替原始证据。

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

Fix H GUI/probe/input均NOT_RUN；implementation checkpoint尚未冻结。
只在先决条件和离线反例全部通过后执行一次上述计划；现场后不修改实现/计划/
轨迹/时限/采样数继续，不进入Explorer或产品阶段。
证据包按本batch显式plan/进度/inventory/统计/summary/finalization与实际attempt
四原文件、新先决条件的必要G/F引用打包；保留字节、绝对原路径不改写，ZIP
manifest另列archive相对映射。含失败及missing声明、不全量uat/EXE/截图，
完整run不拆分、尽量每包≤25MiB；不上传、不提交raw evidence到Git。

```text
FIXE_HISTORICAL_FORMAL = 13_PASS_THEN_BLOCKED_AT_14
FIXF_HISTORICAL_GUI = BLOCKED_BEFORE_INPUT
FIXG_HISTORICAL_BOUNDED = 4_ABORT_PASS_AND_4_NORMAL_PASS
FIXH_BATCH_RESULT = NOT_RUN
OWNED_FREE_TAKEOVER_GATE = NOT_PASSED
RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED
EXPLORER_STAGE = NOT_RUN
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
