#pragma once
#include <string>
#include "window_manager.h"
#include "input_controller.h"
#include "scene_manager.h"
#include "herm_editor.h"
#include "scheduler.h"
#include "input_bus.h"
#include "anomaly_gate.h"
#include "console_panel.h"
#include "profiler_panel.h"
#include "ontology_panel.h"
#include "inspector_panel.h"
#include "gizmos_panel.h"
#include "tensor_inspector.h"
#include "undo_redo.h"
#include "../scene/workspace.h"

namespace mg {

struct VisorApp {
    WindowManager win;
    InputController input;
    SceneManager scene_mgr;
    
    Renderer renderer;
    scene::CameraController cam_ctrl;
    scene::Workspace workspace;

    // --- T-110: Scheduler + Input Bus + Anomaly Gate ---
    Scheduler   scheduler;
    InputBus    input_bus;
    std::vector<Anomaly> last_anomalies;

    // --- T-113: Console/Log panel ---
    ConsolePanel console;

    // --- T-116: Profiler panel ---
    ProfilerPanel profiler;

    // --- T-103: Ontology Tree panel ---
    OntologyPanel ontology;

    // --- T-104: Inspector F/Q/B panel ---
    InspectorPanel inspector;

    // --- T-105: Gizmos overlay panel ---
    GizmosPanel gizmos;

    // --- T-112: Tensor Inspector panel ---
    TensorInspector tensor_inspector;

    // --- T-115: Undo/Redo ---
    UndoRedo    undo_redo;

    // --- T-111: Editor .herm + live-compile ---
    HermEditor  herm_editor;       // editor de codigo .herm
    std::string editor_source;     // buffer del editor de codigo .herm (compat)
    std::string editor_status;     // estado del ultimo compile (exitos/errores)
    bool       editor_has_scene = false;
    
    bool dirty = true;
    bool running = true;
    bool camera_moving = false;
    u32 camera_stop_tick = 0;
    bool title_dirty = true;
    f32  render_scale = 1.0f;
    u32  full_res_tick = 0;
    bool toggle_held = false;
    bool undo_redo_held = false;

    // FPS counter + perf timing
    u32 frame_count = 0;
    u32 fps_last_tick = 0;
    u32 fps = 0;
    u32 last_render_ms = 0;
    u32 last_pump_ms = 0;
    char fps_text[64] = "FPS: --";

    bool bench_vulkan = false;
    u32 bench_frames = 0;
    u32 bench_max_frames = 200;
    LARGE_INTEGER bench_start, bench_end;
    bool bench_clock_started = false;

    bool init(HINSTANCE hInst, const wchar_t* initial_scene);
    void run();
    void renderFrame();
    void updateTitle();
    void drawEditorUI();
};

}
