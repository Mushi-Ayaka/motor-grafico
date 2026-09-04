#include "os/os.h"
#include "../Lenguaje Hermetico/contrato/ont.h"
#include <cstdio>
#include <windows.h>
#include <cstring>
#include <cmath>

int main() {
    wchar_t path[512];
    wcscpy_s(path, L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Lenguaje Hermetico\\libreria\\escenas\\catedral_hermetica_0000.ont");
    mg::FileMapping fm;
    if (!fm.open(path)) { printf("FAIL: open .ont\n"); return 1; }

    const uint8_t* data = (const uint8_t*)fm.data();
    const OntHeader* hdr = (const OntHeader*)data;
    if (hdr->magic != ONT_MAGIC) { printf("FAIL: bad magic 0x%08X\n", hdr->magic); return 1; }

    printf("=== ONT HEADER ===\n");
    printf("version=%u epsilon=%.4f node_count=%u bvh_count=%u material_count=%u bytecode_size=%u\n",
        hdr->version, hdr->epsilon, hdr->node_count, hdr->bvh_count, hdr->material_count, hdr->bytecode_size);
    printf("scene_aabb: min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
        hdr->scene_aabb_min[0], hdr->scene_aabb_min[1], hdr->scene_aabb_min[2],
        hdr->scene_aabb_max[0], hdr->scene_aabb_max[1], hdr->scene_aabb_max[2]);

    const BvhNode* bvh_nodes = (const BvhNode*)(data + sizeof(OntHeader));
    const GraphNode* graph_nodes = (const GraphNode*)((const uint8_t*)(bvh_nodes + hdr->bvh_count));
    const uint8_t* bytecode = (const uint8_t*)(graph_nodes + hdr->node_count);
    const OntMaterial* materials = (const OntMaterial*)(bytecode + hdr->bytecode_size);

    printf("\n=== BVH NODES (%u) ===\n", hdr->bvh_count);
    for (uint32_t i = 0; i < hdr->bvh_count; i++) {
        auto& b = bvh_nodes[i];
        printf("BVH[%2u]: min=(%7.2f,%7.2f,%7.2f) max=(%7.2f,%7.2f,%7.2f) d_min=%.4f L=%.4f skip=%2d first_node=%2u count=%5u flags=0x%04X is_leaf=%d\n",
            i,
            b.min[0], b.min[1], b.min[2],
            b.max[0], b.max[1], b.max[2],
            b.d_min, b.L,
            b.skip_index, b.first_node, b.node_count, b.flags, (b.flags & BVH_FLAG_LEAF));
    }

    printf("\n=== BVH LEAF ASSIGNMENT ===\n");
    for (uint32_t i = 0; i < hdr->bvh_count; i++) {
        auto& b = bvh_nodes[i];
        if (!(b.flags & BVH_FLAG_LEAF)) continue;
        printf("Leaf BVH[%u] graph nodes:", i);
        for (uint32_t j = 0; j < b.node_count; j++) {
            printf(" %u", b.first_node + j);
        }
        printf("\n");
    }

    printf("\n=== GRAPH NODES (%u) ===\n", hdr->node_count);
    for (uint32_t i = 0; i < hdr->node_count; i++) {
        auto& g = graph_nodes[i];
        float tx = g.local_transform[12];
        float ty = g.local_transform[13];
        float tz = g.local_transform[14];
        float sx = std::sqrt(g.local_transform[0]*g.local_transform[0] + g.local_transform[1]*g.local_transform[1] + g.local_transform[2]*g.local_transform[2]);
        printf("GN[%2u]: mat=%u bc_off=%5u bc_len=%3u mode=%u pos=(%7.2f,%7.2f,%7.2f) scale=%.3f\n",
            i, g.material_id, g.bytecode_offset, g.bytecode_length, g.mode,
            tx, ty, tz, sx);
        printf("       bbox=(%7.2f,%7.2f,%7.2f)-(%7.2f,%7.2f,%7.2f)\n",
            g.bbox_min[0], g.bbox_min[1], g.bbox_min[2],
            g.bbox_max[0], g.bbox_max[1], g.bbox_max[2]);
    }

    printf("\n=== MATERIALS (%u) ===\n", hdr->material_count);
    for (uint32_t i = 0; i < hdr->material_count; i++) {
        auto& m = materials[i];
        printf("MAT[%u]: id=%u color=(%.2f,%.2f,%.2f,%.2f) rough=%.2f metal=%.2f emit=(%.2f,%.2f,%.2f) op=%.2f\n",
            i, m.id,
            m.base_color[0], m.base_color[1], m.base_color[2], m.base_color[3],
            m.roughness, m.metallic,
            m.emission[0], m.emission[1], m.emission[2],
            m.opacity);
    }

    printf("\n=== BYTECODE UNIQUENESS ===\n");
    int unique_count = 0;
    for (uint32_t i = 0; i < hdr->node_count; i++) {
        auto& g = graph_nodes[i];
        if (g.bytecode_length == 0) continue;
        bool is_unique = true;
        for (uint32_t j = 0; j < i; j++) {
            auto& g2 = graph_nodes[j];
            if (g.bytecode_length == g2.bytecode_length && g.bytecode_offset == g2.bytecode_offset) {
                is_unique = false; break;
            }
        }
        if (is_unique) {
            unique_count++;
            printf("Unique SDF #%d: GN[%u] offset=%u len=%u ", unique_count, i, g.bytecode_offset, g.bytecode_length);
            for (uint32_t b = 0; b < g.bytecode_length && b < 40; b += 5) {
                uint32_t off = g.bytecode_offset + b;
                if (off + 5 > hdr->bytecode_size) break;
                uint8_t op = bytecode[off];
                float val = 0;
                if (op == ONT_CONST) memcpy(&val, &bytecode[off+1], 4);
                printf("%s", ontOpcodeName((OntOpcode)op));
                if (op == ONT_CONST) printf("(%.3f)", val);
                printf(" ");
            }
            printf("\n");
        }
    }
    printf("Total unique SDF functions: %d\n", unique_count);

    printf("\n=== SCENE GRAPH HIERARCHY ===\n");
    printf("(Using BVH leaf assignments)\n");

    return 0;
}
