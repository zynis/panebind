# R1-C3B Phase 2 — Frame Authority Amendment 执行报告

日期：2026-09-10。自动实现/验证完成；真人顺滑度仍为 PENDING_UAT。

## 授权与历史

本轮使用 Human Root 明确修订的 top-level-frame authority，不声称与旧
single-entry steady-state rule 等价。上一轮的停止是正确的：
OLD_FAST_PATH_PROOF=FAILED，SEMANTIC_EQUIVALENCE_TO_OLD_RULE=NOT_PROVEN。
[旧反例与阻断记录](R1C3B_PHASE2_EXECUTION_REPORT.md)已原样 checkpoint 并推送，
不是 reset/clean/checkout 后重新制造的历史。

Starting HEAD：14e0c6818894c6912d3b3cb4d5a922f427b4e6ce。
Branch：codex/r1c3b-smoothness-profile。
Main baseline：d901094404f04772e1d641842df76d13c551a734。

| Commit | 内容 |
| --- | --- |
| 2cc464119f3fa4093b20b54782c093c50ffdfa4c | 上轮 2 modified + 3 new docs-only checkpoint；标准 push 成功 |
| deda63e | Human Root frame-authority 决策与 Architecture/contract/register 更新 |
| d8bdc0ba3ec7ef625081eacf49113c8293e51268 | 实现、自动测试、runner 与可移植派生 baseline；本轮 UAT implementation SHA |

最终 handoff commit 只更新文档。最终 HEAD、standard push 与 clean/upstream
状态由交接消息记录；不得把文档后继误写成不同的已测试实现。

## 实际实现边界

[正式决策](../architecture/R1C3B_FRAME_AUTHORITY_DECISION.md)区分三层：
canonical Shell anchor A；实际 native frame F；generation-scoped capability。
初始 baseline exclusion、用户创建的新 frame、nonce、唯一 candidate、单 entry
绑定、canonical identity 与全部 native eligibility 未放松。

Phase 2 显式入口在严格 token issuance 后启用 frame contract。Readiness/
pair/setup/arm/START 与 END/restore 可使用完整 inventory，但只审计原 frame
存在，不再把同 frame 后增 entry 或其他 frame 的相同 location 当作权限转移。
anchor A 的身份、当前位置与生命周期必须始终有效，不能由 B 替换。

稳态 active quantum 的两侧 capture、Follower prepare/immediate/postverify
以及额外 Leader pre-native witness 不调用全局 inventory。模式为：
FULL_GLOBAL_INVENTORY / CONSENT_BOUND_FRAME_FAST。模式/phase 的 evidence
不签发任何 capability；私有 session/ledger/seal/permit 才是 native 权限来源。

保留每次直接 anchor current_location 的 filesystem identity、current HWND、
canonical IUnknown、token/session/consent/capability generation、PID/TID/retained
process、image、class/root/owner/style、security/integrity/session/UIAccess/
AppContainer、visible/cloak/min/max、virtual desktop、positioning/DWM bounds、
monitor/work area/PMv2/DPI 和 exact postverify。Core/MovePlan、initial-relative
geometry、Ctrl START latch、feedback ledger/watermark、八消息 fairness、queue
drain/coalescing 均不改。SetWindowPos 仍是 NOSIZE|NOZORDER|NOACTIVATE，无 ASYNC。

两个 anchor 独立，即使同 PID。Native 前先刷新 Leader，再做 Follower immediate
validation；随后无 COM/pump 地检查两侧浏览器 receipts 与已排队的 target
destroy。已观察的 destroy/identity-stream failure 退休 token，阻止 numeric
HWND reuse 在 active 或 cleanup 中复活旧权限。队列扫描不消费、不重排 receipts。
WinEvent receive_raw_event 的正文与起始版本逐字比较一致；Browser Invoke 未改。
新增工作只在 owner 侧的验证边界运行，不引入 polling/event source。

普通 C2A/public one-shot 与 legacy 路由保留旧 full-validation contract；C2B
console、C3A、Phase 1 profile executable 保持默认行为与原函数签名，通过显式
重载启用新模式，不要求旧 C2B 使用 Ctrl 或默认 profiling。

USER_PREEXISTING_WINDOWS_TOUCHED=NO 精确表示：没有对未经授权的独立顶层
native frame 发起操作；不表示授权 frame 内永远只有一个 Shell entry。
本轮没有操作第三方窗口，仅运行 PaneBind 自有/companion 自动 fixture。

## 自动测试与审查证据

| Gate / 覆盖 | 结果 |
| --- | --- |
| Debug build / CTest | PASS / 13 of 13 PASS |
| Release build / CTest | PASS / 13 of 13 PASS |
| Owned Debug / Release --self-test | PASS / PASS，failures=0 |
| Companion Debug / Release --self-test | PASS / PASS，failures=0 |
| C2A/C2B/activation/Core/profile 既有 C++ regressions | 包含于上述 CTest，PASS |
| 旧 runner fixtures | 26 C2B + 61 C3A + 47 Phase 1，全部 PASS |
| 新 frame-profile runner fixtures | 41 PASS，包含端到端离线 positive/negative、delayed/native/postverify arrival、plain/late Ctrl |
| 初始 multi-entry | 原 candidate evaluator 仍 REJECT |
| 发证后 same-frame 1 -> 2 | 原 full rule REJECT 历史保留；新 frame predicate/audit ACCEPT |
| 不同 frame 同 nonce | 原 token 不跨 ledger；原 frame 唯一 target；owned G 未收到 native placement；activation 不继承 |
| 导航/无事件 location mismatch/quit/rehost | 两侧模型均 abort，后续 modeled writes=0，健康后续 facts 不复活失效 |
| A quit、B/F 仍在 | 原 anchor 失效，不能提升 B |
| HWND reuse | retire_native 后旧 token 无效；相同数字 HWND 的新 token 不继承 generation |
| malformed/overflow/wrong-thread/unadvised/retirement/pending browser receipt | 全部 fail closed |
| 已排队 destroy | Leader/Follower 独立检测，不消费 queue；无关 G destroy 不触发目标失效 |
| Native predicates | 20 正向必需项 + 8 禁止状态在新旧 native model 下保持相同拒绝结果 |
| Profiling OFF/ON | 同一 Core/owned fixture 均 12 progressive applies、12 exact ACK、相同几何/native flags/final/restore；plain/late/resize 无 active writes |
| 计数 fixture | 16 × 5 个 modeled validation requests：旧 80 / fast 0；不是 wall-clock 加速证据 |
| 数据来源核对 | 原日志 SHA256 未变；派生 baseline 的 23 个指标重算逐项一致 |
| Review | 没有 active inventory、raw-HWND capability、entry/frame 权限转移、时间缓存、跳过 location/generation/exact postverify；Core 无 diff |

开发中编译发现旧 readiness 函数指针要求两参数签名，因此保留原函数并增加显式
重载；新 destroy fixture 的变量名错误也已修正后重新通过双配置构建/CTest。
最初 runner 拒绝未知 validation record；现只在新 schema 中接受，旧 schema 不放宽。
这些中间失败不被省略，也不构成最终 PASS 的替代证据。

环境：Windows 10.0.26200，VS 18 2026 x64 / MSBuild 18.5.4，MSVC 19.50，
Windows SDK 10.0.26100.0，Visual Studio bundled CMake 4.2.3-msvc3。
CMake/CTest 使用目录：
D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin。

执行命令（cmake/ctest 使用上面的完整路径）：

~~~powershell
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
~~~

## 性能比较口径与未测试风险

Phase 1 的真人结论仍是 C；shell_inventory 为 operation-local exclusive 的
82.93%，16/16 largest。稳态 quantum 范围内 inventory 86 次、exclusive
3942.6706 ms；全 profile 92 次。Quantum span p50/p95 为 218.5243/428.0875 ms，
max queue 51，apply interval p50/p95 为 304.4571/532.0436 ms。

Runner 自动比较的 baseline 是从原始日志派生的
[numeric JSON](R1C3B_PHASE1_DEBUG_PROFILE_BASELINE.json)，保留来源 SHA256，
不提交 raw logs、HWND、用户路径。按 invocation/operation/quantum 分层；
exclusive 只减直接子 span，不叠加重叠父子时间。新 run 会打印各 phase inventory
调用、steady-active exclusive、quantum/queue/coalescing、apply interval、
receipt->owner、owner->native、native、postverify-only、native-start->postverify、
receipt->postverify 和 largest steady-active stage。

ACTIVE_GLOBAL_INVENTORY_TARGET=ZERO 是实现路径及确定性 Gate；真人实际计数
仍要在唯一 Debug UAT 中验证。没有 display-present/FPS 测量，没有提前优化
virtual desktop、native placement、message loop 或第二热点。
真实 Explorer tabs/rehost/quit 时序、端到端 smoothness、profiling 扰动和长期
resource usage 仍 NOT TESTED。不可撤回已进入的 native call；已交付失效信号
阻止后续 active write，不声称瞬间知道未交付的 OS 事件。

只交付[一次 Debug optimized UAT](R1C3B_PHASE2_DEBUG_UAT_HANDOFF.md)。不创建
PR、不 merge、不 tag/release、不代替用户拖动、不要求制造多 tab 场景。

## 自动 Gate（真人运行前）

~~~text
R1C3B_PRIOR_BLOCKED_PROOF_PRESERVED = YES
R1C3B_FRAME_AUTHORITY_AMENDMENT = PASS
R1C3B_INITIAL_SELECTION_UNIQUENESS = PRESERVED
R1C3B_POST_ISSUANCE_FRAME_AUTHORITY = PASS
R1C3B_CANONICAL_ANCHOR_GATE = PASS
R1C3B_DIRECT_LOCATION_GATE = PASS
R1C3B_DIFFERENT_FRAME_ISOLATION_GATE = PASS
R1C3B_SAME_FRAME_MULTI_ENTRY_MODEL_GATE = PASS
R1C3B_REHOST_ABORT_GATE = PASS
R1C3B_ANCHOR_QUIT_ABORT_GATE = PASS
R1C3B_HWND_REUSE_GATE = PASS
R1C3B_CONSENT_BOUND_FAST_PATH = PASS
R1C3B_ACTIVE_GLOBAL_INVENTORY_TARGET = ZERO
R1C3B_SAFETY_GATE = PASS
R1C3B_PHASE2_IMPLEMENTATION_READY = YES
R1C3B_PHASE2_DEBUG_UAT = REQUIRED
R1C3B_SMOOTHNESS_RUNTIME_GATE = PENDING_UAT
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
R1C3A_REVALIDATION_REQUIRED = NO
CALLBACK_WORKLOAD_EXPANDED = NO
HIGH_FREQUENCY_POLLING = NO
WH_KEYBOARD_LL_USED = NO
RAW_INPUT_USED = NO
ZORDER_CHANGE = NO
COMPONENT_SIZE_CHANGE = NO
MIXED_DPI_SUPPORT = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
~~~
