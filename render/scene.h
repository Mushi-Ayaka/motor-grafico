#pragma once
#include "../os/os.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace mg {

// ---------------------------------------------------------------------------
// Scene types (mirrors core.h types for now, will supersede)
// ---------------------------------------------------------------------------
struct Vec3 { f32 x, y, z; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, f32 s)  { return {a.x*s, a.y*s, a.z*s}; }
inline Vec3 operator/(Vec3 a, f32 s)  { return {a.x/s, a.y/s, a.z/s}; }
inline f32  dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline f32  length(Vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
inline Vec3 normalize(Vec3 v) { f32 l = length(v); return l>0 ? v/l : Vec3{0,0,0}; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }

struct Expr {
    bool   is_expr = false;
    f32    constant = 0;
    std::string expression;
};

enum class LightType : u8 { DIRECTIONAL = 0, POINT = 1 };

struct Light {
    std::string name;
    LightType type = LightType::DIRECTIONAL;
    Vec3 direction = {0.3f, 0.8f, 0.5f};
    Vec3 position = {0,0,0};
    Vec3 color = {1,1,1};
    f32 intensity = 1.0f;
    f32 falloff = 0.0f;
};

struct Material {
    u32 id = 0;
    std::string name;
    Vec3 base_color = {1,1,1};
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    Vec3 emission = {0,0,0};
    f32 ior = 1.5f;
    f32 opacity = 1.0f;
    u32 blend_mode = 0;
};

struct SdfNode {
    std::string sdf_type;
    Expr params[4];
    u32 child_a = 0xFFFFFFFF;
    u32 child_b = 0xFFFFFFFF;
    std::string displace_expr;
};

enum class NodeType : u8 { SDF = 0, GROUP = 1, INSTANCE = 2 };
enum class NodeMode : u8 { SOLID = 0, VOLUME = 1 };

struct Node {
    u32 id = 0;
    std::string name;
    NodeType type = NodeType::SDF;
    NodeMode mode = NodeMode::SOLID;
    u32 material_id = 0xFFFFFFFF;
    f32 density_scale = 1.0f;
    u32 blend_mode = 0;
    Vec3 translate = {0,0,0};
    Vec3 rotate = {0,0,0};
    Vec3 scale = {1,1,1};
    SdfNode sdf;
    std::vector<u32> children;
    u32 def_id = 0xFFFFFFFF;
    bool is_compound_child = false;
};

struct Aabb {
    Vec3 min = { 1e9f,  1e9f,  1e9f};
    Vec3 max = {-1e9f, -1e9f, -1e9f};

    void expand(Vec3 p) {
        min.x = std::fmin(min.x, p.x); max.x = std::fmax(max.x, p.x);
        min.y = std::fmin(min.y, p.y); max.y = std::fmax(max.y, p.y);
        min.z = std::fmin(min.z, p.z); max.z = std::fmax(max.z, p.z);
    }

    void expand(const Aabb& o) {
        expand(o.min); expand(o.max);
    }

    bool contains(Vec3 p) const {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y
            && p.z >= min.z && p.z <= max.z;
    }

    f32 surfaceArea() const {
        Vec3 d = {max.x - min.x, max.y - min.y, max.z - min.z};
        return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
    }

    bool intersect(const Vec3& ro, const Vec3& rdir, f32& tmin, f32& tmax) const {
        Vec3 inv = {1.0f / rdir.x, 1.0f / rdir.y, 1.0f / rdir.z};
        f32 t1 = (min.x - ro.x) * inv.x;
        f32 t2 = (max.x - ro.x) * inv.x;
        f32 t3 = (min.y - ro.y) * inv.y;
        f32 t4 = (max.y - ro.y) * inv.y;
        f32 t5 = (min.z - ro.z) * inv.z;
        f32 t6 = (max.z - ro.z) * inv.z;
        tmin = std::fmax(std::fmax(std::fmin(t1, t2), std::fmin(t3, t4)), std::fmin(t5, t6));
        tmax = std::fmin(std::fmin(std::fmax(t1, t2), std::fmax(t3, t4)), std::fmax(t5, t6));
        return tmax >= std::fmax(tmin, 0.0f);
    }
};

struct Camera {
    Vec3 position = {0,2,5};
    Vec3 target = {0,0,0};
    Vec3 up = {0,1,0};
    f32 fov = 60.0f;
};

// ---------------------------------------------------------------------------
// Scene — loaded from RIH via FileMapping
// ---------------------------------------------------------------------------
struct Scene {
    u32 version = 1;
    u32 width = 800, height = 600, frames = 1;
    Camera camera;
    Vec3 background = {0,0,0};
    std::vector<Material> materials;
    std::vector<Node> nodes;
    std::vector<Light> lights;

    bool load(const FileMapping& fm);
    void loadDefault(); // fallback sphere scene
    int  stats(char* buf, int size) const; // returns written chars

    // Map from node ID to array index (built during load)
    u32  idToIndex(u32 id) const;
    std::unordered_map<u32,u32> _id_to_idx;
};

} // namespace mg
