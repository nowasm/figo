#pragma once
// figmalib — responsive layout engine.
//
// Reflows a frame subtree to a target size using Figma's constraints
// (LEFT/CENTER/RIGHT/STRETCH/SCALE per axis) and auto-layout stacks
// (direction, padding, spacing, grow, align, hug sizing). Geometry is always
// recomputed from the authored snapshot (Node::base*), so repeated resizes
// are stable and never accumulate error.

#include "document.h"

namespace figmalib {

// Restore the authored geometry of the whole subtree.
void resetLayout(Node& root);

// Resize `frame` to width × height and reflow its subtree.
void layoutFrame(Node& frame, float width, float height);

}  // namespace figmalib
