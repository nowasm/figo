# -*- coding: utf-8 -*-
"""Generate design-systems/components/m3-components.figo.json — figo's
Material 3 component library template.

Masters live on the "Components" page as COMPONENT_SETs (variant names follow
the runtime convention "Prop=Value, Prop=Value" so ui.setVariant/autoStates
work out of the box). Every fill/stroke binds a theme variable (colorVar), so
ui.setThemeMode / figoTheme.apply retint the whole library. The "Preview" page
holds one frame with an instance of everything for screenshot review.

Usage: python tools/gen_components.py   (rewrites the .figo.json in place)
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "design-systems", "components", "m3-components.figo.json")
PALETTE = os.path.join(ROOT, "design-systems", "components", "m3-palette.json")

# Roboto ships with the library (design-systems/components/fonts, Apache 2.0)
# — point app.json "fonts" at that dir. Weights available: 400/500/700.
FONT = "Roboto"

_ids = {}


def nid(prefix):
    _ids[prefix] = _ids.get(prefix, 0) + 1
    return "%s:%d" % (prefix, _ids[prefix])


def hex_to_rgba(h):
    h = h.lstrip("#")
    v = [int(h[i:i + 2], 16) / 255.0 for i in range(0, len(h), 2)]
    if len(v) == 3:
        v.append(1.0)
    return {"r": v[0], "g": v[1], "b": v[2], "a": v[3]}


with open(PALETTE, encoding="utf-8") as f:
    PAL = json.load(f)


def fill(var, opacity=None):
    """A SOLID paint bound to a theme variable, literal = light value."""
    p = {"type": "SOLID", "color": hex_to_rgba(PAL["light"][var]), "colorVar": var}
    if opacity is not None:
        p["opacity"] = opacity
    return p


def node(type_, name, x, y, w, h, **kw):
    n = {
        "id": kw.pop("id", nid(name.lower().replace(" ", "-").replace("=", "").replace(",", ""))),
        "name": name,
        "type": type_,
        "size": {"x": w, "y": h},
        "relativeTransform": [[1, 0, x], [0, 1, y]],
    }
    n.update(kw)
    return n


def text(name, x, y, w, h, chars, size, weight, color_var, align="LEFT", valign="CENTER"):
    return node(
        "TEXT", name, x, y, w, h,
        fills=[fill(color_var)],
        characters=chars,
        style={
            "fontFamily": FONT, "fontSize": size, "fontWeight": weight,
            "textAlignHorizontal": align, "textAlignVertical": valign,
        },
    )


def state_overlay(w, h, radius, on_var, state):
    """M3 state layer: an on-color wash at 8% (hover) / 10% (pressed)."""
    op = 0.08 if state == "Hover" else 0.10
    return node("RECTANGLE", "State Layer", 0, 0, w, h,
                fills=[fill(on_var, op)], cornerRadius=radius)


# ---- Buttons ----------------------------------------------------------------
# Type=Filled|Elevated|Tonal|Outlined|Text x State=Default|Hover|Pressed.
# 40dp high, full radius, centered 14/500 label (Sober button spec).
BTN_W, BTN_H, BTN_R = 120, 40, 20
BTN_STYLES = {
    "Filled":   {"bg": "primary", "label": "on-primary"},
    "Elevated": {"bg": "surface-container-low", "label": "primary", "shadow": True},
    "Tonal":    {"bg": "secondary-container", "label": "on-secondary-container"},
    "Outlined": {"bg": None, "label": "primary", "stroke": "outline"},
    "Text":     {"bg": None, "label": "primary"},
}


def button_variant(btype, state, x, y):
    st = BTN_STYLES[btype]
    kw = {"cornerRadius": BTN_R, "clipsContent": True}
    kw["fills"] = [fill(st["bg"])] if st["bg"] else [fill("primary", 0.0)]
    if "stroke" in st:
        kw["strokes"] = [fill(st["stroke"])]
        kw["strokeWeight"] = 1.0
        kw["strokeAlign"] = "INSIDE"
    if st.get("shadow"):  # Sober elevated: level-1 elevation
        kw["effects"] = [{"type": "DROP_SHADOW",
                          "color": {"r": 0, "g": 0, "b": 0, "a": 0.22},
                          "offset": {"x": 0, "y": 1}, "radius": 3}]
    children = []
    if state != "Default":
        children.append(state_overlay(BTN_W, BTN_H, BTN_R, st["label"], state))
    children.append(text("Label", 0, 0, BTN_W, BTN_H, btype + " Button" if btype in ("Filled",) else btype,
                         14, 500, st["label"], align="CENTER"))
    c = node("COMPONENT", "Type=%s, State=%s" % (btype, state), x, y, BTN_W, BTN_H, **kw)
    c["children"] = children
    return c


def button_set(x, y):
    variants, col, row = [], 0, 0
    for btype in BTN_STYLES:
        for state in ("Default", "Hover", "Pressed"):
            variants.append(button_variant(btype, state, 20 + col * (BTN_W + 20), 20 + row * (BTN_H + 16)))
            col += 1
        col = 0
        row += 1
    rows = len(BTN_STYLES)
    s = node("COMPONENT_SET", "Button", x, y, 20 * 2 + 3 * BTN_W + 2 * 20,
             20 * 2 + rows * BTN_H + (rows - 1) * 16, fills=[fill("surface", 0.0)])
    s["children"] = variants
    return s


# ---- Switch -------------------------------------------------------------------
SW_W, SW_H = 52, 32


def switch_variant(on, x, y):
    # Sober/M3: off = transparent track + 2px outline border, 16dp outline
    # thumb; on = primary track, 24dp on-primary thumb.
    if on:
        track_kw = {"fills": [fill("primary")], "cornerRadius": 16}
        thumb = node("ELLIPSE", "Thumb", SW_W - 4 - 24, 4, 24, 24, fills=[fill("on-primary")])
    else:
        track_kw = {"fills": [fill("surface", 0.0)],
                    "strokes": [fill("outline")], "strokeWeight": 2.0,
                    "strokeAlign": "INSIDE", "cornerRadius": 16}
        thumb = node("ELLIPSE", "Thumb", 8, 8, 16, 16, fills=[fill("outline")])
    c = node("COMPONENT", "State=%s" % ("On" if on else "Off"), x, y, SW_W, SW_H, **track_kw)
    c["children"] = [thumb]
    return c


def switch_set(x, y):
    s = node("COMPONENT_SET", "Switch", x, y, 20 * 2 + SW_W * 2 + 20, 20 * 2 + SW_H,
             fills=[fill("surface", 0.0)])
    s["children"] = [switch_variant(False, 20, 20), switch_variant(True, 20 + SW_W + 20, 20)]
    return s


# ---- Card ---------------------------------------------------------------------
CARD_W, CARD_H = 380, 120


def card_component(x, y):
    # Sober elevated card: surface-container-low + level-1 shadow.
    c = node("COMPONENT", "Card", x, y, CARD_W, CARD_H,
             fills=[fill("surface-container-low")], cornerRadius=12, clipsContent=True,
             effects=[{"type": "DROP_SHADOW", "color": {"r": 0, "g": 0, "b": 0, "a": 0.22},
                       "offset": {"x": 0, "y": 1}, "radius": 3}])
    c["children"] = [
        text("Title", 16, 16, CARD_W - 32, 24, "Card title", 16, 500, "on-surface", valign="TOP"),
        text("Body", 16, 44, CARD_W - 32, 60,
             "Supporting text for the card. Bind your own data here.",
             14, 400, "on-surface-variant", valign="TOP"),
    ]
    return c


# ---- List row -------------------------------------------------------------------
ROW_W, ROW_H = 380, 64


def listrow_component(x, y):
    c = node("COMPONENT", "List Row", x, y, ROW_W, ROW_H,
             fills=[fill("surface", 0.0)])  # alpha-0: hit-testable everywhere
    c["children"] = [
        node("ELLIPSE", "Leading", 16, 12, 40, 40, fills=[fill("primary-container")]),
        text("Title", 72, 10, ROW_W - 72 - 16, 24, "List item title", 15, 500, "on-surface", valign="BOTTOM"),
        text("Subtitle", 72, 34, ROW_W - 72 - 16, 20, "Supporting text", 13, 400, "on-surface-variant", valign="TOP"),
    ]
    return c


# ---- Text field (outlined) ------------------------------------------------------
TF_W, TF_H = 380, 48  # Sober text-field: min-height 48, radius 4


def textfield_variant(focused, x, y):
    stroke_var = "primary" if focused else "outline"
    c = node("COMPONENT", "State=%s" % ("Focused" if focused else "Default"),
             x, y, TF_W, TF_H,
             fills=[fill("surface", 0.0)],
             strokes=[fill(stroke_var)], strokeWeight=2.0 if focused else 1.0,
             strokeAlign="INSIDE", cornerRadius=4)
    c["children"] = [
        text("Label", 16, 2, TF_W - 32, 14, "Label", 11, 500,
             "primary" if focused else "on-surface-variant", valign="BOTTOM"),
        text("Value", 16, 18, TF_W - 32, 24, "Input text", 14, 400, "on-surface", valign="CENTER"),
    ]
    return c


def textfield_set(x, y):
    s = node("COMPONENT_SET", "Text Field", x, y, 20 * 2 + TF_W, 20 * 2 + TF_H * 2 + 16,
             fills=[fill("surface", 0.0)])
    s["children"] = [textfield_variant(False, 20, 20), textfield_variant(True, 20, 20 + TF_H + 16)]
    return s


# ---- Chip -----------------------------------------------------------------------
CHIP_W, CHIP_H = 110, 32


def chip_variant(selected, x, y):
    # Sober chip: full pill (r16), default = filled surface-container-high.
    if selected:
        kw = {"fills": [fill("secondary-container")], "cornerRadius": 16}
        label_var = "on-secondary-container"
    else:
        kw = {"fills": [fill("surface-container-high")], "cornerRadius": 16}
        label_var = "on-surface"
    c = node("COMPONENT", "State=%s" % ("Selected" if selected else "Default"),
             x, y, CHIP_W, CHIP_H, **kw)
    c["children"] = [text("Label", 0, 0, CHIP_W, CHIP_H, "Chip", 13, 500, label_var, align="CENTER")]
    return c


def chip_set(x, y):
    s = node("COMPONENT_SET", "Chip", x, y, 20 * 2 + CHIP_W * 2 + 20, 20 * 2 + CHIP_H,
             fills=[fill("surface", 0.0)])
    s["children"] = [chip_variant(False, 20, 20), chip_variant(True, 20 + CHIP_W + 20, 20)]
    return s


# ---- Slider ---------------------------------------------------------------------
SL_W, SL_H = 380, 44


def slider_component(x, y):
    # bindSlider-ready: track = the component root, children named Fill/Knob.
    # Sober slider: 4dp secondary-container track, 16dp elevated primary thumb.
    c = node("COMPONENT", "Slider", x, y, SL_W, SL_H, fills=[fill("surface", 0.0)])
    c["children"] = [
        node("RECTANGLE", "Track", 0, 20, SL_W, 4,
             fills=[fill("secondary-container")], cornerRadius=2),
        node("RECTANGLE", "Fill", 0, 20, SL_W * 0.5, 4, fills=[fill("primary")], cornerRadius=2),
        node("ELLIPSE", "Knob", SL_W * 0.5 - 8, 14, 16, 16, fills=[fill("primary")],
             effects=[{"type": "DROP_SHADOW", "color": {"r": 0, "g": 0, "b": 0, "a": 0.25},
                       "offset": {"x": 0, "y": 1}, "radius": 3}]),
    ]
    return c


# ---- FAB ------------------------------------------------------------------------
def fab_component(x, y):
    # Sober FAB: radius 28 (full round), primary-container, level-3 elevation.
    c = node("COMPONENT", "FAB", x, y, 56, 56,
             fills=[fill("primary-container")], cornerRadius=28,
             effects=[{"type": "DROP_SHADOW", "color": {"r": 0, "g": 0, "b": 0, "a": 0.3},
                       "offset": {"x": 0, "y": 4}, "radius": 10}])
    c["children"] = [text("Icon", 0, 0, 56, 56, "+", 26, 500, "on-primary-container", align="CENTER")]
    return c


# ---- Top app bar ------------------------------------------------------------------
def appbar_component(x, y):
    c = node("COMPONENT", "Top App Bar", x, y, 420, 64, fills=[fill("surface")])
    c["children"] = [
        text("Title", 16, 0, 300, 64, "Page title", 20, 500, "on-surface"),
        text("Action", 420 - 56, 0, 40, 64, "⋮", 20, 500, "on-surface-variant", align="CENTER"),
    ]
    return c


# ---- Navigation bar item -----------------------------------------------------------
NAV_W, NAV_H = 84, 64


def navitem_variant(selected, x, y):
    c = node("COMPONENT", "State=%s" % ("Selected" if selected else "Default"),
             x, y, NAV_W, NAV_H, fills=[fill("surface", 0.0)])
    pill_fill = fill("secondary-container") if selected else fill("surface", 0.0)
    icon_var = "on-secondary-container" if selected else "on-surface-variant"
    label_var = "on-surface" if selected else "on-surface-variant"
    c["children"] = [
        node("RECTANGLE", "Pill", (NAV_W - 56) / 2, 4, 56, 32, fills=[pill_fill], cornerRadius=16),
        text("Icon", 0, 4, NAV_W, 32, "●", 13, 400, icon_var, align="CENTER"),
        text("Label", 0, 38, NAV_W, 18, "Tab", 12, 500 if selected else 400, label_var, align="CENTER"),
    ]
    return c


def navitem_set(x, y):
    s = node("COMPONENT_SET", "Nav Item", x, y, 20 * 2 + NAV_W * 2 + 20, 20 * 2 + NAV_H,
             fills=[fill("surface", 0.0)])
    s["children"] = [navitem_variant(False, 20, 20), navitem_variant(True, 20 + NAV_W + 20, 20)]
    return s


# ---- Checkbox ------------------------------------------------------------------------
CB = 24  # touch box; the visual box is 18dp centered


def checkbox_variant(checked, x, y):
    c = node("COMPONENT", "State=%s" % ("Checked" if checked else "Unchecked"),
             x, y, CB, CB, fills=[fill("surface", 0.0)])
    if checked:
        box = node("RECTANGLE", "Box", 3, 3, 18, 18, fills=[fill("primary")], cornerRadius=2)
        mark = text("Mark", 3, 3, 18, 18, "✓", 13, 700, "on-primary", align="CENTER")
        c["children"] = [box, mark]
    else:
        box = node("RECTANGLE", "Box", 3, 3, 18, 18,
                   fills=[fill("surface", 0.0)], strokes=[fill("on-surface-variant")],
                   strokeWeight=2.0, strokeAlign="INSIDE", cornerRadius=2)
        c["children"] = [box]
    return c


def checkbox_set(x, y):
    s = node("COMPONENT_SET", "Checkbox", x, y, 20 * 2 + CB * 2 + 20, 20 * 2 + CB,
             fills=[fill("surface", 0.0)])
    s["children"] = [checkbox_variant(False, 20, 20), checkbox_variant(True, 20 + CB + 20, 20)]
    return s


# ---- Radio button ----------------------------------------------------------------------
def radio_variant(selected, x, y):
    c = node("COMPONENT", "State=%s" % ("Selected" if selected else "Default"),
             x, y, CB, CB, fills=[fill("surface", 0.0)])
    ring_var = "primary" if selected else "on-surface-variant"
    ring = node("ELLIPSE", "Ring", 2, 2, 20, 20,
                fills=[fill("surface", 0.0)], strokes=[fill(ring_var)],
                strokeWeight=2.0, strokeAlign="INSIDE")
    c["children"] = [ring]
    if selected:
        c["children"].append(node("ELLIPSE", "Dot", 7, 7, 10, 10, fills=[fill("primary")]))
    return c


def radio_set(x, y):
    s = node("COMPONENT_SET", "Radio", x, y, 20 * 2 + CB * 2 + 20, 20 * 2 + CB,
             fills=[fill("surface", 0.0)])
    s["children"] = [radio_variant(False, 20, 20), radio_variant(True, 20 + CB + 20, 20)]
    return s


# ---- Segmented button -------------------------------------------------------------------
# One segment per instance; Position picks the end rounding so a row of
# Start+Middle+End instances reads as one M3 segmented control.
SEG_W, SEG_H, SEG_R = 110, 40, 20


def segment_variant(position, selected, x, y):
    # Sober segmented-button: one surface-container capsule (outline-variant
    # border, r20) — the selected item is an inset primary pill (3dp padding).
    radii = {"Start": [SEG_R, 0, 0, SEG_R], "Middle": [0, 0, 0, 0],
             "End": [0, SEG_R, SEG_R, 0]}[position]
    kw = {"rectangleCornerRadii": radii, "fills": [fill("surface-container")],
          "strokes": [fill("outline-variant")], "strokeWeight": 1.0,
          "strokeAlign": "INSIDE"}
    c = node("COMPONENT", "Position=%s, State=%s" % (position, "Selected" if selected else "Default"),
             x, y, SEG_W, SEG_H, **kw)
    label_var = "on-primary" if selected else "on-surface"
    c["children"] = []
    if selected:
        pill_radii = {"Start": [SEG_R - 3, 4, 4, SEG_R - 3], "Middle": [4, 4, 4, 4],
                      "End": [4, SEG_R - 3, SEG_R - 3, 4]}[position]
        c["children"].append(node("RECTANGLE", "Indicator", 3, 3, SEG_W - 6, SEG_H - 6,
                                  fills=[fill("primary")],
                                  rectangleCornerRadii=pill_radii))
    c["children"].append(text("Label", 0, 0, SEG_W, SEG_H, "Label", 13, 500,
                              label_var, align="CENTER"))
    return c


def segment_set(x, y):
    variants = []
    for row, position in enumerate(("Start", "Middle", "End")):
        for col, selected in enumerate((False, True)):
            variants.append(segment_variant(position, selected,
                                            20 + col * (SEG_W + 20), 20 + row * (SEG_H + 12)))
    s = node("COMPONENT_SET", "Segment", x, y, 20 * 2 + SEG_W * 2 + 20,
             20 * 2 + SEG_H * 3 + 12 * 2, fills=[fill("surface", 0.0)])
    s["children"] = variants
    return s


# ---- Search bar ----------------------------------------------------------------------
SEARCH_W, SEARCH_H = 380, 40  # Sober search: min-height 40, r20


def search_component(x, y):
    c = node("COMPONENT", "Search Bar", x, y, SEARCH_W, SEARCH_H,
             fills=[fill("surface-container-low")],
             strokes=[fill("outline-variant")], strokeWeight=1.0, strokeAlign="INSIDE",
             cornerRadius=20, clipsContent=True)
    # magnifier glyph: stroked circle + 45° handle (no font dependency)
    lens = node("ELLIPSE", "Lens", 16, 13, 11, 11,
                fills=[fill("surface", 0.0)], strokes=[fill("on-surface-variant")],
                strokeWeight=2.0, strokeAlign="CENTER")
    d = 0.7071  # cos/sin 45°
    handle = node("RECTANGLE", "Handle", 0, 0, 6, 2, fills=[fill("on-surface-variant")])
    handle["relativeTransform"] = [[d, -d, 25.8], [d, d, 22.6]]
    c["children"] = [
        lens, handle,
        text("Placeholder", 40, 0, SEARCH_W - 56, SEARCH_H, "Search", 14, 400,
             "on-surface-variant"),
    ]
    return c


# ---- Divider / Badge / Avatar ---------------------------------------------------------
def divider_component(x, y):
    return node("COMPONENT", "Divider", x, y, 380, 1, fills=[fill("outline-variant")])


def badge_variant(with_count, x, y):
    if with_count:
        c = node("COMPONENT", "Type=Count", x, y, 20, 16, fills=[fill("error")], cornerRadius=8)
        c["children"] = [text("Count", 0, 0, 20, 16, "3", 10, 500, "on-error", align="CENTER")]
    else:
        c = node("COMPONENT", "Type=Dot", x, y, 8, 8, fills=[fill("error")], cornerRadius=4)
        c["children"] = []
    return c


def badge_set(x, y):
    s = node("COMPONENT_SET", "Badge", x, y, 20 * 2 + 22 + 20 + 8, 20 * 2 + 16,
             fills=[fill("surface", 0.0)])
    s["children"] = [badge_variant(False, 20, 24), badge_variant(True, 48, 20)]
    return s


def avatar_component(x, y):
    # Sober avatar: tertiary ground, 16px semibold initials.
    c = node("COMPONENT", "Avatar", x, y, 40, 40, fills=[fill("tertiary")],
             cornerRadius=20, clipsContent=True)
    c["children"] = [text("Initials", 0, 0, 40, 40, "A", 16, 500,
                          "on-tertiary", align="CENTER")]
    return c


# ---- Progress -----------------------------------------------------------------------
def linear_progress_component(x, y):
    # bindSlider({readonly:true})-ready: root = track, child named Fill.
    c = node("COMPONENT", "Linear Progress", x, y, 380, 4,
             fills=[fill("secondary-container")], cornerRadius=2)
    c["children"] = [node("RECTANGLE", "Fill", 0, 0, 190, 4,
                          fills=[fill("primary")], cornerRadius=2)]
    return c


def circular_progress_component(x, y):
    c = node("COMPONENT", "Circular Progress", x, y, 48, 48, fills=[fill("surface", 0.0)])
    track = node("ELLIPSE", "Track", 4, 4, 40, 40,
                 fills=[fill("surface", 0.0)], strokes=[fill("secondary-container")],
                 strokeWeight=4.0, strokeAlign="INSIDE")
    # 270° arc, indicating: circle center (24,24) r=18, from top clockwise to west
    arc = node("VECTOR", "Arc", 0, 0, 48, 48,
               fillGeometry=[{"path": "M 24 6 A 18 18 0 1 1 6 24", "windingRule": "NONZERO"}],
               fills=[fill("surface", 0.0)], strokes=[fill("primary")],
               strokeWeight=4.0, strokeAlign="CENTER", strokeCap="ROUND")
    c["children"] = [track, arc]
    return c


# ---- Dialog -------------------------------------------------------------------------
DLG_W, DLG_H = 340, 180


def dialog_component(x, y):
    c = node("COMPONENT", "Dialog", x, y, DLG_W, DLG_H,
             fills=[fill("surface-container-high")], cornerRadius=28, clipsContent=True,
             effects=[{"type": "DROP_SHADOW", "color": {"r": 0, "g": 0, "b": 0, "a": 0.35},
                       "offset": {"x": 0, "y": 8}, "radius": 24}])
    c["children"] = [
        text("Title", 24, 20, DLG_W - 48, 28, "Dialog title", 20, 500, "on-surface", valign="TOP"),
        text("Body", 24, 56, DLG_W - 48, 60,
             "A dialog interrupts the flow to ask for a decision.",
             14, 400, "on-surface-variant", valign="TOP"),
        text("Cancel", DLG_W - 200, DLG_H - 48, 80, 32, "Cancel", 14, 500, "primary", align="CENTER"),
        text("Confirm", DLG_W - 104, DLG_H - 48, 80, 32, "Confirm", 14, 500, "primary", align="CENTER"),
    ]
    return c


# ---- Instance helper (materialized clone — figo instances carry real children) ----
def deep_clone(n, id_suffix):
    c = json.loads(json.dumps(n))

    def renumber(m):
        m["id"] = m["id"] + id_suffix
        for k in m.get("children", []):
            renumber(k)
    renumber(c)
    return c


def instance_of(component, name, x, y):
    inst = deep_clone(component, "/i" + nid("inst").split(":")[1])
    inst["type"] = "INSTANCE"
    inst["name"] = name
    inst["componentId"] = component["id"]
    inst["relativeTransform"] = [[1, 0, x], [0, 1, y]]
    return inst


# ---- Assemble ------------------------------------------------------------------------
def main():
    button = button_set(40, 40)
    switch = switch_set(40, 320)
    tf = textfield_set(300, 320)
    chip = chip_set(40, 430)
    nav = navitem_set(340, 430)
    card = card_component(40, 560)
    row = listrow_component(40, 710)
    slider = slider_component(40, 800)
    fab = fab_component(470, 700)
    appbar = appbar_component(40, 880)
    dialog = dialog_component(40, 980)
    checkbox = checkbox_set(560, 40)
    radio = radio_set(720, 40)
    segment = segment_set(560, 150)
    search = search_component(560, 360)
    divider = divider_component(560, 450)
    badge = badge_set(560, 480)
    avatar = avatar_component(680, 480)
    linprog = linear_progress_component(560, 570)
    circprog = circular_progress_component(560, 600)

    components_page = {
        "id": "page:components", "name": "Components", "type": "CANVAS",
        "backgroundColor": hex_to_rgba("#f0f0f3"),
        "children": [button, switch, tf, chip, nav, card, row, slider, fab, appbar,
                     dialog, checkbox, radio, segment, search, divider, badge,
                     avatar, linprog, circprog],
    }

    # Preview: one frame with an instance of everything, on a surface background
    # bound to the theme — the screenshot you review (and retint) as a whole.
    def variant(set_node, name_contains):
        for ch in set_node["children"]:
            if ch["name"] == name_contains:
                return ch
        raise KeyError(name_contains)

    pv = []
    pv.append(instance_of(appbar, "Preview App Bar", 0, 0))
    pv.append(instance_of(variant(button, "Type=Filled, State=Default"), "Btn Filled", 20, 84))
    pv.append(instance_of(variant(button, "Type=Tonal, State=Default"), "Btn Tonal", 150, 84))
    pv.append(instance_of(variant(button, "Type=Elevated, State=Default"), "Btn Elevated", 280, 84))
    pv.append(instance_of(variant(button, "Type=Outlined, State=Default"), "Btn Outlined", 20, 136))
    pv.append(instance_of(variant(button, "Type=Text, State=Default"), "Btn Text", 150, 136))
    pv.append(instance_of(variant(chip, "State=Default"), "Chip A", 20, 192))
    pv.append(instance_of(variant(chip, "State=Selected"), "Chip B", 140, 192))
    pv.append(instance_of(variant(switch, "State=Off"), "Switch Off", 280, 190))
    pv.append(instance_of(variant(switch, "State=On"), "Switch On", 344, 190))
    pv.append(instance_of(card, "Preview Card", 20, 240))
    pv.append(instance_of(row, "Preview Row", 20, 376))
    pv.append(instance_of(variant(tf, "State=Default"), "Preview Field", 20, 452))
    pv.append(instance_of(slider, "Preview Slider", 20, 524))
    pv.append(instance_of(dialog, "Preview Dialog", 20, 584))
    pv.append(instance_of(fab, "Preview FAB", 344, 700))
    pv.append(instance_of(variant(nav, "State=Selected"), "Nav 1", 42, 776))
    pv.append(instance_of(variant(nav, "State=Default"), "Nav 2", 168, 776))
    pv.append(instance_of(variant(nav, "State=Default"), "Nav 3", 294, 776))

    preview_frame = node("FRAME", "Preview", 0, 0, 420, 860,
                         id="frame:preview", fills=[fill("surface")], clipsContent=True)
    preview_frame["children"] = pv

    # Second overview frame: the input/indicator components added later.
    p2 = []
    p2.append(instance_of(search, "Inputs Search", 20, 20))
    p2.append(instance_of(variant(checkbox, "State=Unchecked"), "Cb Off", 20, 100))
    p2.append(instance_of(variant(checkbox, "State=Checked"), "Cb On", 60, 100))
    p2.append(instance_of(variant(radio, "State=Default"), "Rd Off", 120, 100))
    p2.append(instance_of(variant(radio, "State=Selected"), "Rd On", 160, 100))
    p2.append(instance_of(variant(badge, "Type=Dot"), "Badge Dot", 240, 108))
    p2.append(instance_of(variant(badge, "Type=Count"), "Badge Count", 264, 104))
    p2.append(instance_of(avatar, "Inputs Avatar", 340, 92))
    p2.append(instance_of(variant(segment, "Position=Start, State=Selected"), "Seg 1", 20, 156))
    p2.append(instance_of(variant(segment, "Position=Middle, State=Default"), "Seg 2", 130, 156))
    p2.append(instance_of(variant(segment, "Position=End, State=Default"), "Seg 3", 240, 156))
    p2.append(instance_of(divider, "Inputs Divider", 20, 224))
    p2.append(instance_of(linprog, "Inputs Linear", 20, 256))
    p2.append(instance_of(circprog, "Inputs Circular", 20, 288))

    inputs_frame = node("FRAME", "Preview Inputs", 460, 0, 420, 380,
                        id="frame:preview-inputs", fills=[fill("surface")], clipsContent=True)
    inputs_frame["children"] = p2

    preview_page = {
        "id": "page:preview", "name": "Preview", "type": "CANVAS",
        "backgroundColor": hex_to_rgba("#f0f0f3"),
        "children": [preview_frame, inputs_frame],
    }

    doc = {
        "name": "m3-components",
        "variables": {
            "modes": PAL["modes"],
            "activeMode": "light",
            "colors": {k: [PAL["light"][k], PAL["dark"][k]] for k in sorted(PAL["light"])},
        },
        "document": {
            "id": "0:0", "name": "m3-components", "type": "DOCUMENT",
            "children": [preview_page, components_page],
        },
    }
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, ensure_ascii=False, indent=1)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
