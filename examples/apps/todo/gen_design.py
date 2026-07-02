#!/usr/bin/env python3
"""Generate design.json for the todo benchmark app.

Single 420x900 screen, stripe tokens (design-systems/stripe/
design-tokens.json). Header (kicker + title + remaining count), an "add from
candidate pool" card, vertical auto-layout list with one TaskRow template
(cloned by ui.bindList), footer gesture hint. Re-run: python gen_design.py
"""
import json
import os

# stripe tokens (design-systems/stripe/design-tokens.json)
BG = "#ffffff"            # --bg
SURFACE_WARM = "#f6f9fc"  # --surface-warm
FG = "#061b31"            # --fg
FG2 = "#273951"           # --fg-2
MUTED = "#64748d"         # --muted
BORDER = "#e5edf5"        # --border
ACCENT = "#533afd"        # --accent
ACCENT_ON = "#ffffff"     # --accent-on
R_MD = 6                  # --radius-md
R_LG = 8                  # --radius-lg
FONT = "Segoe UI"         # present on Windows; CJK falls back per-glyph

W, H = 420, 900
GUTTER = 24               # --space-6
CONTENT_W = W - 2 * GUTTER  # 372

_next_id = [1]

def nid():
    _next_id[0] += 1
    return f"30:{_next_id[0]}"

def hex_rgba(h, a=1.0):
    h = h.lstrip("#")
    return {"r": int(h[0:2], 16) / 255, "g": int(h[2:4], 16) / 255,
            "b": int(h[4:6], 16) / 255, "a": a}

def solid(c, a=1.0):
    return {"type": "SOLID", "color": hex_rgba(c, a)}

def xf(x, y):
    return [[1, 0, x], [0, 1, y]]

def text(name, x, y, w, h, chars, size, color, weight=400, align="LEFT",
         valign="TOP", tracking=0, visible=True):
    return {"id": nid(), "name": name, "type": "TEXT", "visible": visible,
            "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
            "characters": chars, "fills": [solid(color)],
            "style": {"fontFamily": FONT, "fontWeight": weight, "fontSize": size,
                      "textAlignHorizontal": align, "textAlignVertical": valign,
                      "letterSpacing": tracking}}

def frame(name, x, y, w, h, fill=None, radius=0, stroke=None, stroke_w=0,
          children=None, visible=True, extra=None):
    n = {"id": nid(), "name": name, "type": "FRAME", "visible": visible,
         "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
         "clipsContent": False, "cornerRadius": radius,
         "fills": [solid(fill)] if fill else [],
         "children": children or []}
    if stroke:
        n["strokes"] = [solid(stroke)]
        n["strokeWeight"] = stroke_w
    if extra:
        n.update(extra)
    return n

def ellipse(name, x, y, d, fill=None, stroke=None, stroke_w=0, visible=True):
    n = {"id": nid(), "name": name, "type": "ELLIPSE", "visible": visible,
         "size": {"x": d, "y": d}, "relativeTransform": xf(x, y),
         "fills": [solid(fill)] if fill else []}
    if stroke:
        n["strokes"] = [solid(stroke)]
        n["strokeWeight"] = stroke_w
    return n

# ---- header: kicker + title + remaining count ----
children = [
    text("Kicker", GUTTER, 56, 200, 18, "今日待办", 13, ACCENT, weight=700,
         tracking=1.5),
    text("Title", GUTTER, 78, 280, 42, "待办清单", 32, FG, weight=700,
         tracking=-0.6),
    text("RemainLabel", GUTTER, 128, CONTENT_W, 20, "还剩 5 项", 14, MUTED),
]

# ---- add card: next candidate preview + Add button ----
children.append(frame(
    "AddCard", GUTTER, 164, CONTENT_W, 64, fill=SURFACE_WARM, radius=R_LG,
    children=[
        text("NextHint", 16, 11, 200, 16, "接下来", 12, MUTED),
        text("NextLabel", 16, 30, 256, 20, "给妈妈回个电话", 14, FG2, weight=600),
        frame("AddBtn", 292, 16, 64, 32, fill=ACCENT, radius=R_MD, children=[
            text("AddBtn Label", 0, 0, 64, 32, "添加", 14, ACCENT_ON,
                 weight=700, align="CENTER", valign="CENTER")]),
    ]))

# ---- task list: vertical auto-layout container + one row template ----
ROW_H = 64
row_children = [
    ellipse("CheckOff", 20, 21, 22, fill=BG, stroke=MUTED, stroke_w=1.5),
    frame("CheckOn", 20, 21, 22, 22, fill=ACCENT, radius=11, visible=False,
          children=[text("CheckOn Glyph", 0, 0, 22, 22, "✓", 12, ACCENT_ON,
                         weight=700, align="CENTER", valign="CENTER")]),
    text("TaskText", 58, 0, 294, ROW_H, "任务", 16, FG, valign="CENTER"),
    text("TaskTextDone", 58, 0, 294, ROW_H, "任务", 16, MUTED, valign="CENTER",
         visible=False),
]
task_row = frame("TaskRow", 0, 0, CONTENT_W, ROW_H, fill=BG, radius=R_LG,
                 stroke=BORDER, stroke_w=1, children=row_children)

task_list = frame(
    "TaskList", GUTTER, 252, CONTENT_W, 8 * ROW_H + 7 * 10, children=[task_row],
    extra={"layoutMode": "VERTICAL", "itemSpacing": 10,
           "primaryAxisSizingMode": "AUTO", "counterAxisSizingMode": "FIXED",
           "paddingLeft": 0, "paddingRight": 0, "paddingTop": 0,
           "paddingBottom": 0})
children.append(task_list)

# ---- footer gesture hint ----
children.append(text("GestureHint", GUTTER, 856, CONTENT_W, 16,
                     "点按完成 · 左滑删除 · 长按置顶", 12, MUTED))

doc = {
    "name": "Todo",
    "document": {"id": "30:0", "name": "Document", "type": "DOCUMENT", "children": [
        {"id": "30:1", "name": "Page 1", "type": "CANVAS", "children": [
            frame("Todo", 0, 0, W, H, fill=BG,
                  children=children, extra={"clipsContent": True})]}]},
}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "design.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(doc, f, ensure_ascii=False, indent=1)
print(f"wrote {out}")
