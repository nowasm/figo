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
| macOS (Intel + Apple Silicon) | `macos/` | ✅ universal (x86_64 + arm64), minOS 11.0 |

Built from the private `figo-convert` toolchain (converters `def036c`; fig2json `0bdba31`
on Windows, `3f090df` on macOS), Release, ThorVG static. Windows: MSVC. macOS: clang
universal binaries (`CMAKE_OSX_ARCHITECTURES=arm64;x86_64`, deployment target 11.0,
ThorVG + fig2json lipo'd fat static libs); link only system dylibs. Verified against the
starfall sample on every platform/arch (incl. x86_64 under Rosetta): 2 prefabs / 33 unique
sprites per engine. Note: sprite content hashes can differ across CPU architectures
(float rasterization ULP → a couple of anti-aliased edge pixels); output is deterministic
per architecture.
