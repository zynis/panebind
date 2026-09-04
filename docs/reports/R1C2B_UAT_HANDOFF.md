# PaneBind R1-C2B Debug 人工验证交接

状态：Debug Attempt 1 在 Glue 授权前因 `UnsafeLayout` 安全阻断；Fix 1
增加显式 layout readiness preview 和正确的 blocked-evidence 分流。新的真实
Explorer Glue Move 尚未运行。

```text
R1C2B_DEBUG_UAT_ATTEMPT_1 = BLOCKED
R1C2B_UAT_FIX1 = PASS
R1C2B_IMPLEMENTATION_READY = YES
R1C2B_INTERACTIVE_UAT = REQUIRED
R1C2B_UAT_FIX1_AUTOMATED_TESTS = PASS
R1C2B_UAT_FIX1_FINAL_SHA_AND_PUSH = SEE_FINAL_GIT_HANDOFF
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
   Explorer 标题栏拖动 **Leader** 一段明显距离，然后松开鼠标。只拖一次，
   不需要再按键确认。
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
- Leader 恰好一个 START、至少一个 active LOCATION、恰好一个 END；
- 至少一个 Follower native apply，全部具有 exact operation receipt；
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
| `PASS` | `0` | 完整 runtime evidence 严格通过 |
| `SAFE_BLOCKED / KNOWN_BLOCKED` | `2` | 支持的 pre-native blocker 严格自洽，且 authority/hook/operation 均未到达 |
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

安全阻断或无效证据返回后状态均不是 Runtime PASS：

```text
R1C2B_RUNTIME_GATE = BLOCKED
```

无论 PASS 或 BLOCKED，本轮都不启动 R1-C3。

## 当前未验证范围

Attempt 1 已验证两个真实 Explorer 的签发/配对前缀和 `UnsafeLayout` 安全
阻断，但未进入 Glue runtime。Fix 1 命令尚未由真人执行，因此新 preview、
临时布局、Leader lifecycle、Follower 跟随与 suppression、最终 reconcile、
恢复、事件数量和流畅度仍为 `NOT TESTED`。Release UAT、多显示器/混合 DPI/
跨显示器、目标销毁、hung Explorer、真实 overflow/native failure/
invalidation，以及 Ctrl/global input、Snap、Glue Resize、persistent group 也未
由本次 Debug handoff 验证。

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
