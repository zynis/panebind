# MVP1：guest 无控制台命令对照（2026-10-12）

本轮新增可实际运行的 guest named-pipe 绑定、同意、状态及停止入口；三窗原产品入口、STA/COM
消息泵、Move/shield/writer 和原权限检查均保留。命令停止和断连 abort 已通过真实 guest 检查。
**完整三窗候选仍未交付**：真实前置操作及 B Resize 后，第二次 A 普通手势仍因前台/capture
失效中止，未到达第一次自由移动 placement。Ctrl C 与 Release 完整流程未放行。
没有追加隐藏窗口、抢前台、争抢 capture 或操作未知窗口。

## 源码、实现与验证边界

- 分支 `codex/r1c4b-live-magnet`；Starting HEAD `5246fa84f8a6026435118de876c1191ca2fcaa60`，起点 clean，标准 origin fetch 后 0/0。
- RUNTIME `770c58d29826236d007cf4c4bef53389ca6a140f`，提交 `feat: add guest-only console-free command transport`。
  已标准 push 到 origin，origin `ls-remote refs/heads/codex/r1c4b-live-magnet` 当时等于 RUNTIME。
  最终仅文档提交后的 FINAL/push/status 在最终交付中给出；不把文档 SHA 当成制品编译 SHA。
- `StaGuestCommandPipe`：单一 local pipe、exact driver PID/保留进程句柄、实际同 SID/session/完整性级别检查，
  255-byte 有界 ASCII 命令；沿用空行逐窗确认、Y、S、Q。自动来源固定 `automated_guest_driver`。
  身份与原 guest/RunId/路径/marker 保护均未放宽。普通 Medium/non-elevated 路径不变。
- 真实使用 `CREATE_NO_WINDOW`；产品管道分支不创建/附着/读取控制台，也不依赖标准控制台句柄。
  driver 通过 `ProcessStartInfo.CreateNoWindow=true/UseShellExecute=false` 启动；pipe 的等待继续泵 STA 消息
  并执行会话 owner work，非轮询。无关 guest 控制台是否存在不由此推断。
- 单一 peer 断开/退出优先撤权，产品明确 abort，不能记为 NormalUp。有限 IO 取消未确认时不假称清理成功。
  既有 Ctrl+Shift+F11 和输入资源有界退出保留。本次无手势断连不外推活动 capture 收尾。
- owned 原九项仅按原 RunId `050736d561fe4b3c941e9f194b3f51aa`、原源码
  `3ef009f5e72c5eebdbb64b3c3e0cd6c52c1b2d52` 及固定原始哈希复用；本包 owned `NOT_RUN`。
  shared core/operations 未改；prepare 对 CMake 的唯一 command-only 差异作精确重建比较，未忽略整个构建文件。

## 自动与构建检查

Debug/Release 产品及 IPC 私有测试目标构建通过，/MT x64。实际 IPC 测试两配置各 7 项通过：
命令、断连、有限超时、owner-work 停止、超长输入、非 ASCII、真实 client 退出优先于缓冲 Y；
仅私有 pipe/process/message-only window，不发送开发机现场输入。
测试 fixture 初次继承标准句柄的失败保留；只修 fixture 的 NUL stdio/句柄 allowlist，未宣称产品 stdio 全为 NUL。

四项既有 PS 检查实际通过：`check-mvp1-guest-command-entry.ps1`、`check-mvp1-bind-only-entry.ps1`、
`check-mvp1-driver-confirmation-state.ps1`、`check-mvp1-product-log-reader.ps1`。
受影响 offline CTest Debug/Release 各 4/4：

```powershell
ctest --test-dir out/r1c4b-mvp1-shield-guest-mt -C Debug -R '^(explorer-mvp-attribution|explorer-mvp-flow|windows-live-move-writer|windows-move-frame-continuity)$' --output-on-failure
ctest --test-dir out/r1c4b-mvp1-shield-guest-mt -C Release -R '^(explorer-mvp-attribution|explorer-mvp-flow|windows-live-move-writer|windows-move-frame-continuity)$' --output-on-failure
```

Release 首次缺两个测试 EXE，记为 NOT_RUN 而非逻辑 FAIL；补建对应目标后重验。未启动整套 interactive CTest、Fix H 或稳定性批次。

## 真实 guest 命令检查（先于手势）

| RunId / 模式 | 实际结果 | 关键原始 SHA-256 |
|---|---|---|
| `f66474891c144eac9a3775e454acd26f` / CommandStopValidation | exact 三窗自动确认、Y、S、Q；STOPPED；会话和 pipe 资源真实退出；零手势 | product `0A3134DBB7046BA44E603C4A24E9F084AF1D92901C00EE1A997D62AC622CED02`；driver `CFB4D388DD9284BD0B027432DA1BE16D72224E3BF1BA7C878B24C0BB97483C72` |
| `9a694e9361044e9a81867af97d668adf` / CommandDisconnectValidation | sole client dispose 并退出；实际 ERROR_BROKEN_PIPE=109；BLOCKED/guest_command_disconnected；八项资源及命令清理 true；零手势 | product `35972A0186F43535D02AC2368FC71335245127825B76D5F3A2BB712B83DACD8A`；driver `36E759B038B051450179DF3C0E233579E4D30A20CD8A658065545A1102FA0AC0` |

两批真实 product/driver GetConsoleWindow 均无关联控制台；exact peer 为各自 driver，实际 SID/session 相同、双方
High/12288/elevated=true。所有自动同意标签保留。断连批观察到的是 BROKEN_PIPE，不冒称仅 client-exit wait 的独立现场分支。

## post-Resize A：新对照未通过

RunId `10805ce45b244df5a18f24afb9fe5ba7`，RUNTIME 如上。product 155 行、driver 178 行严格合法且序列各自连续。
产品 PID7824 / owner TID7828，driver PID7472；实际无关联控制台，八次 pipe wait 正常，STA 泵继续处理消息。

真实顺序与证据：

1. A 普通 generation1：自由、XY 吸附、保持、脱离、再吸附五点均 exact；四次实际 placement，保持为 already_exact。
2. Ctrl A generation2：native leader 与 B/C follower 均实际平移 `[34,0]`；无普通 source writer/shield/cancel。
3. B 普通 generation3、C 普通 generation4：同样五点 exact，各四次实际 placement。
   三次普通手势均具实际 Raw/legacy UP、NormalUp、自有 capture 正常释放、解除关联、撤罩；
   各自可靠 UP 后 native claim=0，后续 cursor 检查新增 native receipt=0、实际几何不变。
4. B 原生 Resize generation5：visible 从 `[1183,140,2201,689]` 到 `[1183,140,2201,717]`，实际底边 +28。
   三窗实际 DWM 几何与产品新 status 相等，relation_count=3/topology_ready=true；不是直接摆出近似布局。
5. 第二次 A generation6：实际 DOWN/START → 隔离 ready → 一次 cancel API 成功 → 实际 END → Attach/capture ready
   → handoff 接受（product139）。随后第一 free 输入被退休，**本代 writer quantum/placement=0**；不能引用首次 A 的写入。
   capture-loss/context-loss 的实际 foreground/root 为394466、PID1140/TID1640、`ConsoleWindowClass`，非三窗和遮罩。
   自有 ReleaseCapture 尚未发起；detach QPC504234801–504239393 早于失效采样 QPC504442640，
   资源侧已先撤权。JSONL 行序不是跨 Raw/WinEvent/资源线程的共同原生时序。
6. 测试 driver 在失败后发送清理 UP；receiver 后续实际观察 raw_up，但未绑定 generation，不替代本代匹配 Raw/legacy UP，
   无 NormalUp。精确 PID 的事后只读查询 UNKNOWN，不把缺失结果解释成系统、runner 或产品归属已证实。
   Q 后产品 stop_cleanup 八项 true、STOPPED/session/command/resources_stopped=true；driver `ABORTED`/exit2。
   guest 销毁不算这次手势功能通过。Ctrl C、Release 功能流程均 `NOT_RUN`。

该结果表明：移除产品与命令 driver 控制台依赖**没有消除本场景失败**；不能认定系统根因，也不能声称 guest 没有其他 console。
无新的操作权限事实支持继续写入/抢前台/capture，停止后续现场。最小后续证据缺口是失效原生时刻的未知窗口进程身份，
而非更多隐藏尝试；若继续诊断，应先做撤权后的有界只读身份快照、与本批自建进程身份比对，不因此授予控制权限。

原始 product SHA-256 `D2D02B2E4234C781CBAD101D81AD008259DD532C0B391FDC5E06012C8392DFCE`；
driver `530DF324046AC0015DD8E990C0A783EF613FA82FB3189FD03D3B704A4C836EB6`；
summary `27F44ED75F860C82ABE677DD7B3BE6F26B9E2C1925F6B9676CB18DDDD2C03ACF`。
summary 中旧的 `explorer_started/controller_checked=false` 范围字段不能覆盖原始启动/High 事实；driver_complete=false 也未掩盖。

## 包、可执行检查入口与限制

完整对照包（不是可试用交付）：本地 `uat/r1c4b-mvp1-shield-sandbox/10805ce45b244df5a18f24afb9fe5ba7/`；
远端 `I:\PaneBindMVP1Runs\10805ce45b244df5a18f24afb9fe5ba7\`。
ZIP `6154D70F7E3BB189E599A1E4C9466D25450619404412ECABC5CC08FBC4864267`；
manifest `175B306A13C5D9938CDD9754C2FEA42046B1F6F96228189F2E7D757444CD34B5`；
.wsb `2AE3FF5A10721E7F5B8CAF22FFE5A5E9CBE110F686333F4B6C664BC5416D35C9`。
Debug Explorer `C01971BB9D338A53C4882F313F0326FEC2048AA0712BF648C8371BB1AD539095`；
Release Explorer `544625C332A9839E351915F43DF9B66DDA96D9E657E231AB80F6963E0595471C`。

已实测的检查模式可在本仓库为新 RunId 准备；须按新清单部署并使用其 I 盘 .wsb：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts\prepare-r1c4b-mvp1-shield-sandbox.ps1 -GuestMode CommandStopValidation
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File scripts\prepare-r1c4b-mvp1-shield-sandbox.ps1 -GuestMode CommandDisconnectValidation
```

这里 ExecutionPolicy 仅限新进程，不修改系统持久设置。包打印精确 RunId、ZIP/.wsb/manifest/hash，
I 盘 `.wsb` 的启动入口为 `C:\Windows\System32\WindowsSandbox.exe "I:\PaneBindMVP1Runs\<该新RunId>\owned-shield-I.wsb"`；
本轮实际通过已登录 zs 会话的专用 Interactive task 启动，不把 SSH Session0 当作 guest 交互桌面。
旧 RunId/output 已有证据，不重用。`.wsb` 的 guest-run 实际以 CreateNoWindow 启动 exact driver 与产品，
driver 依次创建/确认 A/B/C，发送 Y、S 查看状态；Stop 模式发送 Q，Disconnect 模式关闭 sole pipe 后退出。
**不是普通 PowerShell 控制台直接启动 driver，也不是任意 PID 可写 pipe。**
它们是自动命令/收尾检查入口，不提供真人三窗试用。BindOnly/手动入口未做本轮现场检查，不推荐作已验证交付；
正常 console 产品入口保留，既有 Ctrl+Shift+F11 保留但本轮未重跑真实热键场景。

High Sandbox：部分功能实际通过，post-Resize A FAIL；Medium、宿主、多屏/混合 DPI、BindOnly 真人和真人 UAT 均 NOT_RUN。
`MVP1_READY_FOR_INDEPENDENT_REVIEW = NO`，尚未形成完整三窗可操作候选。

## 远端保护与清理

本轮仅新增 I 盘专属 RunId：`613d9485d7d84fb38568db3a27048620`、`dce73c6568eb42bab6405b215b77c6ae`、
`f66474891c144eac9a3775e454acd26f`、`9a694e9361044e9a81867af97d668adf`、`10805ce45b244df5a18f24afb9fe5ba7`。
前两批仅部署，未发送输入；初始 host Active 约束不足以判断 guest 条件，后续已取消该新增误判，仍由 guest 原预检判定。
宿主 zs session2 Disconnected 未阻止实际 guest session1 的原活动桌面检查；未连接/解锁宿主。

新 Sandbox 0.8.107 由短命 WindowsSandbox launcher 转交 WindowsSandboxRemoteSession client；
f664 原 launcher/CIM-null 拒绝回执未改。后加精确路径、父 PID、出生时间、配置、模块 hash、安装包身份与保留句柄核验，
仅清理本代 client38660/专用任务。专用 helper 两个普通解析/摘要时间错误的旧脚本保留，不覆盖。
随后两批 client34700/24296 与专用任务均按真实 native 身份清理，前后输出 hash 一致。
所有原包/失败证据保留；input 只读、output 唯一可写映射，未映射其他盘或用户/CrossRec目录。
系统变更、宿主重启、账户/UAC/token/网络配置、CrossRec 和未知用户文件修改均 NO；现场输入仅 guest。
uat/EXE/ZIP 不入 Git，Recovery Index 不改，不创建 PR、merge、tag 或 release。
