// test_render.cpp — Tests del pipeline de renderizado
#include "../os/os.h"
#include "../render/scene.h"
#include "../render/sdf_eval.h"
#include "../render/ray_march.h"
#include "../render/render.h"
#include <cstdio>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace mg;

static int g_tests = 0, g_passed = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    bool _ok = (expr); \
    if (_ok) g_passed++; \
    printf("  %s: %s\n", name, _ok ? "PASS" : "FAIL"); \
} while(0)

// ============================================================================
// 1) Expression evaluator tests
// ============================================================================
static void test_expr_eval() {
    printf("\n--- Expression evaluator ---\n");

    // Constants
    TEST("pi", mg::evalExpr("pi", 0,0,0,0) > 3.14f);
    TEST("e",  mg::evalExpr("e", 0,0,0,0) > 2.71f);

    // Variables
    TEST("x", mg::evalExpr("x", 1.5f,0,0,0) == 1.5f);
    TEST("y", mg::evalExpr("y", 0,2.5f,0,0) == 2.5f);
    TEST("z", mg::evalExpr("z", 0,0,3.5f,0) == 3.5f);
    TEST("w", mg::evalExpr("w", 0,0,0,4.5f) == 4.5f);
    TEST("t alias", mg::evalExpr("t", 0,0,0,9.0f) == 9.0f);

    // Arithmetic
    TEST("1+2", mg::evalExpr("1+2", 0,0,0,0) == 3.0f);
    TEST("3*4", mg::evalExpr("3*4", 0,0,0,0) == 12.0f);
    TEST("10/2", mg::evalExpr("10/2", 0,0,0,0) == 5.0f);
    TEST("2^3", mg::evalExpr("2^3", 0,0,0,0) == 8.0f);
    TEST("parens", mg::evalExpr("(1+2)*3", 0,0,0,0) == 9.0f);
    TEST("unary minus", mg::evalExpr("-5+3", 0,0,0,0) == -2.0f);

    // Functions
    TEST("abs(-3)", mg::evalExpr("abs(-3)", 0,0,0,0) == 3.0f);
    TEST("sin(0)", mg::evalExpr("sin(0)", 0,0,0,0) == 0.0f);
    TEST("cos(0)", mg::evalExpr("cos(0)", 0,0,0,0) == 1.0f);
    TEST("sqrt(9)", mg::evalExpr("sqrt(9)", 0,0,0,0) == 3.0f);
    TEST("max(3,7)", mg::evalExpr("max(3,7)", 0,0,0,0) == 7.0f);
    TEST("min(3,7)", mg::evalExpr("min(3,7)", 0,0,0,0) == 3.0f);
    TEST("clamp", mg::evalExpr("clamp(0,1,0.5)", 0,0,0,0) == 0.5f);
    TEST("clamp low", mg::evalExpr("clamp(0,1,-1)", 0,0,0,0) == 0.0f);
    TEST("clamp high", mg::evalExpr("clamp(0,1,2)", 0,0,0,0) == 1.0f);
    TEST("lerp", mg::evalExpr("lerp(0,10,0.5)", 0,0,0,0) == 5.0f);
    TEST("mix", mg::evalExpr("mix(0,10,0.5)", 0,0,0,0) == 5.0f);
    TEST("length", mg::evalExpr("length(3,4,0)", 0,0,0,0) == 5.0f);
    TEST("pow(2,3)", mg::evalExpr("pow(2,3)", 0,0,0,0) == 8.0f);

    // Expressions with variables (for SDF params)
    TEST("x*0.5", mg::evalExpr("x*0.5", 2.0f,0,0,0) == 1.0f);
    TEST("1.5*0.5", mg::evalExpr("1.5*0.5", 0,0,0,0) == 0.75f);
    TEST("0.65+sin(w)*0.35", mg::evalExpr("0.65+sin(w)*0.35", 0,0,0,1.5708f) > 0.95f); // sin(pi/2)=1 → 1.0
}

// ============================================================================
// 2) SDF primitive tests
// ============================================================================
static void test_sdf_primitives() {
    printf("\n--- SDF primitives ---\n");

//using namespace mg;

    // Sphere at origin, r=1
    TEST("sphere center", sdSphere({0,0,0}, 1.0f) == -1.0f);
    TEST("sphere surface", std::abs(sdSphere({1,0,0}, 1.0f)) < 0.001f);
    TEST("sphere outside", sdSphere({3,0,0}, 1.0f) == 2.0f);

    // Box centered, size 2x2x2
    TEST("box center", sdBox({0,0,0}, {1,1,1}) == -1.0f);
    TEST("box surface", std::abs(sdBox({1,0.5f,0}, {1,1,1})) < 0.001f);
    TEST("box outside", sdBox({2,0,0}, {1,1,1}) == 1.0f);

    // Cylinder r=0.5, h=1
    TEST("cylinder center", sdCylinder({0,0,0}, 0.5f, 1.0f) == -0.5f);
    TEST("cylinder surface r", std::abs(sdCylinder({0.5f,0,0}, 0.5f, 1.0f)) < 0.001f);
    TEST("cylinder outside r", sdCylinder({1,0,0}, 0.5f, 1.0f) == 0.5f);

    // Torus R=1, r=0.25
    TEST("torus center", sdTorus({0,0,0}, 1.0f, 0.25f) > 0.74f); // distance to ring
    // At ring surface
    f32 d_torus = sdTorus({1.25f,0,0}, 1.0f, 0.25f);
    TEST("torus surface", std::abs(d_torus) < 0.01f);

    // Boolean ops
    TEST("union", opUnion(1, 2) == 1);
    TEST("subtract", opSubtract(1, 2) == 1); // max(1, -2) = 1
    TEST("intersect", opIntersect(1, 2) == 2);
    TEST("smooth union boundary", opSmoothUnion(1, 1, 0.5f) < 1.0f);
}

// ============================================================================
// 3) Scene loading tests
// ============================================================================
static void test_scene_loading() {
    printf("\n--- Scene loading ---\n");

    mg::Scene scene;

    // Default scene (fallback sphere)
    scene.loadDefault();
    TEST("default scene nodes", scene.nodes.size() == 1);
    TEST("default scene mats", scene.materials.size() == 1);
    TEST("default scene lights", scene.lights.size() == 1);
    TEST("default sphere name", scene.nodes[0].sdf.sdf_type == "sphere");
    TEST("default sphere r", scene.nodes[0].sdf.params[0].constant == 1.0f);

    // Load bodegon.rih via FileMapping
    WCHAR exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    WCHAR* p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    WCHAR full[MAX_PATH];
    wsprintfW(full, L"%s\\Lenguaje Hermetico\\ejemplos\\bodegon.rih", exe_path);
    // Try LENGUA~1 as fallback
    mg::FileMapping fm;
    bool opened = fm.open(full);
    if (!opened) {
        wsprintfW(full, L"%s\\LENGUA~1\\ejemplos\\bodegon.rih", exe_path);
        opened = fm.open(full);
    }
    TEST("bodegon.rih opened", opened);
    if (!opened) return;

    mg::Scene scene2;
    bool loaded = scene2.load(fm);
    fm.close();
    TEST("scene loaded", loaded);
    if (!loaded) return;

    // Check scene data
    TEST("scene has nodes", scene2.nodes.size() > 0);
    TEST("scene has materials", scene2.materials.size() > 0);

    // bodegon.rih has:
    // - 3 materials (madera, cobre, vidrio)
    // - 8+ nodes (tablero, 4 patas, mesa group, jarron compound, esfera)
    TEST("materials==3", scene2.materials.size() == 3);
    TEST("nodes >= 8", scene2.nodes.size() >= 8);
    TEST("has tablero", scene2.nodes[0].name == "tablero");
    // Check material properties
    TEST("madera roughness", scene2.materials[0].roughness > 0.8f);
    TEST("cobre metallic", scene2.materials[1].metallic == 1.0f);

    // stats
    char buf[256];
    int n = scene2.stats(buf, sizeof(buf));
    TEST("stats written", n > 0);
    TEST("stats contains nodes", strstr(buf, "nodes=") != nullptr);
    printf("  stats: %s\n", buf);
}

// ============================================================================
// 4) SDF tree evaluation tests
// ============================================================================
static void test_sdf_eval() {
    printf("\n--- SDF tree evaluation ---\n");

    // Use default scene (one sphere at origin, r=1)
    mg::Scene scene;
    scene.loadDefault();
    mg::Arena arena;

    // At center of sphere
    f32 d_center = mg::evalScene(scene, {0,0,0}, 0);
    TEST("eval at center", d_center < 0);
    // At surface
    f32 d_surface = mg::evalScene(scene, {1,0,0}, 0);
    TEST("eval at surface", std::abs(d_surface) < 0.01f);
    // Outside
    f32 d_outside = mg::evalScene(scene, {3,0,0}, 0);
    TEST("eval outside", d_outside > 0);

    // findClosestNode
    u32 node = mg::findClosestNode(scene, {0,0,0}, 0);
    TEST("closest node found", node < scene.nodes.size());
    TEST("closest is sphere", scene.nodes[node].sdf.sdf_type == "sphere");

    // findMaterial
    u32 mat = mg::findMaterial(scene, {0,0,0}, 0);
    TEST("material found", mat < scene.materials.size());

    // calcNormal
    mg::Vec3 n = mg::calcNormal(scene, {1.0f, 0, 0}, 0);
    TEST("normal at +x", std::abs(n.x - 1.0f) < 0.01f);
    TEST("normal y≈0", std::abs(n.y) < 0.01f);

    n = mg::calcNormal(scene, {0, 1.0f, 0}, 0);
    TEST("normal at +y", std::abs(n.y - 1.0f) < 0.01f);

    // load bodegon and test scene-level eval
    WCHAR exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    WCHAR* p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    WCHAR full[MAX_PATH];
    wsprintfW(full, L"%s\\Lenguaje Hermetico\\ejemplos\\bodegon.rih", exe_path);
    mg::FileMapping fm;
    if (!fm.open(full)) {
        wsprintfW(full, L"%s\\LENGUA~1\\ejemplos\\bodegon.rih", exe_path);
        fm.open(full);
    }
    if (fm.data()) {
        mg::Scene bodegon;
        bodegon.load(fm);
        fm.close();

        // Test eval at a known point
        f32 d_table = mg::evalScene(bodegon, {0, 0.85f, 0}, 0);
        TEST("table surface", std::abs(d_table) < 0.05f);

        // Normal should point upward at table surface
        mg::Vec3 n_table = mg::calcNormal(bodegon, {0, 0.85f, 0}, 0);
        TEST("table normal up", n_table.y > 0.9f);

        // Far away point
        f32 d_far = mg::evalScene(bodegon, {0, 100, 0}, 0);
        TEST("far point", d_far > 50);
    }
}

// ============================================================================
// 5) Ray march tests
// ============================================================================
static void test_ray_march() {
    printf("\n--- Ray marching ---\n");

    // Use default scene (sphere at origin, r=1)
    mg::Scene scene;
    scene.loadDefault();
    scene.camera.position = {0, 0, 5};
    scene.camera.target = {0, 0, 0};

    mg::Ray ray;
    ray.origin = scene.camera.position;
    ray.dir = mg::getRayDir(scene.camera, 0, 0, 1, 1); // forward

    auto mr = mg::rayMarch(scene, ray, 0);
    TEST("hit sphere", mr.hit);
    TEST("hit distance > 0", mr.t > 0);
    TEST("hit distance < 10", mr.t < 10);
    TEST("hit material valid", mr.material < scene.materials.size());

    // Normal should point outward from sphere (toward camera in +z direction)
    TEST("normal z > 0 (facing camera)", mr.n.z > 0);

    // Ray that misses (shoot upward)
    mg::Ray up_ray;
    up_ray.origin = {0, 0, 5};
    up_ray.dir = mg::normalize({0, 1, -1}); // upward
    auto miss = mg::rayMarch(scene, up_ray, 0);
    TEST("miss scene", !miss.hit);

    // load bodegon and trace a ray to the table
    WCHAR exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    WCHAR* p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    WCHAR full[MAX_PATH];
    wsprintfW(full, L"%s\\Lenguaje Hermetico\\ejemplos\\bodegon.rih", exe_path);
    mg::FileMapping fm;
    if (!fm.open(full)) {
        wsprintfW(full, L"%s\\LENGUA~1\\ejemplos\\bodegon.rih", exe_path);
        fm.open(full);
    }
    if (fm.data()) {
        mg::Scene bodegon;
        bodegon.load(fm);
        bodegon.camera.position = {0, 1.5f, 5};
        bodegon.camera.target = {0, 1, 0};
        fm.close();

        mg::Ray bray;
        bray.origin = bodegon.camera.position;
        bray.dir = mg::getRayDir(bodegon.camera, 0, 0, 1, 1);
        auto bmr = mg::rayMarch(bodegon, bray, 0);
        TEST("bodegon hit", bmr.hit);
        if (bmr.hit) {
            TEST("bodegon hit dist<10", bmr.t < 10);
            u32 mat = bmr.material;
            TEST("bodegon hit material valid", mat < bodegon.materials.size());
        }
    }
}

// ============================================================================
// 6) Shading tests
// ============================================================================
static void test_shading() {
    printf("\n--- Shading ---\n");

    mg::Scene scene;
    scene.loadDefault();

    // Shade at hit point on sphere surface
    mg::Vec3 p = {1, 0, 0};
    mg::Vec3 n = mg::calcNormal(scene, p, 0);
    mg::Vec3 c = mg::shade(scene, p, n, 0, 0);

    TEST("shade r>0", c.x > 0);
    TEST("shade g>0", c.y > 0);
    TEST("shade b>0", c.z > 0);
    TEST("shade not too bright", c.x < 5 && c.y < 5 && c.z < 5);

    // Test directional light shading (no normalization issues)
    p = {0.707f, 0.707f, 0};
    n = mg::calcNormal(scene, p, 0);
    c = mg::shade(scene, p, n, 0, 0);
    TEST("shade at 45deg", c.x > 0 && c.y > 0);
}

// ============================================================================
// 7) AABB early-out + adaptive epsilon tests
// ============================================================================
static void test_optimizations() {
    printf("\n--- Optimizations ---\n");

    Scene scene;
    scene.loadDefault();
    scene.camera.position = {0, 0, 5};
    scene.camera.target = {0, 0, 0};

    // Test AABB early-out with aabb parameter
    // Create aabbs for all nodes (world AABB of default sphere)
    // Default scene has 1 sphere at origin, r=1, so AABB is [-1,-1,-1] to [1,1,1]
    // After transform: translate=(0,0,0), scale=(1,1,1), rotate=(0,0,0)
    Aabb sphere_aabb;
    sphere_aabb.min = {-1, -1, -1};
    sphere_aabb.max = {1, 1, 1};
    std::vector<Aabb> aabbs = {sphere_aabb};

    // evalScene at point far from AABB should early-out correctly
    Vec3 far_point = {100, 100, 100};
    f32 d_no_aabb = evalScene(scene, far_point, 0, nullptr, nullptr);
    f32 d_with_aabb = evalScene(scene, far_point, 0, nullptr, aabbs.data());
    TEST("aabb early-out same result far", std::abs(d_no_aabb - d_with_aabb) < 0.01f);

    // evalScene at point near AABB should give same result
    Vec3 near_point = {1.5f, 0, 0}; // just outside sphere
    f32 d_no2 = evalScene(scene, near_point, 0, nullptr, nullptr);
    f32 d_with2 = evalScene(scene, near_point, 0, nullptr, aabbs.data());
    TEST("aabb early-out same result near", std::abs(d_no2 - d_with2) < 0.01f);

    // calcNormal with AABBs should work
    Vec3 n_no = calcNormal(scene, {1,0,0}, 0, nullptr, nullptr);
    Vec3 n_yes = calcNormal(scene, {1,0,0}, 0, nullptr, aabbs.data());
    TEST("aabb normal same", std::abs(n_no.x - n_yes.x) < 0.01f);

    // Test rayMarch with AABBs
    Ray ray;
    ray.origin = {0, 0, 5};
    ray.dir = getRayDir(scene.camera, 0, 0, 1, 1);
    auto mr_no = rayMarch(scene, ray, 0, 50, 0.001f, 128, nullptr, nullptr);
    auto mr_yes = rayMarch(scene, ray, 0, 50, 0.001f, 128, nullptr, aabbs.data());
    TEST("aabb ray march hit same", mr_no.hit == mr_yes.hit);
    if (mr_no.hit && mr_yes.hit) {
        TEST("aabb ray march t same", std::abs(mr_no.t - mr_yes.t) < 0.01f);
    }

    // Test adaptive epsilon behavior
    // At distance 0, eps should be hit_eps
    // At distance 100, eps should be ~2x hit_eps
    f32 eps_near = 0.001f * (1.0f + 0.0f * 0.01f);  // = 0.001
    f32 eps_far  = 0.001f * (1.0f + 100.0f * 0.01f); // = 0.002
    TEST("adaptive eps near", std::abs(eps_near - 0.001f) < 0.0001f);
    TEST("adaptive eps far", std::abs(eps_far - 0.002f) < 0.0001f);

    // SceneQuery struct
    SceneQuery sq;
    TEST("default query null", sq.query_fn == nullptr);
    TEST("default aabbs null", sq.aabbs == nullptr);

    // renderScene with SceneQuery works (no BVH, just AABBs)
    u32 pixels[16*12] = {};
    Frame test_fb;
    Arena arena;
    arena.init(64 * 1024 * 1024);
    test_fb.init(arena, 16, 12);
    SceneQuery sq2;
    sq2.aabbs = aabbs.data();
    // Without BVH callback — should fall back to all nodes
    renderScene(scene, test_fb, 0, nullptr, sq2);
    TEST("scene query fb not null", test_fb.pixels != nullptr);
    u32 center = test_fb.pixels[(16/2) + (12/2)*16];
    TEST("scene query center pixel", center != 0);
    arena.shutdown();
}

// ============================================================================
// old #7 is now test_optimizations above
// ============================================================================
static void test_render() {
    printf("\n--- Render pipeline ---\n");

    mg::Renderer renderer;
    renderer.arena.init(64 * 1024 * 1024);

    // Use default scene (no file loaded)
    renderer.scene.loadDefault();
    renderer.scene.camera.position = {0, 0, 5};
    renderer.scene.camera.target = {0, 0, 0};

    // Pre-compute transforms
    renderer.transforms.resize(renderer.scene.nodes.size() * 3);
    for (u32 i = 0; i < renderer.scene.nodes.size(); i++) {
        renderer.transforms[i * 3 + 0] = renderer.scene.nodes[i].translate.x;
        renderer.transforms[i * 3 + 1] = renderer.scene.nodes[i].translate.y;
        renderer.transforms[i * 3 + 2] = renderer.scene.nodes[i].translate.z;
    }

    // Render at small resolution
    renderer.render(16, 12);
    TEST("framebuffer allocated", renderer.fb.pixels != nullptr);
    TEST("framebuffer size", renderer.fb.width == 16 && renderer.fb.height == 12);
    TEST("pixels written", renderer.fb.pixels[(16/2) + (12/2)*16] != 0); // center pixel should hit sphere

    // Check that some pixels are background (miss)
    TEST("bg pixel exists", renderer.fb.pixels[0] == mg::Frame::toRgba(0,0,0));

    renderer.arena.shutdown();
}

// ============================================================================
// Main
// ============================================================================
int main() {
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("========================================\n"); fflush(stdout);
    printf("  Render Layer Tests\n"); fflush(stdout);
    printf("========================================\n"); fflush(stdout);

    test_expr_eval();
    test_sdf_primitives();
    test_scene_loading();
    test_sdf_eval();
    test_ray_march();
    test_shading();
    test_optimizations();
    test_render();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    return (g_passed == g_tests) ? 0 : 1;
}
