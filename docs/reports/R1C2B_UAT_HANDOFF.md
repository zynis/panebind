# PaneBind R1-C2B Debug 人工验证交接

状态：实现与自动 Gate 已通过；真实 Explorer Glue Move 尚未运行。

```text
R1C2B_IMPLEMENTATION_READY = YES
R1C2B_INTERACTIVE_UAT = REQUIRED
R1C2B_RUNTIME_GATE = PENDING_UAT
```

本次只执行 **Debug** 人工验证。不要先执行 Release。Codex 不得代替用户
确认授权、创建 Explorer 或拖动窗口。

## 运行前

- 从仓库根目录 `D:\repository\panebind` 运行命令。
- 当前分支应为 `codex/r1c2b-explorer-glue-session`。
- 保留所有既有 Explorer 和其他应用原状；不要把既有窗口拿来做测试。
- 测试用的两个 Explorer 必须是本次提示后分别新建的顶层窗口。
- 两个测试窗口应位于同一显示器、相同 DPI，并保持普通状态（不最大化、
  不最小化）。
- 程序不会 resize。请在各自确认前手动把测试窗口缩小到当前工作区能同时
  容纳两个窗口的程度。
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
   该 Leader 空目录；不要复用任何既有窗口。保持普通状态，必要时手动缩小
   并放在准备使用的显示器上，然后回到 Console 按一次 `ENTER`。
2. 只有 Console 随后打印 **Follower** 路径后，才亲自再新建第二个 Explorer
   顶层窗口，进入该 Follower 空目录；同样保持普通状态、相同显示器/DPI、
   合适尺寸，然后回到 Console 按一次 `ENTER`。不要把 Leader 当 Follower。
3. 查看 Console 中两个目标和临时 Glue 行为的摘要。确认无误后输入 `Y`，再按
   `ENTER`。这是独立的 Glue 授权；前两次 ENTER 不能代替它。
4. 程序完成零间隙测试布局并明确显示“现在只拖动 Leader”后，只用普通
   Explorer 标题栏拖动 **Leader** 一段明显距离，然后松开鼠标。只拖一次，
   不需要再按键确认。
5. 等待 Harness 完成、两个窗口恢复原位置，并继续等待外部 Observer 自然
   结束和 runner 严格校验。看到最终 `evidence gate: PASS` 后，再由你亲自关闭
   Leader 和 Follower 测试窗口。

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

失败时状态为：

```text
R1C2B_RUNTIME_GATE = BLOCKED
```

无论 PASS 或 BLOCKED，本轮都不启动 R1-C3。

## 当前未验证范围

本命令尚未执行，因此真实 Explorer 的签发/配对、临时布局、Leader 生命周期、
Follower 跟随与 suppression、最终 reconcile、恢复、事件数量和流畅度均为
`NOT TESTED`。Release UAT、多显示器/混合 DPI/跨显示器、目标销毁、hung
Explorer、真实 overflow/native failure/invalidation，以及 Ctrl/global input、
Snap、Glue Resize、persistent group 也未由本次 Debug handoff 验证。

```text
R0_OBSERVER_SEMANTICS_CHANGED = NO
R0_REVALIDATION_REQUIRED = NO
R1C2A_REVALIDATION_REQUIRED = NO
USER_PREEXISTING_WINDOWS_TOUCHED = NO
OTHER_THIRD_PARTY_CONTROL = NO
GLOBAL_INPUT_CONTROL = NO
R1C3 = NOT STARTED
```
