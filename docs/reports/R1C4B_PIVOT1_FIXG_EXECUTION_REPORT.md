# R1-C4B Pivot 1 Fix G — Execution Report

日期：2026-09-27–28（Asia/Shanghai）。仅既有 owned test probe 的输入前阻断验算、独立收尾和
只读启动检查。非产品阶段；未改变 Recovery Index 的 INCUBATE 或长期路线图。

## 起点与研究边界

Branch `codex/r1c4b-live-magnet`；starting HEAD
`7d026c2933acef865b23edd9587098d612098a31`，起点 worktree clean。
标准 SSH-over-443 `git fetch origin` PASS；实际 origin URL
`ssh://git@ssh.github.com:443/zynis/panebind.git`；origin/main
`81e40facf52ffdb96f76e4d737740167d485f4a7`，`origin/main...HEAD` 为
0 behind / 30 ahead，当前 upstream 0/0。未修改 transport/config/history。

复用已核验 AltSnap/FancyZones pinned research，仅补查五份官方 capture/
foreground/input 合同；[provenance](../research/SOURCE_PROVENANCE.md) 记录
精确 pin、history、许可、实际复核边界与无代码复用。研究门 PASS 仅支持本轮
test-only 设计，不代表现场 cleanup 或完整接管 gate 通过。

## 实际实现范围

`r1c4b-native-abort-validation.ps1` 在 envelope/身份/顺序验证后，按实际
bootstrap 阶段分派，新增狭窄 `BOOTSTRAP_BLOCKED_BEFORE_ANY_TEST_INPUT`。
无后续 fence diagnostic 不再先抛异常，但缺诊断本身绝不是 BLOCKED 的证据。
必须验证实际非自有 capture、成功稳定查询、前台尝试、topmost restore、
source/guard/receiver/hook lifecycle、双 owner ACK、strict cleanup 无输入，
并拒绝任何输入/native/fault/cancel/writer/acceptance/shield/未知或矛盾记录。
Prefix VERIFIED 仍是 fixture BLOCKED；完整 normal/abort gates 不降级。

Single runner 采用一次 native → 一次独立 post → 独立 after identity/hash →
validator → metadata。native reason、validator error、post/identity errors
分别保留；未收集 unchanged 为 UNKNOWN。Aggregate 重新核验证据与身份，
valid BLOCKED 和任何未完备证据都 STOP，不启动下一项或重跑当前项。

Readonly helper 为 v2 / `foreground_gui_readiness_v1`。一次有界检查按
before context → 11 fixed key observations → foreground tuple → explicit
TID GUI query → foreground tuple → after context 顺序；记录 QPC/error/null。
Buttons 可靠观察与 startup readiness 独立；capture 非零仍可 OBSERVED_UP。
READY 要求 context/tuple 一致、全部输入 UP、GUI query 成功、capture/menu/
move-size clear、禁止模式位 clear。只证明此观察点，不是全局原子保证。
旧 v1 仅按旧合同解释历史按键，不可授权当前 startup。Native fresh fences
保留，只添加 query/tuple/time/error/细分原因日志；成功授权表达式不变。

```text
PRODUCT_HANDOFF_SEMANTIC_DIFF = NONE
GEOMETRY_WRITER_SEMANTIC_DIFF = NONE
OWNED_NATIVE_ABORT_CONTRACT_CHANGE = NONE
BLOCKED_PREFIX_CLASSIFICATION = ADDED
RUNNER_FINALIZATION = REPAIRED
STARTUP_READINESS_CONTRACT = EXTENDED
```

## 不可变历史证据

Fix F original directory（ignored local evidence）：
`uat/r1c4b-fixf/20260927T132106696Z-Debug-Move-controlled-abort-c2473a89d3db4c1da41ceb0ba51b1cb6/`。
Original inventory：
`uat/r1c4b-fixf/20260927T132105318Z-bounded-eight-c7d46eec9ec2462e95df6b3badc8fe18.json`。

| 原文件 | 固定 SHA-256 |
|---|---|
| probe.jsonl | 09ECEDC632F91B4FAEB7CFA00682B7227827FE57AA8EDB79BB6701D68576B09D |
| run.metadata.json | 010322412D0E47E4BD4C52F230BD94325E530B3AA14E9E9F395E1076210F394F |
| environment-pre.json | 089CB6A07FCF1B6465861BE33B2DA4C8A2F1A35D52E010BCAFF02BE9231C5562 |
| environment-post.json | 760621B0D8B4DBBF1940825DD9F35A4365C36F694C162C740BA9E83CE8D36458 |
| original inventory | 5CBAF79AB38296A8800A7190EDF37EDCBF9D024E74C7CEEE10EA1179668F2253 |

本轮起始独立重hash全部匹配。旧 D/E 六份及 frozen Fix E validator hash
亦匹配原 inventory；历史 verdict 不改写。证据 implementation SHA
`2f06924e88b5bef81b35e1d2414aab9b4c6d4f0f`，原 metadata 的 Result、
AfterHEAD、AfterIdentity 均为 null；没有今天补造旧 AfterIdentity。

原 28 条 continuous v5 JSONL 亲读结果：source HWND13571864/PID56980/
TID46328；seq8 SetForegroundWindow=false；seq12 activation_fence move 的
foreground854960，GUI query success/stable，capture71435280（非 source），
menu/move-size0、flags0。seq13 有实际 topmost 恢复 receipt；seq14–15
bootstrap/blocked 一致。所有注入路径调用记录、native ENTER、controlled
fault、cancel、source writer、acceptance、shield 均0。
seq16–18/23–25 actual owner retirement ACK，seq26 hook removed，seq27
receiver registration removed/window destroyed；seq28 source/guard absence
checks。source/guard 没有单独 DestroyWindow receipt，不虚构这类记录：
以固定 emitter 的实际 absence checks、已知创建身份、owner quiescence 与
原 process exit2 的联合 lifecycle 证据解释，不单信 shutdown 汇总。
原 post v1（FG854960/PID22596/TID17040，QPC
1749937750719–1749937805992）在该点可靠观察全部11输入 UP；未查 capture。
旧 activation fence 没有 query TID/QPC/error，不补写或回溯新字段。
输入来源/capture 持有原因 UNKNOWN。

## 自动验证与独立 replay

本轮 AUTOMATED TESTED：Debug/Release builds 均 exit0；正向 offline CTest
各24/24 PASS，31个已分类注册中的7个 interactive 被明确排除。
selection74 checks PASS；prefix83 checks PASS；runner363 checks PASS；
旧 full v5 normal/controlled-abort138 checks PASS；历史 D replay PASS、E
仍 BLOCKED / SKIPPED_NO_AUTHORITY。没有默认全量 CTest、GUI 或输入测试。

环境：Windows10.0.26200，VS18 2026 x64 / MSBuild18.5.4，SDK10.0.26100.0，
CMake/CTest4.2.3-msvc3，Windows PowerShell5.1.26100.9444。
实际命令（`<cmake>` 为 VS bundled CMake 的绝对路径）：

```powershell
<cmake> --build out/r1c4b-live-magnet-debug --config Debug --parallel 4
<cmake> --build out/r1c4b-live-magnet-release --config Release --parallel 4
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-offline-tests.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-offline-tests.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-offline-selection.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-native-abort-validation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-fixg-bootstrap-validation.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-input-reliability-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4b-fixf-history.ps1
```

`<cmake>` =
`D:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`。
Prefix 反例覆盖所有注入类型（连 attempted但sent0也拒绝）、真实 ENTER/fault/
cancel/writer/shield、sequence/lifecycle/nonce/exit、capture0/unknown/source、
GUI查询失败/foreground变化、topmost恢复、owner ACK、旧格式降级和guard实际
coverage矛盾。Runner fixtures 对真实 helper 使用内存 seams，证明 validator/
post/HEAD/worktree/单个hash失败时其它收尾仍执行、一次post、true/false/UNKNOWN、
原native reason与多错误并存、valid BLOCKED STOP2、v1历史UP不可授权v2 startup。
这些 synthetic fixtures 均不是现场观察。

首次只读 replay artifact：
`uat/r1c4b-fixg/20260927T155624392Z-fixf-original-prefix-precheckpoint-8006364ded4b40f8a75905b105b471af.json`，
SHA-256 `40846A10A484B1A18D8FA7144FF1721500E354A8492DFD8164468B4FC31A5338`。
实际命令为 `revalidate-r1c4b-fixg-bootstrap.ps1 -ArtifactPath <该新文件>`。
证据 integrity VALID，prefix VERIFIED_BLOCKED_BEFORE_INPUT，execution/
fixture BLOCKED，gesture/cleanup/takeover NOT_RUN，InputAttempted与
TestDownPending=false，ContractVerified=false，独立历史post buttonsUP。
历史11文件前后hash匹配；原Result/AfterHEAD/AfterIdentity仍null。
该artifact如实绑定 BASE+dirty classifier files hashes，未将BASE冒称新实现SHA。
Clean checkpoint后已生成第二份独立 readonly artifact：
`uat/r1c4b-fixg/20260927T160524985Z-fixf-original-prefix-frozen-a39cc6d4035d4dc997d6bf12cb63a224.json`，
SHA-256 `4444A4AEA058828DCFE8B42C31CF8F47AE20E4EF0BFBABED35A2A3BB5A3A47F6`，
ClassifierHEAD=`5fc0ccb575b82940b78e4a8e4cbac916e12a2b4c`、classifier
worktree=[]、ClassifierSHAContainsCurrentFiles=true；结果保持 VERIFIED prefix /
BLOCKED。未修改首次artifact或旧证据；新现场批次结束后11历史文件再重hash亦全部匹配。

审计 native 八项product/writer函数与BASE逐正文相等，abort/quiescence/ledger
块同样相等。Frozen Fix E validator SHA-256 `9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D`，
normal primitives `064B9C1EDAC92C7A7F011FDF7F8B01D15D339D0214CF21436F753E8876483D6E` 未变。

## 本轮现场计划与 stop point

离线 gates 与原 evidence readonly replay 完成后，冻结 clean implementation
commit `5fc0ccb575b82940b78e4a8e4cbac916e12a2b4c`：
`test: verify bootstrap blocks and independent input probe finalization`。
只执行一次 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File
scripts/run-r1c4b-input-reliability-gates.ps1`，正常交互环境中经授权运行；
没有新 desktop/VM、配置修改、预先试跑、失败重试或额外 readonly CLI。
8项全部按登记顺序执行，GUI_PROBE_INVOCATIONS=8/8，TARGET_GESTURES_ENTERED=8/8。
本节为 AUTOMATED TESTED / 实际 Windows 观察，不是 synthetic 或真人 UAT。

| # | 配置 | 操作 | 模式 | Fixture / Gesture | Cleanup | Takeover | Probe → runner → aggregate |
|---:|---|---|---|---|---|---|---|
| 1 | Debug | Move | controlled abort | PASS_EXPECTED_ABORT / BLOCKED_BY_TEST_FAULT | PASS | NOT_RUN | 2 → 0 → 0 |
| 2 | Debug | BottomResize | controlled abort | PASS_EXPECTED_ABORT / BLOCKED_BY_TEST_FAULT | PASS | NOT_RUN | 2 → 0 → 0 |
| 3 | Debug | Move | normal | PASS / PASS | NOT_NEEDED | PASS | 0 → 0 → 0 |
| 4 | Debug | BottomResize | normal | PASS / PASS | NOT_NEEDED | PASS | 0 → 0 → 0 |
| 5 | Release | Move | controlled abort | PASS_EXPECTED_ABORT / BLOCKED_BY_TEST_FAULT | PASS | NOT_RUN | 2 → 0 → 0 |
| 6 | Release | BottomResize | controlled abort | PASS_EXPECTED_ABORT / BLOCKED_BY_TEST_FAULT | PASS | NOT_RUN | 2 → 0 → 0 |
| 7 | Release | Move | normal | PASS / PASS | NOT_NEEDED | PASS | 0 → 0 → 0 |
| 8 | Release | BottomResize | normal | PASS / PASS | NOT_NEEDED | PASS | 0 → 0 → 0 |

新 immutable inventory：
`uat/r1c4b-fixg/20260927T160611768Z-bounded-eight-8499fca50d464940a191f5f100ebcd5e.json`，
SHA-256 `9FAA3A049D027079F9026C77B34213ECEBC8AC5C9D53C75708AC6C54A1F01A37`。
Result PASS、AggregateExitCode0、ExpectedAbortPassed4、NormalPassed4、
FirstUnexpectedFailure=null。各run四文件独立，inventory保留实际完整路径/hash、
child output、raw verdict 和分层exit；uat未入Git。

每项fresh source/guard/receiver/hook/nonce，八个nonce不同；需要shield的两个
BottomResize normal也各自fresh。八项 pre/post v2 raw proof均READY/11 inputsUP；
AfterHEAD均实施SHA，worktree均[]，Before/After各19项source/import/binary hash
与冻结inventory以及批次后当前文件一致。metadata/log/pre/post hash再次独立核对
全部一致；NativeError/ValidatorError/PostObservationError/AfterIdentityError均空。

Abort实际记录引用（均各一条ENTER、fault、cleanupUP、nativeEXIT）：

| 项 | HWND / PID / TID | ENTER → sample1 → fault | cleanupUP API / RawUP proof | nativeEXIT / WinEND wait / cleanup final |
|---|---|---|---|---|
| Debug Move | 9443040 / 37880 / 39116 | 77 → 86 → 87 | 97 / 99 | 101 / 104 / 109 |
| Debug BottomResize | 9247512 / 55628 / 54452 | 68 → 85 → 86 | 96 / 98 | 100 / 103 / 108 |
| Release Move | 2883648 / 49924 / 33240 | 77 → 86 → 87 | 97 / 99 | 101 / 104 / 109 |
| Release BottomResize | 84349162 / 31904 / 50544 | 68 → 85 → 86 | 96 / 98 | 100 / 103 / 108 |

真实自有DOWN/native loop后、sample1后、cancel前触发已有test-only fault；
cancel_calls0，单次 flags4 cleanup tag `0x50424655` UP 与实际RawUP关联，
native EXIT 与 matched WinEvent END属于cleanup scope且不授权acceptance；
final capture/mode clear、ledger settled、writer quiescent、无pending work，
hook/receiver/source/guard有序退出。Raw回执可先于API父日志，不依父行号伪造时序；
既有ledger observed QPC/receiver watermark验证保持。
Normal四项完整旧合同复算：original anchor、Bottom非参与边固定、WinEvent END
barrier、product/test authority隔离、strict P/V、Raw continuation/UP、final
acceptance及资源退出。Move两项各485行、BottomResize两项各506行，均一条真实
ENTER/EXIT；未降低成功合同。

最新观察仅引用第8项
`uat/r1c4b-fixg/20260927T160918920Z-Release-BottomResize-normal-ce3d01ec6e53452c9cb609ca2128ac84/environment-post.json`。
SHA-256 `3681CF602525FD2D052299A09C1F6F6957AFBE71E891977868B6B946749E756B`。
只读helper PID13548/TID51484，QPC frequency10000000，
完整范围1850902423375–1850902540449；11输入QPC
1850902508234–1850902509306全部UP。FG HWND199402/PID34576/TID14200前后
一致；显式query TID14200，query QPC1850902509420–1850902509721、success/
error0，capture/menu/move-size/flags均0，12谓词全PASS、first_failed=NONE。
文件LastWriteTimeUtc=2026-09-27 16:09:27（文件时间，非捏造观察wall timestamp）。
因此 CURRENT_STARTUP_READINESS=READY（仅该观察点），FINAL_BUTTON_STATE=
OBSERVED_UP，来源READ_ONLY_POST，不声称之后永远安全。
没有证据识别旧 capture 成因或任何外部输入actor，继续UNKNOWN。

这四种配置/操作组合取得native-abort cleanup现场正例，四项normal未发现回归；
没有复现或解释Fix E第14项未知干扰，没有拼接其13次，没有跑5/5、20/20或formal。
本轮8项不是完整稳定性证明，formal与完整OWNED_FREE_TAKEOVER_GATE仍未通过。

## 最终状态与 Git 交付

```text
FIXG_BLOCKED_PREFIX_VALIDATION = PASS
FIXG_RUNNER_FINALIZATION = PASS
FIXG_READONLY_CAPTURE_READINESS = PASS
FIXF_ORIGINAL_EVIDENCE_HASHES_UNCHANGED = YES
FIXF_ORIGINAL_BOOTSTRAP_PREFIX = VERIFIED_BLOCKED_BEFORE_INPUT
CURRENT_STARTUP_READINESS = READY
GUI_PROBE_INVOCATIONS = 8/8
TARGET_GESTURES_ENTERED = 8/8
FIRST_UNEXPECTED_FAILURE = NONE
FINAL_BUTTON_STATE = OBSERVED_UP
FINAL_BUTTON_OBSERVATION_SOURCE = READ_ONLY_POST (item 8 v2 evidence)
OBSERVED_EXTERNAL_INPUT_SOURCE = UNKNOWN
```

Starting SHA见上；implementation SHA为5fc0ccb完整值。最终提交只记录本报告
的已观察结果，不再修改实现、fixtures或runner；final SHA为包含本报告最终状态
的docs completion commit（自身SHA由最终交付给出，不虚构self-reference）。
标准Git push/remote ref核验在该提交形成后执行；精确final SHA、push结果、
worktree和local/upstream divergence以最终交付的实际Git输出为准。
不PR/merge/tag/release；完成推送核验后STOP，不启动下一阶段。

```text
FIXE_HISTORICAL_FORMAL = 13_PASS_THEN_BLOCKED_AT_14
FIXF_HISTORICAL_GUI = BLOCKED_BEFORE_INPUT
FIXG_FORMAL = NOT_RUN
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
