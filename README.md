# 时序之子 · Kairos

**V1.0.1 正式版**。Windows C++ / WebView2 工作台，保留冥零形象，以五种导航和工作方式管理聊天接入、模型、记忆、语音、视频、组件与记录。

[公开仓库](https://github.com/YYZ-YPC/kairos) · [安装包与离线组件](https://github.com/YYZ-YPC/kairos/releases/tag/V1.0.1) · [MIT 许可证](LICENSE)

## 使用

下载 `Kairos-1.0.1-setup.exe`，对照 Release 的 `SHA256SUMS` 校验。默认工作区为文档中的「时序之子」，状态与本机配置位于 `%APPDATA%/Kairos`。安装包包含 Node、rift、hermes、FFmpeg、原生 DLL / launcher、通用配置和插件；这些运行时可离线初始化，不附带 QQ 登录包或任何账号会话。

首次启动选择工作区、准备组件、选择外观并由用户本人连接 QQ。模型、AstrBot、第三方登录与密钥按组件说明自行配置；软件不会附带私人账号或冥零的私人角色正文。冥零头像和参照原图重绘的公共立绘保留，个人角色可在本机导入。

## 五风格 × 五主题

| 风格 | 工作方式 | 规格 |
| --- | --- | --- |
| 灵视 / seer | 聚焦当前任务与最近事件 | [seer](tools/styles/seer.md) |
| 术阵 / array | 顶部任务导航、紧凑操作与依赖回执 | [array](tools/styles/array.md) |
| 界门 / barrier | 日常与管理分组，显式开门查看详情 | [barrier](tools/styles/barrier.md) |
| 时塔 / tower | 垂直楼层索引、逐层切换 | [tower](tools/styles/tower.md) |
| 卷页 / folio | 书签、章节与页边批注 | [folio](tools/styles/folio.md) |

主题为长夜 nightfall、白昼 daylight、断界 boundary、月轨 moon、特行 suit。全部可组合；1–6 切当前风格导航，时塔支持 Alt+↑/↓。风格切换不重载 WebView。[命名与迁移](docs/theme-names.md)。

## 更新与随包修复

设置 → 帮助 → 检查更新，默认 stable，也可选 beta。客户端校验 HTTPS、版本、最小版本、安装包大小和实际 SHA256 后才提示新版；失败写入本机 `updates/check.log`，启动继续。后续发布保留上一版资产，通过 `latest.json.previous` 支持「回滚上一版」；V1.0.1 保留 V1.0.0 的上一版资产；beta 清单跟随最新正式基线。

设置 → 组件 → 随包修复 → **补齐随包组件**。先读取 `pack/components/` 的同版本包，缺失且网络可用才访问 Release；离线不尝试联网，并显示地址、放置位置和校验方法。包与每个文件都校验 SHA256，失败保留原文件。[离线操作说明](docs/offline-components.md)。其他组件仍使用 `tools/components.json` 既有检测、镜像、版本和八项必需锁定规则。

## 从源码构建

需要 Windows、Node.js 24.21.0、Python 3、MinGW g++ / windres、WebView2 Runtime；打包另需 Inno Setup 6。rift/hermes 原始实现不公开，先获取同版本 Release 运行时资产并放入 `pack/components/`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/repair-bundled.ps1 -Offline
node tools/build-web.mjs
cmd /c win32\build.bat
$env:KAIROS_ISCC='C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
cmd /c installer\build-installer.bat -FromBuild
```

修改前端后必须先运行 `node tools/build-web.mjs`，生成的 `win32/web/index.html` 会嵌入 exe。请将 `MINGW_BIN` 指向编译器目录，或在不入仓的 `win32/build.local.bat` 中设置。

## 验证与发布

```powershell
cmd /c win32\tests\run-core-tests.bat
node --test win32/tests/webui.test.cjs
python tools/check-ui-palettes.py --check-css
python acceptance/tests/test-tide-bundle-contract.py
powershell -NoProfile -ExecutionPolicy Bypass -File acceptance/tests/test-bundled-repair.ps1
node tools/render-matrix.mjs
python tools/generate-release.py --dir dist --version V1.0.1 --channel stable
```

渲染矩阵使用隔离的 Edge / Chromium 调试端口 9342。GitHub Actions 构建 C++、运行隔离测试、25 组合渲染矩阵、生成摘要与更新清单，并生成草稿 Release。维护者可用上一版 `latest.json` 传入 `--previous`；资产不删除。beta 清单另保存在 `channel-beta` 预发布指针。

源码、清单、脚本、文档与原创美术入 Git；sidecar、wheels、DLL、exe 等放 Release，不使用 LFS。MIT 适用于本仓库源码；[第三方许可证与 NOTICE](docs/THIRD-PARTY.md) 保留原文。交付结果见 [DELIVERY](DELIVERY.md) 与 `evidence/release/`。
