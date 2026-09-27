# R1-C4B Pivot 1 Fix E — 64位证据修复与不可变重放

本轮数值修复与原不可变Resize replay均PASS；当前Move单次与四组smoke均PASS。Debug Move formal第14次在取消前因输入干扰BLOCKED，之后全部停止；无重试。原metadata不改，未放宽native/validator行为。

## 范围、历史与结论口径

本轮为 EVIDENCE PIPELINE REPAIR；原 Fix D 的 INVALID_EVIDENCE 是当时真实执行结果，不改写成“当时 runner PASS”，也不再解释为 native runtime 的 geometry FAIL。新 corrected replay artifact独立认定原单次证据。最终稳定门另由当前Move及分组smoke/formal决定。

Starting HEAD `9578284f56b4c43701e7618c4f1b8f744a8481f7`；
branch `codex/r1c4b-live-magnet`；起点clean/upstream0/0。
标准origin fetch PASS；origin/main
`81e40facf52ffdb96f76e4d737740167d485f4a7`；起点0 behind/25 ahead。
Primary `ssh://git@ssh.github.com:443/zynis/panebind.git`，
secondary `https://github.com/zynis/panebind.git` 已核对同仓库，本轮不修改配置。
普通 numeric repair commit `3781d2dd901028a1e10f3f4c400157bba5396256`，
subject `fix: validate 64-bit takeover timing evidence`。

旧A/B/C/D报告、JSONL和metadata保留；R0/Core/product/native runtime未改。
Native CPP SHA256
`CEBB558B403B90D3F1A55C521358884BB6C838624EA6C0BC8C5BA481192BC2FB`，
Debug binary `43289B87236C08E68EAC2A23BCA1AD9923454DDB21EBF4DE84447083933DEC6D`，
Release binary `9755812C25B99B90767844829458CB51F1FA8BB99E9A6B7FDDA213E84A984195`，
均保留原Fix D identity。完整 `git diff a67d304...HEAD -- src` 无变化。

## Numeric repair

见 [numeric audit](../research/R1C4B_PIVOT1_FIXE_NUMERIC_AUDIT.md)；
QPC signed64与PowerShell literal/binder范围已按官方资料和本机PS5.1复核。
无新窗口行为设计、无外部代码复用，沿用Fix D prior-art记录。

```powershell
# OLD
$lastReturn=0
$lastReturn=[Math]::Max($lastReturn,$r.native_return_qpc)

# NEW
[long]$lastReturn=0
[long]$currentReturn=$r.native_return_qpc
if($currentReturn -gt $lastReturn){$lastReturn=$currentReturn}
```

正式 QPC_FIELDS=signed Int64；所有主/嵌套时钟、sequence/watermark原值先检查，
不以typed参数预取整或coerce错误输入。显式long accumulator与signed delta；
geometry Math.Max保留原范围，int仅数组index/Bool等非时钟用途。
negative/noninteger/out-of-range stamp与既有clock reversal拒绝；
合法signed cross-stream latency不被clamp，不新增nativeEXIT≤WinEventEND gate。
`VALIDATOR_SEMANTIC_DIFF=NONE`（只numeric safety、diagnostic/type断言与fixtures）；
Product/隔离/shield/anchor/Bottom edge/exact/RawUP/final/cleanup/≤1write等原条件未改。

157项完整fixtures PASS，保留原87项；新70项覆盖Int32.Max/+1、
3000000000、900000000000、1134816073663、1500000000000、9000000000000，
每个量级完整Move/Resize的全verdict signature和Int64 latency不变。
显式max accumulator/宽duration/signed delta以及negative/type/array/tick/clock负例覆盖。
Validator file SHA256
`9FFB838F9B530257774B1E53B61102E9C99166D0926D37300350DBF7B6A4719D`；
fixture SHA256
`2C971F3C1C646DD5D3198911F5CA9F817ADC151AEDBBDF45312A06AA0075D597`。

## Immutable original replay

唯一次正式replay命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/revalidate-r1c4b-input-isolation.ps1
```

Ignored目录 `uat/r1c4b-input-isolation/`，prefix
`20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b`：
原JSONL SHA256
`BB1E1F90D58A0B21C5A0B3F65F3A3DDB3994388C96FFE4C6A0F3ED5EF174B76F`；
原metadata SHA256
`D1B7C425CDB53A6356BC9B23C72376857BBE12996234F7032A5846B9B15BEB09`。
开始、replay前后及交付前均精确核对；未rewrite/normalize/regenerate/replace原文件。

新 `.revalidation.json` 使用CreateNew，不覆盖历史：
SHA256 `27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F`；
replay_time `2026-09-27T09:31:12.0174272Z`；
evidence_implementation_sha `a67d3049c1027e8b2b82f8e122041b71ff5043ca`；
validator_fix_sha `3781d2dd901028a1e10f3f4c400157bba5396256`，
当前validator file hash上述9FFB...；
semantic_contract_version `post_end_shield_v1`；
numeric_fix_only=true；CorrectedValidatorVerified=true；replay_result=PASS；
first_semantic_failure=NONE。原metadata仍CurrentContractsVerified=false，
Result=INVALID_EVIDENCE。Artifact记录原双hash和source/binary before/after一致。

从原460条JSONL执行完整corrected validator：schema/lifecycle、source/receiver、
WinEvent、Product、isolation、shield/native lifecycle、writers/original anchor、
strict postverify、Rawquanta/UP/finalacceptance、shield teardown/resource cleanup全部重新计算。
不读旧report PASS，不重定义函数绕过断言。只有此后才允许新交互gate。
此处replay命令没有启动source/shield/probe、Raw或SendInput。

新认定范围仅原single Debug Resize：
Cancel PASS_WITH_TERMINAL_SETTLEMENT；Handoff/Takeover/P/V exact PASS；
19 source native writes（handoff1+Raw18），shield placements20，Raw27，
native post-END DRAG0，unownedgeometry0，terminalsettlement1，finalleftfalse。
原source HWND76615376/PID45656/TID33956，shield HWND22876276，同owner/nonce197157086769502。
原P[1126,632,1766,1072]/V[1137,632,1755,1061]；
originalpointer[1339,1071]→final[1339,1191]，fullDY120；
finalP[1126,632,1766,1192]/V[1137,632,1755,1181]。
这些是 corrected validator accepted empirical evidence，不是本轮重新生成的运行。
Single verdict中的architecture字段不等于本轮完整formal gate结论。

## 自动回归与已披露执行顺序偏差

环境：WindowsNT10.0.26200、WindowsPowerShell5.1、VS18 Community/MSBuild18.5.4、
SDK10.0.26100.0、C++20。VS bundled CMake/CTest，MSBUILDDISABLENODEREUSE=1。
Debug/Release完整build exit0，CTest各30/30；命令：

```powershell
cmake --build out/r1c4b-live-magnet-debug --config Debug --parallel 1 -- /nodeReuse:false
cmake --build out/r1c4b-live-magnet-release --config Release --parallel 1 -- /nodeReuse:false
ctest --test-dir out/r1c4b-live-magnet-debug -C Debug --output-on-failure
ctest --test-dir out/r1c4b-live-magnet-release -C Release --output-on-failure
```

Replay前曾误将完整默认CTest当作无GUI回归运行；其中既有
`windows-magnet-owned-probe` 实际创建并显示空白owned geometry窗口，
每配置14次self-owned geometry writes，最后销毁。**这是违反本轮第一阶段
禁止GUI的执行顺序偏差**，已在运行后核对源代码并向用户披露；
没有Raw/Input injection/Explorer/用户已有窗口操作。不能称phase1完全无GUI。
没有将此旧CTest当作Fix D Raw/Move实测或replay替代；后续新Raw/SendInput
交互严格等原immutable replay PASS之后才执行。Source代码未因此改变。

旧offline PowerShell fixtures全部本轮重新exit0：C2B26、C3A61、
C3Bprofile47/frame41/VDM28、C4Acapture4synthetic stages/evidence140、
C4Bevidence29/postverify14/automodal144、FixA take-over142、FixB end-handoff61、
FixC end-diagnostics93。以
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/<test>.ps1`
调用，未传live/GUI开关。修复后FixD/E fixtures157 PASS，
replay assessment50 PASS，numeric runner原helper58 PASS；原runner133项也先回归PASS。
Synthetic tests不代表empirical gate；各后续runner最终回归与实际结果在下一节。

## 当前 Move 与分组稳定门

当前实现/runner checkpoint普通提交：
`a5fe4c2a9f02bd6d67e9b6635f5a417a0bcc0229`，
subject `test: gate owned stability on immutable replay`。
新single绑定validator before/after hash；aggregate独立核对replay artifact、旧双hash、
fix SHA ancestor、当前source/binary、原metadata仍INVALID/false，再从原JSONL全重算，
不用旧metadata当PASS。最终policy190项、numeric98项 root独立复跑PASS，
生产runner独立只读审核PASS。没有改变validator9FFB...或native CEBB...。

### 唯一当前 Debug Move 单次正例

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-input-isolation.ps1 -Configuration Debug -Operation Move
```

Ignored prefix `20260927T094708314Z-Debug-Move-0020e70fff0b4e62be06dbd80395d140`；
JSONL SHA `3D117600C4407893C5F34F269CFC19B5F7CA6571BD08BD5765E6863C8E914932`，
metadata SHA `351EF84F286FD5ACE9F0D05B0B5A26020F0D301261A9B5F2E51532BB1C96A701`。
438 rows；Source HWND21630910/PID48656/TID44744/nonce210460004915747，
DPI192/monitor65537；前后HEAD=a5fe4c2a...、clean、binary/validator hash一致。
Probe0、metadataResult PASS、CurrentContractsVerified/ImplementationUnchanged=true。

Cancel PASS_WITH_TERMINAL_SETTLEMENT、Handoff/Takeover/PreReleaseControl PASS；
source native writes19=handoff1+Raw18，Raw27（movement23/UP2，preflight UP不充终结）；
cursor全程由exact source满足隔离，POST_END_INPUT_SHIELD=NOT_NEEDED，
shield creation/placement0，不为统一流程造shield。
originalpointer[1339,641]→final[1519,641]，fullDX180；
P[1126,632,1766,1072]→[1306,632,1946,1072]，
V[1137,632,1755,1061]→[1317,632,1935,1061]。
Actual Raw UP、finalLeft=false、无pending、strict immediate/full exact、source/guard/
receiver/hook清理均独立通过。Native shutdown旧标签CAPTURED_NOT_ACCEPTED保持；
正式PASS来自独立validator。Old Fix B Move PASS也保持原范围。

### Smoke/formal 精确 progression

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4b-input-isolation-gates.ps1 -PassedResizeRevalidation 'uat/r1c4b-input-isolation/20260926T201554647Z-Debug-BottomResize-3e6236754a004decb816079530b3861b.revalidation.json' -ExpectedResizeRevalidationSHA256 27B68CB5923CC65B52CCB3EEE75F32095D6E320A00E127590660730159613A6F -PassedMoveMetadata 'uat/r1c4b-input-isolation/20260927T094708314Z-Debug-Move-0020e70fff0b4e62be06dbd80395d140.metadata.json'
```

四组smoke均先完成PASS，随后才进入formal。每operation fresh source、guard、
receiver process、WinEvent hook、nonce；Resize fresh shield，Move无需要则不创建。
每项仅一次child与完整独立validator；first failure后没有重试或后续操作。

| Configuration / Operation | Smoke | Formal |
| --- | --- | --- |
| Debug Move | 5/5 PASS | 13/20；第14次BLOCKED，之后6次NOT_RUN |
| Debug BottomResize | 5/5 PASS | 0/20 NOT_RUN |
| Release Move | 5/5 PASS | 0/20 NOT_RUN |
| Release BottomResize | 5/5 PASS | 0/20 NOT_RUN |

Inventory `20260927T094931145Z-fixe-gates-c37c0342d5594a399685b57ceef982f2.json`，
SHA `C97A4C6D02F9B091773E9744B2025DEC0274B3A7F148444B2D67C07209D6257C`；
schema v2、Result STOPPED、34 attempts=33PASS+1BLOCKED。
本轮新 v4 input probe共35次（34次aggregate尝试+唯一singleMove）；
另有replay前两次既有CTest自有geometry GUI，单独披露、不计此v4 probe数。
原Resize只是replay，不算新GUI。
没有将第14次BLOCKED计PASS，没有继续完成所谓20/20。

33批次PASS的meta/log/hash/SHA/clean/binary/validator、唯一nonce和全部raw counters
由第二路只读审计重核；四组×七类QPC ticks、n、p50/p95及operation counters
与inventory逐样本一致。失败也是合法typed BLOCKED，不是INVALID_EVIDENCE；
修正的QPC bug未再出现。

### 首失败与 cleanup 安全边界

唯一失败：Debug Move formal repetition14 / runId
`521d2098b1504bdfaa5366d029a56a9f`。
Prefix `20260927T095946432Z-Debug-Move-521d2098b1504bdfaa5366d029a56a9f`；
JSONL SHA `DACA83DC3AA9C5B43E7295B67E16E242526E240E5429C6C949A93C59C6CD03F8`，
metadata SHA `3F733C50D15DAFADA70D538EFFA33899ECB8C6EC32F25BFC0D28F69B89D53D16`。
Source HWND3806562/PID27332/TID28532/nonce118469386212298；
117行合法且连续。Probe/child runner exit2；Result BLOCKED、
CurrentContractsVerified=true、ImplementationUnchanged=true；
Reason `BLOCKED_BY_INPUT_INTERFERENCE`。

在sample1完成（#83，planned[1348,641]）后、sample2注入前的fresh fence停止。
#84–100五条non-test-tag/device-present Raw movement令cursor偏离到[1276,639]，
cleanup又有一条同类movement到[1258,639]。不能据device-present猜测介入者、
人、硬件或其他软件；失败瞬间具体fence subpredicate也未独立记录，仍UNKNOWN。
没有sample2、cancel、native EXIT、WinEvent END、handoff/Raw writer/shield；
native ENTER1、WM_MOVING7（BLOCK前6、cleanup期1），takeover writes0。
因此不构成END-barrier或takeover-writer geometry反例，round architecture仍UNRESOLVED。

Cleanup #105/#111中exact source=foreground=root，capture是owned source，
但MoveSize仍active、GUIflags2，left high bit=true；故SKIPPED_NO_AUTHORITY，
cleanup_UP_attempted/sent=false、actual cleanup Raw UP=false，
PendingButton=true、FinalLeftDown=null。唯一已有Raw UP属于preflight，不能充终结。
#115 capture0来自资源teardown，不替代UP证据；#117 source/guard/receiver/hook销毁，
shield未建、cursor未恢复、external_windows_touched=false。

**最后可靠日志观察仍为左键按下；当前按钮状态未自动确认。**
已告知用户在安全空白区域手动按下并松开左键，未向未知目标补发UP；
不把资源销毁等同input状态已释放。没有后续GUI/input或cleanup retry。

工具调用层观察top-level非零exit1；原child exit2/inventory STOPPED已确证，
aggregate源码期望exit2。调用层映射原因不能仅由这些记录唯一判定，
TOP_LEVEL_EXIT_CLASSIFICATION=UNKNOWN；未重跑顶层来探测，也未重写exit/verdict。
这不撤销已修numeric pipeline或把BLOCKED冒充PASS。

### 分组 timing / operation storm

以下统计scope=smoke_and_formal_only，仅33个完整PASS；
不混入原Resize replay/初始singleMove，也不把失败阶段空数组当0ms。
每格为p50/p95（ms），signed Int64 ticks排序、decimal线性插值再转ms，无SLA。
正常五类metric n分别18/5/5/5；Raw两类event sample n分别324/90/90/90，
不是相同数量的独立gesture。

| Metric | Debug Move | Debug Resize | Release Move | Release Resize |
| --- | ---: | ---: | ---: | ---: |
| native EXIT→WinEvent END | 2.018500 /4.073905 | 1.568400 /4.036840 | 1.464500 /2.388380 | 1.783100 /1.806300 |
| WinEvent END→isolation ready | 3.313350 /11.715145 | 6.882300 /10.585120 | 1.560900 /2.731160 | 6.876300 /9.703260 |
| isolation ready→handoff write | 0.267650 /0.822800 | 0.280000 /0.309860 | 0.088800 /0.107800 | 0.076300 /0.142880 |
| WinEvent END→handoff write | 3.657800 /12.537945 | 7.144800 /10.869040 | 1.650300 /2.824680 | 6.952600 /9.772780 |
| handoff native duration | 3.580100 /7.471970 | 5.925000 /6.360340 | 2.104100 /4.962860 | 4.933600 /6.641860 |
| Raw receipt→owner | 2.302800 /3.491805 | 2.151450 /3.132805 | 2.088050 /3.039750 | 2.045300 /3.192530 |
| Raw receipt→native writer | 2.327550 /3.565025 | 2.174100 /3.161905 | 2.097450 /3.178395 | 2.055200 /3.210200 |

| Group | PASS gestures | Source writes total/max | Handoff total/max | Quantum max | Shield placements total/max | Raw packets total/max | Raw quanta total/max | post-END DRAG total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug Move | 18 | 342/19 | 18/1 | 1 | 0/0 | 486/27 | 324/18 | 0 |
| Debug Resize | 5 | 95/19 | 5/1 | 1 | 100/20 | 135/27 | 90/18 | 0 |
| Release Move | 5 | 95/19 | 5/1 | 1 | 0/0 | 135/27 | 90/18 | 0 |
| Release Resize | 5 | 95/19 | 5/1 | 1 | 100/20 | 135/27 | 90/18 | 0 |

最大每owner quantum source write1；handoff最多1；source gesture最多19。
Shield不是source write；每source result所需shield reposition恰一次，
由validator完整反向绑定postverify sequence验证，无retry/storm。
失败的precancel native DRAG不混入post-END统计；没有END不能据默认0补barrier证据。

## 最终状态

下方Move/Resize cancellation/handoff/takeover PASS只指已接受的single/完整smoke正例，
不把全部formal置PASS。Formal gate未完成，OwnedFree=FAIL（输入干扰前置阻断），
architecture=UNRESOLVED；不擅自标成native架构被geometry反例拒绝。

```text
FIXE_VALIDATOR_64BIT = PASS
QPC_INT32_OVERFLOW_REGRESSION = PASS
FIXD_ORIGINAL_JSONL_HASH_MATCH = PASS
FIXD_ORIGINAL_METADATA_HASH_MATCH = PASS
FIXD_ORIGINAL_PIPELINE_RESULT = INVALID_EVIDENCE
FIXD_REPLAY_VALIDATION_RESULT = PASS
FIXD_REPLAY_FIRST_SEMANTIC_FAILURE = NONE
FIXD_BOTTOM_RESIZE_SINGLE_EMPIRICAL = PASS
CURRENT_FIXD_MOVE_SINGLE = PASS

OWNED_NATIVE_CANCEL_MOVE = PASS_WITH_TERMINAL_SETTLEMENT
OWNED_NATIVE_CANCEL_RESIZE = PASS_WITH_TERMINAL_SETTLEMENT
OWNED_HANDOFF_RECONCILIATION_MOVE = PASS
OWNED_HANDOFF_RECONCILIATION_RESIZE = PASS
OWNED_TAKEOVER_MOVE = PASS
OWNED_TAKEOVER_RESIZE = PASS

DEBUG_MOVE_SMOKE = 5/5
DEBUG_RESIZE_SMOKE = 5/5
RELEASE_MOVE_SMOKE = 5/5
RELEASE_RESIZE_SMOKE = 5/5
DEBUG_MOVE_FORMAL = 13/20
DEBUG_RESIZE_FORMAL = 0/20
RELEASE_MOVE_FORMAL = 0/20
RELEASE_RESIZE_FORMAL = 0/20

OWNED_FREE_TAKEOVER_GATE = FAIL
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
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

停止后仅只读审计与文档/Git交付。不得本轮修native failure cleanup、放宽fence、
重复第14次或启动Explorer。下一轮若授权，需要先讨论/验证干扰场景下仍在native
modal loop时安全终结test-owned DOWN的策略，不能以自动foreign UP代替authority。
完整formal稳定性还未接受；manual恢复不能替本轮缺失的formal evidence。

## 风险、Git边界与后续

POST_CANCEL_LEGACY_MOUSE_DELIVERY_RISK=NOT_PRODUCT_TESTED；
test-only shield不证明未来physical legacy delivery对underlying windows安全。
VISUAL_TERMINAL_RESTORE_FLICKER=NOT_HUMAN_TESTED；
没有人眼验收/Explorer/PureMagnet、其他apps/mixedDPI-monitor或产品Raw接管。
无product shield、SendInput依赖、global mouse hook、DLL injection、polling。
单次/自动重复PASS不证明视觉体验、race-free delivery或idleCPU/20MB资源目标。

原证据/replay/新run与inventory都留ignored uat/，不进Git。
只普通commits/standard push；不API对象重建，不force/amend/rebase/reset，
不PR/merge/tag/release，不创建下一轮branch。最终commit SHA/remote验证、
clean/upstream0/0与origin/main divergence见交付回执，避免报告自身SHA自引用。
