---
description: Design figo screens with anti-template taste discipline (figoedit MCP + design-system tokens + the vendored taste-skill) so the result does not look AI-generated. 用 figo 做设计时套"反模板审美"纪律。当用户要"做个好看的界面/页面"、"设计一个 X 屏"、"别做得像 AI 生成的"、"提升设计品味",或在 new-app 的设计步骤想拔高视觉时使用。
---

# figo-design — figo 设计 + taste 审美(默认开)

把 figo 的设计能力(figoedit MCP 直接读改节点 + design-systems 的 token 下限)和
**taste-skill 的反模板审美纪律**焊在一起。目标只有一个:**产出不要长成 AI 默认那一套**
(居中 hero + 三张等宽卡 + Inter + 紫色渐变 + em-dash + 装饰小标签…)。

> **这个 skill 默认就带着 taste-skill。** 动手前先读
> **`"${CLAUDE_PLUGIN_ROOT}/design-systems/skills/taste/SKILL.md"`**(vendor 自
> [taste-skill](https://github.com/Leonxlnx/taste-skill),MIT)。它是给「写前端代码」
> 的 agent 写的——你的工作是**把它框架无关的审美原则照搬,把它的实现层指令翻译成
> figoedit 节点操作**(见下方翻译表)。aesthetic flavor 另见同目录
> `taste/{minimalist,soft,brutalist}.md`,重做既有界面见 `taste/redesign.md`。

工具:figoedit 在 `"${CLAUDE_PLUGIN_DATA}/bin/figoedit"`(缺则先跑 `figo:setup`);
插件已带其 MCP 连接配置(`http://127.0.0.1:9223/mcp`),启动 figoedit 后 `/mcp` 重连。

---

## 闭环(按顺序走,每步自验)

### 0. Design Read(动手前先读懂房间)
taste-skill §0 的核心,**框架无关,照搬**:先别建节点,先判断用户到底要什么。
一句话写出 **Design Read**:*"我把它读成:给 \<谁> 的 \<什么页面>,\<什么调性>,倾向
\<哪套 design-system / 审美家族>。"* 例:*"读成:给加密用户的钱包首屏,机构信任感,
倾向 coinbase。"* brief 真有歧义只问**一个**问题,能从上下文推断就别问,直接声明并往下走。

**反默认纪律(taste §0.D,最重要)**:不要默认伸手去拿——AI 紫渐变、深色 mesh 上的
居中 hero、三张等宽 feature 卡、到处玻璃拟物、Inter + slate-900。这些是 LLM 的默认,
按 design read 刻意绕过它们。

### 0.5 组件清单(按需求先规划,别等重复才补)
**这步在动手建任何节点之前做,是 figo-design 的硬步骤。** 从 design read + 页面需求里,
列一张**组件清单**(3~8 项):
1. **找重复实体**:哪些 UI 单元会出现 ≥2 次或跨屏复用?(卡片、列表行、按钮族、
   tab、chip、头像块、统计块、输入框、空状态…)
2. **每项定 variant**:同一组件的状态/尺寸变体一次想清楚。
3. **定命名**:每个组件一个唯一稳定名(脚本和 critic 靠图层名找节点)。

然后**先把每个组件的"第一个"建好就立刻 `make_component` 标 master**,后续所有出现
一律 `create_instance` 盖章 + 逐实例 override——**不要先 inline 画一堆一样的、回头再抽**。

### 1. 三个 dial → 落到 design-system
taste §1 的三个 dial 定调子(脑子里的旋钮,不写进文件):`DESIGN_VARIANCE` /
`MOTION_INTENSITY` / `VISUAL_DENSITY`。然后把调子**落到一套
`"${CLAUDE_PLUGIN_ROOT}/design-systems/<name>/"`**(金融 coinbase/revolut,极简
linear-app/stripe,活泼 duolingo,营销 framer,硬边 neobrutalism…)。读它的
`design-tokens.json` + `DESIGN.md` 的 *Agent Prompt Guide* 记住这套系统的性格。
**一套系统贯穿全页**。dial 与 flavor 的对应:

| taste flavor 文件 | 大致对应 figo design-system | dial 基线 |
|---|---|---|
| `taste/minimalist.md` | linear-app / stripe / notion | V5-6 M3-4 D2-3 |
| `taste/soft.md` | duolingo / airbnb | V7-8 M5-7 D3-4 |
| `taste/brutalist.md` | neobrutalism | V8-10 M2-4 D3-5 |

### 2. 套 taste 纪律,用 figoedit 建
启动 `"${CLAUDE_PLUGIN_DATA}/bin/figoedit" <design.json>`,用 MCP 工具建/改节点。
**建之前,把 taste-skill 这些框架无关的硬纪律加载进脑子**(逐条对照 §4 / §9):

- **字体(§4.1)**:Inter 不做默认(除非要中性/Linear 风/无障碍);display 用紧字距、
  紧行高;serif 极度克制;同字族用粗体/斜体强调。
  → figoedit:`fontFamily` / `fontSize`(必须落在该系统字阶)/ `fontWeight` /
  `letterSpacing` / `lineHeight`。
- **配色(§4.2)**:最多 1 个强调色、全页锁死(Color Consistency Lock);禁 AI 紫光;
  阴影染背景色、别纯黑投影。
  → figoedit:`fill` / `stroke` 用 token `#RRGGBB`;阴影走 `effects`(染色、低不透明)。
- **形状(§4.4)**:一套圆角尺度贯穿全页(Shape Consistency Lock)。pill=高/2。
  → figoedit:`cornerRadius` 全部落在系统圆角尺度。
- **布局(§4.3 / §4.7)**:`DESIGN_VARIANCE>4` 就避免居中 hero,改 split / 非对称留白;
  hero 必须一屏装下;导航单行;**禁三张等宽 feature 卡**(§9.C);同一种布局家族一页
  最多出现一次;**eyebrow 小标签每 3 个 section 最多 1 个**(§4.7 头号违规)。
  → figoedit:用 autoLayout 排间距(同一尺度反复出现=有体系),别手凑数字。
- **内容密度(§4.9)**:标题≤8 词、副段≤25 词;**所有可见文案上线前自审一遍**
  (语病、AI 幻觉式俏皮话、假精度数字、Jane Doe / Acme 这类占位名)。
- **可读性(§4.5,error 级,必修)**:每个按钮文字对背景达 WCAG AA;CTA 文案不换行;
  表单 label/placeholder/focus ring 都过对比。
- **em-dash 禁令(§9.G,二元,零容忍)**:界面上任何可见文字**不出现 `—` / `–`**,
  用普通连字符 `-`。这是 AI 的头号 tell。
- **AI Tells(§9)**:版本号 eyebrow(`V0.6`/`BETA`)、`001 · Capabilities` 段号、
  到处中点 `·`、装饰性状态圆点、`Scroll ↓` 提示、locale/天气条——**默认全禁**,
  除非 brief 真要。

**复用即一致**:照 §0.5 的组件清单走——先组件后实例;改 master 后 `sync_instances`。
**节点命名唯一稳定**。

### 3. --shot 自验(感性通道)
```
"${CLAUDE_PLUGIN_DATA}/bin/figoplay" <dir> --shot out.png    # 或 figoedit MCP get_screenshot
```
Read 截图,对着 design read 问:层级一眼分得清吗?留白有呼吸吗?长得像不像 AI 默认?

### 4. audit + critic(理性通道,闭环)
量化 token 合规 + 对比度——眼睛测不准的交给它:
```
audit_design {tokensPath:"${CLAUDE_PLUGIN_ROOT}/design-systems/<name>/design-tokens.json"}
```
`offPalette` / `offTypeScale` / `lowContrast` 趋近 0(剩下的要能说明是有意为之)。
要系统性评审/打磨就转 **`figo:design-critic`**。

### 5. Pre-Flight(出货前逐条过)
- [ ] **Design Read** 一句话声明过了?dial 从 brief 推、不是默认?
- [ ] **一套 design-system 贯穿**,没混系统?
- [ ] **零 em-dash**(`—`/`–`)——标题/标签/按钮/正文/署名/caption 全查过?
- [ ] **一个强调色锁全页**;**一套圆角锁全页**?
- [ ] **每个按钮/CTA 文字对背景过 WCAG AA**?
- [ ] **字号全落字阶、圆角全落圆角尺度、间距成体系**(`audit_design` 验)?
- [ ] **hero 一屏装下**?**没有三张等宽卡**?eyebrow 数 ≤ ⌈section/3⌉?
- [ ] **没 AI Tells**?**文案自审过**?
- [ ] **组件清单先于动手列过**,清单内组件全走 component+instance?
- [ ] `audit_design` 干净(或剩下的偏离都能讲清理由)+ 截图目检通过?

哪一格不能诚实打勾,就还没做完。

---

## 翻译表:taste-skill 的 web 指令 → figo

| taste-skill(web) | figo 等价 / 处置 |
|---|---|
| Tailwind class(`text-6xl tracking-tighter`、`rounded-xl`) | figoedit 节点属性 `fontSize`/`letterSpacing`/`fill`/`cornerRadius`,数值取自 design-tokens.json(折算见 `design-systems/TOKEN_MAPPING.md`) |
| §2.A 选官方组件库(shadcn/Radix…) | 不适用。选一套 `design-systems/<name>/` 代替 |
| `motion/react`、GSAP 滚动动画 | figo 没有 web 动画层。页面级转场用 `ui.navigateTo(name,"slideLeft",0.3)`;复杂滚动劫持动画 = out-of-scope,做干净的静态构图 |
| `next/font` / `@font-face` 字体加载 | figoedit 设 `fontFamily`(系统/已注册字族;见下方字体坑) |
| §4.8 图片策略(image-gen、picsum) | figoedit `import_image` / `import_svg`(可 monochrome/palette 改色);图标用 `design-systems/icons/`(35 个 Lucide)。**别让 UI 光秃,也别 div 假截图** |
| §12 Block Library、§3 RSC、Core Web Vitals | 纯 web,**忽略** |

**框架无关、直接成立、照搬**:§0 Design Read、§0.D 反默认、§1 三个 dial、§4.1-4.9
字体/配色/布局/内容纪律、§9 AI Tells、§9.G em-dash 禁令、§14 Pre-Flight(按上面裁剪)。

---

## figoedit / MCP 前置检查与坑

1. **字体是头号坑(taste-skill 在这里反而会害你)。** taste §4.1 推荐的 Geist /
   Satoshi / Outfit **系统通常都没装**;figo 只认已安装或已注册的字族,设了没有的
   字体会优雅回退 Arial(观感打折)。所以:
   - 把 taste 的字体建议当**「字形性格」参考**,落地时换成**手头真有**的等价字族;
     拿不准就先用系统字族验证版式,再谈换字。
   - 要用特定字体:把 `.ttf/.otf` 丢进 design 文件**同级 `fonts/` 目录**,或设
     环境变量 `FIGO_FONTS_DIR` 指向字体目录——figoedit 启动时会注册。
2. **MCP 的 path 参数一律传绝对路径**(相对路径按 figoedit 的 CWD 解析,会
   "cannot read file"),例如
   `import_svg {path:"${CLAUDE_PLUGIN_ROOT}/design-systems/icons/check.svg", monochrome:"#0052FF"}`、
   `audit_design {tokensPath:"${CLAUDE_PLUGIN_ROOT}/design-systems/coinbase/design-tokens.json"}`。
3. figoedit 意外退出/无响应时:重启它再 `/mcp` 重连即可,文档改动有 `save_document`
   落盘的不丢。

## 和别的 skill 的关系
- **figo:new-app**:从零做 app 的端到端流程。在它的「设计」步骤里套本 skill 的
  taste 纪律,产出就不会是 AI 默认款。
- **figo:design-critic**:做完/改完后的系统性视觉评审与打磨闭环。本 skill 管
  「建得有品味」,design-critic 管「事后审到干净」。
