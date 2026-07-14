#!/usr/bin/env python3
"""Generate design.json for the calendar benchmark app.

framer tokens (design-systems/framer/design-tokens.json): pure-black canvas,
one accent (#0099ff), near-black surfaces with soft hairlines, white primary
button, Segoe UI. Two frames: "Month" (title + weekday header + 6x7 month
grid + day agenda) and "NewEvent" (title input + hour/minute snap wheels).

Month-grid layout notes (the G14 interaction, learned the hard way):
- The grid is ONE bindList of 42 cells in a HORIZONTAL auto-layout with
  layoutWrap=WRAP. bindList forces the primary axis to HUG, and hug
  measurement of a WRAP container uses its NO-WRAP extent (42*53 px) — so the
  grid pins itself with minWidth=maxWidth=371: setSize clamps the hugged
  width back to 371 and stackLayout wraps into 6 rows of 7.
- Each cell is a PLAIN frame (no auto-layout): the dot / today ring /
  selection disc inside are absolutely positioned, so setVisible on them can
  never collapse a flow slot (G14 hides flow children from auto-layout).
  Cells themselves are always visible, so the 42 flow slots are stable.
- Cells carry an alpha-0 fill: hitTestFrame only hits paintable nodes, and a
  tap must land on the cell even between its children.

Wheel picker: the alarm recipe verbatim — FIXED scrolling frame, row pitch
56, viewport 280, paddingTop 112 (2 lead-in rows), 2 PHANTOM tail rows from
JS instead of paddingBottom (G15: trailing padding is outside the scroll
extent), alpha-0 row fills, selection band behind the wheels.

Re-run to regenerate: python gen_design.py
"""
import json
import os

# framer tokens
BG = "#000000"           # --bg
SURFACE = "#090909"      # --surface
FG = "#ffffff"           # --fg
FG2 = "#a6a6a6"          # --fg-2
MUTED_A = 0.6            # --muted = white @ .6
META_A = 0.4             # --meta  = white @ .4
HAIR_A = 0.06            # --border-soft = white @ .06
ACCENT = "#0099ff"       # --accent
BLACK = "#000000"
RADIUS = 12              # --radius-md, the one card radius
FONT = "Segoe UI"

W, H = 420, 900
GUTTER = 24              # --space-6
CONTENT_W = W - 2 * GUTTER  # 372

# month grid
COLS, ROWS = 7, 6
CELL_W, CELL_H = 53, 58
GRID_W = COLS * CELL_W          # 371
GRID_H = ROWS * CELL_H          # 348
GRID_X = (W - GRID_W) // 2      # 24
GRID_Y = 152
DISC = 40                        # today ring / selection disc diameter

# agenda
LIST_Y = 564
ROW_H = 56
LIST_H = 4 * ROW_H + 3 * 8      # 248: four rows, then it scrolls

# wheel picker (alarm recipe)
WHEEL_ROW = 56
WHEEL_VIS = 5
WHEEL_H = WHEEL_ROW * WHEEL_VIS  # 280 viewport
WHEEL_PAD = 2 * WHEEL_ROW        # 112 lead-in
WHEEL_W = 120
WHEEL_Y = 420
HOUR_X, MIN_X = 70, 230
BAND_Y = WHEEL_Y + WHEEL_PAD     # 532 center row

_next_id = [1]

def nid():
    _next_id[0] += 1
    return f"40:{_next_id[0]}"

def hex_rgba(h, a=1.0):
    h = h.lstrip("#")
    return {"r": int(h[0:2], 16) / 255, "g": int(h[2:4], 16) / 255,
            "b": int(h[4:6], 16) / 255, "a": a}

def solid(c, a=1.0):
    return {"type": "SOLID", "color": hex_rgba(c, a)}

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

def rect(name, x, y, w, h, fill, radius=0, alpha=1.0, visible=True):
    return {"id": nid(), "name": name, "type": "RECTANGLE", "visible": visible,
            "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
            "cornerRadius": radius, "fills": [solid(fill, alpha)]}

def ellipse(name, x, y, d, fill=None, fill_alpha=1.0, stroke=None, stroke_w=0,
            visible=True):
    n = {"id": nid(), "name": name, "type": "ELLIPSE", "visible": visible,
         "size": {"x": d, "y": d}, "relativeTransform": xf(x, y),
         "fills": [solid(fill, fill_alpha)] if fill else []}
    if stroke:
        n["strokes"] = [solid(stroke)]
        n["strokeWeight"] = stroke_w
    return n

def frame(name, x, y, w, h, fill=None, fill_alpha=1.0, radius=0, children=None,
          clips=False, visible=True, stroke=None, stroke_a=1.0, extra=None):
    n = {"id": nid(), "name": name, "type": "FRAME", "visible": visible,
         "size": {"x": w, "y": h}, "relativeTransform": xf(x, y),
         "clipsContent": clips, "cornerRadius": radius,
         "fills": [solid(fill, fill_alpha)] if fill else [],
         "children": children or []}
    if stroke:
        n["strokes"] = [solid(stroke, stroke_a)]
        n["strokeWeight"] = 1
    if extra:
        n.update(extra)
    return n

# ---- Month frame ----

def day_cell():
    """bindList template. Plain frame: children are absolute -> setVisible on
    dot/ring/disc never reflows the grid (G14-safe). alpha-0 fill = hittable."""
    cx = (CELL_W - DISC) / 2  # 6.5
    return frame("DayCell", 0, 0, CELL_W, CELL_H, fill=FG, fill_alpha=0.0,
                 children=[
        # selection disc (white) under the number; pre-baked number pair swaps
        # white-on-black <-> black-on-white so no runtime fill changes needed
        frame("SelBg", cx, 6, DISC, DISC, fill=FG, radius=DISC / 2,
              visible=False),
        ellipse("TodayRing", cx, 6, DISC, stroke=ACCENT, stroke_w=2,
                visible=False),
        text("DayNum", 0, 6, CELL_W, DISC, "1", 15, FG,
             align="CENTER", valign="CENTER"),
        text("DayNumSel", 0, 6, CELL_W, DISC, "1", 15, BLACK, weight=600,
             align="CENTER", valign="CENTER", visible=False),
        ellipse("Dot", (CELL_W - 5) / 2, 49, 5, fill=ACCENT, visible=False),
    ])

month_grid = frame("MonthGrid", GRID_X, GRID_Y, GRID_W, GRID_H,
                   children=[day_cell()], extra={
    "layoutMode": "HORIZONTAL", "layoutWrap": "WRAP", "itemSpacing": 0,
    "primaryAxisSizingMode": "FIXED", "counterAxisSizingMode": "FIXED",
    # bindList re-hugs the primary axis with the NO-WRAP extent (42 cells
    # wide); the clamp snaps it back so the stack wraps at 7 columns.
    "minWidth": GRID_W, "maxWidth": GRID_W})

def event_row():
    """Agenda bindList template: time (accent) + title on a soft card."""
    return frame("EvRow", 0, 0, CONTENT_W, ROW_H, fill=SURFACE, radius=RADIUS,
                 stroke=FG, stroke_a=HAIR_A, children=[
        text("EvTime", 16, 0, 60, ROW_H, "10:00", 14, ACCENT, weight=600,
             valign="CENTER"),
        text("EvTitle", 92, 0, CONTENT_W - 108, ROW_H, "组会", 15, FG,
             valign="CENTER"),
    ])

agenda_list = frame("AgendaList", GUTTER, LIST_Y, CONTENT_W, LIST_H,
                    clips=True, children=[event_row()], extra={
    "overflowDirection": "VERTICAL_SCROLLING",
    "layoutMode": "VERTICAL", "itemSpacing": 8,
    "primaryAxisSizingMode": "FIXED",  # scroll viewport, not hug
    "counterAxisSizingMode": "FIXED"})

WEEKDAYS = ["一", "二", "三", "四", "五", "六", "日"]  # Monday-first

month_children = [
    text("MonthTitle", GUTTER, 56, 300, 44, "2026年7月", 32, FG, weight=600,
         tracking=-0.8),
    *[text(f"Wd{i}", GRID_X + i * CELL_W, 124, CELL_W, 16, WEEKDAYS[i], 12, FG,
           alpha=META_A, align="CENTER") for i in range(COLS)],
    month_grid,
    rect("Divider", GUTTER, 512, CONTENT_W, 1, FG, alpha=HAIR_A),
    text("AgendaTitle", GUTTER, 526, 220, 26, "7月2日", 18, FG, weight=600),
    text("AgendaCount", 220, 531, 176, 18, "", 12, FG, alpha=MUTED_A,
         align="RIGHT"),
    agenda_list,
    text("EmptyState", GUTTER, 640, CONTENT_W, 20, "这一天还没有日程", 14, FG,
         alpha=MUTED_A, align="CENTER", visible=False),
    frame("AddBtn", GUTTER, 826, CONTENT_W, 54, fill=FG, radius=27, children=[
        text("AddBtnLabel", 0, 0, CONTENT_W, 54, "+ 添加日程", 15, BLACK,
             weight=600, align="CENTER", valign="CENTER")]),
]

# ---- NewEvent frame ----

def wheel(name, x, item_name, text_name, sample):
    """Alarm recipe: FIXED scrolling frame + alpha-0 rows over the band."""
    item = frame(item_name, 0, 0, WHEEL_W, WHEEL_ROW, fill=FG, fill_alpha=0.0,
                 children=[
                     text(text_name, 0, 0, WHEEL_W, WHEEL_ROW, sample, 22, FG,
                          align="CENTER", valign="CENTER")])
    w = frame(name, x, WHEEL_Y, WHEEL_W, WHEEL_H, clips=True, children=[item])
    w["overflowDirection"] = "VERTICAL_SCROLLING"
    w["layoutMode"] = "VERTICAL"
    w["itemSpacing"] = 0
    w["paddingTop"] = WHEEL_PAD
    # no paddingBottom (G15) — the app appends two phantom rows instead
    w["primaryAxisSizingMode"] = "FIXED"   # REQUIRED or bindList -> maxScroll 0
    w["counterAxisSizingMode"] = "FIXED"
    return w

new_event_children = [
    frame("BtnCancel", 16, 56, 80, 44, fill=FG, fill_alpha=0.0, children=[
        text("BtnCancelLabel", 8, 0, 72, 44, "取消", 15, ACCENT,
             valign="CENTER")]),
    text("PageTitle", 110, 56, 200, 44, "新建日程", 15, FG, weight=600,
         align="CENTER", valign="CENTER"),
    frame("BtnSave", W - 16 - 80, 56, 80, 44, fill=FG, fill_alpha=0.0, children=[
        text("BtnSaveLabel", 0, 0, 72, 44, "保存", 15, ACCENT, weight=600,
             align="RIGHT", valign="CENTER")]),
    text("TitleCaption", GUTTER, 130, 120, 16, "标题", 12, FG, alpha=MUTED_A),
    frame("InputBox", GUTTER, 152, CONTENT_W, 54, fill=SURFACE, radius=RADIUS,
          stroke=FG, stroke_a=HAIR_A, children=[
        text("TitlePlaceholder", 16, 0, CONTENT_W - 32, 54, "输入标题", 15, FG,
             alpha=META_A, valign="CENTER"),
        text("TitleInput", 16, 0, CONTENT_W - 32, 54, "", 15, FG,
             valign="CENTER")]),
    text("TimeCaption", GUTTER, 240, 120, 16, "时间", 12, FG, alpha=MUTED_A),
    text("PreviewTime", GUTTER, 262, CONTENT_W, 80, "09:00", 62, FG,
         weight=600, align="CENTER", tracking=-3),
    # selection band sits BEHIND the (alpha-0 filled) wheels
    rect("SelectionBand", 40, BAND_Y, W - 80, WHEEL_ROW, FG, radius=RADIUS,
         alpha=HAIR_A),
    wheel("HourWheel", HOUR_X, "HourItem", "HourText", "09"),
    text("Colon", 190, BAND_Y, 40, WHEEL_ROW, ":", 22, FG,
         align="CENTER", valign="CENTER"),
    wheel("MinuteWheel", MIN_X, "MinuteItem", "MinuteText", "00"),
    text("WheelHint", GUTTER, 724, CONTENT_W, 16, "上下滑动选择时间", 12, FG,
         alpha=MUTED_A, align="CENTER"),
]

doc = {
    "name": "Calendar",
    "document": {"id": "40:0", "name": "Document", "type": "DOCUMENT", "children": [
        {"id": "40:1", "name": "Page 1", "type": "CANVAS", "children": [
            {"id": nid(), "name": "Month", "type": "FRAME",
             "size": {"x": W, "y": H}, "relativeTransform": xf(0, 0),
             "clipsContent": True, "fills": [solid(BG)],
             "children": month_children},
            {"id": nid(), "name": "NewEvent", "type": "FRAME",
             "size": {"x": W, "y": H}, "relativeTransform": xf(W + 40, 0),
             "clipsContent": True, "fills": [solid(BG)],
             "children": new_event_children}]}]},
}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "design.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(doc, f, ensure_ascii=False, indent=1)
print(f"wrote {out}")
