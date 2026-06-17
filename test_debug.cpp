#include "render/scene.h"
#include "render/render.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>
using namespace mg;

int main() {
    wchar_t abs[512];
    wcscpy_s(abs, L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Lenguaje Hermetico\\libreria\\escenas\\test_suelo.rih");
    wprintf(L"Loading: %s\n", abs);

    mg::FileMapping fm;
    if (!fm.open(abs)) { printf("FAIL: open\n"); return 1; }

    mg::Renderer renderer;
    renderer.load(fm);
    fm.close();

    printf("Scene: %d nodes, %d mats, %d lights, %dx%d\n",
        (int)renderer.scene.nodes.size(), (int)renderer.scene.materials.size(),
        (int)renderer.scene.lights.size(), renderer.scene.width, renderer.scene.height);

    // Check materials
    for (auto& m : renderer.scene.materials) {
        printf("Mat[%d] '%s': color=(%.2f,%.2f,%.2f) rough=%.2f metal=%.2f opacity=%.2f\n",
            m.id, m.name.c_str(), m.base_color.x, m.base_color.y, m.base_color.z,
            m.roughness, m.metallic, m.opacity);
    }

    // Check a few nodes
    for (size_t i = 0; i < renderer.scene.nodes.size() && i < 3; i++) {
        auto& n = renderer.scene.nodes[i];
        printf("Node[%zu] '%s' type=%d sdf_type='%s' mat=%d\n",
            i, n.name.c_str(), (int)n.type, n.sdf.sdf_type.c_str(), n.material_id);
    }

    // Render a single pixel at center
    renderer.render(1, 1);
    u32 pixel = renderer.fb.pixels[0];
    printf("Center pixel: 0x%08X  R=%d G=%d B=%d\n",
        pixel, pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF);

    // Render small image for visual check
    renderer.render(320, 240);
    // Check some pixels
    int mid = 320 * 120 + 160;
    pixel = renderer.fb.pixels[mid];
    printf("Center pixel (320x240): 0x%08X  R=%d G=%d B=%d\n",
        pixel, pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF);
    // Check top-left corner
    pixel = renderer.fb.pixels[0];
    printf("Top-left pixel: 0x%08X  R=%d G=%d B=%d\n",
        pixel, pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF);
    // Check a few more pixels
    for (int y = 0; y < 240; y += 60) {
        for (int x = 0; x < 320; x += 80) {
            pixel = renderer.fb.pixels[y * 320 + x];
            printf("[%d,%d]=0x%08X ", x, y, pixel);
        }
        printf("\n");
    }

    printf("DONE\n");
    return 0;
}
