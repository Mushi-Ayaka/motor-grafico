#include "visor_app.h"
#include <cstdio>
#include <cwchar>
#include <string>
#include <iostream>
#include <fstream>
#include "render/vulkan_core.h"

static std::ofstream g_diag;
static void diag(const char* msg) {
    if (!g_diag.is_open()) {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        char* last = strrchr(exe_path, '\\');
        if (last) *(last+1) = '\0';
        strcat_s(exe_path, "phase6_diag.txt");
        g_diag.open(exe_path, std::ios::app);
    }
    g_diag << msg << "\n";
    g_diag.flush();
}

namespace mg {

bool VisorApp::init(HINSTANCE hInst, const wchar_t* initial_scene) {
    // Open log next to exe (use app mode so logDiag from render.h can also append safely)
    {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        char* last = strrchr(exe_path, '\\');
        if (last) *(last+1) = '\0';
        strcat_s(exe_path, "phase6_diag.txt");
        { std::ofstream clear(exe_path, std::ios::trunc); }
        g_diag.open(exe_path, std::ios::app);
    }
    diag("[PHASE6] init() START");
    workspace.init();

    if (!win.init(hInst, this)) { diag("[PHASE6] FAIL: win.init"); return false; }
    diag("[PHASE6] win.init OK");

    if (!vk_ctx.init(win.hwnd_viewport)) {
        diag("[PHASE6] FAIL: vk_ctx.init");
        std::cerr << "Failed to init Vulkan core" << std::endl;
        return false;
    }
    diag("[PHASE6] vk_ctx.init OK");
    if (!vk_ctx.initSwapchain(workspace.active().w, workspace.active().h)) {
        diag("[PHASE6] FAIL: initSwapchain");
        std::cerr << "Failed to init Vulkan swapchain" << std::endl;
        return false;
    }
    diag("[PHASE6] initSwapchain OK");
    {
        char buf[128];
        sprintf_s(buf, "[DIM] workspace active w=%d h=%d, render_scale=%.2f",
                  workspace.active().w, workspace.active().h, render_scale);
        diag(buf);
    }

    // Check if loading an .ont file
    bool is_ont = false;
    if (initial_scene) {
        char buf[512];
        sprintf_s(buf, "[PHASE6] initial_scene: %ws", initial_scene);
        diag(buf);
        const wchar_t* ext = wcsstr(initial_scene, L".ont");
        if (ext) is_ont = true;
    } else {
        diag("[PHASE6] initial_scene is NULL");
    }

    if (is_ont) {
        diag("[PHASE6] is_ont is TRUE");
        FileMapping fm;
        if (fm.open(initial_scene)) {
            diag("[PHASE6] loadOnt START");
            bool ontOk = renderer.loadOnt(fm);
            diag(ontOk ? "[PHASE6] loadOnt OK" : "[PHASE6] loadOnt returned false (fallback)");
            fm.close();

            // Try to load companion .obs file
            std::wstring obs_path(initial_scene);
            size_t dot = obs_path.rfind(L'.');
            if (dot != std::wstring::npos) {
                obs_path = obs_path.substr(0, dot) + L".obs";
                FileMapping obs_fm;
                if (obs_fm.open(obs_path.c_str())) {
                    renderer.ont_scene.loadObs(obs_fm);
                    obs_fm.close();
                    renderer.ont_scene.applyObs();
                }
            }
        } else {
            diag("[PHASE6] fm.open FAILED for initial_scene. Falling back to loadDefault");
            renderer.ont_scene.loadDefault();
            renderer.ont_mode = true;
        }

        // Apply observation camera if present, otherwise use default orbit
        if (renderer.ont_scene.has_obs && renderer.ont_scene.obs.has_camera) {
            const auto& oc = renderer.ont_scene.obs.camera;
            Vec3 diff = {oc.target.x - oc.position.x, oc.target.y - oc.position.y, oc.target.z - oc.position.z};
            f32 dist = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
            if (dist < 0.001f) dist = 5.0f;
            f32 azimuth = std::atan2(diff.z, diff.x);
            f32 elevation = std::asin(diff.y / dist);
            cam_ctrl.setOrbit(oc.target, dist, azimuth, elevation);
            renderer.ont_scene.camera = oc;
        } else {
            cam_ctrl.setOrbit({0,0,0}, 5.0f, 0, 0.5f);
            cam_ctrl.applyTo(renderer.ont_scene.camera);
        }
    } else {
        scene_mgr.loadScene(renderer, initial_scene, cam_ctrl, workspace.active().w, workspace.active().h);
        if (renderer.scene.nodes.empty()) {
            renderer.scene.loadDefault();
            scene_mgr.rebuildScene(renderer);
        }

        if (renderer.scene.lights.empty()) {
            Light def;
            def.name = "default";
            def.type = LightType::DIRECTIONAL;
            def.direction = {0.3f, 0.8f, 0.5f};
            renderer.scene.lights.push_back(def);
        }

        cam_ctrl.setOrbit(renderer.scene.camera.target, 5.0f, 0, 0.5f);
        cam_ctrl.applyTo(renderer.scene.camera);
    }

    if (renderer.use_vulkan && renderer.ont_mode) {
        renderer.vk_scene.resizeOutputBuffer(workspace.active().w, workspace.active().h);
        diag("[PHASE6] output_buffer resized to match viewport");
    }

    updateTitle();
    
    renderer.time = 0.0f;
    dirty = true;
    // End of init
    diag("[PHASE6] init() END OK");
    return true;
}

void VisorApp::updateTitle() {
    char title[512];
    if (renderer.ont_mode) {
        const char* mode_tag = "";
        if (renderer.use_vulkan && renderer.vulkan_has_scene_shader)
            mode_tag = " [VK]";
        else if (renderer.use_vulkan && !renderer.vulkan_has_scene_shader)
            mode_tag = " [VK-FALLBACK]";
        else
            mode_tag = " [CPU]";
        const char* test_tag = vk_ctx.test_pattern ? " [TEST PATTERN]" : "";
        snprintf(title, sizeof(title), "Motor Grafico%s [ONT: %u graph %u bvh %u mats] FPS: %u | %ums%s",
            mode_tag,
            renderer.ont_scene.header->node_count,
            renderer.ont_scene.header->bvh_count,
            renderer.ont_scene.header->material_count,
            fps, last_render_ms, test_tag);
    } else {
        snprintf(title, sizeof(title), "Motor Grafico [%zu nodes, %zu mats, BVH=%zu nodes]",
            renderer.scene.nodes.size(),
            renderer.scene.materials.size(),
            scene_mgr.bvh.nodes.size());
    }
    win.setWindowTitle(title);

    char props[1024];
    if (renderer.ont_mode) {
        const auto& sc = renderer.ont_scene;
        snprintf(props, sizeof(props),
            "ONT Scene\n"
            "Graph nodes: %u\n"
            "BVH nodes: %u\n"
            "Materials: %u\n"
            "Bytecode: %u bytes\n"
            "Lights: %zu (from %s)\n\n"
            "Camera:\n"
            " Pos: %.2f, %.2f, %.2f\n"
            " Tgt: %.2f, %.2f, %.2f\n"
            " FOV: %.1f",
            sc.header->node_count,
            sc.header->bvh_count,
            sc.header->material_count,
            sc.header->bytecode_size,
            sc.has_obs ? sc.obs.lights.size() : 0,
            sc.has_obs ? ".obs" : "defaults",
            sc.camera.position.x, sc.camera.position.y, sc.camera.position.z,
            sc.camera.target.x, sc.camera.target.y, sc.camera.target.z,
            sc.camera.fov);
    } else {
        snprintf(props, sizeof(props), 
            "Nodes: %zu\n"
            "Materials: %zu\n"
            "Lights: %zu\n"
            "BVH Nodes: %zu\n\n"
            "Camera:\n"
            " Pos: %.2f, %.2f, %.2f\n"
            " Tgt: %.2f, %.2f, %.2f\n"
            " FOV: %.1f",
            renderer.scene.nodes.size(),
            renderer.scene.materials.size(),
            renderer.scene.lights.size(),
            scene_mgr.bvh.nodes.size(),
            renderer.scene.camera.position.x, renderer.scene.camera.position.y, renderer.scene.camera.position.z,
            renderer.scene.camera.target.x, renderer.scene.camera.target.y, renderer.scene.camera.target.z,
            renderer.scene.camera.fov);
    }
    win.updateProps(props);
}

void VisorApp::renderFrame() {
    int w = (int)(workspace.active().w * render_scale + 0.5f);
    int h = (int)(workspace.active().h * render_scale + 0.5f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (!scene_mgr.world_aabbs.empty() && !scene_mgr.bvh.nodes.empty()) {
        renderer.render(w, h, scene_mgr.getQuery());
    } else if (!scene_mgr.world_aabbs.empty()) {
        renderer.render(w, h, scene_mgr.world_aabbs.data(), nullptr, 0);
    } else {
        renderer.render(w, h);
    }
}

void VisorApp::run() {
    u32 last_title_tick = 0;
    bool first_render = true;

    while (running) {
        u32 t0 = GetTickCount();
        win.pumpMessages();
        u32 t1 = GetTickCount();
        last_pump_ms = t1 - t0;

        u32 now = t1;

        // Update animation time
        renderer.time += 0.016f; // ~60 FPS steps
        if (renderer.ont_scene.has_obs && renderer.ont_scene.obs.has_timeline) {
            f32 w_max = renderer.ont_scene.obs.w_max;
            if (w_max > 0) renderer.time = fmod(renderer.time, w_max);
        }

        // Per-frame camera smoothing
        cam_ctrl.updateSmoothing(0.016f);

        // Process camera input (WASD, keys)
        bool cam_moved = input.updateCamera(0.016f, cam_ctrl);
        if (cam_moved) {
            camera_moving = true;
            camera_stop_tick = now;
            if (renderer.ont_mode) {
                cam_ctrl.applyTo(renderer.ont_scene.camera);
            } else {
                cam_ctrl.applyTo(renderer.scene.camera);
            }
            dirty = true;
            title_dirty = true;
        }
        if (!cam_moved && camera_moving && (now - camera_stop_tick > 400)) {
            camera_moving = false;
        }

        // First render always happens immediately
        if (first_render) {
            dirty = true;
            first_render = false;
        }

        // F3 toggles test pattern mode (bypasses compute shader)
        if (GetAsyncKeyState(VK_F3) & 0x8000) {
            if (!toggle_held) {
                vk_ctx.test_pattern = !vk_ctx.test_pattern;
                diag(vk_ctx.test_pattern ? "[TEST] Test pattern ON (red)" : "[TEST] Test pattern OFF");
                toggle_held = true;
            }
        } else {
            toggle_held = false;
        }

        // Render — always renders when dirty
        u32 rt = 0;
        if (dirty) {
            u32 r0 = GetTickCount();
            
            if (renderer.use_vulkan && renderer.ont_mode) {
                // Check for viewport resize
                VkExtent2D cur = vk_ctx.swapchain_extent;
                int vp_w = workspace.active().w;
                int vp_h = workspace.active().h;
                // Render at reduced scale for performance
                int rw = (int)(vp_w * render_scale + 0.5f);
                int rh = (int)(vp_h * render_scale + 0.5f);
                if (rw < 1) rw = 1;
                if (rh < 1) rh = 1;
                if ((int)cur.width != vp_w || (int)cur.height != vp_h || cur.width == 0) {
                    {
                        char buf[256];
                        sprintf_s(buf, "[RESIZE] vp=%dx%d swp_cur=%dx%d rw=%d rh=%d",
                                  vp_w, vp_h, (int)cur.width, (int)cur.height, rw, rh);
                        diag(buf);
                    }
                    vk_ctx.resizeSwapchain(vp_w, vp_h);
                    renderer.vk_scene.resizeOutputBuffer(rw, rh);
                }

                // Start bench clock on first frame
                if (bench_vulkan && !bench_clock_started) {
                    QueryPerformanceCounter(&bench_start);
                    bench_clock_started = true;
                }

                // Update UBO and dispatch Vulkan Compute Shader + Swapchain Present
                UboData ubo;
                ubo.camera_pos[0] = renderer.ont_scene.camera.position.x;
                ubo.camera_pos[1] = renderer.ont_scene.camera.position.y;
                ubo.camera_pos[2] = renderer.ont_scene.camera.position.z;
                ubo.camera_target[0] = renderer.ont_scene.camera.target.x;
                ubo.camera_target[1] = renderer.ont_scene.camera.target.y;
                ubo.camera_target[2] = renderer.ont_scene.camera.target.z;
                ubo.camera_up[0] = renderer.ont_scene.camera.up.x;
                ubo.camera_up[1] = renderer.ont_scene.camera.up.y;
                ubo.camera_up[2] = renderer.ont_scene.camera.up.z;
                ubo.fov = renderer.ont_scene.camera.fov;
                ubo.time = renderer.time;
                ubo.render_scale = render_scale;
                ubo.width = rw;
                ubo.height = rh;
                ubo.trace_t_max = renderer.ont_scene.pipeline.trace_t_max;

                {
                    static int ubo_log_count = 0;
                    if (++ubo_log_count <= 3) {
                        char buf[256];
                        sprintf_s(buf, "[UBO] w=%d h=%d fov=%.1f rscale=%.2f tmax=%.1f pos=(%.1f,%.1f,%.1f)",
                                  ubo.width, ubo.height, ubo.fov, ubo.render_scale, ubo.trace_t_max,
                                  ubo.camera_pos[0], ubo.camera_pos[1], ubo.camera_pos[2]);
                        diag(buf);
                    }
                }
                renderer.vk_scene.updateUBO(ubo);
                vk_ctx.drawFrame(renderer.vk_scene, rw, rh);

                InvalidateRect(win.hwnd_viewport, nullptr, FALSE);
                UpdateWindow(win.hwnd_viewport);
                win.drawFpsOverlay();
            } else {
                renderFrame();
                InvalidateRect(win.hwnd_viewport, nullptr, FALSE);
                UpdateWindow(win.hwnd_viewport);
            }

            rt = GetTickCount() - r0;
            last_render_ms = rt;
            if (!(renderer.use_vulkan && renderer.ont_mode) && !bench_vulkan) dirty = false;
        }

        // FPS counter — update once per second
        frame_count++;
        if (now - fps_last_tick > 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_last_tick = now;
            snprintf(fps_text, sizeof(fps_text), "FPS: %u | render: %ums | pump: %ums | scale: %.2f", fps, last_render_ms, last_pump_ms, render_scale);
        }

        // Update title at most once per second (reduces GDI spam)
        if (title_dirty || now - last_title_tick > 1000) {
            updateTitle();
            last_title_tick = now;
            title_dirty = false;
        }

        // Bench-vulkan: measure total time, exit after max frames
        if (bench_vulkan) {
            bench_frames++;
            if (bench_frames >= bench_max_frames) {
                QueryPerformanceCounter(&bench_end);
                LARGE_INTEGER freq;
                QueryPerformanceFrequency(&freq);
                f64 total_ns = (f64)(bench_end.QuadPart - bench_start.QuadPart) * 1000000000.0 / (f64)freq.QuadPart;
                f64 avg_ms = total_ns / (f64)bench_frames / 1000000.0;
                f64 avg_fps = 1000.0 / avg_ms;
                int bw = workspace.active().w, bh = workspace.active().h;
                FILE* f = nullptr;
                fopen_s(&f, "benchmark_result.txt", "w");
                if (f) {
                    fprintf(f, "Escena: %u nodos, %u BVH, %u materiales, %u bytecode bytes\n",
                        renderer.ont_scene.header->node_count,
                        renderer.ont_scene.header->bvh_count,
                        renderer.ont_scene.header->material_count,
                        renderer.ont_scene.header->bytecode_size);
                    fprintf(f, "Resolucion: %u x %u\n", bw, bh);
                    fprintf(f, "GPU Vulkan Compute (%u frames):\n", bench_frames);
                    fprintf(f, "  Media: %.3f ms (%.1f FPS)\n", avg_ms, avg_fps);
                    fprintf(f, "  Total: %.3f ms\n", total_ns / 1000000.0);
                    fclose(f);
                }
                fprintf(stderr, "\n=== BENCH-VULKAN ===\n");
                fprintf(stderr, "Escena: %u nodos, %u BVH, %u materiales, %u bytecode bytes\n",
                    renderer.ont_scene.header->node_count,
                    renderer.ont_scene.header->bvh_count,
                    renderer.ont_scene.header->material_count,
                    renderer.ont_scene.header->bytecode_size);
                fprintf(stderr, "Resolucion: %u x %u\n", bw, bh);
                fprintf(stderr, "GPU Vulkan Compute (%u frames):\n", bench_frames);
                fprintf(stderr, "  Media: %.3f ms (%.1f FPS)\n", avg_ms, avg_fps);
                fprintf(stderr, "  Total: %.3f ms\n", total_ns / 1000000.0);
                running = false;
            }
        }

        // Adaptive sleep: skip in bench-vulkan mode
        if (!bench_vulkan) {
            Sleep(camera_moving ? 8 : 32);
        }
    }
    renderer.vk_scene.cleanup();
    vk_ctx.cleanup();
    win.destroy();
}

}
