#include "input_controller.h"
#include <windowsx.h>

namespace mg {

bool InputController::updateCamera(float dt, scene::CameraController& cam_ctrl) {
    bool fwd = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool bwd = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool lft = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool rgt = (GetAsyncKeyState('D') & 0x8000) != 0;
    bool up  = (GetAsyncKeyState('Q') & 0x8000) != 0;
    bool dwn = (GetAsyncKeyState('E') & 0x8000) != 0;

    if (fwd || bwd || lft || rgt || up || dwn) {
        cam_ctrl.updateFly(dt, fwd, bwd, lft, rgt, up, dwn);
        return true;
    }
    return false;
}

bool InputController::handleKeyDown(WPARAM wp, scene::CameraController& cam_ctrl, HWND hwnd_viewport) {
    bool dirty = false;
    if (wp == VK_F1) {
        mouse_look = !mouse_look;
        if (mouse_look) {
            // Save current camera state into FREE_FLY before switching
            Vec3 pos = cam_ctrl.getPosition();
            Vec3 tgt = cam_ctrl.getTarget();
            Vec3 dir = {tgt.x - pos.x, tgt.y - pos.y, tgt.z - pos.z};
            f32 l = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
            if (l > 0) { dir.x /= l; dir.y /= l; dir.z /= l; }
            cam_ctrl.yaw = std::atan2(dir.z, dir.x);
            cam_ctrl.pitch = std::asin(dir.y);
            cam_ctrl.fly_position = pos;
            cam_ctrl.fly_velocity = {0, 0, 0};
            cam_ctrl.mode = scene::CameraMode::FREE_FLY;
            GetCursorPos(&last_mouse);
            SetCapture(hwnd_viewport);
            ShowCursor(FALSE);
        } else {
            // Save current camera state into ORBIT before switching
            Vec3 pos = cam_ctrl.getPosition();
            Vec3 tgt = cam_ctrl.getTarget();
            Vec3 dir = {tgt.x - pos.x, tgt.y - pos.y, tgt.z - pos.z};
            f32 l = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
            cam_ctrl.orbit_target = tgt;
            cam_ctrl.orbit_distance = l > 0.001f ? l : 5.0f;
            cam_ctrl.target_distance = cam_ctrl.orbit_distance;
            if (l > 0.001f) { dir.x /= l; dir.y /= l; dir.z /= l; }
            cam_ctrl.orbit_azimuth = std::atan2(dir.z, dir.x);
            cam_ctrl.orbit_elevation = std::asin(dir.y);
            cam_ctrl.mode = scene::CameraMode::ORBIT;
            ReleaseCapture();
            ShowCursor(TRUE);
            SetCursorPos(last_mouse.x, last_mouse.y);
        }
        dirty = true;
    }
    if (wp == VK_OEM_PLUS || wp == VK_ADD) {
        cam_ctrl.zoom(-0.5f);
        dirty = true;
    }
    if (wp == VK_OEM_MINUS || wp == VK_SUBTRACT) {
        cam_ctrl.zoom(0.5f);
        dirty = true;
    }
    return dirty;
}

bool InputController::handleMouseMove(LPARAM lp, scene::CameraController& cam_ctrl, HWND hwnd_viewport, int vp_w, int vp_h) {
    if (mouse_look && hwnd_viewport == GetCapture()) {
        int dx = GET_X_LPARAM(lp) - vp_w / 2;
        int dy = GET_Y_LPARAM(lp) - vp_h / 2;
        RECT rc;
        GetClientRect(hwnd_viewport, &rc);
        
        POINT pt = { rc.left + vp_w / 2, rc.top + vp_h / 2 };
        ClientToScreen(hwnd_viewport, &pt);
        SetCursorPos(pt.x, pt.y);

        if (cam_ctrl.mode == scene::CameraMode::FREE_FLY) {
            cam_ctrl.mouseLook((f32)dx, (f32)(-dy));
        } else {
            cam_ctrl.orbitRotate((f32)dx, (f32)(-dy));
        }
        return true;
    }

    // Middle mouse button drag — always mouse look (rotate in place, no position change)
    if (mouse_dragging) {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        int dx = x - drag_start.x;
        int dy = y - drag_start.y;
        drag_start.x = x;
        drag_start.y = y;
        cam_ctrl.mouseLook((f32)dx, (f32)(-dy));
        return true;
    }

    return false;
}

bool InputController::handleMouseButton(UINT msg, LPARAM lp, scene::CameraController& cam_ctrl, HWND hwnd_capture) {
    if (msg == WM_MBUTTONDOWN) {
        mouse_dragging = true;
        SetCapture(hwnd_capture);
        drag_start.x = GET_X_LPARAM(lp);
        drag_start.y = GET_Y_LPARAM(lp);
        // Don't force ORBIT here — let orbitRotate() handle the mode switch
        // on first mouse move so it properly captures current camera state.
        return false;
    }
    if (msg == WM_MBUTTONUP) {
        mouse_dragging = false;
        ReleaseCapture();
        return false;
    }
    return false;
}

}
