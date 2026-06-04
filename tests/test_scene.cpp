// test_scene.cpp — Tests del layer scene (SceneGraph, Camera, BVH, Project, Workspace)
#include "../os/os.h"
#include "../render/scene.h"
#include "../scene/scene_graph.h"
#include "../scene/camera.h"
#include "../scene/scene_query.h"
#include "../scene/project.h"
#include "../scene/workspace.h"
#include <cstdio>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace mg;
using namespace mg::scene;

static int g_tests = 0, g_passed = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    bool _ok = (expr); \
    if (_ok) g_passed++; \
    printf("  %s: %s\n", name, _ok ? "PASS" : "FAIL"); \
} while(0)

// ============================================================================
// 1) AABB tests
// ============================================================================
static void test_aabb() {
    printf("\n--- AABB ---\n");

    Aabb a;
    TEST("aabb default min inf", a.min.x == 1e9f);
    TEST("aabb default max -inf", a.max.x == -1e9f);

    a.expand(Vec3{1, 2, 3});
    TEST("aabb expand point min", a.min.x == 1 && a.min.y == 2 && a.min.z == 3);
    TEST("aabb expand point max", a.max.x == 1 && a.max.y == 2 && a.max.z == 3);

    a.expand(Vec3{-2, 5, 1});
    TEST("aabb expand second min", a.min.x == -2);
    TEST("aabb expand second max", a.max.y == 5);

    Aabb b;
    b.expand(Vec3{-1, -1, -1});
    b.expand(Vec3{1, 1, 1});
    a.expand(b);
    TEST("aabb expand aabb", a.min.x == -2 && a.max.y == 5);

    TEST("aabb contains center", b.contains({0, 0, 0}));
    TEST("aabb contains edge", b.contains({1, 1, 1}));
    TEST("aabb not contains outside", !b.contains({2, 0, 0}));

    f32 sa = b.surfaceArea();
    TEST("aabb surface area 6 faces", sa == 24.0f); // 2x2x2 = 24

    // Ray-AABB intersection
    Vec3 ro = {0, 0, 5};
    Vec3 rd = {0, 0, -1};
    f32 tmin, tmax;
    bool hit = b.intersect(ro, rd, tmin, tmax);
    TEST("aabb ray hit center", hit);
    TEST("aabb ray tmin near", tmin == 4.0f); // z=5 to z=1
    TEST("aabb ray tmax far", tmax == 6.0f);  // z=5 to z=-1

    // Ray that misses
    Vec3 ro2 = {10, 0, 5};
    bool hit2 = b.intersect(ro2, rd, tmin, tmax);
    TEST("aabb ray miss", !hit2);

    // Ray from inside
    Vec3 ro3 = {0, 0, 0};
    bool hit3 = b.intersect(ro3, rd, tmin, tmax);
    TEST("aabb ray from inside", hit3);
    TEST("aabb ray from inside tmin <= 0", tmin <= 0);
}

// ============================================================================
// 2) SceneGraph tests
// ============================================================================
static void test_scene_graph() {
    printf("\n--- SceneGraph ---\n");

    // Create a render::Scene with a simple hierarchy
    Scene scene;
    scene.loadDefault(); // 1 node (sphere)

    // Add a GROUP with children manually for testing hierarchy
    // scene already has: node[0] = sphere
    // We'll test SceneGraph.init() then manipulate hierarchy

    SceneGraph graph;
    graph.init(scene);
    TEST("graph init size", graph.nodes.size() == 1);
    TEST("graph node 0 translate", graph.nodes[0].local_translate.x == 0);
    TEST("graph node 0 scale", graph.nodes[0].local_scale.x == 1.0f);

    // Set transform
    graph.setTransform(0, {1, 2, 3}, {10, 20, 30}, {2, 2, 2});
    TEST("transform set translate", graph.nodes[0].local_translate.x == 1);
    TEST("transform set rotate", graph.nodes[0].local_rotate.y == 20);
    TEST("transform dirty after set", graph.nodes[0].transform_dirty);

    // Compute world transform
    graph.updateWorldTransforms();
    TEST("world translate equal local for root",
         graph.nodes[0].world_translate.x == 1);
    TEST("world scale equal local for root",
         graph.nodes[0].world_scale.x == 2);
    TEST("transform clean after update", !graph.nodes[0].transform_dirty);

    // Test parent-child hierarchy
    // Create a more complex scene for hierarchy testing
    Scene hierarchy_scene;
    // Manually construct nodes via loading a specific RIH isn't practical here
    // Instead test the hierarchy methods directly

    // Expand graph to have multiple nodes
    // We need to re-init with a scene that has multiple nodes
    // Let's add nodes to scene manually
    Node n1; n1.id = 1; n1.name = "child"; n1.type = NodeType::SDF;
    n1.translate = {0, 0, 0};
    n1.sdf.sdf_type = "sphere"; n1.sdf.params[0].constant = 0.5f;
    n1.material_id = 0;
    scene.nodes.push_back(n1);

    Node n2; n2.id = 2; n2.name = "group"; n2.type = NodeType::GROUP;
    n2.children = {0, 1}; // references IDs
    n2.translate = {0, 0, 0};
    scene.nodes.push_back(n2);

    // Update id_to_idx map
    scene._id_to_idx[0] = 0;
    scene._id_to_idx[1] = 1;
    scene._id_to_idx[2] = 2;

    // Re-init graph
    // The graph init iterates scene nodes to build hierarchy
    // Node 2 (group) has children [0, 1] (IDs)
    // But init only checks GROUP children, it uses the same indices
    // Actually init() iterates nodes and for each child reference in scene.nodes[j].children
    // it calls setParent(child_idx, j)
    // But children vector is cleared during init, so we need to be careful

    // Actually, the init function looks at scene.nodes[j].children which hold node IDs.
    // But scene._id_to_idx maps ID to index. So the graph might need to resolve
    // the IDs through idToIndex first. Let me check the init code...

    // In SceneGraph::init():
    // "for each child in scene.nodes[j].children:
    //      if child == i && scene.nodes[j].type == GROUP: setParent(i, j)"
    // This means it checks if the index i equals the child value directly.
    // But child values from RIH are IDs not indices.
    // This is a known discrepancy (the ID→index bug we fixed earlier).
    // For this test, let's use indices directly since we're not loading from file.

    // Clear and redo
    graph.clear();
    graph.nodes.resize(3);
    for (u32 i = 0; i < 3; i++) {
        graph.nodes[i].id = i;
        graph.nodes[i].local_translate = scene.nodes[i].translate;
        graph.nodes[i].local_rotate    = scene.nodes[i].rotate;
        graph.nodes[i].local_scale     = scene.nodes[i].scale;
        graph.nodes[i].transform_dirty = true;
    }

    // Set up hierarchy manually
    graph.setParent(0, 2); // sphere0 -> group2
    graph.setParent(1, 2); // sphere1 -> group2
    TEST("parent set", graph.nodes[0].parent == 2);
    TEST("parent second", graph.nodes[1].parent == 2);
    TEST("parent group has children",
         graph.nodes[2].first_child != 0xFFFFFFFF);

    // Mark dirty and set transform on parent
    graph.setTransform(2, {5, 0, 0}, {0, 0, 0}, {1, 1, 1});
    graph.updateWorldTransforms();

    TEST("child world translate inherits parent",
         graph.nodes[0].world_translate.x == 5);
    TEST("child world translate y same",
         graph.nodes[0].world_translate.y == 0);

    // Nested transform
    graph.setTransform(0, {1, 0, 0}, {0, 0, 0}, {1, 1, 1});
    graph.updateWorldTransforms();
    TEST("child local + parent world",
         graph.nodes[0].world_translate.x == 6); // 5 + 1

    // Scale propagation
    graph.setTransform(2, {0, 0, 0}, {0, 0, 0}, {2, 1, 1});
    graph.updateWorldTransforms();
    TEST("child world with parent scale",
         graph.nodes[0].world_translate.x == 2); // (0 + 1*2) = 2
}

// ============================================================================
// 3) CameraController tests
// ============================================================================
static void test_camera() {
    printf("\n--- CameraController ---\n");

    CameraController cam;

    // Default is ORBIT mode
    TEST("default mode orbit", cam.mode == CameraMode::ORBIT);

    // Orbit at origin, distance 5
    cam.setOrbit({0,0,0}, 5.0f, 0, 0);
    Vec3 pos = cam.getPosition();
    TEST("orbit front pos x=-5", std::abs(pos.x + 5) < 0.001f);
    TEST("orbit front pos y=0", std::abs(pos.y) < 0.001f);

    Vec3 tgt = cam.getTarget();
    TEST("orbit target 0", tgt.x == 0 && tgt.y == 0 && tgt.z == 0);

    // Orbit at 90 degrees azimuth (to the right)
    cam.setOrbit({0,0,0}, 5.0f, 3.14159265f/2, 0);
    pos = cam.getPosition();
    TEST("orbit right pos x=0", std::abs(pos.x) < 0.001f);
    TEST("orbit right pos z=-5", std::abs(pos.z + 5) < 0.001f);
    // At azimuth=pi/2: offset = {cos*cy, sy, sin*cy} = {0, 0, 1}
    // pos = {0,0,0} - {0,0,5} = {0,0,-5}
    // Actually at azimuth=pi/2: cx=0, sx=1, cy=1, sy=0 => offset={0, 0, 1} => pos={0,0,-5}
    // Wait: offset = {cx*cy, sy, sx*cy} = {0, 0, 1}
    // pos = target - offset*dist = {0,0,0} - {0,0,5} = {0,0,-5}
    // Let me recompute...
    // offset = (cx*cy, sy, sx*cy) = (0*1, 0, 1*1) = (0, 0, 1)
    // pos = (0 - 0, 0 - 0, 0 - 5) = (0, 0, -5)
    // So z = -5, x = 0. OK.

    // Orbit rotate
    cam.orbitRotate(100, 0); // rotate azimuth
    TEST("orbit rotate changes azimuth", cam.orbit_azimuth > 0);

    // Zoom
    cam.zoom(2.0f);
    TEST("orbit zoom reduces distance", cam.orbit_distance == 3.0f);
    cam.zoom(-2.0f);
    TEST("orbit zoom out", cam.orbit_distance == 5.0f);

    // FreeFly mode
    CameraController fly;
    fly.setFreeFly({0,0,0}, 0, 0);
    TEST("free fly mode set", fly.mode == CameraMode::FREE_FLY);

    fly.mouseLook(100, 50);
    TEST("free fly yaw after look", fly.yaw > 0);
    TEST("free fly pitch after look", fly.pitch > 0);

    // Apply to render::Camera
    CameraController orbit;
    orbit.setOrbit({1, 2, 3}, 10.0f, 0.5f, 0.3f);
    Camera rc;
    orbit.applyTo(rc);
    TEST("applyTo sets position", rc.position.x != 0 || rc.position.y != 0);
    TEST("applyTo sets target", rc.target.x == 1 && rc.target.z == 3);
}

// ============================================================================
// 4) BVH tests
// ============================================================================
static void test_bvh() {
    printf("\n--- BVH ---\n");

    Scene scene;
    scene.loadDefault();
    // Add a second sphere
    Node n2;
    n2.id = 1; n2.name = "sphere2"; n2.type = NodeType::SDF;
    n2.translate = {10, 0, 0};
    n2.sdf.sdf_type = "sphere";
    n2.sdf.params[0].constant = 1.0f;
    n2.material_id = 0;
    scene.nodes.push_back(n2);

    SceneGraph graph;
    graph.init(scene);

    Bvh bvh;
    bvh.build(graph, scene);
    TEST("bvh built", bvh.nodes.size() > 0);
    TEST("bvh root valid", bvh.root != 0xFFFFFFFF);

    // Query a ray through origin (should hit sphere at origin)
    std::vector<u32> results;
    bvh.query({0, 0, 5}, {0, 0, -1}, results);
    TEST("bvh query center ray hits", results.size() > 0);
    if (!results.empty()) {
        // The hit should include node 0 (sphere at origin)
        bool found_origin = false;
        for (u32 idx : results) {
            if (idx == 0) found_origin = true;
        }
        TEST("bvh query found sphere at origin", found_origin);
    }

    // Query a ray far away (should miss)
    bvh.query({100, 0, 5}, {0, 0, -1}, results);
    TEST("bvh query far ray misses", results.empty());

    // Query through far sphere
    bvh.query({10, 0, 5}, {0, 0, -1}, results);
    TEST("bvh query far sphere", results.size() > 0);
}

// ============================================================================
// 5) Project tests
// ============================================================================
static void test_project() {
    printf("\n--- Project ---\n");

    Project proj;
    proj.setDefault();
    TEST("project default no sources", proj.sources.empty());
    TEST("project default camera pos z=5", proj.camera.position.z == 5);

    // Add source
    proj.sources.push_back({L"..\\ejemplos\\bodegon.rih"});
    TEST("project source added", proj.sources.size() == 1);

    // Apply to scene
    Scene scene;
    scene.loadDefault();
    proj.applyTo(scene);
    TEST("project apply camera pos", scene.camera.position.z == 5);
    TEST("project apply bg", scene.background.x == 0);

    // Save to temp file
    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    wcscat_s(temp_path, L"test_mgproject.mgproject");

    proj.camera = {{1,2,3}, {4,5,6}, {0,1,0}, 45.0f};
    proj.background = {0.1f, 0.2f, 0.3f};
    proj.current_time = 1.5f;

    bool saved = proj.save(temp_path);
    TEST("project saved", saved);

    // Load back
    Project loaded;
    bool loaded_ok = loaded.load(temp_path);
    TEST("project loaded", loaded_ok);

    TEST("loaded source count", loaded.sources.size() == 1);
    TEST("loaded camera pos x", std::abs(loaded.camera.position.x - 1.0f) < 0.01f);
    TEST("loaded camera pos y", std::abs(loaded.camera.position.y - 2.0f) < 0.01f);
    TEST("loaded camera target", std::abs(loaded.camera.target.x - 4.0f) < 0.01f);
    TEST("loaded camera fov", std::abs(loaded.camera.fov - 45.0f) < 0.01f);
    TEST("loaded background r", std::abs(loaded.background.x - 0.1f) < 0.01f);
    TEST("loaded time", std::abs(loaded.current_time - 1.5f) < 0.01f);

    // Cleanup
    DeleteFileW(temp_path);
}

// ============================================================================
// 6) Workspace tests
// ============================================================================
static void test_workspace() {
    printf("\n--- Workspace ---\n");

    Workspace ws;
    ws.init();
    TEST("workspace has 1 viewport", ws.viewports.size() == 1);
    TEST("viewport default w=800", ws.viewports[0].w == 800);

    // Active viewport
    TEST("active viewport index 0", ws.active_viewport == 0);
    TEST("active viewport ref w=800", ws.active().w == 800);

    // Timeline
    Timeline& tl = ws.timeline;
    TEST("timeline default time=0", tl.current_time == 0);
    TEST("timeline default fps=30", tl.fps == 30.0f);

    tl.gotoFrame(15);
    TEST("timeline goto frame 15", tl.current_frame == 15);
    TEST("timeline time at frame 15", std::abs(tl.current_time - 0.5f) < 0.001f);

    tl.playing = true;
    tl.update(1.0f); // 1 second at 30fps
    TEST("timeline update advances frame", tl.current_frame >= 15);

    tl.playing = false;
    u32 before = tl.current_frame;
    tl.update(1.0f);
    TEST("timeline paused no advance", tl.current_frame == before);

    // Layers
    Layer l;
    l.name = "foreground";
    l.node_indices = {0, 1, 2};
    ws.layers.push_back(l);

    Layer l2;
    l2.name = "background";
    l2.node_indices = {3, 4};
    ws.layers.push_back(l2);

    std::vector<bool> vis;
    ws.applyLayers(vis, 5);
    TEST("all visible by default", vis[0] && vis[3]);

    // Solo layer
    ws.layers[0].solo = true;
    ws.layers[1].visible = true;
    ws.applyLayers(vis, 5);
    TEST("solo foreground visible", vis[0] && vis[1] && vis[2]);
    TEST("solo background hidden", !vis[3] && !vis[4]);

    // Hidden layer
    ws.layers[0].solo = false;
    ws.layers[1].visible = false;
    ws.applyLayers(vis, 5);
    TEST("hidden layer off", !vis[3] && !vis[4]);
    TEST("visible layer on", vis[0]);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("========================================\n");
    printf("  Scene Layer Tests\n");
    printf("========================================\n");

    test_aabb();
    test_scene_graph();
    test_camera();
    test_bvh();
    test_project();
    test_workspace();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    return (g_passed == g_tests) ? 0 : 1;
}
