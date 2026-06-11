#include "figmalib_raylib.h"

namespace figmalib {

RaylibFigmaView::~RaylibFigmaView() {
    if (textureValid_) UnloadTexture(texture_);
}

void RaylibFigmaView::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    const auto w = static_cast<uint32_t>(width);
    const auto h = static_cast<uint32_t>(height);
    if (w == ui_.pixelWidth() && h == ui_.pixelHeight() && textureValid_) return;

    ui_.setViewport(w, h);
    if (textureValid_) {
        UnloadTexture(texture_);
        textureValid_ = false;
    }
}

void RaylibFigmaView::update() {
    const Vector2 mouse = GetMousePosition();
    ui_.pointerMove(mouse.x, mouse.y);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui_.pointerDown(mouse.x, mouse.y);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) ui_.pointerUp(mouse.x, mouse.y);

    if (!ui_.render() && textureValid_) return;
    if (ui_.pixelWidth() == 0 || ui_.pixelHeight() == 0) return;

    if (!textureValid_) {
        Image image{};
        image.data = const_cast<uint32_t*>(ui_.pixels());
        image.width = static_cast<int>(ui_.pixelWidth());
        image.height = static_cast<int>(ui_.pixelHeight());
        image.mipmaps = 1;
        image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        texture_ = LoadTextureFromImage(image);  // copies the data to the GPU
        textureValid_ = texture_.id != 0;
    } else {
        UpdateTexture(texture_, ui_.pixels());
    }
}

void RaylibFigmaView::draw(int x, int y, ::Color tint) const {
    if (textureValid_) DrawTexture(texture_, x, y, tint);
}

}  // namespace figmalib
