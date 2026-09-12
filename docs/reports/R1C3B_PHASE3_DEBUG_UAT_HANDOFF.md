# R1-C3B Phase 3 — 一次 Debug VDM profile UAT

Runtime SHA：201f3c25cee49f7b2ea7af67063cfaf7df2d5725。
Branch：codex/r1c3b-smoothness-profile；后续 handoff commit 为 docs-only。
自动 Gate PASS，真实顺滑度 PENDING_UAT。

在 D:\repository\panebind 的普通 PowerShell 中运行一次：

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c3b-phase3-vdm-profile.ps1 `
  -BuildDirectory out/r1c3b-debug `
  -Configuration Debug `
  -GlueTimeoutSeconds 120 `
  -ObserveSeconds 300
~~~

默认 compact summary，不逐条刷 OP_PROFILE / OP_VALIDATION；需要明细时可选
-VerboseOperations，raw JSONL 无论该开关都完整保存到 ignored uat/r1c3b/。

动作完全不变：按提示新建 Leader / Follower，readiness FIT，先按住 Ctrl，
正常拖动约一秒，松鼠标，再松 Ctrl，等待 restore 与 Observer 自然结束。
不要操作用户既有窗口，不要制造多 tab 或 desktop switch 场景，不跑 Release UAT。

请返回完整 compact summary 与主观描述：A / B / C+ / C / D / E，或“介于 B/C”
等自由描述。不强迫离散分类。重点核对 manager_create_calls_active=0，每个
成功 validation 仍有 fresh query，且 correctness/feedback/final/restore PASS。

若 A/B，停止继续优化；若仍 B/C、C+、C，保存新 profile 交回 review，不在同一轮
继续修改 shell_location、shell_observation、native placement 或 message loop。
