#include "visor_app.h"
#include <cstdio>
#include <cwchar>
#include <string>
#include <iostream>
#include <fstream>
#include <windows.h>
#include <commdlg.h>
#include "render/vulkan_core.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "core/herm_bridge.h"

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

    // --- Init ImGui (editor UI) over the viewport swapchain (T-F6) ---
    if (renderer.use_vulkan) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!vk_ctx.initImGui(win.hwnd_viewport)) {
            diag("[IMGUI] init FAILED (continuando sin UI)");
        } else {
            diag("[IMGUI] init OK");
        }
    }
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
    
    // Editor .herm (T-111): inicializa con fuente por defecto
    herm_editor.initDefault();
    editor_source = herm_editor.source;
    editor_status = "Listo. Edita y pulsa Compile (Ctrl+S).";

    // T-115: Undo/Redo init
    undo_redo.saveState(editor_source);

    // T-113: Console panel init
    console.addLog(LogEntry::Level::INFO, "Motor Grafico initialized");
    console.addLog(LogEntry::Level::INFO, "Scheduler: debounce=%ums", scheduler.debounce_ms);

    // T-110: Scheduler — compile async con debounce
    scheduler.init(
        [this](const std::string& src) -> CompileResult {
            CompileResult r;
            std::string err;
            if (renderer.use_vulkan) {
                mg::OntScene ont;
                if (mg::compileHermToOntScene(src.empty() ? editor_source : src, ont, &err)) {
                    r.ok = true;
                    r.nodes = ont.header->node_count;
                    r.materials = ont.header->material_count;
                    r.bytecode_bytes = ont.header->bytecode_size;
                } else {
                    r.ok = false;
                    r.error = err;
                }
            } else {
                mg::Scene sc;
                if (mg::compileHermToScene(src.empty() ? editor_source : src, sc, &err)) {
                    r.ok = true;
                    r.nodes = (uint32_t)sc.nodes.size();
                    r.materials = (uint32_t)sc.materials.size();
                    r.bytecode_bytes = 0;
                } else {
                    r.ok = false;
                    r.error = err;
                }
            }
            return r;
        },
        [this](const CompileResult& result) {
            if (!result.ok) {
                editor_status = std::string("[FAIL] ") + result.error;
                return;
            }
            // Aplicar al renderer
            std::string err;
            if (renderer.use_vulkan) {
                mg::OntScene ont;
                if (mg::compileHermToOntScene(editor_source, ont, &err)) {
                    renderer.ont_scene = ont;
                    renderer.ont_mode = true;
                    editor_has_scene = true;
                    editor_status = "[ok] ont compiled";
                }
            } else {
                mg::Scene sc;
                if (mg::compileHermToScene(editor_source, sc, &err)) {
                    renderer.scene = sc;
                    if (renderer.scene.lights.empty()) {
                        Light def;
                        def.name = "default";
                        def.type = LightType::DIRECTIONAL;
                        def.direction = {0.3f, 0.8f, 0.5f};
                        renderer.scene.lights.push_back(def);
                    }
                    scene_mgr.rebuildScene(renderer);
                    cam_ctrl.setOrbit(renderer.scene.camera.target, 5.0f, 0, 0.5f);
                    cam_ctrl.applyTo(renderer.scene.camera);
                    renderer.ont_mode = false;
                    editor_has_scene = true;
                    editor_status = "[ok] scene compiled";
                }
            }
            dirty = true;
            title_dirty = true;
        }
    );

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

        // T-110: Scheduler — procesar debounce + compile async
        scheduler.update(now);

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

        // T-115: Ctrl+Z = Undo, Ctrl+Y = Redo
        bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl) {
            if (GetAsyncKeyState('Z') & 0x8000) {
                if (!undo_redo_held) {
                    std::string src;
                    if (undo_redo.undo(src)) {
                        editor_source = src;
                        herm_editor.source = src;
                        herm_editor.reloadFromSource();
                        console.addLog(LogEntry::Level::INFO, "Undo: %zu chars", src.size());
                    }
                    undo_redo_held = true;
                }
            } else if (GetAsyncKeyState('Y') & 0x8000) {
                if (!undo_redo_held) {
                    std::string src;
                    if (undo_redo.redo(src)) {
                        editor_source = src;
                        herm_editor.source = src;
                        herm_editor.reloadFromSource();
                        console.addLog(LogEntry::Level::INFO, "Redo: %zu chars", src.size());
                    }
                    undo_redo_held = true;
                }
            } else {
                undo_redo_held = false;
            }
        } else {
            undo_redo_held = false;
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

                // --- ImGui frame (T-F6) ---
                if (vk_ctx.imgui_enabled) {
                    ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();
                    ImGuiIO& io = ImGui::GetIO();
                    io.DisplaySize = ImVec2((float)workspace.active().w, (float)workspace.active().h);
                    drawEditorUI();
                    ImGui::Render();
                }

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

        // T-116: Update profiler stats every frame
        {
            ProfilerStats ps;
            ps.fps = fps;
            ps.render_ms = last_render_ms;
            ps.pump_ms = last_pump_ms;
            ps.render_scale = render_scale;
            ps.viewport_w = workspace.active().w;
            ps.viewport_h = workspace.active().h;
            ps.ont_mode = renderer.ont_mode;
            if (renderer.ont_scene.header) {
                ps.node_count = renderer.ont_scene.header->node_count;
                ps.material_count = renderer.ont_scene.header->material_count;
                ps.bytecode_size = renderer.ont_scene.header->bytecode_size;
            }
            profiler.update(ps);
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
    vk_ctx.destroyViewportImage();
    if (vk_ctx.imgui_enabled) {
        vk_ctx.shutdownImGui();
        ImGui::DestroyContext();
    }
    vk_ctx.cleanup();
    win.destroy();
}

void VisorApp::drawEditorUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags dsFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("DockSpace", nullptr, dsFlags);
    ImGui::PopStyleVar(2);

    if (ImGui::BeginMenuBar()) {
        // --- File ---
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) { herm_editor.initDefault(); editor_source = herm_editor.source; undo_redo.clear(); undo_redo.saveState(editor_source); }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
#ifdef _WIN32
                char filename[MAX_PATH] = {};
                OPENFILENAMEA ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = GetActiveWindow();
                ofn.lpstrFilter = "Herm Source (*.herm)\0*.herm\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn)) {
                    herm_editor.openFile(filename);
                    editor_source = herm_editor.source;
                    undo_redo.clear();
                    undo_redo.saveState(editor_source);
                    console.addLog(LogEntry::Level::INFO, "Opened: %s", filename);
                }
#endif
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (herm_editor.file_path.empty()) {
                    herm_editor.saveAsDialog();
                } else {
                    herm_editor.saveFile();
                }
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { herm_editor.saveAsDialog(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { running = false; }
            ImGui::EndMenu();
        }
        // --- Edit ---
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                std::string src;
                if (undo_redo.undo(src)) { editor_source = src; herm_editor.source = src; herm_editor.reloadFromSource(); }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                std::string src;
                if (undo_redo.redo(src)) { editor_source = src; herm_editor.source = src; herm_editor.reloadFromSource(); }
            }
            ImGui::EndMenu();
        }
        // --- View ---
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Console", nullptr, &console.visible);
            ImGui::MenuItem("Editor .herm", nullptr, &herm_editor.visible);
            ImGui::EndMenu();
        }
        // --- Help ---
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) { /* TODO */ }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- Toolbar ---
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

    // Mode toggle
    const char* mode_label = renderer.ont_mode ? "Mode: GPU" : "Mode: CPU";
    if (ImGui::Button(mode_label, ImVec2(80, 0))) {
        renderer.ont_mode = !renderer.ont_mode;
        dirty = true;
        title_dirty = true;
    }
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Camera mode
    const char* cam_mode = "Orbit";
    if (ImGui::Button(cam_mode, ImVec2(60, 0))) { /* TODO: cycle modes */ }
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Render scale
    ImGui::Text("Scale:");
    ImGui::SameLine();
    if (ImGui::SliderFloat("##scale", &render_scale, 0.1f, 2.0f, "%.1f")) {
        dirty = true;
        title_dirty = true;
    }
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // T-105: Gizmo mode buttons
    if (ImGui::Button(gizmos.mode == GizmoMode::MOVE ? "[Move]" : "Move", ImVec2(50, 0))) {
        gizmos.mode = GizmoMode::MOVE;
    }
    ImGui::SameLine();
    if (ImGui::Button(gizmos.mode == GizmoMode::ROTATE ? "[Rot]" : "Rot", ImVec2(50, 0))) {
        gizmos.mode = GizmoMode::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::Button(gizmos.mode == GizmoMode::SCALE ? "[Scale]" : "Scale", ImVec2(55, 0))) {
        gizmos.mode = GizmoMode::SCALE;
    }
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // FPS
    ImGui::Text("%s", fps_text);

    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    if (ImGui::Begin("Viewport")) {
        if (vk_ctx.viewport_ds) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float aspect = (float)vk_ctx.viewport_w / (float)vk_ctx.viewport_h;
            float img_w = avail.x;
            float img_h = img_w / aspect;
            if (img_h > avail.y) { img_h = avail.y; img_w = img_h * aspect; }
            ImGui::Image((ImTextureID)vk_ctx.viewport_ds, ImVec2(img_w, img_h));

            // T-105: Draw gizmos overlay on viewport
            gizmos.draw(ontology, scene_mgr.graph, renderer.scene, cam_ctrl,
                        workspace.active().w, workspace.active().h);
        } else {
            ImGui::Text("SDF render (waiting for first frame...)");
        }
        ImGui::End();
    }
    if (ImGui::Begin("Properties")) {
        ImGui::Text("Engine State: Running");
        ImGui::Text("Resolution: %ux%u", workspace.active().w, workspace.active().h);
        ImGui::Separator();

        // T-110: Scheduler state
        ImGui::Text("Scheduler:");
        const char* state_str = "IDLE";
        switch (scheduler.state) {
            case Scheduler::State::IDLE:      state_str = "IDLE"; break;
            case Scheduler::State::DIRTY:     state_str = "DIRTY (waiting...)"; break;
            case Scheduler::State::COMPILING: state_str = "COMPILING..."; break;
            case Scheduler::State::VALIDATING: state_str = "VALIDATING..."; break;
            case Scheduler::State::PUBLISHED: state_str = "PUBLISHED"; break;
            case Scheduler::State::STATE_ERROR: state_str = "ERROR"; break;
        }
        ImGui::Text("  State: %s", state_str);
        if (scheduler.last_good.ok) {
            ImGui::Text("  Last: nodes=%u mats=%u bc=%u bytes",
                scheduler.last_good.nodes, scheduler.last_good.materials,
                scheduler.last_good.bytecode_bytes);
        }

        // Anomaly Gate
        if (!last_anomalies.empty()) {
            ImGui::Separator();
            ImGui::Text("Anomalies:");
            ImGui::TextWrapped("%s", AnomalyGate::summary(last_anomalies).c_str());
        }

        ImGui::Separator();
        ImGui::Text("Editor .herm:");
        ImGui::TextWrapped("%s", editor_status.c_str());
        ImGui::End();
    }
    if (ImGui::Begin("Hello ImGui")) { ImGui::Text("Integracion ImGui OK (T-F6)"); ImGui::End(); }

    // --- Editor de codigo .herm + live-compile (T-111) ---
    if (ImGui::Begin("Editor .herm")) {
        // Set compile callback (captures this via static lambda)
        static VisorApp* s_app = nullptr;
        s_app = this;
        herm_editor.setCompileCallback([](const std::string& src, std::string* err, void* ud) -> bool {
            VisorApp* app = (VisorApp*)ud;
            app->editor_source = src;

            // GPU path
            if (app->renderer.use_vulkan) {
                mg::OntScene ont;
                if (mg::compileHermToOntScene(src, ont, err)) {
                    app->renderer.ont_scene = ont;
                    app->renderer.ont_mode = true;
                    app->editor_has_scene = true;
                    char s[256];
                    snprintf(s, sizeof(s), "[ok] ont: nodes=%u mats=%u bc=%u bytes",
                             ont.header->node_count, ont.header->material_count,
                             ont.header->bytecode_size);
                    app->editor_status = s;
                    app->dirty = true;
                    app->title_dirty = true;
                    app->undo_redo.saveState(src);
                    return true;
                }
                return false;
            }
            // CPU path
            else {
                mg::Scene sc;
                if (mg::compileHermToScene(src, sc, err)) {
                    app->renderer.scene = sc;
                    if (app->renderer.scene.lights.empty()) {
                        Light def;
                        def.name = "default";
                        def.type = LightType::DIRECTIONAL;
                        def.direction = {0.3f, 0.8f, 0.5f};
                        app->renderer.scene.lights.push_back(def);
                    }
                    app->scene_mgr.rebuildScene(app->renderer);
                    app->cam_ctrl.setOrbit(app->renderer.scene.camera.target, 5.0f, 0, 0.5f);
                    app->cam_ctrl.applyTo(app->renderer.scene.camera);
                    app->renderer.ont_mode = false;
                    app->editor_has_scene = true;
                    char s[256];
                    snprintf(s, sizeof(s), "[ok] scene: nodes=%zu mats=%zu lights=%zu",
                             sc.nodes.size(), sc.materials.size(), sc.lights.size());
                    app->editor_status = s;
                    app->dirty = true;
                    app->title_dirty = true;
                    app->undo_redo.saveState(src);
                    return true;
                }
                return false;
            }
        }, this);

        herm_editor.draw();

        // Status in properties
        ImGui::SameLine();
        ImGui::Text("%s", editor_has_scene ? "preview: ON" : "preview: OFF");
        ImGui::End();
    }

    // --- T-113: Console/Log panel ---
    console.draw();

    // --- T-116: Profiler panel ---
    profiler.draw();

    // --- T-103: Ontology Tree panel ---
    ontology.draw(scene_mgr.graph, renderer.scene);

    // --- T-104: Inspector F/Q/B panel ---
    inspector.draw(ontology, scene_mgr.graph, renderer.scene);

    // --- T-112: Tensor Inspector panel ---
    tensor_inspector.draw(ontology, scene_mgr.graph, renderer.scene);
}

}
