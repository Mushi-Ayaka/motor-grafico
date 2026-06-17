#pragma once
#include "scene.h"
#include "sdf_eval.h"
#include <cmath>
#include <cstdio>
#include "scene.h"
#include "bytecode_vm.h"
#include <thread>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
// Math helpers
// ============================================================================

inline f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

// 3D value noise (fast integer hash, 3 octaves)
inline f32 hash3(f32 px, f32 py, f32 pz) {
    unsigned ix = (unsigned)(int)std::floor(px);
    unsigned iy = (unsigned)(int)std::floor(py);
    unsigned iz = (unsigned)(int)std::floor(pz);
    unsigned n = ix * 374761393u + iy * 668265263u + iz * 1274126177u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n = n ^ (n >> 16);
    return (f32)(n & 0x7fffffffu) / 2147483648.0f;
}

inline f32 smoothNoise3D(f32 x, f32 y, f32 z) {
    f32 ix = std::floor(x), iy = std::floor(y), iz = std::floor(z);
    f32 fx = x - ix, fy = y - iy, fz = z - iz;
    fx = fx*fx*(3.0f-2.0f*fx);
    fy = fy*fy*(3.0f-2.0f*fy);
    fz = fz*fz*(3.0f-2.0f*fz);
    f32 v000 = hash3(ix, iy, iz);
    f32 v100 = hash3(ix+1, iy, iz);
    f32 v010 = hash3(ix, iy+1, iz);
    f32 v110 = hash3(ix+1, iy+1, iz);
    f32 v001 = hash3(ix, iy, iz+1);
    f32 v101 = hash3(ix+1, iy, iz+1);
    f32 v011 = hash3(ix, iy+1, iz+1);
    f32 v111 = hash3(ix+1, iy+1, iz+1);
    f32 v00 = lerp(v000, v100, fx);
    f32 v10 = lerp(v010, v110, fx);
    f32 v01 = lerp(v001, v101, fx);
    f32 v11 = lerp(v011, v111, fx);
    f32 v0 = lerp(v00, v10, fy);
    f32 v1 = lerp(v01, v11, fy);
    return lerp(v0, v1, fz);
}

inline f32 fbm3D(f32 x, f32 y, f32 z) {
    f32 value = 0.0f, amplitude = 0.5f, frequency = 1.0f;
    for (int i = 0; i < 3; i++) {
        value += amplitude * smoothNoise3D(x * frequency, y * frequency, z * frequency);
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return value;
}

// Detail noise: 3-octave FBM sampled at hit point (no triplanar overhead)
inline f32 detailNoise(Vec3 p, f32 scale) {
    return fbm3D(p.x * scale, p.y * scale, p.z * scale);
}

// ============================================================================
// Node classification (shared by V0 grid and V1 brick map)
// ============================================================================

// Scan bytecode for time-dependent opcodes (w only; trig on position is static)
inline bool isBcDynamic(const uint8_t* bc, u32 len) {
    for (u32 i = 0; i + 4 < len; i += 5) {
        OntOpcode op = (OntOpcode)bc[i];
        if (op == ONT_VAR_W) return true;
    }
    return false;
}

// Classify graph nodes into static vs dynamic. Returns number of dynamic nodes.
// Stores dynamic node indices in dyn_out (must have room for all nodes).
inline u32 classifyOntNodes(const OntScene& sc, u32* dyn_out) {
    u32 dyn_count = 0;
    for (u32 i = 0; i < sc.header->node_count; i++) {
        const auto& g = sc.graph_nodes[i];
        if (isBcDynamic(sc.bytecode + g.bytecode_offset, g.bytecode_length))
            dyn_out[dyn_count++] = i;
    }
    return dyn_count;
}

#if 0
// V0 uniform grid — replaced by BrickMap V1 (kept for reference)
inline f32 gridSample(const SdfGrid& g, Vec3 p) { ... }
inline void buildSdfGrid(const OntScene& sc, SdfGrid& grid) { ... }
inline f32 evalHybrid(const OntScene& sc, Vec3 p, f32 w, const SdfGrid& grid, ...) { ... }
#endif

// ============================================================================
// BVH traversal (stackless, with skip_index)
// Returns SDF distance for given point p
// ============================================================================

// Transform point by column-major 4x4 matrix (world → local)
inline void applyMatrix(const float m[16], const Vec3& p, float& out_x, float& out_y, float& out_z) {
    out_x = m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12];
    out_y = m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13];
    out_z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
}

// Extract uniform scale factor from column-major 4x4 world→local matrix
// d_world = d_local / col_len where col_len = |first column|
inline f32 extractScale(const float m[16]) {
    return std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
}

// Evaluate all dynamic graph nodes at point p (no BVH, direct traversal)
inline f32 evalDynamicNodes(const OntScene& sc, Vec3 p, f32 w,
                             const u32* dyn_indices, u32 dyn_count,
                             f32 search_radius = 1e9f,
                             u32* out_material = nullptr) {
    f32 result = search_radius;
    u32 best_mat = out_material ? *out_material : 0xFFFFFFFF;
    for (u32 i = 0; i < dyn_count; i++) {
        u32 gi = dyn_indices[i];
        const auto& g = sc.graph_nodes[gi];
        f32 dx = std::fmax(0.0f, std::fmax(g.bbox_min[0] - p.x, p.x - g.bbox_max[0]));
        f32 dy = std::fmax(0.0f, std::fmax(g.bbox_min[1] - p.y, p.y - g.bbox_max[1]));
        f32 dz = std::fmax(0.0f, std::fmax(g.bbox_min[2] - p.z, p.z - g.bbox_max[2]));
        if (dx*dx + dy*dy + dz*dz > result * result) continue;

        float lx, ly, lz;
        applyMatrix(g.local_transform, p, lx, ly, lz);
        f32 d = execBcRaw(sc.bytecode + g.bytecode_offset, g.bytecode_length,
                           lx, ly, lz, w);
        f32 scale = extractScale(g.local_transform);
        if (scale > 1e-8f) d /= scale;
        if (d < result) { result = d; best_mat = g.material_id; }
    }
    if (out_material) *out_material = best_mat;
    return result;
}

inline f32 bvhEval(const OntScene& sc, Vec3 p, f32 w, u32* out_material = nullptr, f32 initial_dist = 1e9f) {
    f32 result = initial_dist;
    u32 best_mat = 0xFFFFFFFF;
    u32 node_idx = 0;
    const u32 bvh_count = sc.header->bvh_count;
    const auto* bvh = sc.bvh_nodes;
    const auto* gn = sc.graph_nodes;
    const auto* bc = sc.bytecode;

    while (node_idx < bvh_count) {
        const auto& node = bvh[node_idx];
        // Distance-to-AABB test: if the AABB is farther than our best distance, skip
        f32 dx = std::fmax(0.0f, std::fmax(node.min[0] - p.x, p.x - node.max[0]));
        f32 dy = std::fmax(0.0f, std::fmax(node.min[1] - p.y, p.y - node.max[1]));
        f32 dz = std::fmax(0.0f, std::fmax(node.min[2] - p.z, p.z - node.max[2]));
        if (dx*dx + dy*dy + dz*dz > result * result) {
            node_idx = (u32)node.skip_index;
            continue;
        }
        if (node.flags & BVH_FLAG_LEAF) {
            for (uint16_t i = 0; i < node.node_count; i++) {
                u32 gi = node.first_node + i;
                if (gi >= sc.header->node_count) break;
                const auto& g = gn[gi];
                float lx, ly, lz;
                applyMatrix(g.local_transform, p, lx, ly, lz);
                f32 d = execBcRaw(bc + g.bytecode_offset, g.bytecode_length,
                                  lx, ly, lz, w);
                f32 scale = extractScale(g.local_transform);
                if (scale > 1e-8f) d /= scale;
                if (d < result) { result = d; best_mat = g.material_id; }
            }
            node_idx = (u32)node.skip_index;
        } else {
            node_idx++;
        }
    }
    if (out_material) *out_material = best_mat;
    return result;
}

#if 0
// V0 uniform grid — replaced by BrickMap V1 (kept for reference)
// ============================================================================
// SdfGrid — build + hybrid evaluation
// ============================================================================

// Build the SDF grid from static nodes at load time
inline void buildSdfGrid(const OntScene& sc, SdfGrid& grid) { ... }

// Hybrid evaluation: grid + dynamic nodes + fallback to exact near surface
inline f32 evalHybrid(const OntScene& sc, Vec3 p, f32 w,
                       const SdfGrid& grid, u32* out_material = nullptr,
                       f32 hit_eps = 0.001f) { ... }
#endif

// ============================================================================
// BrickMap V1 — sparse brick map: build, sample, hybrid eval
// Top-level 16³ grid pointing to 8³ bricks of SDF distances.
// ============================================================================

// Classify which top-level cells contain geometry
static void classifyBrickCells(const OntScene& sc, const bool* is_static,
                                u32 brick_mask[BrickMap::TOP_RES * BrickMap::TOP_RES * BrickMap::TOP_RES],
                                u32& brick_count) {
    brick_count = 0;
    Vec3 bmin = {sc.bvh_nodes[0].min[0], sc.bvh_nodes[0].min[1], sc.bvh_nodes[0].min[2]};
    Vec3 bmax = {sc.bvh_nodes[0].max[0], sc.bvh_nodes[0].max[1], sc.bvh_nodes[0].max[2]};
    f32 top_cs = fmax(bmax.x - bmin.x, fmax(bmax.y - bmin.y, bmax.z - bmin.z)) / (f32)BrickMap::TOP_RES;
    for (u32 ni = 0; ni < sc.header->node_count; ni++) {
        if (!is_static[ni]) continue;
        const auto& gn = sc.graph_nodes[ni];
        int ix0 = (std::max)(0, (std::min)((int)((gn.bbox_min[0] - bmin.x) / top_cs), (int)BrickMap::TOP_RES - 1));
        int iy0 = (std::max)(0, (std::min)((int)((gn.bbox_min[1] - bmin.y) / top_cs), (int)BrickMap::TOP_RES - 1));
        int iz0 = (std::max)(0, (std::min)((int)((gn.bbox_min[2] - bmin.z) / top_cs), (int)BrickMap::TOP_RES - 1));
        int ix1 = (std::max)(0, (std::min)((int)((gn.bbox_max[0] - bmin.x) / top_cs), (int)BrickMap::TOP_RES - 1));
        int iy1 = (std::max)(0, (std::min)((int)((gn.bbox_max[1] - bmin.y) / top_cs), (int)BrickMap::TOP_RES - 1));
        int iz1 = (std::max)(0, (std::min)((int)((gn.bbox_max[2] - bmin.z) / top_cs), (int)BrickMap::TOP_RES - 1));
        for (int iz = iz0; iz <= iz1; iz++)
            for (int iy = iy0; iy <= iy1; iy++)
                for (int ix = ix0; ix <= ix1; ix++) {
                    u32 idx = iz * BrickMap::TOP_RES * BrickMap::TOP_RES + iy * BrickMap::TOP_RES + ix;
                    if (brick_mask[idx] == 0) { brick_mask[idx] = 1; brick_count++; }
                }
    }
}

// Build the brick map from static nodes at load time
inline void buildBrickMap(const OntScene& sc, BrickMap& bm) {
    Vec3 bmin = {sc.bvh_nodes[0].min[0], sc.bvh_nodes[0].min[1], sc.bvh_nodes[0].min[2]};
    Vec3 bmax = {sc.bvh_nodes[0].max[0], sc.bvh_nodes[0].max[1], sc.bvh_nodes[0].max[2]};
    Vec3 size = {bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z};

    bm.origin = bmin;
    bm.top_cell_size = fmax(size.x, fmax(size.y, size.z)) / (f32)BrickMap::TOP_RES;
    bm.inv_top_cell_size = 1.0f / bm.top_cell_size;
    bm.voxel_size = bm.top_cell_size / (f32)BrickMap::BRICK_RES;

    // Init top-level: all empty
    for (u32 i = 0; i < BrickMap::TOP_RES * BrickMap::TOP_RES * BrickMap::TOP_RES; i++)
        bm.top_indices[i] = BrickMap::SENTINEL;

    // Classify nodes (static vs dynamic)
    bm.dyn_node_indices = (u32*)malloc(sc.header->node_count * sizeof(u32));
    bm.dyn_node_count = classifyOntNodes(sc, bm.dyn_node_indices);

    // Precompute static flags
    bool* is_static = (bool*)malloc(sc.header->node_count * sizeof(bool));
    if (!is_static) return;
    for (u32 ni = 0; ni < sc.header->node_count; ni++) {
        is_static[ni] = true;
        for (u32 di = 0; di < bm.dyn_node_count; di++)
            if (bm.dyn_node_indices[di] == ni) { is_static[ni] = false; break; }
    }

    // Classify bricks
    u32 brick_mask[BrickMap::TOP_RES * BrickMap::TOP_RES * BrickMap::TOP_RES] = {0};
    u32 brick_count = 0;
    classifyBrickCells(sc, is_static, brick_mask, brick_count);

    if (brick_count == 0) { free(is_static); bm.valid = true; return; }

    // Allocate brick data
    bm.bricks = (float*)malloc(brick_count * BrickMap::BRICK_VOXELS * sizeof(float));
    if (!bm.bricks) { free(is_static); return; }
    bm.brick_count = brick_count;

    // Assign brick indices
    u32 next_idx = 0;
    for (int iz = 0; iz < (int)BrickMap::TOP_RES; iz++)
        for (int iy = 0; iy < (int)BrickMap::TOP_RES; iy++)
            for (int ix = 0; ix < (int)BrickMap::TOP_RES; ix++) {
                u32 ti = iz * BrickMap::TOP_RES * BrickMap::TOP_RES + iy * BrickMap::TOP_RES + ix;
                if (brick_mask[ti]) bm.top_indices[ti] = next_idx++;
            }

    // Fill brick data: sample static SDF at each voxel center
    f32 top_cs = bm.top_cell_size;
    f32 vox_cs = bm.voxel_size;
    for (int iz = 0; iz < (int)BrickMap::TOP_RES; iz++) {
        for (int iy = 0; iy < (int)BrickMap::TOP_RES; iy++) {
            for (int ix = 0; ix < (int)BrickMap::TOP_RES; ix++) {
                u32 ti = iz * BrickMap::TOP_RES * BrickMap::TOP_RES + iy * BrickMap::TOP_RES + ix;
                if (bm.top_indices[ti] == BrickMap::SENTINEL) continue;
                u32 bi = bm.top_indices[ti];
                float* brick = bm.bricks + bi * BrickMap::BRICK_VOXELS;
                f32 cell_ox = bm.origin.x + ix * top_cs;
                f32 cell_oy = bm.origin.y + iy * top_cs;
                f32 cell_oz = bm.origin.z + iz * top_cs;
                f32 cell_max_x = cell_ox + top_cs, cell_max_y = cell_oy + top_cs, cell_max_z = cell_oz + top_cs;
                for (int vz = 0; vz < (int)BrickMap::BRICK_RES; vz++) {
                    for (int vy = 0; vy < (int)BrickMap::BRICK_RES; vy++) {
                        for (int vx = 0; vx < (int)BrickMap::BRICK_RES; vx++) {
                            Vec3 wp = {cell_ox + (vx + 0.5f) * vox_cs, cell_oy + (vy + 0.5f) * vox_cs, cell_oz + (vz + 0.5f) * vox_cs};
                            f32 min_s = 1e9f;
                            for (u32 ni = 0; ni < sc.header->node_count; ni++) {
                                if (!is_static[ni]) continue;
                                const auto& gn = sc.graph_nodes[ni];
                                if (gn.bbox_min[0] > cell_max_x || gn.bbox_max[0] < cell_ox ||
                                    gn.bbox_min[1] > cell_max_y || gn.bbox_max[1] < cell_oy ||
                                    gn.bbox_min[2] > cell_max_z || gn.bbox_max[2] < cell_oz)
                                    continue;
                                float lx, ly, lz;
                                applyMatrix(gn.local_transform, wp, lx, ly, lz);
                                f32 d = execBcRaw(sc.bytecode + gn.bytecode_offset, gn.bytecode_length, lx, ly, lz, 0);
                                f32 scale = extractScale(gn.local_transform);
                                if (scale > 1e-8f) d /= scale;
                                if (d < min_s) min_s = d;
                            }
                            brick[vz * BrickMap::BRICK_RES * BrickMap::BRICK_RES + vy * BrickMap::BRICK_RES + vx] = min_s;
                        }
                    }
                }
            }
        }
    }

    free(is_static);
    bm.valid = true;
}

// Trilinear sample of the brick map
inline f32 brickSample(const BrickMap& bm, Vec3 p) {
    f32 gx = (p.x - bm.origin.x) * bm.inv_top_cell_size;
    f32 gy = (p.y - bm.origin.y) * bm.inv_top_cell_size;
    f32 gz = (p.z - bm.origin.z) * bm.inv_top_cell_size;
    int ix = (int)std::floor(gx);
    int iy = (int)std::floor(gy);
    int iz = (int)std::floor(gz);
    if (ix < 0 || ix >= (int)BrickMap::TOP_RES || iy < 0 || iy >= (int)BrickMap::TOP_RES || iz < 0 || iz >= (int)BrickMap::TOP_RES)
        return 1e9f;
    u32 bi = bm.top_indices[iz * BrickMap::TOP_RES * BrickMap::TOP_RES + iy * BrickMap::TOP_RES + ix];
    if (bi == BrickMap::SENTINEL) return 1e9f;

    f32 fx = (gx - ix) * (f32)BrickMap::BRICK_RES;
    f32 fy = (gy - iy) * (f32)BrickMap::BRICK_RES;
    f32 fz = (gz - iz) * (f32)BrickMap::BRICK_RES;
    fx = std::fmax(0.0f, std::fmin(fx, (f32)(BrickMap::BRICK_RES - 2 + 0.999f)));
    fy = std::fmax(0.0f, std::fmin(fy, (f32)(BrickMap::BRICK_RES - 2 + 0.999f)));
    fz = std::fmax(0.0f, std::fmin(fz, (f32)(BrickMap::BRICK_RES - 2 + 0.999f)));

    int pi = (int)fx, pj = (int)fy, pk = (int)fz;
    f32 xf = fx - pi, yf = fy - pj, zf = fz - pk;
    int r = (int)BrickMap::BRICK_RES;
    const float* d = bm.bricks + bi * BrickMap::BRICK_VOXELS;

    f32 c000 = d[pk*r*r + pj*r + pi];
    f32 c100 = d[pk*r*r + pj*r + pi+1];
    f32 c010 = d[pk*r*r + (pj+1)*r + pi];
    f32 c110 = d[pk*r*r + (pj+1)*r + pi+1];
    f32 c001 = d[(pk+1)*r*r + pj*r + pi];
    f32 c101 = d[(pk+1)*r*r + pj*r + pi+1];
    f32 c011 = d[(pk+1)*r*r + (pj+1)*r + pi];
    f32 c111 = d[(pk+1)*r*r + (pj+1)*r + pi+1];

    return lerp(lerp(lerp(c000, c100, xf), lerp(c010, c110, xf), yf),
                lerp(lerp(c001, c101, xf), lerp(c011, c111, xf), yf), zf);
}

// Margin multiplier for hybrid fallback: if brick distance < hit_eps * BRICK_MARGIN_MULT,
// fall to exact BVH evaluation. Set to 0 to disable fallback (always use brick).
#ifndef BRICK_MARGIN_MULT
#define BRICK_MARGIN_MULT 8.0f
#endif

// BrickProfiler — path frequency counters (zero overhead without BRICK_PROFILE)
struct alignas(64) BrickProfiler {
    u64 brick_calls = 0;
    u64 bvh_calls = 0;
    u64 dyn_calls = 0;
    u64 total_calls = 0;
};

#ifdef BRICK_PROFILE
inline void brickProfilerPrint(const BrickProfiler& p, const char* label) {
    u64 t = p.total_calls ? p.total_calls : 1;
    fprintf(stderr, "[BRICK_PROFILE %s] total=%llu  brick=%llu (%.1f%%)  bvh=%llu (%.1f%%)  dyn=%llu (%.1f%%)\n",
            label, p.total_calls,
            p.brick_calls, 100.0 * p.brick_calls / t,
            p.bvh_calls, 100.0 * p.bvh_calls / t,
            p.dyn_calls, 100.0 * p.dyn_calls / t);
}
#endif

// Hybrid evaluation: brick map + dynamic nodes + exact fallback near surface
inline f32 evalBrickHybrid(const OntScene& sc, Vec3 p, f32 w,
                            const BrickMap& bm, u32* out_material = nullptr,
                            f32 hit_eps = 0.001f) {
    f32 bd = brickSample(bm, p);
    u32 mat = 0xFFFFFFFF;
    if (bd < hit_eps * BRICK_MARGIN_MULT) {
        bd = bvhEval(sc, p, w, &mat);
    }
    u32 dyn_mat = 0xFFFFFFFF;
    f32 dyn_d = evalDynamicNodes(sc, p, w, bm.dyn_node_indices, bm.dyn_node_count, bd, &dyn_mat);
    if (dyn_d < bd) { 
        if (out_material) *out_material = dyn_mat; 
        return dyn_d; 
    }
    if (out_material) *out_material = mat;
    return bd;
}

// ============================================================================
// BVH normal calculation via finite differences
// ============================================================================

inline Vec3 bvhNormal(const OntScene& sc, Vec3 p, f32 w) {
    const f32 eps = 0.001f;
    f32 d = bvhEval(sc, p, w);
    f32 dx = bvhEval(sc, {p.x + eps, p.y, p.z}, w);
    f32 dy = bvhEval(sc, {p.x, p.y + eps, p.z}, w);
    f32 dz = bvhEval(sc, {p.x, p.y, p.z + eps}, w);
    return normalize({dx - d, dy - d, dz - d});
}

// ============================================================================
// Shading: PBR Cook-Torrance (GGX + Schlick + Smith)
// ============================================================================

// Multi-light PBR shade (used when .obs provides lights)
inline Vec3 shadeOnt(const OntScene& sc, Vec3 p, Vec3 n, Vec3 view_dir,
                      u32 mat_id, const PipelineConfig& pl, f32 ao,
                      const Light* lights, u32 light_count) {
    Vec3 result = {0,0,0};
    if (mat_id >= sc.header->material_count) return result;

    const auto& mat = sc.materials[mat_id];
    Vec3 albedo = {mat.base_color[0], mat.base_color[1], mat.base_color[2]};
    f32 roughness = std::fmax(mat.roughness, 0.001f);
    f32 metallic = std::fmin(std::fmax(mat.metallic, 0.0f), 1.0f);

    // Procedural detail texture (skipped when terrain_blend_strength == 0)
    f32 detail = 0.5f;
    if (pl.terrain_blend_strength > 0.0f) {
        detail = detailNoise(p, 2.0f);
        f32 h = p.y;
        f32 snow_w = std::fmin(std::fmax((h - 1.5f) * 2.0f, 0.0f), 1.0f);
        f32 rock_w = std::fmin(std::fmax((h - 0.0f) * 1.5f, 0.0f), 1.0f) * (1.0f - snow_w);
        f32 grass_w = std::fmin(std::fmax((h + 0.5f) * 1.5f, 0.0f), 1.0f) * (1.0f - rock_w - snow_w);
        f32 dirt_w = 1.0f - snow_w - rock_w - grass_w;
        Vec3 col_snow  = {0.95f, 0.97f, 1.0f};
        Vec3 col_rock  = {0.5f, 0.45f, 0.35f};
        Vec3 col_grass = {0.2f, 0.5f, 0.1f};
        Vec3 col_dirt  = {0.4f, 0.3f, 0.15f};
        Vec3 terrain_col = col_snow * snow_w + col_rock * rock_w + col_grass * grass_w + col_dirt * dirt_w;
        f32 terrain_blend = (1.0f - metallic) * pl.terrain_blend_strength;
        albedo.x = lerp(albedo.x, terrain_col.x * (0.7f + 0.3f * detail), terrain_blend);
        albedo.y = lerp(albedo.y, terrain_col.y * (0.7f + 0.3f * detail), terrain_blend);
        albedo.z = lerp(albedo.z, terrain_col.z * (0.7f + 0.3f * detail), terrain_blend);
    }

    // F0
    Vec3 F0 = {lerp(0.04f, albedo.x, metallic),
               lerp(0.04f, albedo.y, metallic),
               lerp(0.04f, albedo.z, metallic)};

    // Accumulate light contributions
    Vec3 diffuse_sum = {0,0,0}, spec_sum = {0,0,0};

    if (light_count == 0) {
        light_count = 1;
        static Light default_light;
        default_light.type = LightType::DIRECTIONAL;
        default_light.direction = {0.6f, -0.6f, 0.4f};
        default_light.color = {1.0f, 0.92f, 0.8f};
        default_light.intensity = 1.0f;
        lights = &default_light;
    }

    for (u32 li = 0; li < light_count; li++) {
        const Light& l = lights[li];
        Vec3 ldir;
        f32 atten = 1.0f;
        if (l.type == LightType::DIRECTIONAL) {
            ldir = normalize(l.direction);
        } else {
            ldir = normalize(l.position - p);
            f32 dist = length(l.position - p);
            atten = 1.0f / (1.0f + l.falloff * dist * dist);
        }
        Vec3 light_col = {l.color.x * l.intensity * atten,
                          l.color.y * l.intensity * atten,
                          l.color.z * l.intensity * atten};
        f32 ndotl = std::fmax(dot(n, ldir), 0.0f);
        if (ndotl <= 0.0f) continue;

        Vec3 half = normalize(ldir + view_dir);
        f32 ndotv = std::fmax(dot(n, view_dir), 0.001f);
        f32 ndoth = std::fmax(dot(n, half), 0.001f);
        f32 hdotv = std::fmax(dot(half, view_dir), 0.001f);

        // GGX NDF
        f32 a = roughness * roughness;
        f32 a2 = a * a;
        f32 ndoth2 = ndoth * ndoth;
        f32 denom = ndoth2 * (a2 - 1.0f) + 1.0f;
        f32 ndf = a2 / (3.14159265f * denom * denom);

        // Schlick-GGX Geometry
        f32 k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
        f32 G1 = ndotl / (ndotl * (1.0f - k) + k);
        f32 G2 = ndotv / (ndotv * (1.0f - k) + k);
        f32 G = G1 * G2;

        // Fresnel
        f32 Fc = std::pow(1.0f - hdotv, 5.0f);
        Vec3 F = {F0.x + (1.0f - F0.x) * Fc,
                  F0.y + (1.0f - F0.y) * Fc,
                  F0.z + (1.0f - F0.z) * Fc};

        // Specular
        f32 spec_denom = 4.0f * ndotv * ndotl + 0.0001f;
        Vec3 specular = {ndf * G * F.x / spec_denom,
                         ndf * G * F.y / spec_denom,
                         ndf * G * F.z / spec_denom};

        // Diffuse
        Vec3 kD = {(1.0f - F.x) * (1.0f - metallic),
                   (1.0f - F.y) * (1.0f - metallic),
                   (1.0f - F.z) * (1.0f - metallic)};
        Vec3 diffuse = {albedo.x * kD.x * ndotl / 3.14159265f,
                        albedo.y * kD.y * ndotl / 3.14159265f,
                        albedo.z * kD.z * ndotl / 3.14159265f};

        diffuse_sum.x += diffuse.x * light_col.x;
        diffuse_sum.y += diffuse.y * light_col.y;
        diffuse_sum.z += diffuse.z * light_col.z;
        spec_sum.x += specular.x * light_col.x;
        spec_sum.y += specular.y * light_col.y;
        spec_sum.z += specular.z * light_col.z;
    }

    // Ambient hemisphere
    f32 ambient_up = 0.5f + 0.5f * n.y;
    f32 ambient_down = 0.5f - 0.5f * n.y;
    Vec3 sky_col = {0.4f, 0.6f, 0.9f};
    Vec3 ground_col = {0.15f, 0.1f, 0.05f};
    Vec3 ambient_col = {
        (sky_col.x * ambient_up + ground_col.x * ambient_down) * albedo.x * pl.shade_ambient * (1.0f - metallic),
        (sky_col.y * ambient_up + ground_col.y * ambient_down) * albedo.y * pl.shade_ambient * (1.0f - metallic),
        (sky_col.z * ambient_up + ground_col.z * ambient_down) * albedo.z * pl.shade_ambient * (1.0f - metallic)};

    result = (diffuse_sum + spec_sum) * pl.shade_specular * ao + ambient_col;

    // Emission
    result.x += mat.emission[0];
    result.y += mat.emission[1];
    result.z += mat.emission[2];

    return result;
}

inline Vec3 shadeOnt(const OntScene& sc, Vec3 p, Vec3 n, Vec3 view_dir,
                      u32 mat_id, const PipelineConfig& pl, f32 ao = 1.0f) {
    Vec3 result = {0,0,0};
    if (mat_id >= sc.header->material_count) return result;

    const auto& mat = sc.materials[mat_id];
    Vec3 albedo = {mat.base_color[0], mat.base_color[1], mat.base_color[2]};
    f32 roughness = std::fmax(mat.roughness, 0.001f);
    f32 metallic = std::fmin(std::fmax(mat.metallic, 0.0f), 1.0f);

    // Procedural detail texture (skipped when terrain_blend_strength == 0)
    f32 detail = 0.5f;
    if (pl.terrain_blend_strength > 0.0f) {
        detail = detailNoise(p, 2.0f);
        f32 h = p.y;
        f32 snow_w = std::fmin(std::fmax((h - 1.5f) * 2.0f, 0.0f), 1.0f);
        f32 rock_w = std::fmin(std::fmax((h - 0.0f) * 1.5f, 0.0f), 1.0f) * (1.0f - snow_w);
        f32 grass_w = std::fmin(std::fmax((h + 0.5f) * 1.5f, 0.0f), 1.0f) * (1.0f - rock_w - snow_w);
        f32 dirt_w = 1.0f - snow_w - rock_w - grass_w;
        Vec3 col_snow  = {0.95f, 0.97f, 1.0f};
        Vec3 col_rock  = {0.5f, 0.45f, 0.35f};
        Vec3 col_grass = {0.2f, 0.5f, 0.1f};
        Vec3 col_dirt  = {0.4f, 0.3f, 0.15f};
        Vec3 terrain_col = col_snow * snow_w + col_rock * rock_w + col_grass * grass_w + col_dirt * dirt_w;
        f32 terrain_blend = (1.0f - metallic) * pl.terrain_blend_strength;
        albedo.x = lerp(albedo.x, terrain_col.x * (0.7f + 0.3f * detail), terrain_blend);
        albedo.y = lerp(albedo.y, terrain_col.y * (0.7f + 0.3f * detail), terrain_blend);
        albedo.z = lerp(albedo.z, terrain_col.z * (0.7f + 0.3f * detail), terrain_blend);
    }

    // Directional light (warm sun)
    Vec3 ldir = normalize({0.6f, -0.6f, 0.4f});
    Vec3 light_col = {1.0f, 0.92f, 0.8f};
    Vec3 half = normalize(ldir + view_dir);
    Vec3 vl = view_dir;

    f32 ndotl = std::fmax(dot(n, ldir), 0.0f);
    f32 ndotv = std::fmax(dot(n, vl), 0.001f);
    f32 ndoth = std::fmax(dot(n, half), 0.001f);
    f32 hdotv = std::fmax(dot(half, vl), 0.001f);

    // F0 — reflectance at normal incidence (dielectric 0.04, metal = albedo)
    Vec3 F0 = {lerp(0.04f, albedo.x, metallic),
               lerp(0.04f, albedo.y, metallic),
               lerp(0.04f, albedo.z, metallic)};

    // --- GGX Normal Distribution Function ---
    f32 a = roughness * roughness;
    f32 a2 = a * a;
    f32 ndoth2 = ndoth * ndoth;
    f32 denom = ndoth2 * (a2 - 1.0f) + 1.0f;
    f32 ndf = a2 / (3.14159265f * denom * denom);

    // --- Schlick-GGX Geometry (Smith) ---
    f32 k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    f32 G1 = ndotl / (ndotl * (1.0f - k) + k);
    f32 G2 = ndotv / (ndotv * (1.0f - k) + k);
    f32 G = G1 * G2;

    // --- Schlick Fresnel ---
    f32 Fc = std::pow(1.0f - hdotv, 5.0f);
    Vec3 F = {F0.x + (1.0f - F0.x) * Fc,
              F0.y + (1.0f - F0.y) * Fc,
              F0.z + (1.0f - F0.z) * Fc};

    // --- Specular (Cook-Torrance BRDF) ---
    f32 spec_denom = 4.0f * ndotv * ndotl + 0.0001f;
    Vec3 specular = {ndf * G * F.x / spec_denom,
                     ndf * G * F.y / spec_denom,
                     ndf * G * F.z / spec_denom};

    // --- Diffuse (Lambert modulated by 1-F and 1-metallic) ---
    Vec3 kD = {(1.0f - F.x) * (1.0f - metallic),
               (1.0f - F.y) * (1.0f - metallic),
               (1.0f - F.z) * (1.0f - metallic)};
    Vec3 diffuse = {albedo.x * kD.x * ndotl / 3.14159265f,
                    albedo.y * kD.y * ndotl / 3.14159265f,
                    albedo.z * kD.z * ndotl / 3.14159265f};

    // --- Ambient hemisphere (sky blue above, warm brown below) ---
    f32 ambient_up = 0.5f + 0.5f * n.y;
    f32 ambient_down = 0.5f - 0.5f * n.y;
    Vec3 sky_col = {0.4f, 0.6f, 0.9f};
    Vec3 ground_col = {0.15f, 0.1f, 0.05f};
    Vec3 ambient_col = {
        (sky_col.x * ambient_up + ground_col.x * ambient_down) * albedo.x * pl.shade_ambient * (1.0f - metallic),
        (sky_col.y * ambient_up + ground_col.y * ambient_down) * albedo.y * pl.shade_ambient * (1.0f - metallic),
        (sky_col.z * ambient_up + ground_col.z * ambient_down) * albedo.z * pl.shade_ambient * (1.0f - metallic)};

    // Combine (multiply by light color for warm sun)
    Vec3 diffuse_light = {diffuse.x * light_col.x, diffuse.y * light_col.y, diffuse.z * light_col.z};
    Vec3 spec_light = {specular.x * light_col.x, specular.y * light_col.y, specular.z * light_col.z};
    result = (diffuse_light + spec_light) * pl.shade_specular * ao + ambient_col;

    // Emission
    result.x += mat.emission[0];
    result.y += mat.emission[1];
    result.z += mat.emission[2];

    return result;
}

// ============================================================================
// SDF Ambient Occlusion
// ============================================================================

inline f32 sdfAO(const OntScene& sc, Vec3 p, Vec3 n, f32 w) {
    f32 occlusion = 0.0f;
    f32 step = 0.02f;
    for (int i = 1; i <= 4; i++) {
        f32 d = (f32)i * step;
        f32 dist = bvhEval(sc, p + n * d, w);
        occlusion += (d - dist) / d;
    }
    return 1.0f - std::fmin(std::fmax(occlusion, 0.0f), 1.0f);
}

// ============================================================================
// Ray marching with macro-stepping (d_min / L)
// ============================================================================

struct MarchResult {
    bool hit = false;
    Vec3 p = {0,0,0};
    Vec3 n = {0,1,0};
    u32 material = 0xFFFFFFFF;
    f32 t = 0.0f;
};

inline MarchResult rayMarchOnt(const OntScene& sc, Ray ray, f32 w,
                                const PipelineConfig& pl,
                                const BrickMap* bm = nullptr) {
    MarchResult r;
    f32 t = 0.0f;
    const auto* bvh = sc.bvh_nodes;
    u32 bvh_count = sc.header->bvh_count;

    // Ray-AABB intersection to skip empty space before the scene
    if (bvh_count > 0) {
        const auto& root = bvh[0];
        f32 tmin = 0.0f, tmax = pl.trace_t_max;
        for (int axis = 0; axis < 3; axis++) {
            f32 origin = (&ray.origin.x)[axis];
            f32 dir = (&ray.dir.x)[axis];
            f32 bmin = root.min[axis];
            f32 bmax = root.max[axis];
            if (std::abs(dir) < 1e-8f) {
                if (origin < bmin || origin > bmax) { t = tmax + 1; break; }
            } else {
                f32 inv_dir = 1.0f / dir;
                f32 t1 = (bmin - origin) * inv_dir;
                f32 t2 = (bmax - origin) * inv_dir;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) { t = tmax + 1; break; }
            }
        }
        t = std::fmax(tmin, pl.trace_t_min);
    } else {
        t = pl.trace_t_min;
    }

    for (u32 step = 0; step < (u32)pl.trace_max_steps; step++) {
        Vec3 p = ray.origin + ray.dir * t;
        f32 d;
        if (bm && bm->valid)
            d = evalBrickHybrid(sc, p, w, *bm, &r.material, pl.trace_hit_threshold);
        else
            d = bvhEval(sc, p, w, &r.material);
        f32 eps = pl.trace_hit_threshold * (1.0f + t * 0.01f);
        if (std::abs(d) < eps) {
            r.hit = true;
            r.p = p;
            r.t = t;
            r.n = bvhNormal(sc, p, w);
            break;
        }
        t += d * 0.8f;
        if (t > pl.trace_t_max) break;
    }
    return r;
}

// ============================================================================
// Full render for .ont scenes
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
        // GDI BI_RGB 32-bit DIB expects B, G, R, A byte order (little-endian)
        return ((u32)std::fmin(std::fmax(b*255,0),255)) |
               (((u32)std::fmin(std::fmax(g*255,0),255)) << 8) |
               (((u32)std::fmin(std::fmax(r*255,0),255)) << 16) |
               (((u32)std::fmin(std::fmax(a*255,0),255)) << 24);
    }
};

inline void applyPost(Vec3& c, const PipelineConfig& pl) {
    c.x *= pl.post_exposure;
    c.y *= pl.post_exposure;
    c.z *= pl.post_exposure;
    if (pl.post_tonemap) {
        c.x = c.x / (1.0f + c.x);
        c.y = c.y / (1.0f + c.y);
        c.z = c.z / (1.0f + c.z);
    }
    if (pl.post_gamma) {
        f32 inv = 1.0f / 2.2f;
        c.x = std::pow(std::fmax(c.x, 0.0f), inv);
        c.y = std::pow(std::fmax(c.y, 0.0f), inv);
        c.z = std::pow(std::fmax(c.z, 0.0f), inv);
    }
}

inline void renderOntPixel(const OntScene& sc, Frame& fb, f32 w,
                           const Light* lights, u32 light_count, Vec3 bg_color,
                           const PipelineConfig& pl, int x, int y,
                           const BrickMap* bm = nullptr) {
    Ray ray;
    ray.origin = sc.camera.position;
    ray.dir = getRayDir(sc.camera, (f32)x, (f32)y, (f32)fb.width, (f32)fb.height);

    auto mr = rayMarchOnt(sc, ray, w, pl, bm);

    Vec3 c;
    if (mr.hit) {
        Vec3 view_dir = normalize(sc.camera.position - mr.p);
        f32 ao = sdfAO(sc, mr.p, mr.n, w);
        if (lights) {
            c = shadeOnt(sc, mr.p, mr.n, view_dir, mr.material, pl, ao, lights, light_count);
        } else {
            c = shadeOnt(sc, mr.p, mr.n, view_dir, mr.material, pl, ao);
        }
    } else {
        Vec3 sky_bottom = {bg_color.x * 0.3f, bg_color.y * 0.3f, bg_color.z * 0.3f};
        Vec3 sky_horizon = bg_color;
        Vec3 sky_top = {bg_color.x * 1.5f, bg_color.y * 1.5f, bg_color.z * 1.5f};
        f32 sky_t = 0.5f + 0.5f * ray.dir.y;
        if (ray.dir.y > 0.0f) {
            c.x = lerp(sky_horizon.x, sky_top.x, sky_t * 2.0f);
            c.y = lerp(sky_horizon.y, sky_top.y, sky_t * 2.0f);
            c.z = lerp(sky_horizon.z, sky_top.z, sky_t * 2.0f);
        } else {
            c = sky_bottom;
        }
    }
    applyPost(c, pl);
    fb.pixels[y * fb.width + x] = Frame::toRgba(c.x, c.y, c.z);
}

inline void renderOntScene(const OntScene& sc, Frame& fb, f32 w,
                            const BrickMap* bm = nullptr) {
    const auto& pl = sc.pipeline;

    const Light* lights = nullptr;
    u32 light_count = 0;
    if (sc.has_obs && !sc.obs.lights.empty()) {
        lights = sc.obs.lights.data();
        light_count = (u32)sc.obs.lights.size();
    }

    Vec3 bg_color = sc.background;
    if (sc.has_obs && sc.obs.has_background) {
        bg_color = sc.obs.background;
    }

    for (int y = 0; y < fb.height; y++) {
        for (int x = 0; x < fb.width; x++) {
            renderOntPixel(sc, fb, w, lights, light_count, bg_color, pl, x, y, bm);
        }
    }
}

inline void renderOntSceneMT(const OntScene& sc, Frame& fb, f32 w,
                              int thread_count = 0, const BrickMap* bm = nullptr) {
    const auto& pl = sc.pipeline;

    const Light* lights = nullptr;
    u32 light_count = 0;
    if (sc.has_obs && !sc.obs.lights.empty()) {
        lights = sc.obs.lights.data();
        light_count = (u32)sc.obs.lights.size();
    }

    Vec3 bg_color = sc.background;
    if (sc.has_obs && sc.obs.has_background) {
        bg_color = sc.obs.background;
    }

    if (thread_count <= 0) {
        SYSTEM_INFO sys;
        GetSystemInfo(&sys);
        thread_count = (int)sys.dwNumberOfProcessors;
    }
    if (thread_count > fb.height) thread_count = fb.height;
    if (thread_count <= 1 || fb.height < 32) {
        // Single-thread fallback
        for (int y = 0; y < fb.height; y++)
            for (int x = 0; x < fb.width; x++)
                renderOntPixel(sc, fb, w, lights, light_count, bg_color, pl, x, y, bm);
        return;
    }

    int rows_per = fb.height / thread_count;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int i = 0; i < thread_count; i++) {
        int y0 = i * rows_per;
        int y1 = (i == thread_count - 1) ? fb.height : y0 + rows_per;
        threads.emplace_back([&sc, &fb, w, lights, light_count, bg_color, &pl, y0, y1, bm]() {
            for (int y = y0; y < y1; y++)
                for (int x = 0; x < fb.width; x++)
                    renderOntPixel(sc, fb, w, lights, light_count, bg_color, pl, x, y, bm);
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

// ============================================================================
// OLD CODE BELOW — kept for backward compat
// ============================================================================

inline Vec3 shade(const Scene& scene, Vec3 p, Vec3 n, Vec3 view_dir, u32 mat_id, f32 w,
                  const PipelineConfig& pl, Vec3 dynamic_color = {-1,-1,-1}) {
    Vec3 result = Vec3{pl.shade_ambient, pl.shade_ambient, pl.shade_ambient} * 0.5f;
    if (mat_id >= scene.materials.size()) return result;

    const Material& mat = scene.materials[mat_id];
    Vec3 col = (dynamic_color.x >= 0) ? dynamic_color : mat.base_color;

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
        Vec3 half = normalize(ldir + view_dir);
        f32 ndoth = std::fmax(dot(n, half), 0.0f);
        f32 spec = std::pow(ndoth, pl.shade_spec_power);

        Vec3 base = col;

        Vec3 diff = base * ndotl * pl.shade_diffuse;
        Vec3 specc = Vec3{spec, spec, spec} * pl.shade_specular;

        Vec3 contrib = (diff + specc) * inten * atten;
        result.x += contrib.x;
        result.y += contrib.y;
        result.z += contrib.z;
    }

    result.x += mat.emission.x;
    result.y += mat.emission.y;
    result.z += mat.emission.z;
    return result;
}

inline f32 shadowMarch(const Scene& scene, Vec3 ro, Vec3 ldir, f32 w, f32 t_max,
                        const PipelineConfig& pl,
                        const f32* transforms = nullptr,
                        const Aabb* aabbs = nullptr,
                        const u32* visible_nodes = nullptr,
                        u32 visible_count = 0) {
    if (!pl.shadow_enabled) return 1.0f;
    f32 t = pl.trace_t_min;
    f32 result = 1.0f;
    f32 step = t_max / (f32)pl.shadow_steps;
    for (int i = 0; i < pl.shadow_steps && t < t_max; i++) {
        Vec3 p = ro + ldir * t;
        f32 d = evalScene(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
        if (d < pl.trace_hit_threshold) return 0.0f;
        result = std::fmin(result, pl.shadow_max_dist * d / t);
        t += std::fmax(d, step * 0.5f);
    }
    return result;
}

struct MarchResultOld {
    bool hit = false;
    Vec3 p = {0,0,0};
    Vec3 n = {0,1,0};
    u32 material = 0xFFFFFFFF;
    f32 t = 0.0f;
    Vec3 dynamic_color = {-1, -1, -1};
};

inline MarchResultOld rayMarch(const Scene& scene, Ray ray, f32 w,
                             const PipelineConfig& pl,
                             const f32* transforms = nullptr,
                             const Aabb* aabbs = nullptr,
                             const u32* visible_nodes = nullptr,
                             u32 visible_count = 0) {
    MarchResultOld r;
    f32 t = pl.trace_t_min;
    for (u32 step = 0; step < (u32)pl.trace_max_steps; step++) {
        Vec3 p = ray.origin + ray.dir * t;
        f32 d = evalScene(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
        f32 eps = pl.trace_hit_threshold * (1.0f + t * 0.01f);
        if (d < eps && d >= 0.0f) {
            r.hit = true;
            r.p = p;
            r.t = t;
            r.n = calcNormal(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
            u32 node_idx = findClosestNode(scene, p, w, transforms, aabbs, visible_nodes, visible_count);
            if (node_idx < scene.nodes.size()) {
                const Node& hit_node = scene.nodes[node_idx];
                r.material = hit_node.material_id;
                bool has_dynamic = hit_node.material_expr[0].size() | hit_node.material_expr[1].size() | hit_node.material_expr[2].size();
                if (has_dynamic) {
                    Vec3 c;
                    c.x = hit_node.material_expr[0].empty() ? 0 : evalExpr(hit_node.material_expr[0], p.x, p.y, p.z, w);
                    c.y = hit_node.material_expr[1].empty() ? 0 : evalExpr(hit_node.material_expr[1], p.x, p.y, p.z, w);
                    c.z = hit_node.material_expr[2].empty() ? 0 : evalExpr(hit_node.material_expr[2], p.x, p.y, p.z, w);
                    r.dynamic_color = c;
                }
            }
            break;
        }
        f32 sd = (d < 0.0f) ? -d : d;
        t += std::fmax(sd * 1.0f, pl.trace_hit_threshold * 0.5f);
        if (t > pl.trace_t_max) break;
    }
    return r;
}

struct SceneQuery {
    const Aabb* aabbs = nullptr;
    u32 (*query_fn)(void* ctx, const Vec3& ro, const Vec3& rd,
                    u32* out, u32 max) = nullptr;
    void* query_ctx = nullptr;

    u32 query(const Vec3& ro, const Vec3& rd, u32* out, u32 max) const {
        return query_fn ? query_fn(query_ctx, ro, rd, out, max) : 0;
    }
};

inline void renderScene(const Scene& scene, Frame& fb, f32 w,
                         const f32* transforms = nullptr,
                         const Aabb* aabbs = nullptr,
                         const u32* visible_nodes = nullptr,
                         u32 visible_count = 0) {
    const auto& pl = scene.pipeline;
    for (int y = 0; y < fb.height; y++) {
        for (int x = 0; x < fb.width; x++) {
            Ray ray;
            ray.origin = scene.camera.position;
            ray.dir = getRayDir(scene.camera, (f32)x, (f32)y, (f32)fb.width, (f32)fb.height);

            auto mr = rayMarch(scene, ray, w, pl, transforms,
                               aabbs, visible_nodes, visible_count);

            Vec3 bg = scene.background;
            Vec3 c = bg;
            if (mr.hit) {
                Vec3 view_dir = normalize(scene.camera.position - mr.p);
                c = shade(scene, mr.p, mr.n, view_dir, mr.material, w, pl, mr.dynamic_color);
                if (pl.shadow_enabled && !scene.lights.empty()) {
                    Vec3 ldir = normalize(scene.lights[0].direction);
                    f32 shad = shadowMarch(scene, mr.p + mr.n * pl.shadow_bias, ldir, w,
                                           pl.trace_t_max, pl, transforms,
                                           aabbs, visible_nodes, visible_count);
                    c.x *= shad;
                    c.y *= shad;
                    c.z *= shad;
                }
            }
            applyPost(c, pl);
            fb.pixels[y * fb.width + x] = Frame::toRgba(c.x, c.y, c.z);
        }
    }
}

inline void renderScene(const Scene& scene, Frame& fb, f32 w,
                         const f32* transforms,
                         const SceneQuery& sq) {
    const auto& pl = scene.pipeline;
    for (int y = 0; y < fb.height; y++) {
        for (int x = 0; x < fb.width; x++) {
            Ray ray;
            ray.origin = scene.camera.position;
            ray.dir = getRayDir(scene.camera, (f32)x, (f32)y, (f32)fb.width, (f32)fb.height);

            u32 visible[256];
            u32 visible_count = sq.query(ray.origin, ray.dir, visible, 256);

            auto mr = rayMarch(scene, ray, w, pl, transforms,
                               sq.aabbs, visible, visible_count);

            Vec3 bg = scene.background;
            Vec3 c = bg;
            if (mr.hit) {
                Vec3 view_dir = normalize(scene.camera.position - mr.p);
                c = shade(scene, mr.p, mr.n, view_dir, mr.material, w, pl, mr.dynamic_color);
                if (pl.shadow_enabled && !scene.lights.empty()) {
                    Vec3 ldir = normalize(scene.lights[0].direction);
                    f32 shad = shadowMarch(scene, mr.p + mr.n * pl.shadow_bias, ldir, w,
                                           pl.trace_t_max, pl, transforms,
                                           sq.aabbs, visible, visible_count);
                    c.x *= shad;
                    c.y *= shad;
                    c.z *= shad;
                }
            }
            applyPost(c, pl);
            fb.pixels[y * fb.width + x] = Frame::toRgba(c.x, c.y, c.z);
        }
    }
}

} // namespace mg
