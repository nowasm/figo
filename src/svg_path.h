#pragma once
// SVG path-data parser: appends commands from a "d" string onto a tvg::Shape.
// Supports M L H V C S Q T A Z (absolute and relative).

#include <thorvg.h>

namespace figmalib {

bool appendSvgPath(tvg::Shape& shape, const char* d);

}  // namespace figmalib
