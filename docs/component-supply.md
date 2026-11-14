# 组件供给（向导版本 2）

向导：欢迎 → 工作区 → 准备组件 → 风格主题 → 连 QQ → 完成。`wizard_ver<2` 的未完成向导仅迁移一次：原步骤 ≥2 加一；已完成向导保持完成状态。

`tools/components.json` 是分组、依赖、检测、下载和功能说明的唯一清单。运行必需 8 项锁定；默认安装 Rift、Breeze 和 Ollama 服务配置，Ollama 客户端默认不选。未安装客户端时，服务项显示等待在设置安装 Ollama，不隐式下载 1.6 GB。手动四项不入队。SnowLuma 仅回退/按需。

`tools/progress.ps1` 写 `<workspace>\logs\setup\progress.json`，schema=2，先写 `.tmp` 再替换。字段：`run_id/state/phase/pct/done/total/speed/eta/selection/items/started_at/updated_at`。每个 item：`id/state/done/total/speed/label/message/mirror/tier`。仅保存固定说明、组件 id、数字与镜像域名，不保存凭据、命令行或带参数的 URL。进行中的检查、下载、外部命令、哈希和解压均会更新检查点。

机器行：

```text
##ITEM  id|state|done|total|speed|label
##TOTAL pct|done|total|speed|eta|phase
##EVENT id|state|message
```

保留旧 `##STEP/##RESULT` 和整轮结束的 `bootstrap-progress.json`。旧 `-Resume` 保留原行为；工作台恢复只读实时文件的未完成 id，使用 `-Component`，不混用 `-Resume`。

`SetupManager` 持有 `Local\Kairos.Setup` 命名互斥与 `logs\setup\lock` 文件句柄，stdout 即时更新，500 ms 轮询兜底。子进程在带终止保护的 Job 中运行，异常退出杀掉安装子进程树；正常成功退出允许明确启动的服务继续运行。单组件 30 分钟，整轮 90 分钟。

暂停写 `logs\setup\pause`。下载循环和外部命令每 200 ms 检查，正常约 2 秒内响应；C++ 在 8 秒后强制结束整个安装进程树。暂停保留队列与缓存；取消清队列并保留缓存。恢复按磁盘真实文件长度 `curl -C -` 续传。只有取消才清队列。

向导完成前，没有旧可续队列就不触发启动检查或后台下载。组件步进入后自动开始必需/默认队列。必需项优先处理，8 项在位即可下一步，可选项继续运行。`wizard_complete=1` 才在启动 8 秒后做 `-Check`；发现必需缺项且 `setup_auto_repair!=0` 才补装。组件设置可关闭自动补装。

前端只读 `setup` 快照。命令：`setup:start/pause/resume/retry/log/skip-optional`；补充内部交互 `select/mirror/cancel/auto-repair/uninstall`。id 必须在清单内，manual 不可安装，required 不可取消或卸载。默认组件卸载保留数据；依赖不跟随卸载。按需组件复用既有 SoulX、桌宠和 AstrBot 插件安装入口。

仅 `size_bytes>0` 的真实下载包显示字节、速度、ETA；总百分比按组件阶段等权计算。不确定的包依赖、复制树、本地命令只显示阶段。默认 Ollama 的 1.6 GB 是选择提示，不替未知包依赖估算大小。

下载没有带宽限速。`--speed-limit 30000 --speed-time 45` 保留为停滞检测；续传、多镜像、SHA256、ExpectedBytes 与磁盘检查保留。PowerShell 5.1 的 SHA256 使用流式 .NET，避免嵌套作用域丢失 Get-FileHash，并在哈希期间检查暂停和写盘。

源码测试入口：

```powershell
cmd /c win32\tests\run-setup-tests.bat
python acceptance\tests\test-component-supply.py
python acceptance\tests\test-component-uninstall.py
powershell -NoProfile -ExecutionPolicy Bypass -File acceptance\tests\test-component-protocol.ps1
python acceptance\tests\test-component-live.py ffmpeg
python acceptance\tests\test-component-live-warm.py
powershell -NoProfile -ExecutionPolicy Bypass -File acceptance\tests\test-tide-migration.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File acceptance\tests\test-tide-offline.ps1
python acceptance\tests\test-tide-bundle-contract.py
node --test win32\tests\webui.test.cjs
node tools\render-matrix.mjs
```

假源必须同时满足源码含 `acceptance/fixtures/setup-source.ps1`、`KAIROS_SETUP_TEST=1`、origin 为 `http://127.0.0.1:PORT`。安装包不包含 acceptance；缺 fixture 时短路，完全忽略两个测试变量。发布目录隔离有实测。真实网络测试只下载、暂停、续传和取消，不解压或安装大组件。

渲染矩阵使用 9342 上的测试浏览器，创建自己的测试标签，不改已有标签。说明卡片三处复用 fixed 定位、300 ms 悬停、焦点立即显示、触屏点按和视口钳制。字节文本从读屏隐藏，阶段 polite，总进度 aria-valuenow 最多每秒一次。

手动项的「去设置」在必需组件齐备后可用；向导内打开真实配置子页，不推进或完成向导，返回后仍在准备组件。LivingMemory / DeepSeek 可启动并打开 AstrBot 后台，QQ 复用连线页，B 站复用现有连接配置和分页；向导完成后跳到相应正式页面。

仅可选队列未完的恢复保留向导当前步骤；单项修复保留其他未完成 id。自动补装始终明确点名 8 个必需项，不由旧可选队列替代。未来 wizard_ver>2 保持版本，不降级。


没有 WebView2 时使用现有原生降级画面上的欢迎/工作区/组件控制。前两步不下载，组件步先只准备 webview2；安装后刷新运行时检测缓存，SDK 可用即切回真实 HTML 向导再启动剩余选择。工作区原有的启动前阻塞弹窗已交给向导，不在欢迎页自动下载。

换镜像将该组件的来源偏好持久化为 logs/setup/mirror-<id>.txt（仅数字）；next-mirror 请求在下一次处理该组件时消费。可用的直链、pip/npm、uv Python 与子安装器来源依次轮换，SHA256 校验不放宽。没有可替换来源时仍保留当前来源并按原校验重试。

暂停时勾选默认组件只持久化队列，不启动进程；继续后才处理。空闲时在组件设置勾选会安装；安装中加入后续队列。已安装组件的修复使用 retry/Force，不能仅以检测通过代替修复。无 Ollama 客户端时 service 从活动选择中移出并保留等待说明；以后主动安装客户端会补上仍启用的 service。

验收报告：evidence/component-supply/report.md。测试假源和真实网络分开记录；不以未达标的最新网络结果宣称全部验收通过。

## 随包组件（潮信）

潮信是安装包内置组件（`source.kind=bundled`），不进入下载队列，也不轮换镜像。检测先看安装目录里完整的一份 `sidecar\hermes`（start.cmd、src\main.mjs、vendor\ws、vendor\silk-wasm、native\build 两个文件），旧的 `bots\Hermes` 副本只作兜底，不能遮蔽随包版本。`verify` 另外要求 `sidecar\node\node.exe` 与数据目录里的 `config\onebot.json`，所以全新工作区不会再误报缺件。

安装只有两步：`verify_tree` 按 `tools\source-manifests\hermes.json` 校验随包源码树 SHA256；`setup-hermes.ps1 -DataDir` 做本地初始化（生成/复用 token、写协议与 OneBot 全局模板），`needs_network=false`。

数据根是 `%APPDATA%\Kairos\hermes`（`KAIROS_STATE_DIR` 可覆盖），账号、配置、状态、日志都在这里；旧 `bots\Hermes\state` 只补缺失迁移，不覆盖已有文件。启动器先选随包树，显式设置 `HERMES_NODE=<安装目录>\sidecar\node\node.exe` 与 `HERMES_HOME=<数据根>`，系统旧 Node 不会抢先。

界面只说三件事：已随包 → 正在初始化 → 已就绪。随包项不出现下载按钮、换镜像、字节、速度或 ETA；失败只给「修复」，文案是随包文件校验未通过、点修复或重装时序之子，不会泛化成网络失败。向导、组件设置、体检共用同一份 `setup` 快照与同一套状态字。

只读验收：`acceptance/tests/test-tide-migration.ps1`、`test-tide-offline.ps1`、`test-tide-bundle-contract.py`。离线用例把安装目录拷到临时目录、用 `KAIROS_STATE_DIR` 指向临时数据根，跑 `bootstrap.ps1 -Component hermes -Offline` 并用随包 Node 真正监听端口；不依赖 npm、下载器或测试假源。
