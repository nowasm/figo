---
description: Convert a Figma design (.fig / canvas.json / Figma REST JSON) into game-engine prefabs (Cocos Creator / Unity UGUI / Godot 4) using the prebuilt figo2cocos / figo2unity / figo2godot binaries bundled with this repo — no build step needed. 用仓库自带的预编译导出器把 Figma 设计一键转成 Cocos/Unity/Godot 预制体,无需编译。当用户想把 .fig / 设计稿转成游戏引擎预制体、或想测试 figo 的引擎导出能力时使用。
---

# figo:export-prefab — 设计稿 → 游戏引擎预制体(预编译,零构建)

三个引擎导出器的**预编译二进制随本仓库/插件直接附带**,拿到仓库即可转换,
不需要 clone figo-convert、不需要编译:

- 插件安装:`${CLAUDE_PLUGIN_ROOT}/prebuild/<平台>/`
- 源码检出:`<figo 仓库>/prebuild/<平台>/`

| 平台 | 目录 | 说明 |
|---|---|---|
| Windows x64 | `prebuild/win-x64/` | `figo2cocos.exe` / `figo2unity.exe` / `figo2godot.exe` |
| macOS | `prebuild/macos/` | universal(Intel + Apple Silicon),minOS 11.0 |
| Linux | — | 暂无预编译,从 figo-convert 源码构建(见下"源码与进阶") |

`.fig` 解析(fig2json)已静态链接进二进制,**无需任何外部依赖**。
Windows 若启动报缺 `VCRUNTIME140.dll`,装 https://aka.ms/vs/17/release/vc_redist.x64.exe。

## 用法

```
figo2cocos  <输入> <输出目录>            # Cocos Creator 3.x 预制体
figo2unity  <输入> <输出目录> [--linear]  # Unity UGUI 预制体
figo2godot  <输入> <输出目录>            # Godot 4 场景
```

**输入**(自动识别格式):
- `.fig` 文件(Figma 桌面端 File → Save local copy 导出)
- `canvas.json` / figo 的 `design.json`
- Figma REST API JSON(`?geometry=paths`)
- React/HTML 页面需先经 web2canvas 采集成 canvas.json(工具在 figo-convert 仓库,见下)

**输出**:每个顶层 Frame 一个 `.prefab` / `.tscn` + 去重的 `textures/`(或 `sprites/`)
+ 引擎 meta。确定性输出——重复运行产物完全一致。贴图由 figo 核心渲染器烘焙,
与 figo 运行时逐像素一致。

成功输出形如 `RESULT: OK, 2 prefab(s), 33 unique sprite(s)`。

## 冒烟测试(用仓库自带示例,秒级)

```powershell
# Windows,在 figo 仓库根目录
.\prebuild\win-x64\figo2cocos.exe examples\apps\login\design.json <临时目录>\cocos_out
.\prebuild\win-x64\figo2unity.exe examples\apps\login\design.json <临时目录>\unity_out
.\prebuild\win-x64\figo2godot.exe examples\apps\login\design.json <临时目录>\godot_out
```

三条各应输出 `RESULT: OK, 2 prefab(s)/scene(s), ...`。macOS 换 `prebuild/macos/` 同理。

## 导入引擎

- **Cocos Creator 3.x**:把输出目录整体拷进工程 `assets/` 下任意子目录,
  编辑器自动导入;把 prefab 拖进场景即可。
- **Unity(UGUI)**:把输出目录拷进 `Assets/`;prefab 挂到 Canvas 下。
  **必问客户/用户工程的 Color Space**(Project Settings → Player → Color Space):
  - **Gamma**(默认导出)→ 与设计稿逐像素一致;
  - **Linear** → 必须用 `--linear` 重新导出(对半透明做 alpha 预补偿),
    否则辉光/遮罩/半透明面板会系统性偏亮约 50%。
- **Godot 4**:输出目录自带 `project.godot`,可直接作为工程打开;
  或把 `.tscn` + `sprites/` 拷进现有工程。

## 源码与进阶

- 导出器与 web2canvas(HTML/React → canvas.json 采集器)的**源码**在同级
  **figo-convert** 仓库(https://github.com/nowasm/figo-convert),
  它链接本仓库核心库;Linux 或想改导出逻辑时去那边构建。
- 各引擎的深度适配细节(组件映射、字体、九宫格等)见 figo-convert 内
  `figo2cocos` / `figo2unity` / `web-to-godot` 技能文档。
