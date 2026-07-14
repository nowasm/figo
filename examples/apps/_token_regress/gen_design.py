#!/usr/bin/env python3
"""_token_regress design: numeric design tokens (D1).

Two hug stacks + an auto-height text whose spacing/type scale are bound to
number variables; app.js switches modes / edits tokens and asserts the
resulting hug heights. Run from this directory: python gen_design.py
"""
import json
import os

SPACE = {"compact": 8, "cozy": 24}   # itemSpacing + paddingTop/Bottom of Stack
TEXT_MD = {"compact": 16, "cozy": 24}


def rect(nid, name, y, fill=None):
    node = {
        "id": nid, "name": name, "type": "RECTANGLE",
        "size": {"x": 100, "y": 50},
        "relativeTransform": [[1, 0, 8], [0, 1, y]],
        "fills": [fill or {"type": "SOLID",
                           "color": {"r": 0.2, "g": 0.2, "b": 0.2, "a": 1}}],
    }
    return node


def stack(nid, name, x, y, bindings, spacing):
    h = 8 + 50 + spacing + 50 + 8  # paddingTop + row + itemSpacing + row + paddingBottom
    return {
        "id": nid, "name": name, "type": "FRAME",
        "size": {"x": 300, "y": h},
        "relativeTransform": [[1, 0, x], [0, 1, y]],
        "layoutMode": "VERTICAL",
        "primaryAxisSizingMode": "AUTO", "counterAxisSizingMode": "FIXED",
        "paddingLeft": 8, "paddingRight": 8, "paddingTop": 8, "paddingBottom": 8,
        "itemSpacing": spacing,
        "varBindings": bindings,
        "fills": [{"type": "SOLID", "color": {"r": 0.95, "g": 0.95, "b": 0.95, "a": 1}}],
        "children": [
            rect(nid + "1", name + " Row A", 8,
                 fill={"type": "SOLID", "colorVar": "accent",
                       "color": {"r": 0, "g": 0, "b": 0, "a": 1}}),
            rect(nid + "2", name + " Row B", 8 + 50 + spacing),
        ],
    }


doc = {
    "name": "TokenRegress",
    "variables": {
        "modes": ["compact", "cozy"],
        "activeMode": "compact",
        "numbers": {
            "space": [SPACE["compact"], SPACE["cozy"]],
            "text-md": [TEXT_MD["compact"], TEXT_MD["cozy"]],
        },
        "colors": {"accent": ["#ff0000", "#00ff00"]},
    },
    "document": {
        "id": "0:0", "type": "DOCUMENT", "children": [{
            "id": "0:1", "name": "Page 1", "type": "CANVAS", "children": [{
                "id": "1:1", "name": "Main", "type": "FRAME",
                "size": {"x": 420, "y": 900},
                "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                "fills": [{"type": "SOLID",
                           "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                "children": [
                    # Fully token-bound stack: spacing AND vertical padding.
                    stack("2:1", "Stack", 24, 80,
                          {"itemSpacing": "space", "paddingTop": "space",
                           "paddingBottom": "space"},
                          SPACE["compact"]),
                    # No design-time bindings; app.js binds itemSpacing at
                    # runtime via ui.bindVar.
                    stack("3:1", "Stack2", 24, 300, {}, 8),
                    # Auto-height text in a hug wrapper; fontSize bound to the
                    # type-scale token. (A bare TEXT under a plain parent does
                    # not re-wrap at authored size — README known limitation —
                    # so the binding lives inside an auto-layout chain, which
                    # is also the realistic design-system pattern.)
                    {
                        "id": "4:0", "name": "TextWrap", "type": "FRAME",
                        "size": {"x": 200, "y": 40},
                        "relativeTransform": [[1, 0, 24], [0, 1, 560]],
                        "layoutMode": "VERTICAL",
                        "primaryAxisSizingMode": "AUTO",
                        "counterAxisSizingMode": "FIXED",
                        "fills": [{"type": "SOLID",
                                   "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                        "children": [{
                            "id": "4:1", "name": "Label", "type": "TEXT",
                            "size": {"x": 200, "y": 20},
                            "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                            "characters": "The quick brown fox jumps over the "
                                          "lazy dog again and again and again.",
                            "style": {"fontFamily": "Segoe UI",
                                      "fontSize": TEXT_MD["compact"],
                                      "textAutoResize": "HEIGHT"},
                            "fills": [{"type": "SOLID",
                                       "color": {"r": 0, "g": 0, "b": 0, "a": 1}}],
                            "varBindings": {"fontSize": "text-md"},
                        }],
                    },
                ],
            }],
        }],
    },
}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "design.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(doc, f, indent=1)
print("wrote", out)
