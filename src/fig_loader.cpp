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

#include "figo/parser.h"

#ifndef FIGO_FIG2JSON_DEFAULT
#define FIGO_FIG2JSON_DEFAULT "fig2json"
#endif

#ifdef FIGO_HAVE_FIG2JSON_LIB
// In-process conversion via the fig2json static library (src/capi.rs).
extern "C" {
char* fig2json_convert_file(const char* figPath, const char* outDir, char** err);
void fig2json_free(char* s);
}
#endif

namespace figo {

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("figo: cannot open file: " + path.string());
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
    if (const char* env = std::getenv("FIGO_FIG2JSON"); env && *env) return env;
    const fs::path def = FIGO_FIG2JSON_DEFAULT;
    std::error_code ec;
    if (def.is_absolute() && fs::exists(def, ec)) return def.string();
    return "fig2json";
}

// Run "fig2json <fig> <outDir>". Results are cached: skip when canvas.json
// already exists and is newer than both the .fig and the fig2json executable
// (an upgraded converter may emit fields older caches are missing).
fs::path convertFig(const fs::path& figPath) {
    const fs::path outDir = figPath.string() + ".export";
    const fs::path canvasJson = outDir / "canvas.json";

    std::error_code ec;
    if (fs::exists(canvasJson, ec)) {
        const auto figTime = fs::last_write_time(figPath, ec);
        const auto jsonTime = fs::last_write_time(canvasJson, ec);
        bool fresh = !ec && jsonTime >= figTime;
        if (fresh) {
            std::error_code exeEc;
            const fs::path exe = fig2jsonExe();
            const auto exeTime = fs::last_write_time(exe, exeEc);
            if (!exeEc && exeTime > jsonTime) fresh = false;
        }
        if (fresh) return outDir;
    }

    fs::create_directories(outDir, ec);

#ifdef FIGO_HAVE_FIG2JSON_LIB
    // In-process conversion: writes canvas.json + images/ into outDir, same
    // layout as the CLI. Falls through to the CLI only on failure.
    {
        char* err = nullptr;
        char* json = fig2json_convert_file(figPath.string().c_str(),
                                           outDir.string().c_str(), &err);
        if (err) fig2json_free(err);
        if (json) {
            fig2json_free(json);
            if (fs::exists(canvasJson, ec)) return outDir;
        }
    }
#endif

    std::string cmd = "\"" + fig2jsonExe() + "\" \"" + figPath.string() + "\" \"" +
                      outDir.string() + "\"";
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";  // cmd.exe strips one outer quote pair
#endif
    const int rc = std::system(cmd.c_str());
    if (rc != 0 || !fs::exists(canvasJson)) {
        throw std::runtime_error(
            "figo: fig2json conversion failed (exit " + std::to_string(rc) +
            "). Set FIGO_FIG2JSON to the fig2json executable, or pre-convert with: "
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

}  // namespace figo
