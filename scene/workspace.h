#pragma once
#include "../os/os.h"
#include "../render/scene.h"
#include <vector>

namespace mg {
namespace scene {

// ============================================================================
// Viewport — a rendering view into the scene
// ============================================================================
struct Viewport {
    int  x = 0, y = 0;       // position in window
    int  w = 800, h = 600;   // size in pixels
    bool visible = true;

    u32 camera_node = 0xFFFFFFFF; // SceneNode index in SceneGraph
    // or use render::Scene::camera directly

    f32 grid_opacity    = 0.0f;
    bool show_gizmo     = false;
    bool wireframe      = false;
    bool show_aabbs     = false;
    bool show_bvh       = false;
};

// ============================================================================
// Layer — visibility and override controls per node group
// ============================================================================
struct Layer {
    std::string name;
    bool        visible = true;
    bool        solo    = false;  // isolate this layer
    f32         opacity = 1.0f;

    // Node indices belonging to this layer
    std::vector<u32> node_indices;
};

// ============================================================================
// Timeline — animation playback state
// ============================================================================
struct Timeline {
    f32  current_time   = 0.0f;
    u32  current_frame  = 0;
    u32  total_frames   = 1;
    bool playing        = false;
    bool loop           = true;
    f32  fps            = 30.0f;
    f32  playback_speed = 1.0f;

    // Frame range
    u32  start_frame = 0;
    u32  end_frame   = 0;

    void update(f32 dt) {
        if (!playing) return;
        f32 frame_dt = dt * playback_speed;
        current_time += frame_dt;
        f32 frame_f = current_time * fps;
        current_frame = (u32)frame_f;

        if (loop && end_frame > start_frame) {
            if (current_frame > end_frame) {
                current_frame = start_frame;
                current_time  = (f32)start_frame / fps;
            }
        }
    }

    void gotoFrame(u32 frame) {
        current_frame = frame;
        current_time  = (f32)frame / fps;
    }

    f32 getW() const { return current_time; }
};

// ============================================================================
// Workspace — aggregates viewports, layers, timeline
// ============================================================================
struct Workspace {
    std::vector<Viewport> viewports;
    std::vector<Layer>    layers;
    Timeline              timeline;

    // Active viewport index (for input routing)
    u32 active_viewport = 0;

    void init() {
        viewports.clear();
        layers.clear();
        Viewport def;
        def.x = def.y = 0;
        def.w = 800;
        def.h = 600;
        def.camera_node = 0;
        viewports.push_back(def);
    }

    Viewport& active() {
        if (active_viewport >= viewports.size())
            active_viewport = 0;
        return viewports[active_viewport];
    }

    // Update layer visibility based on node indices
    void applyLayers(std::vector<bool>& node_visibility, u32 node_count) const {
        node_visibility.assign(node_count, true);
        bool any_solo = false;
        for (const auto& layer : layers)
            if (layer.solo) any_solo = true;

        for (const auto& layer : layers) {
            bool show = layer.visible;
            if (any_solo) show = layer.solo;
            if (!show) {
                for (u32 idx : layer.node_indices)
                    if (idx < node_count)
                        node_visibility[idx] = false;
            }
        }
    }
};

} // namespace scene
} // namespace mg
