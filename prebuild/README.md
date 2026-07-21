# figo converter prebuilt binaries

Prebuilt, self-contained converters that turn a figo design (`.fig` / `canvas.json` /
Figma REST JSON) into game-engine prefabs. No external tools required — `.fig` parsing
(fig2json) is statically linked.

```
figo2cocos  <input> <out-dir>   # Cocos Creator 3.x prefabs
figo2unity  <input> <out-dir>   # Unity UGUI prefabs   (--linear for Linear color space)
figo2godot  <input> <out-dir>   # Godot 4 scenes
```

Output: `.prefab` / `.tscn` + deduplicated `textures/` (+ `.meta`). Deterministic —
re-running produces identical files.

| Platform | Directory | Status |
|---|---|---|
| Windows x64 | `win-x64/` | ✅ |
| macOS (Intel + Apple Silicon) | `macos/` | pending |

Built from the private `figo-convert` toolchain (converters `def036c`, fig2json `0bdba31`),
MSVC Release, ThorVG static. Verified against the starfall sample: 2 prefabs / 33 unique
sprites per engine.
