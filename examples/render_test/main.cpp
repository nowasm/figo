// Offscreen smoke test: render each frame of the sample UI to a BMP file.
// No engine backend involved — exercises parser + scene builder + renderer.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <figmalib/figmalib.h>

#ifndef ASSETS_DIR
#define ASSETS_DIR "."
#endif

// 32-bit BGRA top-down BMP.
static bool writeBmp(const char* path, const uint32_t* rgba, uint32_t w, uint32_t h) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") != 0 || !f) return false;

    const uint32_t imageSize = w * h * 4;
    uint8_t fileHeader[14] = {'B', 'M'};
    const uint32_t fileSize = 14 + 40 + imageSize;
    std::memcpy(fileHeader + 2, &fileSize, 4);
    const uint32_t dataOffset = 54;
    std::memcpy(fileHeader + 10, &dataOffset, 4);

    uint8_t infoHeader[40] = {};
    const uint32_t infoSize = 40;
    const int32_t negH = -static_cast<int32_t>(h);  // top-down
    const uint16_t planes = 1, bpp = 32;
    std::memcpy(infoHeader + 0, &infoSize, 4);
    std::memcpy(infoHeader + 4, &w, 4);
    std::memcpy(infoHeader + 8, &negH, 4);
    std::memcpy(infoHeader + 12, &planes, 2);
    std::memcpy(infoHeader + 14, &bpp, 2);
    std::memcpy(infoHeader + 20, &imageSize, 4);

    fwrite(fileHeader, 1, 14, f);
    fwrite(infoHeader, 1, 40, f);

    // RGBA (R first in memory) → BGRA, compositing onto dark gray so
    // transparency is visible in the output.
    std::vector<uint8_t> row(w * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&rgba[y * w + x]);
            const float a = p[3] / 255.0f;
            row[x * 4 + 0] = static_cast<uint8_t>(p[2] * a + 40 * (1 - a));
            row[x * 4 + 1] = static_cast<uint8_t>(p[1] * a + 40 * (1 - a));
            row[x * 4 + 2] = static_cast<uint8_t>(p[0] * a + 40 * (1 - a));
            row[x * 4 + 3] = 255;
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    // Optional argument: any supported input (.fig, canvas.json, REST JSON).
    const std::string input = argc > 1 ? argv[1] : ASSETS_DIR "/sample_ui.json";
    std::printf("loading %s\n", input.c_str());
    auto ui = figmalib::FigmaUI::fromFile(input);
    ui->setViewport(900, 640);

    int failures = 0;
    std::vector<std::string> seen;
    for (const auto& frameName : ui->frameNames()) {
        // selectFrame() picks by name; duplicate names would re-select the
        // same node and render() would correctly report "nothing changed".
        if (std::find(seen.begin(), seen.end(), frameName) != seen.end()) continue;
        seen.push_back(frameName);
        if (!ui->selectFrame(frameName)) {
            std::printf("FAIL: selectFrame(%s)\n", frameName.c_str());
            ++failures;
            continue;
        }
        if (!ui->render()) {
            std::printf("FAIL: render(%s) produced no pixels\n", frameName.c_str());
            ++failures;
            continue;
        }
        // Sanity: count non-transparent pixels.
        const uint32_t* px = ui->pixels();
        const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
        uint32_t opaque = 0;
        for (uint32_t i = 0; i < total; ++i) {
            if (reinterpret_cast<const uint8_t*>(&px[i])[3] > 8) ++opaque;
        }
        std::string safe = frameName;
        for (char& c : safe) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '_';
        }
        const std::string out = "render_" + safe + ".bmp";
        writeBmp(out.c_str(), px, ui->pixelWidth(), ui->pixelHeight());
        std::printf("%s: %ux%u, %.1f%% painted -> %s\n", frameName.c_str(),
                    ui->pixelWidth(), ui->pixelHeight(), 100.0 * opaque / total,
                    out.c_str());
        if (opaque < total / 10) {
            std::printf("FAIL: %s mostly empty\n", frameName.c_str());
            ++failures;
        }
    }

    // Hit-test sanity on the sample UI: center of the start button.
    if (ui->selectFrame("MainMenu")) {
        ui->render();
        if (figmalib::Node* hit = ui->hitTest(450, 320)) {
            std::printf("hitTest(450,320) -> %s\n", hit->name.c_str());
        } else {
            std::printf("FAIL: hitTest(450,320) -> null\n");
            ++failures;
        }
    }

    std::printf(failures ? "RESULT: %d failure(s)\n" : "RESULT: OK\n", failures);
    return failures;
}
