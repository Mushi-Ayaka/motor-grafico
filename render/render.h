#pragma once
#include "scene.h"
#include "ray_march.h"
#include "ray_march_simd.h"
#include "jit_compiler.h"
#include "vulkan_pipeline.h"
#include "glsl_gen.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>

namespace mg {

// ============================================================================
// Diagnostic log helper — writes to phase6_diag.txt next to exe
// ============================================================================
inline void logDiag(const char* msg) {
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    char* last = strrchr(exe_path, '\\');
    if (last) *(last + 1) = '\0';
    strcat_s(exe_path, "phase6_diag.txt");
    std::ofstream log(exe_path, std::ios::app);
    if (log.is_open()) {
        log << msg << "\n";
    }
}

// ============================================================================
// Renderer — orchestrates scene + framebuffer
// ============================================================================
struct Renderer {
    Scene     scene;
    OntScene  ont_scene;
    Frame     fb;
    Arena     arena;
    f32       time = 0.0f;
    bool      ont_mode = false;
    bool      use_vulkan = true;
    bool      vulkan_has_scene_shader = false; // true if scene-specialized pipeline is active

    BrickMap    brick_map;
    JitCompiler jit;
    VulkanSceneData vk_scene;

    // Pre-computed transforms (translate only for now)
    std::vector<f32> transforms;

    bool load(const FileMapping& fm) {
        arena.init(64 * 1024 * 1024);
        if (!scene.load(fm)) {
            scene.loadDefault();
            return false;
        }
        transforms.resize(scene.nodes.size() * 3);
        for (u32 i = 0; i < scene.nodes.size(); i++) {
            transforms[i * 3 + 0] = scene.nodes[i].translate.x;
            transforms[i * 3 + 1] = scene.nodes[i].translate.y;
            transforms[i * 3 + 2] = scene.nodes[i].translate.z;
        }
        return true;
    }

    bool loadOnt(const FileMapping& fm) {
        arena.init(64 * 1024 * 1024);
        if (!ont_scene.loadOnt(fm)) {
            ont_scene.loadDefault();
            return false;
        }
        ont_mode = true;

        // Compile JIT for all graph nodes
        jit.init();
        jit.compileScene(ont_scene);

        // Build sparse brick map
        brick_map.destroy();
        buildBrickMap(ont_scene, brick_map);

        if (use_vulkan) {
            vk_scene.init(ont_scene);

            // Generate and compile a scene-specialized compute shader
            GlslGenResult gen = GlslGen::generate(ont_scene);
            if (gen.ok) {
                std::vector<uint32_t> spv;
                std::string compile_error;
                if (compileGlslToSpv(gen.glsl_source, spv, compile_error)) {
                    if (vk_scene.recreateScenePipeline(spv)) {
                        vulkan_has_scene_shader = true;
                        logDiag("[VK] Scene-specialized pipeline created");
                    } else {
                        logDiag("[VK] recreateScenePipeline FAILED despite valid SPIR-V, loading fallback");
                        vk_scene.initPipeline();
                    }
                } else {
                    logDiag(("[GLSL] Compilation error: " + compile_error).c_str());
                    logDiag("[VK] Loading fallback pre-compiled pipeline (no scene data)");
                    vk_scene.initPipeline();
                }
            } else {
                logDiag(("[GLSL] Generation error: " + gen.error).c_str());
                logDiag("[VK] Loading fallback pre-compiled pipeline (no scene data)");
                vk_scene.initPipeline();
            }
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
        if (ont_mode) {
            if (brick_map.valid)
                renderOntSceneMTSIMD(ont_scene, fb, time, 0, &brick_map);
            else
                renderOntSceneMTSIMD(ont_scene, fb, time);
        } else {
            renderScene(scene, fb, time, transforms.data(), aabbs, visible_nodes, visible_count);
        }
    }

    // Render with per-ray SceneQuery (BVH + AABBs)
    void render(int width, int height, const SceneQuery& sq) {
        arena.reset();
        fb.init(arena, width, height);
        if (ont_mode) {
            if (brick_map.valid)
                renderOntSceneMTSIMD(ont_scene, fb, time, 0, &brick_map);
            else
                renderOntSceneMTSIMD(ont_scene, fb, time);
        } else {
            renderScene(scene, fb, time, transforms.data(), sq);
        }
    }
};

} // namespace mg
