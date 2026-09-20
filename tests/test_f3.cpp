// test_f3.cpp -- Tests for F3: GPU-accelerated SDF shader generation
#include "render/spirv_gen.h"
#include "render/glsl_gen.h"
#include "core/herm_bridge.h"
#include <cstdio>
#include <cstring>

static int g_tests = 0, g_passed = 0;
#define TEST(name, expr) do { g_tests++; bool _ok=(expr); if(_ok) g_passed++; printf("  %s: %s\n", name, _ok?"PASS":"FAIL"); } while(0)

static void test_decompile() {
    printf("\n--- F3.1: Bytecode decompilation ---\n");

    const char* src =
        "scene \"s\" { axes { X:100, Y:100, W:10 } }\n"
        "material \"m\" { base_color: [1,0,0]; }\n"
        "node \"n1\" { sdf: sphere(2.5); material: \"m\"; }\n";

    mg::OntScene ont;
    std::string err;
    bool ok = mg::compileHermToOntScene(src, ont, &err);
    TEST("compile ok", ok);
    if (!ok) { printf("  ERROR: %s\n", err.c_str()); return; }

    const mg::OntGraphNode& gn = ont.graph_nodes[0];
    std::string expr = mg::SpirvGen::decompileBytecode(
        ont.bytecode + gn.bytecode_offset, gn.bytecode_length);
    TEST("decompile non-empty", !expr.empty());
    TEST("has p.x", expr.find("p.x") != std::string::npos);
    printf("  expression: %s\n", expr.c_str());
}

static void test_generate() {
    printf("\n--- F3.2: Full shader generation ---\n");

    const char* src =
        "scene \"s\" { axes { X:100, Y:100, W:10 } }\n"
        "material \"m1\" { base_color: [1,0,0]; }\n"
        "material \"m2\" { base_color: [0,1,0]; }\n"
        "node \"n1\" { sdf: sphere(1.0); material: \"m1\"; }\n"
        "node \"n2\" { sdf: box(2.0, 3.0, 4.0); material: \"m2\"; }\n";

    mg::OntScene ont;
    std::string err;
    bool ok = mg::compileHermToOntScene(src, ont, &err);
    TEST("compile ok", ok);
    if (!ok) { printf("  ERROR: %s\n", err.c_str()); return; }

    mg::SpirvGenResult result = mg::SpirvGen::generate(ont);
    TEST("generate ok", result.ok);
    TEST("nodes_processed = 2", result.nodes_processed == 2);
    TEST("unique_sdfs >= 1", result.unique_sdfs >= 1);

    const std::string& glsl = result.glsl_source;
    TEST("has #version 450", glsl.find("#version 450") != std::string::npos);
    TEST("has sdf_ functions", glsl.find("sdf_") != std::string::npos);
    TEST("has evalLeaf", glsl.find("evalLeaf") != std::string::npos);
    TEST("has main", glsl.find("void main()") != std::string::npos);
    TEST("no VM execBcRaw", glsl.find("execBcRaw") == std::string::npos);

    printf("  shader length: %zu chars\n", glsl.length());
    printf("  unique SDFs: %u\n", result.unique_sdfs);
}

static void test_dedup() {
    printf("\n--- F3.3: Bytecode deduplication ---\n");

    const char* src =
        "scene \"s\" { axes { X:100, Y:100, W:10 } }\n"
        "material \"m\" { base_color: [1,0,0]; }\n"
        "node \"a\" { sdf: sphere(1.0); material: \"m\"; }\n"
        "node \"b\" { sdf: sphere(1.0); material: \"m\"; }\n"
        "node \"c\" { sdf: box(2.0, 3.0, 4.0); material: \"m\"; }\n";

    mg::OntScene ont;
    std::string err;
    bool ok = mg::compileHermToOntScene(src, ont, &err);
    TEST("compile ok", ok);
    if (!ok) { printf("  ERROR: %s\n", err.c_str()); return; }

    mg::SpirvGenResult result = mg::SpirvGen::generate(ont);
    TEST("generate ok", result.ok);
    TEST("3 nodes processed", result.nodes_processed == 3);
    TEST("2 unique SDFs (sphere deduped)", result.unique_sdfs == 2);
}

int main() {
    printf("========================================\n");
    printf("  F3 GPU SDF Shader Generation Tests\n");
    printf("========================================\n");

    test_decompile();
    test_generate();
    test_dedup();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    return (g_passed == g_tests) ? 0 : 1;
}
