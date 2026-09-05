# PaneBind R1-C2B Debug 人工验证交接

更新日期：2026-09-05。Debug Attempt 2 按旧 Gate 已 PASS，安全、最终几何、
生命周期与证据完整性均通过，但 31 条 Leader LOCATION 仅形成 1 次 Follower
apply，实时跟随尚未接受。Fix 2 增加 progressive processing quanta 和独立
实时跟随证据 Gate。Fix 2 自动验证已通过；新的真人 Debug UAT 仍待执行。

```text
R1C2B_DEBUG_UAT_ATTEMPT_1 = BLOCKED
R1C2B_UAT_FIX1 = PASS
R1C2B_DEBUG_UAT_ATTEMPT_2_LEGACY_GATE = PASS
R1C2B_DEBUG_UAT_ATTEMPT_2_REALTIME_FOLLOW = INSUFFICIENT_EVIDENCE
R1C2B_UAT_FIX2 = PASS
R1C2B_IMPLEMENTATION_READY = YES
R1C2B_INTERACTIVE_UAT = REQUIRED
R1C2B_UAT_FIX2_AUTOMATED_TESTS = PASS
R1C2B_UAT_FIX2_FINAL_SHA_AND_PUSH = SEE_FINAL_GIT_HANDOFF
R1C2B_RUNTIME_GATE = PENDING_UAT
```

本交接只执行 **Debug** 人工验证。不要先执行 Release。Codex 不得代替用户
确认授权、创建 Explorer、调整尺寸或拖动窗口。

## Attempt 1 已保留事实

忽略目录中的 evidence prefix `20260903T044532644Z` 必须原样保留。Harness
共有 17 条合法、连续记录；外部 Observer 共有 3,681 条合法、连续记录，启动/
停止完整、stderr 为空且无 overflow/drop/post failure。

Leader 与 Follower 的独立 provisioning/consent/candidate 均 PASS，Follower
baseline 排除了 Leader，且两目标 distinct、同 monitor/DPI。随后：

```text
pair_validation = BLOCKED / UnsafeLayout
Glue consent prompt = NOT REACHED
Glue authority/binding = NOT ISSUED
Glue event source = NOT ARMED
Glue operation/native apply = 0
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
```

两窗口可见尺寸均为 `1839 x 1074`，工作区为 `3072 x 1824`；横向需要
`3678`（超 `606`），纵向需要 `2148`（超 `324`）。Observer 捕获到的窗口
移动发生在 Follower confirmation/pair validation 之前，属于真人准备动作，
不能视为 Glue runtime。旧 runner 因无条件要求 PASS-only
`glue_consent_prompt` 等记录而误报 malformed；这就是 Fix 1 的 runner 根因。

## Attempt 2 已保留事实

Prefix `20260905T065805930Z` 的三份原始 evidence 必须原样保留。完整逐条
取证、指纹与字段限制见
[Attempt 2 取证报告](R1C2B_ATTEMPT2_FORENSICS.md)。

```text
ATTEMPT_2_LEGACY_R1C2B_EVIDENCE_GATE = PASS
ATTEMPT_2_SAFETY = PASS
ATTEMPT_2_FINAL_GEOMETRY = PASS
ATTEMPT_2_SESSION_LIFECYCLE = PASS
ATTEMPT_2_EVIDENCE_INTEGRITY = PASS
ATTEMPT_2_REALTIME_FOLLOW = INSUFFICIENT_EVIDENCE / NOT YET ACCEPTED
```

Harness 69 条、Observer 2,714 条均完整连续，stderr 为空。Leader
START/LOCATION/END 为 `1/31/1`；内部只解释了一个 Leader 几何，产生一个
move request 和 30 个 noop。Follower native apply=1，内部 active
LOCATION=0，suppressed/duplicate/missing/reconciled=`0/0/1/1`，最终精确
恢复。旧日志没有 drain 边界，精确 drain-cycle 数及每次事件数
`NOT_RECORDED / NOT_RECOVERABLE`，不能从 queue 高水位 31 补造。

旧 operation 在处理 END 前已有 trace，但无法证明其发生在原生 END 产生
之前；外部 Observer 确实在 END 后看到了 Follower target LOCATION。
这些事实保留旧 PASS，同时说明需要新增多步跟随 Gate。

Fix 2 将 raw receipt 元数据与每个 processing quantum 的 live geometry
sample 分开记录；同 quantum 可以 coalesce 到最新 Leader LOCATION，下一
quantum 则重新采样。owner 最多 pump 8 条消息并在目标 receipt 到达后让出
处理机会，私有 Glue 校验不再进入旧 Shell readiness wait。回调工作量不变，
也没有新增 polling。

## 运行前

- 从仓库根目录 `D:\repository\panebind` 运行命令。
- 当前分支应为 `codex/r1c2b-explorer-glue-session`。
- 保留所有既有 Explorer 和其他应用原状；不要把既有窗口拿来做测试。
- 测试用的两个 Explorer 必须是本次提示后分别新建的顶层窗口。
- 两个测试窗口应位于同一显示器、相同 DPI，并保持普通状态（不最大化、
  不最小化）。
- 程序不会 resize。目标签发后，Console 会用实时尺寸明确显示能否横向或
  纵向容纳以及还差多少；不要再靠猜测尺寸。
- 外部 Observer 总时限为 300 秒；请在这段时间内完成创建、导航、授权和
  拖动。Glue armed 后的拖动等待上限为 120 秒。

## 唯一 Debug 命令

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c2b-explorer-glue-evidence.ps1 `
  -BuildDirectory out/r1c2b-debug `
  -Configuration Debug `
  -GlueTimeoutSeconds 120 `
  -ObserveSeconds 300
```

请不要给命令重定向 stdin/stdout，也不要从会替你发送按键的包装器启动；
授权输入必须来自当前前台 Console。

## 最少人工动作

1. Console 打印 **Leader** 路径后，亲自新建一个 Explorer 顶层窗口，进入
   该 Leader 空目录；不要复用任何既有窗口。保持普通状态并放在准备使用的
   显示器上，然后回到 Console 按一次 `ENTER`。
2. 只有 Console 随后打印 **Follower** 路径后，才亲自再新建第二个 Explorer
   顶层窗口，进入该 Follower 空目录；同样保持普通状态、相同显示器/DPI、
   然后回到 Console 按一次 `ENTER`。不要把 Leader 当 Follower。
3. 查看 `Layout Readiness Preview`。它会列出工作区、两个窗口、横向/纵向
   required/available/excess 和 fit 状态。若显示
   `NEEDS_MANUAL_RESIZE`，由你亲自缩小 Leader 和/或 Follower；不要只移动
   位置。调整后回到 Console 直接按 `ENTER` 重新检查。最多三次 preview；
   PaneBind 不会 resize，也尚未签发 Glue authority 或 arm hook。
4. Preview 显示 `FIT` 后，查看两个目标和临时 Glue 行为摘要。确认无误后输入
   `Y`，再按 `ENTER`。这是独立的 Glue 授权；此前所有 ENTER 均不能代替它。
   正式 begin 会再次 live inspect 和 plan；若 preview 后目标、位置、状态、
   monitor 或 DPI 变化，必须安全阻断。
5. 程序完成零间隙测试布局并明确显示“现在只拖动 Leader”后，只用普通
   Explorer 标题栏，以正常速度连续拖动 **Leader** 约 1 秒，移动明显距离后
   松开鼠标。无需精确计时；不要瞬间甩动后立即松手。只拖一次，不需要再
   按键确认。
6. 等待 Harness 完成、两个窗口恢复原位置，并继续等待外部 Observer 自然
   结束和 runner 严格校验。看到最终 `evidence gate: PASS` 后，再由你亲自关闭
   Leader 和 Follower 测试窗口。

Manual resize 只允许发生在 Glue prompt、authority issuance 和 hook arm 之前，
属于 `TEST FIXTURE PREPARATION ONLY`，不是 Glue Resize 或产品行为。

## Drag 期间不要做的事

- 不按 `Alt`、`Ctrl` 或 `Shift`；
- 不 resize、最大化或最小化 Leader；
- 不拖到屏幕边缘触发 Windows Snap；
- 不移动 Follower；
- 不切换两个测试窗口的目录；
- 不把任一窗口移到另一显示器；
- 不操作第三个 Explorer 或其他应用；
- armed 后不再向 Console 输入按键；
- 不主动关闭 Harness、Observer 或两个测试 Explorer。

这些限制只定义本轮可验证的 Glue Move baseline；它们不是未来产品交互设计。

## PASS 应看到什么

Runner 只有同时满足以下内部和外部证据才会打印 PASS：

- 两个 nonce/baseline/Console consent/唯一新目标链各自完整且角色唯一；
- Follower baseline 明确排除已签发 Leader；
- Glue authority 前存在一至三条连续、数学自洽且 side-effect-free 的
  `pair_layout_preview`，最后一条为 `FIT`；
- 两个目标都是 distinct `explorer.exe` / `CabinetWClass` 能力，并通过同
  monitor/DPI Gate；
- 独立 Glue `Y + ENTER` generation 完整；
- setup、arm、run 三个步骤各唯一 PASS；
- raw Leader 恰好一个 START、至少三个 active LOCATION、恰好一个 END；
- 至少两个 distinct sampled Leader geometries；
- 至少两次 active Follower native apply、至少两个 distinct active Follower
  target geometries，全部具有 exact operation receipt 与 exact postverify；
- 至少一次 active Follower apply 在 END callback receipt 交付之前有明确
  序列证据，最终 Follower target exact；
- 每个 Follower operation 要么有精确、后登记的 self-feedback，要么通过
  exact native receipt + final snapshot 明确记为 missing/reconciled；
- 无 recursive Follower operation、unexpected feedback、queue overflow、
  notification/hook/Observer failure；
- event source 已停止且 lifecycle clean；
- frozen topology 仍是 exact Leader/Follower pair；
- Leader 和 Follower 都精确恢复；
- PaneBind 未关闭测试窗口、未触及既有窗口/其他应用、未使用全局输入；
- 外部 R0 Observer 的 JSONL sequence、hook startup/shutdown、Leader
  START/LOCATION/END coverage 和目标事件计数形成独立审计；它不要求与内部
  source 对 Follower LOCATION 做逐事件或存在性强制一致。

程序不要求“一次 native request 恰好一个 LOCATION”，也不要求 Follower
出现 START/END。缺失 feedback 只有在 exact operation receipt 和 final snapshot
同时成立时才允许 reconcile，且不会伪造成 event ACK。

新增 `REALTIME_FOLLOW_EVIDENCE_GATE` 只证明本次 UAT 的多步跟随，不是
FPS/latency 或最终产品流畅度 SLA，也没有 Follower apply / Leader receipt
比例要求。Runner 将分别打印：

```text
Leader raw LOCATION:
Leader processing quanta:
Distinct Leader geometry samples:
Follower native applies:
Distinct Follower targets:
Follower applies before END:
Follower LOCATION:
Suppressed:
Duplicate:
Missing:
Reconciled:
```

“before END”使用同一 source 的 receipt 序列、operation 的 pre/post-native
watermark 和 processing quantum 证明：source Leader sequence 早于 END，
post-native watermark 尚未包含 END，且该 operation quantum 不含 END。
这表示 END callback delivery 前的 owner 顺序，不推断未记录的原生 END
产生时间或真人松手时间。若 LOCATION 与 END 同 quantum，仍先处理最后
meaningful translation 再 reconcile，但该 apply 不计入 before-END 数。

每条 `pair_layout_preview` 至少记录：

```text
attempt / attempt_limit
leader_size / follower_size / work_area_size
horizontal_required / horizontal_available / horizontal_excess / horizontal_fits
vertical_required / vertical_available / vertical_excess / vertical_fits
same_monitor / same_dpi / orientation
glue_authority_bound = false
glue_authority_consumed = false
native_apply_attempted = false
event_source_armed = false
temporary_peer_exception_retained = false
result = FIT / NEEDS_MANUAL_RESIZE / INVALIDATED
```

成功 preview 只是 readiness aid。正式 begin 不信任旧 snapshot，必须再次
live revalidate 并使用同一个未放宽的水平优先、纵向 fallback、zero-gap、
pure-translation、no-resize planner，以关闭 TOCTOU。

## 原始证据

Runner 将原始文件保存在 ignored 本地目录：

```text
uat/r1c2b/<timestamp>-glue-harness.jsonl
uat/r1c2b/<timestamp>-glue-observer.stdout.jsonl
uat/r1c2b/<timestamp>-glue-observer.stderr.log
uat/r1c2b/leader-<nonce>/
uat/r1c2b/follower-<nonce>/
```

`uat/` 不进入 Git。公开报告不要粘贴完整 nonce 用户路径、无关窗口标题、无关
Explorer 元数据或 raw HWND。

## 如果失败或阻断

- 不自动重试；
- 不改用既有 Explorer；
- 不调整参数后悄悄再跑；
- 不扩大到第三个窗口、其他应用或 Release；
- 保留 `uat/r1c2b/` 的完整原始文件；
- 记录 Console 的最终错误，并把本次 `<timestamp>` 前缀交回分析。

Runner 将结果明确分为三类：

| 输出 | Runner exit | 含义 |
| --- | ---: | --- |
| `PASS` | `0` | 安全、最终几何、生命周期与新实时跟随 Gate 全部通过 |
| `SAFE_BLOCKED / KNOWN_BLOCKED` | `2` | 支持的 pre-native blocker；或安全完成并精确恢复，但 drag/realtime evidence 不足 |
| `INVALID_EVIDENCE` | `1` | JSONL/schema/sequence/lifecycle 错误或 evidence 自相矛盾 |

`SAFE_BLOCKED` 不是 malformed，也不是 PASS。若三次 preview 后仍不 fit，
预期为 `pair_validation / UnsafeLayout`，runner 明确说明未进入 Glue 授权、
未 arm event source、未执行窗口移动，并以 `2` 退出。Fix 1 对未到达 feedback
阶段的 summary 使用：

```text
feedback_suppression_evidence = not_reached
```

只有 pair 已阻断却出现 Glue authority/native binding/layout operation/active
Follower operation，或 summary 与 operation/sequence/schema/Observer lifecycle
冲突时，才是 `INVALID_EVIDENCE`。

安全完成但 raw Leader LOCATION 少于三条时，返回
`SAFE_BLOCKED: INSUFFICIENT_DRAG_EVIDENCE`；raw 数量足够但 distinct
sample、active apply、target 或 before-END 证据不足时，返回
`SAFE_BLOCKED: INSUFFICIENT_REALTIME_FOLLOW`。例如再次出现 `31 -> 1`，
必须安全恢复后以 `2` 退出，不能 PASS，也不把它误称为 malformed。

安全阻断或无效证据返回后状态均不是 Runtime PASS：

```text
R1C2B_RUNTIME_GATE = BLOCKED
```

无论 PASS 或 BLOCKED，本轮都不启动 R1-C3。

## 当前未验证范围

Attempt 2 已验证真实 preview、临时布局、Leader lifecycle、一次 exact
Follower apply、missing reconciliation 与精确恢复。Fix 2 的多 quantum
实时跟随、新 Gate 及实际流畅度仍须新的真人 Debug evidence，不能用自动
tests 代替。Release UAT 在本轮不执行；多显示器/混合 DPI/跨显示器、目标
销毁、hung Explorer、真实 overflow/native failure/invalidation，以及
Ctrl/global input、Snap、Glue Resize、persistent group 均未由本交接验证。

```text
R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
R1C2B_GLUE_COORDINATOR_CHANGED_BY_FIX1 = NO
R1C2B_EVENT_SOURCE_CHANGED_BY_FIX1 = NO
R1C2B_LAYOUT_PLANNER_SEMANTICS_CHANGED_BY_FIX1 = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
R1C3 = NOT STARTED
```
