// test_ont_bridge.cpp - Verifica herm::Rih -> mg::OntScene (puente GPU/Vulkan).
// Compila un .herm y lo convierte a mg::OntScene (binario .ont en memoria).
#include <cstdint>
namespace herm { using f64 = double; }
#include "deps/lenguaje-hermetico/contrato/herm.h"
#include "render/scene.h"
#include "core/herm_bridge.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) { printf("uso: test_ont_bridge <sample.herm>\n"); return 2; }
    std::ifstream f(argv[1]);
    if (!f) { printf("no se pudo abrir %s\n", argv[1]); return 2; }
    std::stringstream ss; ss << f.rdbuf();
    std::string src = ss.str();

    mg::OntScene ont;
    std::string err;
    if (!mg::compileHermToOntScene(src, ont, &err)) {
        printf("COMPILE FAIL %s\n%s\n", argv[1], err.c_str());
        return 1;
    }

    // Validate OntScene
    if (!ont.header) { printf("FAIL: null header\n"); return 1; }
    if (ont.header->magic != 0x20544E4F) { printf("FAIL: bad magic 0x%X\n", ont.header->magic); return 1; }
    if (ont.header->node_count == 0) { printf("FAIL: 0 nodes\n"); return 1; }
    if (ont.header->material_count == 0) { printf("FAIL: 0 materials\n"); return 1; }
    if (ont.header->bytecode_size == 0) { printf("FAIL: 0 bytecode\n"); return 1; }
    if (!ont.graph_nodes) { printf("FAIL: null graph_nodes\n"); return 1; }
    if (!ont.bytecode) { printf("FAIL: null bytecode\n"); return 1; }
    if (!ont.materials) { printf("FAIL: null materials\n"); return 1; }

    printf("[ok] %s -> nodes=%u bvh=%u materials=%u bytecode=%u bytes\n",
        argv[1],
        ont.header->node_count,
        ont.header->bvh_count,
        ont.header->material_count,
        ont.header->bytecode_size);

    // Show first node info
    const mg::OntGraphNode& gn = ont.graph_nodes[0];
    printf("  node[0]: mat=%u bc_off=%u bc_len=%u mode=%u\n",
        gn.material_id, gn.bytecode_offset, gn.bytecode_length, gn.mode);
    printf("  transform: [%.2f %.2f %.2f %.2f]\n",
        gn.local_transform[0], gn.local_transform[5],
        gn.local_transform[10], gn.local_transform[15]);

    // Show material
    const mg::OntMaterial& om = ont.materials[0];
    printf("  mat[0]: color=(%.2f %.2f %.2f %.2f) rough=%.2f metal=%.2f opacity=%.2f\n",
        om.base_color[0], om.base_color[1], om.base_color[2], om.base_color[3],
        om.roughness, om.metallic, om.opacity);

    return 0;
}
