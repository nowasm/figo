#pragma once
// Maps Figma (fontFamily, fontWeight, italic) to a TTF registered with ThorVG.
//
// Lookup order: fonts registered via registerFont() → system fonts (Windows
// font registry) → default family → any previously loaded font.
// Each variant is loaded into ThorVG once under a composite key; fontKeyFor()
// returns that key for use with tvg::Text::font().

#include <string>

namespace figmalib {

class FontProvider {
public:
    FontProvider();
    ~FontProvider();

    void registerFont(const std::string& family, const std::string& ttfPath,
                      int weight, bool italic);

    // Scans a directory for .ttf/.otf files and registers each under the
    // family name from its TTF `name` table and the weight/italic flags from
    // its OS/2 table. Returns the number of fonts registered.
    int registerFontsFromDirectory(const std::string& dir);

    void setDefaultFamily(const std::string& family);

    // Returns the ThorVG font key for the best match, or an empty string if
    // no font could be loaded at all.
    std::string fontKeyFor(const std::string& family, int weight, bool italic);

    // Whether the loaded font behind `fontKey` maps `codepoint` to a glyph
    // (cmap lookup). Unknown keys report true so callers don't churn.
    bool hasGlyph(const std::string& fontKey, unsigned long codepoint);

    // Key of a font that does carry `codepoint`, trying common system CJK /
    // symbol families. Empty when nothing covers it.
    std::string fallbackFontFor(unsigned long codepoint, int weight, bool italic);

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace figmalib
