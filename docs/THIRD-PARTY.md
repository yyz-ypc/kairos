# 第三方声明

沿用组件的原始许可见各组件 LICENSE/NOTICE。以下为分叉时保留的第三方法律声明。

# 第三方组件与许可证

本安装包分发以下第三方组件。它们只在本地运行，不联网鉴权，不依赖 DSH。
具体分发与修改方式见各项；原始许可与版权声明保持完整。

## 1. 隙间 rift（基于 dsh-bili-whale）（sidecar\rift）

- 上游：`dsh-bili-whale` 0.4.6
- 许可证：MIT License，Copyright (c) 2026 ALTERSSSS
- 完整许可证：`sidecar\rift\LICENSE`
- 本仓库对它的修改（可用 `tools\patch-sidecar.mjs` 复现，幂等）：
  1. `tools/lib/paths.mjs` 新增 `biliRoot()/outRoot()/mediaDir()/tmpRoot()/logsRoot()`，
     产物路径改由 `RIFT_ROOT` / `RIFT_OUT_ROOT` 等环境变量决定；
  2. `tools/commands/debrief.mjs` 的封面、九宫格、字幕缓存改写到 `<工作区>\out\<BV>\`；
  3. `tools/commands/bili-watch.mjs` 的中间帧改写到 `<工作区>\tmp\`，抽完立即回收。
- 这些修改不改变上游的命令行接口，也不改变其许可证。

MIT 许可证正文：

```
MIT License

Copyright (c) 2026 ALTERSSSS

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 2. Node.js（sidecar\node\node.exe）

- 许可证：MIT 及若干第三方许可证，版权归 Node.js contributors / OpenJS Foundation
- 许可证文本：https://github.com/nodejs/node/blob/main/LICENSE
- 分发方式：官方 Windows x64 `node.exe` 单文件，未修改。

## 3. FFmpeg（sidecar\rift\bin\ffmpeg.exe）

- 许可证：GPL v3（Gyan Doshi 的 Windows essentials 构建，含 GPL 组件）
- 项目主页：https://ffmpeg.org/
- 源码：https://ffmpeg.org/download.html ，构建脚本见 https://www.gyan.dev/ffmpeg/builds/
- 分发方式：原样分发官方构建的 `ffmpeg.exe`，未修改、未静态链接进本程序。
- 如你所在的分发场景需要，可把 `sidecar\rift\bin\ffmpeg.exe` 换成 LGPL 构建，
  工作台通过 `FFMPEG` 环境变量或同路径加载，不需要改代码。

## 4. 本程序自身

`win32\src` 下的 C++ 源码、`art\` 下的界面图、安装脚本与文档由本项目维护。
安装包不包含 Chromium / WebView2 / Electron / Python 运行时。

## 5. 潮信 hermes（sidecar\hermes）

- 主体：本仓库自有代码，随工作台一并分发；不联网鉴权，不依赖 DSH。
- 内置的第三方依赖（原样分发、未修改；放在 `vendor\` 是为了装好的工作区不需要 `npm install`）：
  1. **ws 8.22.2** —— MIT，Copyright (c) 2011 Einar Otto Stangvik、2013 Arnout Kazemier and
     contributors、2016 Luigi Pinca and contributors；许可证正文在
     `sidecar\hermes\vendor\ws\LICENSE`。
  2. **silk-wasm 3.7.1** —— MIT，上游 <https://github.com/idranme/silk-wasm>；随包的是编译好的
     `vendor\silk-wasm\silk.wasm` 与 `index.cjs`，用于把语音转成 QQ 的 silk 格式。
  3. **MinHook** —— BSD 2-Clause，Copyright (C) 2009-2017 Tsuda Kageyu；许可证正文在
     `sidecar\hermes\native\vendor\minhook\LICENSE.txt`，静态编进
     `native\build\hermes-qq-hook.dll`（QQ 协议 hook），不单独分发源码。
- 与 SnowLuma 的关系：潮信是新版默认 QQ 接入端；`snowluma` 组件仍保留为
  旧版回退（`default_install=false`，默认不装），只有显式 `-BundleLogin` 的安装包才会带它的原版包。

## 编译运行时补充

rift/hermes 原始实现不进入公开 Git 仓库；Release 采用 Node 24.21.0 / V8 字节码、原生 DLL 与无源码映射的浏览器构建产物。bytenode 1.5.7 为 MIT，许可证随 sidecar/*/vendor/bytenode/LICENSE 分发。Node 完整许可在 sidecar/node/LICENSE，silk-wasm 完整许可在 sidecar/hermes/vendor/silk-wasm/LICENSE。已有 rift、ws、MinHook、FFmpeg 和 WebView2 声明保留。
