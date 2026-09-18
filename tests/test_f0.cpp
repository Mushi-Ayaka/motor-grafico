// test_f0.cpp — Tests mínimos para validar fixes F0
// Compilar: cl /EHsc /std:c++17 /utf-8 /O2 /MD /DVK_NO_PROTOTYPES /I "." /I "external\volk" /I "external\VulkanMemoryAllocator\include" test_f0.cpp core\herm_bridge.cpp render\scene.cpp render\sdf_eval.cpp os\win32\mem.cpp os\win32\file.cpp os\win32\timer.cpp os\win32\win32.cpp /link user32.lib gdi32.lib advapi32.lib
// O usar tests\build_f0_test.bat

#include "core/fixed_timestep.h"
#include "core/herm_bridge.h"
#include "render/scene.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static int g_tests = 0, g_passed = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    bool _ok = (expr); \
    if (_ok) g_passed++; \
    printf("  %s: %s\n", name, _ok ? "PASS" : "FAIL"); \
} while(0)

// ============================================================================
// F0.1: Color expr fix — evalExprConst parsea literales
// ============================================================================
static void test_color_expr() {
    printf("\n--- F0.1: Color expr fix ---\n");

    // Constant literal (is_expr=false)
    herm::Expr e1 = {false, 0.75f, ""};
    TEST("literal 0.75", mg::evalExprConstPublic(e1) == 0.75f);

    // Expression that is actually a number
    herm::Expr e2 = {true, 0.0f, "0.5"};
    TEST("expr '0.5' -> 0.5", mg::evalExprConstPublic(e2) == 0.5f);

    // Expression with negative number
    herm::Expr e3 = {true, 0.0f, "-1.23"};
    TEST("expr '-1.23' -> -1.23", fabsf(mg::evalExprConstPublic(e3) - (-1.23f)) < 0.001f);

    // Dynamic expression (contains 'w') -> returns 0.0f
    herm::Expr e4 = {true, 0.0f, "sin(w * 2)"};
    TEST("dynamic 'sin(w*2)' -> 0.0f", mg::evalExprConstPublic(e4) == 0.0f);

    // Dynamic expression (contains 't') -> returns 0.0f
    herm::Expr e5 = {true, 0.0f, "cos(t) * 0.5"};
    TEST("dynamic 'cos(t)*0.5' -> 0.0f", mg::evalExprConstPublic(e5) == 0.0f);

    // Empty expression
    herm::Expr e6 = {true, 0.0f, ""};
    TEST("empty expr -> 0.0f", mg::evalExprConstPublic(e6) == 0.0f);

    // Expression with spaces
    herm::Expr e7 = {true, 0.0f, "  3.14  "};
    TEST("expr '  3.14  ' -> 3.14", fabsf(mg::evalExprConstPublic(e7) - 3.14f) < 0.001f);
}

// ============================================================================
// F0.2: FixedTimestep — W avanza en pasos de 1/60
// ============================================================================
static void test_fixed_timestep() {
    printf("\n--- F0.2: FixedTimestep (W = tick_count * DT) ---\n");

    FixedTimestep ts;

    TEST("initial W = 0", ts.currentW() == 0.0f);
    TEST("initial tick_count = 0", ts.tick_count == 0);

    // 1 frame
    ts.advance(1.0f / 60.0f);
    TEST("1 frame: tick_count = 1", ts.tick_count == 1);
    TEST("1 frame: W = 1/60", fabsf(ts.currentW() - 1.0f/60.0f) < 0.0001f);
    TEST("1 frame: alpha = 0", ts.alpha == 0.0f);

    // 3 frames total
    ts.advance(1.0f / 60.0f);
    ts.advance(1.0f / 60.0f);
    TEST("3 frames: tick_count = 3", ts.tick_count == 3);
    TEST("3 frames: W = 3/60", fabsf(ts.currentW() - 3.0f/60.0f) < 0.0001f);

    // 1.5 frames
    ts.advance(1.5f / 60.0f);
    TEST("1.5 frames: tick_count = 4", ts.tick_count == 4);
    TEST("1.5 frames: alpha = 0.5", fabsf(ts.alpha - 0.5f) < 0.0001f);

    // No drift after 600 frames
    FixedTimestep ts2;
    for (int i = 0; i < 600; i++) ts2.advance(1.0f / 60.0f);
    float expected_w = 600.0f / 60.0f;
    float drift = fabsf(ts2.currentW() - expected_w);
    TEST("no drift after 600 frames", drift < 0.001f);
    TEST("tick_count = 600", ts2.tick_count == 600);

    // Deterministic
    FixedTimestep ts3, ts4;
    for (int i = 0; i < 100; i++) { ts3.advance(0.018f); ts4.advance(0.018f); }
    TEST("deterministic W", ts3.currentW() == ts4.currentW());

    // Reset
    ts3.reset();
    TEST("reset: W = 0", ts3.currentW() == 0.0f);
    TEST("reset: tick_count = 0", ts3.tick_count == 0);
}

// ============================================================================
// F0.3: Slot 0 header — tensor_buffer_size = (N+1)*8*sizeof(float)
// ============================================================================
static void test_slot0_header() {
    printf("\n--- F0.3: Slot 0 header fix ---\n");

    // Create a simple .herm source and compile to OntScene
    const char* source = 
        "scene \"test\" { axes { X: 100, Y: 100, W: 10 } }\n"
        "material \"m\" { base_color: [1, 0, 0]; }\n"
        "node \"n1\" { sdf: sphere(1.0); material: \"m\"; }\n"
        "node \"n2\" { sdf: box(1, 1, 1); material: \"m\"; }\n";

    mg::OntScene ont;
    std::string err;
    bool ok = mg::compileHermToOntScene(source, ont, &err);

    TEST("compile ok", ok);
    if (!ok) { printf("  ERROR: %s\n", err.c_str()); return; }

    uint32_t node_count = ont.header->node_count;
    TEST("node_count = 2", node_count == 2);

    uint32_t expected_size = (node_count + 1) * 8 * sizeof(float);
    TEST("tensor_buffer_size = (N+1)*8*4", ont.header->tensor_buffer_size == expected_size);
    printf("  tensor_buffer_size = %u (expected %u)\n", ont.header->tensor_buffer_size, expected_size);
}

// ============================================================================
// main
// ============================================================================
int main() {
    printf("========================================\n");
    printf("  F0 Validation Tests\n");
    printf("========================================\n");

    test_color_expr();
    test_fixed_timestep();
    test_slot0_header();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    return (g_passed == g_tests) ? 0 : 1;
}
