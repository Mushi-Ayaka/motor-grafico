#pragma once
#include "scene.h"
#include "sdf_eval.h"
#include <cmath>

namespace mg {

// ============================================================================
// Ray
// ============================================================================
struct Ray {
    Vec3 origin;
    Vec3 dir;
};

inline Vec3 getRayDir(const Camera& cam, f32 sx, f32 sy, f32 w, f32 h) {
    f32 aspect = w / h;
    f32 fov_tan = std::tan(cam.fov * 3.14159265f / 360.0f);
    Vec3 fwd = normalize(cam.target - cam.position);
    Vec3 right = normalize(cross(fwd, cam.up));
    Vec3 up = cross(right, fwd);
    f32 px = (2.0f * (sx + 0.5f) / w - 1.0f) * aspect * fov_tan;
    f32 py = (1.0f - 2.0f * (sy + 0.5f) / h) * fov_tan;
    return normalize(fwd + right * px + up * py);
}

// ============================================================================
// Shading (Blinn-Phong)
// ============================================================================

inline Vec3 shade(const Scene& scene, Vec3 p, Vec3 n, u32 mat_id, f32 w) {
    Vec3 result = {0.05f, 0.05f, 0.1f};
    if (mat_id >= scene.materials.size()) return result;

    const Material& mat = scene.materials[mat_id];

    for (const auto& light : scene.lights) {
        Vec3 ldir;
        f32 atten = 1.0f;
        f32 inten = light.intensity;

        if (light.type == LightType::DIRECTIONAL) {
            ldir = normalize(light.direction);
        } else {
            ldir = normalize(light.position - p);
            f32 dist = length(light.position - p);
            atten = 1.0f / (1.0f + light.falloff * dist * dist);
        }

        f32 ndotl = std::fmax(dot(n, ldir), 0.0f);
        Vec3 view = normalize({-p.x, -p.y, -p.z});
        Vec3 half = normalize(ldir + view);
        f32 ndoth = std::fmax(dot(n, half), 0.0f);
        f32 spec = std::pow(ndoth, (1.0f - mat.roughness) * 128.0f + 1.0f);

        Vec3 base = mat.base_color;
        Vec3 contrib;
        if (mat.metallic > 0.5f) {
            contrib.x = (base.x * spec * 0.8f + base.x * ndotl * 0.2f) * inten * atten;
            contrib.y = (base.y * spec * 0.8f + base.y * ndotl * 0.2f) * inten * atten;
            contrib.z = (base.z * spec * 0.8f + base.z * ndotl * 0.2f) * inten * atten;
        } else {
            contrib.x = (base.x * ndotl * 0.7f + spec * 0.3f) * inten * atten;
            contrib.y = (base.y * ndotl * 0.7f + spec * 0.3f) * inten * atten;
            contrib.z = (base.z * ndotl * 0.7f + spec * 0.3f) * inten * atten;
        }
        result.x += contrib.x;
        result.y += contrib.y;
        result.z += contrib.z;
    }

    result.x += mat.emission.x;
    result.y += mat.emission.y;
    result.z += mat.emission.z;
    return result;
}

// ============================================================================
// Ray marching
// ============================================================================

struct MarchResult {
    bool hit = false;
    Vec3 p = {0,0,0};
    Vec3 n = {0,1,0};
    u32 material = 0xFFFFFFFF;
    f32 t = 0.0f;
};

inline MarchResult rayMarch(const Scene& scene, Ray ray, f32 w,
                             f32 max_dist = 50.0f, f32 hit_eps = 0.001f, u32 max_steps = 128,
                             const f32* transforms = nullptr,
                             const Aabb* aabbs = nullptr,
                             const u32* visible_nodes = nullptr,
                             u32 visible_count = 0) {
    MarchResult r;
    f32 t = 0.0f;
    for (u32 step = 0; step < max_steps; step++) {
        Vec3 p = ray.origin + ray.dir * t;
        f32 d = evalScene(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
        // Adaptive epsilon: looser at distance, tighter up close
        f32 eps = hit_eps * (1.0f + t * 0.01f);
        if (d < eps) {
            r.hit = true;
            r.p = p;
            r.t = t;
            r.material = findMaterial(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
            r.n = calcNormal(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
            break;
        }
        t += d;
        if (t > max_dist) break;
    }
    return r;
}

// ============================================================================
// SceneQuery — optional acceleration for renderScene
// ============================================================================

struct SceneQuery {
    const Aabb* aabbs = nullptr; // per-node world AABBs for early-out
    u32 (*query_fn)(void* ctx, const Vec3& ro, const Vec3& rd,
                    u32* out, u32 max) = nullptr; // BVH query callback
    void* query_ctx = nullptr;

    u32 query(const Vec3& ro, const Vec3& rd, u32* out, u32 max) const {
        return query_fn ? query_fn(query_ctx, ro, rd, out, max) : 0;
    }
};

// ============================================================================
// Full render: march + shade per pixel
// ============================================================================

struct Frame {
    u32* pixels = nullptr;
    int  width = 0;
    int  height = 0;

    void init(Arena& arena, int w, int h) {
        pixels = (u32*)arena.alloc(w * h * sizeof(u32), 16);
        width = w;
        height = h;
    }

    static u32 toRgba(f32 r, f32 g, f32 b, f32 a = 1.0f) {
        return ((u32)std::fmin(std::fmax(r*255,0),255)) |
               (((u32)std::fmin(std::fmax(g*255,0),255)) << 8) |
               (((u32)std::fmin(std::fmax(b*255,0),255)) << 16) |
               (((u32)std::fmin(std::fmax(a*255,0),255)) << 24);
    }
};

inline void renderScene(const Scene& scene, Frame& fb, f32 w,
                         const f32* transforms = nullptr,
                         const Aabb* aabbs = nullptr,
                         const u32* visible_nodes = nullptr,
                         u32 visible_count = 0) {
    for (int y = 0; y < fb.height; y++) {
        for (int x = 0; x < fb.width; x++) {
            Ray ray;
            ray.origin = scene.camera.position;
            ray.dir = getRayDir(scene.camera, (f32)x, (f32)y, (f32)fb.width, (f32)fb.height);

            auto mr = rayMarch(scene, ray, w, 50.0f, 0.001f, 128, transforms,
                               aabbs, visible_nodes, visible_count);

            Vec3 bg = scene.background;
            if (mr.hit) {
                Vec3 c = shade(scene, mr.p, mr.n, mr.material, w);
                fb.pixels[y * fb.width + x] = Frame::toRgba(c.x, c.y, c.z);
            } else {
                fb.pixels[y * fb.width + x] = Frame::toRgba(bg.x, bg.y, bg.z);
            }
        }
    }
}

// renderScene with per-ray BVH query
inline void renderScene(const Scene& scene, Frame& fb, f32 w,
                         const f32* transforms,
                         const SceneQuery& sq) {
    for (int y = 0; y < fb.height; y++) {
        for (int x = 0; x < fb.width; x++) {
            Ray ray;
            ray.origin = scene.camera.position;
            ray.dir = getRayDir(scene.camera, (f32)x, (f32)y, (f32)fb.width, (f32)fb.height);

            u32 visible[256];
            u32 visible_count = sq.query(ray.origin, ray.dir, visible, 256);

            auto mr = rayMarch(scene, ray, w, 50.0f, 0.001f, 128, transforms,
                               sq.aabbs, visible, visible_count);

            Vec3 bg = scene.background;
            if (mr.hit) {
                Vec3 c = shade(scene, mr.p, mr.n, mr.material, w);
                fb.pixels[y * fb.width + x] = Frame::toRgba(c.x, c.y, c.z);
            } else {
                fb.pixels[y * fb.width + x] = Frame::toRgba(bg.x, bg.y, bg.z);
            }
        }
    }
}

} // namespace mg
