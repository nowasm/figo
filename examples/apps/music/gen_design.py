#!/usr/bin/env python3
"""Generate design.json for the music benchmark app.

Single 420x900 screen, spotify tokens (design-systems/spotify/
design-tokens.json), all absolute layout so SELFDRIVE pointer coordinates
are known constants. Sections top to bottom:

  Kicker          : (40, 48)   "正在播放"
  Cover           : (60, 76)   300x300, 4 stacked tint layers Cover1..Cover4
                    (one per track, setVisible-swapped by app.js), each with a
                    soft halo circle + 2-char title abbreviation. Swipe target.
  TrackTitle      : (40, 396)  24px bold
  TrackArtist     : (40, 432)  16px muted
  Progress Track  : (40, 470)  340x12  -> center y 476 (bindSlider, seek)
  TimeCur/Total   : (40/320, 490)
  Controls        : Prev (86,522) 48x48, Play (162,522) 96x48, Next (286,522)
                    -> centers (110,546) (210,546) (310,546)
  VolLabel        : (40, 594)
  Volume Track    : (40, 616)  340x12  -> center y 622 (second bindSlider)
  ListTitle       : (40, 652)  "播放列表"
  TrackList       : (40, 686)  340x204, vertical auto-layout, TrackRow 340x48
                    template cloned by ui.bindList

Run:  python gen_design.py   (writes design.json next to itself)
"""
import json
import os

# spotify tokens (design-systems/spotify/design-tokens.json)
BG = "#121212"           # --bg
SURFACE_WARM = "#1f1f1f" # --surface-warm
FG = "#ffffff"           # --fg
MUTED = "#b3b3b3"        # --muted
ACCENT = "#1ed760"       # --accent
ACCENT_ON = "#000000"    # --accent-on
R_LG = 8                 # --radius-lg
FONT = "Segoe UI"        # present on Windows; CJK falls back per-glyph

# Track metadata mirrored in app.js (title abbreviation + a per-album tint;
# tints are album-art content, deliberately desaturated so the spotify green
# stays the single accent).
TRACKS = [
    ("午夜", "#23395d"),   # 午夜快线 - slate blue
    ("雾中", "#4a372a"),   # 雾中信号 - warm umber
    ("环形", "#1e3d33"),   # 环形公路 - deep moss
    ("慢速", "#3b2a4a"),   # 慢速流星 - dusk plum
]
ROW_DATA = [
    ("午夜快线", "落日车队", "3:47"),
    ("雾中信号", "北岛磁带", "4:12"),
    ("环形公路", "青苔俱乐部", "3:05"),
    ("慢速流星", "银河巴士", "4:38"),
]

W, H = 420, 900
GUTTER = 40
CONTENT_W = W - 2 * GUTTER  # 340

_next_id = [1]

def nid():
    _next_id[0] += 1
    return f"40:{_next_id[0]}"

def rgba(h, a=1.0):
    h = h.lstrip("#")
    return {"r": int(h[0:2], 16) / 255, "g": int(h[2:4], 16) / 255,
            "b": int(h[4:6], 16) / 255, "a": a}

def solid(c, a=1.0):
    return {"type": "SOLID", "color": rgba(c, a)}

def xf(x, y):
    return [[1, 0, x], [0, 1, y]]

def text(name, x, y, w, h, chars, size, color, weight=400, align="LEFT",
         valign="TOP", tracking=0, alpha=1.0, visible=True):
    return {"id": nid(), "name": name, "type": "TEXT", "visible": visible,
            "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
            "characters": chars, "fills": [solid(color, alpha)],
            "style": {"fontFamily": FONT, "fontWeight": weight, "fontSize": size,
                      "textAlignHorizontal": align, "textAlignVertical": valign,
                      "letterSpacing": tracking}}

def frame(name, x, y, w, h, fill=None, alpha=1.0, radius=0, children=None,
          visible=True, clips=False, extra=None):
    n = {"id": nid(), "name": name, "type": "FRAME", "visible": visible,
         "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
         "clipsContent": clips, "cornerRadius": radius,
         "fills": [solid(fill, alpha)] if fill else [],
         "children": children or []}
    if extra:
        n.update(extra)
    return n

def ellipse(name, x, y, d, fill, alpha=1.0):
    return {"id": nid(), "name": name, "type": "ELLIPSE",
            "size": {"x": d, "y": d}, "relativeTransform": xf(x, y),
            "fills": [solid(fill, alpha)]}

def slider(name, x, y, w, h, fill_w, knob=True):
    """bindSlider anatomy: track frame + Fill rect child (+ Knob child).
    Baked geometry is placeholder; the engine re-places both at bind time."""
    kids = [frame(name.replace("Track", "Fill"), 0, 0, fill_w, h,
                  fill=FG, radius=h / 2)]
    if knob:
        kids.append(ellipse(name.replace("Track", "Knob"),
                            max(0, fill_w - h), 0, h, FG))
    return frame(name, x, y, w, h, fill=FG, alpha=0.1, radius=h / 2,
                 children=kids)

children = [
    text("Kicker", GUTTER, 48, 200, 16, "正在播放", 12, MUTED, weight=700,
         tracking=1.5),
]

# ---- cover: 4 stacked tint layers, app.js swaps visibility per track ----
cover_layers = []
for i, (abbr, tint) in enumerate(TRACKS):
    cover_layers.append(frame(
        f"Cover{i + 1}", 0, 0, 300, 300, fill=tint, visible=(i == 0),
        children=[
            ellipse(f"Cover{i + 1} Halo", 90, 90, 120, FG, alpha=0.08),
            text(f"Cover{i + 1} Abbr", 0, 128, 300, 44, abbr, 24, FG,
                 weight=700, align="CENTER", valign="CENTER", tracking=6,
                 alpha=0.92),
        ]))
children.append(frame("Cover", 60, 76, 300, 300, radius=R_LG, clips=True,
                      children=cover_layers))

# ---- now playing ----
children += [
    text("TrackTitle", GUTTER, 396, CONTENT_W, 32, ROW_DATA[0][0], 24, FG,
         weight=700),
    text("TrackArtist", GUTTER, 432, CONTENT_W, 22, ROW_DATA[0][1], 16, MUTED),
    slider("Progress Track", GUTTER, 470, CONTENT_W, 12, 0),
    text("TimeCur", GUTTER, 490, 60, 16, "0:00", 12, MUTED),
    text("TimeTotal", 320, 490, 60, 16, ROW_DATA[0][2], 12, MUTED,
         align="RIGHT"),
]

# ---- transport controls ----
children += [
    frame("PrevBtn", 86, 522, 48, 48, fill=SURFACE_WARM, radius=24, children=[
        text("PrevGlyph", 0, 0, 48, 48, "‹‹", 20, FG, weight=700,
             align="CENTER", valign="CENTER")]),
    frame("PlayBtn", 162, 522, 96, 48, fill=ACCENT, radius=24, children=[
        text("PlayLabel", 0, 0, 96, 48, "播放", 16, ACCENT_ON, weight=700,
             align="CENTER", valign="CENTER")]),
    frame("NextBtn", 286, 522, 48, 48, fill=SURFACE_WARM, radius=24, children=[
        text("NextGlyph", 0, 0, 48, 48, "››", 20, FG, weight=700,
             align="CENTER", valign="CENTER")]),
]

# ---- volume ----
children += [
    text("VolLabel", GUTTER, 594, 100, 16, "音量", 12, MUTED),
    slider("Volume Track", GUTTER, 616, CONTENT_W, 12, 238),  # 70 of 0..100
]

# ---- playlist ----
ROW_H = 48
row = frame("TrackRow", 0, 0, CONTENT_W, ROW_H, fill=BG, radius=6, children=[
    frame("RowOn", 0, 0, CONTENT_W, ROW_H, fill=SURFACE_WARM, radius=6,
          visible=False),
    text("RowTitle", 12, 7, 240, 20, ROW_DATA[0][0], 16, FG, weight=600),
    text("RowTitleOn", 12, 7, 240, 20, ROW_DATA[0][0], 16, ACCENT, weight=600,
         visible=False),
    text("RowArtist", 12, 28, 240, 14, ROW_DATA[0][1], 12, MUTED),
    text("RowTime", 272, 0, 56, ROW_H, ROW_DATA[0][2], 12, MUTED,
         align="RIGHT", valign="CENTER"),
])
children += [
    text("ListTitle", GUTTER, 652, 200, 24, "播放列表", 18, FG, weight=700),
    frame("TrackList", GUTTER, 686, CONTENT_W, 4 * ROW_H + 3 * 4,
          children=[row],
          extra={"layoutMode": "VERTICAL", "itemSpacing": 4,
                 "primaryAxisSizingMode": "AUTO",
                 "counterAxisSizingMode": "FIXED"}),
]

doc = {
    "name": "Music",
    "document": {"id": "40:0", "name": "Document", "type": "DOCUMENT",
                 "children": [
        {"id": "40:1", "name": "Page 1", "type": "CANVAS", "children": [
            frame("Music", 0, 0, W, H, fill=BG, children=children,
                  clips=True)]}]},
}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "design.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(doc, f, ensure_ascii=False, indent=1)
print(f"wrote {out}")
