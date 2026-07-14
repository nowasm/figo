#!/usr/bin/env python3
"""_diag_regress design: opt-in diagnostics (D9/D10).

- TightLabel: fixed-size text that fits its box now but overflows at x1.3
  content length (text-stress)
- RoomyLabel: same text with generous width (stays clean under stress)
- TinyBtn: 32x28 clickable (touch-target)
- BigBtn: 120x48 clickable (clean)
- StatelessBtn: instance of a component set with NO hover/pressed variants
  (state-coverage); its set lives off-canvas
Run from this directory: python gen_design.py
"""
import json
import os


def text(nid, name, x, y, w, h, chars, size=16):
    return {
        "id": nid, "name": name, "type": "TEXT",
        "size": {"x": w, "y": h},
        "relativeTransform": [[1, 0, x], [0, 1, y]],
        "characters": chars,
        "style": {"fontFamily": "Helvetica", "fontSize": size},
        "fills": [{"type": "SOLID", "color": {"r": 0.1, "g": 0.1, "b": 0.1, "a": 1}}],
    }


def rect(nid, name, x, y, w, h, color):
    return {
        "id": nid, "name": name, "type": "RECTANGLE",
        "size": {"x": w, "y": h},
        "relativeTransform": [[1, 0, x], [0, 1, y]],
        "fills": [{"type": "SOLID", "color": color}],
    }


BLUE = {"r": 0.25, "g": 0.45, "b": 0.9, "a": 1}

variant = {
    "id": "5:1", "name": "Look=Plain", "type": "COMPONENT",
    "size": {"x": 120, "y": 48}, "relativeTransform": [[1, 0, 0], [0, 1, 0]],
    "children": [rect("5:2", "PlainBg", 0, 0, 120, 48, BLUE)],
}
stateless_set = {
    "id": "5:0", "name": "PlainBtn", "type": "COMPONENT_SET",
    "size": {"x": 120, "y": 48}, "relativeTransform": [[1, 0, 600], [0, 1, 0]],
    "children": [variant],
}
stateless_instance = {
    "id": "6:1", "name": "StatelessBtn", "type": "INSTANCE",
    "componentId": "5:1",
    "size": {"x": 120, "y": 48}, "relativeTransform": [[1, 0, 40], [0, 1, 400]],
    "children": [rect("6:2", "PlainBg", 0, 0, 120, 48, BLUE)],
}

doc = {
    "name": "DiagRegress",
    "document": {
        "id": "0:0", "type": "DOCUMENT", "children": [{
            "id": "0:1", "name": "Page 1", "type": "CANVAS", "children": [{
                "id": "1:1", "name": "Main", "type": "FRAME",
                "size": {"x": 420, "y": 900},
                "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                "fills": [{"type": "SOLID",
                           "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                "children": [
                    # Sized so today's content fits one line with slack, but
                    # +30% wraps to a second line and blows the 22px height —
                    # robust across platform font-metric differences.
                    text("2:1", "TightLabel", 40, 60, 190, 22,
                         "Confirm payment now"),
                    text("2:2", "RoomyLabel", 40, 120, 360, 22,
                         "Confirm payment now"),
                    rect("3:1", "TinyBtn", 40, 200, 32, 28,
                         {"r": 0.9, "g": 0.35, "b": 0.3, "a": 1}),
                    rect("3:2", "BigBtn", 40, 260, 120, 48, BLUE),
                    stateless_instance,
                ],
            }, stateless_set],
        }],
    },
}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "design.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(doc, f, indent=1)
print("wrote", out)
