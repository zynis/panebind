# PaneBind V1 集中交付进展

当前用户功能接线：正式 Medium GUI／托盘入口已接通普通窗口资格、Move／Resize 磁吸、Ctrl 邻接组移动／共享边缩放、停靠／拉伸、窗口命令、中文六页设置与应用规则；离散布局恢复、最大化起手恢复、独立 native writer、视觉属性恢复及实际吸附／共享边反馈均为生产调用链，不只是 helper 或测试模型。

当前整版开发仍为 **PARTIAL**，不是 COMPLETE，也不以此主动收口。最新现场仍为同R4=`af0aa67ba3b4cb62d3ac2bece2c31533c8ba4ff6`／39443，Debug七代owned Move／Ctrl／Resize及postResize回环的几何、planned真实UP与资源清理已闭环；紧接的三包快速UP几何仍FAIL，Release实际前台未取得而未发手势，两个Full仍FAIL，不授完整I02或功能PASS。历史R3=`37e5ffce1290f3c8e07056083e42666859eebe0d`／866及旧R2的**7/60功能、B29／B32共2/40=5%**保持原实测身份，不换绑R4。D23只有下述Debug定向无迟到发起范围，Release及整链待验；D08开放。整版Release／Medium连续链、应用／托盘／显示矩阵未齐，不能宿主试用，真人UAT PENDING。

## R4：Debug owned七代闭环，快速UP与Release仍未通过

候选源码`af0aa67ba3b4cb62d3ac2bece2c31533c8ba4ff6`已将InputService→GestureInputShield→共用writer实际接通；新路线保留精确guest／RunId／包与二进制身份保护，不在普通宿主启用。D23现为`DEV_FIXED_PENDING_TARGETED_RUNTIME`：匹配UP后不再允许新的continuous placement，已发出调用的完成和明确的release DockCommit保持独立语义；这不证明D08已修复。

专用目录`out/v1-captured-input-r4`实际Debug／Release构建均退出0。Debug的10项V1 offline通过，实际wall 2.87秒；Release的10项V1加explorer-mvp-flow共11项通过，实际wall 6.26秒、offline processor 2.12秒，二者不混用。Release产品SHA-256 `3CDDFF7C70FB6CE0A7DB141A3631B74F665D464ABDF910E7731D6FC44E79C8AF`，Debug产品`EBDD02BE05405D1BD17E429349F5377122FA945D7F4667F5C383801344615F7B`；driver Release `29083D75012005D27EFF423FDEAAEBF7C1AF04937E9DA2F50A181D6090E2F62B`／Debug `776152AD37E6365298E4808891D8911E720AE9C392D131260F1C545B31D14B03`。上述为主执行者实际工具结果转录，不是仓库归档原stdout，不替代现场结果。新7a0便携包已生成但普通入口尚未运行；首批e1fc的Debug／Release Full均被驱动缺口阻断如下，不授完整功能PASS。Explorer／整版连续链／普通Medium完整新路线／真人结果仍NOT_RUN，旧R3／R2证据及7项独立合同不改绑此EXE。

最新Run `39443acd23034e8fb7bd4eb8d1b2071a`复用原af0产品EXE，只换driver `b1dc41ba364de2282c5fc232bde34ae39d63d2ad`：Debug `3791D6EDB96A72098E4BAFA9C94BF66942D9FED9B31B7D8594DB8AA32CD35D03`／Release `DFB4D6CDF6FE8187FFB21B3534ABDDFB4A52FB4A678479587EB02497C2345671`；主ZIP `899366858A41383331CF1CF7D78BE305CCD293A97C864385729B23D9FBBEBC14`／manifest `3B3E1FC61953D6162F6F5EBD534FE0375EF65EA7A60349AC7C6CDE8C23647FD9`，packaging HEAD d74482a。原四日志hash与collection核对一致且JSON合法、sequence连续。

- [Debug owned原字段摘录](v1-evidence/39443-r4-debug-owned-core-selected.jsonl)：g1–7依次A五阶段、Ctrl A三成员、B／C五阶段、真实B Resize、postResize A五阶段、Ctrl C三成员。48／64／85／106／119／141／157每次planned单UP、实际几何和无capture均通过，162仅是owned core scope，不含真实Explorer／Release，不是完整I02。
- [产品实际资源与writer](v1-evidence/39443-r4-debug-input-resource-writer-selected.jsonl)：各代actual captureReady→accepted legacyUP→真实release／detach／NormalUp Gone→明确queued legacyUP→NormalEnd；23次该范围native发起QPC均早于各代真实passive-head UP及Raw／legacy观察。没有观察到UP后新增continuous；generation0的后继Center／DockLeft是独立命令，不误当旧手势迟到。按各producer实际QPC比较，不以JSONL行序推出跨线程原生全序。这证明Debug旧回环改善，**D08及完整D23验收仍不关闭**。
- 紧接168一次插入DOWN／MOVE／UP三包，实际head UP及产品UP撤权已到；产品g8在PreLive／PhysicalButtons拒绝，capture／placement均0，未产生NormalUp手势。169预期最终几何未实现而FAIL保留，不能删除该快速操作要求或用安全拒绝冒充移动功能PASS。171正常退出、176实际客户`[0,1]→[1,2]`另有事实，不以guest销毁补PASS。
- [Release前置失败](v1-evidence/39443-r4-release-foreground-failure-selected.jsonl)：28在一次SetForegroundWindow及有界实际等待后`actual_foreground_matches=false`，无DOWN／START／placement；31后继客户验证也未发送输入，不能声称客户恢复PASS。这不同于e1fc旧API短路问题。Release／跨应用仍须真实后续验证，Full仍FAIL。
- b1dc的Debug／Release driver实际构建退出0，guest-path定向测试各1/1，0.15／0.17秒。后继driver `edf4bc45054eec209e9588a3ab9a3e464d2df41d`现也实际D/R构建退出0、guest-path各1/1，0.17／0.20秒；两脚本AST为0错误。Debug driver `B0A167704F953E62414928F2BE3868178BC2644B2D277A1B048CEE2B8A072380`／Release `8609BD877DCB063DE2F3F25A4EA560AAF4B841EC13BF5BB4DC1E6A5AC1B2CD7E`，实际build-identity均edf，af0产品哈希不变。此处均为主执行者工具输出转录、非归档原stdout或现场；edf尚无新Run结果，不加功能／对标分数。

Run `e1fc79d4e1ca4c63b0572da1b4ef9150`，产品及driver均af0，打包HEAD`8f63bf46d2d1b832ce67a9af215af94985df8f28`；主ZIP `D9F9B3274F0B47EEFA4D83CA7109585B856B2D6DE59F04E3386A8CF2B68F6670`／manifest `52EC5A7B3AE2DDCF3CBFEFB5B50E5E316D9EBE86B1F868249F2C76DBDF8C64BE`，7a0便携ZIP `236FDF61F8529296639286158E28DD890E76EDA429E28A317396BF0E6F4152A5`／manifest `78ED29E69FA4B74E178096555257783B8AED8DB26F1354B2443B3A1E913CF486`。四份原日志均已独立核hash、JSON合法及sequence连续。

- [Debug driver](v1-evidence/e1fc-r4-debug-driver-selected.jsonl)34／36／38／40／42真实free、XY snap、hold、detach、resnap exact；产品4次native成功，hold仅already-exact无调用。43装passive hook后44明确`single_test_UP_attempted=false`，45因旧driver仍要求命中source而不是实际shield抛错，计划Normalcase未完成。
- 异常46仅一次测试清理UP后，[产品原回执](v1-evidence/e1fc-r4-debug-product-selected.jsonl)23实际RawUP、25accepted legacyUP、28实际capture释放、30解除关联、31 NormalUp Gone、32明确`shield_queued_legacy`、33正常结束；35资源退出。不能把异常路径的真实闭环改成planned场景PASS，亦不据此关闭D08或授D23定向验收。52后续实际客户计数`[0,0]→[1,1]`与guest销毁不同。
- [Release driver](v1-evidence/e1fc-r4-release-driver-selected.jsonl)28仅SetForegroundWindow返回false就短路，未等实际前台即29失败；产品无DOWN／START／placement。34后续同source实际foreground=true、客户计数`[0,0]→[1,1]`。只支持修正测试观察，不推测产品权限或系统根因。两个Full均FAIL；下一Run只改driver，继续复用af0原产品，不重烘EXE或改绑旧证据。

## 现有验收表的推进状态与仓库内证据

继续整版开发时，已将60个功能、12个模块、B01–B32、I01–I12、环境和应用行的既有实测逐项回填，不再让已经失败／受阻的主状态全停在 `NOT_RUN`。当前验收功能为7项 `PASS`、12项 `FAIL`、41项 `BLOCKED`；`FAIL`表示原身份绑定的实际反例尚未关闭，`BLOCKED`表示该行写明的完整合同／环境／当前候选复验条件尚缺，而不是声称所有子项未运行。原R408与R2/0c710独立B32 PASS保留历史，新R3/a00b/866另有真实新证据，不换绑旧EXE／ZIP／Run。B29只因其完整M01-F05／M11-F05合同实证齐备加分，不能外推B27／B28／B31；B33–B40继续固定DEFERRED／0分。

每行有 `observed`、`remaining_conditions`、`next_action`，以及 `partial_observation_ids`→原观察的产品SHA／二进制哈希／RunId／证据关联；`candidate_identity_ref`仅指待验当前候选，不把旧观察改绑新EXE。模块／对标条件直接引用功能行，未另建一套任务书。后续产品修复或现场新结果按实际身份增量登记；驱动／文档变更不得清空既有产品证据。

[仓库内证据索引](v1-evidence/index.json)和[摘录说明](v1-evidence/README.md)保留152条关键原始观察的脱敏摘录，含dd60／714c真实普通ZIP与诊断、b60／c01组件通过及缺UP／恢复／应用失败、50df／bdab owned回环及T/L/四格、dd60／97顺序Release客户激活失败。已独立核对四份原日志整文件哈希及对应16个原EXE哈希；每条摘录另带原行UTF-8 digest。原始大文件和包仍在索引所列ignored本地`uat/`位置，无需为源码复核反复上传整个目录。

本节只是既有结果映射，不是重新运行现场或结束整版任务。实际缺UP、快速拖动消费时序风险、托盘、官方应用准备和显示环境分别推进；新的确定性反例与修复不被误写成既有LL-UP缺失根因。完整功能合同未齐时不计PASS，不以PARTIAL作为主动停止条件。

M12-F04的独立机器证据合同现按实际校验通过：[工具输出转录](v1-evidence/acceptance-metadata-contract-transcript.txt)明确非原stdout或现场；固定ID唯一、PASS绑定／真实分数、5项反例、当时29原件与摘录hash、当时当前包hash均实际核实。它证明元数据能力，不授其他产品功能PASS；完整整版链不是此项额外门槛。随后f9已实际运行M11-F03真实冲突／命令保护／此页默认与动作帮助，按其独立合同通过，不覆盖其他设置或整版门槛。

## 新确定的输入消费时序缺陷：代码修复与实际定向复验

用户提出的 `InputService` 真实UP先清active、GUI消费迟到MOVE仍要求active的风险，已用实际共用生产判定最小提取复现：旧版定向Debug为226 checks／1 failure。这是已确定的独立时序缺陷，不是此前实际LL UP缺失的根因证明。

产品修复源码为 `408ac527a6bfda7b01a5e83249bd5ed0eca2714d`：排队旧MOVE仍拒绝；只有实际匹配NormalUp的UP点可以提交一次独立terminal计划，按原DOWN／窗口锚点求绝对几何，native发起前仍复核权限。后继DOWN、停止、abort或真实上下文失效不能恢复旧代写权限；旧代finish只退休自身，不能撤销较新代。修复后同一反例及相关代际／一次性／原锚点定向Debug为239 checks／0 failures。

[before／after小型转录](v1-evidence/input-up-dispatch-directed.txt)明确是`exec_command`输出转录，**不是程序原stdout、真实Windows输入投递或guest功能证据**；原转录与脱敏文件哈希分别入索引。随后8189取得下述真实快速UP点terminal定向结果，V1-D15已按该精确范围关闭，不再扩展专项。原D08缺LL-UP S1及完整连续链门槛保持未关闭，两者不是同一根因；dd60旧已测包也不换绑为新R。

新R产品在专用`out/v1-product-408ac52`完成Debug／Release构建及各10项offline实际PASS（4.14s／3.49s）。Release EXE为836608字节、SHA-256 `48F96C2A46A259979D484E0585C471C3FC8F9880EA1239B99C7A83D89CE3924E`；Debug为3287552字节、`BC75138C4E52B215ABA3BAD65643DDF192D0A1B295309C2F280BA9D131A39345`，独立只读哈希与两份`--build-identity`均核实R。另一个driver／脚本提交D=`83bf90362faafdfd431669b2ca545364d3898bcd`只构建测试工具，不重烘R产品。详见[构建与offline转录](v1-evidence/408ac52-build-offline-transcript.txt)；CTest当时未tee，转录不冒充原stdout或现场，当前`LastTest.log`只代表后执行的Debug。新Run8189的真实范围另列如下，不能升级正式DEV或关闭D08。

## f9：真实映射冲突／实际命令／页默认通过；a623首场未到capture

Run `f9b6012a43e345648d52c067ff9594f8`，同R2／458A62、原98fc便携ZIP不变；driver／packaging HEAD=`9a77cd755b08edb5e0a38be567e437d664959be4`，Release driver `EB3C2F76F680ADCF5F838740C3BF7BC5ECC82D53C62266B0E4FDB950B4AF6F97`。主ZIP `49EB7A5E14E7A669678ED707D6A90B423D7BCB1E75F2F78EA91F1C6F30107CD6`／manifest `56602FF600FBF10594874576B424887EF277DA3EEAD8E5CE6FD5E6D2143868C7`。见[实际fixture](v1-evidence/f9-mapping-fixture-selected.jsonl)及[独立产品回执](v1-evidence/f9-mapping-product-selected.jsonl)，不把driver变化重绑成新产品EXE。

- **M11-F03独立PASS**：442测试线程仅成功注册自己的Ctrl+Alt+F10；448实际只读动作legend与Center／MoveRight一致。461真实产品中文冲突notice，原bytes、draft及generation保持；463原Ctrl+Alt+C实际Center exact `[1285,638,1765,958]`。464仅注销成功创建的registration，真实API返回true；469同失败draft仍在，472保存后474新F10实际MoveRight exact `[1300,638,1780,958]`。483实际点击此页默认、鼠标／键盘片段和步长／help一致且未保存前旧配置不动，486保存恢复原选项；489默认Center再次actual exact，490精确测试窗实际销毁。产品PID6272 seq64冲突notice、65／69／74真实native调用7／8／9及66／70／75真实command记录相互对应；不是伪造WM_COMMAND或调用handler。437五键及客户透传、配置／视觉恢复另有对应范围通过。仅授本映射编辑／冲突／页默认／动作帮助合同，不授M09全部动作、托盘帮助、整个M11或B28。
- startup329、import/export588（含新映射检查）、saveFailure696、corrupt749、exclusion845和portable919均有实际阶段范围结果；仅tray755／761 BLOCKED导致932／934整体FAIL，Core I11／应用矩阵／Full NOT_RUN。912正常产品退出、916客户`[7,7]→[8,8]`，927另一个run-copy退出及931客户`[8,8]→[9,9]`，与精确guest／task已清理／证据未变分开记账。
- **a623是另一个新输入平台探针，不是当前R2产品**：[原始小摘录](v1-evidence/a623-consumed-down-probe-selected.jsonl)，Run `a623eafefe2a470eb5877f876da37077`，源码9a77、Debug探针 `F7C81FDB88C16CF6BF8AC2DECAFC51B191C9F2C517DFF4F00A482EF2537AC406`；原日志SHA `DBC8F42DBD5BD797152F93406EAB7F389FA7059F6E7F446567C97BE22B106115`。seq15真实exact LL DOWN已consume，随后final_guarded_cleanup；seq24 capture_attempts=0、native_attempts=0、writer_started=false、completed_gestures=0，V1 InputService／MoveSession／writer均未arm。清理单次UP后seq19真实LL UP但未推断legacy投递，不能记新机制NormalUp。scenario_pass=false；未运行capture建立／丢失／独立退出风险或Explorer，不给PASS。当前具体拒绝项尚未细分，不推测系统、权限或根因，也不凭首场就宣布整机制不可能；旧D08不关闭。
- 用户已确认合法Windows ISO／VM，但RAM不足不能新建VM；不是无介质或未回复，不调整宿主资源／其他任务。所有输入仍限专用guest，未来新Run结果不预填。

## 866：新R3真实可见错误与完整导出合同通过，整批仍非Full

产品／driver源码均为`37e5ffce1290f3c8e07056083e42666859eebe0d`。Run `866d2342e2994233a48c8a97106d660f`，Release产品 `658C12B9550F6F699E2A349F36E6560A897530FEA81DF598DA1E3267244CD067`／driver `0D7F1659DF312B94A9CE91F36A3BB27C37DF9D6FDC5486ED1FB3B8E9C81C522F`；Debug产品 `59D90E1B266ADCEF0C1EE2521FCC6BE6922FDD8866B2EAE6012DCBF7D705D9F7`，本批未执行Debug现场。主ZIP `862BA10029FBACC8540D90ED4ECFBFE2DE816100922559120B29748075CFBC4A`／manifest `934D5DCE05D82116974C7EEEA896ADF7FB3BDDAF667DFC17C5B44B2FD1BC0788`，打包HEAD5a8e746。原R2 current package和98fc整对象已转historical，旧日志／包不改标签。

- **M12-F02独立PASS／D22定向关闭**：[fixture](v1-evidence/866-r3-entry-diagnostic-portable-selected.jsonl)719在真实本地同SID DACL拒5后观察到自有中文错误modal，实际Static文本查询2次；720真实Enter确认。0假包／配置未变；722／723恢复精确原DACL，727／728恢复后实际导出并解析／脱敏，739原实例／配置／证据恢复，740／745通过。[产品PID5488原回执](v1-evidence/866-r3-local-export-failure-product-selected.jsonl)seq6返回1另有独立GUI观察，不以API返回冒充可见结果；seq7恢复后新导出，seq9正常退出。
- [真实故障包原件](v1-evidence/866-r3-actual-fault-diagnostic.txt)1628字节、SHA-256 `AE1201EA6DA9EAD071A9B764D6F07291C4BC494A3572BCD6685A6C17552414C2`与原文件相同，含R3版本、OS26100.9711、DPI144、实际fixture类别、脱敏设置、真实配置保存错误及终止原因。不是seed错误文本或普遍隐私证明。86a旧失败原样保留；本项合同无需无关托盘／整版通过作为额外门槛，也不自签独立review。
- 新便携a00b ZIP `30B2C32DB7C54E931F77600B82247A0A90FF20730EBF6F74B8D4DC87EDB19719`／manifest `719F58EE9B77753DC867986C541E155123CBA40627A536B5C2A6C9242AADB03C`：900核七文件；907／909真实空参中文GUI，911／912同实例二启；927／932默认效果和实际UP，945保存后955／960实际效果／UP，961／965／966正常退出及客户恢复，967原字节／无profile，968仅便携子链。旧工具负例继承未改工具的原身份，不冒充本批重跑。
- R3相应startup329、五键437、映射／页默认588、保存故障696、损坏恢复798、排除894均实际子流程通过；既有六项完整合同的原生记录保持原SHA，R3 UI-only变更影响及新实测单列。**tray810 BLOCKED使981／983整体FAIL**；Core I11／应用／Full NOT_RUN，没有真实完整V1输入Release闭环。
- **D23源码确认开放**：R408 terminal方案仍可能在匹配UP后发起新continuous native，而非已发出调用的在途完成，违反guide5.2；旧251 scoped PASS不覆盖该缺口。M08明确的release DockCommit例外不放行continuous terminal。后继修复／候选尚待实际验证，不能宣称当前R3可试用。
- R3 Debug／Release已构建、各10offline实际通过；本快照仅取得Debug4.83秒，Release时间未知，不猜测。构建不是上述现场证据。精确guest／任务已清理，证据未变，产品退出／DACL恢复不以guest销毁替代。

## f481：owned资源四场景通过，不是Move或Explorer通过

另一个资源探针Run `f4814a3b2fc24894975adbce91a5721f`，源码同37e5但**不是V1产品EXE**，Debug探针 `4A9BB4205FFD6C54221C62D04C03E2318920AEA0D8FA4A508DB6DE9FF2CCF203`；原JSONL分别87／36／49／62行，均合法连续，整文件／原行digest见索引。

- [normal-repeat](v1-evidence/f481-consumed-down-normal-repeat-selected.jsonl)：两代实际legacy UP、自有capture释放、关联解除和NormalUp撤罩，44／83通过。
- [early-UP](v1-evidence/f481-consumed-down-early-up-selected.jsonl)：真实LL UP先于资源请求；30实际PreLive／PhysicalButtons拒绝，ATI／capture均0，不以queued=true授权限；32真实exact source清理UP及队列／几何不变通过，明确非NormalUp。
- [F11停止](v1-evidence/f481-consumed-down-stop-selected.jsonl)：capture就绪后25实际发送Ctrl+Shift+F11，27实际WM_HOTKEY，未预先发送或观察鼠标UP即释放／解除／ExplicitStop撤罩；随后仅一次测试清理UP由exact source实收，37／45通过。
- [source停泵](v1-evidence/f481-consumed-down-source-pause-selected.jsonl)：自有source实际停泵5000毫秒；真实3000毫秒Deadline／Gone tick33687，早于实际resume35703，实际ready→gone2984毫秒。50／58验证独立收尾；明确不是默认30秒实测，也不是writer阻塞试验。
- 四场景均`writer_started=false/native_attempts=0/Move_function_PASS=false`，没有原生START／END或cancel、没有V1 InputService／MoveSession。LL／Raw观察不补NormalUp；物理按住不从driver账本或async左键0推断。只关闭这些新资源风险范围，旧owned九项保持原scope；Explorer／真实writer及其阻塞／新路线Release／真人仍NOT_RUN。精确guest／任务已清理，原始证据不变，销毁guest不代替产品cleanup。

## 历史86a：真实导出失败无可见反馈，后继866已定向修复

Run `86a42fd21f544a0dba0db056be424946`，实际Release／Medium／EntryAndSettings；仍为原R2／458A62，driver源码`4cd9f269ebf619d11860c506d5c52f15bb37bce8`／Release `75F221E5AD111455EB967D994388D72834321F854F3C0CADA3B7391D36BCB3FA`，打包HEAD27e47d2。主ZIP `6F63604221423DA664877C66D82CADD71CAF541788852D3AA5D182017DF2A08E`／manifest `8B20DFEC5FC9E170941DB24B45EDC68F3F8FAECC2D4A4C21EEF74DEBBB2C2FA1`。关键[fixture摘录](v1-evidence/86a-diagnostic-export-failure-fixture-selected.jsonl)及[本地产品原回执](v1-evidence/86a-diagnostic-export-failure-product-selected.jsonl)分别绑定原日志SHA `2061EFE008127154AF852F11907A1A4CF2B58DC6E805591DFD45E9E2420D8040`／`FC84A77DEFBA515D2FD4B3AD951A5DD86FC2D7A34D8CDC54D1CE1B82F54127FD`。

- **M12-F02真实FAIL／V1-D22**：701核原EXE哈希，710／711在精确新guest-local目录空参启动同R2；713精确设置PID7064。714双方实际SID相同，只给该本地目录增加不继承的FILE_ADD_FILE deny，未改映射Output；715实际CreateFile拒绝5。真实诊断按钮点击后719仅有产品seq5错误notice，有界1秒核13个自有线程窗无错误modal；0个diagnostic final/tmp、配置字节不变，720无假包成立。日志错误不是用户可见报错，不把本负例写成环境阻断或继续NOT_RUN。
- 721／722按同一目录身份恢复精确原DACL字节；726／727恢复后真实再次点击，产生新包并完成字段／配置解析及实际标题／路径脱敏专项。产品seq6成功notice、seq8正常退出，733实际客户计数`[6,6]→[7,7]`；738原DACL／证据归档／原实例与配置恢复均true后，739／740／746仍如实保留唯一可见反馈断言FAIL。原损坏配置799、排除895继续有实际阶段PASS，tray811 BLOCKED，982／984整批FAIL，不拼Full。
- 最小产品修复`37e5ffce1290f3c8e07056083e42666859eebe0d`仅在明确导出失败时显示自有中文MessageBox、保留原notice／无假成功；不改变输入／几何或目标窗权限。**当前仅源码修复，构建身份与实际可见反馈尚待新候选复验**；旧R2的6项独立PASS及旧包／Run不换绑新EXE，不授M12-F02或整版PASS。
- 首份部分收集因本地白名单未含精确新增诊断子目录中止，原部分副本保留；后继仅补与远端一致的精确范围后取得完整收集，不把这次工具问题写成产品／权限失败。精确guest／任务清理和证据哈希不变另有回执；guest销毁不替代上述产品退出或DACL恢复。

## d546：新候选owned资源两代正常抬键闭环，不是V1 Move通过

Run `d546969fd0f649fcb878db5079e05184`，源码`4cd9f269ebf619d11860c506d5c52f15bb37bce8`，Debug owned探针EXE `A2058B6E19570FF0CA9DD7D87B3CD9E17996752F20D09C95F2149727BD414546`，不是冻结R2产品。原JSONL为86行、sequence连续且全部合法，62493字节／SHA-256 `5AAD6BF20595E215AE06181375B1EE77486687E437FB572F6DACC2A1749F093E`；[小型关键摘录](v1-evidence/d546-consumed-down-resource-normal-repeat-selected.jsonl)对重复capture／release／detach仅保留必要字段，值不改，原行digest绑定完整原件。

- 两代`28810927835600/601`均有真实exact消费DOWN（15／54）。16／55实际pair授权成立，async左键0只作诊断；21／60记录AttachThreadInput真实成功、自有SetCapture及回读各本代overlay（66166／66194），前台仍source197216。**该候选没有原生START／END或cancel，不能写成已验证native END交接**。
- 33／71实际LL UP观察true并透传；35／73同代实际legacy LEFTUP由资源接受，非PostMessage、非重发UP、非Raw计数代填。39／77由所属资源线程释放本代capture、API成功且回读0；41／79本代实际关联解除成功，42／80 reason=NormalUp、真实撤罩／hotkey注销。44／82资源判定接受；45只在旧资源清理后开始下一代，86正常完成两代并销毁receiver／Raw registration／自有UI。每个producer原时间事实独立解释，不以JSONL排序建立跨线程全序。
- **只授owned资源normal＋repeat范围PASS**。两代均无source placement、native START／END为0，V1 InputService／MoveSession／writer未启动；这不是普通Move磁吸、原D08修复、Explorer跨进程、三窗或Release通过。捕获后实际热键、短Deadline、source线程有界停泵独立退出及其他风险场景仍NOT_RUN；实现中或准备中也不预填。
- 精确guest／任务已清理、证据前后不变、部署保留且未改其他资源；其销毁与已观察到的资源正常退出分开。当前6/60功能、2/40对标不变。下一步先运行新增关联机制的必要退出检查，满足后才继续共用writer与Explorer功能接线。
- [4cd构建／offline小转录](v1-evidence/4cd-consumed-down-build-offline-transcript.txt)：真实Debug／Release构建退出0，各4项受影响offline通过，1.37s／2.07s；明确是exec session24215工具输出转录，不是归档原stdout或现场证据。没有为生成转录重跑。

## f63：明确拒绝在旧异步左键门；修订未预填现场结果

Run `f63d6bc443534f5c94ca637a60b412ae`，另一个Debug平台探针源码`6685a08d8ff3ade1f6714603eee1edf5beb4f619`／EXE `8CE9E00706CB55D01F4A947EF37F7498297E4BEAE9BAF1D1A2C5F33DAD705656`，不是R2产品。见[关键原始摘录](v1-evidence/f63-consumed-down-pre-capture-selected.jsonl)，5410字节原日志SHA-256 `E50916B144E1FF8061329A6E9B1952D466F51E620566D63D37F6D32DC7BF4CD4`。

- seq15真实exact LL DOWN已消费；seq16 receipt／published、同代／单DOWN、未UP／未退休及全部上下文成立，而`GetAsyncKeyState_LBUTTON_return=0`／`left_high=false`。结合该源码入场判定，此次唯一拒绝项是消费DOWN候选仍要求旧物理异步左键high；不证明实际物理UP、系统根因或原D08根因。记录本身明确不是跨线程原子快照，不把JSONL顺序升级为共同原生时序。
- seq25仍为`capture_attempts=0`、`native_attempts=0`、`writer_started=false`、`completed_gestures=0`；未到capture／关联／writer。单次测试清理UP后的seq20实际LL-UP观察为false，不补造NormalUp，也不以清理资源flags证明尚未建立资源的生命周期通过。a623的当时UNKNOWN和原身份不改写。
- 修订`bd53b68629abc69e05a088f929b41379fd3fff2b`让ConsumedDown按真实receipt／generation／active／未UP／未撤权／deadline入场，其他鼠标键仍须实际clear；旧native Move物理门保留。当前标`DEV_FIXED_PENDING_TARGETED_RUNTIME_REVALIDATION`，不是正式产品替换或新路线通过。
- 收集及精确guest／任务清理前后证据哈希一致；部署保留、其他资源未改。guest销毁不算产品cleanup PASS。下一步只复验该修订的真实capture、Raw／legacy UP、解除关联与退出；满足前不外推Explorer，6/60及2/40分数不变。

## 历史0a8f：登录模拟与普通二启合同通过，故障诊断仍缺导出失败负例

Run `0a8f1f0e477a46c1b1160291c3acb53a`，Release／Medium／`EntryAndSettings`，仍为原R2／458A62与原98fc ZIP。driver／packaging HEAD=`e81d7bdcd289aa6b18f949757b19f543f914b142`，Release driver `D7401664865A586132DF17E9D983F8261B01E452E997C14CDF0948754984E569`；主guest ZIP `BF85A23E8D4C871B80394F2B7183DF24BD8E552DD29FDCEFAC8D9477FCC6901D`／manifest `F9690BA6AEB4515069E18A2CD9ECCAF6F7F1EFB83C9A978F51FDAFA9BE77C4BB`。见[fixture摘录](v1-evidence/0a8f-fixture-selected.jsonl)、[普通产品摘录](v1-evidence/0a8f-portable-product-selected.jsonl)与[真实故障包小件](v1-evidence/0a8f-fault-diagnostic.json)，原件及hash另见索引。

- **M01-F05独立PASS**：267实际建立精确本EXE的HKCU `--startup`值；276／279保存enabled=false／startup=true，285原实例正常退出及真实客户恢复。286以该实际命令参数模拟登录，287新身份；288／289实际status为disabled、无自动设置且配置字节未变。290–292手工空参二启退出0，原PID5732／birth、唯一owner及暂停状态不变，打开同主设置。313／314删除自己的入口及恢复原配置，320正常退出，324／329范围PASS。明确是模拟登录，不宣称实际Windows注销／登录；未在开发宿主启用。
- **M01-F01独立PASS**：801原七文件包、808空参中文空格路径启动、810唯一可见中文设置；811–813第二次同EXE空参启动退出0，原PID5688／birth／设置HWND1180494不变、唯一行为owner及实际status保持。GUI子系统／asInvoker／实际Medium身份与生产mutex分支在资源创建前退出共同支持不重复初始化；不声称实测了全桌面hook数量。658–663同R2实际损坏配置notice／exact备份／默认GUI补齐恢复合同。托盘不是这一独立入口功能的额外门槛；97旧顺序配置激活失败保留历史，不宣其根因关闭。
- **M12-F02仍PARTIAL**：553真实readonly配置失败、557／558实际中文modal、560旧bytes／draft保留后，561实际点击诊断，567取得本批新final文件，568真实字段／配置解析和实际owned标题／Run路径省略，569包含先前真实错误。文件SHA `81A9BC6BD06EF212E88B57FC88A9413E731F7929AA3FC49B57A064CA33F180C1`已独立重算，无seed故障文本。这验证“发生配置失败后成功导出故障包”，**尚未验证诊断导出自身失败时明确报错、无假完整文件**；不把两种失败混同，也不要求无关托盘或普遍隐私矩阵。
- 同98fc普通实例828默认吸引10、833真实NormalUp；845／846保存仅解压目录配置，856吸引0、861真实NormalUp；862／867正常退出与自有资源、866客户`[7,7]→[8,8]`、868交付字节／无profile均成立，869仅本子链PASS。329登录、646保存故障、699损坏恢复及795排除阶段通过。仅705／711托盘BLOCKED使882／884整体FAIL，Core I11／应用矩阵／Full NOT_RUN，不授整版通过。
- 877另一个run-copy正常退出、881客户`[8,8]→[9,9]`；精确guest／task已清理、证据未变／部署保留、其他资源未改另有回执，不以guest销毁授产品清理PASS。外部普通完整桌面／多屏仍缺可用隔离资源：用户已有ISO／VM但RAM不足，不写成缺ISO或等待答复。

## 历史9d74：原98fc普通便携闭环，真实配置写失败保护通过

Run `9d74d68f8a95441fa6344d5f9a73d0b2`，`EntryAndSettings`／Release／Medium；产品同R2／458A62不变，driver／packaging HEAD=`a01eb94db1b92fc28e32bed4b25dd4df7d4a9691`，Release driver `5B1170A2BEE1BD3EE71ADE03539504A0B349714C98CDE418FF4FB74AD9C86327`。主guest ZIP `0DA05EF7632C78D43C9BE00FAE6818DFFECB1D2F900B30EA89B318D57226A102`／manifest `38DF97C4002A07DC723B1DE6B09C089444143D043F2B7D3B306877E127AE91D0`；98fc便携ZIP／R2 EXE均未改字节。关键[fixture](v1-evidence/9d74-fixture-selected.jsonl)、[普通产品](v1-evidence/9d74-portable-product-selected.jsonl)、[普通桌面对照](v1-evidence/9d74-medium-desktop-probe-selected.json)可直接复核，原件及digest见[索引](v1-evidence/index.json)。

- 配置写失败：508仅对本轮精确配置文件设置只读属性；512／513实际中文“设置未更改”modal，515原文件hash、有效代际和未保存draft均保留，528／533实际旧磁吸行为及NormalUp成立。516原属性恢复后540同draft真实保存成功，547／552重启及新值、564／569实际新自由Move与NormalUp成立，583原设置恢复。没有修改DACL，不虚构未记录的`ReplaceFile`错误码。结合493真实导入导出、645损坏exact备份／默认恢复、原同R2 L1对partial／旧新schema／未知字段拒绝、明确portable／user路径，**M11-F05完整独立合同PASS**；不额外要求一种DACL故障机制，也不授整个M11／I07通过。
- 登录开关264／267实际建立精确本EXE `--startup`的HKCU入口，276／279关闭移除，284组件PASS。但此函数只循环true→false，**尚未以实际Run值模拟登录，再手工二启核唯一实例及保存enabled状态恢复**。M01-F05保留这一项具体缺口，不拿托盘、I11或应用矩阵作额外阻断。
- 747原98fc七文件核验；754真正空参启动，756在任何`--settings`前唯一中文GUI。771默认吸引10实际`[140,80,540,440]`；775／776既有public status核同PID／source／g1、实际UP callback QPC不早于本次发送、consumed／未abort／matching branch／push均成立，NormalUp／capture0／exact成立。788／789真实保存只改解压目录配置；799吸引0实际`[134,83,534,443]`，803／804 g2同样闭环。805／810正常退出和自有资源、809真实客户`[5,5]→[6,6]`、811原字节及无profile成立，812为该子链PASS。**D21按此定向关闭**；diagnostic路径仍要求实际详细event，没有产品fallback、重发UP或Raw归属伪造，不关闭D08。
- 独立阶段493／592／645／741及便携812通过；本批只有托盘651／657 `BLOCKED`，故825／827整体仍FAIL，owned Core I11、应用矩阵和Full明确NOT_RUN。不把“仅因托盘阻断”改写成整体PASS。820另一个run-copy正常退出、824客户`[6,6]→[7,7]`；精确guest／task清理及证据未变另核，非产品cleanup替代物。
- guest本地文件副本对照的5项尺寸／hash均一致，实际child token Medium8192／non-elevated／nonAdmin／session1成立；ResumeThread=1后有界20秒仍无退出、phase `NOT_PRESENT`／无receipt，随后仅精确testchild终止、账号及DACL清理确认。**本地副本假设不足，普通Shell GUI仍UNKNOWN**，不是产品FAIL或已证明映射IO根因；同路线有限调查停止，不追加API、timeout或无改动重试。用户已有ISO／VM但RAM不足的外部条件不变，不创建新VM／改宿主资源。

## 历史5017：设置行为补齐，更新中文便携真实NormalUp与driver错误分开

Run `5017e5a10bdd47bfae97026f68adee3b`，`EntryAndSettings`／Release／Medium，产品仍为同一R2／458A62；driver=`bc5eb9a4396b1aa1de30f1566233be670afd30f6`／`3F20B5E64057175D9D99E3986189E9246508FD0B6789BBF25B21D99121B44EE6`，packaging HEAD=`ecab19190196365211a1ce22e76cf2316240c899`。原件与逐行digest在[fixture摘录](v1-evidence/5017-fixture-selected.jsonl)、[普通产品摘录](v1-evidence/5017-portable-product-selected.jsonl)及[索引](v1-evidence/index.json)，不把新driver绑定旧Run。

- 493导入／导出、546损坏配置恢复、642排除／移除三个阶段实际PASS：原生export420；import draft／取消438–439、save459、吸引0的实际Move470／真实NormalUp475；实际restart／notice504–505、exact损坏备份506、默认Move522／NormalUp527；规则595、原生START／实际几何／END605–609且无产品writer、移除后产品Move632／NormalUp637。窄标题布局D20按原受阻操作定向关闭，不回写d85／36fd。M11-F05仍缺主方案明确要求的实际写失败不假报保存／原配置保留及重启保护证据，不因这三阶段通过便授整个M11／I07 PASS。
- 新便携`98fc5fb109a740aba7c27baa0cd29825`仅纠正说明、未重建EXE；ZIP `E10AB78556070603432A7979E89EC3CDAEE00C9DD256205D72CBF9C74A80A236`／manifest `940B3629E60EE31C895FEDA7302DD2E91090AD0E79B2187D81044878D82A06F3`。648七文件核验，655真正空参启动，657在任何`--settings`前出现唯一中文GUI，662 Esc、670 g1 START、672默认Move实际exact `[140,80,540,440]`成立。旧0c710的独立包PASS保留其原身份，不换绑本包。
- 674只发送一次最终MOVE＋UP（2/2插入）；675被动队首实际UP=1／CallNext=1／Unhook成功；676已观察真实NormalUp。普通产品seq7几何重新验证、零新增native，seq8 `normal_up=true`／pending退休，shutdown seq10记录同g1实际UP callback一次、consumed／matching branch／push均true。**原677仍FAIL不是缺UP**：普通模式没有diagnostic专属`input_button_observed`，driver把缺该详细事件误判为失败。D21属于测试合同缺口，修复及新Run尚未预填PASS，原683／686失败原样保留；Raw49→50仍无独立source／generation归属，不用它补证据。
- 678本次普通PID6996正常退出、自有资源flags全true，682真实客户计数`[4,4]→[5,5]`；精确guest／临时任务退出、证据哈希未变、部署保留另有cleanup回执，不以销毁guest替代产品结果。新包保存设置后行为等未走完部分仍待定向复验，不把这次普通Move扩成Full／I11／I12。

## d85／36fd／022：复用同R2，新便携独立通过，测试前置与应用失败分别保留

这些批次不重建产品R2，不把driver源码当产品源码。d85 driver=`7e174bf83ae461ed24c0c7c144a9cd8615706285`／Release `6B83975D5015A1A2F8408A59A48352E369BED13A6AF252E7F5C1820BC5A00A2A`；36fd和022 driver=`660cf6ffc84761cb1d1f2cec832eb01700811caa`／Release `B99E9258C614FDF4B4BCAC9D2DB767957EB83E2D2FAE6497CFC180E909E65CBE`。各自原包、manifest、原日志及摘录哈希在[索引](v1-evidence/index.json)，不能拼成Full。

[d85实际摘录](v1-evidence/d85-fixture-selected.jsonl)：GUI208、诊断232、真实快速UP／terminal251、登录开关／实际HKCU284、五键392分别有范围PASS。原生导出420、import draft438／456、save459成立；损坏配置正常stop493、真实restart／notice494／495、exact备份496、GUI默认500成立。导入后行为464、损坏恢复后行为506在caption候选为空时尚未发DOWN，排除543更在规则操作前受阻；这些失败保留，不写成整个阶段从未运行，也不认定为产品规则或输入失败。日志`hit:2`仅为所请求HTCAPTION，非实际回读。

[36fd新增事实](v1-evidence/36fd-fixture-selected.jsonl)将同类前检缩到24次真实查询：positioning `[111,80,369,449]`、DPI144、DWM按钮带`[28,0,248,44]`，464／507／576各24个候选全部非caption，样本返回13／12／3，无查询失败、超时、foreign命中或测试DOWN。只修自建settings／portable测试布局留出标题空间，产品保护不放宽；本批当时未通过，后由5017定向关闭D20。不能用新Run补造d85旧日志的细节或覆盖旧失败。

### 当前0c710便携功能：按其完整独立合同PASS，不宣额外几何链通过

d85的555原七文件核验、562真正空参启动、564在任何`--settings`前的唯一可见中文GUI成立；575该正常实例PID6532退出／资源flags全true，579实际客户计数`[4,4]→[5,5]`。581只是未请求额外检查、未发input，不是另一清理失败。原ZIP `0235C5…5946F`及EXE未重包／替换。574额外Move前检失败保持FAIL，未宣称本子链有Move或保存设置后的动作效果。

[只读包合同复核转录](v1-evidence/d85-portable-contract-readonly.txt)明确不是新guest或原stdout：收集后七项实际尺寸／SHA全部匹配原manifest；真实dumpbin的11系统imports与manifest一致，无漏带外部CRT。packager／helper／25项工具测试文件哈希与原负例完全相同，实际`Prepare-PortableTestPackage` AST也未变，故复用原生产缺文件／外部依赖拒绝证据，不冒充新执行。结合真实干净guest普通入口，**只授M12-F03／B32 PASS（2.5分）**；M12模块、I01／I12、托盘、完整指南操作链和整版均未因此通过。旧R408／2632／9e5完整保留其历史身份。

原0c710内quickstart仍含旧R408／2632身份文案。以上独立包运行合同PASS**不代表中文说明准确或最终交付通过**；后继98fc说明包已纠正并由5017实际普通启动，但完整说明操作链仍待D21修复后的定向复验。旧ZIP／运行身份不修改，不因纯driver变化反复重包。

### 022应用：Terminal通过；Edge实际START后零placement失败，原因UNKNOWN

[fixture摘录](v1-evidence/022-fixture-selected.jsonl)与[product摘录](v1-evidence/022-product-selected.jsonl)绑定同R2、D660的`ApplicationsOnly`，不重跑owned I11或伪绑定Explorer环境。Terminal45精确绑定、五阶段57–65、70真实NormalUp几何保持、Resize79／84真实UP，离散命令／Esc／实际终端客户输入、114 exact根关闭与115类别范围PASS。Notepad117仍无取得的指定官方程序。Code三个Medium目录RW检查成立，但launcher／一child仍259、另一child `0x80070005`；仅4个hidden根，144本次ownlog不可用／0目录0文件、145无合格窗，原因UNKNOWN；不回填30bf曾有log目录的事实。

Edge（155.0.4283.45）157精确绑定，162 caption前检实际PASS，165真实`WM_NCHITTEST=2`／前台exact／capture0／move-size0，167取得g4 START。169首个free目标`[90,210,994,870]`，实际仍`[150,150,1054,810]`、无placement回执；产品PID6564 seq78真实DOWN→79 START→80 `end=3,normal_up=false`，tick70140已在任何placement之前中止，前后UP callback count仍6。**原因尚未记录，不能断言权限、焦点、capture、UIPI或Windows根因**；不再把最新失败写成旧caption超时／19k查询风暴。179实际根仍活／`WM_NULL=false,error=0`，180／184／188 exact关闭未确认，182混合组BLOCKED；不操作未知popup或强关。189产品正常退出、193客户`[0,0]→[1,1]`与专用guest／任务销毁分开，196整批FAIL，不以销毁guest关闭Edge清理未知。

### 当前外部条件

用户已回复：**已有Windows ISO／VM，但内存不足，当前不能建立新VM**；不是缺ISO、未回复或PaneBind不支持Home。未创建新VM，未调整宿主内存、其他项目任务或设置。普通完整桌面与多屏／混合DPI目前仍缺可用隔离交互环境及足够资源；继续当前Sandbox能运行的项目，不要求购买Pro，也不把该环境缺口当作产品功能PASS。

## 30bf／新R2：D17定向关闭，post-Resize释放仍失败，Terminal类别通过

Run `30bf79c65e5b421aa1087963787898b2`，产品／driver源码均`4d07ef1fe3d3888ac7fbdb9b77afccd6d5a2cf62`，Release产品 `458A62CAEA26F76827BFF84AECDE4F5273EE6876809C411B525B8BF110F5F14C`、driver `B2195564DDD10EA22F5915EA1C0669B2B34FCDEE3CCF9C39FF00D7664B44B4A4`。主包ZIP `7137540B7B9059A268CC7A4B3E3E878F06709043F73593AC94317BF137C707D5`／manifest `B0A44B1EC8C45B61E36618C6149C9E83F51AB21B6678C3D53CCE48CC17E2C1ED`；实际Medium RID8192 non-elevated／session1／Default／150%。[fixture摘录](v1-evidence/30bf-fixture-selected.jsonl)、[product摘录](v1-evidence/30bf-product-selected.jsonl)绑定原件与行digest，不拼为Full。

- g26 A五阶段及760真实NormalUp／几何保持通过。product PID6980 seq303 resnap exact；304重复点只回读、305实际LL UP、306 `normal_up_motion_already_exact`再次核几何且零新native、307 NormalUp／pending退休。Ctrl A（775）、B（795）、C（815）、真实Resize（822／827）均取得实际UP及正确几何，**D17按原反例定向关闭**，不追加历史专项。
- 真实Resize后A回环g31的free／XY／hold／detach／resnap（834–842）均exact；844最终MOVE＋单个UP插入2/2成功，但845队首实际UP=0，846产品UP／end为空，Raw54→55仍无source／generation归属。847／868 FAIL，Pause853只是abort；尚无完整两轮／15分钟，D08继续OPEN。不能用前五手势闭环抵消本反例。
- 视觉永久alpha／置顶、实际注册热键、暂停与退出恢复原tuple173／true（953／967–969），应用改alpha199不被覆盖（977／979）有真实子PASS。拖动alpha190／Move1047 exact后实际UP1052缺失，1073整体FAIL，后续摇动未由本批执行。I08 sourceclose602、临时在途1153、grid1258的原UP失败保留；第二成员真实失败无第三调用、后续新Move／接触／Ctrl及各UP，1352组件PASS。NF01独立60.004483秒／CPU0%／工作集27,435,008字节通过，private commit4,194,304不是驻留，不代替I11。
- **Terminal本次类别PASS**：1382精确新Medium根绑定；五阶段Move／1407实际UP、Resize1416／1421实际UP、Center／DockLeft／Undo／Stretch／Undo、Esc恢复、终端客户输入／不误触Move均实际通过，1451 exact根关闭确认、1452类别PASS。不是借旧b60结果，也不外推其余四类或混合组。
- VSCode三个Medium目录RW探针PASS，launcher和一child仍259，另child `0x80070005`；4个hidden根、无合格根或窗口输入。**本批ownlog根已存在、2目录／0文件**，与旧b49无根不同，原因仍UNKNOWN。Edge精确绑定但23,920点搜索／19,410实际hit查询中18,839失败，样本1460超时；1500 caption FAIL、1510／1514／1516 exact关闭未确认，不操作未知popup或强关。
- 1520产品正常退出、自有资源flags全true、unknown native=false；1524真实客户计数`[1,1]→[2,2]`，1527精确自有Raw注销确认。任务／专用guest清理及证据未变独立记账；不以guest销毁关闭Edge清理未知，1528整批FAIL保留。

[普通桌面probe](v1-evidence/30bf-medium-desktop-probe-selected.json)已实际取得QUERY-only child token的Medium8192／non-elevated／nonAdmin／session1，D18原工具SecurityException定向关闭。但ResumeThread后有界20秒无child退出／回执，仍**UNKNOWN而不是普通GUI成立**；精确测试child终止、账号及DACL清理确认，未启动产品／Explorer或输入，不预判为环境DENIED。

[1b5／99f官方准备摘录](v1-evidence/99f-1b5-notepad-preparation-selected.json)保留两次不同事实：1b5因allowed Geo38970误值保护拒绝、无改变／下载；99f实际39070→244已核，MarketUS manifest200／找到Notepad后官方下载需要Entra交互认证，`-1978335114 / 0x8A150076`，0字节无包、不绕凭据或许可。恢复cmdlet完成但回读仍244／`restore_verified=false`，准备工具恢复**FAIL（D19）**；guest销毁只是隔离影响，不能写配置恢复PASS。产品／Notepad功能仍NOT_RUN。

新R2实际构建目录`out/v1-product-4d07ef1`，Debug EXE `7D503C7CB639698627ADFEE519851757B89BA0A7AE677673FD7247EBE3AC895E`；主执行工具转录（非原stdout归档，chunks `505e83 / 23335e`）记录Debug／Release各10/10 offline（总real4.07s／3.01s，label4.03s／1.65s），各直接input-route252／0。不用这些离线数授功能PASS。新便携`0c7107367dfa4f48b7466455b0fda8ab`的ZIP `0235C5C1C8DAC0E072E51DE2AC99FB7EF7D3A96470BF51CEC17EC6DF8F05946F`／manifest `25978E28E35AD04CE28C5540BAE49BBEB33B392B70AD74ACF5F4A6A793D56003`只已提取/ready，普通无参入口尚待定向运行；旧R408/2632原ZIP及9e5历史PASS整对象保留。

## 历史7cd／R408 serial MOVE＋单次UP：真实释放闭环，但终端几何失败

Run `7cd4f252d8b942e89cb9b3c3eb10a1b4`仍用产品R408／Release `48F96C…3924E`，driver为`209f20222d111b64f99c5330aab157214a8d88c5`／`2E51C539FF8838BBFC3063280EA58FB9885035637CD40FFA991C9B67C91DEB79`，packaging HEAD `c4dc45f7666aac23c1d030f95cd39691d1c5fd46`。主包ZIP `6DE7BEADFA1041C180FA2649575E772470496781BE3C173A57E7AECEA667CB1B`／manifest `2C46A4AAC7FA7021CB0EFD8DD55CDABE232E32AB59B034566C0801D722C7CD16`。这是`CoreAndApplications`分组，不是Full或重新绑定9e5。

- 747／749／751／753／755的自由／XY／保持／拉开／重吸几何成立。757一次SendInput插入最终绝对MOVE再单个UP（2/2返回，不重复旧UP）；758队首observer实际UP=1、CallNext返回1、Unhook成功；759产品PID2112同g26实际UP seq304与NormalUp seq306成立，Raw49→50仍不自行授归属。
- 但[产品摘录](v1-evidence/7cd-product-selected.jsonl)显示seq302真实resnap exact `[120,80,380,440]`；303旧排队MOVE因撤权而零native；实际UP后305独立terminal真实调用50却写入`[115,84,375,444]`，对错误自由计划exact，`authority_after=false`。760／767因此仍FAIL，不是“正常UP未到”。Terminal1491同样已有实际UP／NormalUp，1492仍从期望`[150,150,1050,810]`变为`[143,154,1043,814]`，类别未通过。
- 新缺陷**V1-D17**与D08区分：产品修复`e22cd41f80c52ba5f0311961cbdd38c38e82ff50`已提交，构建HEAD `7385a094852a27f1f8b0c541acc3622b16c0ffec`另含测试probe修复；新产品尚未guest运行，不能用7cd给新EXE授PASS。原7cd没有前一input tick，不能据此宣布equal-tick或旧缺UP根因已查明；D08继续OPEN，D15原定向关闭不回退。
- [fixture摘录](v1-evidence/7cd-fixture-selected.jsonl)保留独立visual1158／第二成员故障1437子项PASS、60.0001793秒idle／CPU0%／工作集27,185,152字节；临时在途1238、grid1343与Edge命中1549仍FAIL。1568产品正常退出／自有资源全true、1572实际客户计数`[1,1]→[2,2]`；专用guest／任务退出和证据未变单独记账，不关闭Edge原清理未知，1576整批FAIL。

[7cd普通桌面probe](v1-evidence/7cd-medium-desktop-probe-selected.json)中credential确为Medium8192／non-elevated／session1，`CreateProcessWithTokenW`实际成功，但QUERY-only child token的工具查询抛`SecurityException`，child从未resume，原结果为**UNKNOWN，不是环境DENIED**。主执行实际本地工具调用复现（未归档stdout，不补造原文），工具修复7385已提交、guest复验NOT_RUN，记V1-D18；精确child／本次新账号已清理、DACL哈希未变、未启动产品／Explorer或发送输入。b49原只读DACL DENIED保留其历史事实，不能由本工具异常外推必须新VM。

[e0e4官方Notepad准备摘录](v1-evidence/e0e4-notepad-market-selected.json)保留真实WinGet verbose：信息端点200，包manifest请求`Market=ZZ`返回400，随后CLI解码异常。它是官方应用准备失败，产品／应用功能NOT_RUN，不是网络全断、产品FAIL或永久缺Notepad；未改区域／语言，未安装或请求凭据。精确测试market配置修复393e5a2已提交，实际新准备仍待验证。

## 9e5／同R同2632原ZIP：便携功能PASS，整版门槛独立继续

Run `9e5a86c7f7864aa9ad6569b2ce5fbd92`，产品R／EXE48F96未改、driverD129／EXE `B317E71FC71D2110568688B5E98A4294C44368EA17AB233211776A50E7CB3AAD`，packaging HEAD=`718d8f59f2b3156152cec48875c01811573265d7`，Release／Medium RID8192 non-elevated／session1／Default／144DPI。主包ZIP `9CD06B8B6CB58E783EA3CB6998535094822045AD103E2AF4FE074FAC15EC09E7`、manifest `54D9A9AAB2F4FAF43893CF7FA95A251D2E206DCB7376A8BD769E90EDE57F0007`；[实际摘录](v1-evidence/9e5-fixture-selected.jsonl)及索引绑定原件位置。2632仍是原ZIP字节，不因改driver重新构建／重包。

- PID6316真实设置底部诊断→1579字节`diagnostic-30187.txt`→parser／隐私40→Esc／配置未改41通过，诊断哈希独立重算 `DF70F71462EA9E7FDA255EC59356E3F95AC8E334AEDF6057ACE85C88A0D52041`；正常stop43、实际客户投递47成立。它只验证本次owned标题／Run路径不泄漏，不外推普遍隐私或整个M12。
- 2632独立解压中文空格目录，49空参正常启动；51在任何`--settings`前出现唯一可见中文GUI PID6932。66默认实际Move／71真实NormalUp；83实际Enter保存、84只改解压目录配置；94新设置改变实际Move／99真实NormalUp；100正常退出／资源全true、104客户计数`[2,2]→[3,3]`、106原字节及无profile检查通过。108是未请求的额外检查，不记失败或第二次stop PASS。109／111为该范围PASS，不替代完整I01／I12。
- 独立只读核验ZIP七项路径、尺寸和逐文件SHA-256全部匹配，包括唯一主EXE、portable标记、MIT许可、中文说明／配置／knownissues与manifest。实际`dumpbin /DEPENDENTS`与manifest的11项系统imports一致，没有漏带外部C++运行库。没有开发目录、网络或driver运行依赖。
- [真实工具负例转录](v1-evidence/portable-contract-negatives-transcript.txt)来自实际25/25工具输出（不是原stdout或现场）：生产packager实际依赖函数拒绝`msvcp140.dll`；从AST仅加载真实guest `Prepare-PortableTestPackage`函数，缺一文件Spec在提取／建目录／启动前被七文件合同拒绝。源是当时工作区测试／脚本哈希，不伪绑定到新产品提交或编造guest缺文件运行。

以上按**便携功能本身**合同给M12-F03／B32 PASS（2.5分）；不是因文件存在／退出0或日志里的`I12=PASS`就授整链。M12其他功能、模块DEV和I12完整入口链仍独立未过。V1-D16三个product-source pin由真实parser和普通ZIP入口定向关闭，8189原失败保留。精确guest／临时任务已退出、证据未变、部署保留，与两产品PID的正常stop／客户投递分别记账。之后新driver仍复用同R，不把9e5运行改绑未来driver。

## b49／同R核心与应用：恢复／视觉有实际子结果，缺UP边界仍失败

Run `b49deace80fe49899375bf8d36481ea1`，产品R同一48F96，driverD129同一B317，`CoreAndApplications`／Release／Medium。主包ZIP `C2AFF10198625D99A81EB77D7E963F610C5234398737D02D5702BEAEC140F39F`、manifest `9B8C7759EACA703BF3B1056CF4F8C188C728D3B29B6480417F6F70BF16C32208`，本地重算一致；原fixture1604行，[关键摘录](v1-evidence/b49-fixture-selected.jsonl)。此分组明确不重跑既有GUI／portable／rapid，也不将分组PASS拼成Full。

- 首个I11 g26五阶段747／749／751／753／755几何成立；756独立guest队首observer安装成功，757单次测试UP插入返回1，758观察期实际UP callback=0且Unhook成功；759 Raw左UP49→50，但source／generation归属UNKNOWN，产品实际UP／end仍null，760／781 FAIL。这是**独立低级observer也缺UP**的新边界事实，不再把本反例简单归为GUI消费；也未证明hook永久移除、UIPI、系统原因或真实物理UP。D15快速UP点定向成功不关闭D08。
- Esc、合法失焦、实际Ctrl+Shift+F11分别有本代真实UP及后续Move／Resize通过子项；sourceclose有撤权／无新写／新Move612及Resize622，但原代UP602失败。actor真实START／exact637、默认Deadline638（child发送前→end为30031ms）、后续Move651／Resize661通过；642清理UP的产品匹配仍null，不补NormalUp，I08整体712 FAIL。
- 单屏停靠／拉伸核心已执行，159跨屏环境阻断，396保持BLOCKED。视觉1172实际原alpha173／正常UP恢复、应用自行改199不覆盖、摇动190置顶与再次恢复173等子项PASS；临时在途1252／1336仍FAIL。grid真实单次non-exact拒绝但原代UP1357缺失，1393保持FAIL；第二成员实际API TRUE后identity失效、无第三调用及幸存新Move／Ctrl／实际UP组件1451 PASS，不把API TRUE当作API FALSE，也不由此授完整I10。
- 当前R独立空闲786／787实际60.0122137秒、单逻辑核CPU0%、末工作集27,287,552字节，满足该环境NF01独立预算；private commit3,923,968不是驻留。NF01可独立PASS，不能替代I11两轮／15分钟或跨机性能承诺。
- [普通新账号环境探针](v1-evidence/b49-medium-probe-selected.json)实际credential Medium8192／non-elevated／session1成立，Input／Output DACL ALLOW，但WinSta0／Default DACL DENIED；只读DACL检查不证明MIC或实际GUI访问。该本次新账号按SID精确移除，未改既有ACL／token／账户，未以它运行产品／Explorer或发送输入。它是**具体guest桌面条件阻断，不是产品FAIL，更不是绝对需要新VM的结论**。
- Terminal真实绑定，五阶段几何正确但实际UP／NormalUp1506缺失，队首observer也0、Raw107→108归属UNKNOWN，当前类别FAIL，不用旧b60类别通过抵消。Notepad本批仍缺指定官方程序，继续准备而非永久跳过。VS Code已携官方1.142.0.0，Medium UserData／Extensions／TEMP真实新文件写／flush／读回／删除均PASS；launcher仍259，一个已持有子进程退出`0x80070005`，只有hidden根且ownlog目录不可用，无唯一合格根、未发窗口输入；不是“整个Code退出”或已证明profile无权限。Edge绑定但标题命中1577 FAIL，精确关闭1591未观察，登记保留，原因UNKNOWN。
- 1596产品正常退出／自有资源flags全true、unknown native=false；1600真实客户计数`[1,1]→[2,2]`，与精确guest／任务退出、证据哈希未变分开。Edge清理未确认不因销毁guest关闭，1604整批FAIL保留。

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
| 当前R3实际候选 | `37e5ffce1290f3c8e07056083e42666859eebe0d`／Release658C12、Debug59D90；866仅entry/settings/diagnostic/portable实际范围。旧R2/458、98fc及全部旧包原身份转history，D08/D23与整版仍未闭合 |
| 方案入库 | `c1e8f82f8156db7c4778473f6b4689327f771013`；变更限 `AGENTS.md`、`README.md`、`docs/PANEBIND_V1_DEVELOPMENT_GUIDE.md`，主执行者已完成标准推送 |
| 当前实测包／入口 | 866／R3的EntryAndSettings与新a00b便携；M12F02定向PASS，tray BLOCKED令总FAIL；另f481为资源探针四场景，不是V1 Move |
| 历史原2632便携 | ZIP `4D7CDAEFC22208233716A24DC2DE6B98FAFEF14A83DCB6B450FF2DF20E5602A2`；manifest `090EFBA44ED960CBB9A16DA75A780A14B38A74D4AFDBAB04F3584B189E1B401C`；产品R／EXE48F96未改；便携原packaging HEAD665b828，9e5 guest包HEAD718d8f5分别记录 |
| 历史dd60 runtime／build | `dd603a79aeba64c816e13ba49265db342c14e7e3`；Debug／Release fresh构建及各10项offline实际PASS；旧结果原身份保留，不迁移绑定当前R |
| 历史e2abc打包 HEAD | `dd603a79aeba64c816e13ba49265db342c14e7e3`；旧已测ZIP不重包，整对象已归入historical_packages；当前R/2632另立真实身份 |
| 历史714c现场 | `714c69f6feba4a0f81678c314eb1747e`，干净guest、Release-only／Medium／`SettingsDiagnostic`，106行scoped PASS；97 Debug专项PASS及后继Release失败同时保留，不外推Full／I01／I12／整版 |
| 历史b60主要功能实测 | `b60e9c343c29f75007df7bf1833fd5d4ab8d3cf8`／Run `c01ffe67d3004961bd00765312869c8a`。不把历史结果换dd60或当前R的新EXE标签；当前R另有8189/b49/9e5真实范围 |
| 714c实际退出 | ZIP实例PID5696正常退出，资源flags全true、unknown native=false；实际post-stop客户计数`[2,2]→[3,3]`。精确guest／任务已清理、证据未变、部署保留。旧c01 Edge清理未知不因本次退出或guest销毁关闭 |
| 714c guest ZIP／manifest SHA-256 | `805A0F93BA6643E3ACFD4E891421D1878EC848C6F47240064229B3758B829F2C`／`FB2A8AA99EDB8CF343EC5383FACFE53C7179439253A9EA1A3F2D224F2B32B11F` |
| 历史e2abc真便携包 | PackageId `e2abc1ec0dcf422088d62c53bc633d24`；ZIP `FACE7CD67969582B53901EBC91C63AC2C473AD982F86D0F774E3904563C0B556`；manifest `7764C9B0561FB96B11A8B06B9A0A2A3DF5205038460C2BD299C4DC6A66C7FC8B`；Release EXE `0CEA6E75FE171F472A020290C157EAE5A154D44B0AECC75B1E4AC60A8A7786FE`；原字节与原scope保留，不换绑2632 |
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

ff 以来的七文件便携 ZIP、精确源身份／哈希、普通无参入口及 I10 在途失败用例均已接线；测试 driver 账本、实际低级鼠标回调和 GetAsyncKeyState 分开记录。后继实际结果见各批，不再用“未到达全部后续功能”概括当前状态。M12 的便携组件、固定表／校验器／证据绑定、中文可执行指南／集中缺陷／review入口均已落地；指南的实际命令、六页设置及托盘名称已与源码核对，**60/60 IMPLEMENTED仅表示已实施，不代表实现正确、说明完整现场走查或DEV通过**。当前独立完整合同为7/60功能、B29／B32共2/40（5%）；模块与整链仍未通过，S1不因实施数或独立单项关闭。

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
| ConsumedDown候选入场门 | f63/6685旧async门拒绝原件保留；bd53修订后d546/4cd两代实际capture／legacy UP／释放／解除／NormalUp撤罩通过资源范围 | 原D08仍OPEN；新关联退出风险／V1 writer／Explorer未验，不升级功能分数或宿主试用 |
| aborted／mapped 配对与准备 | 85c／b092 已修旧配对、authority=0 新配对与 hover 准备，2c 已接 mapped 精确 generation CAS | 历史实际 UP 缺失原因 UNKNOWN；1bee 未发本次 DOWN，不作为配对现场复验 PASS |
| 托盘与五键入口 | fdfe／e4 五键实际客户区和映射检查通过子流程；e4 托盘 UID1 图标不可达，BLOCKED | 五键不等于整个 M09 或托盘菜单通过；仍需真实托盘入口 |
| 在途日志与实际异常 | 7cac payload 求值顺序缺陷已修；e4 已到真实临时在途，保留1694失败及后续安全分流 | 不再记成完全未运行，也不凭迟到完成／已恢复属性关闭实际 UP 或全部 I10 失败 |
| GUI诊断与driver临时文件 | 590误纳`.tmp`的确定driver问题已由dd60 Debug97／Release714c真实final导出及parser专项复验关闭 | 设置底部“诊断”入口已实测，不因此授托盘／普遍隐私／完整M12通过 |
| 导出失败可见反馈 | 86a原R2缺实际modal的FAIL保留；866新R3真实modal/ack、0假包、恢复与重导出实际通过 | D22仅定向关闭／M12F02独立PASS，不抬M12模块或整版，不换绑旧R2 |

### 仍缺的实际环境或尚未闭合能力

本轮严格既有SSH别名只读核ZS-WORKSTATION：Professional／build26200；允许的安装注册表／PATH／已知命令位置未确认普通虚拟化程序，Get-VM不可用、Hyper-V CIM为无效命名空间（非权限拒绝），未确认可用注册VM。**不是全盘无VM证明**，未扫VM磁盘／用户数据、未启动或复用任何VM，未改host／CrossRec。瞬时空闲约12259MiB不推翻用户已有ISO／VM但不足新建VM的资源条件，也不构成资源分配许可；具体工具输出来源见验收表环境备注。

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

验收全集见 [V1_ACCEPTANCE.json](V1_ACCEPTANCE.json)，产品操作见[中文快速入门](../V1_QUICK_START.md)。当前为 **2/40（5%）**：独立登录启动／默认恢复B29及便携B32通过，不能称整版80%；B01–B32 全部必过，B33–B40 固定延后但保留在分母中。整版 DEV 门槛尚未满足，真人 UAT 为 `PENDING`。文档、机器表与后续证据更新应继续区分源码接通、局部实际结果和完整门槛，不能用测试数或旧 High 局部证据凑分。

本稿集中交付已测原ZIP、独立最新版中文指南、机器表与历史证据索引，供整体review；它是PARTIAL阶段交付，不代表满足B01–B32或整版完成。缺可靠共享事件身份时不增加RawNormalUp，不无新证据重复同一缺UP路线。最终文档commit／push及远端SHA由主执行者实际核验后交接，本文不预填自引用FinalSHA。原失败与历史包保留，`uat/`、EXE、ZIP 不入Git；不创建PR、merge、tag或公开release。

现场自动输入仍仅在本任务专属可丢弃 guest；远端文件仅在 `I:\PaneBindMVP1Runs\` 本任务目录，不触碰 CrossRec、其他用户数据、账户、token、UAC 或宿主设置。普通宿主试用和真人手感不能由自动 DEV 自签通过。
