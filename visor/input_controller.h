#pragma once
#include <windows.h>
#include "../scene/camera.h"

namespace mg {

struct InputController {
    bool mouse_look = false;
    bool mouse_dragging = false;
    POINT last_mouse = {0, 0};
    POINT drag_start = {0, 0};

    bool updateCamera(float dt, scene::CameraController& cam_ctrl);
    bool handleKeyDown(WPARAM wp, scene::CameraController& cam_ctrl, HWND hwnd_viewport);
    bool handleMouseMove(LPARAM lp, scene::CameraController& cam_ctrl, HWND hwnd_viewport, int vp_w, int vp_h);
    bool handleMouseButton(UINT msg, LPARAM lp, scene::CameraController& cam_ctrl, HWND hwnd_capture);
};

}
