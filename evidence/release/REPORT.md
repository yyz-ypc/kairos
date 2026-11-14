# 时序之子 V1.0.0 首发验收

日期：2026-11-14。公开源码：YYZ-YPC/kairos；MIT。全新 Git 历史，无旧地址兼容提示。正式版本在其余工作开始前完成，基线见 step0.json。最终资产清单和 SHA256 见 artifacts.json；发布核验记录将在 CI 完成后补入。

## 界面与能力去向

| ID / 显示名 | 导航与骨架 | 密度 / 交互 |
| --- | --- | --- |
| seer 灵视 | 左侧聚焦导航、主任务和最近事件 | 低密度，突出下一步 |
| array 术阵 | 顶部任务导航、紧凑控制台 | 高密度，集中操作及回执 |
| barrier 界门 | 日常 / 管理分组、分区入口 | 分组披露细节 |
| tower 时塔 | 垂直楼层、当前层面板 | 逐层前进，Alt+↑/↓ |
| folio 卷页 | 顶部书签、章节、页边批注 | 阅读与归档，前后章 |

五份 tools/styles/<id>.md 均先于实现编写。未移除原有能力：原后端路由 0 今日、1 视频/文章、2 任务详情、3 记录/日志、4 体检、5 能力维护、6 设置/帮助/更新/组件、7 QQ 连线均保留。视频和任务详情改由快捷入口进入；键盘 1–6 对应当前风格导航顺序。修复沿用现有语义，并在设置 / 组件的“随包修复”组提供入口。模型、语音、记忆、唱歌、备份、维护和账号连接仍走既有功能。

主题命名：dark→长夜/nightfall；light→白昼/daylight；contrast→断界/boundary；moon→月轨；suit→特行。前两者描述光照，断界强调轮廓边界，月轨和特行保留原 ID。完整理由见 docs/theme-names.md。生产设置读取路径会迁移旧值并保留其他字段，持久化回归通过。

颜色来自 tools/theme-tokens.json 契约，各风格仅消费语义令牌。25 组合的正文、弱化文字和状态色达到 AA；断界正文达到 AAA，控件边界至少 3:1。用户自定义调色不计入预设的保证范围。按钮具备名称和键盘焦点，支持 reduced-motion。

## 美术与冥零

按用户选择，每套风格采用对应主视觉：灵视星轨、术阵环图、界门门扉、时塔层塔、卷页星图纸面。每套含 hero / 空态 / 图标底材；共 15 张。另保留获授权的冥零头像，并参照原形象重新绘制银白短发、蓝色发梢、蓝眼及黑蓝蝶饰立绘。原始人设正文与私人语音参考不公开。

由本机 ComfyUI 生成，prompt、seed、尺寸、模型与工作流见 tools/art/。最终 17 张 WebP 共 653148 字节，单张小于 300KB，总量小于 8MB。正文不叠在插画复杂底色上。资源通过 RCDATA 嵌入 exe，短路径按内容哈希缓存，保持离线可用；无大段 base64 内联图片。

## 更新、离线组件与源代码边界

更新仅指向新仓库，stable 默认、beta 独立清单。元数据包含 schema/version/channel/min_version/assets(url/sha256/size)/release/previous。HTTPS 及跳转校验、版本与通道校验通过后，还须下载并验证实际安装包大小和 SHA256，才提示更新。异常仅记录 updates/check.log。原生 CLI 的正确哈希、错误哈希、错误大小、缺少哈希 4 项用本地文件验证。上一版结构及回滚策略已有测试；首发 previous=null，没有可实际回滚的历史版本。后续生成清单用 --previous，旧版本资产保留。

六组运行时资产：Node、WebView2 Loader、hermes、rift、keeper、vendor wheels。补齐优先本地 pack/components，整包与逐文件校验后落盘；失败回滚。离线提供 Release 地址、离线放置位置和 Get-FileHash 方法，测试确认不发起网络请求。Rift 随包 npm 依赖须固定版本且逐文件 SHA256 一致才复用，避免空工作区仍走联网 npm。其余 20 个组件、镜像、默认项与八项必需锁定不变；hermes 仍为 bundled，verify_tree 与数据目录语义不变。

rift/hermes 自有实现以 Node 24.21.0 绑定的 V8 字节码及原生二进制供给，公开仓库无其源码；浏览器端为构建产物。字节码用于交付边界，不宣称不可逆或加密保护。原第三方名称与版本、许可证和 NOTICE 保留；Node、silk-wasm、FFmpeg 许可证已随包补齐。仓库 MIT 不覆盖第三方许可。

公开前剔除：原私人 Git 历史、冥零私人角色正文、QQ 登录包/会话、账号/cookie/token、本机 settings/answers、私有语音与提示、旧账号截图和开发日志、原始 rift/hermes 源码。运行时二进制、FFmpeg、wheels 仅作为 Release 资产，未启用 LFS。使用独立 release-source 白名单导出，原开发目录和私人文件保留本机；获授权的冥零头像与新立绘明确保留。分类与文件列表见 exclusions.json，Git 实际入仓审计见 source-audit.json。

## 验证结果与性能

- core 原有六组及 release policy 全过；WebUI 62/62。
- 25 组合 × 关键页截图 75 张，见 evidence/matrix；三档窗口 1400×900、1152×598、760×600，含引导和组件分页共 570 项，失败 0、控制台错误 0。
- 对比度 25/25；bundled 契约 38/38；假包补齐 11/11；实际编译 Hermes 离线 13/13；版本形态 4/4。
- 原组件假源供给、卸载、协议和迁移回归通过。local-verify 清单/依赖图/锁文件/备份更新/PowerShell 解析通过。
- 实际最终安装器在独立目录安装、六组文件核验、删除 Hermes 文件后识别并从本地包修复、Hermes 与 Rift 离线初始化、第三方声明与形象保留、卸载及注册表恢复均验收；明细见 installer-test.json。
- 测试不使用真实 QQ 账号，也不依赖真实组件下载。实际安装在本开发机隔离目录完成，未声称干净 Windows 虚拟机兼容验收。

实际 Kairos.exe + WebView2、全新隔离配置、合成状态测得首屏 709.75ms，目标 1500ms 通过。原 .local 虚拟主机约 2.38–2.67s；改用 .localhost 并提前启动 WebView 后解决名称解析等待，未关闭安全策略。风格切换导航事件 0。第一次遍历 25 组合 WebView private bytes 增量 237629440 字节（约 226.6MiB），再次遍历增量 19177472 字节（约 18.3MiB）；JS heap 增量 404840 字节。内存增量仍明显，记录实测值，不将其等同于泄漏结论。性能工具与完整快照见 tools/measure-webview.mjs、performance.json。

## 发布产物

完整构建与安装器均报告 V1.0.0 / 1.0.0.0。最终大小及 SHA256 由 artifacts.json、Release 的 SHA256SUMS 和安装包同名 .sha256 共同记录。GitHub Actions 会重建 C++、跑测试与渲染矩阵、验收隔离安装、生成清单及草稿 Release；上线前以最终资产重新核验。
