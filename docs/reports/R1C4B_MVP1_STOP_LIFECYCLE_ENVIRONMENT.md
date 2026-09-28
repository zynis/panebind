# R1-C4B MVP1 — stop 生命周期修复与测试环境核查（2026-09-28）

## 现在能实际操作什么

开发机可以构建和运行**无输入**的定向顺序回归，并生成新的 owned Sandbox 测试包；普通宿主直接启动该 probe 会在创建窗口或发送输入之前以 `NOT_READY`/78 拒绝。当前**不能**运行已授权 guest 合成输入或操作三窗 Explorer MVP1：经只读核查，开发机没有已注册的 Windows VM，ZS-Workstation 的 Windows Sandbox 尚未启用。本轮没有在任何宿主或 guest 发送合成输入，也没有碰 CrossRec 任务。

## 起点与确定性缺陷

- 起点与原上游均为 `f2c9cf2d4aec5d946ff1bccf74b6156d2406f1bc`；分支 `codex/r1c4b-live-magnet`，工作树 clean，`git fetch origin` 成功；`origin/main=81e40facf52ffdb96f76e4d737740167d485f4a7`，起点相对 `origin/main` 为 `0 behind / 41 ahead`。
- 原 stop 路径等待 `overlay_gone` 再发送测试 LEFTUP；旧 owner 却先注销 Raw 并销毁 receiver，最后才发 `overlay_gone`。因此 receiver 无法接收之后的测试 UP，`raw_up` 等待和最终 `raw_ups == 1` 均不能在无额外输入的预期路径满足。这是代码顺序错误，不是 Sandbox 缺失造成。
- 修复提交 `328a6a1b0c5f240afb2b30a39c9e1f112dd87449`：遮罩确认销毁后立刻发 `overlay_gone`；同一 owner 线程继续以 `MsgWaitForMultipleObjectsEx` 泵送 receiver，等待**真实** `WM_INPUT UP` 或固定 6000 ms 截止，才注销 Raw、销毁 receiver 并发独立 `receiver_gone`。最终收尾等待有界（7000 ms），若该测试进程的接收器卡死则终止本轮测试进程；绝不为等待 UP 保留全屏遮罩。现有 `receiver_procedure` 是唯一设置 `raw_up` 的路径，原始 Raw 计数与关联断言未删除或补写。
- 这只修复测试专用 probe 的生命周期；`CursorMoveMagnetIntent` 尚未接入真实 writer，不能称作产品 magnet/cleanup PASS。[Microsoft `WM_INPUT`](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input)、[`RegisterRawInputDevices`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices) 与 [`MsgWaitForMultipleObjectsEx`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex) 是相关官方行为依据。

## 定向验证与新包

- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-r1c4b-owned-stop-order.ps1 -SourceRevision f2c9cf2d4aec5d946ff1bccf74b6156d2406f1bc`：退出 1，明确指出旧 owner 未先发 `overlay_gone`。同命令不带 `-SourceRevision`：PASS。它仅验证源码顺序，**不**证明 Windows 的实际消息投递。
- Debug、Release `panebind-owned-overlay-probe` 目标构建 PASS。修复后通过 `scripts/prepare-r1c4b-mvp1-sandbox.ps1` 完成独立 Release `/MT` 两个目标构建与导入检查；probe 的宿主拒绝路径实测 `NOT_READY`/78。没有为本轮启动整套依赖交互桌面的 CTest，也没有重跑 Fix H 100 项。
- 新包 RunId：`370eb9f51b0a4838b3d10da7829bcec2`；源码提交：`328a6a1b0c5f240afb2b30a39c9e1f112dd87449`。忽略目录 `uat/r1c4b-mvp1-sandbox/370eb9f51b0a4838b3d10da7829bcec2/`，probe SHA-256 `23E142785ACC36AA832BC73F4D88AED900D303463885475ED61FD90F210A4E35`。前一 RunId `110448dbd27c4f6dab4cc2678a457a07` 的原包和历史证据保留。
- 已另生成 `desktop-owned-mvp1.wsb`，其 HostFolder 指向经远端只读查询确认的 ZS-Workstation 当前用户 LocalApplicationData 下本轮专用 run 目录，而非开发机路径；映射仅 input（只读）与 output（可写），并关闭网络、剪贴板和非必要设备重定向。`desktop-deploy.zip` 含静态 Release 两个 EXE、guest runner、manifest、marker、output marker 与此远端配置；ZIP SHA-256 `70DEF9C34B677526F7342D060AD13C7F03870E7A34568BAE94E37DDFB8614384`。两者均在 ignored `uat/`，尚未 SCP 或在远端启动；ZIP/EXE 不入 Git。

## 只读环境事实与下一项决定

- 开发机：`CoreCountrySpecific`（Home China）、25H2、build 26200.9457；已安装 VirtualBox 7.2.20，但 `VBoxManage list vms` 和 `list runningvms` 均为空。没有已确认可丢弃 Windows guest。Home 不支持 Windows Sandbox，这是[官方版本限制](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/)，**不是 PaneBind 产品不支持 Home**。
- `crossrec-desktop` 使用既有 SSH 凭据、`BatchMode=yes` 与 `StrictHostKeyChecking=yes` 连接；目标 hostname 为 `ZS-Workstation`。其系统是 Windows Professional 25H2 build 26200.9168；`Containers-DisposableClientVM=Disabled`、`Microsoft-Hyper-V-All=Disabled`；未发现 Sandbox/VirtualBox/VMware/QEMU 命令或已有 VM。console Session 1 Active，但 SSH 连通和 Active console 都不等于已有可供本轮使用的隔离交互 guest，也不能说明 CrossRec 空闲。只读查询未修改 CrossRec 文件、环境、任务或进程。
- 选择的下一路线是 ZS-Workstation 上的 Windows Sandbox，而非在开发机尝试非官方 Home 启用方法或安装新 VM。需要用户协调 CrossRec 验证时段，并决定是否允许管理员启用该机的 `Containers-DisposableClientVM`（可能重启）。本轮不自行执行此系统变更，不上传包或抢占现有 console；得到决定并确认可用后，才用既有 SCP 传最小包、核对远端哈希、创建本轮独立目录、启动可丢弃 guest。guest 只能在其已登录交互桌面发输入；销毁 guest 不计产品 cleanup PASS。

```text
STOP_LIFECYCLE_CODE = IMPLEMENTED
STOP_LIFECYCLE_SOURCE_ORDER = OFFLINE_PASS (old SHA FAIL)
STOP_LIFECYCLE_WIN32_DELIVERY = NOT_TESTED
DEVELOPMENT_HOST_GUEST = NOT_AVAILABLE
ZS_WORKSTATION_SANDBOX = DISABLED
REMOTE_DEPLOYMENT = NOT_RUN
OWNED_GUEST_SCENARIOS = NOT_RUN
EXPLORER_MOVE_MAGNET = NOT_IMPLEMENTED
THREE_WINDOW_MVP1 = NOT_IMPLEMENTED
HUMAN_UAT = NOT_RUN
FULL_R1C4B_ACCEPTANCE = NOT_CLAIMED
```
