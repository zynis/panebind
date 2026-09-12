# R1-C3B Phase 2 — 一次 Debug optimized profile UAT

UAT implementation SHA：d8bdc0ba3ec7ef625081eacf49113c8293e51268。
分支：codex/r1c3b-smoothness-profile。
自动 Gate 已完成；顺滑度 PENDING_UAT。后续 handoff commit 为 docs-only。

新命令明确选择 frame-authority optimized executable；原 Phase 1 runner 保留。
在 D:\repository\panebind 的普通 PowerShell 中运行一次：

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run-r1c3b-phase2-optimized-profile.ps1 `
  -BuildDirectory out/r1c3b-debug `
  -Configuration Debug `
  -GlueTimeoutSeconds 120 `
  -ObserveSeconds 300
~~~

动作不增加：按提示新建 Leader、新建 Follower；readiness FIT 后，先按住 Ctrl，
正常拖动 Leader 约一秒；松鼠标，再松 Ctrl；等待自动恢复与 Observer 自然结束。
不要关闭用户既有窗口，不要手动制造多 tab，不要运行 Release UAT。

请返回完整 summary 与一个评分：A 很顺；B 连续但轻微阶梯/滞后；C 明显一卡一卡；
D 基本 END 才跳；E 抖动或异常。期望 A/B，但不预设结果。

正向 Gate 要求 global_inventory_calls_active=0、无 invalidation、原 correctness/
feedback/restore Gate 全部通过。输出会自动打印 PHASE1 DEBUG vs PHASE2 DEBUG
与 largest steady-active stages。原始日志仍只在 ignored uat/r1c3b/，不要提交 Git。

若 correctness PASS、active inventory=0 但仍评 C，只保存新 profile，
R1C3B_SMOOTHNESS_RUNTIME_GATE=FAIL，交回 review；不要继续自动修改第二热点。
在真人结果前不宣称 smoothness fixed、AquaGlue-equivalent 或 production-ready。
