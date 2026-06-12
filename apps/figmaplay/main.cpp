// figmaplay: the generic script player — an app is <design.fig> + <logic.js>.
//
//   figmaplay [design.fig] [logic.js] [--selfdrive prefix]
//             [--shot out.png] [--frames N]
//
// All behavior lives in the script (see figmalib/script.h for the JS API);
// this host only loads the two files and runs the frame loop. With no
// arguments it plays the wallet demo (examples/scripts/wallet.js).
// The script hot-reloads: save the .js and the running app rebuilds its
// script world in place (design/document state stays). localStorage persists
// next to the script as <script>.storage.json.
// Verification exits (for AI / CI loops):
//   --shot out.png [--frames N]  render N frames (default 30), save a
//                                screenshot, quit. The script can stage state
//                                first (globalThis.SHOT is defined).
//   --selfdrive prefix           the script drives its own tour (SELFDRIVE is
//                                defined); saves <prefix>_home/nav.png.
//
// Web build (emscripten): the design ships pre-converted (canvas.json +
// images) in the preloaded FS together with the script and fonts — see the
// EMSCRIPTEN defaults below and the figmaplay target in CMakeLists.txt.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include <raylib.h>

#include <figmalib/figmalib.h>
#include <figmalib/script.h>
#include <figmalib_raylib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

struct Player {
    std::string design, script, shotPath;
    const char* drivePrefix = nullptr;
    int shotFrames = 30;

    std::unique_ptr<figmalib::FigmaUI> ui;
    std::unique_ptr<figmalib::ScriptHost> host;
    std::unique_ptr<figmalib::RaylibFigmaView> view;
    std::filesystem::file_time_type scriptStamp;
    std::error_code fsEc;
    int frame = 0;
    int watchTick = 0;
    bool done = false;

    bool loadScript() {
        ui->clearHandlers();  // the script re-registers everything it needs
        host = std::make_unique<figmalib::ScriptHost>(*ui);
        host->setStoragePath(script + ".storage.json");
        if (drivePrefix) host->eval("globalThis.SELFDRIVE = true;", "<selfdrive>");
        if (!shotPath.empty()) host->eval("globalThis.SHOT = true;", "<shot>");
        const bool ok = host->runFile(script);
        ui->markDirty();
        return ok;
    }

    void tick() {
        // Hot reload: rebuild the script world when the .js changes on disk.
        if (++watchTick >= 20) {  // ~3x per second
            watchTick = 0;
            const auto now = std::filesystem::last_write_time(script, fsEc);
            if (!fsEc && now != scriptStamp) {
                scriptStamp = now;
                std::printf("[figmaplay] reloading %s\n", script.c_str());
                std::fflush(stdout);
                loadScript();
            }
        }
        if ((IsKeyPressed(KEY_BACKSPACE) && !ui->focusedNode()) ||
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            ui->navigateBack();
        }
        view->resize(GetScreenWidth(), GetScreenHeight());
        host->update(GetFrameTime());  // timers, onUpdate, fetch results
        view->update();

        BeginDrawing();
        ClearBackground(Color{12, 14, 18, 255});
        view->draw();
        EndDrawing();

        if (!shotPath.empty()) {
            if (++frame >= shotFrames) {
                TakeScreenshot(shotPath.c_str());
                std::printf("[figmaplay] screenshot -> %s\n", shotPath.c_str());
                done = true;
            }
        } else if (drivePrefix) {
            ++frame;
            if (frame == 30) {
                TakeScreenshot((std::string(drivePrefix) + "_home.png").c_str());
            } else if (frame == 110) {
                TakeScreenshot((std::string(drivePrefix) + "_nav.png").c_str());
            } else if (frame >= 140) {
                done = true;
            }
        }
    }
};

Player* g_player = nullptr;

#ifdef __EMSCRIPTEN__
void emFrame() { g_player->tick(); }
#endif

}  // namespace

int main(int argc, char** argv) {
    auto* p = new Player();  // outlives main on the web (set_main_loop unwinds)
    g_player = p;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selfdrive" && i + 1 < argc) p->drivePrefix = argv[++i];
        else if (arg == "--shot" && i + 1 < argc) p->shotPath = argv[++i];
        else if (arg == "--frames" && i + 1 < argc) p->shotFrames = std::atoi(argv[++i]);
        else if (arg.size() > 3 && arg.compare(arg.size() - 3, 3, ".js") == 0) p->script = arg;
        else p->design = arg;
    }
#ifdef __EMSCRIPTEN__
    if (p->design.empty()) p->design = "/assets/wallet/canvas.json";
    if (p->script.empty()) p->script = "/scripts/wallet.js";
#else
    if (p->design.empty()) {
        for (const char* cand : {"wallet.fig", "../wallet.fig",
                                 "D:/work_open/fig2psd/test/figma/wallet.fig"}) {
            if (FILE* f = fopen(cand, "rb")) {
                fclose(f);
                p->design = cand;
                break;
            }
        }
    }
    if (p->script.empty()) p->script = std::string(EXAMPLES_DIR) + "/scripts/wallet.js";
#endif
    if (p->design.empty()) {
        std::printf("usage: figmaplay [design.fig] [logic.js] [--selfdrive prefix] "
                    "[--shot out.png] [--frames N]\n");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(420, 900, "figmaplay — design.fig + logic.js");

    p->ui = figmalib::FigmaUI::fromFile(p->design);
#ifdef __EMSCRIPTEN__
    // No system fonts in the browser: the design's font files ship in the
    // preloaded FS.
    p->ui->renderer().registerFontsFromDirectory("/fonts");
#endif
    if (!p->loadScript()) {
        CloseWindow();
        return 1;
    }
    p->scriptStamp = std::filesystem::last_write_time(p->script, p->fsEc);
    p->view = std::make_unique<figmalib::RaylibFigmaView>(*p->ui);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(emFrame, 0 /*use rAF*/, 1 /*never returns*/);
#else
    while (!WindowShouldClose() && !p->done) p->tick();
    CloseWindow();
    delete p;
#endif
    return 0;
}
