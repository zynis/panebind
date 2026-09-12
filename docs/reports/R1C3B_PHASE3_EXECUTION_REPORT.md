# R1-C3B Phase 3 — VDM lifetime reuse 执行报告

日期：2026-09-12。实现与自动 Gate 完成，真人 Phase 3 smoothness 仍 PENDING_UAT。

## A. Git infrastructure

[Transport repair](GIT_TRANSPORT_RESILIENCE_REPAIR.md) 已完成并通过标准 push /
ls-remote 核对 44e76a8e1eb290924c10e23c6581836f3bda2efb。复用上轮创建的专用
Ed25519 key，passphrase NONE，用户已手动注册；SSH 验证 Hi zynis。
SSH-over-443 network/authentication/Git ls-remote/fetch/push dry-run 均 PASS。
HTTPS fallback 本次 fetch/ls-remote 也 PASS，起始 refs 与 primary 一致。
ssh-agent persistence 仍需管理员操作，但 IdentityFile 直接认证已工作，不是 blocker。

~~~text
origin = ssh://git@ssh.github.com:443/zynis/panebind.git
github-https = https://github.com/zynis/panebind.git
STANDARD_GIT_TRANSPORT_ONLY = YES
REMOTE_GIT_RESILIENCE_POLICY = PASS
GIT_TRANSPORT_INFRASTRUCTURE_REPAIR = PASS
~~~

没有更改代理/TLS/持久 HTTP version，没有 API object/ref fallback、force push、
reset、clean、PR、merge、tag 或 release；SSH 文件与 key 均未进入 Git。

## B. Phase 3 scope and source audit

Starting HEAD：0623c7ded7c5bc9ecfec195e0a00bfa907140708。
Branch：codex/r1c3b-smoothness-profile。
Main baseline（origin/main）：d901094404f04772e1d641842df76d13c551a734。
Implementation/UAT SHA：201f3c25cee49f7b2ea7af67063cfaf7df2d5725。
最终 handoff commit 只改文档；其完整 HEAD 和 push 后 divergence 在最终交接消息中记录。

当前源码原路径确实为每次 validation 都 CoCreateInstance(INPROC_SERVER)、查询、
Release。新路径只在 Phase 3 私有 Glue session 启用 retained manager；普通 C2A、
legacy、C2B、C3A 和 Phase 1/2 默认 executable 继续原 ephemeral 路径。
新 executable 为 panebind-explorer-vdm-glue-harness.exe；schema 为
panebind.r1c3b3.explorer_glue_profile。旧 executable/schema 不被替换。

缓存的是 IVirtualDesktopManager interface instance，不是 desktop query result。
每个原本需要 desktop eligibility 的 validation，仍以当前已授权 HWND 调用
IsWindowOnCurrentVirtualDesktop，一次调用、局部 BOOL 从 FALSE 开始、要求 S_OK
且 true。False/create/query failure 仍 fail closed；不 recreate/retry active call。

一份 service 由 ExplorerGlueSession 私有 owner STA 持有，两侧只借用 read-only
service，不共享 token/anchor/frame authority。wrapper 非 copy/move；创建前确认
已有 STA/MainSTA，再持有一份平衡的 CoInitializeEx 引用。所有调用验证 owner/
apartment；close 在 Release 后才 CoUninitialize，且幂等。正常 restore 后显式
close；异常展开中 member destruction 先于 target sessions 的 CoUninitialize。
现有父对象对错误线程析构的保留式 fail-safe 未改变，不新增外线程 COM cleanup。

源码冻结比对确认 Shell observation/location、native identity/state、security/
geometry/monitor/DPI 区域不变；Core、输入/activation、WinEvent source、Shell
inventory/Browser callback 文件无本轮 diff。Native flags、postverify、feedback、
八消息 fairness、quantum/coalescing、frame authority 均未改变。
新增 Stage 为 virtual_desktop_manager_acquire / virtual_desktop_query，保留
virtual_desktop parent。创建/查询/释放按 phase 计数，validation 还记录实际调用
delta；不能仅凭 query 总数非零通过，必须每个成功 validation 都恰有一次查询。

## Phase 2 人工结果保留与数据质量

用户报告 C+：明显优于 C，但未达到 B，不改写成 smoothness PASS。
Prefix：20260910T161142993Z；runtime d8bdc0ba3ec7ef625081eacf49113c8293e51268。
本轮以原 Phase 2 runner 离线严格重放，exit 0，correctness/profile/frame gates PASS。
60 applies、60 feedback/suppression，duplicate/missing/recursive 0，final/restore
exact，active global inventory 0。原日志继续 ignored，未提交或改写。

Harness SHA256：A982CE289FC600C87FDAFF863D32540911A2A262D8B2792B82805FCBB8307F1F。
VDM operation-local exclusive share=47.44401097096647%，largest 52/60；native
7/60、location 1/60。该原 stage 包括 acquire/query/release，不能把 47.44% 全部
谎称为已单独测得的 CoCreate 时间。

| 指标 | Phase 1 Debug | Phase 2 Debug |
| --- | ---: | ---: |
| Active global inventory calls | 86 | 0 |
| Follower applies | 16 | 60 |
| Quantum p50 / p95 ms | 218.5243 / 428.0875 | 81.4957 / 145.457 |
| Apply interval p50 / p95 ms | 304.4571 / 532.0436 | 100.2221 / 205.6511 |
| Receipt->owner p50 ms | 161.0284 | 46.8883 |
| Owner->native p50 ms | 239.39925 | 69.0361 |
| Receipt->postverify p50 / p95 ms | 340.71445 / 545.8175 | 96.4764 / 144.0038 |
| Max queue depth | 51 | 23 |

数据质量核验将 invocation、operation-local exclusive 和 quantum 分开；派生
baseline 的 23 项指标及 VDM family share 已重新计算对上。Phase 2 active
create/query 各 378 是从成功 validation 数与冻结源码推断，并非独立子 stage
实测；runner 明确标注 inferred，Phase 3 则使用新计数。只提交不含原始窗口/
用户路径的[派生 numeric baseline](R1C3B_PHASE2_DEBUG_PROFILE_BASELINE.json)。

## 自动验证

| 测试 | 结果 |
| --- | --- |
| Debug build + CTest | PASS，14/14 |
| Release build + CTest | PASS，14/14 |
| Owned Debug / Release self-test | PASS / PASS，failures 0 |
| Companion Debug / Release self-test | PASS / PASS，failures 0 |
| C2B / C3A / Phase1 / Phase2 runner fixtures | 26 / 61 / 47 / 41，全部 PASS |
| Phase3 runner fixtures | 28 PASS，含端到端、compact/verbose、No Ctrl/late Ctrl、漏 query、recreate、release 次数/phase、result-cache 伪造 |
| 60-operation-equivalent fake COM sequence | 1 manager、360 fresh queries、active creates=0；profiling ON/OFF 均成立 |
| true -> false / query fail / S_FALSE | 拒绝下一次 modeled write；不复用 true、不 recreate/retry |
| create failure / uninitialized / MTA / foreign thread | fail closed，无错误 apartment 的 interface 调用 |
| normal close / double close / exception cleanup | 单次 Release 且发生于仍有效 STA，引用计数平衡 |
| Read-only native probe | 实际创建 VDM、查询 PaneBind 自有 hidden HWND、先 Release 后 apartment shutdown；无 Explorer UAT |
| Share accounting | 拆子 stage 前后 family exclusive 总量相同；不制造表面提速 |
| Diff hygiene / frozen regions / help | PASS |

开发中修正了 SDK CLSID 头文件依赖、desktop_query diagnostic 的变量作用域，
以及新 synthetic span 漏 schema_version；修正后完整回归通过，不掩盖中间失败。

环境：Windows 10.0.26200，VS 18 2026 x64，MSVC 19.50，SDK 10.0.26100.0，
Visual Studio bundled CMake 4.2.3-msvc3。cmake/ctest 实际使用目录：
D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin。

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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
~~~

## Handoff / 限制

默认 runner 只显示 compact summary；VerboseOperations 恢复逐 operation/quantum
明细。完整 raw JSONL 不裁剪，所有校验照常执行。汇总保留 correctness、各 phase
inventory/create/query/release、hot stages、三阶段比较、timing、feedback/final/restore。
VDM family share 汇总互不重叠的 parent/child exclusive 时间，避免因拆 stage
而虚假降低父 stage 百分比。

真实 Phase 3 Explorer smoothness、desktop switch 故障时序、长期资源与 profiling
扰动仍 NOT TESTED；active CoCreate zero 目前是实现/自动 Gate，真人 run 仍需确认。
不宣称 FPS、smoothness fixed 或 production-ready。只交付
[一次 Phase 3 Debug UAT](R1C3B_PHASE3_DEBUG_UAT_HANDOFF.md)。A/B 后停止优化；
若仍 B/C、C+ 或 C，仅保留新 profile 交回 review，不继续第三热点。

~~~text
R1C3B_PHASE2_HUMAN_RESULT = CORRECTNESS_PASS_SMOOTHNESS_C_PLUS
R1C3B_PHASE3_VIRTUAL_DESKTOP_ANALYSIS_GATE = PASS
R1C3B_PHASE3_COM_LIFETIME_GATE = PASS
R1C3B_PHASE3_QUERY_FRESHNESS_GATE = PASS
R1C3B_PHASE3_IMPLEMENTATION_READY = YES
VIRTUAL_DESKTOP_RESULT_CACHE = NO
VIRTUAL_DESKTOP_CHECK_FREQUENCY_CHANGED = NO
ACTIVE_VDM_COCREATE_TARGET = ZERO
ACTIVE_VIRTUAL_DESKTOP_QUERY = PRESERVED
R1C3B_PHASE3_DEBUG_UAT = REQUIRED
R1C3B_SMOOTHNESS_RUNTIME_GATE = PENDING_UAT
ZORDER_CHANGE = NO
COMPONENT_SIZE_CHANGE = NO
MIXED_DPI_SUPPORT = NO
R0_OBSERVER_SEMANTICS_CHANGED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_REVALIDATION_REQUIRED = NO
R1C3A_REVALIDATION_REQUIRED = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
~~~
