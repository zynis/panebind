# R1-C4A Fix 5 — Pre-Accept Mutable Geometry / Structured Capture

日期：2026-09-15。本轮实现与自动验证交接，**不是真人 UAT Seal**。
分支 `codex/r1c4a-dynamic-group-move`；Starting HEAD
`ea372cf71f7128d34ac0b99a6669e604a35ada08`。起点 clean，标准 fetch origin PASS，
local/upstream 0/0；origin/main 为 `81e40facf52ffdb96f76e4d737740167d485f4a7`。
最终 SHA、commit、push 和 divergence 在最终交接记录，避免提交内容自引用 SHA。

## 1. 真人日志 forensic（先于修复，只读）

`uat/r1c4a/20260915T085644241Z-bce8cda6261a4bbfa66947e6f16ab32b.jsonl`
共 20 条，JSONL 合法，sequence 1–20 连续，startup/shutdown 完整。
SHA256：`AA6220980BCD5613560099A8C9E0177E25101A35C9F9C11CB9CD92695E1520E5`。
原日志未删除、改写或提交。

| 记录 | 实际证据 |
| --- | --- |
| 1 | startup：harness PID 40180，owner STA 19764，QPC frequency 10,000,000 |
| 2–10 | A/B/C 提示及确认各 3 次；单目标 token/eligibility/exclusion 确认成功 |
| 3 / 6 / 9 / 11 / 18 | 五次 console wait 全部 complete / error 0；mode 503→503；recheck wait/pump/dispatch 106/106/106 |
| 12 | group_consent confirmed=true |
| 13–15 | 三个 binding，group=1，capability generation=1，consent generation=5 |
| 16 | binding snapshots：Explorer / CabinetWClass，DPI 192，DISPLAY1，work area [0,0,3072,1824] |
| 17 | preview #1 valid=true，但 ready=false / disconnected_topology；relation_count=0；各自独立 component |
| 19 | preview #2 valid=false，snapshots=null，只有 member_validation_failed；没有底层诊断 |
| 20 | shutdown BLOCKED / member_validation_failed / TOPOLOGY_PREVIEW |

第一次成功 capture 的目标：

| 成员 | HWND（十进制） | PID | TID | visible rect |
| --- | --- | --- | --- | --- |
| A | 7343570 | 12664 | 36652 | [94,158,1707,1202] |
| B | 331360 | 12664 | 13548 | [143,207,1756,1251] |
| C | 331448 | 12664 | 34704 | [192,256,1805,1300] |

人工 Move/Resize 是用户报告的操作；旧日志没有第二次实际 rect，不能从它推断
移动了哪个成员、是否跨 monitor/DPI，或哪个 COM 事件被触发。
两次 preview 都记录 gesture_generation/native_apply/HDWP/pending 为 0、
event_source_running=false、Leader=false；没有 accepted baseline 或 runtime batch。

```text
FAILED_MEMBER_INDEX = UNKNOWN
FAILED_CAPTURE_STAGE = UNKNOWN (inside group capture)
EXPLORER_ELIGIBILITY_REASON = NOT_RECORDED
GLUE_VALIDATION_INVALIDATION = NOT_RECORDED
BROWSER_ANCHOR_FACTS = NOT_RECORDED
CURRENT_EVIDENCE_DIAGNOSTIC_RESOLUTION = INSUFFICIENT
CURRENT_HUMAN_CAPTURE_ROOT_CAUSE = NOT_PROVABLE_FROM_EXISTING_EVIDENCE
```

源码路径：preview_readiness → capture → group_binding_matches →
validate_or_retire → receipts_healthy。旧 optional 将所有失败合为 nullopt。
现有 native validator 本来就不冻结 visible/positioning rect；fixture 也正确忽略
这两项。但 monitor/DPI 的普通失败经过 sticky wrapper 会 retire，且原 browser sink
把 NavigateComplete2/OnQuit 外的所有 DISPID 算 malformed。

## 2. 已证明的缺陷与研究边界

[研究、平台签名与测试计划](../research/R1C4A_FIX5_PREACCEPT_VALIDATION.md)记录
AltSnap/FancyZones 固定版本、历史与许可证，Microsoft Learn 和 SDK 四事件签名。
无上游代码复制、翻译或改写。

修复前对 **实际 production sink** 的合成 IDispatch 调用运行 429 checks，404 failures：
四种已知合法几何事件各 100 次被拒绝，且各自 malformed 计数非零。修复后加上
几何计数检查，433 checks / 0 failures。测试没有创建/操作 Explorer。
因此可证明 sink 的协议分类缺陷；**不能证明旧真人日志的失败就是这个原因**。
官方 WebBrowser property 事件说明不等价于 Windows 11 Explorer 的已观测发送行为。

## 3. Fix 5 contract

- 私有 `GroupCaptureMode::PreAcceptMutableGeometry` 不走 sticky
  validate_native_target/validate_or_retire wrapper，但调用同一个完整 inner validator。
  token、consent、HWND/PID/TID/root/owner/class/image、canonical anchor、nonce location、
  identity/security/session/integrity 与 current desktop 仍全部验证。
- x/y/width/height/visible/positioning 持续 fresh capture，不进入 immutable context。
  手工预接受 Resize 只是 fixture arrangement，**不是 C4C Glue Resize**。
- 可恢复仅限 MonitorChanged/DpiChanged：Native eligibility 的这些返回位于全部安全
  验证之后，还需 anchor/browser proof、再次 binding 与三成员 receipt health 检查。
  有任一成员 fatal 则优先 fatal。移回**原** monitor/DPI 才可接受，不重设 monitor anchor，
  不开放 mixed-DPI/multi-monitor runtime。
- token/identity/navigation/quit/security/desktop/stream/未知上下文或不可用 geometry
  仍 fatal/retire/group poison；错误线程不触碰另一线程的 ledger。非连通 topology
  是 NOT READY，不 retire。不可恢复条件有明确结构化原因。
- Y 后再次完整 fresh pre-accept capture，冻结 accepted baseline，再启动 source。
  pre-accept native writes/gesture generation/pending/Leader/source running 仍全为零。
  active capture 默认 Strict，monitor/DPI failure 仍 fatal；ResizeOrMixed 仍按旧 runtime abort。

## 4. 诊断与事件分类

`GroupCaptureResult` 保存失败 member/stage、eligibility enum/code、数值 diagnostic
domain/code、glue invalidation 和三成员 bounded browser/anchor facts。
日志 `capture_contract=structured_preaccept_v1`；每个 preview/accepted 都附 `capture`。
Binding / NativeValidation / ReceiptHealth / Context 可区分，无法归属成员的 group-level
Context 不臆造 index。已知四个 geometry DISPIDs 只计数，不保存参数值或 URL，不增加
navigation callback/latest sequence，不作为 identity 或 navigation authority。
未知 DISPID、错误参数/IID/flags、wrong thread、post-retirement、quit、错误 navigation
identity 仍 fail closed。共享 sink 的这一显式分类修正也经过旧轮回归，不能声称整个
shared browser 文件字节不变。

serializer 仅固定标识符、数值、布尔和 null；不输出 ExplorerDiagnostic 的 api/detail、
自由路径、用户文件、clipboard 或输入字符。字符串标识有字符集和长度边界。
PowerShell 验证器拒绝新增自由文本字段、矛盾的 fatal/recoverable/succeeded、缺失
成员、错误 anchor/epoch 和不健康 stream。旧 BLOCKED 日志仍可读，输出诊断不足；
旧无 capture contract 的 PASS 不可升级为 Fix 5 PASS。

## 5. 自动验证（非真人 UAT）

环境：Windows 10.0.26200；PowerShell 5.1；VS 18 2026 / MSVC 19.50；SDK 10.0.26100.0。
复用 `out/r1c4-topology-debug` / `out/r1c4-topology-release`，限制两并发、禁 node reuse。
具体命令：

```powershell
$cmake = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$env:MSBUILDDISABLENODEREUSE='1'
& $cmake --build out/r1c4-topology-debug --config Debug --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4-topology-debug -C Debug --output-on-failure
& $cmake --build out/r1c4-topology-release --config Release --parallel 2 -- /nodeReuse:false
& $ctest --test-dir out/r1c4-topology-release -C Release --output-on-failure
# 各配置运行两个受控自测：
& "$build/src/platform/windows/$config/panebind-owned-window-harness.exe" --self-test
& "$build/src/platform/windows/$config/panebind-companion-harness.exe" --self-test
# 下列各 runner 均使用 powershell.exe -NoProfile -ExecutionPolicy Bypass -File：
scripts/test-r1c2b-evidence-runner.ps1
scripts/test-r1c3a-evidence-runner.ps1
scripts/test-r1c3b-profile-runner.ps1
scripts/test-r1c3b-frame-profile-runner.ps1
scripts/test-r1c3b-vdm-profile-runner.ps1
scripts/test-r1c4a-evidence-runner.ps1
scripts/test-r1c4a-capture-json.ps1 -BuildDirectory out/r1c4-topology-debug -Configuration Debug
scripts/test-r1c4a-capture-json.ps1 -BuildDirectory out/r1c4-topology-release -Configuration Release
```

当前已验证：Debug/Release CTest 各 23/23，四项 Owned/Companion PASS；browser sink
433 checks PASS；mutable capture 69 checks PASS；C4A runner 140 fixtures PASS；
实际 C++ serializer → PowerShell 四种 stage 在 Debug/Release 都 PASS。
旧轮 runner 最终汇总：C2B PASS；C3A PASS；C3B Phase 1 PASS；Phase 2 PASS（41）；
Phase 3 PASS（28）。所有必需自动回归已完成。原日志最终只读重放仍为 BLOCKED /
member_validation_failed，CaptureDiagnostics.Resolution=INSUFFICIENT，底层字段为 null。
一次临时 round-trip 命令因 PowerShell 执行策略未加载 validator，其继续输出的
“PASS”**不计为证据**；已新增 ErrorActionPreference=Stop 的正式脚本并用进程级
ExecutionPolicy Bypass 重跑上述两配置。未修改全局执行策略。

## 6. 冻结与待验收

C4B Magnet header/ranking/Move/Resize/multi-axis/fast policy 及 Core behavior 无改动。
HDWP batch、group event source、Ctrl/console helper、C3B Glue coordinator 未改动。
GroupSession 从 attribute 到 quantum/gesture/restore 的 active 代码字节等价；改动
集中于 pre-accept、capture result plumbing 和共享 browser 的四事件协议识别。

仍 NOT TESTED：修复后的真人 Explorer 重测、真实临时跨 monitor/DPI 后移回、
本次旧日志的确切底层原因、真实 Explorer geometry DISPIDs/时序、交互顺滑度。
需要独立 review 后由用户执行真人 UAT；新日志现在能够指出具体失败成员/阶段/原因。
不要求或代执行真人测试。C4B live / C4C / C4D、其他应用、Z-order 均未开始。
完成标准 push 后 STOP；不创建 PR，不 merge/tag/release。
