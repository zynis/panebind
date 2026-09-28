# R1-C4B MVP1 — Sandbox readiness continuation (2026-09-28)

## 现在能启动并操作什么

本轮已能在仓库内构建 Debug/Release 的 **测试专用 owned overlay probe**，并从干净提交一键生成专用 `.wsb` 包。该包将输入只读映射、证据输出单独可写映射；若由受支持的 Windows Sandbox 在已登录交互桌面启动，LogonCommand 会自动先检查输入环境，再顺序运行五个 owned 场景，遇到首个失败即停。**本机没有启动 guest，也没有运行其中的 GUI/合成输入。** 包不是三窗 Explorer 产品原型。

当前宿主的 `EditionID=CoreCountrySpecific`、`ProductName=Windows 10 Home China`（注册表兼容名称）、`DisplayVersion=25H2`、`CurrentBuild=26200`、`UBR=9457`。`wsb.exe`、`WindowsSandbox.exe` 与 `.wsb` 文件关联均不存在；更关键的是 Microsoft 的 Windows Sandbox 支持矩阵明确排除 Home。可选功能的精确状态和 BIOS 虚拟化状态在当前非管理员执行环境无法可靠读取，故不猜测。没有启用功能、重启、改 BIOS、安装虚拟化软件或触碰已有 VM/会话。

官方依据：[Windows Sandbox 支持版本](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/)、[`.wsb` 配置](https://learn.microsoft.com/en-us/windows/security/application-security/application-isolation/windows-sandbox/windows-sandbox-configure-using-wsb-file)、[`RegisterHotKey` 与 F12 保留规则](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey)。

## 固定起点、修正与包

- 起点 `baf6fa3495f68c2679f7aa198110d2e59229edb4`，分支 `codex/r1c4b-live-magnet`，工作树 clean；标准 `git fetch origin` 成功。
- 测试专用 probe 的 Ctrl+Shift+F12 改为 Ctrl+Shift+F11。只有 `RegisterHotKey` 成功且实际匹配的 `WM_HOTKEY` 在真实按下、原生 Move 仍活跃时到达，才记热键路径；记录 `UnregisterHotKey`、遮罩撤销、测试专用 LEFTUP 与 native END。测试键序部分注入仍 FAIL，并尝试单独 key-up 清理。这些均仅经过编译，未在 guest 现场确认。
- 原永久 `NOT_READY` 改为严格的测试 guest 入口：精确可执行文件/日志路径、32 位 run-id marker、`WDAGUtilityAccount`、非零交互 Session 和活动 Default desktop；不匹配时在任何 GUI/输入前返回 78。这是防止误在宿主运行的门，不是防伪安全证明。
- `writer-stall` 仍只是在模拟 UP **之后**使测试 owner 睡眠；没有真实磁吸 writer，不能据此声称“按键仍按下时 writer 卡住”的撤罩能力。
- 新增 `scripts/prepare-r1c4b-mvp1-sandbox.ps1` 与 `scripts/r1c4b-mvp1-sandbox-guest.ps1`。准备脚本只打包、不启动 Sandbox；`Release-MT-x64` 两个测试 EXE 的导入表仅含 USER32/GDI32/WTSAPI32/ADVAPI32/KERNEL32，避免额外 VC++ 运行库依赖。guest 脚本将启动/错误状态写在 output 映射，不依赖 `wsb exec` stdout。
- 生成包源提交：`7dd3662d9e740ab9aede9d64b388997f9ba204cb`；RunId `110448dbd27c4f6dab4cc2678a457a07`；本地忽略目录 `uat/r1c4b-mvp1-sandbox/110448dbd27c4f6dab4cc2678a457a07/`，配置文件为其中 `owned-mvp1.wsb`。输入只读、输出可写，关闭网络、剪贴板、vGPU、音视频输入与打印机重定向。probe SHA-256：`C52D5E61260E0C5E58735E50EE14E8D6AD3847DB31D0BD7AEE329AF9FA3A01E6`。`uat/` 与 EXE 均不入 Git。

## 本轮实际验证

- Debug/Release `panebind-owned-overlay-probe` 目标构建 PASS；独立 guest Release `/MT` 两个目标构建与系统 DLL 导入检查 PASS；两份 PowerShell 脚本 AST 检查 0 错。
- `ctest -L offline`：Debug 26/26、Release 26/26 PASS。尝试 Debug 全套 `ctest` 为 32/33；唯一失败是既有 interactive `windows-explorer-vdm-unit` 的真实 owned-frame desktop query，在当前执行桌面返回非 `S_OK`。这不是本轮 probe 的 guest 结果，也未被解释为产品反例。
- 在宿主上用完整 probe 参数检查拒绝路径：返回 `NOT_READY`/78，未生成日志、窗口或输入。测试 guest：**NOT_RUN**。Raw Input、native Move、遮罩命中/不激活、正常/异常 UP、F11 实际触发、真正 writer、临时 Explorer 与三窗集成：**NOT_TESTED**。

## 下一个具体外部条件与停止边界

需要一台用户可提供的受支持 Windows Pro/Enterprise/Education、具备 Windows Sandbox 的测试主机或等效可丢弃交互 guest。当前 Home 版不能通过在此环境运行其他 `wsb` 命令修复；不要擅自升级、启用功能或重启。取得该环境后，先运行这个 owned 包并审查原始日志；机制通过才接入共用 `CursorMoveMagnetIntent` 和真实 writer，再继续 exact 临时 Explorer 与同入口 A/B/C、Ctrl Glue、原生 Resize 验证。当前没有机制失败证据，亦没有产品功能 PASS。

```text
MVP1_GUEST_ENVIRONMENT = BLOCKED_BY_UNSUPPORTED_HOST_EDITION
MVP1_GUEST_PROBE = NOT_RUN
MVP1_OWNED_LIVE_MAGNET = NOT_IMPLEMENTED
MVP1_EXPLORER_MOVE_MAGNET = NOT_IMPLEMENTED
MVP1_CTRL_GLUE_SAME_ENTRY = NOT_RUN
MVP1_NATIVE_RESIZE_PRESERVED = NOT_REVALIDATED
MVP1_RELEASE_FUNCTIONAL_RUN = NOT_RUN
MVP1_TRYOUT_CANDIDATE = NOT_READY
PHYSICAL_INPUT_HUMAN_UAT = NOT_RUN
FULL_R1C4B_ACCEPTANCE = NOT_CLAIMED
```
