#pragma once
// figmalib — high-level UI runtime.
//
// Owns a parsed Figma document plus a renderer and adds frame selection,
// hit-testing, pointer input and per-node callbacks. This is the API game
// code talks to; engine backends only shuttle pixels and input events.

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "document.h"
#include "renderer.h"

namespace figmalib {

class FigmaUI {
public:
    using ClickHandler = std::function<void(Node&)>;
    using HoverHandler = std::function<void(Node&, bool entered)>;

    static std::unique_ptr<FigmaUI> fromFile(const std::string& path);
    static std::unique_ptr<FigmaUI> fromJson(const std::string& jsonText);
    ~FigmaUI();

    Document& document();
    Renderer& renderer();

    // ---- Frames ----
    std::vector<std::string> frameNames() const;
    bool selectFrame(const std::string& name);  // also accepts a node id
    Node* currentFrame() const;

    // ---- Render ----
    void setViewport(uint32_t width, uint32_t height);
    bool render();  // true → pixel buffer changed, re-upload texture
    const uint32_t* pixels() const;
    uint32_t pixelWidth() const;
    uint32_t pixelHeight() const;
    void markDirty();

    // ---- Input (viewport/pixel coordinates) ----
    void pointerMove(float x, float y);
    void pointerDown(float x, float y);
    void pointerUp(float x, float y);

    // Deepest visible node under the point, or nullptr.
    Node* hitTest(float x, float y);
    Node* hoveredNode() const;
    Node* pressedNode() const;

    // Callbacks fire for the named node or any ancestor of the hit node with
    // that name (so a click on a button's label still triggers the button).
    void onClick(const std::string& nodeName, ClickHandler fn);
    void onHover(const std::string& nodeName, HoverHandler fn);

    // ---- Runtime mutation (marks dirty, returns false if node not found) ----
    bool setVisible(const std::string& nodeName, bool visible);
    bool setOpacity(const std::string& nodeName, float opacity);  // <0 resets to authored
    bool setText(const std::string& nodeName, const std::string& text);

private:
    FigmaUI();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace figmalib
