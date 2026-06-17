#include "render/scene.h"
#include "render/render.h"
#include "scene/scene_graph.h"
#include "scene/scene_query.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>
using namespace mg;

int main() {
    wchar_t abs[512];
    GetFullPathNameW(L"..\\Lenguaje Hermetico\\ejemplos\\bodegon.rih", 512, abs, nullptr);
    wprintf(L"Loading: %s\n", abs);

    mg::FileMapping fm;
    if (!fm.open(abs)) { printf("FAIL: open\n"); return 1; }

    mg::Renderer renderer;
    renderer.load(fm);
    fm.close();

    printf("Scene: %d nodes\n", (int)renderer.scene.nodes.size());

    // Build scene graph + BVH (same as visor)
    scene::SceneGraph graph;
    graph.init(renderer.scene);
    graph.updateWorldTransforms();

    // Compute AABBs
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++)
        graph.computeLocalAabb(i, renderer.scene);
    graph.updateWorldTransforms();
    for (u32 i = 0; i < (u32)graph.nodes.size(); i++)
        graph.getWorldAabb(i);

    scene::Bvh bvh;
    bvh.build(graph, renderer.scene);
    printf("BVH nodes: %d\n", (int)bvh.nodes.size());
    printf("BVH root bounds: (%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
        bvh.nodes[bvh.root].bounds.min.x, bvh.nodes[bvh.root].bounds.min.y, bvh.nodes[bvh.root].bounds.min.z,
        bvh.nodes[bvh.root].bounds.max.x, bvh.nodes[bvh.root].bounds.max.y, bvh.nodes[bvh.root].bounds.max.z);

    // Query from camera position toward center
    Vec3 ro = {0, 1.5f, 5};
    Vec3 rd = normalize(Vec3{0, 1, 0} - ro);
    std::vector<u32> visible;
    bvh.query(ro, rd, visible);
    printf("Visible nodes from camera: ");
    for (u32 v : visible) printf("%d ", v);
    printf(" (%d total)\n", (int)visible.size());

    // Also query from slightly different direction
    rd = normalize(Vec3{-0.5f, 1.1f, 0} - ro);
    bvh.query(ro, rd, visible);
    printf("Visible nodes toward jarron: ");
    for (u32 v : visible) printf("%d ", v);
    printf(" (%d total)\n", (int)visible.size());

    printf("DONE\n");
    return 0;
}
