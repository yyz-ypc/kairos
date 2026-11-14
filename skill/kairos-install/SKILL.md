---
name: kairos-install
description: 安装和维护 Windows 桌面应用「时序之子」Kairos；处理 AstrBot、QQ 接入、记忆、语音、桌宠和看视频的现有配置。
---

# 时序之子 · V1.0.1

公开仓库 https://github.com/YYZ-YPC/kairos ，MIT 源码；下载只使用该仓库 Release 的同版本安装包和运行时资产。不要要求用户在聊天中提供密钥、cookie 或账号会话。

唯一版本源 win32/src/version.h，品牌源 win32/src/brand.h。程序 Kairos.exe；状态 `%APPDATA%/Kairos`，程序与工作区分开。工作区自身是机器人根；组件位于 bots 下。随包 hermes 数据目录 `%APPDATA%/Kairos/hermes`，默认端口25109。rift/hermes 名称与版本不改，hermes 保持 bundled，八项必需组件不可跳过。

先通过 scripts/probe.ps1 或程序目录 tools/bootstrap.ps1 -Check 体检。缺随包文件时使用 tools/repair-bundled.ps1；本地 pack/components 优先，包和每个文件 SHA256 校验。离线加 -Offline，不联网；显示 https://github.com/YYZ-YPC/kairos/releases 、所缺包放置位置和 Get-FileHash 校验方法。修复后使用原 bootstrap 初始化，不改变其他组件检测、镜像和锁定规则。

五风格为 seer/array/barrier/tower/folio；五主题为 nightfall/daylight/boundary/moon/suit。旧 theme 的 dark/light/contrast 自动迁移。QQ 登录由用户本人完成，不复用他人会话。测试使用隔离工作区与本地假源。

公共版本保留冥零头像和参照原图重绘的形象；私人角色正文、参考语音与账号数据不分发。默认使用通用角色模板，可通过现有导入入口替换为用户自己的角色。换声音、搬迁、备份、卸载按用户明确范围操作；动 private 或 AstrBot 数据前先备份。

更新默认 stable，beta 显式选择；清单、版本、HTTPS、大小和实际安装包 SHA256 都通过后才提示。校验失败只记录日志。回滚只用清单中真实存在的上一版，先备份程序，不动用户数据。首发没有上一版。
