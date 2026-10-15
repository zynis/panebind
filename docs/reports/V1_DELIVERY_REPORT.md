# PaneBind V1 集中交付进展

当前用户功能接线：正式 Medium GUI／托盘入口已接通普通窗口资格、Move／Resize 磁吸、Ctrl 邻接组移动／共享边缩放、停靠／拉伸、窗口命令、中文六页设置与应用规则；离散布局恢复、最大化起手恢复、独立 native writer、视觉属性恢复及实际吸附／共享边反馈均为生产调用链，不只是 helper 或测试模型。

实际能证明的操作目前仍主要来自隔离 guest 的普通权限 owned 窗口。已有 T→L／四格／八窗与历史 grid 拒绝子场景；最新 cf／1bee 在 I10 发 DOWN 前因 hover／前台对象改变而中止，本次 DOWN 和 native 均未发出，不是 grid 或 UP 的再次复现。post-stop 的实际点击发送和窗口计数增长通过，但整体仍 FAIL。真实 Explorer、完整跨应用组、显示矩阵和最终便携包仍未通过整版验收，不将接线或局部自动 DEV 等同宿主试用／人工 UAT。

## 源码与包身份

本稿是持续开发快照，不固定最终交付 SHA，不预填下一批结果。

| 项目 | 当前准确状态 |
| --- | --- |
| 开发分支 | `codex/r1c4b-live-magnet` |
| 方案入库 | `c1e8f82f8156db7c4778473f6b4689327f771013`；变更限 `AGENTS.md`、`README.md`、`docs/PANEBIND_V1_DEVELOPMENT_GUIDE.md`，主执行者已完成标准推送 |
| 最新已运行 runtime | `cf2232d9f8112ce82b5b45ce4b40dab95486918c`；Debug／Release 构建与各 10 项 offline 通过，实际现场运行 Release；不据此转记功能通过 |
| 最新已审阅现场 | cf／RunId `1bee4d83fa2d4786ad0319a69d3227c6`；I10 DOWN 前 hover／前台对象改变，本次 DOWN／native 未发出，整体 FAIL；eae／d49／8453 原始失败保留 |
| 当前批次收尾 | 1bee collection 与精确 guest／任务清理回执已保存，证据未变；shutdown 资源字段 true，post-stop 真实发送及计数 `[1,1]→[2,2]` 通过此子场景，不代表整版 cleanup 验收 |
| 当前 guest ZIP／manifest SHA-256 | `C454AC6F6E793A73BB41B6E317856791C5CDD4D39FB4F8B5C2A378A4DF7554FD`／`7124F85243D54C9B4741B461D8E3C8AC0B0B68580D6A6C01C52F3D1654594C4A` |
| 当前真便携包 | PackageId `d6ab86dd05844a938e8e466bf0c0a21b`；ZIP `95DBF8BA35BBD3B016B808341E41EA5870C76B1A7FCBF2320E92AE79E5021F58`；Release EXE `1DFCC8522194FFC94D13EC387B1C9F931DF81E72DB7355DFF0609B1D1AF015A6`；1bee 未到达 I12 普通入口，NOT_RUN；旧 b650／6d022 保留 |
| 历史 aae 已收证 | `a2eb05f7e76d05ec9378e0d12631dfd22c326fbb`／`aae67076ccda4bdb9300c9015108231a`，整体 FAIL；原始证据及清理回执保留 |
| aae ZIP／manifest SHA-256 | `31132A5BCBB4EAB8F488BCE647B80A5A36A6E57540B5DA83FF41FC5BE2D8B8EF`／`DF95AEEA2336F7217A051A5341B8BE85F5E379147B48CC377B369ABCEF4BC793` |
| aae Release／Debug EXE SHA-256 | `A1F92E7773607A6DB8967E0AE10B47257474C3947B3F3B96B643A639574C9E56`／`6C1B5EB112CC8169E166A13A627AE16B2A354BAB63882224FCC65301E0C2C596` |
| 已实际运行历史批 88e | 源码 `fcca29306ded7f483f8381901bbc7fb706cdf1c6`；RunId `88e4574bac3c4eba801431a559d58a3a` |
| 已实际运行历史批 8cc | 源码 `be011280b0d8dc7715163655642964cb112f5656`；RunId `8cc920c8b1584531b248e602b672d186` |
| 8cc ZIP SHA-256 | `902FBFFED8028C3B0DD2C1ED69A9C3099BB53F7E6B87B5DCB449C9EDBADA0479` |
| 8cc manifest SHA-256 | `F907191C43BC13423890EC5EEC4B417F7546BA7FDA5C29C015F5284D74BCC5AD` |
| 8cc Release／Debug EXE SHA-256 | `E64087A761163A9A1BA4293291997098B822F177C9BC369DF35BEF4993FB4A39`／`CE5B232741824E5BA68F47F43867AB3EB400658F572C3FE2760A00A3EA22475B` |

每批实际身份保存在 `uat/v1-sandbox/<RunId>/package-receipt.json` 与 `input/manifest.json`，便携包在 `uat/v1-portable/<PackageId>/package-receipt.json`。历史包与原始失败继续保留。`source_commit`／EXE self identity 为 RUNTIME，`packaging_head` 为实际文档打包 HEAD；仅允许三个交付文档的已提交差异，源码／脚本／LICENSE／AGENTS 及 dirty 输入均拒绝。实际纯 Git guard 正反检查已通过，不赋予功能 PASS，也不将旧 ZIP 结果改绑定到文档更新后的新 ZIP。打包 receipt 的部署字段不是当前执行结果，现场状态以实际回执为准。

## 已运行的功能与明确失败

### 1bee／cf：I10 发 DOWN 前对象改变，真实 post-stop 点击通过

原始证据：`uat/v1-control/1bee4d83fa2d4786ad0319a69d3227c6/collection-20261015T161147990Z-6fe4611acd8b4b148eb258a15c99dbcb/`。

- fixture seq507 已完成本批 owned T／L／四格／八窗子链；seq509–513 准备下一精确 source 与实际 cursor。产品 PID3604 seq136 曾确认 source HWND459520／HTRIGHT，随后 seq137–139 记录前台及 hover 对象改变、资格／命中不满足；fixture seq514 明确 `test_DOWN_sent=false, test_UP_sent=false, generation=0`，seq515 为 `layout_source_or_hover_hit_changed_before_DOWN`。本次 I10 没有 DOWN、START 或 native 调用，不是 grid 非 exact 或缺失 UP 的新复现，改变对象的根因未定。
- seq516 正常退出，产品 seq140／141 为 timing／shutdown；所有资源字段 true、无 unknown native completion。seq517–520 确有实际 cursor、DOWN／UP 发送及窗口计数 `[1,1]→[2,2]`，post-stop 投递子场景 PASS；seq521 整体仍 FAIL。collection 与 cleanup 回执确认精确 guest／任务清理、证据未变，不由 guest 销毁推定产品 cleanup。
- 既有配对修复已提交：`85c1599172f946ef86b9e84838e0a068fcda3341` 退休旧 aborted pair；`b092f01ab18ba9624b4d483c9e2404a844ac6309` 修复 authority=0 下新配对及 hover 准备刷新；`2c37573ae7afc160b00094474eabef411c03949c` 增加 mapped 配对精确 generation CAS。不能据此认定历史实际 UP 缺失根因已证实或闭环。2c 的 ea7 包仅部署、未启动，不算现场运行。
- 托盘真实菜单、五键与映射用例已接生产测试入口，1bee 未到达，保持 NOT_RUN；后续 I10 恢复、设置生命周期、I11、应用矩阵及真便携 ZIP 普通入口均未由本批验证。不预填后续源码／包结果。

### eae／2a：首写入前 FocusLost，真实 post-stop 点击通过

原始证据：`uat/v1-control/eae17bbdfc6740789cd8c06b602c1be4/collection-20261015T152931313Z-b28a5b4cf7b640058713ddc85354334b/`。

- 产品 PID976 的 seq138 为本代真实 DOWN，seq139 为 gen16 START，seq140 立即以 end=2／FocusLost 终止，未发生本代 native placement。fixture seq517 的 grid 检查 FAIL，实际仍为原矩形 `[140,140,540,540]`；其 `observed_native_result` 固定文案不是 API 回执，不能据此宣称已调用或再次复现 grid 非 exact。
- 失败后仅一笔测试清理 UP（seq519，SendInput 返回 1）；shutdown seq145 的实际 UP callback 快照 count=16、last generation=15，没有本代 UP 进入现有 hook 的观察。不能据此断定 UIPI、系统根因或制造 UP／NormalUp；d49 曾真正执行 grid 后拒绝及缺失 UP 的历史保持原样。
- post-stop 此次确有独立 cursor、DOWN、UP（seq522–524），seq525 明确 `executed=true`、两笔 SendInput 实际调用、窗口计数 `[1,1]→[2,2]`，该真实投递子场景 PASS。与 d49 未走到发送、仅有旧 raw `executed=true` 的情形区分；退出字段 true、guest 销毁或本次点击都不把整批 FAIL 改为 PASS。
- 当时发现确定性源码反例：旧 aborted 配对残留时，新 DOWN 透传但对应新 UP 被旧配对吞掉；后续 85c／b092 与 2c 的已提交修复见上节。该反例不自动证明 eae FocusLost 或未观察 UP 的根因，历史失败保留；恢复、在途失败和 I12 未运行仍保持 NOT_RUN。

### d49／ff：grid 单次调用成立，abort 后实际 UP 未观察

原始证据：`uat/v1-control/d49ec6c63b5341b8b70f054106a4c74d/collection-20261015T151212261Z-f94be534497241cfb01ea55a64b9a124/`。

- owned T／L／四格／八窗子链再次完成（seq507），本批真实 grid17 目标 `[140,140,577,540]`、实际 `[140,140,564,539]`，SetWindowPos 返回 true，产品记 non-exact 并 abort，不假称 API FALSE（fixture seq517，产品 PID5832／gen16／call42）。
- seq518 的单次 native 调用子项通过；driver outstanding=true、Async=0 与最后 native 回执 authority_after=true 分别记录。最后一项是原生回执当时的值，不表示 abort 后继续拥有写入权限；任何一项都不代替真实抬键。
- abort 后 status 真实响应（seq520／521）；测试 UP 的 SendInput 返回 1（seq522），但 seq523 未观察到本代产品实际 UP，`I10_abort_real_UP_not_NormalUp=FAIL`。这只确定观察合同未满足，尚不确定消息在哪一层中断，也不制造 NormalUp。
- 正常退出码 0、shutdown 全部资源 flags true、unknown completion=false；seq525→526 间没有 test_cursor／test_button_send，post-stop 未执行到点击发送，具体未满足的前置条件 UNKNOWN。保留 seq526 的 raw `executed=true, actual_down_up_delivered=false`；其中 executed 来自资源 flags，不证明已点击，因此本批不构成“产品吞输入／投递失败”证据，也不判整体 cleanup PASS。guest／临时任务按精确归属清理。grid 后新 Move／Ctrl、第二成员在途失败、设置生命周期、中断／15 分钟链、应用矩阵及 I12 便携入口均未到达，继续 NOT_RUN；普通接线／fixture 缺陷仍在授权范围内定位修复。

### cb807／8453：T→L、四格和八窗通过子场景，grid 后联合断言失败

- cb807：源码 `48bd08fa2f8202399f77d9f28d47433fa79167fd`，collection `uat/v1-control/cb80769a1ae24298a1f7ccc5946d48d3/collection-20261015T143649465Z-d5c83e648265495ba700b49af4ba84d2/`。T 两轴共享缩放、成员最小值／原始 DOWN 拉回、实际 Resize 后普通 Move 重建接触、L 布局换 source Ctrl Move、四格双轴及后续普通 Move 均通过对应检查（seq303–397）。八窗初始独立布局 seq398 失败；整体仍 FAIL，不能继承为八窗通过。
- 8453：源码 e811，collection `uat/v1-control/8453f79385e64ca29c2a73eeb4bc305c/collection-20261015T144744444Z-042358dfed054ee0bb830e0709f2bf4d/`。按实际 native 最小尺寸准备八窗后，末成员作为 Ctrl source、全部成员几何及该组资源销毁通过（seq419–430）；此前 T／L／四格对应操作也完成。
- 8453 原生 grid17 实际产生目标与回读不一致，产品拒绝且未伪称 API FALSE，seq440 通过。但紧接的 `I10_native_grid_single_attempt_before_revoke` 联合断言 seq441 失败；原记录不能区分次数、driver 账本、键状态等哪一子项失败，不预写“UP 已坏”“多次写入”或具体根因。后续 Move／Ctrl 恢复、I10 其余项目与整条链不计 PASS。
- 两批正常 stop 的资源字段与独立 client 投递分别保留；最终投递检查 cb807 seq401／8453 seq445 均为 false，不把进程退出或编排 guest 清理替代整体产品 cleanup。旧 aae 的 T START 失败仍保留，但不再作为“尚未越过 T”的最新进度。

### aae：双窗共享 Resize 局部通过，T 布局 START 失败

原始证据目录：

`uat/v1-control/aae67076ccda4bdb9300c9015108231a/collection-20261015T140152142Z-ddf706cc15c248109b5ed8df484de5e5/`

- Release／Medium 的双窗竖向共享边、相反侧 source、横向共享边实际缩放及 min 交集统一钳制通过（fixture seq548–582）；参与窗口实际矩形与产品 native exact 回执对应，例如 `process_id=6704` 的 call35–40。固定尺寸邻居场景在真实 DOWN／Move／UP 后保持几何且没有 placement（seq583–591）。这些是 M07／约束处理子场景，不是完整三窗／所有 source 或整条 I10 通过。
- 随后新建 T 布局已到实际前台／命中准备；产品 seq89 是 `prepared_actual_source_and_hit`，seq90 读到 source 自身 capture／move-size=`1835770`，fixture seq598 为 `layout_real_DOWN_production_start=FAIL`。该次未领取产品 START，不能写作原生观测到 `clicks=2`，也不能以准备快照推定产品已接管。
- 后续 caf4 已补“拖动过的 press 不再保留为双击首击候选”，支持返回原锚点后仍失效；cb807／8453 已越过 T 并完成上述子场景。该复验不补造 aae 缺失的 clicks=2 观察，也不宣布系统根因。
- 原产品正常退出，shutdown 的 hooks／overlay／native worker／visual recovery／restore 标记均为 true，issued completion unknown=false；但 fixture seq601 的实际 post-stop client DOWN／UP 投递为 false。因此只报告原生退出回执和失败的输入恢复观察，不判整体 cleanup PASS。
- collection 已保存，本批精确 guest 已退出、临时任务已移除，证据前后哈希不变；这些编排清理不代替产品 cleanup。T 之后的 grid17、设置生命周期、完整连续链、应用矩阵和真 ZIP 普通入口未由本批执行，保持 NOT_RUN。

### 88e：Release／Medium 局部功能

原始证据目录：

`uat/v1-control/88e4574bac3c4eba801431a559d58a3a/collection-20261015T130241266Z-5cad0286f4094d2a98d5965a144fdeb2/`

- owned A/B/C 普通 Move 五阶段、Ctrl A、实际 Resize 后 A 回环、Ctrl C source／follower，以及 Center／DockLeft 实际矩形通过。它证明 owned 操作子链，不是完整 Explorer 或混合应用 I02。
- 真实生产设置窗口、六页截图及鼠标／Tab／Enter／Esc 操作、取消不保存、吸引距离改为 0 后下一手势效果、正常重启后设置及实际效果保持，通过对应局部检查。六页截图存在不等于所有 DPI、可访问性或设置项均已验收。
- Explorer 止于 `new_directory_file_identity_unavailable`，未执行其功能流程。
- Edge 绑定后，真实 DOWN 的产品手势检查失败；VS Code／Terminal 绑定受阻，不能记作应用功能通过。
- 视觉测试已观察原始属性、正常退出、临时透明 UP 恢复及两次摇动的相应结果，但最后 `visual_GUI_real_checkbox` 即时读取失败（fixture seq539），整批仍为 FAIL。后续 be 版本修正了该读取等待，不能回写旧批通过。

### 8cc：视觉流程返回，布局阶段中断

原始证据目录：

`uat/v1-control/8cc920c8b1584531b248e602b672d186/collection-20261015T132057658Z-ce753c3739044698b6a459e57d538228/`

- 视觉流程执行到原配置、原始 tuple 和布局恢复，并写出 `visual_mapping_sequence`（fixture seq600）。这是该批视觉子流程证据，不是整版清理或所有异常退出通过。
- 随后布局 fixture 创建、设置及前台核验完成，原始记录停在 seq605 的光标观察。产品日志需按 `process_id=7184` 区分重启后的实例：seq61／63 曾准备成功，seq62 出现 `application_track_limits_unavailable`，seq64 实际 capture 和 move-size 均为 HWND `1508758`。这些事实支持定位自建 STA／同步查询／原生操作衔接，不能仅凭行序断言全部跨线程因果。
- 本批没有正常完成的 guest summary／整批退出回执。已按精确本轮实例身份销毁 guest、移除临时任务并保留证据；这是测试影响被隔离，**不是产品 cleanup PASS**，也不证明原生调用已经自行退出。
- Explorer 新目录的本地 file ID 已真实取得；但目标根窗 token 记录为 `integrity_rid=8192, elevated=true, medium=false`（fixture seq252），产品随即拒绝 `target_not_medium_non_elevated`，没有发送该目标手势。不能把 bootstrap 的 High 或旧 High guest 基线写成这个根窗的 `12288`，也不启用旧例外绕过正式 Medium 判定。
- Edge 的 DOWN 前身份复核失败仍保留。Terminal 原观察表明实际根映像为 cmd，不是此前假定的 conhost。Code 的隐藏根／子根资格拒绝（主执行者诊断中含 `0x80070005`）尚未确定根因；不能把准备 ZIP、启动命令返回或移除参数当作绑定成功。
- guest 的 System32 Notepad 与 WindowsNotepad Appx 确实缺失；这是当前矩阵的具体软件缺项，不是已经测试失败的 Notepad 业务行为。

此前 269b 的首设置 checkbox 命中失败、df93 的外层 1800／内层 1200 超时范围不一致及无 fixture 输入拒绝，也继续保留原始记录。后续修复／新 Run 不将历史改写成通过。

## 当前生产实现与新修复

后继批 `92b2b41ea7f946bebca7a519a3c79831`（源码 `caf4f7934258105d9eed743aaa438174d67a20f8`）已生成并部署真实七文件便携 ZIP，但未到达其普通入口验收。Release 视觉流程在第二次摇动起手后实际记录 `FocusLost`（产品 seq45），独立系统光标回读仍为旧位置（fixture seq470）；这不是已确定的 hook 根因，也不放宽失焦撤权。随后正常 shutdown 的资源字段及实际客户区 DOWN/UP 投递均通过（fixture seq473–475），整批依然 FAIL。完整原始证据保存在 `uat/v1-control/92b2b41ea7f946bebca7a519a3c79831/collection-20261015T142746862Z-2777c1a48e6244e7839ee601f67830af/`；精确 guest／启动任务已清理，文件保留。收集中文目录时的首份部分副本也保留，后续仅设置编排进程与本次 SSH 命令的 UTF-8，不修改机器／用户配置，取得稳定前后清单。

该局部失败不阻止未运行的布局、约束、设置与中断流程；下一批把它们前移，仍保留视觉失败与必要的一次失败时只读事实，不重复操作未知前台窗口。

主通路为 `panebind.exe` → 轻量真实输入适配 → `Catalog` 身份／桌面／几何 → 锁定原始 DOWN 与成员／配置 → 纯几何计划 → 唯一 `WindowWriter` → 实际回读 → 关系和非激活反馈。正式入口是通用 Medium 产品，不依赖固定三窗、Explorer nonce、旧 shield 交接或自动 driver 才能使用。

已提交 runtime 包含此前 4232 以来的具体修正：

- hover 预备只取资格／命中所需快照，避免重复同步 bounds 查询；真实 operation 仍保留完整 bounds 预检，不把未知约束设为允许。
- 测试 STA 的有界等待处理必要 sent messages，针对 8cc 自建窗约束查询与等待相互阻断；不是给产品增设轮询。
- Catalog 使用确定的实际矩形遮挡判定；吸附边线与 Ctrl 共享边从实际回执／几何绘制。尚无本批现场可视验收。
- Edge driver 消除 DOWN 前重复 bounds 查询；Terminal 精确根映像适配为实际 cmd。都等待匹配新构建复验，不凭修代码关闭。
- 已接原生 grid17 拒绝与后续 Move／Ctrl 恢复入口；8453 已真实观察 non-exact 后拒绝，但后续联合断言失败，恢复尚未通过。grid 由自有应用在 `WM_WINDOWPOSCHANGING` 实施，不伪造产品已知 grid。
- Code 去掉可能覆盖显式命令行的 bootstrap data 设置，仅作为可区分假设；尚不能据此宣布根因或修复成功。

原始 DOWN、单 writer、撤权后无新调用、在途调用与完成未知分开、actual postverify 及精确视觉所有权合同继续保留。构建、L1/core 测试与局部 owned 结果都不能替代当前 Release 的整功能／模块验收。

ff 已提交并构建：真实七文件便携 ZIP 的精确源身份／哈希和普通无参入口验证；I10 在途成员失败／存活成员恢复用例；测试 driver 账本、实际低级鼠标回调和 GetAsyncKeyState 分开观察，避免联合断言无法定位。d49 的分流已给出上述具体事实，但未到达后续在途／便携路径。JSON 现有真实生产接线为 57/60 IMPLEMENTED，三项 M12 交付工作为 IN_PROGRESS；这是实现分类，不是 57 项 DEV PASS。ZIP 解包合成检查、身份 guard、构建和离线回归均不替代现场功能。

## 集中缺口与待核验项

### 已修或已有接线，等待对应现场闭环

| 项目 | 证据与现状 | 下一项明确验证 |
| --- | --- | --- |
| 设置命中／checkbox 时序 | 269b 首命中失败的定向修复后已有真实六页操作；88e 视觉 checkbox 失败后，aae 视觉配置／tuple／布局恢复流程返回 | 旧失败保留，不再把已复验的同一 checkbox 当未修缺陷；登录启动、排除、导入导出及损坏配置 I07 仍待到达 |
| 布局 START／click 候选 | aae 原失败保留；cb807／8453 已完成 T→L、四格，8453 八窗末成员联动通过子场景 | 继续当前后续功能，不重开同一 T 专项；完整拓扑／应用／环境不能由局部结果替代 |
| Edge／Terminal adapter | 重复 bounds 查询及实际 cmd 根映像识别已有具体修正 | 精确新窗口重新绑定、真实 DOWN、完整几何及客户区输入 |
| grid 与应用钳制 | 8453／d49 曾真实 non-exact 后拒绝；eae 无本代 native；1bee 更早在 DOWN 前 hover／前台对象改变 | 保留每批差异；1bee 不能新增 grid／UP 结论，仍须真实匹配输入与后续 Move／Ctrl |
| 视觉／恢复与边线 | 临时／永久原值、独立 helper、completion barrier、迟到完成与实际边线已接线，已有视觉局部证据 | 当前构建补异常／未知完成和反馈实际观察，不能把正常 stop 推成硬崩溃通过 |
| post-stop 实际输入恢复 | 1bee／eae 实际点击发送、命中和计数增长 PASS；d49 未执行到发送，不能判投递失败；历史各批原记录保留 | 继续分别记录发送／实际接收，不能推成整版 cleanup 验收 |
| aborted／mapped 配对与准备 | 85c／b092 已修旧配对、authority=0 新配对与 hover 准备，2c 已接 mapped 精确 generation CAS | 历史实际 UP 缺失原因 UNKNOWN；1bee 未发本次 DOWN，不作为配对现场复验 PASS |
| 托盘与五键入口 | 已接实际菜单／五键／映射的 guest 用例；1bee 未到达 | NOT_RUN，不由离线组合测试或可编译推定实际入口通过 |

### 仍缺的实际环境或尚未闭合能力

| 项目 | 准确边界 |
| --- | --- |
| 正式 Medium Explorer | 本批根窗 elevated=true，不满足正式资格；本地目录身份已恢复，不再误报为 file ID 未实现。不得套用旧 High guest 例外。 |
| Code | 精确可见合格根绑定仍未成立，隐藏根／子根拒绝尚未定因；参数变化只是待检验假设。 |
| Notepad | 当前 guest 的指定系统 EXE／Appx 缺失；未运行的应用路径不能记 PASS，不在宿主安装或修改账户／UAC来补分。 |
| E03／E04 | 当前没有真实双屏／负坐标与混合 DPI 环境证据，属于具体环境缺项；纯模拟坐标不代替。 |
| 200% UI、虚拟桌面、普通用户真机 | 尚未执行，分别保留 `NOT_RUN`；当前截图或 guest 局部结果不外推。 |
| I08／I10／I11 | driver 断开收尾、第二成员真实 native failure、完整两轮与 15 分钟混合运行未闭环；d49 在 grid 后实际 UP 检查失败，eae 更早 FocusLost，1bee 在 DOWN 前准备失败。grid 非 exact 或窗口消失不冒充 API 返回 FALSE。 |
| 最终便携交付 | 最新真便携包 d6ab 已绑定 cf 并加入 1bee，实际普通无参入口未到达；旧 b650／6d022 保留。解包／打包准备、identity-only 或合成检查不替代 I12。 |

## 持续验证与交付边界

验收全集见 [V1_ACCEPTANCE.json](V1_ACCEPTANCE.json)，产品操作见[中文快速入门](../V1_QUICK_START.md)。当前仍为 **0/40** 对标整体通过；B01–B32 全部必过，B33–B40 固定延后但保留在分母中。整版 DEV 门槛尚未满足，真人 UAT 为 `PENDING`。文档、机器表与后续证据更新应继续区分源码接通、局部实际结果和完整门槛，不能用测试数或旧 High 局部证据凑分。

主执行继续匹配当前 SHA／新 RunId 的隔离验证和普通缺陷修复；本稿不预填这些功能结果，也不构成阶段终止点。后续源码／包哈希、功能回执及最终标准 Git 远端核验继续由实际证据集中补齐。原失败与历史包保留，`uat/`、EXE、ZIP 不入 Git；不创建 PR、merge、tag 或公开 release。

现场自动输入仍仅在本任务专属可丢弃 guest；远端文件仅在 `I:\PaneBindMVP1Runs\` 本任务目录，不触碰 CrossRec、其他用户数据、账户、token、UAC 或宿主设置。普通宿主试用和真人手感不能由自动 DEV 自签通过。
