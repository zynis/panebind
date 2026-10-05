# MVP1：精确三窗 High Sandbox 例外与功能验证

日期：2026-10-06（Asia/Shanghai）。分支 `codex/r1c4b-live-magnet`。
BASE `2bc40fe33c12a9110e27ff9a1c930804ddcf3323`；起点 clean，标准 origin
fetch 后分支 0/0；origin/main `81e40facf52ffdb96f76e4d737740167d485f4a7`，不合并。

## 授权与实现

用户本轮明确批准的例外仅适用于专用可丢弃 Sandbox、当前 RunId、三个分别
确认新建的 Explorer root。严格 EXE/日志路径、双 32-byte marker、WDAG、非零
活动且解锁 session、Default desktop 的实际 UOI_IO 均需满足。保护仅防误运行，
不称防伪安全边界。候选的显式 flag 不能绕过原生条件。

scope 创建读取实际 controller token；每窗在 baseline exclusion、精确路径、
Shell identity、HWND/PID/TID 及新确认代际成立后才占用唯一槽，最多三项。
签发后固定真实 token/capability generation；组要求同 scope 和互异槽。
运行中资格/Move writer/Glue follower/Resize 后刷新复用原 native validator，
重新读取实际 SID/session/IL/elevation 和原始 security tuple。未知、System、
混合或失效身份拒绝；普通无 scope 路径仍要求 Medium/non-elevated。

真实字段 `medium=false/elevated=true` 保留，例外单列 applied、RunId、槽和
代际。同 SID/session 日志来自实际比较回执，不是常量或账户名推断。
未修改账户、token、UAC、宿主设置或共享 shield/UP/清理实现。
研究/许可见 [SOURCE_PROVENANCE](../research/SOURCE_PROVENANCE.md)。

Debug/Release 构建通过；`ctest --test-dir out/r1c4b-mvp1-shield-guest-mt
-C Debug|Release -R '^(windows-explorer-unit|explorer-mvp-flow)$'
--output-on-failure` 各 2/2 PASS；实际日志共享读取回归 PASS。脚本 AST、嵌入
C# 编译及实际 driver 权限断言正反例通过，未当作现场输入证据。

## 前置证据复用

owned 九项保留 RunId `050736d561fe4b3c941e9f194b3f51aa`、来源
`3ef009f5e72c5eebdbb64b3c3e0cd6c52c1b2d52`。prepare 实际验证原始九个
JSONL hash、身份/sequence/真实清理和固定 summary hash，并比较共享 core/
operations/构建输入未改，才标 `REUSED_PRIOR_EVIDENCE`。本包 owned `NOT_RUN`，
不伪造为新实现九项通过，也不把 owned 结果外推跨进程。

## 首批现场包

RUNTIME `196d66f312ab0904fb3d1f4c21c4c6a514f4d409`；RunId
`be76ea0a60424113a4e71ace443d75e5`，专用目录
`I:\PaneBindMVP1Runs\be76ea0a60424113a4e71ace443d75e5`。
ZIP `366154F98D798862093E69DC9A2641F676F0B9A97112A7008C4D5039B6C77E8A`；
manifest `81DAFB88ACEC9666C02CD9F0C6B16F428D58B7074A3329BA043FD4F59FA60DF8`；
.wsb `6D6342E6DE9D87809EF8D0F9FD0CF612E1DFFD7C04194085616D4E248BB27570`。
Debug Explorer `00459D357AE1618F19D54F0D77BA992DB927E5709D6CFFD1CF3F5CA7DA44748B`；
Release Explorer `944A59321E49F7B39EF075309C701CC40F6BBD3C21A37F01F0F0212F4786A78D`。
远端核验全部 12 文件/marker/映射 hash；仅 input RO 与 output RW，禁用网络、
设备与剪贴板转发。只创建本轮专用任务和 guest，未重启/启用新功能。

现场顺序：当前输入预检 → 三个 exact 新窗分别绑定 → A 普通 Move 实际闭环
→ A/B/C/连续手势/磁吸保持脱离重吸附/XY → Ctrl Glue → 原生 Resize 刷新
→ 停止；Debug 排障，Release 同一功能闭环。原始失败保留，每次修复用新
源码身份/RunId。不以退出码、构建或销毁 guest 代替功能/清理结果。
Raw/WinEvent 的 JSONL 行序不是共同原生时序。实际结果待以下记录。

首批停止在测试预检入口合同：runner 新后缀 `-preflight-explorer.jsonl` 不在现有
输入环境 EXE 的精确 guest suffix allowlist，退出78、无预检 JSONL、没有 Explorer
或其输入。旧失败和终局 summary 保留，不当成环境或 High 策略失败。最小修复
仅 runner 使用已允许的 `-preflight-release-normal.jsonl`；summary 仍明确
current_input_identity，不暗示本包 owned normal 已运行。不改共享实现，仍复用
原九项。精确本批 guest/启动任务清理后以新 SHA/RunId 继续。
