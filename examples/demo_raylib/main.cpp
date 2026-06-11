// figmalib demo: a Figma file rendered as the UI of a raylib "game".
//
// The animated starfield is the game scene; everything else on screen comes
// from examples/assets/sample_ui.json (Figma REST API format).

#include <cmath>
#include <string>

#include <raylib.h>

#include <figmalib/figmalib.h>
#include <figmalib_raylib.h>

#ifndef ASSETS_DIR
#define ASSETS_DIR "."
#endif

int main(int argc, char** argv) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(900, 640, "figmalib - Figma UI in raylib");

    // Optional argument: a .fig file, fig2json canvas.json, or REST API JSON.
    const std::string input = argc > 1 ? argv[1] : ASSETS_DIR "/sample_ui.json";
    auto ui = figmalib::FigmaUI::fromFile(input);

    // Hover feedback: dim any button the pointer is over.
    for (const char* name : {"btn-start", "btn-options", "btn-quit", "btn-back"}) {
        ui->onHover(name, [&ui](figmalib::Node& n, bool entered) {
            ui->setOpacity(n.name, entered ? 0.8f : -1.0f);  // <0 restores authored
        });
    }

    bool quit = false;
    int runs = 0;
    ui->onClick("btn-start", [&](figmalib::Node&) {
        ++runs;
        ui->setText("subtitle", "Game started! (run #" + std::to_string(runs) + ")");
    });
    ui->onClick("btn-options", [&](figmalib::Node&) { ui->selectFrame("Settings"); });
    ui->onClick("btn-back", [&](figmalib::Node&) { ui->selectFrame("MainMenu"); });
    ui->onClick("btn-quit", [&](figmalib::Node&) { quit = true; });

    figmalib::RaylibFigmaView view(*ui);

    // Left/right arrows page through frames (useful for multi-screen .fig files).
    const auto frames = ui->frameNames();
    int frameIdx = 0;

    while (!WindowShouldClose() && !quit) {
        if (!frames.empty() && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT))) {
            const int dir = IsKeyPressed(KEY_RIGHT) ? 1 : -1;
            frameIdx = (frameIdx + dir + static_cast<int>(frames.size())) %
                       static_cast<int>(frames.size());
            ui->selectFrame(frames[frameIdx]);
        }
        view.resize(GetScreenWidth(), GetScreenHeight());
        view.update();

        BeginDrawing();
        ClearBackground(Color{10, 8, 20, 255});

        // "Game" layer: drifting stars behind the (semi-transparent) Figma UI.
        const float t = static_cast<float>(GetTime());
        const int w = GetScreenWidth(), h = GetScreenHeight();
        for (int i = 0; i < 64; ++i) {
            const float speed = 12.0f + (i % 7) * 9.0f;
            const float x = std::fmod(i * 137.0f + t * speed, static_cast<float>(w + 40)) - 20;
            const float y = std::fmod(i * 251.0f, static_cast<float>(h));
            const float r = 1.0f + (i % 3);
            const unsigned char a = static_cast<unsigned char>(90 + (i * 53) % 120);
            DrawCircleV({x, y}, r, Color{200, 210, 255, a});
        }

        view.draw();
        DrawFPS(8, 8);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
