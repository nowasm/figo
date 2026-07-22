---
description: Install or repair the figo toolchain (prebuilt binaries) for this plugin. Run this first when any figo skill reports a missing tool, right after installing the plugin, or after a plugin update. 安装/修复 figo 工具链——装完插件先跑这个,或任何 figo 技能报"找不到工具"时跑。
---

# figo:setup — 安装 figo 工具链

figo 插件的技能依赖一组 CLI 工具。本技能把它们装进**跨插件更新持久**的数据目录:

- 工具目录(下称 **BIN**):`${CLAUDE_PLUGIN_DATA}/bin`
- 插件资源根(design-systems / templates / examples / tools 等,下称 **ROOT**):`${CLAUDE_PLUGIN_ROOT}`

工具清单:`figoplay`(app 播放器/截图)、`figoedit`(可视化编辑器 + MCP)、
`render_test`(自检)。Windows 下都带 `.exe` 后缀。
**三引擎导出器(figo2cocos / figo2unity / figo2godot)已预编译并随插件附带**,
无需安装:`${CLAUDE_PLUGIN_ROOT}/prebuild/win-x64/`(Windows)或
`${CLAUDE_PLUGIN_ROOT}/prebuild/macos/`(macOS universal),用法见
`figo:export-prefab` 技能。其源码与 web2canvas 在 figo-convert 仓库
(https://github.com/nowasm/figo-convert)。

## 1. 检查现状

先看 BIN 下工具是否已在(在则跳到第 4 步验证):

```
ls "${CLAUDE_PLUGIN_DATA}/bin"
```

## 2a. Windows:下载预编译二进制(推荐,秒级)

```powershell
New-Item -ItemType Directory -Force "${CLAUDE_PLUGIN_DATA}/bin" | Out-Null
Invoke-WebRequest -Uri "https://github.com/nowasm/figo/releases/latest/download/figo-tools-windows-x64.zip" -OutFile "$env:TEMP\figo-tools.zip"
Expand-Archive -Force "$env:TEMP\figo-tools.zip" "${CLAUDE_PLUGIN_DATA}/bin"
Remove-Item "$env:TEMP\figo-tools.zip"
```

- 运行时依赖:VC++ 2015-2022 运行库(绝大多数机器已装)。工具启动报缺
  `VCRUNTIME140.dll` 就装 https://aka.ms/vs/17/release/vc_redist.x64.exe。
- `.fig` 解析(fig2json)已静态链进 exe,无需额外安装。

## 2b. macOS / Linux:源码构建(进阶)

预编译包暂只有 Windows。其他平台从源码构建(需 git、CMake ≥3.20、Ninja、meson、C++20 编译器):

```bash
WORK=~/figo-build && mkdir -p $WORK && cd $WORK
# 1) ThorVG 静态库(figo 的矢量光栅化引擎)
git clone --depth 1 https://github.com/thorvg/thorvg
cd thorvg && meson setup build_static --default-library=static -Dstatic=true && ninja -C build_static && cd ..
# 2) figo 本体(quickjs/raylib 由 CMake FetchContent 自动拉取)
git clone --depth 1 https://github.com/nowasm/figo
cd figo && mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
# 3) 装进插件数据目录
mkdir -p "${CLAUDE_PLUGIN_DATA}/bin"
cp figoplay figoedit render_test "${CLAUDE_PLUGIN_DATA}/bin/"
```

引擎导出器 **Windows/macOS 无需构建**——仓库 `prebuild/` 已附带预编译二进制
(见 `figo:export-prefab` 技能)。仅 Linux(或需要改导出逻辑)时另 clone
figo-convert(与 figo 同级)并在那边构建:
`cmake -S figo-convert -B figo-convert/build -G Ninja -DCMAKE_BUILD_TYPE=Release
&& cmake --build figo-convert/build`,产出的 figo2cocos / figo2unity /
figo2godot 一并拷进 BIN。

- CMake 默认在 figo 源码的**兄弟目录** `../thorvg` 找 ThorVG(上面的布局正好满足);
  放别处用 `-DTHORVG_INCLUDE_DIR=... -DTHORVG_LIBRARY=...` 指过去。
- **macOS/Linux 上 `.fig` 直读需要外部 fig2json**(Rust,MIT):
  `git clone https://github.com/nowasm/fig2json && cd fig2json && cargo build --release`,
  然后设 `FIGO_FIG2JSON=<fig2json 可执行文件路径>`,或预转换
  `fig2json <file.fig> <outDir>` 后用产出的 canvas.json。不装也不影响
  canvas.json / Figma REST JSON / HTML 输入。

## 3. web2canvas 依赖(仅 HTML/React 输入需要)

HTML/React → canvas.json 的采集器已随导出器移到 figo-convert 仓库
(`figo-convert/tools/web2canvas`),需要 Node 18+ 和一次 `npm install`
(驱动已装的 Edge/Chrome,不下载 Chromium):

```
cd <figo-convert>/tools/web2canvas && npm install
```

## 4. 验证(必做)

```
"${CLAUDE_PLUGIN_DATA}/bin/render_test"
```

应输出 `RESULT: OK`(缺系统字体的环境里 `auto-height text did not grow` 单点失败
属环境问题,可忽略)。引擎导出链用仓库自带的预编译二进制再做一次真转换冒烟
(Windows 用 `prebuild/win-x64/figo2cocos.exe`,macOS 用 `prebuild/macos/figo2cocos`):

```
"${CLAUDE_PLUGIN_ROOT}/prebuild/win-x64/figo2cocos.exe" "${CLAUDE_PLUGIN_ROOT}/examples/apps/login/design.json" <临时目录>/cocos_out
```

应输出 `RESULT: OK, 2 prefab(s), ...`。通过 = 工具链就绪,临时产物用完即删。

## 故障排查

- **下载失败/超时**:公司代理环境给 `Invoke-WebRequest` 加 `-Proxy`;或手动从
  https://github.com/nowasm/figo/releases 下载 zip 解压到 BIN。
- **Expand-Archive 报锁**:BIN 下工具正被占用(figoedit/figoplay 在跑),先结束进程。
- **figoedit MCP 连不上**:MCP 是 figoedit 进程内置的(http://127.0.0.1:9223/mcp),
  必须先启动 `"${CLAUDE_PLUGIN_DATA}/bin/figoedit" <设计文件>`,再在 Claude Code 里
  `/mcp` 重连。
