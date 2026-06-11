#include "font_provider.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>

#include <thorvg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace figmalib {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Style-name candidates per CSS weight, most specific first.
const std::vector<std::string>& styleNamesFor(int weight) {
    static const std::map<int, std::vector<std::string>> table = {
        {100, {"thin", "hairline"}},
        {200, {"extralight", "extra light", "ultralight"}},
        {300, {"light"}},
        {400, {"regular", "normal", "book"}},
        {500, {"medium"}},
        {600, {"semibold", "semi bold", "demibold", "demi bold"}},
        {700, {"bold"}},
        {800, {"extrabold", "extra bold", "ultrabold"}},
        {900, {"black", "heavy"}},
    };
    auto it = table.lower_bound(weight - 49);
    if (it == table.end()) --it;
    return it->second;
}

struct SystemFont {
    std::string displayName;  // lowercased registry value name, "(truetype)" stripped
    std::string path;
};

#ifdef _WIN32
std::vector<SystemFont> enumerateSystemFonts() {
    std::vector<SystemFont> fonts;
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0,
                      KEY_READ, &key) != ERROR_SUCCESS) {
        return fonts;
    }
    char winDir[MAX_PATH] = {};
    GetWindowsDirectoryA(winDir, MAX_PATH);
    const std::string fontDir = std::string(winDir) + "\\Fonts\\";

    for (DWORD i = 0;; ++i) {
        char name[512];
        BYTE data[1024];
        DWORD nameLen = sizeof(name), dataLen = sizeof(data), type = 0;
        const LONG rc = RegEnumValueA(key, i, name, &nameLen, nullptr, &type, data, &dataLen);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS || type != REG_SZ) continue;

        std::string display = toLower(name);
        for (const char* suffix : {" (truetype)", " (opentype)"}) {
            const size_t pos = display.find(suffix);
            if (pos != std::string::npos) display.resize(pos);
        }
        std::string file(reinterpret_cast<char*>(data));
        // ThorVG's loader handles .ttf/.otf; skip collections and bitmap fonts.
        const std::string lowerFile = toLower(file);
        if (lowerFile.size() < 4) continue;
        const std::string ext = lowerFile.substr(lowerFile.size() - 4);
        if (ext != ".ttf" && ext != ".otf") continue;

        std::string path = file.find(':') != std::string::npos ? file : fontDir + file;
        fonts.push_back({display, path});
    }
    RegCloseKey(key);
    return fonts;
}
#else
std::vector<SystemFont> enumerateSystemFonts() { return {}; }
#endif

// ---- minimal sfnt (TTF/OTF) metadata reader --------------------------------
// Pulls the typographic family from the `name` table (nameID 16, falling back
// to 1) and weight/italic from the OS/2 table. Enough to register arbitrary
// font files under the exact family names Figma documents reference.

struct SfntInfo {
    std::string family;
    int weight = 400;
    bool italic = false;
};

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
uint32_t be32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 | p[3];
}

bool parseSfnt(const std::vector<uint8_t>& d, SfntInfo& out) {
    if (d.size() < 12) return false;
    const uint32_t tag = be32(d.data());
    if (tag != 0x00010000 && tag != 0x4F54544F /*OTTO*/ && tag != 0x74727565 /*true*/)
        return false;
    const uint16_t numTables = be16(d.data() + 4);
    if (d.size() < 12 + numTables * 16u) return false;

    uint32_t nameOff = 0, nameLen = 0, os2Off = 0, os2Len = 0;
    for (uint16_t i = 0; i < numTables; ++i) {
        const uint8_t* rec = d.data() + 12 + i * 16;
        const uint32_t t = be32(rec);
        const uint32_t off = be32(rec + 8), len = be32(rec + 12);
        if (t == 0x6E616D65) { nameOff = off; nameLen = len; }        // 'name'
        else if (t == 0x4F532F32) { os2Off = off; os2Len = len; }     // 'OS/2'
    }

    if (os2Off && os2Len >= 64 && os2Off + 64 <= d.size()) {
        const uint16_t weightClass = be16(d.data() + os2Off + 4);
        if (weightClass >= 100 && weightClass <= 1000) out.weight = weightClass;
        const uint16_t fsSelection = be16(d.data() + os2Off + 62);
        out.italic = (fsSelection & 0x01) != 0;
    }

    if (!nameOff || nameLen < 6 || nameOff + nameLen > d.size()) return false;
    const uint8_t* nt = d.data() + nameOff;
    const uint16_t count = be16(nt + 2);
    const uint16_t strOff = be16(nt + 4);
    std::string family1, family16;
    for (uint16_t i = 0; i < count; ++i) {
        const uint8_t* rec = nt + 6 + i * 12;
        if (rec + 12 > d.data() + nameOff + nameLen) break;
        const uint16_t platform = be16(rec), encoding = be16(rec + 2);
        const uint16_t nameId = be16(rec + 6);
        const uint16_t len = be16(rec + 8), off = be16(rec + 10);
        if (nameId != 1 && nameId != 16) continue;
        const uint8_t* s = nt + strOff + off;
        if (s + len > d.data() + nameOff + nameLen) continue;
        std::string value;
        if (platform == 3 || (platform == 0)) {  // UTF-16BE
            for (uint16_t k = 0; k + 1 < len; k += 2) {
                const uint16_t cp = be16(s + k);
                if (cp < 0x80) value += static_cast<char>(cp);
                else value += '?';  // family matching is ASCII-insensitive anyway
            }
        } else if (platform == 1 && encoding == 0) {  // Mac Roman ≈ ASCII
            value.assign(reinterpret_cast<const char*>(s), len);
        } else {
            continue;
        }
        if (value.empty()) continue;
        if (nameId == 16 && family16.empty()) family16 = value;
        if (nameId == 1 && family1.empty()) family1 = value;
    }
    out.family = !family16.empty() ? family16 : family1;
    return !out.family.empty();
}

}  // namespace

struct FontProvider::Impl {
    std::vector<SystemFont> systemFonts;
    bool systemFontsLoaded = false;
    // (family|weight|italic) → explicit path
    std::unordered_map<std::string, std::string> registered;
    // composite key → loaded into ThorVG
    std::unordered_map<std::string, bool> loaded;
    std::string defaultFamily = "Segoe UI";
    std::string lastGoodKey;

    static std::string composite(const std::string& family, int weight, bool italic) {
        return toLower(family) + "|" + std::to_string(weight) + (italic ? "|i" : "");
    }

    const std::vector<SystemFont>& system() {
        if (!systemFontsLoaded) {
            systemFonts = enumerateSystemFonts();
            systemFontsLoaded = true;
        }
        return systemFonts;
    }

    std::string findSystemPath(const std::string& family, int weight, bool italic) {
        const std::string fam = toLower(family);
        const auto& styles = styleNamesFor(weight);

        // Build candidate display names, most specific first.
        std::vector<std::string> candidates;
        for (const auto& s : styles) {
            if (italic) candidates.push_back(fam + " " + s + " italic");
        }
        if (italic) candidates.push_back(fam + " italic");
        for (const auto& s : styles) candidates.push_back(fam + " " + s);
        if (weight < 550 && !italic) candidates.push_back(fam + " regular");
        candidates.push_back(fam);

        for (const auto& want : candidates) {
            for (const auto& f : system()) {
                if (f.displayName == want) return f.path;
            }
        }
        // Last resort: any face whose name starts with the family.
        for (const auto& f : system()) {
            if (f.displayName.rfind(fam + " ", 0) == 0) return f.path;
        }
        return {};
    }

    // Loads the TTF under `key` so tvg::Text::font(key) resolves. Returns success.
    bool loadAs(const std::string& key, const std::string& path) {
        if (auto it = loaded.find(key); it != loaded.end()) return it->second;

        std::ifstream f(path, std::ios::binary);
        bool ok = false;
        if (f) {
            std::vector<char> data((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
            if (!data.empty()) {
                ok = tvg::Text::load(key.c_str(), data.data(),
                                     static_cast<uint32_t>(data.size()), "ttf",
                                     true /*copy*/) == tvg::Result::Success;
            }
        }
        loaded[key] = ok;
        if (ok) lastGoodKey = key;
        return ok;
    }
};

FontProvider::FontProvider() : impl_(new Impl) {}
FontProvider::~FontProvider() { delete impl_; }

void FontProvider::registerFont(const std::string& family, const std::string& ttfPath,
                                int weight, bool italic) {
    impl_->registered[Impl::composite(family, weight, italic)] = ttfPath;
}

int FontProvider::registerFontsFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    int count = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string ext = toLower(entry.path().extension().string());
        if (ext != ".ttf" && ext != ".otf") continue;

        std::ifstream f(entry.path(), std::ios::binary);
        if (!f) continue;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());

        SfntInfo info;
        if (!parseSfnt(data, info)) continue;
        registerFont(info.family, entry.path().string(), info.weight, info.italic);
        ++count;
    }
    return count;
}

void FontProvider::setDefaultFamily(const std::string& family) {
    impl_->defaultFamily = family;
}

std::string FontProvider::fontKeyFor(const std::string& family, int weight, bool italic) {
    const std::string key = Impl::composite(family, weight, italic);

    if (auto it = impl_->registered.find(key); it != impl_->registered.end()) {
        if (impl_->loadAs(key, it->second)) return key;
    }
    // No exact registered match: pick the registered face of this family with
    // the nearest weight, preferring the requested italic flag.
    {
        const std::string famPrefix = toLower(family) + "|";
        std::string bestPath;
        int bestScore = 1 << 30;
        for (const auto& [k, path] : impl_->registered) {
            if (k.rfind(famPrefix, 0) != 0) continue;
            const size_t bar = famPrefix.size() - 1;
            const size_t bar2 = k.find('|', bar + 1);
            const int w = std::atoi(k.substr(bar + 1, bar2 == std::string::npos
                                                          ? std::string::npos
                                                          : bar2 - bar - 1).c_str());
            const bool ital = k.size() > 2 && k.compare(k.size() - 2, 2, "|i") == 0;
            const int score = std::abs(w - weight) + (ital != italic ? 1000 : 0);
            if (score < bestScore) {
                bestScore = score;
                bestPath = path;
            }
        }
        if (!bestPath.empty() && impl_->loadAs(key, bestPath)) return key;
    }

    if (std::string path = impl_->findSystemPath(family, weight, italic); !path.empty()) {
        if (impl_->loadAs(key, path)) return key;
    }

    if (toLower(family) != toLower(impl_->defaultFamily)) {
        return fontKeyFor(impl_->defaultFamily, weight, italic);
    }
    // Default family also missing — try Arial, then anything we ever loaded.
    if (toLower(family) != "arial") {
        if (std::string path = impl_->findSystemPath("Arial", weight, italic); !path.empty()) {
            if (impl_->loadAs(key, path)) return key;
        }
    }
    return impl_->lastGoodKey;
}

}  // namespace figmalib
