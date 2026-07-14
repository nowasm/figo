# 设计能力差距盘点 — 专业 UI 视角 × 设计表达力缺口

> 与 [benchmark-gaps.md](benchmark-gaps.md)（运行时/交互缺口 G1–G15）互补的另一条线。
> 2026-07 基于对 `include/figo/document.h` / `src/layout.cpp` / `src/scene_builder.cpp` /
> `design-systems/TOKEN_MAPPING.md` 的核查。结论先行：**运行时缺口决定"能不能做出来"，
> 设计缺口决定"做出来像不像专业产品"。缺口集中在 5 类：token 只到颜色、组件同步
> 覆盖式、动效只有 dissolve、三个现代视觉硬缺口（毛玻璃/混合模式/蒙版）、
> audit 只查违规不查品质。**

## 设计面现状一句话

强项：矢量渲染保真（渐变/阴影/模糊/圆角/裁剪）、约束 + auto-layout（wrap/min-max/
SPACE_BETWEEN/绝对定位子项）、组件变体切换、颜色 design token（colorVar +
light/dark modes + Material 3 种子生成）、字形级字体回退、audit_design 三查
（离色板/离字阶/WCAG 对比度）、auto-height 文本 v1（`layout.cpp` setSize：
autoResize=HEIGHT 的框高随排版结果走——README"已知限制"第一条已过时，待更新）。

空洞：数值类 token 不进文档（TOKEN_MAPPING.md 是一次性人工翻译层）；实例同步
覆盖 override；动效面只有转场 + dissolve；blendMode 源码零命中、蒙版仅内部给
阴影用（`scene_builder.cpp:920`）、BACKGROUND_BLUR 明文降级；排版细节
（tnum/maxLines/真 baseline）缺；audit 不查间距网格/触控目标/dark 平价/
内容压力。

## 缺口清单（按 专业观感提升 ÷ 工程成本 排序）

| # | 缺口 | 影响面 | 内容 | 关键事实 |
|---|------|-------|------|---------|
| D1 | 数值 token 不进文档 | 全部 app | ~~`VariableTable::Var` 值类型只有 `Color`~~ **v1 已实现(2026-07)**：`VariableTable::numVars`（float，与色值分命名空间、共享 modes）+ `NodeData::numVarBindings`（prop→token：fontSize/cornerRadius/strokeWeight/itemSpacing/padding*）；`applyVariables()` 统一回写并报告布局影响，FigmaUI 沿 G14 语义从最外层 auto-layout 祖先 `relayoutNode`（`layoutFrame` 在 authored 尺寸下整树早退，走不通）。全链路：REST 解析/保存（`variables.numbers` + 节点 `varBindings`）、JS（`ui.setVariable` 数字形 / `ui.bindVar` / `ui.getVariable` 双命名空间）、figoedit MCP（set_variables `numbers`、update_nodes `varBindings`、get_node 回显）。回归：layout_test `testNumericTokens`（含 roundtrip）、`_token_regress` 基准 app（21 断言）、figoedit --selftest 3 用例。**仍缺**：平面父级下裸 TEXT 的 fontSize 绑定不重排（README 既有限制，归 D8 文本测量）；string 变量；audit 间距合规检查（归 D9） | document.h `VariableTable`；TOKEN_MAPPING.md 的人工折算层仍在（AI 建节点时改用绑定即可逐步退役） |
| D2 | 实例同步覆盖 override | 组件化的一切 | ~~逐实例 override 需同步后重打~~ **v1 已实现(2026-07)**：不做事后 diff（分不清 override 和旧 master 内容），改为**编辑时显式记账**——update_nodes 打进实例子树的补丁（剔除 x/y/w/h/name）按 `[名字,同名序号]` 路径记到实例根的 `NodeData::instanceOverrides`（JSON 账本，随 .figo.json 持久化），`sync_instances` 重克隆 master 后**回放**账本；目标在新 master 里不存在则丢弃并计入返回的 `droppedOverrides`。undo 覆盖（NodeProps 补 capture instanceOverrides + numVarBindings）。回归：figoedit --selftest 7 用例（记录/保留/传播/账本）。**仍缺**：编辑器 GUI 手工编辑不记账（仅 MCP 路径）；组件属性（text/bool/swap prop）；嵌套实例账本的精确语义 | 无活链决策保留；账本对 runtime 透明（实例是烤好的克隆） |
| D3 | 动效只有 dissolve 级 | 感知质量 | ~~Smart Animate 式变体切换~~ **v1 已实现(2026-07)**：`setVariant(..., dur, VariantAnim::Smart)` / JS `{duration, animate:"smart"}`——换变体前按名字路径（`name#同名序号` 链）快照旧子树的平移/尺寸/透明度/纯色 fill，换入布局后对同路径节点建 tween（cubic-out，`update(dt)` 步进，逐步按路径重解析、中途 rebuild 自动丢弃），配不上的新节点淡入，全配不上回落 dissolve；默认仍 dissolve（不改既有语义），消失节点即时消失（无幽灵克隆）。回归：`_value_regress` 增 7 断言（from 态/中间态/root 不淡出/终态/appear）。**仍缺**：`ui.animate` 属性动画 API + spring 曲线；Lottie（ThorVG 自带 loader，桌面接线成本低；wasm sjlj 限制已知） | `src/ui.cpp` SmartAnim/stepSmartAnims；变体切换架构复用 README"组件变体切换" |
| D4 | BACKGROUND_BLUR 毛玻璃 | 现代视觉 | ~~退化为半透明纯色面板~~ **v1 已实现(2026-07)**：比"冻结近似"更好——**双 pass 光栅**（scene_builder 注册 `BackdropBinding`：节点 fill 之下预留 clip 到轮廓的空 scene 槽；`Renderer::render()` 先隐掉玻璃节点渲一遍、按 `absoluteTransform` 裁出屏幕矩形、CPU 三次盒式模糊≈高斯**烘进像素**后作为普通 Picture 放回槽内，再正常渲第二遍）。每次 render 重采样→滚动内容下毛玻璃是"活"的；无玻璃的 app 零开销。模糊烘像素（而非 GaussianBlur scene effect）是必须的：ThorVG 的 clip 在场景后效果前应用、mask 在此嵌套结构不生效。sigma 按视图缩放折算（HiDPI 正确）。回归：render_test "backdrop"段（玻璃内模糊/玻璃外锐利的像素断言）。**仍缺**：GL 直渲与 renderOverlay 转场贴图不采样（退化半透明面板）；旋转玻璃取 AABB 近似 | `src/renderer.cpp` captureBackdrops/boxBlur、scene_builder BackdropBinding |
| D5 | 混合模式 blendMode | 图像叠加类设计 | ~~零命中~~ **已实现(2026-07)**：`NodeData::blendMode`（UPPER_SNAKE 原文），scene build 时映射 ThorVG `BlendMethod`（MULTIPLY→Luminosity 全套 + LINEAR_DODGE→Add；NORMAL/PASS_THROUGH=默认）。REST/canvas 双解析、序列化、MCP update_nodes/get_node。回归：render_test mask/blend 段 | `scene_builder.cpp` blendMethodOf |
| D6 | 蒙版图层 | 插画/头像/渐变淡出 | ~~isMask 未实现~~ **v1 已实现(2026-07)**：子节点 `isMask` 的 alpha 蒙版其**上方同级**（后画的），自身不绘制；实现为 children 循环切段——每枚蒙版在当前 target 内开新嵌套段 `seg->mask(maskScene, Alpha)`，第二枚蒙版叠加在第一枚之内（Figma 语义）。形状/渐变 alpha 淡出蒙版均可（目检：圆形头像裁切 + 线性渐变淡出）。canvas.json 兼容 kiwi 原名 `mask`。**仍缺**：luminance 蒙版（maskType）；被蒙掉区域 hit-test 仍可命中 | `scene_builder.cpp` children 分段；render_test mask/blend 段 |
| D7 | 视觉效果长尾 | 保真度 | ~~四项缺口~~ **v1 已实现(2026-07)**：INNER_SHADOW `spread`（洞按 spread 内缩——primitive 轮廓精确，path 几何无廉价 inset 仍忽略）；图片 fill **TILE**（内在尺寸×scalingFactor 平铺，Picture::duplicate 铺进 scene，>512 瓦片回落 stretch）与 **CROP**（Figma imageTransform 逆矩阵链 S_node∘M⁻¹∘S_img⁻¹）；**描边角度/菱形渐变**（有 strokeGeometry 时程序化渐变位图 clip 到描边区域精确渲染；live-stroke 仍退化径向）；`import_svg` **clipPath/mask**（借 D6 isMask：导成 Clip/Mask Group，多形状取并集，`<mask>` alpha 近似亮度）。回归：render_test d7 段（spread 环带/圆锥描边方向性/TILE 字段 roundtrip）+ figoedit selftest（clipPath→isMask） | scene_builder makeImage/addInnerShadows/pushStrokePaint、svg_import ClipIndex |
| D8 | 排版深化 | 文本密集 UI | ~~tnum/maxLines/段距缺位~~ **v1 已实现(2026-07)**：`TextStyle` 增 `tabularFigures`（数字逐字成 piece 占统一槽宽=最宽数字、槽内居中，连续数字 glue 防折行截断——GSUB tnum 的替身，计时/价格不抖）、`maxLines` 行钳制真正生效（测绘同源钳制，配 `textTruncation:ENDING` 末行补省略号并逐 piece 回退到放得下）、`paragraphSpacing`（显式 \n 行后加距，placement/caret/选区/取点统一走新 `advances` 数组）。带这些特性的单一样式文本自动改道 TextFlow 管线（原走 ThorVG 原生 Text），测量与绘制一致。REST/canvas 解析（含 Figma `opentypeFlags.TNUM`）、序列化、MCP update_nodes/get_node。README 过时的 auto-height 限制描述已同步修正。回归：render_test typography 段（测量 3 断言 + 离屏渲染 2 断言）。**仍缺**：BASELINE 真实字体度量（仍为样式近似）；数字组过长时整组搬移可能超宽 | scene_builder TextFlow/buildTextFlow |
| D9 | audit 只查违规不查品质 | AI 闭环上限 | ~~只查三样~~ **v1 已实现(2026-07)**，按知识来源分两侧：**静态侧 audit_design** 增 off-spacing（auto-layout 间距/padding 对 token spacing 阶，无阶回落 4pt 网格）、渐变底文字对比度（对**最差 stop** 判 WCAG，不再只看纯色祖先）、**dark 平价 low-contrast-mode**（colorVar 绑定的文字/底色逐 mode 重解析，当前过但换肤后跌破 AA 的提前报）、radius-cluster（近重复圆角聚类，如 10 与 12 并用）；**运行时侧 ui.diagnostics(opts)** 增 touch-target（有 onClick/onLongPress 的节点 <44px——引擎知道谁可点，静态审计猜不准）、state-coverage（可点实例的组件集无 Hover/Pressed 变体）。默认零参调用行为不变（空数组=干净的既有契约保留）。回归：figoedit selftest 2 用例 + `_diag_regress` 基准 app | editor_mcp toolAuditDesign、ui.cpp diagnostics(opts) |
| D10 | 内容压力测试 | i18n / 动态数据 | ~~伪本地化缺位~~ **已实现(2026-07)**：`ui.diagnostics({textStress: 1.3})`——现在放得下、但文案 ×1.3 后（翻译膨胀的廉价替身）会溢出盒子的定尺寸文本报 `text-stress`（同 text-overflow 的排版复测管线，构造加长孪生节点重排；自适应尺寸文本随内容生长、豁免）。figo-web 双语视频实测踩中的"英文一换就爆版"从此可提前查 | ui.cpp diagnostics textStress 分支；`_diag_regress` |
| D11 | 响应式断点 / RTL / 动态字号 | 远期 | 断点变体（按视口宽切 frame 或 token mode）；RTL 镜像（auto-layout reverse + 图标翻转）；用户字号缩放（依赖 D8 文本驱动布局成熟） | reflow 机制是地基，modes 泛化（D1）后断点=一种 mode |

## 缺口 × 场景矩阵

| 场景 | 命中缺口 |
|---|---|
| 设计系统规模化（多 app 共用 token/组件） | **D1、D2** |
| "不像 AI 默认款"的高级感 | **D3、D4**、D5、D8 |
| 图像/插画密集（电商、媒体、社交） | D5、D6、D7 |
| 金融/数据仪表盘 | D8(tnum)、D9 |
| 出海/多语言 | **D10**、D11(RTL) |
| AI 自主设计质量兜底 | **D9、D10**（audit 是闭环的"眼睛"） |

## 补齐顺序建议（backlog）

1. ~~**D1 数值 token**~~ **v1 已实现(2026-07)**，见上表 D1 行。
2. ~~**D2 override 保留**~~ **v1 已实现(2026-07)**，见上表 D2 行（组件属性后置）。
3. ~~**D3 Smart Animate**~~ **v1 已实现(2026-07)**，见上表 D3 行
   （spring/`ui.animate` 后置）。
4. ~~**D4 毛玻璃 + D6 蒙版 + D5 blendMode**~~ **均已实现(2026-07)**，见上表
   D4/D5/D6 行。
5. ~~**D9 audit 品质维度 + D10 伪本地化**~~ **v1 已实现(2026-07)**，见上表
   D9/D10 行（bench "设计质量分"度量后置——先让新检查在真实 app 上跑出基线）。
6. ~~**D8 排版细节** + **D7 视觉长尾**~~ **均已实现(2026-07)**，见上表 D7/D8 行。
   **D11** 等 D1/D8 成熟后立项（断点=token mode 泛化的自然延伸；RTL/动态字号
   依赖文本布局进一步成熟——D1-D10 落地后此为唯一未立项项）。

原则同 [ROADMAP.md](ROADMAP.md)：不凭空设计 API；闭环确定性（毫秒级截图自验、
结构化节点操作）不得退化——D3/D4 的动画与模糊都必须在 `--shot` 帧数门控下
可确定性截图。
