# figmalib

把 **Figma 设计稿直接作为游戏 UI** 渲染到游戏引擎中的 C++ 矢量 UI 库。

矢量光栅化基于 [ThorVG](https://github.com/thorvg/thorvg)，演示后端使用
[raylib](https://github.com/raysan5/raylib)。核心库与引擎完全解耦 —— 任何能上传
RGBA 纹理、转发鼠标事件的引擎（Unity native plugin、Unreal、Godot、自研引擎）都
可以用几十行代码接入。

```
.fig 文件 ──fig2json──▶ canvas.json ──┐
                                      │  figmalib::loadFigmaFile（自动识别格式）
Figma REST JSON (?geometry=paths) ────┤
                                      ▼
节点树  figmalib::Document (Frame / Rect / Ellipse / Vector / Text / ...)
        │  scene_builder
        ▼
ThorVG 场景图 ──SwCanvas──▶ RGBA8888 像素缓冲（直通 alpha）
        │                          │
        ▼                          ▼
   命中检测 / 输入 / 回调      引擎后端上传为纹理绘制
```

## 目录结构

```
include/figmalib/   公共 API（document / parser / renderer / ui）
src/                解析器、SVG path 解析、ThorVG 场景构建、字体、渲染器
backends/raylib/    raylib 后端（其他引擎后端的参考模板，~100 行）
examples/           demo_raylib 演示程序 + 示例 Figma JSON
third_party/        nlohmann/json 单头文件
```

## 构建（Windows / MSVC）

1. 构建 ThorVG 静态库（同级目录 `../thorvg`）：

   ```
   meson setup build_static --default-library=static -Dstatic=true ^
       -Dengines=cpu -Dloaders=svg,lottie,ttf,png,jpg,webp -Dbuildtype=release
   ninja -C build_static
   ```

2. 构建 figmalib（raylib 由 CMake FetchContent 自动拉取）：

   ```
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   build\demo_raylib.exe                       # 内置示例 UI
   build\demo_raylib.exe path\to\design.fig    # 任意 .fig / canvas.json / REST JSON
   build\render_test.exe [file]                # 离屏渲染自检，输出 BMP
   ```

   demo 中 ←/→ 方向键切换设计稿里的各个 frame。

ThorVG 路径可用 `-DTHORVG_INCLUDE_DIR=... -DTHORVG_LIBRARY=...` 覆盖。

## 使用

```cpp
#include <figmalib/figmalib.h>
#include <figmalib_raylib.h>

auto ui = figmalib::FigmaUI::fromFile("menu.json");   // Figma REST JSON
ui->onClick("btn-start", [&](figmalib::Node&) { startGame(); });
ui->onHover("btn-start", [&](figmalib::Node& n, bool in) {
    ui->setOpacity(n.name, in ? 0.8f : -1.0f);
});

figmalib::RaylibFigmaView view(*ui);
while (!WindowShouldClose()) {
    view.resize(GetScreenWidth(), GetScreenHeight());
    view.update();                       // 输入 + 脏区重绘 + 纹理上传
    BeginDrawing();
    drawGameWorld();                     // UI 半透明区域可透出游戏画面
    view.draw();
    EndDrawing();
}
```

### 获取 Figma 数据

**方式一：本地 .fig 文件（推荐，离线）**

Figma 中 File → Save local copy... 保存 `.fig`，然后直接：

```cpp
auto ui = figmalib::FigmaUI::fromFile("design.fig");
```

figmalib 会自动调用 [fig2json](https://github.com/kreako/fig2json) CLI 转换
（结果缓存在 `design.fig.export/`，.fig 未变化时复用），图片自动从 .fig 内嵌
数据解出并接入渲染。fig2json 查找顺序：`FIGMALIB_FIG2JSON` 环境变量 → CMake
`FIGMALIB_FIG2JSON` 配置的默认路径 → PATH。也可以手动预转换
（`fig2json design.fig outdir`）后直接加载 `outdir/canvas.json`。

canvas.json 解析移植自 [fig2psd](../fig2psd) 的归一化逻辑：节点/填充/效果的
类型结构推断、组件实例从 master 克隆补全（含 symbolOverrides 与共享样式解析）、
derivedSymbolData 烘焙几何回退、vectorNetwork 缝合、逆矩阵渐变几何换算。

**方式二：REST API**

```
GET https://api.figma.com/v1/files/<FILE_KEY>?geometry=paths
    -H "X-Figma-Token: <PERSONAL_ACCESS_TOKEN>"
```

把返回的 JSON 存为文件即可被 `FigmaUI::fromFile` 加载。`geometry=paths` 会附带
矢量轮廓（`fillGeometry`/`strokeGeometry`），描边的 INSIDE/OUTSIDE 对齐也由此精确
还原。图片填充需另行下载（`/v1/images`），放入 `Renderer::setImageDirectory` 指定
目录，文件名为 `imageRef`。

## 已支持的 Figma 特性

- 节点：FRAME / GROUP / RECTANGLE / ELLIPSE / LINE / VECTOR / STAR / POLYGON /
  BOOLEAN_OPERATION / TEXT / COMPONENT / INSTANCE
- 填充：纯色、线性渐变、径向渐变（角度/菱形渐变退化为径向）、图片（FILL/FIT/STRETCH）
- 描边：粗细、虚线、端点/拐角样式、INSIDE/OUTSIDE/CENTER 对齐（裁剪/遮罩模拟；
  有 `strokeGeometry` 时精确还原）
- 圆角（含四角独立半径）、整层透明度、`clipsContent` 裁剪
- 效果：投影（DROP_SHADOW）、图层模糊（LAYER_BLUR）
- 文本：字体目录注册（解析 TTF name/OS/2 表）+ 系统字体匹配（Windows 注册表）、
  Figma 行盒垂直排版（lineHeight + half-leading）、对齐、字距、自动换行、
  省略号截断（textTruncation）、富文本分段样式（单行多色/多字重）
- 字体约定：输入文件旁的 `fonts/` 目录与 `FIGMALIB_FONTS_DIR` 环境变量自动注册
- 运行时：帧切换、命中检测、hover/click 回调、显隐/透明度/文本动态修改

## 暂未支持（路线图）

- 自动布局（auto-layout）/ 约束的响应式重排（目前整帧等比缩放）
- 组件变体（variants）状态切换、原型交互（prototype interactions）
- 多行富文本（多行时回退整段基础样式）、非 ASCII 富文本分段
- INNER_SHADOW；BACKGROUND_BLUR（毛玻璃需要背景采样，ThorVG 不支持）
- 角度/菱形渐变精确渲染（当前退化为径向）
- .fig 的 kiwi 二进制原生解析（当前经 fig2json CLI 转换，已缓存）
- GPU 后端（ThorVG GlCanvas / WgCanvas 共享纹理零拷贝）
