#include "figo/document.h"

#include <algorithm>
#include <cstdio>

namespace figo {

void Node::contentExtent(float& w, float& h) const {
    w = 0;
    h = 0;
    for (const auto& c : children) {
        if (!c->effectivelyVisible() || c->scrollFixed) continue;
        // Children can be rotated: take the max over all four corners.
        const float xs[2] = {0, c->width}, ys[2] = {0, c->height};
        for (float cx : xs) {
            for (float cy : ys) {
                float px, py;
                c->relativeTransform.apply(cx, cy, px, py);
                w = std::max(w, px);
                h = std::max(h, py);
            }
        }
    }
}

float Node::maxScrollX() const {
    if (scrollDirection != ScrollDirection::Horizontal &&
        scrollDirection != ScrollDirection::Both) {
        return 0;
    }
    float w, h;
    contentExtent(w, h);
    return std::max(0.0f, w - width);
}

float Node::maxScrollY() const {
    if (scrollDirection != ScrollDirection::Vertical &&
        scrollDirection != ScrollDirection::Both) {
        return 0;
    }
    float w, h;
    contentExtent(w, h);
    return std::max(0.0f, h - height);
}

Node* Node::findById(const std::string& nodeId) {
    if (id == nodeId) return this;
    for (auto& c : children) {
        if (Node* n = c->findById(nodeId)) return n;
    }
    return nullptr;
}

Node* Node::findByName(const std::string& nodeName) {
    if (name == nodeName) return this;
    for (auto& c : children) {
        if (Node* n = c->findByName(nodeName)) return n;
    }
    return nullptr;
}

void Node::visit(const std::function<bool(Node&)>& fn) {
    if (!fn(*this)) return;
    for (auto& c : children) c->visit(fn);
}

std::vector<Node*> Document::topLevelFrames() const {
    std::vector<Node*> frames;
    if (!root) return frames;
    auto frameLike = [](NodeType t) {
        return t == NodeType::Frame || t == NodeType::Component || t == NodeType::Instance ||
               t == NodeType::Section;
    };
    for (auto& page : root->children) {
        if (page->type == NodeType::Canvas) {
            for (auto& child : page->children) {
                if (frameLike(child->type)) frames.push_back(child.get());
            }
        } else if (frameLike(page->type) && page->width > 0 && page->height > 0) {
            // canvas.json pages can miss the CANVAS inference; accept direct frames.
            frames.push_back(page.get());
        }
    }
    // A bare node tree (no DOCUMENT/CANVAS wrapper): treat the root as a frame.
    if (frames.empty() && root->type != NodeType::Document) frames.push_back(root.get());
    return frames;
}

std::unique_ptr<Node> cloneNode(const Node& src, Node* parent) {
    auto n = std::make_unique<Node>();
    static_cast<NodeData&>(*n) = src;  // member-wise copy of all value state
    n->parent = parent;
    n->children.reserve(src.children.size());
    for (const auto& c : src.children) n->children.push_back(cloneNode(*c, n.get()));
    return n;
}

void setNodeText(Node& n, const std::string& text) {
    n.characters = text;
    // Replace stale rich-text runs (they index the old string) with a single
    // base-style run over the new text. Keeping a run (rather than none)
    // routes rendering through the per-token path, whose font fallback works
    // glyph-by-glyph — mixed Latin/CJK runtime strings need that.
    n.textRuns.clear();
    if (!text.empty()) {
        TextRun run;
        run.start = 0;
        run.end = static_cast<int>(text.size());
        run.style = n.textStyle;
        n.textRuns.push_back(run);
    }
}

// ---- Theme variables ----

int VariableTable::modeIndex(const std::string& mode) const {
    for (size_t i = 0; i < modes.size(); ++i) {
        if (modes[i] == mode) return static_cast<int>(i);
    }
    return -1;
}

int VariableTable::ensureMode(const std::string& mode) {
    int idx = modeIndex(mode);
    if (idx >= 0) return idx;
    modes.push_back(mode);
    for (auto& v : vars) {
        v.values.push_back(v.values.empty() ? Color{} : v.values.front());
    }
    for (auto& v : numVars) {
        v.values.push_back(v.values.empty() ? 0.0f : v.values.front());
    }
    return static_cast<int>(modes.size()) - 1;
}

void VariableTable::set(const std::string& name, const Color& c, const std::string& mode) {
    if (modes.empty()) modes.push_back("light");
    Var* var = nullptr;
    for (auto& v : vars) {
        if (v.name == name) {
            var = &v;
            break;
        }
    }
    if (!var) {
        vars.push_back({name, std::vector<Color>(modes.size(), c)});
        var = &vars.back();
    }
    if (mode.empty()) {
        std::fill(var->values.begin(), var->values.end(), c);
    } else {
        const int idx = ensureMode(mode);  // may grow every var's values
        for (auto& v : vars) {
            if (v.name == name) {
                v.values[static_cast<size_t>(idx)] = c;
                break;
            }
        }
    }
    if (activeMode >= static_cast<int>(modes.size())) activeMode = 0;
}

const Color* VariableTable::get(const std::string& name, const std::string& mode) const {
    const int idx = mode.empty() ? activeMode : modeIndex(mode);
    if (idx < 0 || idx >= static_cast<int>(modes.size())) return nullptr;
    for (const auto& v : vars) {
        if (v.name == name && static_cast<size_t>(idx) < v.values.size()) {
            return &v.values[static_cast<size_t>(idx)];
        }
    }
    return nullptr;
}

void VariableTable::setNumber(const std::string& name, float value, const std::string& mode) {
    if (modes.empty()) modes.push_back("light");
    NumVar* var = nullptr;
    for (auto& v : numVars) {
        if (v.name == name) {
            var = &v;
            break;
        }
    }
    if (!var) {
        numVars.push_back({name, std::vector<float>(modes.size(), value)});
        var = &numVars.back();
    }
    if (mode.empty()) {
        std::fill(var->values.begin(), var->values.end(), value);
    } else {
        const int idx = ensureMode(mode);  // may grow every var's values
        for (auto& v : numVars) {
            if (v.name == name) {
                v.values[static_cast<size_t>(idx)] = value;
                break;
            }
        }
    }
    if (activeMode >= static_cast<int>(modes.size())) activeMode = 0;
}

const float* VariableTable::getNumber(const std::string& name, const std::string& mode) const {
    const int idx = mode.empty() ? activeMode : modeIndex(mode);
    if (idx < 0 || idx >= static_cast<int>(modes.size())) return nullptr;
    for (const auto& v : numVars) {
        if (v.name == name && static_cast<size_t>(idx) < v.values.size()) {
            return &v.values[static_cast<size_t>(idx)];
        }
    }
    return nullptr;
}

bool colorFromHex(const std::string& hex, Color& out) {
    if (hex.size() != 7 && hex.size() != 9) return false;
    if (hex[0] != '#') return false;
    unsigned v = 0;
    for (size_t i = 1; i < hex.size(); ++i) {
        const char ch = hex[i];
        unsigned d;
        if (ch >= '0' && ch <= '9') d = static_cast<unsigned>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') d = static_cast<unsigned>(ch - 'a') + 10;
        else if (ch >= 'A' && ch <= 'F') d = static_cast<unsigned>(ch - 'A') + 10;
        else return false;
        v = (v << 4) | d;
    }
    if (hex.size() == 7) v = (v << 8) | 0xFF;
    out.r = static_cast<float>((v >> 24) & 0xFF) / 255.0f;
    out.g = static_cast<float>((v >> 16) & 0xFF) / 255.0f;
    out.b = static_cast<float>((v >> 8) & 0xFF) / 255.0f;
    out.a = static_cast<float>(v & 0xFF) / 255.0f;
    return true;
}

std::string colorToHex(const Color& c) {
    auto b = [](float f) {
        return static_cast<unsigned>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    char buf[10];
    if (b(c.a) == 255) std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", b(c.r), b(c.g), b(c.b));
    else std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", b(c.r), b(c.g), b(c.b), b(c.a));
    return buf;
}

bool Document::applyVariables() {
    if (variables.empty() || !root) return false;
    bool layoutChanged = false;
    root->visit([&](Node& n) {
        auto apply = [&](std::vector<Paint>& paints) {
            for (auto& p : paints) {
                if (p.colorVar.empty()) continue;
                if (const Color* c = variables.get(p.colorVar)) p.color = *c;
            }
        };
        apply(n.fills);
        apply(n.strokes);
        for (const auto& [prop, varName] : n.numVarBindings) {
            const float* v = variables.getNumber(varName);
            if (!v) continue;
            if (prop == "fontSize") {
                // Base style only; rich-text runs keep their own sizes.
                if (n.textStyle.fontSize != *v) layoutChanged = true;
                n.textStyle.fontSize = *v;
            } else if (prop == "cornerRadius") {
                n.cornerRadius = *v;
                n.rectangleCornerRadii.reset();  // uniform token wins
            } else if (prop == "strokeWeight") {
                n.strokeWeight = *v;
            } else if (prop == "itemSpacing") {
                if (n.autoLayout.itemSpacing != *v) layoutChanged = true;
                n.autoLayout.itemSpacing = *v;
            } else if (prop == "paddingLeft") {
                if (n.autoLayout.paddingLeft != *v) layoutChanged = true;
                n.autoLayout.paddingLeft = *v;
            } else if (prop == "paddingRight") {
                if (n.autoLayout.paddingRight != *v) layoutChanged = true;
                n.autoLayout.paddingRight = *v;
            } else if (prop == "paddingTop") {
                if (n.autoLayout.paddingTop != *v) layoutChanged = true;
                n.autoLayout.paddingTop = *v;
            } else if (prop == "paddingBottom") {
                if (n.autoLayout.paddingBottom != *v) layoutChanged = true;
                n.autoLayout.paddingBottom = *v;
            }
        }
        return true;
    });
    return layoutChanged;
}

void Document::captureBaseLayout() {
    if (!root) return;
    root->visit([](Node& n) {
        n.baseTransform = n.relativeTransform;
        n.baseWidth = n.width;
        n.baseHeight = n.height;
        return true;
    });
}

}  // namespace figo
