#pragma once
#include <cstdint>
#include "../scene/scene_graph.h"
#include "../scene/camera.h"
#include "../render/scene.h"
#include "ontology_panel.h"
#include "imgui.h"

namespace mg {

enum class GizmoMode : uint8_t {
    NONE = 0,
    MOVE = 1,
    ROTATE = 2,
    SCALE = 3
};

class GizmosPanel {
public:
    bool visible = true;
    GizmoMode mode = GizmoMode::MOVE;
    bool snap_enabled = false;
    float snap_value = 0.5f;

    // Dragging state
    bool dragging = false;
    int active_axis = -1; // 0=X, 1=Y, 2=Z
    float drag_start[3] = {0, 0, 0};

    void draw(OntologyPanel& ontology, scene::SceneGraph& graph,
              Scene& scene, scene::CameraController& cam,
              uint32_t viewport_w, uint32_t viewport_h);

private:
    bool worldToScreen(Vec3 world, Vec3 cam_pos, Vec3 cam_tgt,
                       uint32_t vw, uint32_t vh, float& sx, float& sy);
    void drawMoveGizmo(ImDrawList* dl, float cx, float cy, float scale);
    void handleDrag(scene::SceneNode& sn, Node& rn, int axis, float dx, float dy);
};

} // namespace mg
