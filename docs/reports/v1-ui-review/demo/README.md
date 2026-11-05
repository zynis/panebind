# 五段真实动作展示

当前352驱动要求实际16帧，当前仍0/16、T10 NOT_RUN。停靠段按before／preview／committed／after-Undo四帧，其余四段各三帧；不得用文字替代提交或撤销画面。先完成T08/T09，颜色T07放最后。以下原15帧表为截至2f5f836的历史交付缺口，不代表已取得任何帧。


**实际0/15帧，T10 NOT_RUN。** 新c77产品/驱动已构建；合法guest启动仍受资源门限制。源代码中已接真实五段动作、独立几何断言与原像素捕获，但没有执行就没有演示通过。

| 次序 | 对象与动作 | before / during / after |
| --- | --- | --- |
| 01 | 自有空白source：普通磁吸靠近、XY吸附、保持、拉开、重吸 | NOT_RUN / NOT_RUN / NOT_RUN |
| 02 | 三个精确自有空白fixture：Ctrl联动移动 | NOT_RUN / NOT_RUN / NOT_RUN |
| 03 | 两个精确自有空白fixture：共享边Ctrl缩放 | NOT_RUN / NOT_RUN / NOT_RUN |
| 04 | 单屏自有fixture：停靠预览、真实松键提交、撤销 | NOT_RUN / NOT_RUN / NOT_RUN |
| 05 | 新编辑器：改变居中键后真实生效 | NOT_RUN / NOT_RUN / NOT_RUN |

本轮没有安装录屏器、生成模拟图或冒称五应用兼容/真人UAT。后续实际帧必须逐张绑定真实产品源码、EXE哈希、driver、Run、DPI和独立动作回执；图片仅为展示，不能替代正常UP及实际写入/资源清理证据。
