#pragma once
// Builds a ThorVG scene graph from a figo node tree.

#include <array>
#include <string>
#include <vector>

#include <thorvg.h>

#include "figo/document.h"
#include "font_provider.h"

namespace figo {

// One scrolled child of a scrolling frame: enough to retarget its scene (and
// its own clipper, which does not inherit the scene's local transform) when
// only the scroll offset changes — scrolling never rebuilds the scene graph.
struct ScrollBinding {
    Node* scroller = nullptr;     // the scrolling frame
    Node* child = nullptr;        // the shifted child (subtree root)
    tvg::Scene* scene = nullptr;  // owned by the canvas scene graph
    tvg::Shape* clip = nullptr;   // child's own clipper, if any
    Mat23 baseLocal;              // child local matrix without the scroll shift
};

// One node carrying a visible BACKGROUND_BLUR effect: the renderer fills
// `holder` (an empty scene slot behind the node's fills, clipped to the node
// outline) with a blurred capture of whatever rendered below the node — a
// two-pass raster in Renderer::render(). Recaptured every render, so the
// glass stays live under scrolling. CPU (SwCanvas) only; the GL path leaves
// the holder empty (translucent-panel degradation).
struct BackdropBinding {
    Node* node = nullptr;           // node with the BACKGROUND_BLUR effect
    tvg::Scene* nodeScene = nullptr;  // the node's own scene (hidden in pass 1)
    tvg::Scene* holder = nullptr;   // capture slot behind the fills
    float radius = 0;               // Figma blur radius
    uint8_t builtOpacity = 255;     // nodeScene opacity to restore after pass 1
};

struct BuildContext {
    FontProvider* fonts = nullptr;
    std::string imageDir;
    std::vector<ScrollBinding>* scrollBindings = nullptr;  // filled during build
    std::vector<BackdropBinding>* backdropBindings = nullptr;  // filled during build
};

// Returns a scene for the node subtree, or nullptr if the node is hidden.
// `isRoot` drops the node's own canvas-position transform so the frame renders
// at origin. Side effect: fills in node.absoluteTransform (in root-frame space)
// for hit-testing.
tvg::Scene* buildNodeScene(Node& node, const Mat23& parentAbs, BuildContext& ctx,
                           bool isRoot = false);

// Measures a TEXT node's content with the same tokenize/wrap pass the
// renderer uses: wrapped at maxWidth (<= 0 → no wrapping). Outputs the widest
// line and the total line-box height. Returns false when fonts are missing.
bool measureTextNode(const Node& n, float maxWidth, BuildContext& ctx,
                     float& outWidth, float& outHeight);

// Nearest caret position (UTF-8 byte offset into Node::characters) for a
// node-local point, using the same flow as rendering. Clicks left/right of a
// line clamp to its ends; y clamps to the first/last line. -1 when fonts are
// missing; 0 for empty text.
int textByteFromPoint(const Node& n, BuildContext& ctx, float x, float y);

// Node-local {x, y, w, h} rectangles covering the byte range [a, b) — one per
// flowed line touched. Used for the selection highlight. False when empty.
bool textSelectionRects(const Node& n, int a, int b, BuildContext& ctx,
                        std::vector<std::array<float, 4>>& outRects);

}  // namespace figo
