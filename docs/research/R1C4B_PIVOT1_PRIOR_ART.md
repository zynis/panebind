# R1-C4B Architecture Pivot 1 — prior art 与输入权威边界

研究日期：2026-09-26。PaneBind base：
`070b05a8f44c8b08503b3643f838a83703495987`。
范围仅为 `CancelNativeLoopAndTakeOverInput` 的自动 authority probe 研究。
本记录不接受已有 `ConcurrentNativeLoopCorrection`，也不把上游报告当作
PaneBind 实测。实际 verdict 由本轮 probe 原始日志、validator 和执行报告决定。

## 本轮修正的约束

用户本轮明确允许 Raw Input 研究及 owned probe；`WH_MOUSE_LL` 仅为研究候选，
本轮不得实现。旧 [Fix 3 alternatives](R1C4B_FIX3_ALTERNATIVES.md) 中 Raw Input
和全局鼠标 hook 当时被禁止的结论是历史范围结论，不应继续用作本轮禁止理由。
DLL/code injection、轮询、busy loop、未获 consent 的窗口控制仍然禁止。

## 实际检查的 mature prior art

| 项目 | exact pin / license | 本轮实际检查 | 可用教训及边界 |
| --- | --- | --- | --- |
| AltSnap，mature maintained | `5c86416ad21e4b72844a998a746bd3bb0bee5f5d`；GPL-3.0-or-later，reference only | 本地 `License.txt`、`hooks.c` GPL header；WorkerThread、LetWindowKickBack、MoveResizeWindowNow_、LowLevelMouseProc、HookMouse、NO_HOOK_LL 分支；`altsnap.c` hook lifetime 历史 diff | 自有移动输入管线和系统 native drag 同时纠正不是同一模型。处理 movement work 的 owner 与连续移动合并是成熟实践；其轮询、Sleep、synthetic lifecycle 和 GPL 控制流均不进入 PaneBind。 |
| PowerToys/FancyZones，mature production | `19c4d805321db86f3634e6968e14dbf25cbba14a`；MIT，reference only | 通过 immutable source URL 实际读取 `WindowMouseSnap.cpp`、`DraggingState.cpp` 和 root `LICENSE`；PR 48569、PR 49985、下面列出的历史 diff | 正常完成和 abort 分开，避免残留拖动状态影响前台应用。其 zone highlight / drag-end Snap 不证明 cancel 后 live takeover。 |

没有复制、翻译、改写或适配外部实现代码；本轮没有外部代码 attribution 新义务。
未来任何 MIT code reuse 仍需独立 license/provenance 决策。AltSnap GPL 永远只作
当前研究参考，不能将其结构机械翻译为 PaneBind 实现。

AltSnap 检查依据：
[pinned hooks.c](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/hooks.c)、
[license](https://github.com/RamonUnch/AltSnap/blob/5c86416ad21e4b72844a998a746bd3bb0bee5f5d/License.txt)。
本地 `git rev-parse HEAD` 得到该 exact pin；工作区干净。本轮读取的关键区段为
hooks.c 693–737、1404–1490、5430–5540、5650–5680、5778–5805。
`rg` 检查 hooks.c/altsnap.c 没有找到 `WM_CANCELMODE` 或 Raw Input receiver。
这只覆盖所查文件，不能声明整个项目没有任何相应能力。

FancyZones 检查依据：
[WindowMouseSnap.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/WindowMouseSnap.cpp)、
[DraggingState.cpp](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/src/modules/fancyzones/FancyZonesLib/DraggingState.cpp)、
[LICENSE](https://raw.githubusercontent.com/microsoft/PowerToys/19c4d805321db86f3634e6968e14dbf25cbba14a/LICENSE)。
同 pin `FancyZones.cpp`、`KeyState.h`、`KeyboardInput.cpp` 本轮 URL 返回 cache miss，
没有声称重新读取这些文件。已读 source/license/history 足够支持本记录的有限
教训，但不能借旧 inspection 宣称本轮验证了 FZ Raw Input implementation。

## issues / PR / commit history

AltSnap [issue 572](https://github.com/RamonUnch/AltSnap/issues/572) 是用户报告：
AltSnap 1.64 的 move/resize 会激活 FancyZones，1.63 没有同样现象。
它提示 synthetic lifecycle 与别的 window manager 可能相互作用，
不是 PaneBind 观察，也不是已验证的根因。
[PR 609](https://github.com/RamonUnch/AltSnap/pull/609) 及实际本地
[`7f4afe59076b70980f71af202f63609ca3ac5745`](https://github.com/RamonUnch/AltSnap/commit/7f4afe59076b70980f71af202f63609ca3ac5745)
diff 将 movement work 移到 worker 并合并连续 movement 消息。
另实际读取
[`df25d36c6369bb13aa02ec83974e625fc7922c35`](https://github.com/RamonUnch/AltSnap/commit/df25d36c6369bb13aa02ec83974e625fc7922c35)
的 sizing recipient-return 处理 diff，及
[`8a5c422928d37d34e63cb8b99d4a74e14cb955f0`](https://github.com/RamonUnch/AltSnap/commit/8a5c422928d37d34e63cb8b99d4a74e14cb955f0)
中停用时保留 hooks.dll 的 lifetime diff。这些历史不建立 WM_CANCELMODE authority。

FancyZones [PR 48569](https://github.com/microsoft/PowerToys/pull/48569) 和
[`dd26d86580168d2e368701f7b0c4d629dc9cd9ac`](https://github.com/microsoft/PowerToys/commit/dd26d86580168d2e368701f7b0c4d629dc9cd9ac)
的 actual rendered diff 补齐 destroy ingress/dispatch，并在 destroyed dragged
window 上 abort，清除 dragging state；不能将 destroyed HWND 当作成功 drop。
[PR 49985](https://github.com/microsoft/PowerToys/pull/49985) 记录自动测试发现的
Shift 被吞后状态未更新、首个 highlight 被 mode transition 清掉等问题；其 review
还要求 work-area invalidation 使用非提交 teardown。
[`d68980a81bb8de144bdec998a114e948bf68c563`](https://github.com/microsoft/PowerToys/commit/d68980a81bb8de144bdec998a114e948bf68c563)
实际读取的是 rendered metadata 和 test guidance hunks，不声称完整展开其所有
product diff。教训是先证明输入确实送达，再判断状态机；这些 upstream tests 没有
在 PaneBind 环境运行，也不为本轮 20/20 数量提供任何替代证据。

## 官方输入 contract 与 SendInput 证据缺口

Microsoft [Raw Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input)
描述以 `WM_INPUT` 向注册窗口交付设备输入；它可以后台交付。
[RegisterRawInputDevices](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices)
要求先注册，并规定每个 process 每个 device class 只有一个注册 target window。
[RAWINPUTDEVICE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice)
将 `RIDEV_INPUTSINK` 定义为后台接收并要求 exact `hwndTarget`。
本轮 mouse TLC 是 `0x01/0x02`；禁止 `RIDEV_NOLEGACY`、`RIDEV_CAPTUREMOUSE`。
[WM_INPUT](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input) 区分
foreground `RIM_INPUT` 与 background `RIM_INPUTSINK`；前者必须经 DefWindowProc
完成系统 cleanup。
[GetRawInputData](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputdata)
使用消息携带的真正 HRAWINPUT；自己构造的 WM_INPUT 或 RAWMOUSE 不构成证据。

[RAWMOUSE](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse)
给出 relative/absolute 标志和 left-button-up transition，且 raw movement 不受
Control Panel mouse speed 影响。因此从 raw delta 积分出的路径不能当作 Windows
screen cursor；本轮按真实 mouse WM_INPUT event 调用一次 GetCursorPos，冻结
anchor 后计算目标。按钮 release 必须有实际 `RI_MOUSE_LEFT_BUTTON_UP` witness。
这些是有限受托 probe 的输入记录规则，不证明每个 injected sample 一一对应 packet。

[SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
承诺向 mouse/keyboard input stream 串行插入 simulated events，受 UIPI 约束。
[MOUSEINPUT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput)
描述 simulated mouse event；`MOUSEEVENTF_MOVE_NOCOALESCE` 的承诺仅是 legacy
WM_MOUSEMOVE 不合并。这两份 contract 均未承诺 SendInput 必然产生 WM_INPUT、
mouse raw delta、device origin 或 raw left-button-up。
Microsoft 的 [2014-02-13 input queue explanation](https://devblogs.microsoft.com/oldnewthing/20140213-00/?p=1773/)
解释 SendInput 与硬件进入同一输入队列以及异步状态更新，但其 Raw Input Thread
是系统处理线程，不等同于向注册 application 交付 WM_INPUT 的承诺。

结论：`SENDINPUT_IMPLIES_WM_INPUT = NOT_ESTABLISHED_BY_PRIMARY_CONTRACT`。
必须先实际记录自动路径的 WM_INPUT continuation / up；若 registration 成功且
SendInput/native drag 成功，但 raw 不到达，应记录 `OWNED_RAW_INPUT_BACKGROUND =
BLOCKED`、自动输入链路缺口及 `RAW_INPUT_TAKEOVER_ARCHITECTURE = UNRESOLVED`。
这种结果不能虚报 takeover PASS，也不能仅凭 injected 输入缺失证明物理 Raw Input
架构不可行。没有授权新增 driver/virtual HID、computer-use、human drag 或 hook。

## cancel 与 capture 不可推断的部分

[WM_CANCELMODE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-cancelmode)
规定 DefWindowProc 取消 standard scrollbar/menu 处理并释放 capture；没有直接
保证 native move/resize loop 必定退出或后续 geometry 不再由系统写入。
[WM_CAPTURECHANGED](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-capturechanged)
通知 losing window；[WM_EXITSIZEMOVE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-exitsizemove)
在实际退出 modal loop 后发送。二者都要记录，且 SendMessage 返回值不能代替它们。
[GetCapture](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getcapture)
仅查询调用线程；controller 线程得到 NULL 不能证明 source 已释放 capture。
owned source owner-thread witness 与 exact target TID 的 GetGUIThreadInfo 应保留
明确来源。foreign Explorer 只能以有界 SendMessageTimeout 和 exact consent identity
进入下一层，在 owned cancellation/free takeover/Magnet gates 全部通过之前不执行。

## WH_MOUSE_LL 重新定性

[LowLevelMouseProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc)
明确说明 callback 在安装线程上下文运行，经发送消息调用，安装线程必须 pump；
`WH_MOUSE_LL` hook 本身不会注入其他 process。它可以处理 driver 或 injected mouse
input，具有 hook-chain 和 timeout 约束。超时可能静默移除；官方建议很多场景优先
Raw Input。AltSnap 使用 DLL 装载自己的 callback，不等于把该 callback 注入 foreign
process；应区分 hook type、callback module packaging 与 foreign execution context。

正确分类：global low-level mouse hook、non-injected、event-driven、shared desktop
resource。`WH_MOUSE_LL == DLL injection` 为错误陈述。
`WH_MOUSE_LL_FALLBACK = TECHNICALLY_POSSIBLE` 是文档可行性，不是本轮实现/接受。
实际 global mouse hook 仍 `NONE`，不得在 Raw Input 不到达时偷偷启用 fallback。

## 有限研究 gate

`PRIOR_ART_SOURCE_LICENSE_HISTORY = PASS`；`OFFICIAL_CONTRACT_REVIEW = PASS`。
该 gate 仅允许独立 test-owned probe 取得原始事实，不给 product READY authority。
cancel Move/Bottom Resize 成功后仍要 free takeover Debug/Release 各 20/20；通过后才
接既有纯 Magnet solver，然后才进入 test-created Explorer。任一实际 cancel FAIL
按用户要求在 cancel stage 拒绝；若输入 witness 缺失则明确证据缺口。此文不预填
任意 CANCEL / TAKEOVER / MAGNET / EXPLORER PASS。
`C_SNAP_ON_RELEASE = UNSELECTED_FALLBACK`，`R1C4B_HUMAN_UAT = NOT_READY`。
