// test_f1.cpp — Tests para RIH Optimizer (F1)
#include "render/ri_optimizer.h"
#include "render/scene.h"
#include "core/herm_bridge.h"
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

// Helper: create a simple bytecode stream with CONST + CONST + ADD pattern
static void makeFoldableBytecode(std::vector<uint8_t>& bc) {
    // CONST 2.0
    bc.push_back(0); // ONT_CONST
    float f = 2.0f;
    uint8_t* fp = (uint8_t*)&f;
    bc.push_back(fp[0]); bc.push_back(fp[1]); bc.push_back(fp[2]); bc.push_back(fp[3]);
    // CONST 3.0
    bc.push_back(0); // ONT_CONST
    f = 3.0f;
    fp = (uint8_t*)&f;
    bc.push_back(fp[0]); bc.push_back(fp[1]); bc.push_back(fp[2]); bc.push_back(fp[3]);
    // ADD
    bc.push_back(14); // ONT_ADD
    bc.push_back(0); bc.push_back(0); bc.push_back(0); bc.push_back(0);
    // END
    bc.push_back(34); // ONT_END
    bc.push_back(0); bc.push_back(0); bc.push_back(0); bc.push_back(0);
}

// Helper: create bytecode with algebraic simplification opportunity (x + 0)
static void makeAlgebraicBytecode(std::vector<uint8_t>& bc) {
    // CONST 5.0
    bc.push_back(0); // ONT_CONST
    float f = 5.0f;
    uint8_t* fp = (uint8_t*)&f;
    bc.push_back(fp[0]); bc.push_back(fp[1]); bc.push_back(fp[2]); bc.push_back(fp[3]);
    // CONST 0.0
    bc.push_back(0); // ONT_CONST
    f = 0.0f;
    fp = (uint8_t*)&f;
    bc.push_back(fp[0]); bc.push_back(fp[1]); bc.push_back(fp[2]); bc.push_back(fp[3]);
    // ADD (x + 0 -> x)
    bc.push_back(14); // ONT_ADD
    bc.push_back(0); bc.push_back(0); bc.push_back(0); bc.push_back(0);
    // END
    bc.push_back(34); // ONT_END
    bc.push_back(0); bc.push_back(0); bc.push_back(0); bc.push_back(0);
}

// ============================================================================
// F1.1: Constant folding
// ============================================================================
static void test_constant_folding() {
    printf("\n--- F1.1: Constant folding ---\n");

    // 2.0 + 3.0 should fold to 5.0
    std::vector<uint8_t> bc;
    makeFoldableBytecode(bc);

    uint32_t ops = mg::optimizeBytecode(bc.data(), (uint32_t)bc.size());
    TEST("ops eliminated >= 1", ops >= 1);

    // Read result: should be CONST 5.0 at position 0
    float result;
    memcpy(&result, &bc[1], 4);
    TEST("2.0 + 3.0 = 5.0", fabsf(result - 5.0f) < 0.001f);

    // Position 5 should be END (nop)
    TEST("position 5 is nop", bc[5] == 34); // ONT_END
    TEST("position 10 is nop", bc[10] == 34); // ONT_END
}

// ============================================================================
// F1.2: Algebraic simplification
// ============================================================================
static void test_algebraic_simplify() {
    printf("\n--- F1.2: Algebraic simplification ---\n");

    // 5.0 + 0.0 should simplify to 5.0
    std::vector<uint8_t> bc;
    makeAlgebraicBytecode(bc);

    uint32_t ops = mg::optimizeBytecode(bc.data(), (uint32_t)bc.size());
    TEST("ops eliminated >= 1", ops >= 1);

    float result;
    memcpy(&result, &bc[1], 4);
    TEST("5.0 + 0.0 = 5.0", fabsf(result - 5.0f) < 0.001f);
}

// ============================================================================
// F1.3: OntScene optimization
// ============================================================================
static void test_ontscene_optimization() {
    printf("\n--- F1.3: OntScene optimization ---\n");

    // Test constant folding in scene compilation
    const char* source =
        "scene \"test\" { axes { X: 100, Y: 100, W: 10 } }\n"
        "material \"m\" { base_color: [1, 0, 0]; }\n"
        "node \"n1\" { sdf: sphere(1.0); material: \"m\"; }\n"
        "node \"n2\" { sdf: box(2.0, 3.0, 4.0); material: \"m\"; }\n";

    mg::OntScene ont;
    std::string err;
    bool ok = mg::compileHermToOntScene(source, ont, &err);

    TEST("compile ok", ok);
    if (!ok) { printf("  ERROR: %s\n", err.c_str()); return; }

    TEST("node_count = 2", ont.header->node_count == 2);

    // Check that bytecode was optimized (ops should be reduced)
    // The optimizer runs during compilation, so we just verify the scene is valid
    TEST("bytecode_size > 0", ont.header->bytecode_size > 0);
    TEST("material_count > 0", ont.header->material_count > 0);
}

// ============================================================================
// main
// ============================================================================
int main() {
    printf("========================================\n");
    printf("  F1 RIH Optimizer Tests\n");
    printf("========================================\n");

    test_constant_folding();
    test_algebraic_simplify();
    test_ontscene_optimization();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    return (g_passed == g_tests) ? 0 : 1;
}
