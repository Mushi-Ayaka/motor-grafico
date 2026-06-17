#include "render/scene.h"
#include "render/render.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>
using namespace mg;

int main() {
    wchar_t abs[512];
    GetFullPathNameW(L"build\\test_suelo.rih", 512, abs, nullptr);
    wprintf(L"Loading: %s\n", abs);

    mg::FileMapping fm;
    if (!fm.open(abs)) { printf("FAIL: open\n"); return 1; }

    mg::Renderer renderer;
    if (!renderer.load(fm)) { printf("FAIL: load\n"); return 1; }
    fm.close();

    printf("Scene: %d nodes, %d mats\n",
        (int)renderer.scene.nodes.size(), (int)renderer.scene.materials.size());

    for (auto& m : renderer.scene.materials) {
        printf("Mat[%d] '%s': color=(%.2f,%.2f,%.2f)\n",
            m.id, m.name.c_str(), m.base_color.x, m.base_color.y, m.base_color.z);
    }

    renderer.render(320, 240);
    u32 pixel = renderer.fb.pixels[320 * 120 + 160];
    printf("Center pixel: 0x%08X  R=%d G=%d B=%d\n",
        pixel, pixel & 0xFF, (pixel >> 8) & 0xFF, (pixel >> 16) & 0xFF);

    pixel = renderer.fb.pixels[0];
    printf("Top-left pixel: 0x%08X\n", pixel);

    printf("DONE\n");
    return 0;
}
