# R1-C4A 自动实现与独立 Review 交接

后续 Fix 1 修正可重复 live readiness 和 accepted restore baseline；
当前交接见 [Fix 1 报告](R1C4A_FIX1_READINESS_REPORT.md)。下文保留初版的
18/18、41 fixtures 等历史结果，不代表 Fix 1 的测试数量。已发生的初版
真人尝试仅在 readiness 阶段阻断，未形成三窗口 runtime 验收。

日期：2026-09-13。范围：Dynamic-Leader Bounded Glue Group Move。
实现及自动验证完成后停止；**本报告不是三窗口人工验收 Seal**。

## Git 与边界

- Starting main / HEAD：`81e40facf52ffdb96f76e4d737740167d485f4a7`。
- 开始时 main clean，标准 `git fetch origin`、`git pull --ff-only origin main`
  成功，`origin/main...HEAD = 0/0`；origin 为已核验的 SSH-over-443。
- 工作分支：`codex/r1c4a-dynamic-group-move`，未在 main 开发。
- 最终提交 SHA、标准 push 与远端 SHA/差异检查记录在本轮最终交接消息；
  不在提交内伪造一个自引用 SHA。
- `uat/` 仍由根 `.gitignore` 忽略，未提交用户日志。
- PR / merge / tag / release：NO。R1-C4B/C4C 未开始。

## 实现与证据等级

| 项目 | 实现 / 自动证据 | 真人证据 |
| --- | --- | --- |
| 同一授权集 A→B→C 连续成为 Leader | IMPLEMENTED；Core 三次生命周期、稳定 group / 递增 gesture、旧 Leader 下一次可跟随测试 PASS | NOT TESTED |
| 新路径无永久角色 | private group seal、callback bindings、member records 均无角色；仅 Gesture / 历史审计记录有 source | NOT TESTED |
| fresh authorized-only component | 复用 WindowAdjacencyGraph / TranslationSession；L-shape、corner-only、断开成员、未经授权 D 测试 PASS | NOT TESTED |
| 两 follower 单 batch | 私有 HDWP executor；B valid/C invalid 时不调用 Begin/Defer/End；Begin、Defer #1/#2、End 失败 seam PASS | 仅测试自建隐藏窗口的真实 HDWP 探针 PASS；不是 Explorer UAT |
| 逐成员 postverify / feedback | 两种 ACK 顺序、独立 duplicate/missing、错误成员/批次、旧 gesture/watermark、一项/两项 mismatch 测试 PASS | NOT TESTED |
| Explorer 授权与 invalidation | 新桥复用原完整每-frame validator、原始 canonical anchor、fresh VDM；所有成员完成 preflight 后登记 pending；末端 native 身份及已交付生命周期检查 | 新三窗口 navigation/quit/rehost/HWND reuse 等真实故障 NOT TESTED |
| Ctrl 规则 | 复用 sample_ctrl，高位 START latch；回调只在成员 START 采样；plain/late Ctrl Core 负路径 PASS | 不重跑已封存 C3A/C3B UAT |
| readiness / restore | 三窗口纯平移 L-shape；尺寸、工作区缺口、边、monitor/DPI、溢出测试 PASS；三次后才 restore | NOT TESTED |
| 时序与 runner | QPC、原始 receipt、quantum、batch、逐成员观察几何和 summary；41 个 fixture PASS | 尚无 C4A 延迟或主观成绩 |

Core 使用初始快照 + total Leader delta，没有累计 follower 上次位置。
一个成功 END 清除角色/拓扑/pending，重新进入 GroupReady；成员 capability
和 event source 保留。严重失败使整个 group fail-closed，不声明 rollback。

现有 C3B pair coordinator/source/session/activation 实现没有重写；仅既有
ExplorerTestSession 私有 Impl 增加 role-neutral group 字段和隔离桥入口。
R0 observer、R1-A 数学、旧 Owned/Companion/Explorer 行为路径保持原样。

## 自动验证

环境：Windows 10.0.26200，VS 18 2026 x64 / MSVC 19.50.35729，SDK
10.0.26100.0，Windows PowerShell 5.1。cmake/ctest 使用 VS bundled CMake：

```text
D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

| 验证 | 结果 |
| --- | --- |
| Debug configure/build | PASS |
| Release configure/build | PASS |
| Debug CTest | 18/18 PASS |
| Release CTest | 18/18 PASS |
| Owned Debug / Release self-test | PASS / PASS，failures=0 |
| Companion Debug / Release self-test | PASS / PASS，failures=0 |
| C2B runner | PASS（26 fixtures） |
| C3A runner | PASS（61 fixtures） |
| C3B Phase 1 / Phase 2 / Phase 3 runners | PASS（47 / 41 / 28 fixtures） |
| C4A runner | PASS（41 fixtures，包括 JSONL 文件与非法行） |

复现命令（为便于阅读，省略上述 exe 绝对路径）：

```powershell
cmake -S . -B out/r1c4a-debug -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c4a-debug --config Debug --parallel
ctest --test-dir out/r1c4a-debug -C Debug --output-on-failure
cmake -S . -B out/r1c4a-release -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c4a-release --config Release --parallel
ctest --test-dir out/r1c4a-release -C Release --output-on-failure
.\out\r1c4a-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c4a-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-frame-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4a-evidence-runner.ps1
```

开发过程中修正了新代码的 enum 名称编译错误，以及新 PowerShell JSONL
读取器异常路径未 Dispose 的文件占用问题；修正后重跑通过。首次合成日志
测试在系统 Temp 下遗留 `panebind-c4a-jsonl-2ebbd15cfe96474ebfd7e953a71b7dfd`
目录，环境拒绝了清理命令；未绕过。之后成功测试生成的临时 fixture 正常
清理。该目录不在仓库中，不含用户 UAT，也不影响构建或提交。

## 独立 Review 重点与 NOT TESTED

1. 私有 seal / member capability / 原始 Shell anchor 的边界和全成员
   preflight；新路径没有公开任意 HWND admission 或 native operation。
2. START 使用 processing-time fresh geometry，而非虚构 event-time 几何。
   异步交付可能已破坏 adjacency，此时必须拒绝三成员 live gate。
3. WinEvent 没有 batch id 或几何。owner 匹配 member-specific exact pending、
   generation、receipt/native-time watermarks；同 tick 不 ACK。历史 pending
   与新 target 相同且未观察反馈时 fail-closed；不能猜归属。
4. END 后允许精确 missing-event reconciliation，不把 missing 当 observed。
   END-sourced final batch 单独标识；runner 要求每次 gesture 有真正
   LOCATION-sourced、native-start 早于 END callback 的 realtime batch。
5. 新三窗口全部真实行为、性能 p50/p95、主观 A–E 和 rigid-body feel 尚未测试。
   实际窗口尺寸可能只留少量垂直移动空间；不能为通过测试自动 resize。
6. 2/4/8 scaling、产品最大组、Glue Resize、z-order、mixed DPI、多显示器、
   其他第三方程序、长期 CPU/内存目标均未验证或不在本轮范围。

## 后续人工流程（本轮不执行）

先交由 ChatGPT 独立 review。只有 review PASS 后，Human Root 才能通过
`scripts/run-r1c4a-group-evidence.ps1 -IndependentReviewPassed` 启动 Debug
真人流程。无该显式参数不会启动 harness。用户分别新建 A/B/C Explorer 并
进入独立 nonce 目录，再明确同意组布局；同一 session 完成 A、B、C 三次
Ctrl+Move，不重新 provision/layout。最后 exact restore，不关闭 Explorer。

raw JSONL 与 implementation SHA / harness SHA256 metadata 位于 ignored
`uat/r1c4a/`。离线复核入口为同 runner 的 `-ValidateEvidencePath`。
本轮未运行该真人流程；不得将合成 fixture 或 owned probe 解释成真人结果。

## 自动阶段状态

```text
R1C4A_DYNAMIC_LEADER_MODEL = PASS
R1C4A_PERSISTENT_LEADER_IDENTITY = NONE
R1C4A_AUTHORIZED_MEMBER_SET = PASS
R1C4A_ROLE_NEUTRAL_EVENT_SOURCE = PASS
R1C4A_GESTURE_COMPONENT_DISCOVERY = PASS
R1C4A_GESTURE_1_A_LEADER_MODEL = PASS
R1C4A_GESTURE_2_B_LEADER_MODEL = PASS
R1C4A_GESTURE_3_C_LEADER_MODEL = PASS
R1C4A_GROUP_GENERATION_STABLE = PASS
R1C4A_GESTURE_GENERATION_MONOTONIC = PASS
R1C4A_BATCH_PLACEMENT_GATE = PASS
R1C4A_ALL_FOLLOWER_PREFLIGHT = PASS
R1C4A_BATCH_POSTVERIFY = PASS
R1C4A_MEMBER_SPECIFIC_FEEDBACK = PASS
R1C4A_CROSS_GESTURE_FEEDBACK_ISOLATION = PASS
R1C4A_LIVE_MEMBER_COUNT = 3
PRODUCT_MAX_COMPONENT_SIZE = UNDECIDED
CTRL_ACTIVATION_CHANGED = NO
ZORDER_CHANGE = NO
GLUE_RESIZE = NO
MIXED_DPI_SUPPORT = NO
OTHER_THIRD_PARTY_CONTROL = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
R1C3B_REGRESSION = PASS
R1C4A_IMPLEMENTATION_READY = YES
R1C4A_HUMAN_UAT = REQUIRED
R1C4A_RUNTIME_GATE = PENDING_UAT
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```

上述 PASS 为实现、自动模型/fixture 与代码检查的阶段结论；
`R1C4A_LIVE_MEMBER_COUNT = 3` 指目标 fixture，不是已观察的真人窗口数量。
