# V1.0.1：潮信修复与冥零图标

## 问题与修复

真实安装布局中，程序目录与工作区分离。旧流程的 verify_tree 校验程序目录，但 `{sidecar}` 默认跟随工作区，setup-hermes 的默认 Source 也跟随 Base，导致初始化报“随包的潮信文件不存在”。重复点击修复无法改变错误路径。

现在共享解析优先使用程序目录的 sidecar；显式 KAIROS_SIDECAR 覆盖保持有效，缺少程序随包布局时仍支持旧工作区布局。潮信安装命令显式传入 Source，直接调用 setup-hermes 时也独立从脚本所属程序目录解析。数据与令牌仍位于原工作区和 Kairos 状态目录。

恢复原冥零头像 ICO：16–256 像素的六组图标资源与原文件逐组比对一致；程序窗口、任务栏、安装器、卸载图标及桌面快捷方式均使用该图标。没有更改五风格、五主题或第三方名称与版本。

## 验证

- 分离目录的离线回归在修复前失败；修复后 17 项全过，覆盖直接调用、令牌保留及实际编译 Hermes 的本地 API。
- 真实安装副本已完成离线修复检查：退出码 0，Hermes 显示“已就绪（随安装包）”；已有 OneBot 配置逐字节未改变。没有登录 QQ、发送消息或启动真实账号。
- 随包修复 12 项、bundled 契约 38 项、WebUI 62 项通过。
- 其他 20 个组件清单项完全不变；Hermes 的 bundled、detect、verify、source、版本和锁定语义不变，仅安装命令补入显式 Source。
- 完整构建通过。云端全量验收及最终安装包本机复验均通过。

本机修复先备份原程序与受影响脚本；私人备份保存在用户工作区的“旧版”目录，不进入公开仓库。脱敏记录见 ../hotfix-1.0.1/repair.json 和 icon.json。

## 最终交付

成功 CI：[https://github.com/yyz-ypc/kairos/actions/runs/43598525134](https://github.com/yyz-ypc/kairos/actions/runs/43598525134)，构建提交 `d54ed7d3136b383faccf94884f5fb86aae7309d9`。core、WebUI 62、契约 38、随包修复 12、迁移 19、分离目录离线 17、渲染矩阵 570 全部通过，云端与最终包本机隔离安装均为 12/12。

`Kairos-1.0.1-setup.exe`：137952855 字节。SHA256：`6a95b27a95d7a9d704ab01e5fd98d8afb2dee5e6faed7eb858b4a77751ec1e3e`。本地七项资产、更新清单和 GitHub 上传 digest 已逐项核对。`latest.json.previous` 保留 V1.0.0；原版资产未改写。

[修复记录](../hotfix-1.0.1/repair.json) · [原图标验证](../hotfix-1.0.1/icon.json) · [最终安装验收](../hotfix-1.0.1/installer-test.json) · [CI](../hotfix-1.0.1/ci.json) · [资产清单](../hotfix-1.0.1/artifacts.json)。
