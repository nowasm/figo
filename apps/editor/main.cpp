// figoedit — Figma-style editor built on figo + raylib + raygui.
//
//   figoedit [file.fig | canvas.json | file.json]
//
// Drag & drop a file onto the window to open it. Ctrl+S saves a figo
// JSON next to the original (never overwrites the .fig).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "editor.h"
#include "editor_mcp.h"

#include <nlohmann/json.hpp>
#include <raygui.h>

#ifndef ASSETS_DIR
#define ASSETS_DIR "."
#endif

namespace fs = std::filesystem;
using namespace figoedit;

namespace {

void registerConventionFonts(EditorState& ed, const std::string& inputPath) {
    std::error_code ec;
    ed.fontDirs.clear();
    const fs::path sibling = fs::path(inputPath).parent_path() / "fonts";
    if (fs::is_directory(sibling, ec)) ed.fontDirs.push_back(sibling.string());
    if (const char* env = std::getenv("FIGO_FONTS_DIR"); env && *env) {
        if (fs::is_directory(env, ec)) ed.fontDirs.push_back(env);
    }
    for (const auto& dir : ed.fontDirs) ed.renderer.registerFontsFromDirectory(dir);
}

}  // namespace

namespace figoedit {

bool openFile(EditorState& ed, const std::string& path) {
    try {
        figo::LoadedFile loaded = figo::loadFigmaFile(path);
        ed.invalidateCache();  // before the old document (and its nodes) dies
        ed.file = std::move(loaded);
        ed.filePath = path;
        ed.savePath = path + ".figo.json";
        ed.imageDir = ed.file.imageDirectory;
        if (ed.imageDir.empty()) {
            // Adopt the sibling "<doc>.assets" folder that import_image writes
            // to, so IMAGE fills saved into a .figo.json still resolve on reopen
            // — whether the user reopens the original input or the saved file.
            std::error_code ec;
            for (const std::string& base : {ed.savePath, ed.filePath}) {
                const fs::path assets = fs::path(base).replace_extension(".assets");
                if (fs::is_directory(assets, ec)) { ed.imageDir = assets.string(); break; }
            }
        }
        if (!ed.imageDir.empty()) ed.renderer.setImageDirectory(ed.imageDir);
        registerConventionFonts(ed, path);
        ed.undoStack.clear();
        ed.redoStack.clear();
        ed.expanded.clear();
        ed.unsaved = false;
        ed.selectPage(0);
        ed.updateAbsoluteTransforms();
        // Rebuild the UI font with the document's codepoints (layer names may
        // be CJK; the atlas only carries what is actually used).
        rebuildUiFontFor(*ed.file.document);
        GuiSetFont(gUiFont);
        ed.setStatus("Opened " + path);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "figoedit: %s\n", e.what());
        ed.setStatus(std::string("Open failed: ") + e.what(), 5);
        return false;
    }
}

void saveFile(EditorState& ed) {
    if (!ed.file.document) return;
    if (figo::saveDocumentFile(*ed.file.document, ed.savePath)) {
        ed.unsaved = false;
        ed.setStatus("Saved " + ed.savePath);
    } else {
        ed.setStatus("Save FAILED: " + ed.savePath, 5);
    }
}

void saveFileAs(EditorState& ed) {
    if (!ed.file.document) return;
    const std::string path = showSaveFileDialog(ed.savePath);
    if (path.empty()) return;
    ed.savePath = path;
    saveFile(ed);
}

// Play: write the live document to disk, then launch figoplay on it as a
// detached process. If the file belongs to an app project (a sibling app.json
// whose "design" resolves to it), the whole project plays — viewport, libs,
// fonts, entryFrame all apply. Otherwise the plain two-file form runs with a
// sibling app.js / <stem>.js, or an empty stub script for a static preview.
void playPreview(EditorState& ed) {
    if (!ed.file.document) return;
    namespace fs = std::filesystem;
    std::error_code ec;

#ifdef _WIN32
    const fs::path exe = fs::path(GetApplicationDirectory()) / "figoplay.exe";
#else
    const fs::path exe = fs::path(GetApplicationDirectory()) / "figoplay";
#endif
    if (!fs::exists(exe, ec)) {
        ed.setStatus("Play: figoplay not found next to figoedit — build it first", 5);
        return;
    }

    const fs::path dir = fs::absolute(fs::path(ed.filePath), ec).parent_path();
    auto sameFile = [&](const fs::path& a, const fs::path& b) {
        std::error_code e;
        return fs::exists(a, e) && fs::exists(b, e) && fs::equivalent(a, b, e);
    };

    // App project: play the directory so app.json config applies.
    const fs::path manifest = dir / "app.json";
    if (fs::exists(manifest, ec)) {
        std::string designRel;
        try {
            nlohmann::json m;
            std::ifstream in(manifest);
            in >> m;
            designRel = m.value("design", std::string());
        } catch (const std::exception&) {
            // fall through to the two-file form below
        }
        const fs::path design = dir / designRel;
        if (!designRel.empty() &&
            (sameFile(design, ed.filePath) || sameFile(design, ed.savePath))) {
            // The live document must be what figoplay loads. The app's design
            // is a figo JSON (never a .fig) — writing it is a regular save.
            if (design.extension() == ".json" &&
                !figo::saveDocumentFile(*ed.file.document, design.string())) {
                ed.setStatus("Play: cannot write " + design.string(), 5);
                return;
            }
            if (sameFile(design, ed.savePath)) ed.unsaved = false;
            if (spawnDetached(exe.string(), {dir.string()})) {
                ed.setStatus("Playing app " + dir.string(), 4);
            } else {
                ed.setStatus("Play: failed to launch figoplay", 5);
            }
            return;
        }
    }

    // Plain design file: launch the two-file form. An untouched document
    // plays from the original input (a .fig / canvas.json keeps its extracted
    // images directory, which a saved .figo.json loses); an edited one is
    // saved first and plays from the save path.
    fs::path design = fs::absolute(fs::path(ed.filePath), ec);
    if (ed.unsaved || !ed.undoStack.empty()) {
        saveFile(ed);
        design = fs::absolute(fs::path(ed.savePath), ec);
    }
    fs::path script = dir / "app.js";
    if (!fs::exists(script, ec)) {
        script = fs::absolute(fs::path(ed.filePath), ec);
        while (script.has_extension()) script.replace_extension();  // strip .fig(.figo.json)
        script += ".js";
    }
    if (!fs::exists(script, ec)) {
        // No logic — a stub script still gives a scrollable static preview.
        script = fs::temp_directory_path(ec) / "figoedit_play_stub.js";
        std::ofstream(script) << "// figoedit static preview\n";
    }
    if (spawnDetached(exe.string(), {design.string(), script.string()})) {
        ed.setStatus("Playing " + design.filename().string() + " + " +
                         script.filename().string(),
                     4);
    } else {
        ed.setStatus("Play: failed to launch figoplay", 5);
    }
}

}  // namespace figoedit

// Headless logic check: document mutations, undo/redo, tree ops, save
// round-trip — everything below the mouse wiring. Run: figoedit --selftest [file]
static int selftest(const std::string& path) {
    int failures = 0;
    auto expect = [&](bool ok, const char* what) {
        std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok) ++failures;
    };

    EditorState ed;
    figo::LoadedFile loaded = figo::loadFigmaFile(path);
    ed.file = std::move(loaded);
    Node* page = nullptr;
    for (auto& c : ed.file.document->root->children) {
        if (c->type == figo::NodeType::Canvas) {
            page = c.get();
            break;
        }
    }
    if (!page) page = ed.file.document->root.get();
    ed.page = page;
    ed.scope = page;
    expect(!page->children.empty(), "page has frames");
    Node* frame = page->children.front().get();

    // move + undo
    const float x0 = frame->relativeTransform.m02;
    ed.selection = {frame};
    ed.beginGesture();
    frame->relativeTransform.m02 = x0 + 100;
    ed.gestureChanged = true;
    ed.commitGesture();
    expect(frame->relativeTransform.m02 == x0 + 100, "move applied");
    ed.undo();
    expect(frame->relativeTransform.m02 == x0, "move undone");
    ed.redo();
    expect(frame->relativeTransform.m02 == x0 + 100, "move redone");
    ed.undo();

    // duplicate + delete + undo
    const size_t count = page->children.size();
    ed.selection = {frame};
    ed.duplicateSelection();
    expect(page->children.size() == count + 1, "duplicate adds a sibling");
    ed.deleteSelection();  // deletes the duplicate (now selected)
    expect(page->children.size() == count, "delete removes it");
    ed.undo();
    expect(page->children.size() == count + 1, "undo restores deleted node");
    ed.undo();
    expect(page->children.size() == count, "undo removes the duplicate");

    // save round-trip
    const std::string tmp = path + ".selftest.json";
    expect(figo::saveDocumentFile(*ed.file.document, tmp), "save");
    auto reloaded = figo::loadFigmaFile(tmp);
    Node* rpage = nullptr;
    for (auto& c : reloaded.document->root->children) {
        if (c->type == figo::NodeType::Canvas) {
            rpage = c.get();
            break;
        }
    }
    expect(rpage && rpage->children.size() == count, "round-trip frame count");
    if (rpage && !rpage->children.empty()) {
        Node* rframe = rpage->children.front().get();
        expect(std::fabs(rframe->relativeTransform.m02 - x0) < 0.01f &&
                   rframe->width == frame->width && rframe->name == frame->name,
               "round-trip frame props");
    }
    std::remove(tmp.c_str());

    // ---- theme variables (Theme panel plumbing): value edit, rename
    // propagation, undo/redo through UndoEntry::vars ----
    {
        figo::Document& doc = *ed.file.document;
        if (frame->fills.empty()) {
            figo::Paint p;
            p.type = figo::PaintType::Solid;
            frame->fills.push_back(p);
        }
        doc.variables.set("accent", figo::Color{1, 0, 0, 1});
        frame->fills[0].colorVar = "accent";
        doc.applyVariables();
        expect(frame->fills[0].color.r == 1.0f && frame->fills[0].color.b == 0.0f,
               "color var resolves into bound fill");

        figo::VariableTable before = doc.variables;
        doc.variables.set("accent", figo::Color{0, 0, 1, 1});
        ed.pushVarsUndo(std::move(before));
        doc.applyVariables();
        expect(frame->fills[0].color.b == 1.0f && frame->fills[0].color.r == 0.0f,
               "var value edit retints bound fill");
        ed.undo();
        expect(frame->fills[0].color.r == 1.0f, "var value edit undone");
        ed.redo();
        expect(frame->fills[0].color.b == 1.0f, "var value edit redone");

        // Rename + binding rewrite land in one undo entry (panel rename flow).
        before = doc.variables;
        std::vector<NodeProps> touched{NodeProps::capture(frame)};
        frame->fills[0].colorVar = "primary";
        for (auto& v : doc.variables.vars)
            if (v.name == "accent") v.name = "primary";
        ed.pushVarsUndo(std::move(before), std::move(touched));
        doc.applyVariables();
        expect(doc.variables.get("primary", "light") != nullptr &&
                   frame->fills[0].colorVar == "primary",
               "var rename rewrites binding");
        ed.undo();
        expect(doc.variables.get("accent", "light") != nullptr &&
                   doc.variables.get("primary", "light") == nullptr &&
                   frame->fills[0].colorVar == "accent",
               "var rename undone (table + binding)");
    }

    // ---- MCP dispatch (headless, no sockets) ----
    using nlohmann::json;
    auto rpc = [&](const json& msg) {
        const std::string out = mcpHandleMessageForTest(ed, msg.dump());
        return out.empty() ? json() : json::parse(out);
    };
    auto call = [&](const char* tool, json args) {
        return rpc({{"jsonrpc", "2.0"},
                    {"id", 1},
                    {"method", "tools/call"},
                    {"params", {{"name", tool}, {"arguments", std::move(args)}}}});
    };
    auto callText = [&](const char* tool, json args) {
        const json r = call(tool, std::move(args));
        return json::parse(r["result"]["content"][0]["text"].get<std::string>());
    };

    json init = rpc({{"jsonrpc", "2.0"},
                     {"id", 1},
                     {"method", "initialize"},
                     {"params",
                      {{"protocolVersion", "2025-03-26"},
                       {"capabilities", json::object()},
                       {"clientInfo", {{"name", "selftest"}, {"version", "0"}}}}}});
    expect(init["result"]["serverInfo"]["name"] == "figoedit", "mcp initialize");
    json tools = rpc({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}});
    expect(tools["result"]["tools"].is_array() && tools["result"]["tools"].size() >= 15,
           "mcp tools/list");

    json state = callText("get_editor_state", json::object());
    expect(state.contains("pages"), "mcp get_editor_state");

    json created = callText("create_node",
                            {{"type", "rectangle"},
                             {"x", 10},
                             {"y", 20},
                             {"width", 80},
                             {"height", 40},
                             {"fill", "#FF8800"},
                             {"cornerRadius", 8},
                             {"name", "mcp-rect"}});
    const std::string newIdStr = created.value("id", "");
    expect(!newIdStr.empty() && ed.file.document->findById(newIdStr) != nullptr,
           "mcp create_node");
    Node* created_node = ed.file.document->findById(newIdStr);
    expect(created_node && created_node->cornerRadius == 8 &&
               created_node->relativeTransform.m02 == 10,
           "mcp create_node props");

    json upd = call("update_nodes",
                    {{"updates",
                      json::array({{{"id", newIdStr}, {"x", 50}, {"fill", "#00FF00"}}})}});
    expect(!upd["result"].value("isError", false) &&
               created_node->relativeTransform.m02 == 50,
           "mcp update_nodes");
    ed.undo();
    expect(created_node->relativeTransform.m02 == 10, "mcp update is undoable");

    json shot = call("get_screenshot", {{"nodeId", newIdStr}, {"maxSize", 128}});
    expect(shot["result"]["content"][0]["type"] == "image" &&
               !shot["result"]["content"][0]["data"].get<std::string>().empty(),
           "mcp get_screenshot");

    json dup = callText("duplicate_node", {{"id", newIdStr}});
    const std::string dupId = dup.value("id", "");
    expect(!dupId.empty() && dupId != newIdStr &&
               ed.file.document->findById(dupId) != nullptr,
           "mcp duplicate_node");

    // move the duplicate into the first frame, then undo (reparent round-trip)
    json moved = call("move_node", {{"id", dupId}, {"parentId", frame->id}});
    Node* dupNode = ed.file.document->findById(dupId);
    const bool moveOk = !moved["result"].value("isError", false) && dupNode &&
                        dupNode->parent == frame;
    if (!moveOk)
        std::printf("  move_node response: %s (frame id '%s')\n",
                    moved.dump().c_str(), frame->id.c_str());
    expect(moveOk, "mcp move_node");
    ed.undo();
    expect(dupNode->parent == page, "mcp move_node undo restores parent");

    json del = call("delete_nodes", {{"ids", json::array({dupId, newIdStr})}});
    expect(!del["result"].value("isError", false) &&
               ed.file.document->findById(newIdStr) == nullptr,
           "mcp delete_nodes");
    ed.undo();
    expect(ed.file.document->findById(newIdStr) != nullptr, "mcp delete undo");

    json bad = call("update_nodes",
                    {{"updates", json::array({{{"id", "no-such-node"}, {"x", 1}}})}});
    expect(bad["result"].value("isError", false), "mcp bad id reports tool error");

    // Numeric design tokens: set_variables {numbers} + update_nodes varBindings
    // (the binding must resolve to the active mode's value in the same call).
    json varsSet = call(
        "set_variables",
        {{"numbers", {{"space", {{"light", 14}, {"dark", 24}}}}}});
    const float* spaceTok = ed.file.document->variables.getNumber("space", "light");
    expect(!varsSet["result"].value("isError", false) && spaceTok && *spaceTok == 14,
           "mcp set_variables numbers");
    json bind = call("update_nodes",
                     {{"updates", json::array({{{"id", newIdStr},
                                                {"varBindings",
                                                 {{"cornerRadius", "space"}}}}})}});
    Node* bound = ed.file.document->findById(newIdStr);
    expect(!bind["result"].value("isError", false) && bound &&
               bound->numVarBindings.size() == 1 && bound->cornerRadius == 14,
           "mcp varBindings bind+resolve");
    json badBind = call("update_nodes",
                        {{"updates", json::array({{{"id", newIdStr},
                                                   {"varBindings",
                                                    {{"rotation", "space"}}}}})}});
    expect(badBind["result"].value("isError", false), "mcp varBindings bad prop errors");

    // Override-preserving instance sync: master frame + text child → instance
    // → per-instance text override → master edits → sync. The override must
    // survive, the master's fill change must propagate.
    json mkFrame = callText("create_node", {{"type", "frame"},
                                            {"x", 200},
                                            {"y", 10},
                                            {"width", 120},
                                            {"height", 60},
                                            {"fill", "#222222"},
                                            {"name", "card-master"}});
    const std::string masterId = mkFrame.value("id", "");
    json mkText = callText("create_node", {{"type", "text"},
                                           {"parentId", masterId},
                                           {"x", 8},
                                           {"y", 8},
                                           {"text", "Title"},
                                           {"name", "Heading"}});
    const std::string masterTextId = mkText.value("id", "");
    expect(!masterId.empty() && !masterTextId.empty(), "mcp component scaffolding");
    call("make_component", {{"id", masterId}});
    json instJson = callText("create_instance",
                             {{"componentId", masterId}, {"x", 200}, {"y", 100}});
    const std::string instId = instJson.value("id", "");
    Node* instNode = ed.file.document->findById(instId);
    expect(instNode != nullptr && instNode->type == figo::NodeType::Instance,
           "mcp create_instance");
    Node* instHeading = instNode ? instNode->findByName("Heading") : nullptr;
    expect(instHeading != nullptr, "mcp instance heading exists");
    call("update_nodes",
         {{"updates", json::array({{{"id", instHeading->id}, {"text", "Custom"}}})}});
    expect(instNode && !instNode->instanceOverrides.empty(), "mcp override recorded");
    call("update_nodes",
         {{"updates", json::array({{{"id", masterId}, {"fill", "#3355ff"}},
                                   {{"id", masterTextId}, {"text", "NewTitle"}}})}});
    json syncRes = callText("sync_instances", {{"componentId", masterId}});
    instNode = ed.file.document->findById(instId);  // fresh subtree, same id
    instHeading = instNode ? instNode->findByName("Heading") : nullptr;
    expect(instNode && instHeading && instHeading->characters == "Custom",
           "mcp sync preserves override");
    expect(instNode && !instNode->fills.empty() &&
               figo::colorToHex(instNode->fills.front().color) == "#3355ff",
           "mcp sync propagates master edit");
    expect(syncRes.value("droppedOverrides", -1) == 0, "mcp sync keeps ledger");

    // audit_design quality checks: off-scale spacing + dark-parity contrast.
    const std::string tokensPath = path + ".selftest-tokens.json";
    {
        std::ofstream tf(tokensPath, std::ios::binary);
        tf << R"({"tokens":[
            {"name":"--bg","value":"#ffffff","type":"color"},
            {"name":"--space-sm","value":"8px","type":"dimension"},
            {"name":"--space-md","value":"16px","type":"dimension"}]})";
    }
    json alFrame = callText("create_node", {{"type", "frame"},
                                            {"x", 400},
                                            {"y", 10},
                                            {"width", 100},
                                            {"height", 100},
                                            {"fill", "#ffffff"},
                                            {"name", "audit-stack"}});
    call("update_nodes",
         {{"updates",
           json::array({{{"id", alFrame.value("id", "")},
                         {"autoLayout", {{"mode", "VERTICAL"}, {"itemSpacing", 13}}}}})}});
    call("set_variables",
         {{"colors",
           {{"ink", {{"light", "#111111"}, {"dark", "#0a0a0a"}}},
            {"paper", {{"light", "#ffffff"}, {"dark", "#111111"}}}}}});
    json bgF = callText("create_node",
                        {{"type", "frame"},
                         {"x", 520},
                         {"y", 10},
                         {"width", 120},
                         {"height", 60},
                         {"fill", {{"type", "SOLID"}, {"colorVar", "paper"}}},
                         {"name", "audit-bg"}});
    json inkTxt = callText("create_node", {{"type", "text"},
                                           {"parentId", bgF.value("id", "")},
                                           {"x", 8},
                                           {"y", 8},
                                           {"text", "Sample"},
                                           {"name", "audit-ink"}});
    call("update_nodes",
         {{"updates", json::array({{{"id", inkTxt.value("id", "")},
                                    {"fill", {{"type", "SOLID"}, {"colorVar", "ink"}}}}})}});
    json auditOut = callText("audit_design", {{"tokensPath", tokensPath}});
    expect(auditOut["summary"].value("offSpacing", 0) >= 1, "mcp audit off-spacing");
    expect(auditOut["summary"].value("lowContrastMode", 0) >= 1, "mcp audit dark parity");
    std::remove(tokensPath.c_str());

    // import_svg with a clipPath: the circle def becomes an isMask child in
    // front of the clipped content (D6 mask semantics at runtime).
    json svgRes = callText(
        "import_svg",
        {{"data", "<svg viewBox=\"0 0 100 100\">"
                  "<defs><clipPath id=\"c\"><circle cx=\"50\" cy=\"50\" r=\"40\"/>"
                  "</clipPath></defs>"
                  "<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#ff0000\" "
                  "clip-path=\"url(#c)\"/></svg>"},
         {"name", "clip-test"}});
    Node* svgRoot = ed.file.document->findById(svgRes.value("id", ""));
    Node* clipGroup = svgRoot ? svgRoot->findByName("Clip Group") : nullptr;
    expect(clipGroup && clipGroup->children.size() == 2 &&
               clipGroup->children.front()->isMask &&
               !clipGroup->children.front()->fills.empty() &&
               clipGroup->children.front()->fills.front().color.r > 0.99f,
           "mcp import_svg clipPath -> isMask");

    std::printf(failures ? "SELFTEST: %d failure(s)\n" : "SELFTEST: OK\n", failures);
    return failures;
}

int main(int argc, char** argv) {
    // MCP flags can appear anywhere; strip them before positional parsing.
    // Default: serve MCP on 127.0.0.1:9223 (FIGOEDIT_MCP_PORT overrides).
    int mcpPortWanted = 9223;
    if (const char* env = std::getenv("FIGOEDIT_MCP_PORT"); env && *env)
        mcpPortWanted = std::atoi(env);
    std::vector<char*> args;
    args.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-mcp") == 0) {
            mcpPortWanted = 0;
        } else if (std::strcmp(argv[i], "--mcp-port") == 0 && i + 1 < argc) {
            mcpPortWanted = std::atoi(argv[++i]);
        } else {
            args.push_back(argv[i]);
        }
    }
    argc = static_cast<int>(args.size());
    argv = args.data();

    if (argc > 1 && std::string(argv[1]) == "--selftest") {
        const std::string file = argc > 2 ? argv[2] : ASSETS_DIR "/sample_ui.json";
        return selftest(file);
    }
    // --screenshot <out.png> [file]: render ~40 frames with the first frame
    // auto-selected, export the framebuffer, exit. Non-interactive UI check.
    std::string shotPath;
    int shotArgOffset = 0;
    if (argc > 2 && std::string(argv[1]) == "--screenshot") {
        shotPath = argv[2];
        shotArgOffset = 2;
    }

    // No FLAG_WINDOW_HIGHDPI: GLFW already marks the process DPI-aware on
    // Windows (window units are physical pixels, no bitmap stretching), and
    // the flag desyncs raylib's internal scale matrix after SetWindowSize,
    // pushing the right-hand panels outside the framebuffer.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1440, 900, "figoedit");
    SetExitKey(0);  // Esc is a selection command, not quit
    SetTargetFPS(120);
    figoedit::initUiScale();
    figoedit::initUiFont();
    figoedit::applyEditorTheme();
    GuiSetFont(figoedit::gUiFont);
    GuiSetStyle(DEFAULT, TEXT_SIZE, figoedit::fontM());
    // Make the window comfortable on the current monitor.
    {
        const int mon = GetCurrentMonitor();
        const int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
        SetWindowSize(static_cast<int>(mw * 0.85f), static_cast<int>(mh * 0.85f));
        SetWindowPosition(static_cast<int>(mw * 0.06f), static_cast<int>(mh * 0.05f));
    }

    std::fprintf(stderr, "[dpi] screen=%dx%d render=%dx%d scale=%.2f uiScale=%.2f\n",
                 GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(),
                 GetWindowScaleDPI().x, gUiScale);

    EditorState ed;
    const std::string initial =
        argc > 1 + shotArgOffset ? argv[1 + shotArgOffset] : ASSETS_DIR "/sample_ui.json";
    if (!openFile(ed, initial)) {
        // keep the window open so a file can be dropped in
        ed.setStatus("Drop a .fig / .json file to open", 10);
    }
    // Test hook: select the first frame at startup (screenshot automation).
    // FIGOEDIT_AUTOSELECT=1 forces it on, =0 forces it off (e.g. to shoot the
    // no-selection Theme panel); default follows --screenshot.
    const char* autoSel = std::getenv("FIGOEDIT_AUTOSELECT");
    const bool wantAutoSel = autoSel && *autoSel ? *autoSel == '1' : !shotPath.empty();
    if (wantAutoSel && ed.page && !ed.page->children.empty()) {
        ed.selection = {ed.page->children.front().get()};
    }

    // MCP server: lets an AI client design directly in this editor session.
    if (mcpPortWanted > 0) {
        if (figoedit::mcpStart(ed, mcpPortWanted)) {
            std::fprintf(stderr, "[mcp] listening on http://127.0.0.1:%d/mcp\n",
                         mcpPortWanted);
            ed.setStatus("MCP on http://127.0.0.1:" + std::to_string(mcpPortWanted) +
                             "/mcp",
                         6);
        } else {
            std::fprintf(stderr, "[mcp] failed to bind port %d (already in use?)\n",
                         mcpPortWanted);
            ed.setStatus("MCP failed to bind port " + std::to_string(mcpPortWanted), 6);
        }
    }

    int frameCount = 0;
    while (!WindowShouldClose()) {
        if (!shotPath.empty()) {
            // Deterministic screenshot: keep the first frame selected even if
            // stray real-mouse input lands in the window.
            if (wantAutoSel && ed.page && !ed.page->children.empty()) {
                ed.selection = {ed.page->children.front().get()};
            }
            if (++frameCount > 40) {
                Image shot = LoadImageFromScreen();
                ExportImage(shot, shotPath.c_str());
                UnloadImage(shot);
                break;
            }
        }
        // drag & drop
        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0) openFile(ed, dropped.paths[0]);
            UnloadDroppedFiles(dropped);
        }
        // file shortcuts
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrl && IsKeyPressed(KEY_S)) {
            shift ? saveFileAs(ed) : saveFile(ed);
        }
        if (ctrl && IsKeyPressed(KEY_O)) {
            const std::string path = showOpenFileDialog();
            if (!path.empty()) openFile(ed, path);
        }
        if (IsKeyPressed(KEY_F5)) playPreview(ed);

        figoedit::mcpPump(ed);  // run queued AI tool calls on the main thread

        if (ed.file.document && ed.page) updateCanvas(ed);

        BeginDrawing();
        ClearBackground(::Color{30, 30, 30, 255});
        if (ed.file.document && ed.page) drawCanvas(ed);
        drawLayersPanel(ed);
        drawInspector(ed);
        drawToolbar(ed);  // last: the File dropdown overlays the panels
        EndDrawing();
    }

    figoedit::mcpStop();
    ed.invalidateCache();
    CloseWindow();
    return 0;
}
