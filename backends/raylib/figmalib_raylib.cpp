#include "figmalib_raylib.h"

#include <rlgl.h>

namespace figmalib {

RaylibFigmaView::~RaylibFigmaView() {
    if (textureValid_) UnloadTexture(texture_);
    if (rt_.id != 0) UnloadRenderTexture(rt_);
}

bool RaylibFigmaView::setGpu(bool enabled) {
    if (enabled == gpuWanted_) return gpuActive_;
    gpuWanted_ = enabled;
    // Drop both paths' resources; the next resize() rebuilds the active one.
    if (textureValid_) {
        UnloadTexture(texture_);
        textureValid_ = false;
    }
    if (rt_.id != 0) {
        UnloadRenderTexture(rt_);
        rt_ = {};
    }
    gpuActive_ = false;
    resize(GetScreenWidth(), GetScreenHeight());
    return gpuActive_;
}

void RaylibFigmaView::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    const auto w = static_cast<uint32_t>(width);
    const auto h = static_cast<uint32_t>(height);

    if (gpuWanted_) {
        if (gpuActive_ && rt_.id != 0 && w == ui_.pixelWidth() && h == ui_.pixelHeight()) return;
        if (rt_.id != 0) UnloadRenderTexture(rt_);
        rt_ = LoadRenderTexture(width, height);
        gpuActive_ = rt_.id != 0 &&
                     ui_.setViewportGL(static_cast<int32_t>(rt_.id), w, h);
        if (gpuActive_) return;
        // GL engine unavailable → CPU fallback below.
        if (rt_.id != 0) {
            UnloadRenderTexture(rt_);
            rt_ = {};
        }
        gpuWanted_ = false;
    }

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

    if (gpuActive_) {
        // ThorVG drives GL directly: flush raylib's pending batch first, then
        // restore the state rlgl assumes (it caches and won't re-apply).
        rlDrawRenderBatchActive();
        ui_.render();
        rlDisableFramebuffer();
        rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
        rlSetBlendMode(RL_BLEND_ADDITIVE);  // force a transition so ALPHA re-applies
        rlSetBlendMode(RL_BLEND_ALPHA);
        rlEnableColorBlend();
        rlDisableDepthTest();
        rlDisableScissorTest();
        return;
    }

    if (!ui_.render() && textureValid_) return;
    if (ui_.pixelWidth() == 0 || ui_.pixelHeight() == 0 || !ui_.pixels()) return;

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
    if (gpuActive_) {
        // FBO color attachments are bottom-up in GL: draw with a flipped
        // source rect (the usual raylib RenderTexture convention).
        const Rectangle src{0, 0, static_cast<float>(rt_.texture.width),
                            -static_cast<float>(rt_.texture.height)};
        DrawTextureRec(rt_.texture, src, {static_cast<float>(x), static_cast<float>(y)}, tint);
        return;
    }
    if (textureValid_) DrawTexture(texture_, x, y, tint);
}

}  // namespace figmalib
