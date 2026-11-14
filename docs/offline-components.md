# 更新与离线补齐

仓库和 Release：[YYZ-YPC/kairos](https://github.com/YYZ-YPC/kairos)。程序在设置 → 帮助 → 检查更新选择 stable / beta，默认 stable。HTTPS 清单、版本、最小版本、实际安装包大小与 SHA256 全部通过才提示；失败只记录本机 updates/check.log。设置不受升级影响。首次发布没有上一版；后续 latest.json 的 previous 指向保留的上一版资产，可点「回滚上一版」，先备份再安装。

设置 → 组件 → **补齐随包组件**复用修复入口，只恢复资产清单内的文件。优先读取程序目录 `pack/components/`，每个 ZIP 和 ZIP 内每个文件逐个校验 SHA256；全部验证后才写入，失败恢复原文件。hermes 保持 source.kind=bundled、verify_tree 和 `%APPDATA%/Kairos/hermes` 语义，八项必需锁定规则保持。

离线时不会发起下载。请在联网机器从同版本 Release 下载 `kairos-*-1.0.0.zip`，放入安装目录的 `pack/components/`，对照 SHA256SUMS 执行：

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath '.\pack\components\kairos-hermes-1.0.0.zip'
powershell -NoProfile -ExecutionPolicy Bypass -File tools\repair-bundled.ps1 -Offline
```

源码构建也使用同一清单；初次执行修复后 `bin/WebView2Loader.dll`、Node 和私有运行时就绪。rift/hermes 原始实现不进入公开仓库；Release 包含编译字节码、原生二进制及浏览器构建产物，没有源码映射。其第三方许可证、NOTICE 和名称版本原样保留。Node 字节码与随包 Node 24.21.0 绑定，运行时必须使用该 Node。
