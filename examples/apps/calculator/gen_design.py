# -*- coding: utf-8 -*-
"""Generate the calculator design from the figo M3 component library.

Every fill binds a theme variable (colorVar) from design-systems/components/
m3-palette.json, so ui.setThemeMode("dark") retints the whole app; the
theme toggle is a real Switch instance stamped from the library's master
(the master set ships in this document so ui.setVariant works).

Usage: python gen_design.py   (rewrites design.figo.json)
"""
import copy
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
PALETTE = os.path.join(ROOT, "design-systems", "components", "m3-palette.json")
LIBRARY = os.path.join(ROOT, "design-systems", "components", "m3-components.figo.json")
OUT = os.path.join(HERE, "design.figo.json")

FONT = "Roboto"

with open(PALETTE, encoding="utf-8") as f:
    PAL = json.load(f)
with open(LIBRARY, encoding="utf-8") as f:
    LIB = json.load(f)

_seq = [0]


def nid(prefix="n"):
    _seq[0] += 1
    return "%s:%d" % (prefix, _seq[0])


def hex_to_rgba(h):
    h = h.lstrip("#")
    v = [int(h[i:i + 2], 16) / 255.0 for i in range(0, len(h), 2)]
    if len(v) == 3:
        v.append(1.0)
    return {"r": v[0], "g": v[1], "b": v[2], "a": v[3]}


def fill(var, opacity=None):
    p = {"type": "SOLID", "color": hex_to_rgba(PAL["light"][var]), "colorVar": var}
    if opacity is not None:
        p["opacity"] = opacity
    return p


def node(type_, name, x, y, w, h, **kw):
    n = {"id": kw.pop("id", nid()), "name": name, "type": type_,
         "size": {"x": w, "y": h},
         "relativeTransform": [[1, 0, x], [0, 1, y]]}
    n.update(kw)
    return n


def text(name, x, y, w, h, chars, size, weight, color_var, align="CENTER", valign="CENTER"):
    return node("TEXT", name, x, y, w, h, fills=[fill(color_var)], characters=chars,
                style={"fontFamily": FONT, "fontSize": size, "fontWeight": weight,
                       "textAlignHorizontal": align, "textAlignVertical": valign})


# ---- key grid ------------------------------------------------------------------
# 4 cols x 5 rows of squircle keys; the M3 calculator color mapping:
#   digits/dot -> surface-container-high, AC/±/% -> tertiary-container,
#   operators  -> secondary-container,    =      -> primary.
KIND = {
    "digit":  ("surface-container-high", "on-surface"),
    "action": ("tertiary-container", "on-tertiary-container"),
    "op":     ("secondary-container", "on-secondary-container"),
    "equal":  ("primary", "on-primary"),
}
KEY_W, KEY_H, GAP, MARGIN, RADIUS = 86, 74, 12, 20, 28
GRID = [
    [("KeyClear", "AC", "action"), ("KeySign", "±", "action"),
     ("KeyPct", "%", "action"), ("KeyDiv", "÷", "op")],
    [("Key7", "7", "digit"), ("Key8", "8", "digit"), ("Key9", "9", "digit"),
     ("KeyMul", "×", "op")],
    [("Key4", "4", "digit"), ("Key5", "5", "digit"), ("Key6", "6", "digit"),
     ("KeySub", "-", "op")],
    [("Key1", "1", "digit"), ("Key2", "2", "digit"), ("Key3", "3", "digit"),
     ("KeyAdd", "+", "op")],
    [("Key0", "0", "digit"), ("KeyDot", ".", "digit"), ("KeyEq", "=", "equal")],
]


def key(name, label, kind, x, y, w):
    bg, fg = KIND[kind]
    k = node("FRAME", name, x, y, w, KEY_H, fills=[fill(bg)],
             cornerRadius=RADIUS, clipsContent=True)
    size = 24 if kind == "digit" else 26
    weight = 400 if kind == "digit" else 500
    k["children"] = [text("Label", 0, 0, w, KEY_H, label, size, weight, fg)]
    return k


def build_keys():
    keys = []
    top = 900 - MARGIN - (5 * KEY_H + 4 * GAP)
    for r, row in enumerate(GRID):
        y = top + r * (KEY_H + GAP)
        if len(row) == 4:
            for c, (name, label, kind) in enumerate(row):
                keys.append(key(name, label, kind, MARGIN + c * (KEY_W + GAP), y, KEY_W))
        else:  # last row: double-width 0, then . and =
            name, label, kind = row[0]
            keys.append(key(name, label, kind, MARGIN, y, KEY_W * 2 + GAP))
            for c, (name, label, kind) in enumerate(row[1:], start=2):
                keys.append(key(name, label, kind, MARGIN + c * (KEY_W + GAP), y, KEY_W))
    return keys, top


# ---- library reuse: the Switch master set + one instance -------------------------
def lib_component_set(name):
    for page in LIB["document"]["children"]:
        if page["name"] != "Components":
            continue
        for child in page["children"]:
            if child["name"] == name and child["type"] == "COMPONENT_SET":
                return copy.deepcopy(child)
    raise KeyError(name)


def instance_of(master_set, variant_name, inst_name, x, y):
    master = next(c for c in master_set["children"] if c["name"] == variant_name)
    inst = copy.deepcopy(master)

    def renumber(n):
        n["id"] = "calc-" + n["id"]
        for c in n.get("children", []):
            renumber(c)
    renumber(inst)
    inst["type"] = "INSTANCE"
    inst["name"] = inst_name
    inst["componentId"] = master["id"]
    inst["relativeTransform"] = [[1, 0, x], [0, 1, y]]
    return inst


def main():
    switch_set = lib_component_set("Switch")
    keys, grid_top = build_keys()

    frame = node("FRAME", "Calculator", 0, 0, 420, 900,
                 id="frame:calculator", fills=[fill("surface")], clipsContent=True)
    frame["children"] = [
        text("Title", MARGIN, 28, 200, 32, "Calculator", 16, 500,
             "on-surface-variant", align="LEFT"),
        instance_of(switch_set, "State=Off", "ThemeSwitch", 420 - MARGIN - 52, 28),
        # display: right-aligned expression + big result above the grid
        text("Expression", MARGIN, grid_top - 128, 420 - 2 * MARGIN, 30, "", 20, 400,
             "on-surface-variant", align="RIGHT"),
        text("Result", MARGIN, grid_top - 96, 420 - 2 * MARGIN, 72, "0", 56, 400,
             "on-surface", align="RIGHT"),
    ] + keys

    doc = {
        "name": "calculator",
        "variables": {
            "modes": PAL["modes"],
            "activeMode": "light",
            "colors": {k: [PAL["light"][k], PAL["dark"][k]] for k in sorted(PAL["light"])},
        },
        "document": {
            "id": "0:0", "name": "calculator", "type": "DOCUMENT",
            "children": [
                {"id": "page:main", "name": "Main", "type": "CANVAS",
                 "backgroundColor": hex_to_rgba("#f0f0f3"), "children": [frame]},
                {"id": "page:components", "name": "Components", "type": "CANVAS",
                 "backgroundColor": hex_to_rgba("#f0f0f3"), "children": [switch_set]},
            ],
        },
    }
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, ensure_ascii=False, indent=1)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
