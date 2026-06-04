#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

namespace mg {

using f32 = float;
using u32 = uint32_t;
using u8  = uint8_t;

struct Vec3 { f32 x, y, z; };
struct ColorRGB { f32 r, g, b; };

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, f32 s)  { return {a.x*s, a.y*s, a.z*s}; }
inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }
inline Vec3 operator/(Vec3 a, f32 s)  { return {a.x/s, a.y/s, a.z/s}; }
inline Vec3 normalize(Vec3 v) { f32 l = std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); return l>0 ? v/l : Vec3{0,0,0}; }
inline f32  dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline f32  length(Vec3 v) { return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }

struct BBox {
    Vec3 min, max;
    bool hit(const Vec3& origin, const Vec3& inv_dir) const;
};

// Expression evaluable
struct Expr {
    bool   is_expr = false;
    f32    constant = 0;
    std::string expression;
};

// Light
enum class LightType : u8 { DIRECTIONAL = 0, POINT = 1 };
struct Light {
    std::string name;
    LightType type = LightType::DIRECTIONAL;
    Vec3 direction = {0.3f, 0.8f, 0.5f};
    Vec3 position = {0,0,0};
    ColorRGB color = {1,1,1};
    f32 intensity = 1.0f;
    f32 falloff = 0.0f;
};

// Material
struct Material {
    u32 id = 0;
    std::string name;
    ColorRGB base_color = {1,1,1};
    f32 roughness = 0.5f;
    f32 metallic = 0.0f;
    ColorRGB emission = {0,0,0};
    f32 ior = 1.5f;
    f32 opacity = 1.0f;
    u32 blend_mode = 0;  // 0=alpha, 1=add, 2=replace
};

// SDF node
struct SdfNode {
    std::string sdf_type;
    Expr params[4];
    u32 child_a = 0xFFFFFFFF;
    u32 child_b = 0xFFFFFFFF;
    std::string displace_expr;
};

// Scene node
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
    bool is_compound_child = false; // expanded from SDF array, skip in top-level eval

    BBox bbox; // computed at load time
};

// Scene
struct Camera {
    Vec3 position = {0,2,5};
    Vec3 target = {0,0,0};
    Vec3 up = {0,1,0};
    f32 fov = 60.0f;
};

struct Rih {
    u32 version = 1;
    u32 width = 800, height = 600, frames = 1;
    Camera camera;
    ColorRGB background = {0,0,0};
    std::vector<Material> materials;
    std::vector<Node> nodes;
    std::vector<Light> lights;
};

// Constants
constexpr f32 PI = 3.14159265358979323846f;

} // namespace mg
