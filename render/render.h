#pragma once
#include "scene.h"
#include "ray_march.h"
#include <vector>

namespace mg {

// ============================================================================
// Renderer — orchestrates scene + framebuffer
// ============================================================================
struct Renderer {
    Scene   scene;
    Frame   fb;
    Arena   arena;
    f32     time = 0.0f;

    // Pre-computed transforms (translate only for now)
    std::vector<f32> transforms;

    bool load(const FileMapping& fm) {
        arena.init(64 * 1024 * 1024);
        if (!scene.load(fm)) {
            scene.loadDefault();
            return false;
        }
        // Pre-compute transforms
        transforms.resize(scene.nodes.size() * 3);
        for (u32 i = 0; i < scene.nodes.size(); i++) {
            transforms[i * 3 + 0] = scene.nodes[i].translate.x;
            transforms[i * 3 + 1] = scene.nodes[i].translate.y;
            transforms[i * 3 + 2] = scene.nodes[i].translate.z;
        }
        return true;
    }

    // Render at given resolution (uses pre-allocated arena)
    void render(int width, int height,
                const Aabb* aabbs = nullptr,
                const u32* visible_nodes = nullptr,
                u32 visible_count = 0) {
        arena.reset();
        fb.init(arena, width, height);
        renderScene(scene, fb, time, transforms.data(), aabbs, visible_nodes, visible_count);
    }

    // Render with per-ray SceneQuery (BVH + AABBs)
    void render(int width, int height, const SceneQuery& sq) {
        arena.reset();
        fb.init(arena, width, height);
        renderScene(scene, fb, time, transforms.data(), sq);
    }
};

} // namespace mg
