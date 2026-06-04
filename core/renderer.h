#pragma once
#include "core.h"
#include <vector>

namespace mg {

struct Framebuffer {
    u32 width = 0, height = 0;
    std::vector<u32> pixels; // RGBA 8-bit
};

struct RenderConfig {
    u32 width = 400;
    u32 height = 300;
    f32 time = 0.0f;
    Camera camera;
    u32 max_steps = 64;
    f32 max_dist = 50.0f;
    f32 hit_eps = 0.001f;
    bool show_bboxes = false;
};

void render(Rih& rih, const RenderConfig& cfg, Framebuffer& fb);

} // namespace mg
