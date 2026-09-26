# R1-C4B Pivot 1 Fix D — Product authority 与测试输入隔离

Base `e4ecdeaf4311e4adc95fcd94ff967df4c0f1bc83`；仅独立 test-owned probe。
研究与 source/license/history 见 [Fix D research](../research/R1C4B_PIVOT1_FIXD_INPUT_ISOLATION.md)。
旧 A/B/C JSONL、metadata、validator、报告和 verdict 不修改、不追认。

## 两类 proof，不再用 root membership 代表产品权限

`ProductGestureAuthority` 检查 exact source identity/PID/TID、desktop/visible、
global foreground、native EXIT + matching WinEvent END、GUI query/capture/menu/
move-size/flags、held left/no Raw UP、输入/receiver/takeover health、无 foreign
capture transfer、冻结 DPI/monitor、terminal/actual P/V availability/exact、
零 native post-END DRAG 与未归属 geometry。其结构不包含 cursor root 字段。
writer 仍需要有效 cursor 来计算 target；这不意味着产品必须拥有该点的 hit root。

`TestSyntheticInputIsolation` 单独证明 SendInput 的当前/destination root
为 exact self-created source 或 shield，包含实际 source/shield 身份、nonce、
生命周期、几何、NOACTIVATE、foreground 和逐点 proof。guard 不可冒充 shield，
root 不可伪填 source。失败记录 `TestInputIsolationUnavailable`、NO INPUT、
NO SOURCE WRITE，而不是宣告 product authority 或 END witness 失败。

```text
real native EXIT + matched WinEvent END
  -> fresh ProductGestureAuthority
  -> test-only post-END shield / independent isolation proof
  -> first source handoff write (strictly after END and isolation ready)
  -> actual Raw movement / source writer / isolation maintenance
  -> actual Raw UP + final source acceptance
  -> exact shield teardown
```

旧 C 的 `WinEventBarrier=FAIL` 是 witness + fresh 全部前置条件的复合 gate，
不是 WinEvent END 匹配失败。新 classifier 只读复算两份旧 C：若产品字段全满足，
可独立记 Product PASS / TestIsolation BLOCKED；旧 overall BLOCKED 与 Resize
takeover NOT_RUN 保留。rect containment 不替代 hit-test ownership，不能继续
靠扩大旧 guard 尺寸过门。

## 独立 v4 probe 合同

新 opt-in：`--run-owned-input-isolation-test --gesture move|bottom-resize
--evidence-log NEW_FILE`，schema `r1c4b-takeover-owned/v4`。
startup 记录 `diagnostic_contract=separated_authority_v1`、
`authority_contract=product_gesture_v1`、
`input_isolation_contract=post_end_shield_v1`、`run_nonce`；其余保留真实
foreground/WinEvent/Raw 合同。v1/v2/v3 原语义保持，未将实验加入产品入口。

native SOURCE writes 和 SHIELD placement 记录为不同对象/操作，后者不计
source takeover writes，但也不隐藏 QPC、native return/error、实际 rect 与身份。
WinEvent callback 只 bounded record/notify，不 capture、input、pump 或写窗口。
same UI-thread install/unhook，exact PID/TID/source/armed START→END matcher
沿用；所有阶段均先记录 facts，再 fail closed。

## Bottom shield：仅 END 后、持续不盖 source

source UI owner 只在两个 END + Product PASS 后创建 exact self-owned
`WS_POPUP / WS_EX_NOACTIVATE` shield，以 `HWND_TOPMOST` 和
`SWP_NOACTIVATE | SWP_SHOWWINDOW` 一次 checked placement；不改 external
HWND z-order、不将 source 变成产品 topmost、不做 fullscreen overlay。
原 pre-native activation bootstrap 保持已接受的语义，临时 owned-source
visibility 必须在 native START 前恢复；本轮不添加 source Z-order 行为。

屏幕定位 rect 为半开区间。Bottom corridor 横向是 planned cursor x±50，
bottom 为 planned max y+50+1；top 为
`max(terminal_source_P.bottom,current_actual_source_P.bottom)`。
上方 margin 按 source 非交叠边界裁剪，不声称对称50px或覆盖 source client。
geometry checked arithmetic、实际 monitor/work-area 和 positive extent
不满足就阻断，不扩大覆盖范围。

setup 在任何 shield creation/placement 前还独立记录 test-data plan：实际
cursor 必须与 original DOWN + native sample2 计划一致（沿用测试 fence 的
±1px），全部当前/剩余点及 shield plan 必须落在既定 work area 内。
外来 movement 不可被加入计划以扩大 corridor；失败不 clamp、不建 shield、
不发输入、不写 source，也不改变根独立的 Product verdict。

每次 source strict exact postverify 后，owner 仅由该事件单次维护 shield
到当前 source rect 之外；不 polling、timer retry 或再次 source SWP。
验证 source identity/context、foreground 仍是 source、shield 实际 style/
PID/TID/nonce/rect、source/shield 不交叠，shield 未 activation/focus。
因此 source Resize 扩展的 client/frame 不能继续被 fixed topmost shield 覆盖。

setup 仅用 WindowFromPoint，有限检查当前 cursor +18个 remaining planned
points：在 current source P 内必须 root==source，否则 root==shield。
每次 synthetic post-END MOVE/LEFTUP 的 API 边界再 fresh 检查实际
destination/current source/shield identity/lifecycle；不是复用 setup 的
root snapshot。foreign/NULL root 只记录 numeric HWND/owned=false，不读
title/path/contents/UI tree/user data。失败 NO INPUT。

每个 source writer 也独立记录 `writer_input_isolation`，用该 owner quantum
已取得的 cursor 点 fresh capture/root proof，不再 GetCursorPos 或 polling；
`writer_begin` 绑定其 sequence。不能沿用历史 setup ready，不能把后来
foreign cursor 的 test 隔离失败合并进 Product authority。失败 NO SOURCE WRITE。

Move 全部当前/剩余点在 source 内时 shield 为 NOT_NEEDED，不机械创建。
shield 不设置 focus/capture、不请求 activation、不返回 hit-test 穿透。
样式/placement success 不能代替实际 foreground、root 与 lifecycle proof。

## Source intent、Raw 与 teardown 不变

original DOWN pointer + native ENTER P/V 冻结；terminal EXIT 只作稳定性基线。
Bottom target 只有 `original_bottom + full_cursor_dy`，left/top/right 冻结；
checked visible→positioning bridge、每 quantum 最多一次 source native write、
立即/full P/V strict exact。shield rect 不是 intent anchor，也不提供 Magnet/
Relation/geometry authority。不用 corrected-rect 累加、DWM grace/sleep/retry。

完整 Resize 必须有18个 remaining actual RIM_INPUTSINK movement、每个实际
owner quantum/full-original-anchor/exact、真实 Raw UP、最终 left high bit
false、source exact、无 pending movement/write。正常 shield teardown 只能
在 Raw UP + final source acceptance 后由创建线程 checked Destroy，记录
IsWindow=false；failure cleanup 不得冒充 acceptance。

新 v4 正常结束不注入 saved-cursor restore MOVE：saved point 可能是 foreign
root，不能因“恢复”而豁免所有 post-END synthetic input 的隔离条件。
记录 `cursor_restore_skipped` / `cursor_restored=false`，最终 cursor 留在
已验证的路径末点；仍要求真实 UP、button false、source exact 与资源清理。
旧 v1/v2/v3 cursor restore 合同保持不变。

失败先退休 acceptance；只有重新 fresh 证明 TestIsolation 和完整输入安全
条件且有 test-owned pending DOWN，才一次 test LEFTUP。否则 NO INPUT；
独立记录 attempted/sent/实际 cleanup Raw UP/final button/capture/cursor。
先保留 shield/receiver 完成 bounded cleanup，之后只销毁 exact own resources。

v4 Source work 投递还验证 exact PID/TID/nonce、未 stop/retired；退役先停止
acceptance。finish 投递失败不使用 WM_CLOSE fallback，不操作可能已回收的
numeric HWND。cleanup lifetime 与 acceptance scope 分离，但 Source 已退役
或 stop 时不再注入；原始 identity 事实仍保留，不靠改写事实伪造安全状态。

## 执行边界与未解决的产品风险

新 single runner `scripts/run-r1c4b-input-isolation.ps1` 默认仅 Debug
BottomResize×1；完整 PASS 才 Debug Move×1。两者 PASS metadata 才允许
`run-r1c4b-input-isolation-gates.ps1`：Debug5/Release5 smoke，全部 PASS 才
Debug20/Release20 formal。每 operation fresh source/receiver/hook/generation，
Resize fresh shield；Move 可以 NOT_NEEDED。任何失败 STOP，无重试直到成功。
原始证据保留 ignored `uat/r1c4b-input-isolation/`，不提交 Git。

统计 native EXIT→WinEvent END→isolation ready→handoff native start、handoff
duration、Raw receipt→owner quantum/native writer 的实际 QPC；p50/p95 用
线性插值，无样本为 NOT_RUN，无 SLA。WindowFromPoint 有限调用不意味着
latency SLA，也不让全局 SendInput 成为 race-free HWND-addressed transaction。

`POST_CANCEL_LEGACY_MOUSE_DELIVERY_TO_UNDERLYING_WINDOWS = NOT YET PRODUCT-TESTED`。
未来真实产品使用 physical input + Raw observation，不注入 post-END
MOVE/LEFTUP、不使用 test shield。native cancel 释放 capture 后物理 legacy
movement/up 对 underlying windows 的副作用必须另轮研究/测试，不能被 owned
shield positive 掩盖；当前 PRODUCT_INPUT_SHIELD=NONE。

R0/Core/产品 runtime 不变；PRODUCT_RAW_INPUT=NOT_IMPLEMENTED，产品 SendInput/
global mouse hook/DLL/polling 均 NONE。NO Pure Magnet / Explorer / Human UAT /
PR / merge / tag / release；owned formal 全 PASS 后亦 STOP，Explorer 至多
READY_FOR_NEXT_STAGE。实际结果由本轮 execution report 记录，不预填 PASS。

## 本轮实际停止点

[Execution report](../reports/R1C4B_PIVOT1_FIXD_EXECUTION_REPORT.md)：首个 Debug
BottomResize probe0，但独立 validator 因实际64位 QPC / Int32 Math.Max 重载
overflow 返回 INVALID_EVIDENCE。保留原日志/metadata；无补丁后追认、无重试，
Fix D Move与所有重复门 NOT_RUN。Product/隔离子检查可独立通过，完整 Resize
gate仍未验收；architecture UNRESOLVED，Explorer/Human UAT不启动。
