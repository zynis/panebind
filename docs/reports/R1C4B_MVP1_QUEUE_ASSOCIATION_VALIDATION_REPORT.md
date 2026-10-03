# MVP1：真实 END 后输入队列关联验证

日期：2026-10-04（Asia/Shanghai）。分支 `codex/r1c4b-live-magnet`。
BASE `0e7a4a8240e2317381ef772674c7d31a47a9b81e`，起点 clean；标准 origin
fetch 成功，分支与 origin 同步。研究/许可见
[SOURCE_PROVENANCE](../research/SOURCE_PROVENANCE.md)。旧 capture 失败报告不改。

## 本轮候选边界

只有 strict Sandbox/RunId/路径保护下的候选 adapter 可请求：真实 END 后，把
本代资源线程关联至已验证 source HWND 的实际线程，保持至真实 UP 或 abort。
同非零 session、三个 desktop 的实际 UOI_IO、精确 HWND/PID/TID、foreground、
capture/menu/move-size 与已发布授权都要重验。原始 DOWN 锚点、一次 bounded
cancel、共用 Move 会话/连续性/单一 writer 不变；没有 foreground/focus 抢占。

native attach TRUE 才建立本代 pair；capture 后重新核验才允许写入。Raw UP
立即撤销未发出请求，已发出调用按真实后验记录，不恢复权限。NormalUp 必须
真实 Raw UP + legacy LEFTUP。清理为 own ReleaseCapture → exact-pair detach
FALSE → 撤罩；未知或失败不进入 Idle，不允许下一手势/宣称 clean。
API 开始、完成与返回分开记录，尚未完成的结果为 null，输入线程不写磁盘。
JSONL 序号不是 Raw/WinEvent 共同原生时序。

owned 新验证复用产品 shield/session/continuity/writer。`normal-repeat` 在同
进程、同 source/resource 线程与 receiver 下执行两个 generation；先核实第一代
真实闭环、capture clear、detach 和按键 clear，再新建第二代单手势状态。
`source-pause` 仅让自有 source WndProc 有界停泵 5 s，与产品短时 3 s deadline
比较；只在 source 恢复之前实际完成必要清理才可判独立退出通过。
`capture-fail` 是 adapter 撤权，不是 native AttachThreadInput FALSE 实测。

## 第一批：正常闭环成立，后续 driver 轨迹错误

RUNTIME `20a5e49f4660087f30d345ee2d04913e8163959e`；RunId
`e9228871cd2042d78740d4e521c902b2`。guest 为 WDAGUtilityAccount、session 1。
原始 normal-repeat JSONL 151 行，合法连续，SHA-256
`E13EB745185C2677ACB81B8E783F70409FAFC2A1B09C011087B5466AA6FD5A61`。

第一代 generation `31233291672498`，source HWND `197198` / PID `7272` /
TID `7304`，资源 TID `7312`，overlay `66180`。seq32 真实 END；37/38 真实
attach 开始/TRUE 完成；39 SetCapture previous=0、actual=overlay，具名 source
GUI capture 也实际回读 overlay，foreground 保持 source；51/52 native placement
exact/snapped `[148,70,546,269]`；57 Raw UP、61/62 实际 overlay LEFTUP/legacy UP；
65 own release TRUE/actual capture=0；67 detach TRUE；68 NormalUp 撤罩。
72 正常 verdict 与 73 下一代前具名 capture/key readback 均满足。此为 owned
自动事实，不外推 Explorer 或真人操作。

第二代又关联/capture，但固定轨迹仍指向已 exact 的吸附位置，122/129 回执
`already_exact`、native_attempted=false。driver 仍要求真实新增 placement，
所以整批 FAIL。没有把无调用改成调用；最终真实两 UP、own release/detach/
NormalUp/receiver shutdown clean，但这不消除轨迹失败。

修复 `a990332d341f9269b9fd77b5a1639c1f23bb82b5` 仅修改后续测试轨迹：从该代
原始 DOWN/window 锚点先真实自由移动 `(-48,+32)`，检查实际 native 新调用与
exact 几何，再重新磁吸。产品机制不改，原失败包与日志保留，新 SHA/RunId 重验。

## 新批次与构建

新 RunId `ca3ff0d5a20a4831b9e2debe9ff03b27`，包 RUNTIME 为上述 `a990332...`。
ZIP SHA-256 `C4298986765C0833F84558C417789662FB46D0B1F76B46C9F2B50BC46E3FD394`；
manifest `E937D69C92BC7A53ED127963315835DBD661BB22FD5D15ECFE77E7C1B5359D59`；
I 盘 .wsb `5014542877F6CDE9905AB29D490390804397B03D2471D52E9C2CF197791EA109`。
Debug owned `FC16A1E76DB71CD1F614C428B1F9B055358EC127EF67F03842626C69366528E3`；
Debug Explorer `7B441DA75EC8F9D8308C977389F578C7B6C59768B3B1417E0D8D4BADA0DB7E72`；
Release owned `337E310CDF0E12F35F6332D3633489DDDF5D94911C0546869BB7A18A8C852BCE`；
Release Explorer `40E67CD757A89D68F4170E76DECC07D654EF92E6AAFE0D698E773F977468922E`。

VS18 2026 / x64 / SDK 10.0.26100 / CMake 4.2.3 / C++20 / PMV2 / static MT。
`scripts/prepare-r1c4b-mvp1-shield-sandbox.ps1` 完成 Debug/Release 构建、嵌入 SHA
和依赖/包哈希检查。`ctest --test-dir out/r1c4b-mvp1-shield-guest-mt -C
Debug|Release -R '^(explorer-mvp-flow|windows-live-move-writer)$'
--output-on-failure` 两配置各 2/2 PASS；新增谓词测试直接使用生产 same-generation
association cleanup 判断，不制造原生 API 事实。没有 interactive CTest 或历史批次。

## 桌面保护与资源记录

只创建两批 `I:\PaneBindMVP1Runs\<RunId>`，12 文件独立核验。仅 input 只读映射至
guest `C:\PaneBindMVP1\Input`，output 独立可写；网络/设备/剪贴板关闭。
第一批仅删除本轮启动任务、销毁本轮 guest PID `15704`；全部证据/包保留，
前后输出哈希一致。guest 销毁不计产品 cleanup PASS。没有系统变更、重启、
宿主合成输入、CrossRec/用户文件或配置更改。

## 第二批实际结果

ca3ff0d5... 的所有 owned JSONL 合法、sequence 连续、startup SHA 匹配，
结论根据原始事实审查，不只依据退出码。Debug normal-repeat 157 行，两代各有
真实 END/attach/capture/Raw+legacy UP/release/detach/NormalUp，第二代另有
真实 free placement `[100,102,498,301]` 后 resnap；两代共 3 次 native placement，
无 writer failure、对照窗口鼠标投递为零。SHA-256
`CCB63992CBE0E7BEB9878AEED0BFAB9CB4F442C5765737D9DD63C4F0533003AB`。

- 关联/capture 后按住时真实 F11：77 行，WM_HOTKEY 一次，release TRUE、capture=0、
  detach TRUE、ExplicitStop 撤罩、随后真实测试清理 Raw UP；hash
  `0956E4EDC7AB01A1B7F4F2847F62A281E998BC3D36CD7CEA8D179A8347D587C3`。
- writer dispatch callback 按住时受阻：87 行，3 s 请求的 Deadline、own release/
  detach/撤罩独立发生，等待撤罩的测试 callback 随后退出，native placement 0；hash
  `F7C4C1A31B87299C41AC9C5F023916E41C6365668D5E90ED0E2014AD4F6D7B86`。
  这不是已发出的 SetWindowPos 系统调用被强行取消。
- source 实际停泵：81 行，tick ready=33937、pause=33953、Gone=36937、
  resume=38953 ms。实际撤罩距 ready 3000 ms，比 source 恢复早 2016 ms，
  此前 release TRUE/capture=0 与 detach TRUE 已收到，仍按住；abort 而非 NormalUp。
  hash `3662EDDA79A9DA778DDE2B73BE14F59DCE55FB4108026753F0F2B00B05C31669`。
  只证明此 5 s 自有 source 暂停／短 3 s 场景，不是默认 30 s 或所有挂起状态证明。
- END 后早 UP：61 行，attach/capture/placement 零，真实 UP、abort cleanup；hash
  `0421223E7CF654EEE4A8CD46D0E9BB1741375E67BFD4225AACA60CE836FC9107`。
- adapter 撤权：66 行，native attach/capture/placement 零，拒绝且 clean；hash
  `14D5C383807B9BDBB683CA3EC21510ADAAA46E40A89D1FE7555B7CAB91890454`。
- 实际自有 capture 丢失：79 行，WM_CAPTURECHANGED、abort release 已丢失 capture、
  exact detach/撤罩、没有重新争抢、placement 零；hash
  `78AA3211B44595FE5A0466F4FE1EDC469F2AF7C6BED22D2CED485644305CCE8F`。
- Release normal：84 行，真实 exact snapped placement 与完整 NormalUp/解除；hash
  `84A101BE62EB88957423BC757B6156D29996E67037972C69139FFBE8254ACBBC`。

每场 receiver/Raw 注销、writer/UI 退出、hotkey 注销与 shield clean 均独立有实际回执。
实际 AttachThreadInput FALSE 失败没有制造；失败/未知解除只经过生产谓词离线测试。

owned Gate 通过后 runner 自行调用 Explorer driver，但启动前检查
`guest_input_desktop_not_active_at_driver_start` 拒绝；没有创建产品、临时 Explorer
或发送其输入。旧 driver 额外要求 guest 会话等于物理 console 会话，其分项未记录，
所以不把具体失败项猜测为已观察根因。将改用当前会话真实状态和实际 input desktop，
保留 strict guest 保护并记录分项，以新 SHA/RunId 继续。
本批仅移除本轮任务/销毁精确 guest PID `11196`，所有输出哈希保持不变；包/证据保留。

后续现场结果与终局 Git 状态待实际运行后补记。普通宿主试用及真人 UAT
未授权/未执行，不能以构建成功或 owned 局部闭环宣布三窗候选可试用。

## 第三批：owned 采样分流的新拒绝

RUNTIME `10d89530457a8ffabd7487a53179174d44a2aecd`，RunId
`a976d2a60b054d4bb1e092bc22bfc0bf`。仅 desktop driver 与报告变更后的包。
normal-repeat 150 行，hash
`B72188DC16E09E31F1162255319CC0457E92961EAF69AC0E33B5AD7F12015E62`。
首代正常闭环；第二代真实 free placement exact 后，129
`cursor_sample_invalid` 拒绝，未执行 resnap。最终实际两 UP、自有 release、
detach/NormalUp 与 receiver shutdown clean；整体场景 FAIL，未放行 Explorer。
旧日志没有具体采样 facts，不能猜测为 DWM、外部位移或关联 API 根因。
精确 guest PID `18912` 与本轮任务已清理，包/输出保留且哈希一致。

代码检查另发现 owned 确定性分流缺陷：共享 model 的 duplicate cursor 可返回
false 且仍 active，入口反而将其 retire；同样，取走 Raw batch 后旧 notice 未
消费。修正只在 owned actual adapter 区分真实 facts 拒绝、model 退休、无新计划
和有效计划；正常 UP 竞态不被重新 escape 成异常。queue notice 在原 producer
短锁内消费；测试 driver 单次移动后按真实 packet watermark/cursor 等新事件，
有界 2 次通知／总 2 s，不重发 input、不轮询。必要拒绝 facts/actual geometry/
version/model 分流记录用于解释新证据，不把只诊断的 last confirmed frame 当授权。
生产共用 3-BOOL dispatch 谓词与实际 Move/writer 组合定向测试覆盖 duplicate 后
仍可下一 changed placement、真实 facts/UP 仍拒绝；旧连续性门槛完全保留。

## 第四批：历史 Raw cursor 与处理时当前 cursor

RUNTIME `eb84aac0c1950add2a8a6f2fb6594cbd2f37fa1b`，RunId
`ace0ca9967c94927a5e08ea1d33c36fc`，154 行，hash
`C64215E17D59831D35CC4735254AF70A4067B02DA63D931520F7256C1F42A234`。
seq132 明确显示所有权限/命中/实际 frame continuity 都有效，版本3/Expected，
receiver 历史 cursor `[161,112]` 被正确当成 duplicate/NoNewPlan，不再退休。
driver 当前 GetCursorPos 已真实回读 `[204,84]`，等待其历史 Raw 样本 2 s 未得到
新计划，整体仍 FAIL。不能证明该 packet10 对应哪一次 SendInput，也不推定为
操作系统或 AttachThreadInput 的根因。首代及最后 cleanup 两UP/own release/
detach 仍真实闭环。精确 guest PID `11856` 与本轮任务清理，旧输出哈希不变。

最小修正现有 owner 边界：post-handoff Raw motion 事件仍是唯一驱动信号，
处理时用现有 GetCursorPos 一次 fresh 当前 cursor/QPC 做意图与权限判断；
receiver 历史坐标另记，不重发 input/轮询，也不把历史packet watermark当作
某一次合成 MOVE 的因果证明。owned 与 Explorer 同样处理，原始 DOWN 不变。
