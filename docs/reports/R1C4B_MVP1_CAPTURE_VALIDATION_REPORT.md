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

## 第三批自有 client 对照

SHA `add9dafb03241a04c2842ce8f3bb39270bffb167`、RunId
`83cf87cafc4b4928bbe5c1413fcb3846`。legacy-control 33 行真实 JSONL，
hash `8C5BDB29D0CD5D39744D5905662466005A2F2DDF841B8E7A36DD2549ABD5CD79`。
Raw DOWN/UP 和自有 client DOWN/UP 各一次，所有 native Move/cancel/capture/writer
计数为零、cleanup clean。仅 guest 输入投递对照 PASS，不是 NormalUp。
因此不能把第一批缺失 UP 归为本 guest 完全无法产生 legacy UP。

下一 capture-stop 在预检被正常 foreground policy 拒绝（旧 foreground 65806，
新自有 bootstrap 66192，SetForegroundWindow=false），未发送场景输入。
对测试专属 bootstrap 增加一次 exact new blank client 点击，在 strict guest
guard、具名 GUI/capture clear、button clear、actual root 校验后执行；300ms 有界
消息等待，继续以实际 foreground/桌面/输入事实验收，不调整 foreground policy。
产品 shield 不抢前台，研究合同不变。第三批 task 与 guest PID 6280 已清理，
部署及全部原始结果保留。capture 风险和 normal 组合 UP 仍未运行。

第四批 `a6eb42fefd4242c990d2226d6d203e59`，SHA
`8b4673b4d15fa9a1023348b09aaa574c98ed2e78`：legacy-control 再次真实通过，
随后 capture-stop 的测试预检通过，但 owned source 本身未取得 foreground。
该场景只生成 startup/resource/UI-ready/退出 7 行，无 DOWN、cancel、capture 或
placement。不是 capture 机制反例。按同一 exact guest 范围修正 owned 启动
激活：测试启动 click 明确独立于实际手势，Raw receiver 在激活之后启动，
不重置或补造手势计数。失败批次原始日志保持，继续新身份定向验证。

## 第五批新增风险观察

SHA `a614a6a39474271c442797057b6246b6fbf0c2a5`，RunId
`d1a7fbfe68d74ca2acb3deda5eee1f51`：

| 场景 | 原始证据与结果 |
| --- | --- |
| END 后 capture-held F11 | 71 行，真实 hotkey、按住时 self release/撤罩、再真实 cleanup Raw UP，PASS；hash `96DF3A0C8D35D64871FB7AB377841766632B3D1A0AD513EAFA28A5E83BDC82C5` |
| END 后早 UP | 62 行，真实 END/Raw UP、capture/API placement 0、abort cleanup，PASS；hash `EC13A20AEB2014A8B68B24214B655A00283C70391734D9B9004C140162B57833` |
| capture authority 撤回 | 66 行，真实拒绝 attempted=false、没有 SetCapture/API placement，PASS；hash `27B9C86976C4EEE7EF1C5C3A29BCD22D35D7331938D26235EC18F5BC971D4896` |
| capture-lost 原触发 | 64 行 FAIL；hash `EFDD88F286098BCD5A6A0FF4C9BF71305E3C3A04073B858D0FB9C4812F8FC46B` |

最后一项 source UI 的 control SetCapture(actual=459474) 未让 resource overlay
262894 收到 WM_CAPTURECHANGED；两个具名线程不能合成“全桌面唯一捕获”的证明。
driver 期望没有实际成立，不能制造 CaptureLost 或改成 PASS。最后 left_high=true，
只销毁专属 guest PID 1440 隔离该测试失败，不算输入 cleanup PASS；其他场景已
通过的 actual cleanup 不受此重分类覆盖。后续改为精确自有 shield 的一次 bounded
WM_CANCELMODE，让 DefWindowProc 真实释放并观察 capturechanged；不触碰 foreign
capture、不补 UP。source 原 cancel 仍只有一次。

同时复核本代已捕获期间的 Raw GUI 上下文：foreground/source capture 若非
本代自有 overlay，则由资源线程基于实际 Raw 快照撤销并退出，不等 writer。
这是事件驱动的上下文补齐，不新增 hook/轮询。writer-stall、normal 组合 UP、
Release/Explorer/三窗仍未运行，先完成必要风险与 NormalUp 再放行。

## 第六批：真实 capture-lost 与 writer 中断

SHA `30ced13591f781dde3738a820055025d9d89de1e`，RunId
`84bfcddcb591441c9afe71813c0f7e32`。capture-stop、早 UP、撤权、legacy-control
继续通过。修正的 capture-lost 有实际 WM_CANCELMODE 投递、实际
WM_CAPTURECHANGED/CaptureLost、owner capture=0、abort 撤罩及真实 cleanup UP，
PASS；原始 hash `D88DBFF9B955246119CE392DC83C9B5F4534DF4AA5CE86F0036DF96F2BC01D03`。

writer-stall 原始 hash `5AE2B72450AFAF4A38D53611746657C14E0FCCD303ACD4303250823AD9FF0A29`。
actual writer callback 在 held 时阻塞，资源收到 ContextLost，实际 release 和撤罩
先完成，随后 cleanup 采样 foreground 459142 而非 source 131378，禁止当时注入 UP。
这不证明 deadline。旧固定 31 秒 Sleep 与 driver 从提前 Gone 起的 4 秒等待不一致，
导致 writer 尚未醒来时 test process 退出 74；保留 FAIL，而不是产品清理通过。
最终 source foreground 恢复后真实 cleanup Raw UP 已收到、left=false；专属 guest
PID 20492 被销毁，结果不参与产品 verdict。

修正测试受控阻塞为等待独立 isolation_gone（最多31秒），不等待 writer 驱动撤罩；
ContextLost 仍不计 Deadline PASS。normal 放在长时 stall 之前，主 UP 证据不被
环境中断隐藏。同时资源 loop 显式检查绝对截止时间，防止非空消息队列令
MsgWait 永不返回 WAIT_TIMEOUT；不新增轮询或计时框架。

为有界故障触发，内部请求可选择更短的单次截止时间，严格只允许 1..30000ms，
默认与上限均为30秒。owned writer-stall 请求3000ms，仍运行同一真实资源到期路径；
不会把3秒测试计为默认30秒实测，更不是性能SLA。Explorer/default 不改变。

## 最终现场检查点：NormalUp 仍失败

RUNTIME `99f8fed4b98f9f2e18d421a85d14e99443572f94`，RunId
`1deb70437dda4bde90335ecb77e54016`。ZIP
`58F72C475AEB8D9D3ED47C9B43B84B85C5F263A6835AB7C846EC360EFA423FD6`，
manifest `77603D504C1E43A78C5074BE61F5EE44168D1A97F63CE26C08C9554E29CAD82C`，
Debug owned `E9212E077FDCE22573567714B3071B98B455468504E7DA08B7D1D00A6B0B9EC1`，
Debug Explorer `72FCCF4B988F5CCDC5FE3870A4A544F1654B476455208D980CC72A5EA508F553`。

| 实际 Debug guest 场景 | 结论 |
| --- | --- |
| 空白 client 真实 DOWN/UP | PASS，仅输入能力对照 |
| END 后 capture-held F11 | PASS，真实 hotkey、own release/撤罩及 cleanup UP |
| END 后早 UP | PASS，零 capture/placement |
| capture authority 撤回 | PASS，实际拒绝，SetCapture attempted=false |
| 真实自有 capture loss | PASS，实际 WM_CAPTURECHANGED、abort、不重抢 |
| 普通 Move 磁吸＋正常 UP | FAIL：exact 写入有，Raw UP 有，legacy UP 无 |
| 修复后3秒 writer-stall Deadline | NOT_RUN，normal gate 失败后未执行 |
| 默认30秒 deadline | 未成功实测；第六批 ContextLost 不是 Deadline |
| Release / Explorer / 三窗 / 真人 | NOT_RUN |

最终 normal 76 行连续合法 JSONL，hash
`21BB84397D145DA67F6A70EA929B9A2DA43C421EAFA916AD2A42546914A45161`。
source HWND262356 / TID8092；overlay131436 / owner TID8140。
真实 END 后建立 capture，foreground 前后为 source；source/foreground GUI
capture=0、resource GUI capture=overlay。仅为具名线程事实，不是全桌面证明。
54/55 一次 native placement / exact snapped receipt；59/60 唯一 INPUT
`MOVE|ABSOLUTE|VIRTUALDESK|LEFTUP`（49157）；61 真实匹配 Raw UP、left=false；
64 实际 WM_MOUSEMOVE、own capture 仍 overlay；66/67 legacy/正常撤罩等待失败。
71/73 Shutdown own release / capturechanged / 撤罩，76 clean=true。
这是 abort 清理，不是 NormalUp。UP 后零额外 placement、没有补发 UP。

UP-only 第一批和组合单次 UP 最终批均失败，而 client UP-only 对照成功。
现有证据证明非激活遮罩的本线程 capture 回读不足以满足本链路 legacy UP 合同，
不确定系统内部根因，也不证明必须抢前台。具体缺失能力：保持 source foreground
时，原手势真实 legacy UP 能被自有资源线程可靠观察。没有证据支持继续改 style、
堆重试或造 UP；受阻 Explorer/三窗停止，不开放宿主试用。

下一尚未执行候选：仅 guest 研究真实 END 后短暂 `AttachThreadInput` 队列关联，
再用同一自有 SetCapture。它共享 focus/key states、重置某些键状态，超出当前
仅 SetCapture 的许可；必须先确认新增影响，不能悄悄引入。是否解决仍 UNKNOWN。

最终 Debug/Release 构建、受影响离线各2/2 PASS；无 interactive CTest/Fix H 重跑。
标准 origin 已推送 RUNTIME，最终仅报告提交后再核验 HEAD。
远端仅创建 I:\PaneBindMVP1Runs\ 下七个专属目录：d76bc479…、dc7cb4e7…、
83cf87ca…、a6eb42fe…、d1a7fbfe…、84bfcddc…、1deb7043…。
部署和全部证据保留，各批专属 task/guest 已清理，最终 PID6408。
无新系统功能变更/重启，未修改或停止 CrossRec，未在宿主输入。guest 销毁不计
产品 cleanup PASS。uat/EXE/ZIP 未入 Git，Recovery Index 未改，无 PR/merge/tag/release。

MVP1_OPERABLE_THREE_WINDOW_CANDIDATE = NOT_DELIVERED
MVP1_NORMAL_UP = FAIL
EXPLORER / THREE_WINDOW / HUMAN_UAT = NOT_RUN
无新系统功能变更或重启；宿主不发送输入；guest 销毁不计产品 cleanup PASS。
