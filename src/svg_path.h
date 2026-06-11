#pragma once
// SVG path-data parser: appends commands from a "d" string onto a tvg::Shape.
// Supports M L H V C S Q T A Z (absolute and relative).

#include <thorvg.h>

namespace figmalib {

// sx/sy scale every coordinate as it is parsed — used by responsive layout to
// stretch authored vector geometry to a node's current size without touching
// stroke widths (a shape-level transform would scale those too).
bool appendSvgPath(tvg::Shape& shape, const char* d, float sx = 1.0f, float sy = 1.0f);

}  // namespace figmalib
