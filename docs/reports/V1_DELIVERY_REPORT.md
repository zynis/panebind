# PaneBind V1 集中交付进展

当前用户功能接线：正式 Medium GUI／托盘入口已接通普通窗口资格、Move／Resize 磁吸、Ctrl 邻接组移动／共享边缩放、停靠／拉伸、窗口命令、中文六页设置与应用规则；离散布局恢复、最大化起手恢复、独立 native writer、视觉属性恢复及实际吸附／共享边反馈均为生产调用链，不只是 helper 或测试模型。

本次集中阶段交付为 **PARTIAL**，不是 COMPLETE。交付已测 Release 便携包 e2abc／runtime dd60：干净 guest 中普通空参数显示中文设置，真实 GUI 保存前后两次 Move／NormalUp、正常退出／字节检查，以及设置底部诊断导出／解析专项均有实际结果。主要功能动作仍单独绑定 b60／c01：Terminal 类别、单屏停靠／拉伸核心、actor 默认截止后新手势及第二成员失败恢复等，不把这些旧构建动作改绑定 dd60。普通 I11、grid／sourceclose 原代实际 UP 缺失的 **S1 未关闭，不宜宿主试用**；托盘、正式 Medium Explorer、完整五类应用及显示矩阵仍未过，顺序 Debug→Release 的输入／前台差异原因 UNKNOWN。60项已实施不等于正确性或DEV通过，正式功能／模块／B／I通过均为0；真人UAT仍PENDING。

## 现有验收表的推进状态与仓库内证据

继续整版开发时，已将60个功能、12个模块、B01–B32、I01–I12、环境和应用行的既有实测逐项回填，不再让已经失败／受阻的主状态全停在 `NOT_RUN`。功能当前为9项 `FAIL`、51项 `BLOCKED`，正式PASS仍0；`FAIL`表示原身份绑定的实际反例尚未关闭，`BLOCKED`表示该行写明的完整合同／环境／当前候选复验条件尚缺，而不是声称所有子项未运行。B33–B40继续固定DEFERRED／0分。

每行有 `observed`、`remaining_conditions`、`next_action`，以及 `partial_observation_ids`→原观察的产品SHA／二进制哈希／RunId／证据关联；`candidate_identity_ref`仅指待验当前候选，不把旧观察改绑新EXE。模块／对标条件直接引用功能行，未另建一套任务书。后续产品修复或现场新结果按实际身份增量登记；驱动／文档变更不得清空既有产品证据。

[仓库内证据索引](v1-evidence/index.json)和[摘录说明](v1-evidence/README.md)保留152条关键原始观察的脱敏摘录，含dd60／714c真实普通ZIP与诊断、b60／c01组件通过及缺UP／恢复／应用失败、50df／bdab owned回环及T/L/四格、dd60／97顺序Release客户激活失败。已独立核对四份原日志整文件哈希及对应16个原EXE哈希；每条摘录另带原行UTF-8 digest。原始大文件和包仍在索引所列ignored本地`uat/`位置，无需为源码复核反复上传整个目录。

本节只是既有结果映射，不是重新运行现场或结束整版任务。实际缺UP、快速拖动消费时序风险、托盘、官方应用准备和显示环境分别推进；新的确定性反例与修复不被误写成既有LL-UP缺失根因。完整功能合同未齐时不计PASS，不以PARTIAL作为主动停止条件。

## 新确定的输入消费时序缺陷：代码修复与实际定向复验

用户提出的 `InputService` 真实UP先清active、GUI消费迟到MOVE仍要求active的风险，已用实际共用生产判定最小提取复现：旧版定向Debug为226 checks／1 failure。这是已确定的独立时序缺陷，不是此前实际LL UP缺失的根因证明。

产品修复源码为 `408ac527a6bfda7b01a5e83249bd5ed0eca2714d`：排队旧MOVE仍拒绝；只有实际匹配NormalUp的UP点可以提交一次独立terminal计划，按原DOWN／窗口锚点求绝对几何，native发起前仍复核权限。后继DOWN、停止、abort或真实上下文失效不能恢复旧代写权限；旧代finish只退休自身，不能撤销较新代。修复后同一反例及相关代际／一次性／原锚点定向Debug为239 checks／0 failures。

[before／after小型转录](v1-evidence/input-up-dispatch-directed.txt)明确是`exec_command`输出转录，**不是程序原stdout、真实Windows输入投递或guest功能证据**；原转录与脱敏文件哈希分别入索引。随后8189取得下述真实快速UP点terminal定向结果，V1-D15已按该精确范围关闭，不再扩展专项。原D08缺LL-UP S1及完整连续链门槛保持未关闭，两者不是同一根因；dd60旧已测包也不换绑为新R。

新R产品在专用`out/v1-product-408ac52`完成Debug／Release构建及各10项offline实际PASS（4.14s／3.49s）。Release EXE为836608字节、SHA-256 `48F96C2A46A259979D484E0585C471C3FC8F9880EA1239B99C7A83D89CE3924E`；Debug为3287552字节、`BC75138C4E52B215ABA3BAD65643DDF192D0A1B295309C2F280BA9D131A39345`，独立只读哈希与两份`--build-identity`均核实R。另一个driver／脚本提交D=`83bf90362faafdfd431669b2ca545364d3898bcd`只构建测试工具，不重烘R产品。详见[构建与offline转录](v1-evidence/408ac52-build-offline-transcript.txt)；CTest当时未tee，转录不冒充原stdout或现场，当前`LastTest.log`只代表后执行的Debug。新Run8189的真实范围另列如下，不能升级正式DEV或关闭D08。

## 8189／新R：快速UP点terminal闭环，两个driver错pin失败保留

RunId `81896dad13d5423e5de14a8363f7bcc6`，Release／真实Medium RID8192、non-elevated、session1、Default／WinSta0、单屏144DPI。产品R=`408ac527a6bfda7b01a5e83249bd5ed0eca2714d`／EXE `48F96C…3924E`，driver D=`83bf90362faafdfd431669b2ca545364d3898bcd`／EXE `B7E0A77896E0DB3B841C66396AFEC93ADB4FA1F2A89BE9CA31CFEB1B7F609160`；packaging HEAD=`665b8289e36eac93e0ea485a79e52f541dd8120f`。主包ZIP `BA27609500A64AE2EEE0C4070F7064DC0435EF97398C6086DAB8E4DD0DAC0FFC`／manifest `99AE0E4A2AFCE93155CFFF90C97C9D980B946F0505973619E333DC1E2BF6E4D8`均独立重算一致。原件在`uat/v1-control/81896dad13d5423e5de14a8363f7bcc6/collection-20261016T075702366Z-19db097048e24dcb8b4604baadcb14fb/`；[fixture摘录](v1-evidence/8189-fixture-selected.jsonl)与[product摘录](v1-evidence/8189-product-selected.jsonl)已入库。

- 243单次SendInput数组插入DOWN／MOVE／UP三个元素，未等产品START再放UP；插入成功本身不作投递证明。244取得同g2实际产品LL UP和NormalUp，独立actual矩形`[195,271,595,611]` exact；普通native attempt=0、terminal=1。产品PID4148实际seq33 UP→34真实terminal placement（`normal_up_completion=true,authority_after=false`）→35 NormalUp／pending退休。没有恢复active权限或重放旧MOVE；测试也没有从插入方式推断GUI实际落后时长。
- 本次被动队首hook实际观察UP一次、CallNext返回1、Unhook成功；仅为guest测试观测，不是产品Raw替代归属，也不证明旧缺LL-UP根因。后继同批导入设置的真实Move470／NormalUp475，以及重启后移除排除的Move625／NormalUp630成立。V1-D15定向关闭，D08仍OPEN；这些片段不拼成Full／I02／I11。
- 当前R设置GUI208、登录启动285、五键393、导入导出493、损坏重启545、排除635各组件有实际阶段PASS。托盘551／557仍BLOCKED，实际NIM_ADD=false，未发送未知菜单输入。
- 225实际导出1579字节`diagnostic-51359.txt`，其真实header为产品R，但D driver仍用自身源码SHA核对；226／233保持FAIL，未把文件存在算parser通过。641的portable-ready marker也正确写R而被错用D核验，保持原FAIL，**新2632 ZIP的普通空参入口本批NOT_RUN**，不是产品启动或几何失败。V1-D16登记这三处driver产品SHA pin缺陷；工作区修复用guard已核验的product_source，待新driver以同R复验，不重建产品。
- 642产品正常退出、资源flags全true、unknown native=false；646实际客户计数`[3,3]→[4,4]`，与后续精确guest／临时任务退出分别记账。cleanup回执确认证据未变、部署保留、其他资源未改，不能以guest销毁替代产品结论。driver648整体FAIL保留。

后续先由修正driver的`CoreAndApplications`推进未跑核心／应用，再定向`SettingsDiagnostic`复验两个pin失败；复用同R EXE，不重复已通过GUI，不将分组子项拼成Full。portable2632 ZIP哈希为`4D7CDAEFC22208233716A24DC2DE6B98FAFEF14A83DCB6B450FF2DF20E5602A2`、manifest `090EFBA44ED960CBB9A16DA75A780A14B38A74D4AFDBAB04F3584B189E1B401C`，只作已核构建身份，不宣普通入口已验。

## 源码与包身份

本稿是持续开发快照，不固定最终交付 SHA，不预填下一批结果。

| 项目 | 当前准确状态 |
| --- | --- |
| 开发分支 | `codex/r1c4b-live-magnet` |
| 新R实际候选 | R=`408ac527a6bfda7b01a5e83249bd5ed0eca2714d`，D/R构建与各10offline通过；8189取得快速UP点terminal和GUI组件范围，两个driver错pin失败保留。下列dd60仍是此前实际验证的普通入口包，不改写其身份 |
| 方案入库 | `c1e8f82f8156db7c4778473f6b4689327f771013`；变更限 `AGENTS.md`、`README.md`、`docs/PANEBIND_V1_DEVELOPMENT_GUIDE.md`，主执行者已完成标准推送 |
| 本阶段 runtime／build | `dd603a79aeba64c816e13ba49265db342c14e7e3`；Debug／Release fresh构建及各10项offline实际PASS；新EXE只取得下列诊断／普通ZIP专项证据，不迁移绑定旧整套动作 |
| 此包打包 HEAD | `dd603a79aeba64c816e13ba49265db342c14e7e3`；已测ZIP不重包。最终三文档提交SHA由主执行者实际commit／push后给出，不在本文预填 |
| 最新已审阅现场 | `714c69f6feba4a0f81678c314eb1747e`，干净guest、Release-only／Medium／`SettingsDiagnostic`，106行scoped PASS；97 Debug专项PASS及后继Release失败同时保留，不外推Full／I01／I12／整版 |
| 主要功能实测身份 | `b60e9c343c29f75007df7bf1833fd5d4ab8d3cf8`／Run `c01ffe67d3004961bd00765312869c8a`。产品主要逻辑未因此重写，但完整动作未在dd60重新运行，不把历史结果换新EXE标签 |
| 714c实际退出 | ZIP实例PID5696正常退出，资源flags全true、unknown native=false；实际post-stop客户计数`[2,2]→[3,3]`。精确guest／任务已清理、证据未变、部署保留。旧c01 Edge清理未知不因本次退出或guest销毁关闭 |
| 714c guest ZIP／manifest SHA-256 | `805A0F93BA6643E3ACFD4E891421D1878EC848C6F47240064229B3758B829F2C`／`FB2A8AA99EDB8CF343EC5383FACFE53C7179439253A9EA1A3F2D224F2B32B11F` |
| 本阶段已测真便携包 | PackageId `e2abc1ec0dcf422088d62c53bc633d24`；ZIP `FACE7CD67969582B53901EBC91C63AC2C473AD982F86D0F774E3904563C0B556`；manifest `7764C9B0561FB96B11A8B06B9A0A2A3DF5205038460C2BD299C4DC6A66C7FC8B`；Release EXE `0CEA6E75FE171F472A020290C157EAE5A154D44B0AECC75B1E4AC60A8A7786FE`；原字节保留，最新中文指南／报告单独附交付 |
| 历史 aae 已收证 | `a2eb05f7e76d05ec9378e0d12631dfd22c326fbb`／`aae67076ccda4bdb9300c9015108231a`，整体 FAIL；原始证据及清理回执保留 |
| aae ZIP／manifest SHA-256 | `31132A5BCBB4EAB8F488BCE647B80A5A36A6E57540B5DA83FF41FC5BE2D8B8EF`／`DF95AEEA2336F7217A051A5341B8BE85F5E379147B48CC377B369ABCEF4BC793` |
| aae Release／Debug EXE SHA-256 | `A1F92E7773607A6DB8967E0AE10B47257474C3947B3F3B96B643A639574C9E56`／`6C1B5EB112CC8169E166A13A627AE16B2A354BAB63882224FCC65301E0C2C596` |
| 已实际运行历史批 88e | 源码 `fcca29306ded7f483f8381901bbc7fb706cdf1c6`；RunId `88e4574bac3c4eba801431a559d58a3a` |
| 已实际运行历史批 8cc | 源码 `be011280b0d8dc7715163655642964cb112f5656`；RunId `8cc920c8b1584531b248e602b672d186` |
| 8cc ZIP SHA-256 | `902FBFFED8028C3B0DD2C1ED69A9C3099BB53F7E6B87B5DCB449C9EDBADA0479` |
| 8cc manifest SHA-256 | `F907191C43BC13423890EC5EEC4B417F7546BA7FDA5C29C015F5284D74BCC5AD` |
| 8cc Release／Debug EXE SHA-256 | `E64087A761163A9A1BA4293291997098B822F177C9BC369DF35BEF4993FB4A39`／`CE5B232741824E5BA68F47F43867AB3EB400658F572C3FE2760A00A3EA22475B` |

每批实际身份保存在 `uat/v1-sandbox/<RunId>/package-receipt.json` 与 `input/manifest.json`，便携包在 `uat/v1-portable/<PackageId>/package-receipt.json`。历史包与原始失败继续保留。`source_commit`／EXE self identity 为 RUNTIME，`packaging_head` 为实际打包HEAD；允许的三个交付文档已提交差异与源码变化严格分开。此次更新不重新打包已测e2abc：本报告和仓库最新版中文指南是独立附交付材料，不能声称ZIP内的文档也已更新／实测。打包receipt的部署字段不是当前执行结果，现场状态以实际回执为准。

## 已运行的功能与明确失败

### dd60／714c与97：设置GUI诊断和普通ZIP专项闭环，不覆盖旧功能整链

714c原始证据：`uat/v1-control/714c69f6feba4a0f81678c314eb1747e/collection-20261015T194851747Z-315d6753dfb64d8e94079d2cb3c49f5c/`。97原始证据：`uat/v1-control/97a0fa7ebb39452188674011891963d9/collection-20261015T194554881Z-1ac68b9eca0646b7af9c4fb9c7eeac22/`。均为自动guest driver，不是真人UAT。

- 97 Debug实际点击设置窗口底部“诊断 (&D)”，生成1590字节`diagnostic-30546.txt`，解析／隐私专项39、Esc与配置未写40、正常退出及post-stop真实计数`[1,1]→[2,2]`通过，48为该专项PASS。没有因托盘不可达而省略已有GUI导出路径。
- 同guest后继Release29真实客户激活FAIL：DOWN／UP发送各返回1，但实际客户计数仍`[0,0]`、前台不匹配；未到设置、parser或普通ZIP。产品0operations／0callbacks；正常stop后的客户检查因前台条件失败根本没有再发DOWN／UP，不能写“投递失败”。这一顺序配置差异原因UNKNOWN；下列干净guest成功不把它改为通过，也不证明系统根因。
- 干净guest714c只运行Release：设置精确身份、实际诊断按钮点击、新final诊断文件与parser／隐私39、Esc／配置未写40通过。收件`diagnostic-30359.txt`为1590字节，SHA-256 `D35D5EBC610AB887A16F9C732084F69416C7EDBB9B16ABFC52162F8654BB252F`，本地独立重算一致。本次只证明实际owned标题／Run路径不在导出中，不声称所有文档／所有配置的普遍隐私保证。
- 同一714c真实解压e2abc普通ZIP：48空参数启动，50在任何`--settings`命令之前出现唯一可见中文设置；默认吸引10DIP时实际Move目标`[140,80,380,440]`、68同g1 NormalUp。80真实Enter保存、81仅解压目录配置改变，吸引0时下一实际Move目标`[134,83,374,443]`、94同g2 NormalUp。95 ZIP实例正常退出／资源全true，99真实post-stop计数`[2,2]→[3,3]`；101交付字节与无profile检查通过，102是该普通入口子流程PASS。
- 104 scoped summary与106 driver PASS明确仅设置诊断＋普通ZIP，不授托盘／Full／I01／I12／M12／整版通过；103是未请求的额外poststop检查，不是另一次投递失败，也不是第二次stop通过。精确guest／任务清理与证据前后哈希一致独立记账，不能代替产品事实。主要功能的b60/c01观察仍按其原始源码／二进制保存，未换绑到dd60。

### 590／1133：原子临时文件清单缺陷已修，旧失败保留

原始证据：`uat/v1-control/590650e822c34312ad51e172c93f2fce/collection-20261015T193727214Z-95fcc4b1b059456982ca2cf81ad50aed/`。

- Debug真实GUI/click已生成1506字节final诊断文件，但driver误将原子rename中的`.tmp`纳入inventory，随后`file_size`抛错；parser未运行，原41行专项FAIL保持。
- Release精确GUI身份成立而前台不匹配，未完成parser或普通ZIP，32行FAIL保持。单有文件、退出码或guest销毁均不算功能通过。
- driver只清点本次精确final文件的修复由dd60 Debug97和Release714c真实导出／parser复验通过，**这个确定driver问题已专项关闭**，不列作当前产品S1。顺序Debug→Release输入／前台差异仍UNKNOWN，未因此关闭。

### c01／b60：单屏停靠／拉伸、默认截止和失败后恢复取得实际子结果

原始证据：`uat/v1-control/c01ffe67d3004961bd00765312869c8a/collection-20261015T192211498Z-87901c575af04496bcfdf88477990983/`。Release／Medium／`IndependentEntry`，只计实际独立范围，不拼成 Full 或整版通过。

- 设置 GUI／登录启动／五键／导入导出／损坏恢复／排除及还原、七文件 ZIP 普通无参可见设置／真实保存效果／退出、Terminal 本次类别、owned 视觉再次取得对应实际子结果。portable647、Terminal2154、visual1779均只绑定本源码／包；托盘498／504仍因精确 UID1 图标不可达 BLOCKED。
- I04／I05 seq700–1005 单屏核心实际通过：自由移动／XY、屏幕边缘预览与离开取消、角停靠真实 UP 提交、拖离恢复原尺寸与 DOWN 锚点、实际注册半屏热键、停靠后自由移动806／重吸附810、smartDock 开关的不同实际矩形；智能拉伸开启下的边／角双击与 Shift 组合、微移／调高、真实 GUI 更改热键的几何效果及邻居不改。原配置恢复1003与自有窗关闭1004通过。774实际仅单屏，跨屏为 `ENVIRONMENT_BLOCKED`，phase1011保持 BLOCKED，不称整个 I04 或所有智能拉伸模式通过。
- actor g23／call43 真实 DOWN、START 和 exact Move1252通过；默认 deadline=30000ms，产品实际 Deadline end／pending退休1253。30031ms 是 child **发送前 tick87906 到产品 end117937**，不是 actor 退出后或 START 后整30秒。1255无新增 native 调用；1257仅测试 cleanup UP 已发／native idle，同 g实际产品UP仍为null，不补成NormalUp。随后同 PID6356 新 g24 Move、g25 Resize exact及各实际 LL UP／NormalUp通过。I08另一个 sourceclose g20虽撤权、无新调用且后续恢复成立，其原匹配UP1217仍FAIL，整个I08不通过。
- 第二成员故障2017：第一个 exact，第二个原生 API 返回 TRUE 后身份丢失，第三个没有调用；不是观察到 API FALSE。status2020、原代实际UP且无新批2022、存活两窗新 Move2029／形成接触2039／Ctrl2049及匹配UP和资源关闭2053均通过，component2058 PASS。该真实失败与恢复符合本次子场景，不新增必须另取 API FALSE 的门槛，也不升整个I10。
- grid1957／1958实际单次API TRUE／non-exact拒绝、1961 status、1965撤权后无新增placement，以及后续新 Move／Ctrl／真实UP和资源关闭成立；但原g6UP1964为null，grid原断言FAIL、phase2000仍FAIL。临时在途1859的原失败及安全继续记录也保留。
- I11 g26的五阶段实际几何正确，hold为`already_exact_no_native_call`，不是五次 native调用；1372／1373实际LL UP/end仍null，Raw独立计数97→98的source／generation归属UNKNOWN。显式Pause为异常收尾，不是NormalUp；两轮／15分钟未完成。没有可靠共享事件身份，产品未增加 RawNormalUp fallback，S1保持OPEN，不无新证据重复同路线。
- 独立空闲1399实测60.0035803秒、单逻辑核CPU0%，末工作集26,562,560字节、private commit3,981,312字节；private不是驻留，idle不代替I11。Explorer正式资格、Notepad／VSCode缺项及Edge命中／exact关闭问题仍保留。产品正常退出和post-stop计数`[5,5]→[6,6]`成立，精确guest／任务已清理、证据未变；整批仍FAIL，销毁guest不代替未确认的Edge或产品整版cleanup。

### e4／593：普通便携入口、Terminal 与独立组件有实际进展，整批仍 FAIL

原始证据：`uat/v1-control/e4a9f65c5034440f9822babf794956db/collection-20261015T185026239Z-b2e503d862f240b7b630d979e3892d5d/`。这是 `IndependentEntry`，不是绕开失败后补记 Full 通过；每一独立阶段之间都核验当前产品身份、真实 idle／pending 状态、driver 账本及原三窗资源归属，失败保留后才在安全条件成立时继续。

- 真实设置 GUI、登录启动开关／自己的实际 Run 值、导入导出、损坏配置备份／默认恢复、排除及还原的 phase 分别在 seq203、236、442、492、580 通过。标题五键实际映射／同代 UP／置顶反转和客户区五键透传子流程 seq344 通过；不是把映射 UP 当作拖动 NormalUp。
- 真七文件 ZIP 在带中文／空格的独立解压位置以空参数正常启动（seq593）；首次同 PID 唯一中文设置实际可见（595），没有发送 `--settings` 促成首次出现。默认设置、Esc、实际 Move、GUI 保存后下一 Move 效果、正常退出／真实客户输入恢复、交付字节不变及无用户 profile 写入均有回执，647 为该便携子流程 PASS。它不是 `--build-identity`、构建或 driver 注入产品权限的替代入口；真人 UAT 和整版验收仍未通过。
- Release 独立空闲实测 seq1234 为 60.0009748 秒、单逻辑核 CPU 平均 0%，结束工作集 26,361,856 字节、private commit 3,645,440 字节。后者是提交量，不是驻留内存；这不是两轮／15 分钟 I11 证据。owned 视觉映射、原属性及布局恢复组件 seq1614 通过。
- 实际 Terminal 的五阶段 Move、匹配真实 UP、Resize、Center／DockLeft／Undo／Stretch、Esc 恢复与客户区输入 seq1874–1925 通过；1927 精确根关闭确认，1928 为该类别本次范围 PASS。只证明此真实终端类别，不外推 Explorer／编辑器或混合组。
- 托盘 seq498／504 为 BLOCKED：精确当前 UID1 图标不可达。dock 后自由移动806、I08 匹配 UP1032／actor START1064、I11 普通 UP1208、临时在途1694、grid status1796、第二成员 status1824 各保留 FAIL；没有把 `FAIL_SAFE_TO_CONTINUE` 改成场景通过。单屏的跨屏项目774明确为 `ENVIRONMENT_BLOCKED`。
- Explorer 实际根 `integrity_rid=8192,elevated=true,medium=false`，资格拒绝且没有发送该目标输入；Notepad／VSCode 本批指定 EXE 缺失，BLOCKED。Edge 在 actual hit 检查1958失败，随后 exact 根关闭未确认，登记保留；未知身份不被强关，整个混合组未完成。
- 产品正常退出的真实资源 flags、post-stop 客户计数增长成立，但 Edge 清理未确认且整批 `driver_shutdown=FAIL`。这些结果与 guest 后续销毁分别记账，不把影响被隔离等同产品 cleanup 通过。后继修复及最终候选身份尚由主执行继续，不预填结果。

### bdab／50df：owned I02 子链及 T／L／四格通过，八窗因失焦中止

源码 `50dfdaa69352ad86f3b8da73c05badd3f361f73e`；原始证据：`uat/v1-control/bdab2e5e8aa648caa7bfe50124caa89d/collection-20261015T183312474Z-cf8ba2b9a13f455cac309ec73f6fbb83/`。

- Release `Full` fixture seq30–134 实际完成 owned A/B/C 普通五阶段、Ctrl A、B 原生 Resize 后 A 回环、Ctrl C、匹配 UP 及 Center／Dock 命令；随后 T／L／四格对应共享缩放和 source 切换通过子场景。它是 owned 子链，不替代正式 I02 的 Explorer／混合应用范围。
- 八窗 source7 的真实 Ctrl DOWN／g15 后，前台短暂变为未知 HWND3146652；产品按 FocusLost 撤权，本代 placement=0。驱动新的光标插入返回1，但独立 GetCursorPos 未到目标，seq512 `test_cursor_not_exact`，整批 FAIL。不操作未知窗口、不重试原八窗链，也不把它预判成产品写入错误。
- 正常 stop 与 post-stop 客户输入通过各自子项，不能关闭八窗失败。后面的应用和普通便携入口未由本批运行；不能把 e4 的独立分段结果拼接成 bdab 的 Full 通过。

### 945c／3a65：Terminal 真实正常 UP，旧 fixture 的 Console 查询分支误拒

源码 `3a65a41e7e4c4382ddde4a343b7df30d301a4857`；原始证据：`uat/v1-control/945c28c08a4a43d4b6801b15706aa33d/collection-20261015T182256411Z-b77926536cb34647a4b701dda2338002/`。

- Terminal 五阶段实际几何成立，最终 `[150,150,1050,810]`。fixture681 单次 UP 后，682 引出产品 PID5852 seq22 的同 g1 实际 LL UP，seq23 明确 `normal_up=true,pending_retired=true`。
- fixture683 整体仍 FAIL；旧 `up()` 直接按 source TID 查询 Console 的 GUI 队列，与生产精确前台 Console fallback 不一致。原683没有该查询的具体错误码，不能补造；686以生产 helper 实际确认 `foreground_console`／GUI idle。此确定性驱动差异已修，e4 以新身份重新验证 Terminal 完整本次类别范围。
- 原945c整批失败与当时未完成的 Resize／命令／客户输入保持原样；这一真实 NormalUp 不关闭其他构建中普通或异常场景的实际 UP 缺失。

### fdfe／783：GUI、五键与损坏恢复通过子流程，其余失败不隐藏

源码 `783078111ebee3d17cdaa80244180e0f4b4f155b`；原始证据：`uat/v1-control/fdfe8a79403342259401aad1b6a45b7c/collection-20261015T180117207Z-cb793400912e4c7bade308ff7387196a/`。

- IndependentEntry 的真实六页 GUI（201）、登录启动开关与实际 Run 值恢复（232）、五键客户区透传／标题映射及资源关闭（338）通过各自子流程。损坏配置前真实正常退出与客户输入恢复、重启提示、exact 备份字节、默认值和原选项恢复377–416通过，未把历史快照充作本次结果。
- 原生文件名键入361失败；实际 local Esc 取消不使导入导出通过。托盘421／425 BLOCKED。排除 native Move 的起手预检 HTCAPTION=2 与实际 `WM_NCLBUTTONDOWN` wparam=8 冲突，485失败，不能写成已进入原生 Move。
- Edge 绑定后 DOWN／START531失败；旧 catch 的 outstanding DOWN 尚未清理，安全门534拒绝继续；exact 根关闭未确认，登记保留，536／548 FAIL。后继已修一次有账本依据的测试清理及 caption 几何判断，旧失败不改写。Terminal 和普通七文件 ZIP 入口本批未到达。

### 8cee／f626：普通 C 五阶段几何通过，正常 UP 收尾失败

原始证据：`uat/v1-control/8cee44b3378f41998e8e5dcf9635f5f3/collection-20261015T163804486Z-3dda0c7311f849bbab49bbefc9f3ee8f/`。

- fixture seq69–83、产品 PID7108 seq37–43 记录普通 C 的真实 DOWN／START gen4，以及 free、XY snap、hold、detach、resnap 实际几何均 exact。hold 本来已 exact，回执明确未调用 native；其余对应实际单次调用，不能把五阶段都说成各有原生调用。
- seq84 单次测试 UP 的 SendInput 返回 1；约 3500ms 等待内无本代 actual UP／gesture_end，seq85 的 `normal_up_and_no_late_geometry` FAIL。最终产品 seq44 仅在 stop 时记录 end=8／Exit、`normal_up=false`；shutdown seq46 的 UP callback count=3、last generation=3。不得以停止终止代替正常松键。
- 松键前实际 cursor `[440,273]` 位于实际 C `[414,260,674,440]` 内，旧窗外轨迹不能解释全部 UP 失败；本批未证明 UIPI、hook 被移除或系统根因，原因 UNKNOWN。普通收尾现在阻断实时手势整链，不再仅是异常注入路线的问题。
- seq87 正常退出、资源 flags true、无未知 native 完成；seq88–91 真正发送 cursor／DOWN／UP，窗口计数 `[0,0]→[1,1]`，post-stop 投递子场景 PASS。seq92 整体 FAIL，精确 guest／任务已清理且证据未变，不由此关闭正常 UP 问题。
- 本批未到设置生命周期、托盘、五键、I11、应用矩阵或真 ZIP 入口，保留本批 NOT_RUN。后继 IndependentEntry 和 fixture 只读 Raw 观察已实施并按各自新 SHA／RunId 运行，详见上文；不改写8cee失败，不将只读 Raw 计数当作某 source／generation 的授权或真实 NormalUp。

### 7cac／0fa：真实在途超时、迟到完成与临时属性恢复，实际 UP 仍未观察

原始证据：`uat/v1-control/7cac94a0561349c3ba1fde679cd27c11/collection-20261015T162312746Z-12c84270720a4e3ab136d02478f86151/`。

- fixture seq565–577 与产品 PID6868 seq143–147 记录真实 OpacityDown `205→173`，随后实际 DOWN／START gen16 与临时 alpha `173→190`。不是诊断许可代替实际修改。
- 自建 source 在 `WM_WINDOWPOSCHANGING` 中有界暂停 1000ms；call44 的实际 `native_before_qpc=519183982`、`native_after_qpc=529210003` 及迟到 exact 回读证实原生调用真的在途约 1 秒。250ms writer 等待预算耗尽后，产品 seq148 记 completion unknown，seq150 以 SourceLost／end=3 退休 pending，不能把超时说成调用未发出或已取消。
- 单次测试 UP（fixture seq580／581，SendInput 返回 1）在 source 仍阻塞时发出；产品未观察到本代 hook UP，shutdown seq156 最后 callback 仍为 gen15。seq583 的完整终止检查 FAIL；尽管产品已有 SourceLost 退休记录，也不能补成 NormalUp，缺失 UP 的原因 UNKNOWN。
- seq151 记录 call44 迟到 exact、`new_write_authority_granted=false`；seq152／153 通过实际 alpha API 将 `190→173` 并取得 helper commit ACK、退休临时持有状态；seq155 正常退出前又恢复原 tuple `173→205`。这些具体恢复事实成立，但不是整个异常链通过。
- fixture seq579 的 `native_entry_qpc=0`、flags／positioning 零值是日志 payload 实参与 `wait(...)` 实参求值顺序造成的确定性证据缺陷，不能推断 native 未进入；原日志不改。后继 `f626b6ee8c934da08497172898f6d93d155d5964` 已修并通过双配置构建／各10项 offline，8cee 实际运行但更早在普通 C 收尾失败、未到该在途场景，故这项定向复验仍 NOT_RUN。
- seq585 正常退出，产品 seq156 资源字段全 true、无未知在途完成；seq586–589 真实发送 cursor／DOWN／UP 并观察窗口计数 `[1,1]→[2,2]`，post-stop 投递子场景 PASS。seq590 整体 FAIL。精确 guest／任务已清理、证据保持不变，不以销毁 guest 代替清理通过。
- 设置生命周期、托盘、五键、I11、应用矩阵及 I12 均未到达，NOT_RUN。后续仅调整测试顺序让未受阻功能先运行，不无改动重复同一缺 UP 路线，不将后续未运行包写成通过。

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
- Catalog 使用确定的实际矩形遮挡判定；吸附边线与 Ctrl 共享边从实际回执／几何绘制。e4 的 owned 视觉组件有实际结果，但未外推所有 DPI／应用或真人观感。
- Edge driver 消除 DOWN 前重复 bounds 查询；Terminal 精确根映像适配为实际 cmd，Console GUI 查询也与生产严格 fallback 对齐。Terminal 已由 e4 重新运行本次类别；Edge 仍有实际命中及收尾失败，不能共同写成通过。
- 已接原生 grid17 拒绝与后续 Move／Ctrl 恢复入口；8453 已真实观察 non-exact 后拒绝，但当时后续联合断言失败、恢复未通过。c01 后继 Move／Ctrl 恢复子项已通过，原 g6 实际 UP 仍缺失，完整 I10 未通过。grid 由自有应用在 `WM_WINDOWPOSCHANGING` 实施，不伪造产品已知 grid。
- Code 去掉可能覆盖显式命令行的 bootstrap data 设置，仅作为可区分假设；尚不能据此宣布根因或修复成功。

原始 DOWN、单 writer、撤权后无新调用、在途调用与完成未知分开、actual postverify 及精确视觉所有权合同继续保留。构建、L1/core 测试与局部 owned 结果都不能替代当前 Release 的整功能／模块验收。

ff 以来的七文件便携 ZIP、精确源身份／哈希、普通无参入口及 I10 在途失败用例均已接线；测试 driver 账本、实际低级鼠标回调和 GetAsyncKeyState 分开记录。后继实际结果见各批，不再用“未到达全部后续功能”概括当前状态。M12 的便携组件、固定表／校验器／证据绑定、中文可执行指南／集中缺陷／review入口均已落地；指南的实际命令、六页设置及托盘名称已与源码核对，**60/60 IMPLEMENTED仅表示已实施，不代表实现正确、说明完整现场走查或DEV通过**。完整功能 DEV 仍0/60、对标仍0/40、整链仍未通过，S1不因实施数关闭。

## 集中缺口与待核验项

### 已修或已有接线，等待对应现场闭环

| 项目 | 证据与现状 | 下一项明确验证 |
| --- | --- | --- |
| 设置与生命周期 | 269b／88e 旧命中失败保留；e4 实际六页、登录启动、导入导出、损坏恢复、排除及原配置恢复通过对应子流程 | 完整 M11／I07、所有设置项与高 DPI 走查仍未通过，不重开已关闭 checkbox 专项 |
| 布局 START／click 候选 | aae 原失败保留；cb807／8453 已完成 T→L、四格，8453 八窗末成员联动通过子场景 | 继续当前后续功能，不重开同一 T 专项；完整拓扑／应用／环境不能由局部结果替代 |
| Edge／Terminal adapter | 945c Terminal 实际 NormalUp 后旧 fixture 查询误拒；e4 对齐严格 Console fallback 后该类别实际 Move／Resize／命令／Esc／客户输入通过 | Terminal 不外推其他类别；Edge 命中与 exact 根清理仍有失败，保留登记且不控制身份未知窗 |
| grid 与成员失败 | c01真实grid非exact拒绝与后继新Move／Ctrl成立，但原g6UP缺失；第二成员API TRUE后identityloss／停止第三及存活两窗恢复子流程通过 | grid原失败保留，整个I10不升PASS；不把身份丢失叫API FALSE，也不为此增加新的矩阵门槛 |
| 视觉／恢复与边线 | 7cac call44 迟到 exact／alpha 恢复事实保留；e4 owned 视觉组件实际通过，但临时在途组件仍 FAIL | 不能把正常视觉或 helper 恢复推成缺 UP、故障／硬崩溃整链通过 |
| post-stop 实际输入恢复 | 8cee 真实计数0→1、7cac／1bee／eae 真实计数1→2通过；d49 未执行到发送，不能判投递失败 | post-stop 投递不能关闭普通手势本代 UP 缺失，也不能推成整版 cleanup 验收 |
| 普通 Move 正常收尾 | c01 I11 g26几何正确却无实际LL UP/end；独立Raw97→98仍不能归属。旧8cee／e4等失败保留，其他NormalUp成功不补缺失 | S1 OPEN；没有可靠共享事件身份，不加未证明Raw fallback，也不以无新事实重复运行同路线 |
| aborted／mapped 配对与准备 | 85c／b092 已修旧配对、authority=0 新配对与 hover 准备，2c 已接 mapped 精确 generation CAS | 历史实际 UP 缺失原因 UNKNOWN；1bee 未发本次 DOWN，不作为配对现场复验 PASS |
| 托盘与五键入口 | fdfe／e4 五键实际客户区和映射检查通过子流程；e4 托盘 UID1 图标不可达，BLOCKED | 五键不等于整个 M09 或托盘菜单通过；仍需真实托盘入口 |
| 在途日志与实际异常 | 7cac payload 求值顺序缺陷已修；e4 已到真实临时在途，保留1694失败及后续安全分流 | 不再记成完全未运行，也不凭迟到完成／已恢复属性关闭实际 UP 或全部 I10 失败 |
| GUI诊断与driver临时文件 | 590误纳`.tmp`的确定driver问题已由dd60 Debug97／Release714c真实final导出及parser专项复验关闭 | 设置底部“诊断”入口已实测，不因此授托盘／普遍隐私／完整M12通过 |

### 仍缺的实际环境或尚未闭合能力

| 项目 | 准确边界 |
| --- | --- |
| 正式 Medium Explorer | 本批根窗 elevated=true，不满足正式资格；本地目录身份已恢复，不再误报为 file ID 未实现。不得套用旧 High guest 例外。 |
| Code | 历史隐藏根／子根拒绝尚未定因；e4 本包未带指定官方 EXE，明确 BLOCKED，不能沿用旧资产准备为本批已运行。 |
| Notepad | 当前 guest 的指定系统 EXE／Appx 缺失；未运行的应用路径不能记 PASS，不在宿主安装或修改账户／UAC来补分。 |
| E03／E04 | 当前没有真实双屏／负坐标与混合 DPI 环境证据，属于具体环境缺项；纯模拟坐标不代替。 |
| 200% UI、虚拟桌面、普通用户真机 | 尚未执行，分别保留 `NOT_RUN`；当前截图或 guest 局部结果不外推。 |
| I08／I10／I11 | c01 actor真实默认Deadline及后续同PIDMove／Resize、第二成员失败后恢复、status实际响应已通过对应子项；sourceclose／grid匹配UP、I11正常UP、完整两轮／15分钟仍未闭环。独立idle60不替代I11，历史失败保留。 |
| 顺序配置输入差异 | 97 Debug专项成功后，同guest的Release实际客户激活失败，未到GUI/parser/ZIP；原因UNKNOWN。714c独立Release-only成功不证明根因，也不抹去原失败；不操作未知前台或追加无依据API。 |
| 本阶段便携交付 | 已测e2abc／dd60在714c完成普通无参可见GUI、真实保存前后两Move／NormalUp及退出／字节检查子流程，原ZIP字节不变；最新版指南独立附交付。b5a15／b60、458849／593及所有旧包保留，整版验收与完整说明走查未通过，不重绑证据。 |

## 持续验证与交付边界

验收全集见 [V1_ACCEPTANCE.json](V1_ACCEPTANCE.json)，产品操作见[中文快速入门](../V1_QUICK_START.md)。当前仍为 **0/40** 对标整体通过；B01–B32 全部必过，B33–B40 固定延后但保留在分母中。整版 DEV 门槛尚未满足，真人 UAT 为 `PENDING`。文档、机器表与后续证据更新应继续区分源码接通、局部实际结果和完整门槛，不能用测试数或旧 High 局部证据凑分。

本稿集中交付已测原ZIP、独立最新版中文指南、机器表与历史证据索引，供整体review；它是PARTIAL阶段交付，不代表满足B01–B32或整版完成。缺可靠共享事件身份时不增加RawNormalUp，不无新证据重复同一缺UP路线。最终文档commit／push及远端SHA由主执行者实际核验后交接，本文不预填自引用FinalSHA。原失败与历史包保留，`uat/`、EXE、ZIP 不入Git；不创建PR、merge、tag或公开release。

现场自动输入仍仅在本任务专属可丢弃 guest；远端文件仅在 `I:\PaneBindMVP1Runs\` 本任务目录，不触碰 CrossRec、其他用户数据、账户、token、UAC 或宿主设置。普通宿主试用和真人手感不能由自动 DEV 自签通过。
