// dump_glsl.cpp - Genera e imprime el GLSL especializado de un .ont (debug)
#include <windows.h>
#include "os/os.h"
#include "render/scene.h"
#include "render/glsl_gen.h"
#include "ont.h"
#include <cstdio>

static const char* opName(uint8_t op) {
    switch (op) {
        case ONT_CONST: return "CONST";
        case ONT_VAR_X: return "X"; case ONT_VAR_Y: return "Y"; case ONT_VAR_Z: return "Z";
        case ONT_ADD: return "ADD"; case ONT_SUB: return "SUB"; case ONT_MUL: return "MUL";
        case ONT_DIV: return "DIV"; case ONT_NEG: return "NEG"; case ONT_ABS: return "ABS";
        case ONT_SQRT: return "SQRT"; case ONT_MIN: return "MIN"; case ONT_MAX: return "MAX";
        case ONT_END: return "END";
        default: return "?";
    }
}

static void dumpBytecode(const mg::OntScene& sc) {
    for (uint32_t i = 0; i < sc.header->node_count; i++) {
        const mg::OntGraphNode& gn = sc.graph_nodes[i];
        printf("// node[%u] mat=%u off=%u len=%u transform_t=[%.2f %.2f %.2f]\n",
               i, gn.material_id, gn.bytecode_offset, gn.bytecode_length,
               gn.local_transform[12], gn.local_transform[13], gn.local_transform[14]);
        const uint8_t* bc = sc.bytecode + gn.bytecode_offset;
        uint32_t pos = 0;
        while (pos < gn.bytecode_length) {
            uint8_t op = bc[pos++];
            printf("%s", opName(op));
            if (op == ONT_CONST && pos + 4 <= gn.bytecode_length) {
                float v; memcpy(&v, bc + pos, 4); pos += 4;
                printf("(%.3f)", v);
            }
            printf(" ");
        }
        printf("\n");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("uso: dump_glsl <archivo.ont>\n"); return 2; }
    wchar_t wpath[512];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wpath, 512);
    mg::FileMapping fm;
    if (!fm.open(wpath)) { printf("no abre %s\n", argv[1]); return 2; }
    mg::OntScene sc;
    if (!sc.loadOnt(fm)) { printf("loadOnt FAIL\n"); return 1; }
    fm.close();
    printf("// nodes=%u bvh=%u mats=%u bc=%u epsilon=%.4f\n",
          sc.header->node_count, sc.header->bvh_count, sc.header->material_count,
          sc.header->bytecode_size, sc.header->epsilon);
    dumpBytecode(sc);
    auto r = mg::GlslGen::generate(sc);
    if (!r.ok) { printf("GEN ERR: %s\n", r.error.c_str()); return 1; }
    printf("%s\n", r.glsl_source.c_str());
    return 0;
}
