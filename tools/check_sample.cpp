#include <cstdio>
#include <cstdint>
#include <windows.h>

int main() {
    const wchar_t* path = L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Lenguaje Hermetico\\libreria\\escenas\\catedral_hermetica_0000.ont";
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { printf("FAIL open\n"); return 1; }
    DWORD size = GetFileSize(h, NULL);
    uint8_t* data = new uint8_t[size];
    DWORD read;
    ReadFile(h, data, size, &read, NULL);
    CloseHandle(h);

    uint32_t node_count = *(uint32_t*)(data + 12);
    uint32_t bvh_count = *(uint32_t*)(data + 16);
    uint32_t bytecode_size = *(uint32_t*)(data + 24);
    printf("Nodes: %u BVH: %u Bytecode: %u\n", node_count, bvh_count, bytecode_size);

    // Correct sizes from ont.h:
    // BvhNode: 4+4+4+4+4+4+4+4+2+2 = 52 bytes  (min[4]+max[4]+d_min+L+skip_index+first_node+node_count+flags)
    // GraphNode: 16*4 + 4 + 4 + 4 + 4*4 + 4*4 + 1 + 3 = 64+4+4+4+16+16+1+3 = 112 bytes
    uint32_t gn_off = 52 + bvh_count * 52;
    uint32_t bc_off = gn_off + node_count * 112;
    printf("GN at %u, BC at %u (0x%X)\n", gn_off, bc_off, bc_off);

    // Verify total file size
    printf("File size: %u, Expected min: %u\n", size, bc_off + bytecode_size);

    // Scan for SAMPLE (opcode 30)
    int sample_count = 0;
    for (uint32_t i = 0; i < node_count; i++) {
        uint32_t base = gn_off + i * 112;
        // local_transform starts at base, material_id at base+64
        // uint32_t mat_id = *(uint32_t*)(data + base + 64);
        uint32_t bc_start = *(uint32_t*)(data + base + 68);
        uint32_t bc_len = *(uint32_t*)(data + base + 72);
        float tx = *(float*)(data + base + 12*4);
        float ty = *(float*)(data + base + 13*4);
        float tz = *(float*)(data + base + 14*4);

        bool has_sample = false;
        bool has_end = false;
        uint32_t abs_bc_start = bc_off + bc_start;
        if (bc_len >= 5 && abs_bc_start + bc_len <= bc_off + bytecode_size) {
            for (uint32_t b = 0; b < bc_len; b += 5) {
                uint8_t op = data[abs_bc_start + b];
                if (op == 30) has_sample = true;
                if (op == 34) has_end = true;
            }
        }
        printf("GN[%2u]: bc_off=%5u bc_len=%3u pos=(%7.2f,%7.2f,%7.2f) SAMPLE=%d END=%d\n",
            i, bc_start, bc_len, tx, ty, tz, has_sample ? 1 : 0, has_end ? 1 : 0);
        if (has_sample) sample_count++;
    }
    printf("Total nodes with SAMPLE: %d\n", sample_count);

    delete[] data;
    return 0;
}
