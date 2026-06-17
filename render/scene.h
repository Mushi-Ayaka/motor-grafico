#pragma once
#include "../os/os.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstring>

#include "jit_compiler.h"

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

// Old structures (kept for backward compat during transition)
struct Expr {
    bool   is_expr = false;
    f32    constant = 0;
    std::string expression;
    std::vector<f32> bytecode;
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
    std::string material_expr[3];
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

struct PipelineConfig {
    int  trace_max_steps   = 80;
    f32  trace_hit_threshold = 0.001f;
    f32  trace_t_min       = 0.01f;
    f32  trace_t_max       = 50.0f;

    f32  shade_ambient     = 0.1f;
    f32  shade_diffuse     = 0.6f;
    f32  shade_specular    = 0.3f;
    f32  shade_spec_power  = 16.0f;

    bool shadow_enabled    = true;
    int  shadow_steps      = 32;
    f32  shadow_max_dist   = 5.0f;
    f32  shadow_bias       = 0.01f;

    bool post_tonemap      = true;
    bool post_gamma        = true;
    f32  post_exposure     = 2.0f;

    f32  terrain_blend_strength = 0.0f; // 0=off, 0.6=terrain scenes
};

// ============================================================================
// ONT types — loaded from .ont binary
// Packed structs matching the .ont format
// ============================================================================

#pragma pack(push, 1)
struct OntHeader {
    uint32_t magic;
    uint32_t version;
    float    epsilon;
    uint32_t node_count;
    uint32_t bvh_count;
    uint32_t material_count;
    uint32_t bytecode_size;
    float    scene_aabb_min[4];
    float    scene_aabb_max[4];
    uint64_t reserved[8];
};

struct OntBvhNode {
    float    min[4];
    float    max[4];
    float    d_min;
    float    L;
    int32_t  skip_index;
    uint32_t first_node;
    uint16_t node_count;
    uint16_t flags;
};

struct OntGraphNode {
    float    local_transform[16];
    uint32_t material_id;
    uint32_t bytecode_offset;
    uint32_t bytecode_length;
    float    bbox_min[4];
    float    bbox_max[4];
    uint8_t  mode;
    uint8_t  pad[3];
};

struct OntMaterial {
    uint32_t id;
    float    base_color[4];
    float    roughness;
    float    metallic;
    float    emission[3];
    float    opacity;
    uint32_t reserved[4];
};

// .obs — Observation (optional metadata alongside .ont)
struct ObsHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t reserved0;
    uint32_t camera_offset;
    uint32_t lights_offset;
    uint32_t timeline_offset;
    uint32_t background_offset;
    uint32_t resolution_offset;
    uint64_t reserved[4];
};

static constexpr uint32_t OBS_CAMERA     = 1u << 0;
static constexpr uint32_t OBS_LIGHTS     = 1u << 1;
static constexpr uint32_t OBS_TIMELINE   = 1u << 2;
static constexpr uint32_t OBS_BACKGROUND = 1u << 3;
static constexpr uint32_t OBS_RESOLUTION = 1u << 4;
static constexpr uint32_t OBS_MAGIC      = 0x2053424F;

struct ObsCamera {
    float position[3];
    float target[3];
    float up[3];
    float fov;
    float aperture;
    float focal_dist;
};

struct ObsLight {
    uint8_t type;
    uint8_t pad[3];
    float direction[3];
    float position[3];
    float color[3];
    float intensity;
    float falloff;
};

struct ObsLightsHeader {
    uint32_t count;
};

struct ObsTimeline {
    uint32_t w_frames;
    float w_min;
    float w_max;
    uint32_t t_unit_len;
};

#pragma pack(pop)

// ============================================================================
// Opcodes for .ont bytecode (matching Herm compiler's OpCode)
// ============================================================================
enum OntOpcode : uint8_t {
    ONT_CONST = 0,  ONT_VAR_X,  ONT_VAR_Y,  ONT_VAR_Z,  ONT_VAR_W,
    ONT_VAR_NX, ONT_VAR_NY, ONT_VAR_NZ,
    ONT_VAR_VX, ONT_VAR_VY, ONT_VAR_VZ,
    ONT_VAR_LX, ONT_VAR_LY, ONT_VAR_LZ,
    ONT_ADD, ONT_SUB, ONT_MUL, ONT_DIV, ONT_MOD,
    ONT_NEG, ONT_POW,
    ONT_SIN, ONT_COS, ONT_TAN,
    ONT_ABS, ONT_SQRT, ONT_FLOOR, ONT_CEIL,
    ONT_MIN, ONT_MAX,
    ONT_CLAMP, ONT_LERP, ONT_MIX,
    ONT_SAMPLE, ONT_END
};

static constexpr uint32_t ONT_MAGIC     = 0x20544E4F;
static constexpr uint16_t BVH_FLAG_LEAF = 1;

// ============================================================================
// OntScene — loaded directly from .ont binary
// ============================================================================
struct OntObservation {
    bool has_camera = false;
    bool has_lights = false;
    bool has_timeline = false;
    bool has_background = false;
    bool has_resolution = false;

    Camera camera = {};
    std::vector<Light> lights;
    u32 w_frames = 1;
    f32 w_min = 0.0f, w_max = 0.0f;
    Vec3 background = {0.1f, 0.1f, 0.2f};
    u32 width = 800, height = 600;
};

// ============================================================================
// BrickMap — V1 sparse brick map for static geometry
// Top-level 16³ grid pointing to 8³ bricks of SDF distances.
// Only allocates bricks for cells that contain geometry.
// Effective resolution: 128³, memory: ~1KB per occupied top-level cell.
// ============================================================================
struct BrickMap {
    static constexpr u32 TOP_BITS = 4;    // 16 = 2^4
    static constexpr u32 BRICK_BITS = 3;  // 8 = 2^3
    static constexpr u32 TOP_RES = 16;
    static constexpr u32 BRICK_RES = 8;
    static constexpr u32 BRICK_VOXELS = 512; // 8*8*8
    static constexpr u32 SENTINEL = 0xFFFFFFFF;

    Vec3  origin;                      // world-space min corner
    float top_cell_size;               // size of one top-level cell
    float inv_top_cell_size;
    float voxel_size;                  // size of one brick voxel (top_cell_size / BRICK_RES)

    u32   top_indices[TOP_RES * TOP_RES * TOP_RES]; // 16³, each = brick index+1 or SENTINEL
    u32   brick_count = 0;
    float* bricks = nullptr;           // brick_count * BRICK_VOXELS floats

    u32*  dyn_node_indices = nullptr;  // indices into graph_nodes that are dynamic
    u32   dyn_node_count = 0;
    bool  valid = false;

    void destroy() {
        free(bricks); bricks = nullptr;
        free(dyn_node_indices); dyn_node_indices = nullptr;
        brick_count = 0;
        valid = false;
    }
};

// Legacy uniform grid (V0) — kept for reference, no longer used
// struct SdfGrid { static constexpr u32 RES = 64; ... };
struct OntScene {
    u32 width = 800, height = 600;
    Camera camera = {};
    Vec3 background = {0.1f, 0.1f, 0.2f};
    PipelineConfig pipeline;

    // Observation data (loaded from optional .obs file)
    OntObservation obs;
    bool has_obs = false;

    // Pointers into the .ont file mapping (or owned copies)
    const OntHeader*     header     = nullptr;
    const OntBvhNode*    bvh_nodes  = nullptr;
    const OntGraphNode*  graph_nodes = nullptr;
    const uint8_t*       bytecode   = nullptr;
    const OntMaterial*   materials  = nullptr;

    // Owned copies (when loading without persistent mapping)
    std::vector<uint8_t> _data;
    std::vector<uint8_t> _obs_data;

    // JIT compiled functions corresponding to each node (if successfully compiled)
    std::vector<SdfJitFunc4> jit_functions;

    bool loadOnt(const FileMapping& fm);
    bool loadObs(const FileMapping& fm);
    void applyObs();
    void loadDefault();
};

// ============================================================================
// Legacy Scene — loaded from RIH JSON/RIB
// ============================================================================
struct Scene {
    u32 version = 1;
    u32 width = 800, height = 600, frames = 1;
    Camera camera;
    Vec3 background = {0,0,0};
    std::vector<Material> materials;
    std::vector<Node> nodes;
    std::vector<Light> lights;
    PipelineConfig pipeline;

    bool load(const FileMapping& fm);
    void loadDefault();
    int  stats(char* buf, int size) const;
    u32  idToIndex(u32 id) const;
    std::unordered_map<u32,u32> _id_to_idx;
};

} // namespace mg
