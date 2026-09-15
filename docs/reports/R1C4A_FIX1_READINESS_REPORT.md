# R1-C4A Fix 1：可重复 Live Readiness 与 Accepted Restore Baseline

历史报告：固定 L 形尺寸/摆放要求已由
[2026-09-15 架构修正](R1C4_ARCHITECTURE_CORRECTION_REPORT.md)取代。
fresh capture、原授权上下文验证和 accepted restore baseline 原则继续有效。

完成日期：2026-09-14（Asia/Shanghai，跨日完成）。实现与自动验证完成；待 ChatGPT 独立 re-review，未运行
修复后的真人 UAT。该结果不是三窗口 runtime Seal。

## 起点及保留证据

- Branch：`codex/r1c4a-dynamic-group-move`，未新建分支。
- BASE_SHA：`95fd949c15068f17c6289f1f66072724a5e0cab6`。
- 开始时 working tree clean，标准 `git fetch origin` 成功，upstream `0/0`。
- Runtime、tests、report 作为同一个完整 Fix 提交；RUNTIME_SHA、TEST_SHA、
  REPORT_SHA、HEAD、REMOTE_HEAD 在最终交接中记录，不在提交内部伪造自引用 SHA。

已检查 ignored evidence：
`20260913T152205215Z-4fd64b71a63c4a70980351385310c2ea`。
JSONL 13 条合法记录，sequence 连续、startup/shutdown 完整；三窗口均为
Explorer，visible size 均为 1839×1026，工作区 3072×1824。L-shape 需要
3678×2052，宽/高缺口 606/228，harness exit 2。没有 receipt、gesture 或 batch
记录；冻结初版代码在调用 setup 之前即返回。因此该尝试为：

```text
BLOCKED_BY_LAYOUT_READINESS
RUNTIME = NOT_STARTED
```

不是 `R1C4A_RUNTIME_FAIL`。新 runner 对该原文件离线分类成功，并以 exit 2
保留 blocked 语义；未将 blocked 当 PASS。

| 本地文件 | 长度 | 修复前后相同的 SHA256 |
| --- | --- | --- |
| `.jsonl` | 3508 bytes | ACDAD7F0D17086ECA165539E5D0879545E4010C552519F2094A1447A88F41880 |
| `.metadata.json` | 524 bytes | 75222EDF68CB66CCB957EC39315209676F7526E6C2A9956B81D17DBF446B415E |

两文件均未修改、删除或提交；`/uat/` 仍 ignored。

## 修复内容

1. `GroupReadinessFixture` 只管理 preview 与 accepted baseline 数据，不持有
   native placement 或 capability 签发入口。生产 capture 回调使用原有
   `ExplorerGroupBridge::capture`，完整复验现有三个授权 frame。
2. `preview_readiness()` owner-thread only，每次 fresh capture。记录尺寸、
   work area、required width/height、缺口、reason、attempt、capture QPC 和
   group/gesture/native/hook/pending 状态。有效 preview 不产生 native apply、
   HDWP、gesture、Leader 或 hook activation。
3. NOT FIT 时显示每个成员的尺寸，并按实际超出的 A+B、C、A+C、B 分别提示
   需要手工减少的宽/高；Enter 重检、Q 取消。只改用户提示，不改 layout policy。
   用户无需 re-provision、重建 group 或重新签发 token。
4. setup 再独立 fresh capture/recompute。若不再 FIT，零写入返回调整循环；
   若 FIT，冻结这一份 accepted snapshot，并赋给 `original_` 和 `current_`。
   binding snapshot 单独保留。最终恢复 accepted 的 visible/positioning 几何，
   保留用户手工调整后的尺寸，不恢复 bind 时的旧尺寸。
5. 导航、关闭、rehost、token/identity/security 失效等使完整 capture 失败，
   fixture 终止；monitor/DPI 和其他非几何上下文也不能被 baseline rebase
   放宽。失效后不再提供继续调整的机会。
6. runner 要求至少一个 preview、恰好一个 accepted，所有 preview 零控制
   副作用、setup-before 等于 accepted fresh baseline、最终 restore 等于该
   baseline。兼容旧 layout-only blocked evidence，但不接受旧 PASS 冒充 Fix 1。

setup recheck 也计为一个 preview attempt；例如 immediate fit 为普通 preview
#1 + setup fresh check #2，只有 #2 的 FIT snapshot 可被接受。`readiness_accepted`
和 setup-check 的状态/QPC 在 native 之前捕获；JSON 在 setup 返回后输出。
日志序列不伪装成 native 发生时间，runner 使用 QPC 验证先后。

## 不变范围与依据

本轮没有设计新的 native/window 行为。沿用已通过的 R1-C4A 平台研究，并
检查同仓库 C2B/C3B 已有的 live-preview / 人工调整工作流；不复制其 pair
authority，不改变 C3B runtime。没有新增外部项目或外部代码重用。

对 BASE_SHA 的精确文本比较确认以下函数体未改：
`group_layout_readiness`、`register_pending`、`apply_targets`、`attribute`、
`move_batch`、`quantum`、`run_gesture`、`restore`。

以下源文件与 BASE_SHA 无差异：Core `glue_group_move.h`、HDWP executor、
group event source、`explorer_session.cpp` 的完整验证/批量桥，以及旧 C3B
session/event source/activation。没有修改 dynamic Leader、movement math、
feedback attribution、Ctrl、z-order、Resize、mixed-DPI 或 component-size policy。
restore 函数未重写，仅其所用 `original_` 现在来自 accepted fresh baseline。

## 自动验证

环境沿用 Windows 10.0.26200、VS 18 2026 / MSVC 19.50、SDK 10.0.26100.0、
Windows PowerShell 5.1。使用 VS bundled CMake/bin 下的 cmake.exe / ctest.exe。

| 验证 | 结果 |
| --- | --- |
| Debug / Release build | PASS / PASS |
| Debug / Release CTest | 19/19 PASS / 19/19 PASS |
| Owned Debug / Release self-test | PASS / PASS，failures=0 |
| Companion Debug / Release self-test | PASS / PASS，failures=0 |
| C2B / C3A runners | PASS（26 / 61 fixtures） |
| C3B Phase 1 / 2 / 3 runners | PASS（47 / 41 / 28 fixtures） |
| C4A runner | 66 fixtures PASS（原 41 + Fix 1 新增 25） |
| 原失败日志离线回放 | BLOCKED_BY_LAYOUT_READINESS，exit 2 |

新 CTest `explorer-group-readiness` 覆盖 A–E：立即 FIT；初始 oversized 后人工
尺寸变化；preview FIT/setup fresh NOT FIT 并重回循环；capture 失效后终止；
重复 preview 的稳定 generation、无角色/pending/native/hook 副作用。另验证
owner-thread Gate 和 monitor/DPI/location/PID/VDM 非几何上下文不得 rebase。
使用与生产相同的 fixture capture 边界及 translation bridge，不操控 Explorer。

runner 新 fixture 覆盖 resized accepted baseline、setup TOCTOU retry、缺失/
重复 accepted、preview 伪造 native/gesture/hook 状态、旧 setup/restore baseline、
错误尺寸/缺口、legacy block、Q cancel 和用 readiness block 隐藏 native activity。
既有 runtime/feedback/timing 检查保留。

命令（cmake/ctest 对应 VS bundled exe）：

```powershell
cmake --build out/r1c4a-debug --config Debug --parallel
ctest --test-dir out/r1c4a-debug -C Debug --output-on-failure
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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/run-r1c4a-group-evidence.ps1 -ValidateEvidencePath uat/r1c4a/20260913T152205215Z-4fd64b71a63c4a70980351385310c2ea.jsonl
```

## 停止点

修复后的真实人工调整/预览体验、三窗口 Ctrl+Move、流畅度和真实异常恢复
仍为 NOT TESTED。accepted 后若窗口在原有最终 native preflight 期间又变化，
仍沿用原 fail-closed 路径，不扩大本 Fix 为 batch 重试或 rollback。

FIT 只代表原有布局能放入工作区，不保证有充足的拖动余量；提示会提醒用户
额外留空间，但没有改变尺寸或 placement policy。必须先独立 re-review；
本轮不启动真人 UAT、不要求重跑旧版本、不创建 PR/merge/tag/release。

```text
R1C4A_FIX1_REPEATABLE_READINESS = PASS
R1C4A_LIVE_READINESS_CAPTURE = PASS
R1C4A_PREVIEW_NATIVE_WRITES = ZERO
R1C4A_ACCEPTED_BASELINE_REBASE = PASS
R1C4A_PREVIEW_SETUP_TOCTOU_GATE = PASS
R1C4A_RESTORE_TO_ACCEPTED_BASELINE = PASS
R1C4A_DYNAMIC_LEADER_MODEL = PASS
R1C4A_BATCH_PLACEMENT_GATE = PASS
R1C4A_MEMBER_SPECIFIC_FEEDBACK = PASS
R1C3B_REGRESSION = PASS
R1C4A_IMPLEMENTATION_READY = YES
R1C4A_HUMAN_UAT = REQUIRED
R1C4A_RUNTIME_GATE = PENDING_UAT
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```
