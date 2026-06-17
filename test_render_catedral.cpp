#include "render/scene.h"
#include "render/render.h"
#include "os/os.h"
#include <cstdio>
#include <windows.h>
using namespace mg;

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
    bi.biSize = 40; bi.biWidth = w; bi.biHeight = h;
    bi.biPlanes = 1; bi.biBitCount = 24;
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
    wchar_t path[512];
    wcscpy_s(path, L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Lenguaje Hermetico\\libreria\\escenas\\catedral_hermetica_0000.ont");
    mg::FileMapping fm;
    if (!fm.open(path)) { printf("FAIL: open .ont\n"); return 1; }
    mg::Renderer r;
    r.loadOnt(fm);
    fm.close();
    printf("Ont: %u nodes, %u bvh, %u mats, %u bc bytes\n",
        r.ont_scene.header->node_count, r.ont_scene.header->bvh_count,
        r.ont_scene.header->material_count, r.ont_scene.header->bytecode_size);

    // Load companion .obs
    wchar_t obs_path[512];
    wcscpy_s(obs_path, L"C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Lenguaje Hermetico\\libreria\\escenas\\catedral_hermetica_0000.obs");
    FileMapping obs_fm;
    if (obs_fm.open(obs_path)) {
        r.ont_scene.loadObs(obs_fm);
        obs_fm.close();
        r.ont_scene.applyObs();
        printf("Obs loaded: camera=%d lights=%d timeline=%d bg=%d res=%d\n",
            r.ont_scene.obs.has_camera, r.ont_scene.obs.has_lights,
            r.ont_scene.obs.has_timeline, r.ont_scene.obs.has_background,
            r.ont_scene.obs.has_resolution);
    } else {
        printf("No .obs found, using defaults\n");
    }

    // Print camera info
    auto& cam = r.ont_scene.camera;
    printf("Camera: pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) fov=%.1f up=(%.2f,%.2f,%.2f)\n",
        cam.position.x, cam.position.y, cam.position.z,
        cam.target.x, cam.target.y, cam.target.z,
        cam.fov, cam.up.x, cam.up.y, cam.up.z);

    // Print scene AABB
    auto* hdr = r.ont_scene.header;
    printf("Scene AABB: min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
        hdr->scene_aabb_min[0], hdr->scene_aabb_min[1], hdr->scene_aabb_min[2],
        hdr->scene_aabb_max[0], hdr->scene_aabb_max[1], hdr->scene_aabb_max[2]);

    // Print BVH root AABB
    if (hdr->bvh_count > 0) {
        auto& root = r.ont_scene.bvh_nodes[0];
        printf("BVH root: min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f) flags=%d skip=%d first_node=%d count=%d\n",
            root.min[0], root.min[1], root.min[2],
            root.max[0], root.max[1], root.max[2],
            root.flags, root.skip_index, root.first_node, root.node_count);
    }

    // Debug SDF at several points in world space
    Vec3 debug_pts[] = {
        {0, 0, 0},       // origin
        {0, -0.1, 0},    // just below ground
        {0, 4.5, 0},     // dome center
        {0, 0, -3},      // in front of wall fondo
        {0, -0.5, 0},    // under ground
        {8, 4, 10},      // camera position
    };
    for (auto& pt : debug_pts) {
        f32 d = bvhEval(r.ont_scene, pt, r.time);
        printf("SDF at (%.2f,%.2f,%.2f) = %.4f\n", pt.x, pt.y, pt.z, d);
    }

    // Print first 5 graph nodes' transforms and material
    u32 gn_limit = (hdr->node_count < 5) ? hdr->node_count : 5;
    for (u32 gi = 0; gi < gn_limit; gi++) {
        auto& g = r.ont_scene.graph_nodes[gi];
        printf("GN %u: mat=%u bc_off=%u bc_len=%u mode=%u\n",
            gi, g.material_id, g.bytecode_offset, g.bytecode_length, g.mode);
        printf("  xform col0=(%.2f,%.2f,%.2f,%.2f) col3=(%.2f,%.2f,%.2f,%.2f)\n",
            g.local_transform[0], g.local_transform[1], g.local_transform[2], g.local_transform[3],
            g.local_transform[12], g.local_transform[13], g.local_transform[14], g.local_transform[15]);
        printf("  bbox min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
            g.bbox_min[0], g.bbox_min[1], g.bbox_min[2],
            g.bbox_max[0], g.bbox_max[1], g.bbox_max[2]);
    }

    // Print lights info
    printf("Lights: %zu\n", r.ont_scene.obs.lights.size());
    for (size_t i = 0; i < r.ont_scene.obs.lights.size(); i++) {
        auto& l = r.ont_scene.obs.lights[i];
        printf("  Light %zu: dir=(%.2f,%.2f,%.2f) color=(%.2f,%.2f,%.2f) intensity=%.2f falloff=%.2f\n",
            i, l.direction.x, l.direction.y, l.direction.z,
            l.color.x, l.color.y, l.color.z, l.intensity, l.falloff);
    }

    // Render
    r.render(400, 300);
    printf("Rendered %dx%d\n", r.fb.width, r.fb.height);

    // Analyze center region
    int cx = r.fb.width/2, cy = r.fb.height/2;
    u32 cp = r.fb.pixels[cy * r.fb.width + cx];
    printf("Center pixel: R=%d G=%d B=%d (0x%08X)\n", cp&0xFF, (cp>>8)&0xFF, (cp>>16)&0xFF, cp);

    // Sample a few pixels across the frame
    for (int y = 0; y < r.fb.height; y += r.fb.height/4) {
        for (int x = 0; x < r.fb.width; x += r.fb.width/4) {
            u32 p = r.fb.pixels[y * r.fb.width + x];
            printf("  [%3d,%3d] R=%d G=%d B=%d\n", x, y, p&0xFF, (p>>8)&0xFF, (p>>16)&0xFF);
        }
    }

    // Check if frame is mostly black (render failure)
    int black = 0, total = r.fb.width * r.fb.height;
    for (int i = 0; i < total; i++) {
        u32 p = r.fb.pixels[i];
        if ((p & 0xFF) < 10 && ((p>>8)&0xFF) < 10 && ((p>>16)&0xFF) < 10) black++;
    }
    printf("Black pixels: %d/%d (%.1f%%)\n", black, total, 100.0f*black/total);

    saveBmp("C:\\Users\\Josue B\\Desktop\\Josue B\\Documents\\Jonatan Baron\\Proyectos\\proyecto de IA artistica\\Motor Grafico\\build\\catedral_0000.bmp", r.fb);
    printf("Saved catedral_0000.bmp\n");
    printf("DONE\n");
    return 0;
}
