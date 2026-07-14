# figo theme — Material You 动态主题

一个种子色 → 整套 Material 3 语义配色（light + dark 各 53 个角色）→ 写进文档的
**主题变量表**。fill 绑定 token 名（而不是字面色值）后，换肤/暗色切换是运行时一次调用。

## 用法

app.json 里把库挂进 `libs`（在 app.js 之前加载）：

```jsonc
{ "script": "app.js", "libs": ["../../design-systems/theme/theme.js"] }
```

app.js：

```js
figoTheme.apply("#0b57d0");     // 种子色 -> 生成 light+dark 两套角色并写入变量表
ui.setThemeMode("dark");        // 暗色模式：所有绑定的 fill 一次性重解析
figoTheme.apply("#9a25ae", { variant: "vibrant" });  // 换品牌色，绑定不变
```

设计侧绑定（figoedit MCP）：

```jsonc
// set_variables 先建表（或直接让 figoTheme.apply 在运行时建），然后：
// update_nodes: {"id": "Card", "fill": {"type": "SOLID", "colorVar": "surface-container"}}
```

脚本侧绑定：`ui.bindFill("Card", "surface-container")`。

## API

- `figoTheme.fromSeed(hex, opts?) -> {light: {...}, dark: {...}}` — 纯计算不落库。
- `figoTheme.apply(hex, opts?)` — fromSeed + `ui.setVariables(..., "light"/"dark")`。
- `opts.variant`：`tonalSpot`（默认）| `vibrant` | `expressive` | `neutral` |
  `monochrome` | `fidelity` | `content` | `rainbow` | `fruitSalad`。
- `opts.contrast`：-1..1（默认 0）。

角色名是 kebab-case 的 M3 语义 token：`primary` / `on-primary` /
`primary-container` / `surface` / `surface-container`(-lowest/low/high/highest) /
`on-surface-variant` / `outline` / `error` / `inverse-surface` …

## 实现

`theme.js` = [@material/material-color-utilities](https://github.com/material-foundation/material-color-utilities)
（Google，Apache 2.0，HCT/CAM16 全量算法）esbuild 打成的单文件 IIFE（全局 `MCU`）
+ 底部 `figoTheme` wrapper。重新生成的命令见文件头注释。
