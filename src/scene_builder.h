#pragma once
// Builds a ThorVG scene graph from a figmalib node tree.

#include <string>

#include <thorvg.h>

#include "figmalib/document.h"
#include "font_provider.h"

namespace figmalib {

struct BuildContext {
    FontProvider* fonts = nullptr;
    std::string imageDir;
};

// Returns a scene for the node subtree, or nullptr if the node is hidden.
// `isRoot` drops the node's own canvas-position transform so the frame renders
// at origin. Side effect: fills in node.absoluteTransform (in root-frame space)
// for hit-testing.
tvg::Scene* buildNodeScene(Node& node, const Mat23& parentAbs, BuildContext& ctx,
                           bool isRoot = false);

}  // namespace figmalib
