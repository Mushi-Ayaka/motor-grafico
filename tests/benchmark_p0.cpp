// benchmark_p0.cpp — Mide métricas P0 para ARCHITECTURE_PARTITION_SPEC §5.1
// Uso: benchmark_p0.exe <scene.herm>
// Compilar: ver tests/build_benchmark.bat

#include "core/herm_bridge.h"
#include "render/render.h"
#include "render/vulkan_pipeline.h"
#include "os/os.h"
#include <cstdio>
#include <chrono>
#include <windows.h>
using namespace mg;

static bool saveBmp(const char* path, const Frame& fb) {
    int w = fb.width, h = fb.height;
    int rowBytes = ((w * 24 + 31) / 32) * 4;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    BITMAPFILEHEADER bf = {};
    bf.bfType = 0x4D42;
    bf.bfSize = 14 + 40 + rowBytes * h;
    bf.bfOffBits = 14 + 40;
    fwrite(&bf, sizeof(bf), 1, f);
    BITMAPINFOHEADER bi = {};
    bi.biSize = 40; bi.biWidth = w; bi.biHeight = h;
    bi.biPlanes = 1; bi.biBitCount = 24;
    bi.biSizeImage = rowBytes * h;
    fwrite(&bi, sizeof(bi), 1, f);
    std::vector<uint8_t> row(rowBytes);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            u32 px = fb.pixels[y * w + x];
            row[x * 3 + 0] = (px >> 16) & 0xFF;
            row[x * 3 + 1] = (px >> 8) & 0xFF;
            row[x * 3 + 2] = px & 0xFF;
        }
        fwrite(row.data(), 1, rowBytes, f);
    }
    fclose(f);
    return true;
}

// Count bytecode ops in OntScene
static uint32_t countBytecodeOps(const OntScene& ont) {
    if (!ont.header || !ont.bytecode) return 0;
    uint32_t total_ops = 0;
    for (uint32_t i = 0; i < ont.header->node_count; i++) {
        const auto& node = ont.graph_nodes[i];
        total_ops += node.bytecode_length / 5; // each op = 5 bytes
    }
    return total_ops;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: benchmark_p0.exe <scene.herm>\n");
        printf("  Renders the scene on CPU path and outputs P0 metrics.\n");
        return 1;
    }

    const char* herm_path = argv[1];
    printf("=== P0 Benchmark ===\n");
    printf("Scene: %s\n\n", herm_path);

    // 1. Read .herm source
    FILE* f = fopen(herm_path, "rb");
    if (!f) { printf("FAIL: cannot open %s\n", herm_path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string source(size, 0);
    fread(source.data(), 1, size, f);
    fclose(f);

    // 2. Compile .herm -> OntScene
    auto t0 = std::chrono::high_resolution_clock::now();
    OntScene ont;
    bool ok = compileHermToOntScene(source, ont);
    auto t1 = std::chrono::high_resolution_clock::now();
    if (!ok) { printf("FAIL: compile failed\n"); return 1; }

    double compile_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("Compile .herm -> OntScene: %.2f ms\n", compile_ms);

    // 3. Print scene info
    printf("\n--- Scene Info ---\n");
    printf("Nodes:      %u\n", ont.header->node_count);
    printf("BVH nodes:  %u\n", ont.header->bvh_count);
    printf("Materials:  %u\n", ont.header->material_count);
    printf("Bytecode:   %u bytes\n", ont.header->bytecode_size);
    printf("Tensor buf: %u bytes (%u slots)\n", ont.header->tensor_buffer_size,
           ont.header->tensor_buffer_size / (8 * sizeof(float)));
    printf("AABB min:   [%.2f, %.2f, %.2f]\n",
           ont.header->scene_aabb_min[0], ont.header->scene_aabb_min[1], ont.header->scene_aabb_min[2]);
    printf("AABB max:   [%.2f, %.2f, %.2f]\n",
           ont.header->scene_aabb_max[0], ont.header->scene_aabb_max[1], ont.header->scene_aabb_max[2]);

    uint32_t total_ops = countBytecodeOps(ont);
    printf("Total ops:  %u\n", total_ops);

    // 4. CPU Render
    Renderer renderer;
    renderer.ont_mode = false;
    renderer.ont_scene = std::move(ont);

    // Build BrickMap
    auto t2 = std::chrono::high_resolution_clock::now();
    buildBrickMap(renderer.ont_scene, renderer.brick_map);
    auto t3 = std::chrono::high_resolution_clock::now();
    double brickmap_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    printf("\nBrickMap build: %.2f ms\n", brickmap_ms);

    // JIT compile
    renderer.jit.init();
    auto t4 = std::chrono::high_resolution_clock::now();
    renderer.jit.compileScene(renderer.ont_scene);
    auto t5 = std::chrono::high_resolution_clock::now();
    double jit_ms = std::chrono::duration<double, std::milli>(t5 - t4).count();
    printf("JIT compile:    %.2f ms\n", jit_ms);

    // Render 1 frame (CPU)
    int W = 800, H = 600;
    renderer.fb.init(W, H);
    renderer.time = 0.0f;

    auto t6 = std::chrono::high_resolution_clock::now();
    renderer.render(W, H, 1.0f);
    auto t7 = std::chrono::high_resolution_clock::now();
    double render_cpu_ms = std::chrono::duration<double, std::milli>(t7 - t6).count();
    printf("CPU render:     %.2f ms (%.1f FPS)\n", render_cpu_ms, 1000.0 / render_cpu_ms);

    // Save output
    saveBmp("benchmark_p0_output.bmp", renderer.fb);
    printf("Saved: benchmark_p0_output.bmp\n");

    // 5. Summary
    printf("\n=== P0 Metrics ===\n");
    printf("compile_ms:       %.2f\n", compile_ms);
    printf("brickmap_ms:      %.2f\n", brickmap_ms);
    printf("jit_ms:           %.2f\n", jit_ms);
    printf("render_cpu_ms:    %.2f\n", render_cpu_ms);
    printf("render_cpu_fps:   %.1f\n", 1000.0 / render_cpu_ms);
    printf("total_ops:        %u\n", total_ops);
    printf("node_count:       %u\n", renderer.ont_scene.header->node_count);
    printf("material_count:   %u\n", renderer.ont_scene.header->material_count);
    printf("bytecode_bytes:   %u\n", renderer.ont_scene.header->bytecode_size);

    return 0;
}
