---
description: Build a complete app (design .fig/.json + logic .js) from scratch with figo — scaffold, design via figoedit MCP, script, screenshot self-verify, iterate. 用 figo 从零做一个 app。当用户要"做一个 app / 一个页面 / 一个原型",或要新建/改写一个 figoplay app 工程时使用。
---

# 用 figo 做一个 app

一个 app = **设计(design.json/.fig)+ 逻辑(app.js)+ `app.json`**,渲染走
ThorVG,逻辑跑在 QuickJS。下面是从零做出来的闭环,**按顺序走,每步都自验**。

工具目录(下称 BIN):`${CLAUDE_PLUGIN_DATA}/bin`,Windows 下带 .exe 后缀;
**缺工具先跑 `figo:setup`**。完整 JS API 见
`"${CLAUDE_PLUGIN_ROOT}/include/figo/script.h"` 头注。

## 0. 先想清楚(30 秒)
- **形态** → 选模板:列表类 `list-detail`,多页带底部导航 `tab-shell`,表单 `form`。
- **审美** → 选设计系统:见 `"${CLAUDE_PLUGIN_ROOT}/design-systems/"`(金融选
  coinbase/revolut,极简选 linear-app/stripe,活泼选 duolingo…)。
  **要产出不长成 AI 默认款,设计步骤套 `figo:figo-design` 技能的审美纪律。**

## 1. 脚手架
```
python "${CLAUDE_PLUGIN_ROOT}/tools/figmanew.py" --list                # 看有哪些模板
python "${CLAUDE_PLUGIN_ROOT}/tools/figmanew.py" <dir> -t <template> -n "<App Name>" -d <design-system>
```
产出标准工程 `<dir>/{app.json, app.js, design.json}`(在你的项目目录下)。
**从模板改,不要从零生成。**不贴合任何模板时,复制
`"${CLAUDE_PLUGIN_ROOT}/examples/apps/sample"` 起步。

## 2. 设计(figoedit MCP)
插件已带 figoedit 的 MCP 连接配置(`http://127.0.0.1:9223/mcp`)。启动编辑器连上:
```
"${CLAUDE_PLUGIN_DATA}/bin/figoedit" <dir>/design.json
```
(启动后若 MCP 未连上,用 `/mcp` 重连。)用 21 个 MCP 工具改设计(都在主线程
帧间执行,与用户共享 undo):
- **读**:`get_node_tree` / `get_node` / `get_editor_state`、`get_screenshot`(AI 的眼睛)
- **写**:`create_node` / `update_nodes`(批量=一次 undo)/ `delete_nodes` /
  `duplicate_node` / `move_node`
- **插画**:`import_image`(位图)/ `import_svg`(矢量,含 monochrome/palette 改色)
- **复用**:`make_component` / `create_instance` / `sync_instances`
- **审计**:`audit_design`(token 合规 + 对比度)
- **联动/存**:`set_selection` / `set_page` / `save_document` / `undo` / `redo`

要点:
- **MCP 的 path 参数一律传绝对路径**(相对路径按 figoedit 的启动目录解析,会
  "cannot read file")。
- **别让 UI 光秃**:图标用 `"${CLAUDE_PLUGIN_ROOT}/design-systems/icons/"`
  (35 个 Lucide,`import_svg {path:"<绝对路径>/<name>.svg", monochrome:"<token色>"}`),
  插画用 `import_svg`/`import_image`。
- **重复元素用组件**:卡片/按钮/列表行先做好一个 → `make_component` →
  `create_instance` 盖章;改 master 后 `sync_instances` 传播(同步会覆盖实例内容,
  逐实例的文案/颜色 override 需同步后重打)。一致性和返工都省。
- **套设计系统 token**:建节点前读
  `"${CLAUDE_PLUGIN_ROOT}/design-systems/<sys>/design-tokens.json"` + `DESIGN.md` 的
  "Agent Prompt Guide",按 `"${CLAUDE_PLUGIN_ROOT}/design-systems/TOKEN_MAPPING.md"`
  把 token 折算成 figoedit 属性(`fill` 用 `#RRGGBB`、`cornerRadius`、文本
  `fontFamily`/`fontSize`/`fontWeight`、`effects` 阴影)。pill 圆角=高/2;
  rgba→`#RRGGBBAA`。
- **节点命名是契约**:脚本靠**唯一图层名**找节点(`ui.find("xxx")`)。给要被脚本
  操作的节点起稳定、唯一的名字。
- **列表容器**:要用 `ui.bindList` 的列表,容器必须开**垂直 auto-layout**
  (`update_nodes` 的 `autoLayout{mode:VERTICAL,itemSpacing}`)且**只放一个 item
  模板子节点**——bindList 拿首个子节点克隆 N 份、靠 auto-layout 竖排,多放会重叠。
- **字体是头号坑**:`fontFamily` 只认系统已装或已注册的字族,设了没有的字体会
  回退 Arial(观感打折)。要用特定字体:把 `.ttf/.otf` 丢进 design 文件同级
  `fonts/` 目录,或设 `FIGO_FONTS_DIR` 指向字体目录,figoedit 启动时注册。
- 改完 `save_document` 写回 `<dir>/design.json`。

> 不开编辑器也能直接编辑 `design.json`(REST 风格:node 有 `id/name/type/size/`
> `relativeTransform[[1,0,tx],[0,1,ty]]/fills/children`,TEXT 有 `characters/style`)。
> 颜色是 0..1 的 `{r,g,b,a}`。

## 3. 逻辑(app.js)
对着 script.h 写。常用:
- 事件:`ui.onClick(name, fn(node,x,y))` / `ui.onHover` / `ui.onLongPress`(≥0.5s,
  消费 click)/ `ui.onSwipe(name, fn(node,dir))`("left"/"right")/
  `ui.onScroll(name, fn(node,x,y))` / `ui.onUpdate(dt=>…)`
- 导航:`ui.navigateTo(name, "slideLeft", 0.28)` / `ui.navigateBack()` /
  `ui.selectFrame(name)`
- 数据:`ui.bindList(name, count, (item,i)=>{ item.find("x").text = … })`
- 改属性:`ui.setText/setVisible/setOpacity/setVariant/setScroll`、
  `ui.setEditable/focusText/blur`(表单)
- 滑杆/进度:`ui.bindSlider(track, {min,max,step?,value,knob?,fill?,onChange(v,committed)})`;
  滚轮选择器:`node.snapToChildren` + `ui.onScrollEnd` + `ui.snapTo`
- 异步/存储:`setTimeout/setInterval`、`fetch()`(Promise)、`localStorage`
- 主题:`ui.setThemeMode("dark")` / `ui.setVariable(name,"#hex",mode?)` /
  `ui.bindFill(name, varName)`(设计里 fill 用 colorVar 绑 token)
- manifest:`globalThis.APP`(`APP.entryFrame` 等)

规则:
- **可重入/幂等**——figoplay 改 .js 会热重载、整段重跑,重复跑结果必须一致。
- **设计数据修补在脚本里做**(别改设计源),如
  `ui.findAll("Bottom Nav Bar").forEach(n => n.scrollFixed = true)`。
- node 句柄别跨 `bindList/setVariant/navigation` 持有(底层节点会重建);事件处理器
  里调 bindList/setVariant 会立即停止本次冒泡,处理器持有的 node 参数随之失效。
- **可点容器必须有 fill**:无 fill 的 FRAME 在子节点空隙处不可命中。需要"不可见但
  可命中"就给 alpha=0 的 fill。

## 4. 自验(必做,每次改完都验)
```
"${CLAUDE_PLUGIN_DATA}/bin/figoplay" <dir> --shot out.png [--frames N]   # 渲 N 帧截图退出
```
然后 **Read 这张截图**目检。`--shot` 同时写 `out.diagnostics.json`(字体回退/文本
截断/裁切的结构化警告,空数组=干净)——截图"看起来不对但不知道为什么"先看这里。

交互验证用 SELFDRIVE(见模板里的 `app.js` 或
`"${CLAUDE_PLUGIN_ROOT}/examples/scripts/wallet.js"`):脚本里
`if (globalThis.SELFDRIVE)` 用 `ui.tap(name)` 合成点击驱动巡演,
`figoplay <dir> --selfdrive sd` 产出 `sd_*.png`。
- figoplay **运行中改 .js 会热重载**,可边看边改。
- 合成多帧手势必须**一个 tick 内**完成(down/move/update/up 一气呵成)。
- 巡演/测试脚本数帧不要累计 dt——首帧含文件加载耗时;转场截图门控用
  `ui.transitionProgress() >= 1`(空闲返回 1)。

## 5. 按设计系统审计(可选但推荐)
走 `figo:design-critic` 技能:截图(感性) + `audit_design`(量化) → 逐条修 → 重审。

## 6. 打包多端(进阶)
把 app 打成 win/web/android 单 app 包需要完整仓库的构建树(figmapack 会重链
figoplay 嵌图标),插件不含。需要时克隆 https://github.com/nowasm/figo 按仓库
README 构建后跑 `python tools/figmapack.py <app-dir> --target win|web|android`。

---
**闭环**:脚手架 → figoedit 套 token 建设计 → 写 app.js → `--shot` 看图 → 迭代。
每一步都有"眼睛"(get_screenshot / --shot),不要盲改。
