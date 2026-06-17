#pragma once
#include "../render/scene.h"
#include "../render/render.h"
#include "../scene/scene_graph.h"
#include "../scene/scene_query.h"
#include "../scene/camera.h"

namespace mg {

struct SceneManager {
    scene::SceneGraph graph;
    scene::Bvh bvh;
    std::vector<Aabb> world_aabbs;

    void loadScene(Renderer& renderer, const wchar_t* path, scene::CameraController& cam_ctrl, int vp_w, int vp_h);
    void rebuildScene(Renderer& renderer);
    void rebuildAabbs(Renderer& renderer);
    
    // Callback para el renderer
    static u32 bvhQuery(void* ctx, const Vec3& ro, const Vec3& rd, u32* out, u32 max);
    
    SceneQuery getQuery();
};

}
