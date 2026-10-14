#include "visor_app.h"
#include <cstdio>
#include <cwchar>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>
#include "render/vulkan_core.h"
#include "imgui.h"
#include "imgui_internal.h" // v0.35: DockBuilder* para layout default
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "core/herm_bridge.h"
#include "input_bus.h"
#include "core/systems.h"

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
    scene_path = initial_scene ? initial_scene : L"";
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

    if (vk_ctx.init(win.hwnd_viewport)) {
        diag("[PHASE6] vk_ctx.init OK");
        if (!vk_ctx.initSwapchain(workspace.active().w, workspace.active().h)) {
            diag("[PHASE6] FAIL: initSwapchain — disabling Vulkan");
            renderer.use_vulkan = false;
        } else {
            diag("[PHASE6] initSwapchain OK");
        }
    } else {
        diag("[PHASE6] vk_ctx.init FAILED — disabling Vulkan, using CPU renderer");
        renderer.use_vulkan = false;
    }
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
            renderer.arena.init(64 * 1024 * 1024);
            renderer.jit.init();
            renderer.jit.compileScene(renderer.ont_scene);
            renderer.brick_map.destroy();
            buildBrickMap(renderer.ont_scene, renderer.brick_map);
            if (renderer.use_vulkan) {
                renderer.vk_scene.init(renderer.ont_scene);
                GlslGenResult gen = GlslGen::generate(renderer.ont_scene);
                if (gen.ok) {
                    std::vector<uint32_t> spv;
                    std::string compile_error;
                    if (compileGlslToSpv(gen.glsl_source, spv, compile_error)) {
                        if (renderer.vk_scene.recreateScenePipeline(spv)) {
                            renderer.vulkan_has_scene_shader = true;
                            logDiag("[VK] Default scene pipeline created");
                        } else {
                            logDiag("[VK] recreateScenePipeline FAILED, loading fallback");
                            renderer.vk_scene.initPipeline();
                        }
                    } else {
                        logDiag(("[GLSL] Compilation error: " + compile_error).c_str());
                        renderer.vk_scene.initPipeline();
                    }
                } else {
                    logDiag(("[GLSL] Generation error: " + gen.error).c_str());
                    renderer.vk_scene.initPipeline();
                }
            }
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
    undo_redo.executeCommand(std::make_shared<TextEditCommand>(editor_source, "", editor_source, "Initial state"));

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
                    r.tensor_slot_count = r.nodes + 1; // +1 for camera slot 0

                    // F0.5: Run Anomaly Gate validation
                    // Note: we need the RIH for full AST validation, but we only have OntScene here.
                    // For now, validate tensor slots. Full AST validation happens when we have the RIH.
                    auto tensor_anomalies = AnomalyGate::validateTensorSlots(r.nodes);
                    r.anomalies.insert(r.anomalies.end(), tensor_anomalies.begin(), tensor_anomalies.end());

                    // Validate bytecode if available
                    if (ont.bytecode && ont.header->bytecode_size > 0) {
                        auto bc_anomalies = AnomalyGate::validateBytecode(ont.bytecode, ont.header->bytecode_size);
                        r.anomalies.insert(r.anomalies.end(), bc_anomalies.begin(), bc_anomalies.end());
                    }
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
                    r.tensor_slot_count = r.nodes + 1; // +1 for camera slot 0
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

        // FixedTimestep: advance W in ΔW=1/60 steps
        float real_dt = 0.016f; // approximate frame time
        fixed_ts.advance(real_dt);
        renderer.time = fixed_ts.currentW();
        if (renderer.ont_scene.has_obs && renderer.ont_scene.obs.has_timeline) {
            f32 w_max = renderer.ont_scene.obs.w_max;
            if (w_max > 0) renderer.time = fmod(renderer.time, w_max);
        }

        // Systems v1: run in play_mode
        if (input_bus.play_mode && renderer.vk_scene.tensor_slot_count > 0) {
            // Snapshot tensor_buffer → tensor_snapshot (inmutable durante tick)
            static float tensor_snapshot[65536][8];
            static float tensor_delta[65536][8];
            const float* tensor_data = renderer.vk_scene.getTensorStagingData();
            if (tensor_data) {
                memcpy(tensor_snapshot, tensor_data, renderer.vk_scene.tensor_slot_count * 8 * sizeof(float));
            }
            memset(tensor_delta, 0, sizeof(tensor_delta));

            // Create system context
            SystemContext_v1 ctx;
            ctx.dt = FixedTimestep::DT;
            ctx.input = input_bus.state.snapshot();
            ctx.tensor_read = tensor_snapshot;
            ctx.tensor_delta = tensor_delta;
            ctx.graph = &scene_mgr.graph;
            ctx.node_count = renderer.vk_scene.tensor_slot_count;

            // Run registered systems
            for (uint32_t i = 0; i < DEFAULT_SYSTEM_COUNT; i++) {
                DEFAULT_SYSTEMS[i].tick(&ctx);
            }

            // Apply deltas atómicamente (suma conmutativa)
            // Note: In v1, this modifies the snapshot; actual GPU buffer update happens in next frame
        }

        // Per-frame camera smoothing
        cam_ctrl.updateSmoothing(0.016f);

        // Process camera input (WASD, keys)
        bool cam_moved = input.updateCamera(0.016f, cam_ctrl);
        bool cam_rotated = input.mouse_dragging || input.mouse_look;
        if (cam_moved || cam_rotated) {
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

        // Autosave check (every 30 seconds)
        if (!project_path.empty() && project.shouldAutosave(now)) {
            project.applyFrom(renderer.scene);
            project.save(project_path.c_str());
            project.saveBackup(project_path.c_str());
            project.updateAutosaveTime(now);
            console.addLog(LogEntry::Level::INFO, "Autosave: %ls", project_path.c_str());
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

        // F2: oculta/muestra la UI ImGui (para grabar video limpio)
        if (GetAsyncKeyState(VK_F2) & 0x8000) {
            if (!ui_toggle_held) {
                vk_ctx.ui_visible = !vk_ctx.ui_visible;
                diag(vk_ctx.ui_visible ? "[UI] Overlay ON" : "[UI] Overlay OFF");
                ui_toggle_held = true;
            }
        } else {
            ui_toggle_held = false;
        }

        // T-115: Ctrl+Z = Undo, Ctrl+Y = Redo
        bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ctrl) {
            if (GetAsyncKeyState('Z') & 0x8000) {
                if (!undo_redo_held) {
                    if (undo_redo.undo()) {
                        herm_editor.source = editor_source;
                        herm_editor.reloadFromSource();
                        console.addLog(LogEntry::Level::INFO, "Undo: %s", undo_redo.undoDescription().c_str());
                    }
                    undo_redo_held = true;
                }
            } else if (GetAsyncKeyState('Y') & 0x8000) {
                if (!undo_redo_held) {
                    if (undo_redo.redo()) {
                        herm_editor.source = editor_source;
                        herm_editor.reloadFromSource();
                        console.addLog(LogEntry::Level::INFO, "Redo: %s", undo_redo.redoDescription().c_str());
                    }
                    undo_redo_held = true;
                }
            } else if (GetAsyncKeyState('K') & 0x8000) {
                // T-119: Ctrl+K = Command Palette
                if (!command_palette.visible) {
                    command_palette.toggle();
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
            LARGE_INTEGER qpc_r0, qpc_rf;
            QueryPerformanceCounter(&qpc_r0);
            QueryPerformanceFrequency(&qpc_rf);
            
            if (renderer.use_vulkan && renderer.ont_mode) {
                // Check for viewport resize
                VkExtent2D cur = vk_ctx.swapchain_extent;
                int vp_w = workspace.active().w;
                int vp_h = workspace.active().h;
                // Render at reduced scale for performance
                int rw = (int)(vp_w * render_scale + 0.5f);
                int rh = (int)(vp_h * render_scale + 0.5f);
                // Escena con resolución fija (.obs axes X/Y): renderizar a esa resolución
                if (renderer.ont_scene.has_obs && renderer.ont_scene.obs.has_resolution &&
                    renderer.ont_scene.obs.width > 0 && renderer.ont_scene.obs.height > 0) {
                    rw = (int)(renderer.ont_scene.obs.width  * render_scale + 0.5f);
                    rh = (int)(renderer.ont_scene.obs.height * render_scale + 0.5f);
                }
                if (rw < 1) rw = 1;
                if (rh < 1) rh = 1;
                if (stats_mode) { stats_rw = (uint32_t)rw; stats_rh = (uint32_t)rh; }
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
                // El dispatch escribe rw*rh píxeles: el buffer debe tener ese tamaño exacto
                {
                    VkDeviceSize want = (VkDeviceSize)rw * (VkDeviceSize)rh * 4;
                    if (renderer.vk_scene.output_buffer.buffer != VK_NULL_HANDLE &&
                        renderer.vk_scene.output_buffer.size != want) {
                        renderer.vk_scene.resizeOutputBuffer(rw, rh);
                    }
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
                ubo.shade_ambient = renderer.ont_scene.pipeline.shade_ambient;
                ubo.shade_specular = renderer.ont_scene.pipeline.shade_specular;
                ubo.post_exposure = renderer.ont_scene.pipeline.post_exposure;
                ubo.post_tonemap = renderer.ont_scene.pipeline.post_tonemap ? 1u : 0u;
                ubo.post_gamma = renderer.ont_scene.pipeline.post_gamma ? 1u : 0u;

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
                    ImGui_ImplVulkan_NewFrame(); // recrea font texture si fue destruida
                    ImGui::NewFrame();
                    ImGuiIO& io = ImGui::GetIO();
                    io.DisplaySize = ImVec2((float)workspace.active().w, (float)workspace.active().h);
                    drawEditorUI();
                    ImGui::Render();
                }

                // A2: capturar resultado de drawFrame — false = OUT_OF_DATE/SUBOPTIMAL → recrear swapchain
                bool frame_ok = vk_ctx.drawFrame(renderer.vk_scene, rw, rh);
                if (!frame_ok) {
                    diag("[VK] drawFrame returned OUT_OF_DATE/SUBOPTIMAL — recreando swapchain");
                    vk_ctx.resizeSwapchain(vp_w, vp_h);
                    renderer.vk_scene.resizeOutputBuffer(rw, rh);
                }

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
            {
                LARGE_INTEGER qpc_r1;
                QueryPerformanceCounter(&qpc_r1);
                render_ms_qpc = (double)(qpc_r1.QuadPart - qpc_r0.QuadPart) * 1000.0 / (double)qpc_rf.QuadPart;
            }
            if (!(renderer.use_vulkan && renderer.ont_mode) && !bench_vulkan) dirty = false;
        }

        // FPS counter — update once per second
        frame_count++;
        if (stats_mode) {
            LARGE_INTEGER qpc_now, qpc_freq;
            QueryPerformanceCounter(&qpc_now);
            QueryPerformanceFrequency(&qpc_freq);
            // A2: contar solo frames con dispatch real (pipeline_ready && acquire OK)
            bool did_render = (renderer.use_vulkan && renderer.ont_mode)
                ? renderer.vk_scene.pipeline_ready
                : true;
            if (did_render) stats_frames_render++;
            else            stats_frames_skip++;
            if (stats_clock_started) {
                double ms = (double)(qpc_now.QuadPart - stats_qpc_prev.QuadPart) * 1000.0 / (double)qpc_freq.QuadPart;
                if (ms > 0.0 && ms < 60000.0) {
                    stats_frame_ms.push_back(ms);
                    stats_render_ms.push_back(render_ms_qpc);
                    stats_gpu_ms.push_back(vk_ctx.last_gpu_ms);
                }
            }
            stats_qpc_prev = qpc_now;
            stats_clock_started = true;
        }
        if (now - fps_last_tick > 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_last_tick = now;
            snprintf(fps_text, sizeof(fps_text), "FPS: %u | render: %ums | pump: %ums | scale: %.2f", fps, last_render_ms, last_pump_ms, render_scale);

            // A4: nvidia-smi polling 1Hz en modo --stats (gpu%, power, temp, vram, clocks)
            if (stats_mode) {
                pollNvidiaSmi();
            }
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

        // T-114: Autosave backup every 60 seconds
        if (project_dirty && !project_path.empty()) {
            static u32 last_autosave = 0;
            if (now - last_autosave > 60000) {
                project.applyFrom(renderer.scene);
                project.editor_source = editor_source;
                project.gizmo_mode = (int)gizmos.mode;
                project.render_scale = render_scale;
                project.saveBackup(project_path.c_str());
                last_autosave = now;
            }
        }

        // T-118: Update Tensor Inspector with GPU readback data
        if (vk_ctx.readback_ready) {
            float gpu_tensor[8] = {};
            if (vk_ctx.getReadbackData(gpu_tensor, 8)) {
                tensor_inspector.updateFromGPU(gpu_tensor);
            }
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
                if (renderer.ont_scene.has_obs && renderer.ont_scene.obs.has_resolution &&
                    renderer.ont_scene.obs.width > 0 && renderer.ont_scene.obs.height > 0) {
                    bw = (int)(renderer.ont_scene.obs.width  * render_scale + 0.5f);
                    bh = (int)(renderer.ont_scene.obs.height * render_scale + 0.5f);
                }
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

        // Adaptive sleep: skip in bench-vulkan / stats mode (medición honesta)
        if (!bench_vulkan && !stats_mode) {
            Sleep(camera_moving ? 8 : 32);
        }
    }
    if (stats_mode) writeStats();
    // On-close save
    if (!project_path.empty() && project.isDirty()) {
        project.applyFrom(renderer.scene);
        project.save(project_path.c_str());
        project.saveBackup(project_path.c_str());
        project.clearDirty();
    }
    if (vk_ctx.device) vkDeviceWaitIdle(vk_ctx.device);
    renderer.vk_scene.cleanup();
    vk_ctx.destroyViewportImage();
    if (vk_ctx.imgui_enabled) {
        vk_ctx.shutdownImGui();
        ImGui::DestroyContext();
    }
    vk_ctx.cleanup();
    win.destroy();
}

// ============================================================================
// STATS (--stats): estadística de población por corrida -> stats/*.csv
// ============================================================================

// A4: nvidia-smi polling — una consulta por segundo, costo despreciable
void VisorApp::pollNvidiaSmi() {
    FILE* pipe = _popen(
        "nvidia-smi --query-gpu=utilization.gpu,power.draw,temperature.gpu,memory.used,clocks.sm "
        "--format=csv,noheader,nounits", "r");
    if (!pipe) return;
    char line[256] = {};
    if (fgets(line, sizeof(line), pipe)) {
        // Formato esperado: "45, 75.50, 62, 822, 1410" (separado por coma+espacio)
        double u, p, t, m, c;
        if (sscanf_s(line, "%lf, %lf, %lf, %lf, %lf", &u, &p, &t, &m, &c) == 5) {
            gpu_util_pct = u;
            gpu_power_w = p;
            gpu_temp_c = t;
            vram_used_mb = m;
            gpu_clock_mhz = c;
        }
    }
    _pclose(pipe);
}

void VisorApp::writeStats() {
    if (stats_frame_ms.empty()) {
        fprintf(stderr, "[STATS] Sin frames capturados.\n");
        return;
    }

    size_t n = stats_frame_ms.size();
    std::vector<double> f = stats_frame_ms;
    std::sort(f.begin(), f.end());

    double sum = 0.0;
    for (double v : f) sum += v;
    double mean = sum / (double)n;
    double var = 0.0;
    for (double v : f) { double d = v - mean; var += d * d; }
    var /= (double)n;                       // varianza de POBLACIÓN (÷N)
    double sd = std::sqrt(var);
    double mn = f.front();
    double mx = f.back();
    double med = (n % 2) ? f[n / 2] : (f[n / 2 - 1] + f[n / 2]) / 2.0;
    double p95 = f[(size_t)(0.95 * (double)(n - 1))];
    double p99 = f[(size_t)(0.99 * (double)(n - 1))];
    double cv = (mean > 0.0) ? (sd / mean) * 100.0 : 0.0;
    double fps_mean = (mean > 0.0) ? 1000.0 / mean : 0.0;
    double fps_1pct = (p99 > 0.0) ? 1000.0 / p99 : 0.0;
    double total_s = 0.0;
    for (double v : stats_frame_ms) total_s += v;
    total_s /= 1000.0;

    double r_mean = 0.0, r_p95 = 0.0, r_min = 0.0, r_max = 0.0;
    if (!stats_render_ms.empty()) {
        std::vector<double> r = stats_render_ms;
        std::sort(r.begin(), r.end());
        double s = 0.0;
        for (double v : r) s += v;
        r_mean = s / (double)r.size();
        r_min = r.front();
        r_max = r.back();
        r_p95 = r[(size_t)(0.95 * (double)(r.size() - 1))];
    }

    // A1: estadísticas de GPU dispatch time (vkGetQueryPoolResults)
    double g_mean = 0.0, g_p95 = 0.0, g_min = 0.0, g_max = 0.0;
    if (!stats_gpu_ms.empty()) {
        std::vector<double> g = stats_gpu_ms;
        std::sort(g.begin(), g.end());
        double s = 0.0;
        for (double v : g) s += v;
        g_mean = s / (double)g.size();
        g_min = g.front();
        g_max = g.back();
        g_p95 = g[(size_t)(0.95 * (double)(g.size() - 1))];
    }

    // Nombre de escena (basename sin extensión)
    char name_buf[512] = "desconocida";
    if (!scene_path.empty()) {
        std::wstring base = scene_path;
        size_t slash = base.find_last_of(L"\\/");
        if (slash != std::wstring::npos) base = base.substr(slash + 1);
        size_t dot = base.rfind(L'.');
        if (dot != std::wstring::npos) base = base.substr(0, dot);
        WideCharToMultiByte(CP_UTF8, 0, base.c_str(), -1, name_buf, (int)sizeof(name_buf), nullptr, nullptr);
    }
    std::string scene_name = name_buf;

    // Metadatos de escena
    uint32_t nodes = 0, bvh = 0, mats = 0, bc = 0;
    if (renderer.ont_scene.header) {
        nodes = renderer.ont_scene.header->node_count;
        bvh   = renderer.ont_scene.header->bvh_count;
        mats  = renderer.ont_scene.header->material_count;
        bc    = renderer.ont_scene.header->bytecode_size;
    }

    // GPU + timestamp
    const char* gpu = vk_ctx.device_name[0] ? vk_ctx.device_name : "desconocido";
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char fecha[64];
    sprintf_s(fecha, "%04u-%02u-%02u %02u:%02u:%02u",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    char ts[64];
    sprintf_s(ts, "%04u%02u%02u_%02u%02u%02u",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::error_code ec;
    std::filesystem::create_directories("stats", ec);

    // --- 1) runs.csv (append, una fila por corrida) ---
    bool header_new = !std::filesystem::exists("stats/runs.csv", ec);
    FILE* fcsv = nullptr;
    fopen_s(&fcsv, "stats/runs.csv", "a");
    if (fcsv) {
        if (header_new) {
            fprintf(fcsv, "fecha,escena,vp_w,vp_h,escala,render_w,render_h,nodos,bvh,materiales,bytecode,gpu,"
                          "frames,media_ms,mediana_ms,desv_pob_ms,var_pob_ms,min_ms,max_ms,rango_ms,"
                          "p95_ms,p99_ms,cv_pct,fps_media,fps_1pct_low,"
                          "render_media_ms,render_min_ms,render_max_ms,render_p95_ms,"
                          "gpu_media_ms,gpu_min_ms,gpu_max_ms,gpu_p95_ms,frames_render,frames_skip,"
                          "gpu_util_pct,gpu_power_w,gpu_temp_c,vram_mb,gpu_clock_mhz,total_s\n");
        }
        fprintf(fcsv,
            "%s,\"%s\",%d,%d,%.2f,%u,%u,%u,%u,%u,%u,\"%s\","
            "%zu,%.4f,%.4f,%.4f,%.6f,%.4f,%.4f,%.4f,"
            "%.4f,%.4f,%.2f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,"
            "%.3f,%.3f,%.3f,%.3f,%u,%u,"
            "%.1f,%.2f,%.1f,%.1f,%.1f,%.3f\n",
            fecha, scene_name.c_str(),
            workspace.active().w, workspace.active().h, render_scale, stats_rw, stats_rh,
            nodes, bvh, mats, bc, gpu,
            n, mean, med, sd, var, mn, mx, mx - mn,
            p95, p99, cv, fps_mean, fps_1pct,
            r_mean, r_min, r_max, r_p95,
            g_mean, g_min, g_max, g_p95, stats_frames_render, stats_frames_skip,
            gpu_util_pct, gpu_power_w, gpu_temp_c, vram_used_mb, gpu_clock_mhz, total_s);
        fclose(fcsv);
    }

    // --- 2) dump por frame: stats/run_<ts>_<escena>.csv ---
    char per_path[640];
    snprintf(per_path, sizeof(per_path), "stats/run_%s_%s.csv", ts, scene_name.c_str());
    FILE* fpf = nullptr;
    fopen_s(&fpf, per_path, "w");
    if (fpf) {
        fprintf(fpf, "frame,frame_ms,render_ms,gpu_ms\n");
        for (size_t i = 0; i < stats_frame_ms.size(); i++) {
            double rm = (i < stats_render_ms.size()) ? stats_render_ms[i] : 0.0;
            double gm = (i < stats_gpu_ms.size())   ? stats_gpu_ms[i]   : 0.0;
            fprintf(fpf, "%zu,%.4f,%.3f,%.3f\n", i, stats_frame_ms[i], rm, gm);
        }
        fclose(fpf);
    }

    // --- 3) Screenshot BMP (BGRA en memoria = orden BMP nativo, bottom-up) ---
    char shot_path[640] = "";
    {
        vkDeviceWaitIdle(vk_ctx.device);
        uint32_t sw = stats_rw, sh = stats_rh;
        VkDeviceSize need = (VkDeviceSize)sw * (VkDeviceSize)sh * 4;
        if (sw > 0 && sh > 0 &&
            renderer.vk_scene.output_staging.buffer != VK_NULL_HANDLE &&
            renderer.vk_scene.output_staging.size >= need) {
            void* mapped = nullptr;
            if (vmaMapMemory(vk_ctx.allocator, renderer.vk_scene.output_staging.allocation, &mapped) == VK_SUCCESS) {
                snprintf(shot_path, sizeof(shot_path), "stats/shot_%s_%s.bmp", ts, scene_name.c_str());
                FILE* fbmp = nullptr;
                fopen_s(&fbmp, shot_path, "wb");
                if (fbmp) {
                    uint32_t row_bytes = sw * 4;
                    uint32_t data_size = row_bytes * sh;
                    uint32_t file_size = 54 + data_size;
                    uint8_t hdr[54] = {};
                    auto put16 = [&](int off, uint16_t v) { hdr[off] = (uint8_t)(v & 255); hdr[off + 1] = (uint8_t)(v >> 8); };
                    auto put32 = [&](int off, uint32_t v) {
                        hdr[off] = (uint8_t)(v & 255); hdr[off + 1] = (uint8_t)((v >> 8) & 255);
                        hdr[off + 2] = (uint8_t)((v >> 16) & 255); hdr[off + 3] = (uint8_t)(v >> 24);
                    };
                    hdr[0] = 'B'; hdr[1] = 'M';
                    put32(2, file_size);
                    put32(10, 54);
                    put32(14, 40);
                    put32(18, sw);
                    put32(22, sh);          // positivo = filas bottom-up
                    put16(26, 1);           // planes
                    put16(28, 32);          // bpp
                    put32(30, 0);           // BI_RGB
                    put32(34, data_size);
                    fwrite(hdr, 1, 54, fbmp);
                    const uint8_t* px = (const uint8_t*)mapped;
                    for (int y = (int)sh - 1; y >= 0; y--)
                        fwrite(px + (size_t)y * row_bytes, 1, row_bytes, fbmp);
                    fclose(fbmp);
                }
                vmaUnmapMemory(vk_ctx.allocator, renderer.vk_scene.output_staging.allocation);
            }
        }
    }

    // --- 4) Resumen en consola ---
    fprintf(stderr, "\n=== STATS: %s ===\n", scene_name.c_str());
    fprintf(stderr, "GPU: %s | res render: %ux%u (escala %.2f, vp %dx%d)\n",
            gpu, stats_rw, stats_rh, render_scale, workspace.active().w, workspace.active().h);
    fprintf(stderr, "Escena: %u nodos, %u bvh, %u materiales, %u bytecode bytes\n",
            nodes, bvh, mats, bc);
    fprintf(stderr, "Frames: %zu (%u render / %u skip) en %.3f s\n", n, stats_frames_render, stats_frames_skip, total_s);
    fprintf(stderr, "frame_ms  media=%.3f mediana=%.3f desv=%.3f (pob) var=%.4f cv=%.2f%%\n",
            mean, med, sd, var, cv);
    fprintf(stderr, "frame_ms  min=%.3f max=%.3f rango=%.3f p95=%.3f p99=%.3f\n",
            mn, mx, mx - mn, p95, p99);
    fprintf(stderr, "FPS       media=%.2f 1%%_low=%.2f\n", fps_mean, fps_1pct);
    fprintf(stderr, "render_ms media=%.3f min=%.3f max=%.3f p95=%.3f\n",
            r_mean, r_min, r_max, r_p95);
    if (!stats_gpu_ms.empty()) {
        fprintf(stderr, "gpu_ms    media=%.3f min=%.3f max=%.3f p95=%.3f\n",
                g_mean, g_min, g_max, g_p95);
    }
    if (gpu_util_pct >= 0.0) {
        fprintf(stderr, "GPUHW     util=%.0f%% power=%.1fW temp=%.0fC vram=%.0fMB clock=%.0fMHz\n",
                gpu_util_pct, gpu_power_w, gpu_temp_c, vram_used_mb, gpu_clock_mhz);
    }
    fprintf(stderr, "Archivos: stats/runs.csv + %s", per_path);
    if (shot_path[0]) fprintf(stderr, " + %s", shot_path);
    fprintf(stderr, "\n");
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
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                herm_editor.initDefault();
                editor_source = herm_editor.source;
                undo_redo.clear();
                undo_redo.executeCommand(std::make_shared<TextEditCommand>(editor_source, "", editor_source, "New project"));
                project.setDefault();
                project_path.clear();
                project_dirty = false;
                console.addLog(LogEntry::Level::INFO, "New project created");
            }
            if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O")) {
#ifdef _WIN32
                char filename[MAX_PATH] = {};
                OPENFILENAMEA ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = GetActiveWindow();
                ofn.lpstrFilter = "MG Project (*.mgproj)\0*.mgproj\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameA(&ofn)) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
                    if (wlen > 0) {
                        std::wstring wpath((size_t)wlen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, filename, -1, &wpath[0], wlen);
                        if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
                        if (project.loadWithRecovery(wpath.c_str())) {
                            project_path = wpath;
                            // Apply project state
                            project.applyTo(renderer.scene);
                            if (!project.editor_source.empty()) {
                                herm_editor.source = project.editor_source;
                                herm_editor.reloadFromSource();
                                editor_source = project.editor_source;
                            }
                            gizmos.mode = (GizmoMode)project.gizmo_mode;
                            render_scale = project.render_scale;
                            console.auto_scroll = project.auto_scroll;
                            console.filter_level = project.console_filter;
                            undo_redo.clear();
                            undo_redo.executeCommand(std::make_shared<TextEditCommand>(editor_source, "", editor_source, "Load project"));
                            dirty = true;
                            title_dirty = true;
                            console.addLog(LogEntry::Level::INFO, "Project loaded: %s", filename);
                        } else {
                            console.addLog(LogEntry::Level::ERROR_LOG, "Failed to load project: %s", filename);
                        }
                    }
                }
#endif
            }
            if (ImGui::MenuItem("Open .herm...", "Ctrl+O")) {
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
                    undo_redo.executeCommand(std::make_shared<TextEditCommand>(editor_source, "", editor_source, "Open .herm file"));
                    // Add to project sources
                    project.sources.clear();
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
                    if (wlen > 0) {
                        std::wstring wpath((size_t)wlen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, filename, -1, &wpath[0], wlen);
                        if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
                        project.sources.push_back({wpath});
                    }
                    project_dirty = true;
                    console.addLog(LogEntry::Level::INFO, "Opened: %s", filename);
                }
#endif
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                if (project_path.empty()) {
                    // Save As
#ifdef _WIN32
                    char filename[MAX_PATH] = {};
                    OPENFILENAMEA ofn = {};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = GetActiveWindow();
                    ofn.lpstrFilter = "MG Project (*.mgproj)\0*.mgproj\0";
                    ofn.lpstrFile = filename;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrDefExt = "mgproj";
                    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
                    if (GetSaveFileNameA(&ofn)) {
                        int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
                        if (wlen > 0) {
                            std::wstring wpath((size_t)wlen, L'\0');
                            MultiByteToWideChar(CP_UTF8, 0, filename, -1, &wpath[0], wlen);
                            if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
                            project_path = wpath;
                        }
                    }
#endif
                }
                if (!project_path.empty()) {
                    // Sync editor state to project
                    project.applyFrom(renderer.scene);
                    project.editor_source = editor_source;
                    project.gizmo_mode = (int)gizmos.mode;
                    project.render_scale = render_scale;
                    project.auto_scroll = console.auto_scroll;
                    project.console_filter = console.filter_level;
                    if (project.save(project_path.c_str())) {
                        project_dirty = false;
                        console.addLog(LogEntry::Level::SUCCESS, "Project saved");
                    } else {
                        console.addLog(LogEntry::Level::ERROR_LOG, "Failed to save project");
                    }
                }
            }
            if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S")) {
#ifdef _WIN32
                char filename[MAX_PATH] = {};
                OPENFILENAMEA ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = GetActiveWindow();
                ofn.lpstrFilter = "MG Project (*.mgproj)\0*.mgproj\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrDefExt = "mgproj";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
                if (GetSaveFileNameA(&ofn)) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
                    if (wlen > 0) {
                        std::wstring wpath((size_t)wlen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, filename, -1, &wpath[0], wlen);
                        if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
                        project_path = wpath;
                        // Sync and save
                        project.applyFrom(renderer.scene);
                        project.editor_source = editor_source;
                        project.gizmo_mode = (int)gizmos.mode;
                        project.render_scale = render_scale;
                        if (project.save(project_path.c_str())) {
                            project_dirty = false;
                            console.addLog(LogEntry::Level::SUCCESS, "Project saved as: %s", filename);
                        }
                    }
                }
#endif
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export...", "Ctrl+E")) {
                export_dialog.visible = true;
            }
            if (ImGui::MenuItem("Source Browser", "Ctrl+B")) {
                source_browser.visible = !source_browser.visible;
                if (source_browser.visible && source_browser.root_path.empty()) {
                    // Set root from project path
                    if (!project_path.empty()) {
                        char mbuf[MAX_PATH];
                        WideCharToMultiByte(CP_UTF8, 0, project_path.c_str(), -1, mbuf, MAX_PATH, nullptr, nullptr);
                        std::string proj_str(mbuf);
                        size_t last_sep = proj_str.find_last_of("\\/");
                        if (last_sep != std::string::npos) {
                            source_browser.root_path = proj_str.substr(0, last_sep);
                            source_browser.refresh();
                        }
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { running = false; }
            ImGui::EndMenu();
        }
        // --- Edit ---
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                std::string src;
                if (undo_redo.undo()) { herm_editor.source = editor_source; herm_editor.reloadFromSource(); }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                std::string src;
                if (undo_redo.redo()) { herm_editor.source = editor_source; herm_editor.reloadFromSource(); }
            }
            ImGui::EndMenu();
        }
        // --- Window (v0.35): toggles por ventana; stubs deshabilitados ---
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Viewport",         nullptr, &show_viewport);
            ImGui::MenuItem("Editor .herm",     nullptr, &herm_editor.visible);
            ImGui::MenuItem("Properties",       nullptr, &show_properties);
            ImGui::MenuItem("Console",          nullptr, &console.visible);
            ImGui::Separator();
            ImGui::MenuItem("Profiler",         nullptr, &profiler.visible);
            ImGui::MenuItem("Ontology Tree",    nullptr, &ontology.visible);
            ImGui::MenuItem("Inspector",        nullptr, &inspector.visible);
            ImGui::MenuItem("Tensor Inspector", nullptr, &tensor_inspector.visible);
            ImGui::MenuItem("Timeline",         nullptr, &timeline_panel.visible);
            ImGui::MenuItem("Source Browser",   nullptr, &source_browser.visible);
            ImGui::Separator();
            ImGui::BeginDisabled(); // stub: escribe .bat con TODO
            ImGui::MenuItem("Export Executable (stub)");
            ImGui::EndDisabled();
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

    // v0.35: layout default solo si no hay layout persistido (imgui.ini con [Docking])
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID center = dockspace_id;
        ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.25f, nullptr, &center);
        ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.28f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Editor .herm", left);
        ImGui::DockBuilderDockWindow("Properties",   right);
        ImGui::DockBuilderDockWindow("Console",      bottom);
        ImGui::DockBuilderDockWindow("Viewport",     center);
        ImGui::DockBuilderFinish(dockspace_id);
        diag("[UI] DockSpace layout default creado (4 paneles)");
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    // T-120: Multi-viewport with tabs
    viewport_mgr.setDefaultDS(vk_ctx.viewport_ds);
    if (ImGui::Begin("Viewport", &show_viewport)) {
        // Tab bar
        if (ImGui::BeginTabBar("##viewport_tabs")) {
            for (int i = 0; i < (int)viewport_mgr.tabs.size(); i++) {
                ImGuiTabBarFlags flags = ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
                if (ImGui::BeginTabItem(viewport_mgr.tabs[i].name.c_str(), nullptr, flags)) {
                    viewport_mgr.active_tab = i;
                    ImGui::EndTabItem();
                }
            }
            if (ImGui::TabItemButton("+")) {
                char name[32];
                snprintf(name, sizeof(name), "View %d", viewport_mgr.next_tab_id++);
                viewport_mgr.addTab(name, ViewportMode::RENDER);
            }
            ImGui::EndTabBar();
        }

        // Draw active viewport
        if (viewport_mgr.active_tab >= 0 && viewport_mgr.active_tab < (int)viewport_mgr.tabs.size()) {
            auto& tab = viewport_mgr.tabs[viewport_mgr.active_tab];
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
        }

        // Mode indicator
        if (viewport_mgr.active_tab >= 0 && viewport_mgr.active_tab < (int)viewport_mgr.tabs.size()) {
            const char* mode_str = "?";
            switch (viewport_mgr.tabs[viewport_mgr.active_tab].mode) {
                case ViewportMode::RENDER: mode_str = "Render"; break;
                case ViewportMode::IR:     mode_str = "IR/Topology"; break;
                case ViewportMode::TENSOR: mode_str = "Tensor"; break;
            }
            ImGui::Text("Mode: %s", mode_str);
        }
    }
    ImGui::End();
    if (ImGui::Begin("Properties", &show_properties)) {
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
    }
    ImGui::End();

    // --- Editor de codigo .herm + live-compile (T-111) ---
    if (ImGui::Begin("Editor .herm", &herm_editor.visible)) {
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
                    std::string old_src = app->editor_source;
                    app->editor_source = src;
                    app->undo_redo.executeCommand(std::make_shared<TextEditCommand>(app->editor_source, old_src, src, "Compile .herm (ONT)"));
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
                    std::string old_src = app->editor_source;
                    app->editor_source = src;
                    app->undo_redo.executeCommand(std::make_shared<TextEditCommand>(app->editor_source, old_src, src, "Compile .herm (CPU)"));
                    return true;
                }
                return false;
            }
        }, this);

        herm_editor.draw();

        // Status in properties
        ImGui::SameLine();
        ImGui::Text("%s", editor_has_scene ? "preview: ON" : "preview: OFF");
    }
    ImGui::End();

    // --- T-113: Console/Log panel ---
    console.draw();

    // --- T-116: Profiler panel ---
    profiler.draw();

    // --- S2.8: Timeline UI ---
    timeline_panel.draw(workspace.timeline);

    // --- T-103: Ontology Tree panel ---
    ontology.draw(scene_mgr.graph, renderer.scene);

    // --- T-104: Inspector F/Q/B panel ---
    inspector.draw(ontology, scene_mgr.graph, renderer.scene);

    // --- T-112: Tensor Inspector panel ---
    tensor_inspector.draw(ontology, scene_mgr.graph, renderer.scene);

    // --- T-117: Source Browser panel ---
    source_browser.draw();

    // --- T-117: Export dialog ---
    export_dialog.draw();

    // --- T-119: Command Palette ---
    command_palette.draw();
}

}
