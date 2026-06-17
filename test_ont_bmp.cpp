#include "render/scene.h"
#include "render/render.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>
using namespace mg;

static void analyzeFrame(const Frame& fb, const char* label) {
    int w = fb.width, h = fb.height;
    int n = w * h;
    u32 minCol = 0xFFFFFFFF, maxCol = 0;
    double rTot = 0, gTot = 0, bTot = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            u32 p = fb.pixels[y * w + x];
            u8 r = p & 0xFF, g = (p>>8)&0xFF, b = (p>>16)&0xFF;
            rTot += r; gTot += g; bTot += b;
            if (p < minCol) minCol = p;
            if (p > maxCol) maxCol = p;
        }
    printf("[%s] %dx%d avg=%.0f,%.0f,%.0f min=0x%08X max=0x%08X\n",
        label, w, h, rTot/n, gTot/n, bTot/n, minCol, maxCol);
}

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
    bi.biSize = 40;
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
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

int main() {
    // Test sphere
    wchar_t abs[512];
    wcscpy_s(abs, L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Motor Grafico\\build\\test_sphere.ont");
    mg::FileMapping fm;
    if (!fm.open(abs)) { printf("FAIL: open sphere\n"); return 1; }
    mg::Renderer r;
    r.loadOnt(fm);
    fm.close();

    printf("Sphere: %u nodes, %u bvh, %u mats, %u bc bytes\n",
        r.ont_scene.header->node_count, r.ont_scene.header->bvh_count,
        r.ont_scene.header->material_count, r.ont_scene.header->bytecode_size);
    
    // Set appropriate camera for sphere
    r.ont_scene.camera.position = {0, 0, -5};
    r.ont_scene.camera.target = {0, 0, 0};
    r.ont_scene.camera.fov = 60;
    r.ont_scene.camera.up = {0, 1, 0};

    // Test SDF evaluation at camera position (should be positive = outside sphere)
    f32 d0 = bvhEval(r.ont_scene, {0,0,-5}, 0);
    printf("SDF at camera (0,0,-5) = %.4f (should be ~4.0)\n", d0);
    f32 d1 = bvhEval(r.ont_scene, {0,0,0}, 0);
    printf("SDF at origin (0,0,0) = %.4f (should be ~-1.0)\n", d1);
    f32 d2 = bvhEval(r.ont_scene, {0,0,1}, 0);
    printf("SDF at (0,0,1) = %.4f (should be ~0.0)\n", d2);

    r.render(640, 480);
    analyzeFrame(r.fb, "sphere");
    saveBmp("C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Motor Grafico\\build\\frame_sphere.bmp", r.fb);

    // Verify center pixel is NOT sky blue and NOT black
    u32 center = r.fb.pixels[640 * 240 + 320];
    u8 cr = center & 0xFF, cg = (center>>8)&0xFF, cb = (center>>16)&0xFF;
    printf("Center: R=%d G=%d B=%d\n", cr, cg, cb);
    printf("Expected: R>0, G>0, B>0 (lit red sphere)\n");

    printf("DONE\n");
    return 0;
}
