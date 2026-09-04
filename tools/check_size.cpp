#include <cstdio>
#include <cstdint>
#pragma pack(push, 1)
struct OntHeader {
    uint32_t magic;
    uint32_t version;
    float    epsilon;
    uint32_t node_count;
    uint32_t bvh_count;
    uint32_t material_count;
    uint32_t bytecode_size;
    float    scene_aabb_min[4];
    float    scene_aabb_max[4];
    uint64_t reserved[8];
};
struct OntGraphNode {
    float    local_transform[16];
    uint32_t material_id;
    uint32_t bytecode_offset;
    uint32_t bytecode_length;
    float    bbox_min[4];
    float    bbox_max[4];
    uint8_t  mode;
    uint8_t  pad[3];
};
#pragma pack(pop)

int main() {
    printf("sizeof(OntHeader)=%llu (expected 124)\n", (unsigned long long)sizeof(OntHeader));
    printf("sizeof(OntGraphNode)=%llu (expected 112)\n", (unsigned long long)sizeof(OntGraphNode));
    bool ok = sizeof(OntHeader)==124 && sizeof(OntGraphNode)==112;
    printf("%s\n", ok ? "MATCH - fix correcto" : "MISMATCH - algo aun mal");
    return ok ? 0 : 1;
}
