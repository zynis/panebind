# R1-C4A Fix 3 — Preserve host terminal modes

日期：2026-09-15。范围：VS Code Integrated Terminal 的 console mode 回归；
未执行真人复制/粘贴 probe，也未执行 Explorer UAT。

## 修复结论

生产 reader 的 `InputModeScope` 和全部 `SetConsoleMode` 调用已移除。
输入模式只读观察，不改变或恢复；不清除 Quick Edit、Processed Input、VT
Input，也不修改 VS Code / Windows Terminal 设置、clipboard 或快捷键。

Ctrl+C 的低层记录（0x03 或 Ctrl+C key）忽略为非命令，不 abort、不 echo ^C、
不改变已经输入的命令。Escape 继续取消，Q 由对应 prompt 解释。原有 processed
input 的系统行为保留；没有全局 ConsoleCtrl handler、hook、worker 或 process-group
控制技巧。选区 Ctrl+C 是否由 frontend 完成复制仍需真人确认。

STA 的 MsgWaitForMultipleObjectsEx / QS_ALLINPUT / MWMO_INPUTAVAILABLE /
PeekMessageW / TranslateMessage / DispatchMessage 段与 BASE 完全相同，仍使用
ReadConsoleInputExW NOWAIT，没有 cooked/blocking secondary read。

研究与直接平台链接见
[R1C4A_TERMINAL_MODE_PRESERVATION.md](../research/R1C4A_TERMINAL_MODE_PRESERVATION.md)。
用户报告与 Fix 2 主动修改 mode 的代码证据一致；没有把 frontend UI 行为推断
成已完成的自动验收。当前现场问题为 `BLOCKED_BY_TERMINAL_MODE_REGRESSION`，
不是 dynamic Leader、Glue Resize 或 batch runtime FAIL。

## 实测发现与范围

自动测试创建隐藏的、独占的私有 console 子进程，先检查 console client 只有
测试进程自己，然后设置三组 fixture mode。生产 reader 内部不设置 mode。
owner STA 的自建 message-only window 在 read 期间测量 mode，并将短测试字符
写入该测试 console；不打开 Explorer、不绑定 capability、不使用 clipboard。

| 私有 fixture | 输入模式数值 | 受保护 bits | 结果 |
| --- | --- | --- | --- |
| A | 711 | Quick Edit / Processed / VT 均开 | before/during/after 相等，正常输入与 Escape PASS |
| B | 135 | Processed 开，Quick Edit / VT 关 | before/during/after 相等，正常输入与 Escape PASS |
| C | 640 | VT 开，Processed / Quick Edit 关 | before/during/after 相等，正常输入与 Escape PASS |

初次 mode-A 实测返回 `YQ` 而不是 `Q`：启用 VT 时 Backspace 变成 DEL，旧
editor 忽略了该编码。查验 Microsoft VT input 表后补充 DEL Backspace 和 ESC
Escape 的最小映射，再测全部通过。没有关闭 VT，没有添加完整 CSI/terminal
editor；不支持的 ESC-prefixed editing 控制序列仍 fail closed。

私有子进程仅用于自动测试隔离，不是供真人复制路径的 manual probe。其启动
使用 CREATE_NEW_CONSOLE + SW_HIDE，不使用 CREATE_NEW_PROCESS_GROUP 或
GenerateConsoleCtrlEvent。父测试进程等待子进程时尚未初始化 COM；超时只能
终止自己创建且不含用户数据的测试子进程。没有终止用户的旧 harness。

## Evidence 更新

```text
console_wait_contract = sta_message_pump_v2
console_mode_contract = preserve_host_mode_v1
console_input_contract = readconsoleinputex_nowait_v1
```

每次 wait 记录 input_mode_before / input_mode_after / modes_observed /
mode_changed 以及原有 bounded counters。观察到 host 自己改变 mode 时只报告，
不擅自恢复，也不因此把命令当 abort；受控 PASS evidence 要求两次观测相等。
模式查询失败会报告失败，不能拿默认值当成功观测。

不读取或记录 clipboard 内容、字符、复制路径。既有 target provisioning 的
nonce-directory evidence 不变；它不表示实际 clipboard 内容。新的 per-wait
日志没有这些内容字段。runner 拒绝缺失/错误 mode contract、mode 变化或伪造
观测，旧 v1 不得升级成 Fix 3 PASS；历史 blocked evidence 兼容逻辑保留。

## 自动验证与冻结范围

环境：Windows 10.0.26200；VS 18 2026 / MSVC 19.50；SDK 10.0.26100.0；
Windows PowerShell 5.1；使用 VS bundled CMake/bin。

| 验证 | 结果 |
| --- | --- |
| Debug / Release configure + build | PASS / PASS |
| Debug / Release CTest | 20/20 PASS / 20/20 PASS |
| Owned Debug / Release self-test | PASS / PASS，failures=0 |
| Companion Debug / Release self-test | PASS / PASS，failures=0 |
| C2B / C3A / C3B Phase 1/2/3 runners | PASS（26 / 61 / 47 / 41 / 28 fixtures） |
| C4A runner | 89 fixtures PASS（原 78 按新契约更新 + 新 11） |
| 生产代码 audit | mode setter、blocking input、global Ctrl handler、polling 均 ZERO |

STA tests 保留 message-before-input、multiple messages、partial line + later
message、console-first、WM_QUIT、no busy polling、Unicode、容量和错误路径；
Escape abort 保留，Ctrl+C 改为 ignore 测试。新增真实私有 console 三-mode
测试及 VT DEL/ESC 映射，替代已经删除的 mode-mutation/restore scope 测试。

`src/core/`、`src/platform/windows/explorer/`、旧 C3B harness 对 BASE 无差异。
readiness baseline/context comparison、setup TOCTOU、restore、Ctrl+Move、HDWP、
feedback、dynamic Leader、active gesture pump、z-order、mixed DPI 均未改变。

复现命令（cmake/ctest 对应 VS bundled exe）：

```powershell
cmake -S . -B out/r1c4a-fix3-debug -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c4a-fix3-debug --config Debug --parallel
ctest --test-dir out/r1c4a-fix3-debug -C Debug --output-on-failure
cmake -S . -B out/r1c4a-fix3-release -G "Visual Studio 18 2026" -A x64
cmake --build out/r1c4a-fix3-release --config Release --parallel
ctest --test-dir out/r1c4a-fix3-release -C Release --output-on-failure
.\out\r1c4a-fix3-debug\src\platform\windows\Debug\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-fix3-release\src\platform\windows\Release\panebind-owned-window-harness.exe --self-test
.\out\r1c4a-fix3-debug\src\platform\windows\Debug\panebind-companion-harness.exe --self-test
.\out\r1c4a-fix3-release\src\platform\windows\Release\panebind-companion-harness.exe --self-test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c2b-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3a-evidence-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-frame-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c3b-vdm-profile-runner.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-r1c4a-evidence-runner.ps1
```

## 用户进程与本地证据保护

开始时旧 Fix 2 harness（PID 31784，out/r1c4a-debug）仍运行，
`20260914T102820366Z-da1e345243f94bc58629190491121b5a.jsonl` 被占用，无法取得
开始哈希。本轮未结束该进程，也未改写日志；后续检查已不见该进程，并出现其
metadata，不能把外部进程产生的变化归为本轮写入。

因此使用独立 `out/r1c4a-fix3-debug/release` 构建，runner 的默认 Debug 目录
同步更新，避免覆盖仍运行的旧 exe。开始可读取的六份历史文件哈希复核一致；
占用文件不宣称拥有 before/after 一致性证明。全部 `uat/` 保持 ignored，未
删除、改写或提交；旧会话不会被新提交热替换。

## 人工 Gate / Git / 停止点

独立 re-review 后，由最终 C4A UAT 的第一个 nonce prompt 承担人工验证：
等待 Enter 期间选择路径、Ctrl+C、切到 Explorer 粘贴、回控制台 Enter。
需在 VS Code Integrated Terminal、Windows Terminal、classic host 确认正常。
本轮没有替 Human Root 执行该流程，也不以私有 console 测试冒充 clipboard UI
结果。readiness 等待期间 A/B/C resize 的响应和完整三手势 UAT 仍待人工验证。

Branch：`codex/r1c4a-dynamic-group-move`。
BASE_SHA：`625d72436f41da21f06089d0fbf45a728319997e`。
开始 clean，标准 fetch PASS，upstream 0/0。最终 commit/HEAD/remote SHA 和
push/status 在最终交接记录；不在提交内部伪造自引用 SHA。

```text
R1C4A_FIX3_TERMINAL_COMPAT_ANALYSIS = PASS
R1C4A_OWNER_STA_MESSAGE_PUMP = PASS
R1C4A_OWNER_BLOCKING_READCONSOLE = ZERO
R1C4A_CONSOLE_WAIT_BUSY_POLLING = NO
R1C4A_CONSOLE_MODE_MUTATION = ZERO
R1C4A_QUICK_EDIT_MODE_CHANGED = NO
R1C4A_PROCESSED_INPUT_CHANGED = NO
R1C4A_VT_INPUT_CHANGED = NO
R1C4A_TERMINAL_COPY_PASTE_RUNTIME = PENDING_HUMAN
R1C4A_READINESS_MANUAL_RESIZE_ALLOWED = YES
R1C4A_READINESS_NATIVE_WRITES = ZERO
R1C4A_ACCEPTED_BASELINE_REBASE = PASS
R1C4A_DYNAMIC_LEADER_MODEL = PASS
R1C4A_BATCH_PLACEMENT_GATE = PASS
R1C4A_MEMBER_SPECIFIC_FEEDBACK = PASS
R1C3B_REGRESSION = PASS
R1C4A_IMPLEMENTATION_READY = YES
R1C4A_HUMAN_UAT = REQUIRED
R1C4A_RUNTIME_GATE = PENDING_UAT
PR = NO
MERGE = NO
TAG = NO
RELEASE = NO
```
