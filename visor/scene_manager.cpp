#include "scene_manager.h"
#include "../os/os.h"

namespace mg {

u32 SceneManager::bvhQuery(void* ctx, const Vec3& ro, const Vec3& rd, u32* out, u32 max) {
    SceneManager* self = reinterpret_cast<SceneManager*>(ctx);
    if (!self || self->bvh.nodes.empty()) return 0;
    static std::vector<u32> tmp;
    self->bvh.query(ro, rd, tmp);
    u32 n = (u32)tmp.size() < max ? (u32)tmp.size() : max;
    for (u32 i = 0; i < n; i++) out[i] = tmp[i];
    return n;
}

SceneQuery SceneManager::getQuery() {
    SceneQuery sq;
    sq.aabbs = world_aabbs.data();
    sq.query_fn = bvhQuery;
    sq.query_ctx = this;  // el campo correcto segun ray_march.h:125
    return sq;
}

void SceneManager::rebuildAabbs(Renderer& renderer) {
    world_aabbs.resize(graph.nodes.size());
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++) {
        graph.computeLocalAabb(i, renderer.scene);
    }
    graph.updateWorldTransforms();
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++) {
        world_aabbs[i] = graph.getWorldAabb(i);
    }
}

void SceneManager::rebuildScene(Renderer& renderer) {
    graph.init(renderer.scene);
    rebuildAabbs(renderer);
    bool has_sdf_leaves = false;
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++) {
        if (renderer.scene.nodes[i].type == NodeType::SDF && !renderer.scene.nodes[i].is_compound_child) {
            has_sdf_leaves = true; break;
        }
    }
    if (has_sdf_leaves) bvh.build(graph, renderer.scene);
}

void SceneManager::loadScene(Renderer& renderer, const wchar_t* path, scene::CameraController& cam_ctrl, int vp_w, int vp_h) {
    FileMapping fm;
    if (!fm.open(path)) {
        renderer.scene.loadDefault();
        return;
    }
    renderer.load(fm);
    fm.close();

    graph.init(renderer.scene);
    graph.updateWorldTransforms();

    bool has_sdf_leaves = false;
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++) {
        if (renderer.scene.nodes[i].type == NodeType::SDF && !renderer.scene.nodes[i].is_compound_child) {
            has_sdf_leaves = true;
            break;
        }
    }
    rebuildAabbs(renderer);
    if (has_sdf_leaves) {
        bvh.build(graph, renderer.scene);
    }

    cam_ctrl.setOrbit(renderer.scene.camera.target, 5.0f, 0, 0.5f);
}

}
