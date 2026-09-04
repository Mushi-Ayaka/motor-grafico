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

    const int hdr_sz = 124;  // OntHeader: 4+4+4+4+4+4+4+16+16+64 = 124
    const int gn_sz = 112;   // GraphNode: 64+4+4+4+16+16+1+3 = 112
    uint32_t gn_off = hdr_sz + bvh_count * 52;
    uint32_t bc_off = gn_off + node_count * gn_sz;
    uint32_t base = gn_off;

    printf("Header: 52, BVH: %u, GN: %u, BC: %u\n", bvh_count*52, node_count*gn_sz, bytecode_size);
    printf("Total expected: %u, File: %u\n", bc_off + bytecode_size, size);

    printf("\nGN[0] raw bytes (first 128):\n");
    for (int i = 0; i < 128; i++) {
        if (i % 16 == 0) printf("\n  %3d:", i);
        printf(" %02X", data[base + i]);
    }
    printf("\n");

    // Read float interpretations at various offsets
    printf("\nGN[0] as floats (first 16 = transform):\n");
    for (int i = 0; i < 16; i++) {
        float f = *(float*)(data + base + i*4);
        printf("  [%2d] = %.4f", i, f);
        if ((i+1) % 4 == 0) printf("\n");
    }

    printf("\nGN[0] as uint32 at offset 64-80:\n");
    for (int i = 64; i < 80; i += 4) {
        uint32_t u = *(uint32_t*)(data + base + i);
        printf("  [%2d] = 0x%08X = %u (as float: %.2f)\n", i, u, u, *(float*)(data + base + i));
    }

    // Now read all GN bytecode offsets correctly
    printf("\n=== ALL GN with gn_sz=%d ===\n", gn_sz);
    for (uint32_t i = 0; i < node_count; i++) {
        uint32_t b = gn_off + i * gn_sz;
        uint32_t bc_off_val = *(uint32_t*)(data + b + 68);
        uint32_t bc_len_val = *(uint32_t*)(data + b + 72);
        float tx = *(float*)(data + b + 48);  // local_transform[12]
        float ty = *(float*)(data + b + 52);  // local_transform[13]
        float tz = *(float*)(data + b + 56);  // local_transform[14]

        bool valid = (bc_off_val + bc_len_val <= bytecode_size) && (bc_len_val >= 5);
        printf("GN[%2u]: bc_off=%5u bc_len=%3u pos=(%7.2f,%7.2f,%7.2f) %s\n",
            i, bc_off_val, bc_len_val, tx, ty, tz, valid ? "OK" : "INVALID");
    }

    delete[] data;
    return 0;
}
