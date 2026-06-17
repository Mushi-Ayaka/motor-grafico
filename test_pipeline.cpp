#include "render/scene.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>

int main() {
    wchar_t abs[512];
    GetFullPathNameW(L"..\\Lenguaje Hermetico\\ejemplos\\pipeline_test.rih", 512, abs, nullptr);
    wprintf(L"trying: %s\n", abs);

    mg::FileMapping fm;
    if (!fm.open(abs)) {
        printf("FAIL: could not open\n");
        return 1;
    }
    mg::Scene scene;
    bool ok = scene.load(fm);
    fm.close();
    if (!ok) {
        printf("FAIL: scene load\n");
        return 1;
    }
    const auto& pl = scene.pipeline;
    printf("trace: steps=%d threshold=%.4f t_max=%.1f\n", pl.trace_max_steps, pl.trace_hit_threshold, pl.trace_t_max);
    printf("shade: amb=%.2f diff=%.2f spec=%.2f power=%.1f\n", pl.shade_ambient, pl.shade_diffuse, pl.shade_specular, pl.shade_spec_power);
    printf("shadow: enabled=%d steps=%d max_dist=%.1f bias=%.4f\n", pl.shadow_enabled, pl.shadow_steps, pl.shadow_max_dist, pl.shadow_bias);
    printf("post: tonemap=%d gamma=%d exposure=%.1f\n", pl.post_tonemap, pl.post_gamma, pl.post_exposure);
    printf("PASS\n");
    return 0;
}
