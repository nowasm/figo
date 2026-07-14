# figo M3 组件库（token 绑定母版模板）

样式规格对齐 [Sober](https://soberjs.com/)（apprat/sober，MIT）的 Material You
实现——各组件的尺寸/圆角/内边距/色角色取自其组件 CSS 的设计数值。字体随库
附带 Roboto 400/500/700（`fonts/`，Apache 2.0），app.json 的 `fonts` 指向该目录。

`m3-components.figo.json` 是一个**模板文档**：Components 页放着一套 Material 3
组件母版（COMPONENT / COMPONENT_SET），所有 fill/stroke 都绑定主题变量
（`colorVar`），文档自带完整变量表（light + dark 各 53 个 M3 语义角色，
基线种子色 `#6750A4`）。Preview 页有一个 420x860 的总览帧用于目检。

新 app 的标准起手式：**复制这个文件当 design**，屏幕用 `create_instance`
盖章组件搭，配色天生一致、天生支持暗色/换肤：

```js
figoTheme.apply("#0b57d0");   // 换品牌色（app.json libs 挂 design-systems/theme/theme.js）
ui.setThemeMode("dark");      // 暗色模式
```

## 母版清单

| 组件 | 变体 | 说明 |
|---|---|---|
| Button | Type=Filled/Elevated/Tonal/Outlined/Text × State=Default/Hover/Pressed | 40dp、全圆角、state layer 已做好，`ui.autoStates` 直接可用 |
| Switch | State=On/Off | 52x32，M3 track/thumb 规格 |
| Text Field | State=Default/Focused | outlined 风格，Value 文本节点可 `setEditable` |
| Chip | State=Default/Selected | assist/filter chip |
| Nav Item | State=Selected/Default | 底部导航单项（pill 高亮） |
| Checkbox | State=Checked/Unchecked | 24dp 触区、18dp 视觉盒 |
| Radio | State=Selected/Default | 20dp 圆环 + 内点 |
| Segment | Position=Start/Middle/End × State=Default/Selected | 一段一个实例，Start+Middle+End 拼成 M3 segmented control |
| Badge | Type=Dot/Count | error 色小红点 / 计数胶囊 |
| Card / List Row / Slider / FAB / Top App Bar / Dialog / Search Bar / Avatar / Divider / Linear Progress / Circular Progress | 单变体 | Slider 与 Linear Progress 的 Fill/Knob 命名与 `ui.bindSlider` 约定一致（进度条用 `readonly:true`） |

总览帧两个：`Preview`（基础组件）、`Preview Inputs`（表单/指示件）。

变体名遵循运行时约定 `Prop=Value, Prop=Value`，`ui.setVariant("btn", "State", "Hover")`
/ `ui.autoStates` 开箱即用。List Row 等可点容器已带 alpha=0 fill（整行可命中）。

## 常用 token

背景 `surface`，卡片 `surface-container`(-low/high/highest)，正文 `on-surface`，
次要文字 `on-surface-variant`，主色 `primary`/`on-primary`，强调容器
`primary-container`/`secondary-container`，描边 `outline`/`outline-variant`，
错误 `error`，语义扩展 `success`/`warning`（含 on-/-container 全套，Sober 同款
固定色值——不随种子色换肤，警示语义恒定）。全表看 `m3-palette.json` 或
figoedit `get_variables`。

## 重新生成

```
python tools/gen_components.py     # 读 m3-palette.json，重写 m3-components.figo.json
```

改样式改 `tools/gen_components.py`；换基线调色板重新生成 `m3-palette.json`
（见 design-systems/theme/README.md 的 fromSeed）。
