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
            // Switch to FREE_FLY with mouse look
            cam_ctrl.mode = scene::CameraMode::FREE_FLY;
            GetCursorPos(&last_mouse);
            SetCapture(hwnd_viewport);
            ShowCursor(FALSE);
        } else {
            // Switch back to ORBIT
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

    // Middle mouse button drag for orbit
    if (mouse_dragging) {
        int x = GET_X_LPARAM(lp);
        int y = GET_Y_LPARAM(lp);
        int dx = x - drag_start.x;
        int dy = y - drag_start.y;
        drag_start.x = x;
        drag_start.y = y;
        cam_ctrl.orbitRotate((f32)dx, (f32)(-dy));
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
        cam_ctrl.mode = scene::CameraMode::ORBIT;
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
