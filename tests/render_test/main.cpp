// Offscreen smoke test: render each frame of the sample UI to a BMP file.
// No engine backend involved — exercises parser + scene builder + renderer.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <figo/figo.h>

#ifndef ASSETS_DIR
#define ASSETS_DIR "."
#endif

// 32-bit BGRA top-down BMP.
static bool writeBmp(const char* path, const uint32_t* rgba, uint32_t w, uint32_t h) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0 || !f) return false;
#else
    f = std::fopen(path, "wb");
    if (!f) return false;
#endif

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
    auto ui = figo::FigmaUI::fromFile(input);
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

    // Responsive reflow: render the first frame at a different aspect ratio
    // with constraints/auto-layout active instead of scale-to-fit.
    if (!seen.empty() && ui->selectFrame(seen.front())) {
        ui->setResizeMode(figo::FigmaUI::ResizeMode::Reflow);
        ui->setViewport(1280, 720);
        if (!ui->render()) {
            std::printf("FAIL: reflow render produced no pixels\n");
            ++failures;
        } else {
            const uint32_t* px = ui->pixels();
            const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
            uint32_t opaque = 0;
            for (uint32_t i = 0; i < total; ++i) {
                if (reinterpret_cast<const uint8_t*>(&px[i])[3] > 8) ++opaque;
            }
            writeBmp("render_reflow.bmp", px, ui->pixelWidth(), ui->pixelHeight());
            std::printf("reflow %s: %ux%u, %.1f%% painted -> render_reflow.bmp\n",
                        seen.front().c_str(), ui->pixelWidth(), ui->pixelHeight(),
                        100.0 * opaque / total);
            if (opaque < total / 10) {
                std::printf("FAIL: reflow render mostly empty\n");
                ++failures;
            }
        }
        ui->setResizeMode(figo::FigmaUI::ResizeMode::Scale);
        ui->setViewport(900, 640);
    }

    // Scrolling: find a scrollable frame, scroll it to the end, and verify
    // the pixels actually moved (and that fixed children kept the frame from
    // being a pure translate — any change at all passes).
    {
        figo::Node* scrollable = nullptr;
        for (const auto& frameName : seen) {
            if (!ui->selectFrame(frameName)) continue;
            ui->currentFrame()->visit([&](figo::Node& n) {
                if (!scrollable && n.scrolls() &&
                    (n.maxScrollX() > 0 || n.maxScrollY() > 0)) {
                    scrollable = &n;
                }
                return true;
            });
            if (scrollable) break;
        }
        if (!scrollable) {
            std::printf("scroll: no scrollable node in document (skipped)\n");
        } else if (!ui->render()) {
            std::printf("FAIL: scroll baseline render\n");
            ++failures;
        } else {
            const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
            std::vector<uint32_t> before(ui->pixels(), ui->pixels() + total);
            const float toX = scrollable->maxScrollX(), toY = scrollable->maxScrollY();
            if (!ui->setScroll(scrollable->name, toX, toY) || !ui->render()) {
                std::printf("FAIL: setScroll(%s)\n", scrollable->name.c_str());
                ++failures;
            } else {
                uint32_t diff = 0;
                for (uint32_t i = 0; i < total; ++i) {
                    if (ui->pixels()[i] != before[i]) ++diff;
                }
                writeBmp("render_scrolled.bmp", ui->pixels(), ui->pixelWidth(),
                         ui->pixelHeight());
                std::printf("scroll %s by (%.0f, %.0f): %.1f%% pixels changed -> "
                            "render_scrolled.bmp\n",
                            scrollable->name.c_str(), toX, toY, 100.0 * diff / total);
                if (diff == 0) {
                    std::printf("FAIL: scrolling changed nothing\n");
                    ++failures;
                }
            }
            // Resize while scrolled: a viewport tall enough to swallow the
            // scroll range must pull the offset back in (window-resize bug).
            ui->setResizeMode(figo::FigmaUI::ResizeMode::Reflow);
            ui->setViewport(900, 640);
            ui->setScroll(scrollable->name, scrollable->maxScrollX(),
                          scrollable->maxScrollY());
            ui->setViewport(900, 2400);
            ui->render();
            if (scrollable->scrollY > scrollable->maxScrollY() + 0.01f ||
                scrollable->scrollX > scrollable->maxScrollX() + 0.01f) {
                std::printf("FAIL: scroll offset (%.1f, %.1f) beyond range "
                            "(%.1f, %.1f) after resize\n",
                            scrollable->scrollX, scrollable->scrollY,
                            scrollable->maxScrollX(), scrollable->maxScrollY());
                ++failures;
            } else {
                std::printf("scroll clamp after resize: (%.1f, %.1f) within "
                            "(%.1f, %.1f)\n",
                            scrollable->scrollX, scrollable->scrollY,
                            scrollable->maxScrollX(), scrollable->maxScrollY());
            }
            ui->setResizeMode(figo::FigmaUI::ResizeMode::Scale);
            ui->setViewport(900, 640);
            ui->setScroll(scrollable->name, 0, 0);
        }
    }

    // Text measurement drives layout: in Reflow mode an auto-height text node
    // must grow when a longer string wraps to more lines.
    {
        figo::Node* textNode = nullptr;
        for (const auto& frameName : seen) {
            if (!ui->selectFrame(frameName)) continue;
            ui->currentFrame()->visit([&](figo::Node& n) {
                if (!textNode && n.type == figo::NodeType::Text && n.width > 40 &&
                    !n.characters.empty()) {
                    textNode = &n;
                }
                return true;
            });
            if (textNode) break;
        }
        if (!textNode) {
            std::printf("text-measure: no text node (skipped)\n");
        } else {
            const std::string savedChars = textNode->characters;
            const std::string savedResize = textNode->textStyle.autoResize;
            textNode->textStyle.autoResize = "HEIGHT";
            ui->setResizeMode(figo::FigmaUI::ResizeMode::Reflow);
            ui->setViewport(900, 640);
            ui->render();
            const float before = textNode->height;
            // Mutate the node directly — setText() resolves by name, which can
            // collide with the frame itself (union.fig "Practice designs").
            textNode->characters =
                "The quick brown fox jumps over the lazy dog again and "
                "again and again until the line is far too long to fit";
            textNode->textRuns.clear();
            ui->setViewport(899, 640);  // size change forces a reflow
            ui->render();
            const float after = textNode->height;
            std::printf("text-measure %s: height %.1f -> %.1f\n",
                        textNode->name.c_str(), before, after);
            if (!(after > before + 1)) {
                std::printf("FAIL: auto-height text did not grow\n");
                ++failures;
            }
            textNode->characters = savedChars;
            textNode->textStyle.autoResize = savedResize;
            ui->setResizeMode(figo::FigmaUI::ResizeMode::Scale);
            ui->setViewport(900, 640);
        }
    }

    // Text editing: focus an editable node, type (incl. CJK), navigate, and
    // delete; the caret must also show up in the pixels.
    {
        figo::Node* textNode = nullptr;
        for (const auto& frameName : seen) {
            if (!ui->selectFrame(frameName)) continue;
            ui->currentFrame()->visit([&](figo::Node& n) {
                if (!textNode && n.type == figo::NodeType::Text &&
                    !n.characters.empty()) {
                    textNode = &n;
                }
                return true;
            });
            if (textNode) break;
        }
        if (!textNode) {
            std::printf("edit: no text node (skipped)\n");
        } else {
            const std::string savedChars = textNode->characters;
            const std::string savedName = textNode->name;
            textNode->name = "__edit_target__";
            textNode->characters = "ab";
            ui->setEditable("__edit_target__");
            ui->render();
            const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
            std::vector<uint32_t> before(ui->pixels(), ui->pixels() + total);

            ui->focusText("__edit_target__");          // caret at end
            ui->textInput("c你好");                     // ab|c你好
            ui->editKey(figo::FigmaUI::EditKey::Backspace);  // drop 好
            ui->editKey(figo::FigmaUI::EditKey::Left);       // skip 你
            ui->editKey(figo::FigmaUI::EditKey::Left);       // skip c
            ui->textInput("X");                         // ab X c你
            bool ok = textNode->characters == "abXc\xe4\xbd\xa0";
            if (!ok) {
                std::printf("FAIL: edit ops produced \"%s\"\n",
                            textNode->characters.c_str());
                ++failures;
            }
            if (!ui->render()) {
                std::printf("FAIL: edit render\n");
                ++failures;
            } else {
                uint32_t diff = 0;
                for (uint32_t i = 0; i < total; ++i) {
                    if (ui->pixels()[i] != before[i]) ++diff;
                }
                std::printf("edit \"%s\": %u pixels changed (text + caret)\n",
                            textNode->characters.c_str(), diff);
                if (diff == 0) {
                    std::printf("FAIL: editing changed no pixels\n");
                    ++failures;
                }
            }
            ui->blur();
            textNode->characters = savedChars;
            textNode->name = savedName;
            textNode->textRuns.clear();
            ui->markDirty();
        }
    }

    // Data-driven list: clone an auto-layout container's template child per
    // item, stamp each with text, and verify stacking + pixel changes.
    {
        figo::Node* list = nullptr;
        for (const auto& frameName : seen) {
            if (!ui->selectFrame(frameName)) continue;
            ui->currentFrame()->visit([&](figo::Node& n) {
                if (!list && n.autoLayout.enabled() && !n.children.empty() &&
                    n.parent && n.width > 0) {
                    // Need a text somewhere in the template to stamp.
                    bool hasText = false;
                    n.children.front()->visit([&](figo::Node& t) {
                        if (t.type == figo::NodeType::Text) hasText = true;
                        return !hasText;
                    });
                    if (hasText) list = &n;
                }
                return true;
            });
            if (list) break;
        }
        if (!list) {
            std::printf("bindList: no auto-layout list candidate (skipped)\n");
        } else {
            const std::string savedName = list->name;
            list->name = "__list__";
            ui->render();
            const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
            std::vector<uint32_t> before(ui->pixels(), ui->pixels() + total);

            const bool ok = ui->bindList("__list__", 5, [](figo::Node& item, size_t i) {
                bool done = false;
                item.visit([&](figo::Node& t) {
                    if (!done && t.type == figo::NodeType::Text) {
                        figo::setNodeText(t, "Item " + std::to_string(i + 1));
                        done = true;
                    }
                    return !done;
                });
            });
            if (!ok || list->children.size() != 5) {
                std::printf("FAIL: bindList -> %zu items\n", list->children.size());
                ++failures;
            } else {
                const auto& a = *list->children[0];
                const auto& b = *list->children[1];
                const bool stacked = a.relativeTransform.m02 != b.relativeTransform.m02 ||
                                     a.relativeTransform.m12 != b.relativeTransform.m12;
                ui->render();
                uint32_t diff = 0;
                for (uint32_t i = 0; i < total; ++i) {
                    if (ui->pixels()[i] != before[i]) ++diff;
                }
                std::printf("bindList %s: 5 items, stacked=%d, %u pixels changed\n",
                            savedName.c_str(), stacked ? 1 : 0, diff);
                if (!stacked || diff == 0) {
                    std::printf("FAIL: bindList items not laid out / not drawn\n");
                    ++failures;
                }
            }
            list->name = savedName;
        }
    }

    // Navigation: transitions composite cached textures in the BACKEND, so
    // the renderer's pixel buffer holds the incoming frame for the whole
    // animation. The library-side contract is: the incoming frame rasterizes
    // once right away, transitionId bumps for the backend snapshot, progress
    // advances while animating, and back returns.
    if (seen.size() >= 2) {
        ui->selectFrame(seen[0]);
        ui->render();
        const uint32_t total = ui->pixelWidth() * ui->pixelHeight();
        ui->selectFrame(seen[1]);
        ui->render();
        std::vector<uint32_t> b(ui->pixels(), ui->pixels() + total);
        ui->selectFrame(seen[0]);
        ui->render();
        const uint32_t idBefore = ui->transitionId();

        if (!ui->navigateTo(seen[1], figo::FigmaUI::Transition::SlideLeft, 0.3f)) {
            std::printf("FAIL: navigateTo(%s)\n", seen[1].c_str());
            ++failures;
        } else {
            const bool idBumped = ui->transitionId() != idBefore;
            // The animation clock clamps each tick to 1/30 s, so step in
            // frame-sized increments like a real host would.
            for (int i = 0; i < 5; ++i) ui->update(1.0f / 30);  // ~halfway
            const bool midAnimating = ui->animating();
            const float midProgress = ui->transitionProgress();
            ui->render();
            uint32_t midDiffB = 0;  // pixels must already show the incoming frame
            for (uint32_t i = 0; i < total; ++i) {
                if (ui->pixels()[i] != b[i]) ++midDiffB;
            }
            for (int i = 0; i < 20; ++i) ui->update(1.0f / 30);  // finish
            const bool doneAnimating = ui->animating();
            const bool backOk =
                ui->canGoBack() && ui->navigateBack(0.0f) && ui->currentFrame() &&
                ui->currentFrame()->name == seen[0];
            std::printf("navigate %s -> %s: id+%d mid(anim=%d p=%.2f diffB=%u) "
                        "end(anim=%d) back=%d\n",
                        seen[0].c_str(), seen[1].c_str(), idBumped ? 1 : 0,
                        midAnimating ? 1 : 0, midProgress, midDiffB,
                        doneAnimating ? 1 : 0, backOk ? 1 : 0);
            if (!idBumped || !midAnimating || midProgress <= 0 || midProgress >= 1 ||
                midDiffB != 0 || doneAnimating || !backOk) {
                std::printf("FAIL: navigation transition wrong\n");
                ++failures;
            }
        }
    }

    // Click-to-navigate consumes the event: nav items share names with their
    // destination frames, so a click must not bubble past the navigation and
    // fire the old frame's own handler (Discover page -> tap Trade ->
    // Marketplace must stick, not bounce back to Discover).
    {
        const bool hasFrames = ui->selectFrame("Discover") &&
                               ui->selectFrame("Marketplace") &&
                               ui->selectFrame("Discover");
        figo::Node* tradeItem =
            hasFrames ? ui->currentFrame()->findByName("Trade") : nullptr;
        if (!tradeItem) {
            std::printf("nav-bubble: no Discover/Trade setup (skipped)\n");
        } else {
            ui->onClick("Trade", [&](figo::Node&, float, float) { ui->navigateTo("Marketplace"); });
            ui->onClick("Discover", [&](figo::Node&, float, float) { ui->navigateTo("Discover"); });
            ui->render();  // refresh absoluteTransform for the hit test
            float cx, cy, vx, vy;
            tradeItem->absoluteTransform.apply(tradeItem->width * 0.5f,
                                               tradeItem->height * 0.5f, cx, cy);
            ui->renderer().contentTransform().apply(cx, cy, vx, vy);
            ui->pointerDown(vx, vy);
            ui->pointerUp(vx, vy);
            ui->update(10.0f);  // finish any transition
            const std::string where =
                ui->currentFrame() ? ui->currentFrame()->name : "(null)";
            std::printf("nav-bubble: Discover + Trade click -> %s\n", where.c_str());
            if (where != "Marketplace") {
                std::printf("FAIL: navigation did not consume the click\n");
                ++failures;
            }
        }
    }

    // Hit-test sanity on the sample UI: center of the start button.
    if (ui->selectFrame("MainMenu")) {
        ui->render();
        if (figo::Node* hit = ui->hitTest(450, 320)) {
            std::printf("hitTest(450,320) -> %s\n", hit->name.c_str());
        } else {
            std::printf("FAIL: hitTest(450,320) -> null\n");
            ++failures;
        }
    }

    // Scroll snapping (snapToChildren): a real-velocity fling must decay,
    // then EASE onto the nearest child boundary, and onScrollEnd must fire
    // exactly once at rest with the snapped index. Uses a purpose-built
    // inline document (frame == viewport, scale 1) so pointer px == frame px.
    {
        std::string json =
            "{\"name\":\"snap\",\"document\":{\"id\":\"0:0\",\"type\":\"DOCUMENT\","
            "\"children\":[{\"id\":\"0:1\",\"type\":\"CANVAS\",\"name\":\"P\","
            "\"children\":[{\"id\":\"1:1\",\"type\":\"FRAME\",\"name\":\"Main\","
            "\"size\":{\"x\":400,\"y\":400},"
            "\"relativeTransform\":[[1,0,0],[0,1,0]],\"clipsContent\":true,"
            "\"fills\":[{\"type\":\"SOLID\",\"color\":{\"r\":1,\"g\":1,\"b\":1,\"a\":1}}],"
            "\"children\":[{\"id\":\"2:1\",\"type\":\"FRAME\",\"name\":\"SnapList\","
            "\"size\":{\"x\":400,\"y\":400},"
            "\"relativeTransform\":[[1,0,0],[0,1,0]],\"clipsContent\":true,"
            "\"overflowDirection\":\"VERTICAL_SCROLLING\","
            "\"fills\":[{\"type\":\"SOLID\",\"color\":{\"r\":0.9,\"g\":0.9,\"b\":0.9,\"a\":1}}],"
            "\"children\":[";
        for (int i = 0; i < 10; ++i) {  // 10 rows of 100 at pitch 110
            if (i) json += ",";
            json += "{\"id\":\"3:" + std::to_string(i) + "\",\"type\":\"RECTANGLE\","
                    "\"name\":\"Row" + std::to_string(i) + "\","
                    "\"size\":{\"x\":400,\"y\":100},"
                    "\"relativeTransform\":[[1,0,0],[0,1," + std::to_string(i * 110) + "]],"
                    "\"fills\":[{\"type\":\"SOLID\",\"color\":{\"r\":0.5,\"g\":0.5,\"b\":0.8,\"a\":1}}]}";
        }
        json += "]}]}]}]}}";
        auto sui = figo::FigmaUI::fromJson(json);
        sui->setViewport(400, 400);
        sui->render();
        figo::Node* list = sui->currentFrame()->findByName("SnapList");
        // maxScrollY = 9*110 + 100 - 400 = 690; boundaries i*110 clamped to 690.
        if (!list || std::abs(list->maxScrollY() - 690.0f) > 0.5f) {
            std::printf("FAIL: snap fixture maxScrollY %.1f (want 690)\n",
                        list ? list->maxScrollY() : -1.0f);
            ++failures;
        } else {
            list->snapToChildren = true;
            int ends = 0, lastIdx = -2;
            float endX = -1, endY = -1;
            sui->onScrollEnd("SnapList", [&](figo::Node&, float x, float y, int idx) {
                ++ends;
                endX = x;
                endY = y;
                lastIdx = idx;
            });
            // Fling: build up drag velocity over several ticks, then release.
            sui->pointerDown(200, 350);
            for (int i = 1; i <= 5; ++i) {
                sui->pointerMove(200, 350.0f - 30.0f * i);
                sui->update(1.0f / 60);
            }
            sui->pointerUp(200, 200);
            for (int i = 0; i < 600; ++i) sui->update(1.0f / 60);  // decay + snap
            const float rest = list->scrollY;
            const int wantIdx = static_cast<int>(std::lround(rest / 110.0f));
            const bool onBoundary =
                std::abs(rest - wantIdx * 110.0f) < 0.5f || std::abs(rest - 690.0f) < 0.5f;
            std::printf("snap: fling rest=%.1f ends=%d idx=%d\n", rest, ends, lastIdx);
            if (rest <= 150.0f || !onBoundary) {
                std::printf("FAIL: fling did not settle on a child boundary\n");
                ++failures;
            }
            if (ends != 1 || lastIdx != sui->snapIndex(*list) || lastIdx < 0) {
                std::printf("FAIL: onScrollEnd after fling: ends=%d idx=%d snapIndex=%d\n",
                            ends, lastIdx, sui->snapIndex(*list));
                ++failures;
            }
            // Wheel notch smaller than the item pitch still advances one item.
            ends = 0;
            sui->setScroll("SnapList", 0, 0);
            for (int i = 0; i < 10; ++i) sui->update(1.0f / 60);  // flush the end
            ends = 0;
            sui->scrollBy(200, 200, 0, 30);  // 30px notch, item pitch 110
            for (int i = 0; i < 300; ++i) sui->update(1.0f / 60);
            if (std::abs(list->scrollY - 110.0f) > 0.5f || ends != 1 || lastIdx != 1) {
                std::printf("FAIL: wheel quantize: scrollY=%.1f ends=%d idx=%d\n",
                            list->scrollY, ends, lastIdx);
                ++failures;
            }
            // snapTo eases to an exact boundary and reports its index.
            ends = 0;
            sui->snapTo("SnapList", 5);
            for (int i = 0; i < 300; ++i) sui->update(1.0f / 60);
            if (std::abs(list->scrollY - 550.0f) > 0.5f || ends != 1 || lastIdx != 5) {
                std::printf("FAIL: snapTo(5): scrollY=%.1f ends=%d idx=%d\n",
                            list->scrollY, ends, lastIdx);
                ++failures;
            }
            // Non-snap container: onScrollEnd still fires, index is -1.
            list->snapToChildren = false;
            ends = 0;
            sui->setScroll("SnapList", 0, 42);
            sui->update(1.0f / 60);
            if (ends != 1 || lastIdx != -1 || endY != 42.0f) {
                std::printf("FAIL: non-snap end: ends=%d idx=%d y=%.1f\n", ends,
                            lastIdx, endY);
                ++failures;
            }
            (void)endX;
        }
    }

    // ---- Theme variables: resolution, mode switch, save/load roundtrip ----
    {
        const char* varDoc = R"({
            "name": "vars",
            "variables": {
                "modes": ["light", "dark"],
                "activeMode": "light",
                "colors": {"primary": ["#112233", "#aabbcc"]}
            },
            "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                    {"id": "1:1", "name": "F", "type": "FRAME",
                     "size": {"x": 100, "y": 100},
                     "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                     "fills": [{"type": "SOLID",
                                "color": {"r": 1, "g": 0, "b": 0, "a": 1},
                                "colorVar": "primary"}]}
                ]}
            ]}
        })";
        auto check = [&](figo::Document& doc, float wantR255, const char* what) {
            figo::Node* f = doc.findByName("F");
            const float r = f ? f->fills.at(0).color.r * 255.0f : -1.0f;
            if (!f || std::fabs(r - wantR255) > 1.0f) {
                std::printf("FAIL: variables %s: fill r=%.1f want %.1f\n", what, r, wantR255);
                ++failures;
            }
        };
        auto doc = figo::parseDocument(varDoc);
        check(*doc, 0x11, "parse resolves light");   // literal red was overwritten
        doc->variables.activeMode = 1;
        doc->applyVariables();
        check(*doc, 0xaa, "dark mode applies");
        auto redoc = figo::parseDocument(figo::writeDocumentJson(*doc));  // roundtrip
        if (redoc->variables.modes.size() != 2 || redoc->variables.activeMode != 1 ||
            redoc->findByName("F")->fills.at(0).colorVar != "primary") {
            std::printf("FAIL: variables roundtrip lost table/binding\n");
            ++failures;
        }
        check(*redoc, 0xaa, "roundtrip keeps active mode");
        std::printf("variables: resolve/mode-switch/roundtrip checked\n");
    }

    // ---- BACKGROUND_BLUR: the glass panel blends a hard edge underneath it,
    // while the same edge stays sharp outside the panel (no bleed).
    {
        const char* glassDoc = R"({
            "name": "glass",
            "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                    {"id": "1:1", "name": "F", "type": "FRAME",
                     "size": {"x": 200, "y": 200},
                     "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                     "fills": [{"type": "SOLID",
                                "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                     "children": [
                        {"id": "2:1", "name": "Ink", "type": "RECTANGLE",
                         "size": {"x": 100, "y": 200},
                         "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                         "fills": [{"type": "SOLID",
                                    "color": {"r": 0, "g": 0, "b": 0, "a": 1}}]},
                        {"id": "3:1", "name": "Glass", "type": "FRAME",
                         "size": {"x": 100, "y": 100},
                         "relativeTransform": [[1, 0, 50], [0, 1, 50]],
                         "effects": [{"type": "BACKGROUND_BLUR", "visible": true,
                                      "radius": 16,
                                      "color": {"r": 0, "g": 0, "b": 0, "a": 1},
                                      "offset": {"x": 0, "y": 0}}]}
                     ]}
                ]}
            ]}
        })";
        auto doc = figo::parseDocument(glassDoc);
        figo::Renderer r;
        r.setFrame(doc->findByName("F"));
        if (!r.setTarget(200, 200) || !r.render()) {
            std::printf("FAIL: backdrop render did not run\n");
            ++failures;
        } else {
            const uint32_t* px = r.pixels();
            auto red = [&](int x, int y) {
                return static_cast<int>(px[static_cast<size_t>(y) * 200 + x] & 0xFF);
            };
            // Inside the glass, 4px on each side of the black/white edge:
            // blurred → both visibly mixed (σ ≈ 5.5 for radius 16).
            const int inL = red(96, 100), inR = red(104, 100);
            if (inL < 30 || inL > 225 || inR < 30 || inR > 225) {
                std::printf("FAIL: backdrop not blurred inside glass (l=%d r=%d)\n",
                            inL, inR);
                ++failures;
            }
            // Outside the glass (above it), the same offsets stay sharp.
            const int outL = red(94, 20), outR = red(106, 20);
            if (outL > 25 || outR < 230) {
                std::printf("FAIL: backdrop bleeds outside glass (l=%d r=%d)\n",
                            outL, outR);
                ++failures;
            }
            if (failures == 0) std::printf("backdrop: blur inside, sharp outside\n");
        }
    }

    // ---- isMask + blendMode: a mask child confines its later siblings; a
    // MULTIPLY layer darkens what's below it.
    {
        const char* maskDoc = R"({
            "name": "maskblend",
            "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                    {"id": "1:1", "name": "F", "type": "FRAME",
                     "size": {"x": 200, "y": 200},
                     "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                     "fills": [{"type": "SOLID",
                                "color": {"r": 1, "g": 0, "b": 0, "a": 1}}],
                     "children": [
                        {"id": "2:3", "name": "Gray", "type": "RECTANGLE",
                         "blendMode": "MULTIPLY",
                         "size": {"x": 200, "y": 50},
                         "relativeTransform": [[1, 0, 0], [0, 1, 150]],
                         "fills": [{"type": "SOLID",
                                    "color": {"r": 0.5, "g": 0.5, "b": 0.5, "a": 1}}]},
                        {"id": "2:1", "name": "MaskRect", "type": "RECTANGLE",
                         "isMask": true,
                         "size": {"x": 100, "y": 200},
                         "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                         "fills": [{"type": "SOLID",
                                    "color": {"r": 1, "g": 1, "b": 1, "a": 1}}]},
                        {"id": "2:2", "name": "Blue", "type": "RECTANGLE",
                         "size": {"x": 200, "y": 100},
                         "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                         "fills": [{"type": "SOLID",
                                    "color": {"r": 0, "g": 0, "b": 1, "a": 1}}]}
                     ]}
                ]}
            ]}
        })";
        auto doc = figo::parseDocument(maskDoc);
        figo::Renderer r;
        r.setFrame(doc->findByName("F"));
        if (!r.setTarget(200, 200) || !r.render()) {
            std::printf("FAIL: mask/blend render did not run\n");
            ++failures;
        } else {
            const uint32_t* px = r.pixels();
            auto rgb = [&](int x, int y, int& cr, int& cg, int& cb) {
                const uint32_t p = px[static_cast<size_t>(y) * 200 + x];
                cr = static_cast<int>(p & 0xFF);
                cg = static_cast<int>((p >> 8) & 0xFF);
                cb = static_cast<int>((p >> 16) & 0xFF);
            };
            int cr, cg, cb;
            // Inside the mask: the Blue sibling shows.
            rgb(50, 50, cr, cg, cb);
            if (cb < 200 || cr > 50) {
                std::printf("FAIL: mask kept area not blue (%d,%d,%d)\n", cr, cg, cb);
                ++failures;
            }
            // Outside the mask: Blue is masked away → the red frame shows.
            rgb(150, 50, cr, cg, cb);
            if (cr < 200 || cb > 50) {
                std::printf("FAIL: masked-out area not red (%d,%d,%d)\n", cr, cg, cb);
                ++failures;
            }
            // MULTIPLY gray over the red frame: r = 255*0.5, g/b stay 0.
            // (Gray sits below the mask, so it is not masked.)
            rgb(150, 175, cr, cg, cb);
            if (std::abs(cr - 128) > 12 || cg > 20 || cb > 20) {
                std::printf("FAIL: multiply over red got (%d,%d,%d) want ~(128,0,0)\n",
                            cr, cg, cb);
                ++failures;
            }
            if (failures == 0) std::printf("mask/blend: confined + multiplied\n");
        }
    }

    // ---- D8 typography: maxLines clamps the measured flow, paragraphSpacing
    // adds after explicit newlines, tabularFigures gives digits one slot.
    {
        auto textDoc = [](const char* extraStyle, const char* chars) {
            std::string j = R"({
                "name": "type",
                "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                    {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                        {"id": "1:1", "name": "F", "type": "FRAME",
                         "size": {"x": 300, "y": 300},
                         "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                         "children": [
                            {"id": "2:1", "name": "T", "type": "TEXT",
                             "size": {"x": 120, "y": 200},
                             "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                             "characters": ")" + std::string(chars) + R"(",
                             "style": {"fontFamily": "Helvetica", "fontSize": 16,
                                       "lineHeightPx": 20)" + extraStyle + R"(},
                             "fills": [{"type": "SOLID",
                                        "color": {"r": 0, "g": 0, "b": 0, "a": 1}}]}
                         ]}
                    ]}
                ]}
            })";
            return figo::parseDocument(j);
        };
        figo::Renderer r;
        auto measure = [&](figo::Document& doc, float maxW, float& w, float& h) {
            return r.measureText(*doc.findByName("T"), maxW, w, h);
        };
        float w = 0, h = 0;

        // maxLines: a long wrapped paragraph clamps to 2 line boxes.
        auto unclamped = textDoc("", "one two three four five six seven eight nine ten");
        auto clamped = textDoc(R"(, "maxLines": 2, "textTruncation": "ENDING")",
                               "one two three four five six seven eight nine ten");
        float hFree = 0;
        if (!measure(*unclamped, 120, w, hFree) || !measure(*clamped, 120, w, h)) {
            std::printf("FAIL: typography measure did not run\n");
            ++failures;
        } else {
            if (!(hFree > 40.5f)) {
                std::printf("FAIL: unclamped text should wrap past 2 lines (h=%.1f)\n", hFree);
                ++failures;
            }
            if (std::fabs(h - 40.0f) > 0.5f) {
                std::printf("FAIL: maxLines=2 measured %.1f, want 40 (2 x 20px)\n", h);
                ++failures;
            }
        }

        // paragraphSpacing: "a\nb" gains exactly the spacing between lines.
        auto para = textDoc(R"(, "paragraphSpacing": 10)", "alpha\\nbeta");
        if (!measure(*para, 300, w, h) || std::fabs(h - 50.0f) > 0.5f) {
            std::printf("FAIL: paragraphSpacing measured %.1f, want 50 (2x20 + 10)\n", h);
            ++failures;
        }

        // tabularFigures: any two digit strings of equal length measure equal.
        auto ones = textDoc(R"(, "tabularFigures": true)", "1111");
        auto eights = textDoc(R"(, "tabularFigures": true)", "8888");
        float w1 = 0, w8 = 0;
        if (!measure(*ones, 0, w1, h) || !measure(*eights, 0, w8, h) ||
            std::fabs(w1 - w8) > 0.1f) {
            std::printf("FAIL: tabular digits differ: '1111'=%.2f '8888'=%.2f\n", w1, w8);
            ++failures;
        }

        // Rendering side (offscreen): the flow-pipeline placement must obey
        // the same clamp/spacing — scan the raster for the lowest ink row.
        auto lowestInk = [&](figo::Document& doc) -> int {
            figo::Renderer rr;
            rr.setFrame(doc.findByName("F"));
            if (!rr.setTarget(300, 300) || !rr.render()) return -2;
            const uint32_t* px = rr.pixels();
            int maxY = -1;
            for (int y = 0; y < 300; ++y) {
                for (int x = 0; x < 300; ++x) {
                    const uint32_t p = px[static_cast<size_t>(y) * 300 + x];
                    if (((p >> 24) & 0xFF) > 100) maxY = y;
                }
            }
            return maxY;
        };
        const int clampedInk = lowestInk(*clamped);
        if (clampedInk < 0 || clampedInk > 55) {  // 2 x 20px boxes + descender slack
            std::printf("FAIL: maxLines render ink reaches y=%d, want <= 55\n", clampedInk);
            ++failures;
        }
        auto para3 = textDoc(R"(, "paragraphSpacing": 10)", "alpha\\nbeta\\ngamma");
        const int paraInk = lowestInk(*para3);
        if (paraInk < 70) {  // 3rd paragraph sits below 2 boxes + 2 x 10px spacing
            std::printf("FAIL: paragraphSpacing render ink at y=%d, want >= 70\n", paraInk);
            ++failures;
        }
        if (failures == 0) std::printf("typography: maxLines/paragraph/tabular checked\n");
    }

    // ---- D7 visual long tail: INNER_SHADOW spread ring, angular gradient on
    // a pre-outlined stroke (direction-dependent — the old radial fallback
    // was symmetric), and image-paint TILE/CROP field roundtrip.
    {
        const char* d7Doc = R"({
            "name": "d7",
            "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                    {"id": "1:1", "name": "F", "type": "FRAME",
                     "size": {"x": 240, "y": 120},
                     "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                     "fills": [{"type": "SOLID",
                                "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                     "children": [
                        {"id": "2:1", "name": "Spread", "type": "RECTANGLE",
                         "size": {"x": 100, "y": 100},
                         "relativeTransform": [[1, 0, 10], [0, 1, 10]],
                         "fills": [{"type": "SOLID",
                                    "color": {"r": 1, "g": 1, "b": 1, "a": 1}}],
                         "effects": [{"type": "INNER_SHADOW", "visible": true,
                                      "radius": 0, "spread": 20,
                                      "color": {"r": 0, "g": 0, "b": 0, "a": 1},
                                      "offset": {"x": 0, "y": 0}}]},
                        {"id": "2:2", "name": "Ring", "type": "VECTOR",
                         "size": {"x": 100, "y": 100},
                         "relativeTransform": [[1, 0, 130], [0, 1, 10]],
                         "strokes": [{"type": "GRADIENT_ANGULAR",
                                      "gradientHandlePositions":
                                        [{"x": 0.5, "y": 0.5}, {"x": 1, "y": 0.5},
                                         {"x": 0.5, "y": 1}],
                                      "gradientStops": [
                                        {"position": 0,
                                         "color": {"r": 1, "g": 0, "b": 0, "a": 1}},
                                        {"position": 1,
                                         "color": {"r": 0, "g": 0, "b": 1, "a": 1}}]}],
                         "strokeGeometry": [{"path":
                           "M0 0 L100 0 L100 100 L0 100 Z M12 12 L12 88 L88 88 L88 12 Z",
                           "windingRule": "EVENODD"}]}
                     ]}
                ]}
            ]}
        })";
        auto doc = figo::parseDocument(d7Doc);
        figo::Renderer r;
        r.setFrame(doc->findByName("F"));
        if (!r.setTarget(240, 120) || !r.render()) {
            std::printf("FAIL: d7 render did not run\n");
            ++failures;
        } else {
            const uint32_t* px = r.pixels();
            auto rgb = [&](int x, int y, int& cr, int& cg, int& cb) {
                const uint32_t p = px[static_cast<size_t>(y) * 240 + x];
                cr = static_cast<int>(p & 0xFF);
                cg = static_cast<int>((p >> 8) & 0xFF);
                cb = static_cast<int>((p >> 16) & 0xFF);
            };
            int cr, cg, cb;
            // Spread ring: 20px band inside the rect edge is dark, center white.
            rgb(20, 60, cr, cg, cb);  // 10px inside the node edge
            if (cr > 80) {
                std::printf("FAIL: inner-shadow spread band missing (r=%d)\n", cr);
                ++failures;
            }
            rgb(60, 60, cr, cg, cb);  // node center: outside the 20px band
            if (cr < 200) {
                std::printf("FAIL: inner-shadow spread flooded the center (r=%d)\n", cr);
                ++failures;
            }
            // Conic stroke: left and right edges of the ring differ (a radial
            // fallback is left-right symmetric around the center).
            int rl, gl, bl2, rr, gr, br2;
            rgb(136, 60, rl, gl, bl2);   // ring left band (node x 6)
            rgb(224, 60, rr, gr, br2);   // ring right band (node x 94)
            if (std::abs(rl - rr) < 40 && std::abs(bl2 - br2) < 40) {
                std::printf("FAIL: stroke conic gradient looks symmetric "
                            "(l=%d,%d,%d r=%d,%d,%d)\n", rl, gl, bl2, rr, gr, br2);
                ++failures;
            }
        }

        // Image paint TILE/CROP roundtrip: scalingFactor + imageTransform.
        const char* imgDoc = R"({
            "name": "img",
            "document": {"id": "0:0", "name": "d", "type": "DOCUMENT", "children": [
                {"id": "0:1", "name": "p", "type": "CANVAS", "children": [
                    {"id": "1:1", "name": "I", "type": "RECTANGLE",
                     "size": {"x": 100, "y": 100},
                     "relativeTransform": [[1, 0, 0], [0, 1, 0]],
                     "fills": [{"type": "IMAGE", "imageRef": "tex",
                                "scaleMode": "TILE", "scalingFactor": 0.5}]}
                ]}
            ]}
        })";
        auto idoc = figo::parseDocument(imgDoc);
        auto idoc2 = figo::parseDocument(figo::writeDocumentJson(*idoc));
        const auto& ip = idoc2->findByName("I")->fills.front();
        if (ip.imageScaleMode != "TILE" || std::fabs(ip.imageScale - 0.5f) > 0.01f) {
            std::printf("FAIL: image TILE fields lost in roundtrip (%s, %.2f)\n",
                        ip.imageScaleMode.c_str(), ip.imageScale);
            ++failures;
        }
        if (failures == 0) std::printf("d7: spread/conic-stroke/image-fields checked\n");
    }

    std::printf(failures ? "RESULT: %d failure(s)\n" : "RESULT: OK\n", failures);
    return failures;
}
