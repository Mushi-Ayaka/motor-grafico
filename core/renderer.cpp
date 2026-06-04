#include "renderer.h"
#include "sdf_eval.h"
#include <algorithm>
#include <cmath>

namespace mg {

static Vec3 getRayDir(const Camera& cam, f32 sx, f32 sy, f32 width, f32 height) {
    f32 aspect = width / height;
    f32 fov_tan = std::tan(cam.fov * PI / 360.0f);

    Vec3 forward = normalize(cam.target - cam.position);
    Vec3 right = normalize(cross(forward, cam.up));
    Vec3 up = cross(right, forward);

    f32 px = (2.0f * (sx + 0.5f) / width - 1.0f) * aspect * fov_tan;
    f32 py = (1.0f - 2.0f * (sy + 0.5f) / height) * fov_tan;

    return normalize(forward + right * px + up * py);
}

// Simple bounding box test
bool BBox::hit(const Vec3& origin, const Vec3& inv_dir) const {
    f32 t1 = (min.x - origin.x) * inv_dir.x;
    f32 t2 = (max.x - origin.x) * inv_dir.x;
    f32 t3 = (min.y - origin.y) * inv_dir.y;
    f32 t4 = (max.y - origin.y) * inv_dir.y;
    f32 t5 = (min.z - origin.z) * inv_dir.z;
    f32 t6 = (max.z - origin.z) * inv_dir.z;

    f32 tmin = std::fmax(std::fmax(std::fmin(t1, t2), std::fmin(t3, t4)), std::fmin(t5, t6));
    f32 tmax = std::fmin(std::fmin(std::fmax(t1, t2), std::fmax(t3, t4)), std::fmax(t5, t6));

    return tmax >= std::fmax(tmin, 0.0f);
}

// Compute approximate bounding boxes from node transforms
static void computeBBoxes(Rih& rih) {
    for (auto& node : rih.nodes) {
        (void)node;
        // BBox will be computed from children in real app
        // For prototype, just set empty bboxes
    }
}

// Pre-compute world transforms
static void computeTransforms(const Rih& rih, f32* out, u32 count) {
    for (u32 i = 0; i < count && i < rih.nodes.size(); i++) {
        out[i * 3 + 0] = rih.nodes[i].translate.x;
        out[i * 3 + 1] = rih.nodes[i].translate.y;
        out[i * 3 + 2] = rih.nodes[i].translate.z;
    }
}

static u32 toRgba(f32 r, f32 g, f32 b, f32 a = 1.0f) {
    u8 ri = (u8)std::fmin(std::fmax(r * 255.0f, 0), 255);
    u8 gi = (u8)std::fmin(std::fmax(g * 255.0f, 0), 255);
    u8 bi = (u8)std::fmin(std::fmax(b * 255.0f, 0), 255);
    u8 ai = (u8)std::fmin(std::fmax(a * 255.0f, 0), 255);
    return (u32)ri | ((u32)gi << 8) | ((u32)bi << 16) | ((u32)ai << 24);
}

static ColorRGB shade(const Rih& rih, const Vec3& p, const Vec3& n, u32 mat_id, f32 w) {
    ColorRGB result = {0.05f, 0.05f, 0.1f}; // ambient

    if (mat_id >= rih.materials.size()) return result;

    const Material& mat = rih.materials[mat_id];
    ColorRGB base = mat.base_color;
    f32 rough = mat.roughness;
    f32 metal = mat.metallic;

    for (const auto& light : rih.lights) {
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

        // Lambert diffuse
        f32 ndotl = std::fmax(dot(n, ldir), 0.0f);

        // Blinn-Phong specular
        Vec3 view = normalize(Vec3{0,0,0} - p); // simplified
        Vec3 half = normalize(ldir + view);
        f32 ndoth = std::fmax(dot(n, half), 0.0f);
        f32 spec = std::pow(ndoth, (1.0f - rough) * 128.0f + 1.0f);

        ColorRGB contrib;
        if (metal > 0.5f) {
            // Metallic: specular is tinted by base_color, diffuse is dark
            contrib.r = (base.r * spec * 0.8f + base.r * ndotl * 0.2f) * inten * atten;
            contrib.g = (base.g * spec * 0.8f + base.g * ndotl * 0.2f) * inten * atten;
            contrib.b = (base.b * spec * 0.8f + base.b * ndotl * 0.2f) * inten * atten;
        } else {
            // Dielectric: white specular, colored diffuse
            contrib.r = (base.r * ndotl * 0.7f + spec * 0.3f) * inten * atten;
            contrib.g = (base.g * ndotl * 0.7f + spec * 0.3f) * inten * atten;
            contrib.b = (base.b * ndotl * 0.7f + spec * 0.3f) * inten * atten;
        }

        result.r += contrib.r;
        result.g += contrib.g;
        result.b += contrib.b;
    }

    // Emission
    result.r += mat.emission.r;
    result.g += mat.emission.g;
    result.b += mat.emission.b;

    return result;
}

void render(Rih& rih, const RenderConfig& cfg, Framebuffer& fb) {
    fb.width = cfg.width;
    fb.height = cfg.height;
    fb.pixels.resize(cfg.width * cfg.height);

    // Pre-compute transforms
    auto transforms = std::vector<f32>(rih.nodes.size() * 3);
    computeTransforms(rih, transforms.data(), (u32)rih.nodes.size());
    computeBBoxes(rih);

    f32 w = cfg.time;

    for (u32 y = 0; y < cfg.height; y++) {
        for (u32 x = 0; x < cfg.width; x++) {
            Vec3 ro = cfg.camera.position;
            Vec3 rd = getRayDir(cfg.camera, (f32)x, (f32)y, (f32)cfg.width, (f32)cfg.height);

            f32 t = 0.0f;
            u32 hit_mat = 0xFFFFFFFF;
            Vec3 hit_p = {0,0,0};
            Vec3 hit_n = {0,1,0};
            bool hit = false;

            // Ray march
            for (u32 step = 0; step < cfg.max_steps; step++) {
                Vec3 p = ro + rd * t;
                f32 d = evalScene(rih, p, w, transforms.data());

                if (d < cfg.hit_eps) {
                    hit = true;
                    hit_p = p;
                    hit_mat = findMaterial(rih, p, w, transforms.data());
                    hit_n = calcNormal(rih, p, w, transforms.data());
                    break;
                }

                t += d;
                if (t > cfg.max_dist) break;
            }

            if (hit) {
                ColorRGB c = shade(rih, hit_p, hit_n, hit_mat, w);
                fb.pixels[y * cfg.width + x] = toRgba(c.r, c.g, c.b);
            } else {
                fb.pixels[y * cfg.width + x] = toRgba(
                    rih.background.r, rih.background.g, rih.background.b);
            }
        }
    }
}

} // namespace mg
