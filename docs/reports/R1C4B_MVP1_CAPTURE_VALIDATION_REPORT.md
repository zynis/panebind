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

## 第一批 guest 实测（保留 FAIL）

RUNTIME `c7f5159f3b0d4cd858cc19f1c29aba3108fa4610`；RunId
`d76bc4790f894277867aee807db64c5c`。ZIP SHA-256
`3A5EA146634FA0E92BD45CF4B075B713F70A526E159CEACF7BF72E0BBF7657C5`；
WSB `82BECF19C51678044D64409349367F31F7C81A7EF62B0520C27EC4B3F35647A1`；
manifest `DF27DFAA0AB9EEA0D4615192EBD8B26F2B1458B145397F23FE60B72F9EBE91E5`。
Debug owned EXE `37143DF26814CF280FCE426AE34B9010A1134B4CAA9341774056A862BC71879D`。

ZS-WORKSTATION Sandbox Enabled，无重启/功能变更。guest WDAGUtilityAccount、
session 1、交互 desktop/input preflight READY。实际 normal 67 行 JSONL 连续合法，
SHA-256 `6674D6D144042B14D02A132EDA8AA5DF122B7E008D6DE66A84B5563CECCF9B10`。

source HWND `131702`，source TID `7328`；overlay `66184`、owner TID `7320`。
sequence 30 为真实 END；35 SetCapture attempted、previous=0、actual_after=overlay，
三个具名线程 GUI 可读，source foreground 前后不变。43/44 一次真实 exact 磁吸写入；
49 匹配 Raw UP、physical left=false；52/54/55/56 遮罩仍收到 WM_MOUSEMOVE，
capture 仍属于遮罩。没有实际 legacy LEFTUP，57/58 等待超时，NormalUp FAIL。
62 退出时自有 ReleaseCapture 成功、actual after=0、own WM_CAPTURECHANGED；
64 Shutdown（非 NormalUp）撤罩，67 shield_clean=true、writer_failures=0。
这是 abort 清理通过，绝非正常抬键收尾通过。

消息泵全范围取出/分派，注册 RIDEV_INPUTSINK 而非 NOLEGACY，UP 路径无 tag
过滤；未找到确定性的消息丢弃代码缺陷。现有证据不能确定 Windows/Sandbox
内部根因，也不证明必须抢前台才能解决。下一步仅用空白 owned client 真实
DOWN/UP 对照及新增 capture 风险场景定位，不以未通过 normal 放行 Explorer。

启动编排的账户回读从 NTAccount 规范化为 SID，首次 exact-task 检查因此拒绝；
读取专属 task XML、核对实际 SID 后用新控制脚本恢复。旧脚本、失败状态及
恢复记录保留，包/实现不覆盖。仅移除本批临时 task 与专属 guest PID 17360，
部署和全部证据保留且 hash 前后不变；guest 销毁不参与产品 verdict。

owned normal：FAIL；其他新增风险场景、Release、Explorer、三窗、真人：NOT_RUN。

## 第二批启动合同缺项（实现问题，已修）

SHA `48e4f6562078be54337543a5785a86f5d4fb2f77`，RunId
`dc7cb4e7e3294d328eaf1e2c1f142300`。测试输入预检的精确 suffix allowlist
仍只有旧场景，新增 legacy-control 被返回 78；没有启动 owned，也没有发送
现场输入。为五个已授权新增场景加入精确 suffix，保留 WDAG/交互 session/
RunId/目录/EXE/marker 所有保护，不提供 bypass。第二批专属 task、guest PID
5508 已撤销，包和拒绝记录保留。此失败不算产品或 Sandbox 机制反例。
无新系统功能变更或重启；宿主不发送输入；guest 销毁不计产品 cleanup PASS。
