#include "figmalib/ui.h"

#include <cstdlib>
#include <filesystem>
#include <unordered_map>

#include "figmalib/parser.h"

namespace figmalib {

struct FigmaUI::Impl {
    std::unique_ptr<Document> doc;
    Renderer renderer;
    Node* frame = nullptr;
    Node* hovered = nullptr;
    Node* pressed = nullptr;
    std::unordered_map<std::string, std::vector<ClickHandler>> clickHandlers;
    std::unordered_map<std::string, std::vector<HoverHandler>> hoverHandlers;

    // Point in root-frame coordinates → deepest visible hit node.
    Node* hitTestFrame(Node& node, float fx, float fy) {
        if (!node.effectivelyVisible() || node.type == NodeType::Slice) return nullptr;

        const auto inv = node.absoluteTransform.inverted();
        if (!inv) return nullptr;
        float lx, ly;
        inv->apply(fx, fy, lx, ly);
        const bool inside =
            lx >= 0 && ly >= 0 && lx <= node.width && ly <= node.height;

        if (node.clipsContent && !inside) return nullptr;

        // Topmost child wins.
        for (auto it = node.children.rbegin(); it != node.children.rend(); ++it) {
            if (Node* hit = hitTestFrame(**it, fx, fy)) return hit;
        }

        if (!inside) return nullptr;
        const bool paintable = !node.fills.empty() || !node.strokes.empty() ||
                               !node.fillGeometry.empty() || !node.characters.empty();
        return paintable ? &node : nullptr;
    }

    Node* hitTestViewport(float x, float y) {
        if (!frame) return nullptr;
        const auto inv = renderer.contentTransform().inverted();
        if (!inv) return nullptr;
        float fx, fy;
        inv->apply(x, y, fx, fy);
        return hitTestFrame(*frame, fx, fy);
    }

    // Fires handlers registered for the node or any of its ancestors.
    template <typename Map, typename Fn>
    void fireUp(Map& handlers, Node* node, Fn&& invoke) {
        for (Node* n = node; n; n = n->parent) {
            if (auto it = handlers.find(n->name); it != handlers.end()) {
                for (auto& h : it->second) invoke(h, *n);
            }
            if (n == frame) break;
        }
    }

    Node* findMutable(const std::string& name) {
        Node* base = frame ? frame : (doc ? doc->root.get() : nullptr);
        return base ? base->findByName(name) : nullptr;
    }
};

FigmaUI::FigmaUI() : impl_(std::make_unique<Impl>()) {}
FigmaUI::~FigmaUI() = default;

std::unique_ptr<FigmaUI> FigmaUI::fromFile(const std::string& path) {
    auto ui = std::unique_ptr<FigmaUI>(new FigmaUI());
    LoadedFile loaded = loadFigmaFile(path);  // .fig / canvas.json / REST JSON
    ui->impl_->doc = std::move(loaded.document);
    if (!loaded.imageDirectory.empty()) {
        ui->impl_->renderer.setImageDirectory(loaded.imageDirectory);
    }
    // Conventions for design fonts: a "fonts" directory next to the input
    // file, plus the FIGMALIB_FONTS_DIR environment variable.
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path sibling = fs::path(path).parent_path() / "fonts";
        if (fs::is_directory(sibling, ec)) {
            ui->impl_->renderer.registerFontsFromDirectory(sibling.string());
        }
        if (const char* env = std::getenv("FIGMALIB_FONTS_DIR"); env && *env) {
            if (fs::is_directory(env, ec)) {
                ui->impl_->renderer.registerFontsFromDirectory(env);
            }
        }
    }
    auto frames = ui->impl_->doc->topLevelFrames();
    if (!frames.empty()) {
        ui->impl_->frame = frames.front();
        ui->impl_->renderer.setFrame(frames.front());
    }
    return ui;
}

std::unique_ptr<FigmaUI> FigmaUI::fromJson(const std::string& jsonText) {
    auto ui = std::unique_ptr<FigmaUI>(new FigmaUI());
    ui->impl_->doc = parseDocument(jsonText);
    auto frames = ui->impl_->doc->topLevelFrames();
    if (!frames.empty()) {
        ui->impl_->frame = frames.front();
        ui->impl_->renderer.setFrame(frames.front());
    }
    return ui;
}

Document& FigmaUI::document() { return *impl_->doc; }
Renderer& FigmaUI::renderer() { return impl_->renderer; }

std::vector<std::string> FigmaUI::frameNames() const {
    std::vector<std::string> names;
    for (Node* f : impl_->doc->topLevelFrames()) names.push_back(f->name);
    return names;
}

bool FigmaUI::selectFrame(const std::string& name) {
    for (Node* f : impl_->doc->topLevelFrames()) {
        if (f->name == name || f->id == name) {
            impl_->frame = f;
            impl_->renderer.setFrame(f);
            impl_->hovered = nullptr;
            impl_->pressed = nullptr;
            return true;
        }
    }
    return false;
}

Node* FigmaUI::currentFrame() const { return impl_->frame; }

void FigmaUI::setViewport(uint32_t width, uint32_t height) {
    impl_->renderer.setTarget(width, height);
}

bool FigmaUI::render() { return impl_->renderer.render(); }
const uint32_t* FigmaUI::pixels() const { return impl_->renderer.pixels(); }
uint32_t FigmaUI::pixelWidth() const { return impl_->renderer.width(); }
uint32_t FigmaUI::pixelHeight() const { return impl_->renderer.height(); }
void FigmaUI::markDirty() { impl_->renderer.markDirty(); }

void FigmaUI::pointerMove(float x, float y) {
    Node* hit = impl_->hitTestViewport(x, y);
    if (hit == impl_->hovered) return;
    if (impl_->hovered) {
        impl_->fireUp(impl_->hoverHandlers, impl_->hovered,
                      [](HoverHandler& h, Node& n) { h(n, false); });
    }
    impl_->hovered = hit;
    if (hit) {
        impl_->fireUp(impl_->hoverHandlers, hit,
                      [](HoverHandler& h, Node& n) { h(n, true); });
    }
}

void FigmaUI::pointerDown(float x, float y) {
    impl_->pressed = impl_->hitTestViewport(x, y);
}

void FigmaUI::pointerUp(float x, float y) {
    Node* hit = impl_->hitTestViewport(x, y);
    if (hit && impl_->pressed) {
        // Click counts if press and release share the hit node or an ancestor.
        Node* target = nullptr;
        for (Node* n = hit; n; n = n->parent) {
            if (n == impl_->pressed) {
                target = n;
                break;
            }
            if (n == impl_->frame) break;
        }
        if (!target) {
            for (Node* n = impl_->pressed; n; n = n->parent) {
                if (n == hit) {
                    target = n;
                    break;
                }
                if (n == impl_->frame) break;
            }
        }
        if (target) {
            impl_->fireUp(impl_->clickHandlers, target,
                          [](ClickHandler& h, Node& n) { h(n); });
        }
    }
    impl_->pressed = nullptr;
}

Node* FigmaUI::hitTest(float x, float y) { return impl_->hitTestViewport(x, y); }
Node* FigmaUI::hoveredNode() const { return impl_->hovered; }
Node* FigmaUI::pressedNode() const { return impl_->pressed; }

void FigmaUI::onClick(const std::string& nodeName, ClickHandler fn) {
    impl_->clickHandlers[nodeName].push_back(std::move(fn));
}

void FigmaUI::onHover(const std::string& nodeName, HoverHandler fn) {
    impl_->hoverHandlers[nodeName].push_back(std::move(fn));
}

bool FigmaUI::setVisible(const std::string& nodeName, bool visible) {
    Node* n = impl_->findMutable(nodeName);
    if (!n) return false;
    n->runtimeVisible = visible ? 1 : 0;
    impl_->renderer.markDirty();
    return true;
}

bool FigmaUI::setOpacity(const std::string& nodeName, float opacity) {
    Node* n = impl_->findMutable(nodeName);
    if (!n) return false;
    n->runtimeOpacity = opacity;
    impl_->renderer.markDirty();
    return true;
}

bool FigmaUI::setText(const std::string& nodeName, const std::string& text) {
    Node* n = impl_->findMutable(nodeName);
    if (!n || n->type != NodeType::Text) return false;
    n->characters = text;
    impl_->renderer.markDirty();
    return true;
}

}  // namespace figmalib
