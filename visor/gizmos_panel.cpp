#include "gizmos_panel.h"
#include <cmath>

namespace mg {

bool GizmosPanel::worldToScreen(Vec3 world, Vec3 cam_pos, Vec3 cam_tgt,
                                 uint32_t vw, uint32_t vh, float& sx, float& sy) {
    // Simple perspective projection
    Vec3 fwd = {cam_tgt.x - cam_pos.x, cam_tgt.y - cam_pos.y, cam_tgt.z - cam_pos.z};
    float fl = std::sqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
    if (fl < 0.001f) return false;
    fwd.x /= fl; fwd.y /= fl; fwd.z /= fl;

    // Right vector (assume up = 0,1,0)
    Vec3 up = {0, 1, 0};
    Vec3 right = {fwd.y*up.z - fwd.z*up.y, fwd.z*up.x - fwd.x*up.z, fwd.x*up.y - fwd.y*up.x};
    float rl = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    if (rl < 0.001f) return false;
    right.x /= rl; right.y /= rl; right.z /= rl;

    // Recalculate up
    Vec3 u = {right.y*fwd.z - right.z*fwd.y, right.z*fwd.x - right.x*fwd.z, right.x*fwd.y - right.y*fwd.x};

    // Vector from camera to point
    Vec3 d = {world.x - cam_pos.x, world.y - cam_pos.y, world.z - cam_pos.z};

    // Project onto camera axes
    float z = d.x*fwd.x + d.y*fwd.y + d.z*fwd.z;
    if (z < 0.1f) return false; // behind camera

    float x = d.x*right.x + d.y*right.y + d.z*right.z;
    float y = d.x*u.x + d.y*u.y + d.z*u.z;

    // Perspective divide
    float fov = 1.0f; // ~90 degrees
    sx = (x / z * fov + 1.0f) * 0.5f * (float)vw;
    sy = (-y / z * fov + 1.0f) * 0.5f * (float)vh;
    return true;
}

void GizmosPanel::drawMoveGizmo(ImDrawList* dl, float cx, float cy, float scale) {
    float arrow_len = 40.0f * scale;
    float head_size = 8.0f * scale;

    // X axis (red)
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + arrow_len, cy), IM_COL32(255, 50, 50, 200), 2.0f);
    dl->AddTriangleFilled(ImVec2(cx + arrow_len, cy),
                          ImVec2(cx + arrow_len - head_size, cy - head_size * 0.5f),
                          ImVec2(cx + arrow_len - head_size, cy + head_size * 0.5f),
                          IM_COL32(255, 50, 50, 200));

    // Y axis (green)
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy - arrow_len), IM_COL32(50, 255, 50, 200), 2.0f);
    dl->AddTriangleFilled(ImVec2(cx, cy - arrow_len),
                          ImVec2(cx - head_size * 0.5f, cy - arrow_len + head_size),
                          ImVec2(cx + head_size * 0.5f, cy - arrow_len + head_size),
                          IM_COL32(50, 255, 50, 200));

    // Z axis (blue)
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + arrow_len * 0.7f, cy + arrow_len * 0.5f), IM_COL32(50, 50, 255, 200), 2.0f);
    dl->AddTriangleFilled(ImVec2(cx + arrow_len * 0.7f, cy + arrow_len * 0.5f),
                          ImVec2(cx + arrow_len * 0.7f - head_size, cy + arrow_len * 0.5f - head_size * 0.3f),
                          ImVec2(cx + arrow_len * 0.7f - head_size * 0.3f, cy + arrow_len * 0.5f + head_size * 0.5f),
                          IM_COL32(50, 50, 255, 200));
}

void GizmosPanel::handleDrag(scene::SceneNode& sn, Node& rn, int axis, float dx, float dy) {
    float speed = 0.01f;
    float delta = (dx + dy) * speed;

    if (snap_enabled) {
        delta = std::round(delta / snap_value) * snap_value;
    }

    switch (axis) {
        case 0: // X
            sn.local_translate.x += delta;
            rn.translate.x += delta;
            break;
        case 1: // Y
            sn.local_translate.y -= delta;
            rn.translate.y -= delta;
            break;
        case 2: // Z
            sn.local_translate.z += delta;
            rn.translate.z += delta;
            break;
    }
    sn.transform_dirty = true;
    sn.aabb_dirty = true;
}

void GizmosPanel::draw(OntologyPanel& ontology, scene::SceneGraph& graph,
                        Scene& scene, scene::CameraController& cam,
                        uint32_t viewport_w, uint32_t viewport_h) {
    if (!visible || mode == GizmoMode::NONE) return;

    int32_t sel = ontology.selected_node;
    if (sel < 0 || sel >= (int32_t)graph.nodes.size() || sel >= (int32_t)scene.nodes.size()) return;

    scene::SceneNode& sn = graph.nodes[sel];
    Node& rn = scene.nodes[sel];
    Vec3 pos = sn.world_translate;

    // Project to screen
    Vec3 cam_pos = cam.getPosition();
    Vec3 cam_tgt = cam.getTarget();
    float sx, sy;
    if (!worldToScreen(pos, cam_pos, cam_tgt, viewport_w, viewport_h, sx, sy)) return;

    // Get draw list (overlay)
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Scale based on distance
    Vec3 d = {pos.x - cam_pos.x, pos.y - cam_pos.y, pos.z - cam_pos.z};
    float dist = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
    float scale = std::max(0.3f, std::min(2.0f, dist * 0.1f));

    // Draw gizmo based on mode
    if (mode == GizmoMode::MOVE) {
        drawMoveGizmo(dl, sx, sy, scale);
    } else if (mode == GizmoMode::ROTATE) {
        // Simple circle gizmo
        float radius = 30.0f * scale;
        dl->AddCircle(ImVec2(sx, sy), radius, IM_COL32(255, 255, 100, 200), 32, 2.0f);
    } else if (mode == GizmoMode::SCALE) {
        // Simple box gizmo
        float sz = 8.0f * scale;
        dl->AddRect(ImVec2(sx - sz, sy - sz), ImVec2(sx + sz, sy + sz), IM_COL32(255, 150, 50, 200), 0, 0, 2.0f);
    }

    // Handle mouse interaction
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    float mdx = mouse_pos.x - sx;
    float mdy = mouse_pos.y - sy;
    float mdist = std::sqrt(mdx*mdx + mdy*mdy);

    if (mode == GizmoMode::MOVE) {
        // Check if near axis
        float axis_threshold = 15.0f * scale;
        int hovered_axis = -1;

        // X axis check (horizontal from center)
        if (std::abs(mdy) < axis_threshold && mdx > 0 && mdx < 50.0f * scale) {
            hovered_axis = 0;
        }
        // Y axis check (vertical from center)
        else if (std::abs(mdx) < axis_threshold && mdy < 0 && -mdy < 50.0f * scale) {
            hovered_axis = 1;
        }
        // Z axis check (diagonal)
        else if (mdist < axis_threshold) {
            hovered_axis = 2;
        }

        // Start drag
        if (hovered_axis >= 0 && ImGui::IsMouseClicked(0) && !dragging) {
            dragging = true;
            active_axis = hovered_axis;
            drag_start[0] = mouse_pos.x;
            drag_start[1] = mouse_pos.y;
        }
    }

    // Continue drag
    if (dragging && ImGui::IsMouseDown(0)) {
        float dx = mouse_pos.x - drag_start[0];
        float dy = mouse_pos.y - drag_start[1];
        handleDrag(sn, rn, active_axis, dx, dy);
        drag_start[0] = mouse_pos.x;
        drag_start[1] = mouse_pos.y;
        graph.markDirty(sel);
    }

    // End drag
    if (dragging && ImGui::IsMouseReleased(0)) {
        dragging = false;
        active_axis = -1;
    }
}

} // namespace mg
