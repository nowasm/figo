// Document → Figma-REST-style JSON (figo's save format).
// parseDocument() reads this back losslessly, so the editor can round-trip.

#include <fstream>

#include <nlohmann/json.hpp>

#include "figo/parser.h"

namespace figo {

using json = nlohmann::json;

namespace {

const char* typeName(NodeType t) {
    switch (t) {
    case NodeType::Document: return "DOCUMENT";
    case NodeType::Canvas: return "CANVAS";
    case NodeType::Frame: return "FRAME";
    case NodeType::Group: return "GROUP";
    case NodeType::Section: return "SECTION";
    case NodeType::Rectangle: return "RECTANGLE";
    case NodeType::Ellipse: return "ELLIPSE";
    case NodeType::Line: return "LINE";
    case NodeType::Vector: return "VECTOR";
    case NodeType::BooleanOperation: return "BOOLEAN_OPERATION";
    case NodeType::Star: return "STAR";
    case NodeType::RegularPolygon: return "REGULAR_POLYGON";
    case NodeType::Text: return "TEXT";
    case NodeType::Component: return "COMPONENT";
    case NodeType::ComponentSet: return "COMPONENT_SET";
    case NodeType::Instance: return "INSTANCE";
    case NodeType::Slice: return "SLICE";
    default: return "FRAME";
    }
}

json colorJson(const Color& c) {
    return json{{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
}

json paintJson(const Paint& p) {
    json j;
    switch (p.type) {
    case PaintType::Solid: j["type"] = "SOLID"; break;
    case PaintType::GradientLinear: j["type"] = "GRADIENT_LINEAR"; break;
    case PaintType::GradientRadial: j["type"] = "GRADIENT_RADIAL"; break;
    case PaintType::GradientAngular: j["type"] = "GRADIENT_ANGULAR"; break;
    case PaintType::GradientDiamond: j["type"] = "GRADIENT_DIAMOND"; break;
    case PaintType::Image: j["type"] = "IMAGE"; break;
    }
    if (!p.visible) j["visible"] = false;
    if (p.opacity != 1.0f) j["opacity"] = p.opacity;
    if (!p.colorVar.empty()) j["colorVar"] = p.colorVar;
    if (p.type == PaintType::Solid) {
        j["color"] = colorJson(p.color);
    } else if (p.type == PaintType::Image) {
        j["imageRef"] = p.imageRef;
        j["scaleMode"] = p.imageScaleMode;
        if (p.imageScale != 1.0f) j["scalingFactor"] = p.imageScale;
        if (p.imageTransform) {
            const auto& m = *p.imageTransform;
            j["imageTransform"] =
                json::array({json::array({m[0], m[1], m[2]}), json::array({m[3], m[4], m[5]})});
        }
    } else {
        json stops = json::array();
        for (const auto& s : p.stops) {
            stops.push_back({{"position", s.position}, {"color", colorJson(s.color)}});
        }
        j["gradientStops"] = std::move(stops);
        j["gradientHandlePositions"] = json::array(
            {{{"x", p.handle0[0]}, {"y", p.handle0[1]}},
             {{"x", p.handle1[0]}, {"y", p.handle1[1]}},
             {{"x", p.handle2[0]}, {"y", p.handle2[1]}}});
    }
    return j;
}

json nodeJson(const Node& n) {
    json j;
    j["id"] = n.id;
    j["name"] = n.name;
    j["type"] = typeName(n.type);
    if (!n.visible) j["visible"] = false;
    if (n.opacity != 1.0f) j["opacity"] = n.opacity;

    if (n.width > 0 || n.height > 0) j["size"] = {{"x", n.width}, {"y", n.height}};
    const Mat23& m = n.relativeTransform;
    j["relativeTransform"] = json::array(
        {json::array({m.m00, m.m01, m.m02}), json::array({m.m10, m.m11, m.m12})});
    if (n.clipsContent) j["clipsContent"] = true;
    if (n.isMask) j["isMask"] = true;
    if (!n.blendMode.empty()) j["blendMode"] = n.blendMode;

    auto constraintName = [](Constraint c, bool horizontal) {
        switch (c) {
        case Constraint::Center: return "CENTER";
        case Constraint::Max: return horizontal ? "RIGHT" : "BOTTOM";
        case Constraint::Stretch: return horizontal ? "LEFT_RIGHT" : "TOP_BOTTOM";
        case Constraint::Scale: return "SCALE";
        default: return horizontal ? "LEFT" : "TOP";
        }
    };
    if (n.constraintH != Constraint::Min || n.constraintV != Constraint::Min) {
        j["constraints"] = {{"horizontal", constraintName(n.constraintH, true)},
                            {"vertical", constraintName(n.constraintV, false)}};
    }
    if (n.autoLayout.enabled()) {
        const AutoLayout& al = n.autoLayout;
        auto alignName = [](AutoLayout::Align a) {
            switch (a) {
            case AutoLayout::Align::Center: return "CENTER";
            case AutoLayout::Align::Max: return "MAX";
            case AutoLayout::Align::SpaceBetween: return "SPACE_BETWEEN";
            case AutoLayout::Align::Baseline: return "BASELINE";
            default: return "MIN";
            }
        };
        j["layoutMode"] = al.mode == AutoLayout::Mode::Horizontal ? "HORIZONTAL" : "VERTICAL";
        j["primaryAxisSizingMode"] = al.primarySizing == AutoLayout::Sizing::Fixed ? "FIXED" : "AUTO";
        j["counterAxisSizingMode"] = al.counterSizing == AutoLayout::Sizing::Fixed ? "FIXED" : "AUTO";
        if (al.primaryAlign != AutoLayout::Align::Min) j["primaryAxisAlignItems"] = alignName(al.primaryAlign);
        if (al.counterAlign != AutoLayout::Align::Min) j["counterAxisAlignItems"] = alignName(al.counterAlign);
        if (al.paddingLeft != 0) j["paddingLeft"] = al.paddingLeft;
        if (al.paddingRight != 0) j["paddingRight"] = al.paddingRight;
        if (al.paddingTop != 0) j["paddingTop"] = al.paddingTop;
        if (al.paddingBottom != 0) j["paddingBottom"] = al.paddingBottom;
        if (al.itemSpacing != 0) j["itemSpacing"] = al.itemSpacing;
        if (al.counterSpacing != 0) j["counterAxisSpacing"] = al.counterSpacing;
        if (al.wrap) j["layoutWrap"] = "WRAP";
    }
    if (n.layoutGrow != 0) j["layoutGrow"] = n.layoutGrow;
    if (n.layoutAlignStretch) j["layoutAlign"] = "STRETCH";
    if (n.layoutAbsolute) j["layoutPositioning"] = "ABSOLUTE";
    if (n.minWidth > 0) j["minWidth"] = n.minWidth;
    if (n.maxWidth > 0) j["maxWidth"] = n.maxWidth;
    if (n.minHeight > 0) j["minHeight"] = n.minHeight;
    if (n.maxHeight > 0) j["maxHeight"] = n.maxHeight;

    if (!n.fills.empty()) {
        json fills = json::array();
        for (const auto& p : n.fills) fills.push_back(paintJson(p));
        j["fills"] = std::move(fills);
    }
    if (!n.strokes.empty()) {
        json strokes = json::array();
        for (const auto& p : n.strokes) strokes.push_back(paintJson(p));
        j["strokes"] = std::move(strokes);
        j["strokeWeight"] = n.strokeWeight;
        j["strokeAlign"] = n.strokeAlign == StrokeAlign::Inside    ? "INSIDE"
                           : n.strokeAlign == StrokeAlign::Outside ? "OUTSIDE"
                                                                   : "CENTER";
        if (!n.strokeDashes.empty()) j["strokeDashes"] = n.strokeDashes;
        if (n.strokeCap != "NONE") j["strokeCap"] = n.strokeCap;
        if (n.strokeJoin != "MITER") j["strokeJoin"] = n.strokeJoin;
    }

    if (n.cornerRadius > 0) j["cornerRadius"] = n.cornerRadius;
    if (n.rectangleCornerRadii) {
        const auto& r = *n.rectangleCornerRadii;
        j["rectangleCornerRadii"] = json::array({r[0], r[1], r[2], r[3]});
    }

    auto geometryJson = [](const std::vector<PathGeometry>& geoms) {
        json arr = json::array();
        for (const auto& g : geoms) {
            arr.push_back({{"path", g.path},
                           {"windingRule", g.evenOdd ? "EVENODD" : "NONZERO"}});
        }
        return arr;
    };
    if (!n.fillGeometry.empty()) j["fillGeometry"] = geometryJson(n.fillGeometry);
    if (!n.strokeGeometry.empty()) j["strokeGeometry"] = geometryJson(n.strokeGeometry);

    if (!n.effects.empty()) {
        json fx = json::array();
        for (const auto& e : n.effects) {
            json ej;
            switch (e.type) {
            case Effect::Type::DropShadow: ej["type"] = "DROP_SHADOW"; break;
            case Effect::Type::InnerShadow: ej["type"] = "INNER_SHADOW"; break;
            case Effect::Type::LayerBlur: ej["type"] = "LAYER_BLUR"; break;
            case Effect::Type::BackgroundBlur: ej["type"] = "BACKGROUND_BLUR"; break;
            }
            if (!e.visible) ej["visible"] = false;
            ej["color"] = colorJson(e.color);
            ej["offset"] = {{"x", e.offsetX}, {"y", e.offsetY}};
            ej["radius"] = e.radius;
            if (e.spread != 0) ej["spread"] = e.spread;
            fx.push_back(std::move(ej));
        }
        j["effects"] = std::move(fx);
    }

    if (n.type == NodeType::Text) {
        j["characters"] = n.characters;
        const TextStyle& ts = n.textStyle;
        json style;
        style["fontFamily"] = ts.fontFamily;
        if (!ts.fontPostScriptName.empty()) style["fontPostScriptName"] = ts.fontPostScriptName;
        style["fontSize"] = ts.fontSize;
        style["fontWeight"] = ts.fontWeight;
        if (ts.italic) style["italic"] = true;
        if (ts.lineHeightPx > 0) style["lineHeightPx"] = ts.lineHeightPx;
        if (ts.letterSpacing != 0) style["letterSpacing"] = ts.letterSpacing;
        style["textAlignHorizontal"] = ts.alignH == TextStyle::AlignH::Center      ? "CENTER"
                                       : ts.alignH == TextStyle::AlignH::Right     ? "RIGHT"
                                       : ts.alignH == TextStyle::AlignH::Justified ? "JUSTIFIED"
                                                                                   : "LEFT";
        style["textAlignVertical"] = ts.alignV == TextStyle::AlignV::Center   ? "CENTER"
                                     : ts.alignV == TextStyle::AlignV::Bottom ? "BOTTOM"
                                                                              : "TOP";
        if (ts.autoResize != "NONE") style["textAutoResize"] = ts.autoResize;
        if (ts.truncateEnding) style["textTruncation"] = "ENDING";
        if (ts.maxLines > 0) style["maxLines"] = ts.maxLines;
        if (ts.paragraphSpacing > 0) style["paragraphSpacing"] = ts.paragraphSpacing;
        if (ts.tabularFigures) style["tabularFigures"] = true;
        j["style"] = std::move(style);
    }

    if (!n.componentId.empty()) j["componentId"] = n.componentId;

    if (!n.numVarBindings.empty()) {
        json vb = json::object();
        for (const auto& [prop, var] : n.numVarBindings) vb[prop] = var;
        j["varBindings"] = std::move(vb);
    }

    if (!n.instanceOverrides.empty()) {
        json ov = json::parse(n.instanceOverrides, nullptr, false);
        if (!ov.is_discarded()) j["instanceOverrides"] = std::move(ov);
    }

    if (!n.children.empty()) {
        json kids = json::array();
        for (const auto& c : n.children) kids.push_back(nodeJson(*c));
        j["children"] = std::move(kids);
    }
    return j;
}

}  // namespace

std::string writeDocumentJson(const Document& doc) {
    json root;
    root["name"] = doc.name;
    if (!doc.variables.empty()) {
        const VariableTable& vt = doc.variables;
        json vars;
        vars["modes"] = vt.modes;
        if (vt.activeMode >= 0 && vt.activeMode < static_cast<int>(vt.modes.size())) {
            vars["activeMode"] = vt.modes[static_cast<size_t>(vt.activeMode)];
        }
        json colors = json::object();
        for (const auto& v : vt.vars) {
            json values = json::array();
            for (const auto& c : v.values) values.push_back(colorToHex(c));
            colors[v.name] = std::move(values);
        }
        if (!vt.vars.empty()) vars["colors"] = std::move(colors);
        if (!vt.numVars.empty()) {
            json numbers = json::object();
            for (const auto& v : vt.numVars) numbers[v.name] = v.values;
            vars["numbers"] = std::move(numbers);
        }
        root["variables"] = std::move(vars);
    }
    root["document"] = doc.root ? nodeJson(*doc.root) : json::object();
    return root.dump(1, '\t');
}

bool saveDocumentFile(const Document& doc, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const std::string text = writeDocumentJson(doc);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return f.good();
}

}  // namespace figo
