# MVP1：真实 END 后自有 capture 与抬键收尾

2026-10-03，branch `codex/r1c4b-live-magnet`，BASE
`3f55edf3f3cacdb6ccc6fdde6337ef0682b4a418`。

沿用 GestureInputShield、Move 会话、连续性和单一 writer。资源所属线程仅在
adapter 已发布本代真实 END、普通 Move、exact source、按下、遮罩实际就绪、
source/foreground/resource 三个具名线程 GUI 满足条件后调用一次 SetCapture。
CaptureReady 之后 owner 再核验实际几何与权限，才安装 writer。原始 DOWN
锚点不变，END 到 capture 的旧 Raw 样本不用于写入。previous HWND 不是 BOOL。

Raw UP 撤销 pending；NormalUp 必须实际 Raw UP＋legacy LEFTUP。所属线程仅
释放本代自有遮罩 capture；实际 release、WM_CAPTURECHANGED 和销毁独立记录。
意外 loss、停止或 30 秒 deadline 退休写入并独立撤罩，不争抢或操作 foreign
capture。保留 exact Sandbox/RunId/路径保护，不开放普通宿主。

owned 增加 END 后实际 Ctrl+Shift+F11、早 UP、adapter 撤权拒绝、test-owned
capture loss，沿用按住时 writer 阻塞。Explorer driver 按实际几何检查 free、
XY snap、hold、detach、re-snap、A→B→C→A、Ctrl follower delta、Resize 状态刷新。
每一步必须有实际回执和几何，测试 driver 输入不会进入产品。

## 检查点

Debug 产品/owned 已构建；受影响离线 `explorer-mvp-flow` 和
`windows-live-move-writer` 2/2 PASS，仅代表离线模型范围。
提交后新包绑定源码 SHA、RunId 和 binary SHA-256，实际现场结果运行后追加。
历史失败批次 `3068edd02e06461095a845d88b5f7977`、
`0da578c4f88f409ab98da79aa404b0f0` 保持原样。

当前 owned / Explorer / 三窗 / 真人现场结果：NOT_RUN。
无新系统功能变更或重启；宿主不发送输入；guest 销毁不计产品 cleanup PASS。
