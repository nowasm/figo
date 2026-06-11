// File-type dispatch for loadFigmaFile():
//   *.fig        → convert via the fig2json CLI into "<file>.export/", parse
//                  the resulting canvas.json, expose its images directory
//   canvas.json  → fig2json format (sniffed from content)
//   *.json       → Figma REST API format

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "figmalib/parser.h"

#ifndef FIGMALIB_FIG2JSON_DEFAULT
#define FIGMALIB_FIG2JSON_DEFAULT "fig2json"
#endif

namespace figmalib {

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("figmalib: cannot open file: " + path.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string lowerExt(const fs::path& p) {
    std::string ext = p.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

// fig2json discovery: env override → compile-time default → bare name (PATH).
std::string fig2jsonExe() {
    if (const char* env = std::getenv("FIGMALIB_FIG2JSON"); env && *env) return env;
    const fs::path def = FIGMALIB_FIG2JSON_DEFAULT;
    std::error_code ec;
    if (def.is_absolute() && fs::exists(def, ec)) return def.string();
    return "fig2json";
}

// Run "fig2json <fig> <outDir>". Results are cached: skip when canvas.json
// already exists and is newer than the .fig.
fs::path convertFig(const fs::path& figPath) {
    const fs::path outDir = figPath.string() + ".export";
    const fs::path canvasJson = outDir / "canvas.json";

    std::error_code ec;
    if (fs::exists(canvasJson, ec)) {
        const auto figTime = fs::last_write_time(figPath, ec);
        const auto jsonTime = fs::last_write_time(canvasJson, ec);
        if (!ec && jsonTime >= figTime) return outDir;
    }

    fs::create_directories(outDir, ec);
    std::string cmd = "\"" + fig2jsonExe() + "\" \"" + figPath.string() + "\" \"" +
                      outDir.string() + "\"";
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";  // cmd.exe strips one outer quote pair
#endif
    const int rc = std::system(cmd.c_str());
    if (rc != 0 || !fs::exists(canvasJson)) {
        throw std::runtime_error(
            "figmalib: fig2json conversion failed (exit " + std::to_string(rc) +
            "). Set FIGMALIB_FIG2JSON to the fig2json executable, or pre-convert with: "
            "fig2json \"" + figPath.string() + "\" \"" + outDir.string() + "\"");
    }
    return outDir;
}

bool looksLikeCanvasJson(const nlohmann::json& root) {
    if (root.contains("components") || root.contains("blobs")) return true;
    auto doc = root.find("document");
    if (doc == root.end() || !doc->is_object()) return false;
    // REST documents always carry "id" + "type"; fig2json output has neither.
    return !doc->contains("id") || !doc->contains("type");
}

}  // namespace

LoadedFile loadFigmaFile(const std::string& path) {
    const fs::path p = path;
    LoadedFile result;

    if (lowerExt(p) == ".fig") {
        const fs::path outDir = convertFig(p);
        result.document = parseCanvasDocument(readFile(outDir / "canvas.json"));
        std::error_code ec;
        const fs::path images = outDir / "images";
        if (fs::is_directory(images, ec)) result.imageDirectory = images.string();
        return result;
    }

    const std::string text = readFile(p);
    nlohmann::json root = nlohmann::json::parse(text);
    if (looksLikeCanvasJson(root)) {
        result.document = parseCanvasDocument(text);
        // canvas.json sits next to its extracted images directory.
        std::error_code ec;
        const fs::path images = p.parent_path() / "images";
        if (fs::is_directory(images, ec)) result.imageDirectory = images.string();
    } else {
        result.document = parseDocument(text);
    }
    return result;
}

}  // namespace figmalib
