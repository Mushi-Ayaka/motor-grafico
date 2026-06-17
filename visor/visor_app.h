#pragma once
#include "window_manager.h"
#include "input_controller.h"
#include "scene_manager.h"
#include "../scene/workspace.h"

namespace mg {

struct VisorApp {
    WindowManager win;
    InputController input;
    SceneManager scene_mgr;
    
    Renderer renderer;
    scene::CameraController cam_ctrl;
    scene::Workspace workspace;
    
    bool dirty = true;
    bool running = true;
    bool camera_moving = false;
    u32 camera_stop_tick = 0;
    bool title_dirty = true;
    f32  render_scale = 1.0f;
    u32  full_res_tick = 0;
    bool toggle_held = false;

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
};

}
