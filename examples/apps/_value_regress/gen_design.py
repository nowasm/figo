#!/usr/bin/env python3
"""Design generator for _value_regress (G3: bindSlider / autoStates /
setVariant dissolve). Layout at 420x900, scale 1 (viewport == frame):
  Panel          : abs (24,120) 372x400, vertical scroll, maxScrollY = 200
  Volume Track   : abs (60,180) 300x24  (inside Panel, y 60)
  Progress Track : abs (60,260) 300x12  (readonly)
  Demo Button    : abs (60,700) 120x44, instance of StateBtn component set
                   (State=Default/Hover/Pressed; label text marks the state)
Run:  python gen_design.py  (writes design.json next to itself)
"""
import json
from pathlib import Path

_id = [0]
def nid():
    _id[0] += 1
    return f"9:{_id[0]}"

def rt(x, y):
    return [[1, 0, x], [0, 1, y]]

def solid(r, g, b, a=1.0):
    return [{"type": "SOLID", "color": {"r": r, "g": g, "b": b, "a": a}}]

def text(name, x, y, w, h, chars, size=16, weight=600, color=(0.15, 0.17, 0.2)):
    return {
        "id": nid(), "name": name, "type": "TEXT",
        "size": {"x": w, "y": h}, "relativeTransform": rt(x, y),
        "characters": chars, "fills": solid(*color),
        "style": {"fontFamily": "Segoe UI", "fontWeight": weight,
                  "fontSize": size, "textAlignHorizontal": "LEFT",
                  "textAlignVertical": "TOP"},
    }

def frame(name, x, y, w, h, children, **kw):
    n = {"id": nid(), "name": name, "type": "FRAME",
         "size": {"x": w, "y": h}, "relativeTransform": rt(x, y),
         "clipsContent": kw.pop("clips", False), "children": children}
    n.update(kw)
    return n

def rect(name, x, y, w, h, fill, radius=0):
    n = {"id": nid(), "name": name, "type": "RECTANGLE",
         "size": {"x": w, "y": h}, "relativeTransform": rt(x, y),
         "fills": solid(*fill)}
    if radius:
        n["cornerRadius"] = radius
    return n


def slider(name, x, y, w, h, fill_w, knob=True):
    kids = [rect(name.replace("Track", "Fill"), 0, 0, fill_w, h,
                 (0.22, 0.45, 0.95), radius=h / 2)]
    if knob:
        kids.append({
            "id": nid(), "name": name.replace("Track", "Knob"), "type": "ELLIPSE",
            "size": {"x": h, "y": h}, "relativeTransform": rt(fill_w - h, 0),
            "fills": solid(0.12, 0.14, 0.2),
        })
    return frame(name, x, y, w, h, kids, fills=solid(0.87, 0.89, 0.93),
                 cornerRadius=h / 2)


def state_component(state, y, bg):
    return {
        "id": nid(), "name": f"State={state}", "type": "COMPONENT",
        "size": {"x": 120, "y": 44}, "relativeTransform": rt(0, y),
        "children": [
            rect("BtnBg", 0, 0, 120, 44, bg, radius=10),
            text("StateLabel", 16, 12, 90, 20, state.lower(),
                 color=(1, 1, 1)),
        ],
    }


panel = frame("Panel", 24, 120, 372, 400, [
    text("PanelTitle", 36, 16, 300, 24, "Sliders"),
    slider("Volume Track", 36, 60, 300, 24, 90),          # value 30 of 0..100
    text("ProgressTitle", 36, 110, 300, 20, "Progress", size=13, weight=400),
    slider("Progress Track", 36, 140, 300, 12, 75, knob=False),  # 0.25 of 0..1
    rect("Spacer", 36, 220, 300, 380, (0.94, 0.95, 0.97), radius=12),
], clips=True, overflowDirection="VERTICAL_SCROLLING",
    fills=solid(1, 1, 1), cornerRadius=16)

components = [
    state_component("Default", 0, (0.22, 0.45, 0.95)),
    state_component("Hover", 60, (0.30, 0.55, 1.00)),
    state_component("Pressed", 120, (0.12, 0.30, 0.75)),
]
default_id = components[0]["id"]

def make_instance(name, x):
    return {
        "id": nid(), "name": name, "type": "INSTANCE",
        "componentId": default_id,
        "size": {"x": 120, "y": 44}, "relativeTransform": rt(x, 700),
        "children": [
            rect("BtnBg", 0, 0, 120, 44, (0.22, 0.45, 0.95), radius=10),
            text("StateLabel", 16, 12, 90, 20, "default", color=(1, 1, 1)),
        ],
    }

instance = make_instance("Demo Button", 60)       # autoStates target
fade_instance = make_instance("Fade Button", 240)  # setVariant dissolve target


# Smart-animate target: variants differ in a child's width (Bar 40 -> 120),
# the bg color, and one child that only the On variant has (Badge).
def smart_component(state, y, bar_w, bg, with_badge):
    kids = [
        rect("SBg", 0, 0, 140, 44, bg, radius=10),
        rect("Bar", 10, 17, bar_w, 10, (1, 1, 1), radius=5),
    ]
    if with_badge:
        kids.append(rect("Badge", 120, 6, 12, 12, (1.0, 0.8, 0.2), radius=6))
    return {
        "id": nid(), "name": f"Mode={state}", "type": "COMPONENT",
        "size": {"x": 140, "y": 44}, "relativeTransform": rt(0, y),
        "children": kids,
    }


smart_components = [
    smart_component("Off", 0, 40, (0.25, 0.28, 0.35), False),
    smart_component("On", 60, 120, (0.10, 0.60, 0.40), True),
]
smart_off_id = smart_components[0]["id"]
smart_instance = {
    "id": nid(), "name": "Smart Button", "type": "INSTANCE",
    "componentId": smart_off_id,
    "size": {"x": 140, "y": 44}, "relativeTransform": rt(60, 780),
    "children": [
        rect("SBg", 0, 0, 140, 44, (0.25, 0.28, 0.35), radius=10),
        rect("Bar", 10, 17, 40, 10, (1, 1, 1), radius=5),
    ],
}

main = frame("Main", 0, 0, 420, 900, [
    text("Title", 24, 48, 372, 40, "Value Regress", size=28, weight=700,
         color=(0.1, 0.11, 0.13)),
    panel,
    instance,
    fade_instance,
    smart_instance,
], clips=True, fills=solid(0.97, 0.97, 0.98))

state_set = {
    "id": nid(), "name": "StateBtn", "type": "COMPONENT_SET",
    "size": {"x": 120, "y": 164}, "relativeTransform": rt(600, 0),
    "children": components,
}
smart_set = {
    "id": nid(), "name": "SmartBtn", "type": "COMPONENT_SET",
    "size": {"x": 140, "y": 104}, "relativeTransform": rt(600, 260),
    "children": smart_components,
}

doc = {
    "name": "ValueRegress",
    "document": {
        "id": "0:0", "name": "Document", "type": "DOCUMENT",
        "children": [{
            "id": "0:1", "name": "Page 1", "type": "CANVAS",
            "children": [main, state_set, smart_set],
        }],
    },
}

out = Path(__file__).with_name("design.json")
out.write_text(json.dumps(doc, indent=1), encoding="utf-8")
print(f"wrote {out}")
