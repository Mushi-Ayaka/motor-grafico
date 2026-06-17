#pragma once
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include "ray_march.h"

namespace mg {

// ============================================================================
// SSE execBcRaw4 — 4-wide SIMD bytecode VM
// Evaluates same bytecode for 4 points simultaneously
// ============================================================================

inline void execBcRaw4(const uint8_t* bc, size_t bc_len,
                       const f32 x[4], const f32 y[4], const f32 z[4], f32 w,
                       f32 result[4], int depth = 0,
                       const f32 nx[4] = nullptr, const f32 ny[4] = nullptr, const f32 nz[4] = nullptr,
                       const f32 vx[4] = nullptr, const f32 vy[4] = nullptr, const f32 vz[4] = nullptr,
                       const f32 lx[4] = nullptr, const f32 ly[4] = nullptr, const f32 lz[4] = nullptr) {
    if (!bc || bc_len == 0) {
        for (int i = 0; i < 4; i++) result[i] = 0.0f;
        return;
    }

    __m128 stack[128];
    int sp = -1;

    __m128 xs = _mm_loadu_ps(x);
    __m128 ys = _mm_loadu_ps(y);
    __m128 zs = _mm_loadu_ps(z);
    __m128 ws = _mm_set1_ps(w);

    __m128 nxs = nx ? _mm_loadu_ps(nx) : _mm_setzero_ps();
    __m128 nys = ny ? _mm_loadu_ps(ny) : _mm_setzero_ps();
    __m128 nzs = nz ? _mm_loadu_ps(nz) : _mm_setzero_ps();
    __m128 vxs = vx ? _mm_loadu_ps(vx) : _mm_setzero_ps();
    __m128 vys = vy ? _mm_loadu_ps(vy) : _mm_setzero_ps();
    __m128 vzs = vz ? _mm_loadu_ps(vz) : _mm_setzero_ps();
    __m128 lxs = lx ? _mm_loadu_ps(lx) : _mm_setzero_ps();
    __m128 lys = ly ? _mm_loadu_ps(ly) : _mm_setzero_ps();
    __m128 lzs = lz ? _mm_loadu_ps(lz) : _mm_setzero_ps();

    __m128 sign_bit = _mm_set1_ps(-0.0f);
    __m128 zero = _mm_setzero_ps();

    auto pop = [&]() -> __m128 { return sp >= 0 ? stack[sp--] : zero; };
    auto push = [&](__m128 v) { if (sp < 126) stack[++sp] = v; };

    size_t pc = 0;
    while (pc < bc_len) {
        OntOpcode op = (OntOpcode)bc[pc];
        f32 c;
        if (op == ONT_CONST) {
            memcpy(&c, bc + pc + 1, sizeof(f32));
        } else {
            c = 0;
        }
        pc += 5;

        switch (op) {
            case ONT_CONST:   push(_mm_set1_ps(c)); break;

            case ONT_VAR_X: push(xs); break;
            case ONT_VAR_Y: push(ys); break;
            case ONT_VAR_Z: push(zs); break;
            case ONT_VAR_W: push(ws); break;

            case ONT_VAR_NX: push(nxs); break;
            case ONT_VAR_NY: push(nys); break;
            case ONT_VAR_NZ: push(nzs); break;
            case ONT_VAR_VX: push(vxs); break;
            case ONT_VAR_VY: push(vys); break;
            case ONT_VAR_VZ: push(vzs); break;
            case ONT_VAR_LX: push(lxs); break;
            case ONT_VAR_LY: push(lys); break;
            case ONT_VAR_LZ: push(lzs); break;

            case ONT_ADD: {
                __m128 b = pop(), a = pop();
                push(_mm_add_ps(a, b));
            } break;

            case ONT_SUB: {
                __m128 b = pop(), a = pop();
                push(_mm_sub_ps(a, b));
            } break;

            case ONT_MUL: {
                __m128 b = pop(), a = pop();
                push(_mm_mul_ps(a, b));
            } break;

            case ONT_DIV: {
                __m128 b = pop(), a = pop();
                __m128 d = _mm_div_ps(a, b);
                __m128 m = _mm_cmpeq_ps(b, zero);
                push(_mm_andnot_ps(m, d));
            } break;

            case ONT_MOD: {
                __m128 b = pop(), a = pop();
                f32 av[4], bv[4];
                _mm_storeu_ps(av, a); _mm_storeu_ps(bv, b);
                for (int i = 0; i < 4; i++) av[i] = bv[i] != 0 ? std::fmod(av[i], bv[i]) : 0;
                push(_mm_loadu_ps(av));
            } break;

            case ONT_NEG: {
                push(_mm_xor_ps(pop(), sign_bit));
            } break;

            case ONT_POW: {
                __m128 b = pop(), a = pop();
                f32 av[4], bv[4];
                _mm_storeu_ps(av, a); _mm_storeu_ps(bv, b);
                for (int i = 0; i < 4; i++) av[i] = std::pow(av[i], bv[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_SIN: {
                __m128 a = pop();
                f32 av[4]; _mm_storeu_ps(av, a);
                for (int i = 0; i < 4; i++) av[i] = std::sin(av[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_COS: {
                __m128 a = pop();
                f32 av[4]; _mm_storeu_ps(av, a);
                for (int i = 0; i < 4; i++) av[i] = std::cos(av[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_TAN: {
                __m128 a = pop();
                f32 av[4]; _mm_storeu_ps(av, a);
                for (int i = 0; i < 4; i++) av[i] = std::tan(av[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_ABS: {
                push(_mm_andnot_ps(sign_bit, pop()));
            } break;

            case ONT_SQRT: {
                push(_mm_sqrt_ps(pop()));
            } break;

            case ONT_FLOOR: {
                __m128 a = pop();
                f32 av[4]; _mm_storeu_ps(av, a);
                for (int i = 0; i < 4; i++) av[i] = std::floor(av[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_CEIL: {
                __m128 a = pop();
                f32 av[4]; _mm_storeu_ps(av, a);
                for (int i = 0; i < 4; i++) av[i] = std::ceil(av[i]);
                push(_mm_loadu_ps(av));
            } break;

            case ONT_MIN: {
                __m128 b = pop(), a = pop();
                push(_mm_min_ps(a, b));
            } break;

            case ONT_MAX: {
                __m128 b = pop(), a = pop();
                push(_mm_max_ps(a, b));
            } break;

            case ONT_CLAMP: {
                __m128 hi = pop(), lo = pop(), v = pop();
                push(_mm_max_ps(_mm_min_ps(v, hi), lo));
            } break;

            case ONT_LERP:
            case ONT_MIX: {
                __m128 t = pop(), b = pop(), a = pop();
                push(_mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), t)));
            } break;

            case ONT_SAMPLE: {
                __m128 dz = pop(), dy = pop(), dx = pop();
                if (depth >= 1) {
                    push(zero);
                    break;
                }
                f32 dxv[4], dyv[4], dzv[4], bx[4], by[4], bz[4];
                _mm_storeu_ps(dxv, dx); _mm_storeu_ps(dyv, dy); _mm_storeu_ps(dzv, dz);
                _mm_storeu_ps(bx, xs); _mm_storeu_ps(by, ys); _mm_storeu_ps(bz, zs);
                f32 nxv[4], nyv[4], nzv[4];
                for (int i = 0; i < 4; i++) {
                    nxv[i] = bx[i] + dxv[i];
                    nyv[i] = by[i] + dyv[i];
                    nzv[i] = bz[i] + dzv[i];
                }
                f32 sr[4];
                execBcRaw4(bc, bc_len, nxv, nyv, nzv, w, sr, depth + 1,
                          nx, ny, nz, vx, vy, vz, lx, ly, lz);
                push(_mm_loadu_ps(sr));
            } break;

            default: {
                __m128 r = sp >= 0 ? stack[sp] : zero;
                _mm_storeu_ps(result, r);
                return;
            }
        }
    }
    __m128 r = sp >= 0 ? stack[sp] : zero;
    _mm_storeu_ps(result, r);
}

// ============================================================================
// SSE matrix transform — apply column-major 4x4 to 4 points
// ============================================================================

inline void applyMatrix4(const float m[16],
                         __m128 xs, __m128 ys, __m128 zs,
                         __m128& ox, __m128& oy, __m128& oz) {
    ox = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[0]), xs),
                    _mm_mul_ps(_mm_set1_ps(m[4]), ys)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[8]), zs),
                    _mm_set1_ps(m[12])));
    oy = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[1]), xs),
                    _mm_mul_ps(_mm_set1_ps(m[5]), ys)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[9]), zs),
                    _mm_set1_ps(m[13])));
    oz = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[2]), xs),
                    _mm_mul_ps(_mm_set1_ps(m[6]), ys)),
        _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[10]), zs),
                    _mm_set1_ps(m[14])));
}

// ============================================================================
// bvhEval4 — 4-point packet BVH traversal with SIMD leaf evaluation
// Traces 4 points through the BVH together, tracking per-point node indices.
// At each leaf, evaluates graph nodes for all 4 points using execBcRaw4,
// and updates results only for points that are inside the leaf's AABB.
// ============================================================================

inline void bvhEval4(const OntScene& sc,
                     f32 px[4], f32 py[4], f32 pz[4], f32 w,
                     f32 result[4], u32 material[4],
                     const f32* initial_dist = nullptr,
                     const bool* ray_mask = nullptr) {
    __m128 res = initial_dist ? _mm_loadu_ps(initial_dist) : _mm_set1_ps(1e9f);
    __m128i best_mat = _mm_set1_epi32(0xFFFFFFFF);
    __m128 xs = _mm_loadu_ps(px);
    __m128 ys = _mm_loadu_ps(py);
    __m128 zs = _mm_loadu_ps(pz);
    __m128 zero = _mm_setzero_ps();

    const u32 bvh_count = sc.header->bvh_count;
    const auto* bvh = sc.bvh_nodes;
    const auto* gn = sc.graph_nodes;
    const auto* bc = sc.bytecode;

    u32 ni[4] = {0, 0, 0, 0};
    bool done[4] = {false, false, false, false};
    int remaining = 0;

    // Mask out inactive rays — prevents SIMD divergence from forcing full tree traversal
    if (ray_mask) {
        for (int i = 0; i < 4; i++) {
            done[i] = !ray_mask[i];
            if (done[i]) ni[i] = bvh_count;
            else remaining++;
        }
    } else {
        remaining = 4;
    }

    while (remaining > 0) {
        u32 min_ni = 0xFFFFFFFF;
        for (int i = 0; i < 4; i++)
            if (!done[i] && ni[i] < min_ni) min_ni = ni[i];
        if (min_ni >= bvh_count) break;

        const auto& node = bvh[min_ni];

        // Distance-to-AABB test: if the box is farther than current best distance, skip
        __m128 bmin_x = _mm_set1_ps(node.min[0]);
        __m128 bmin_y = _mm_set1_ps(node.min[1]);
        __m128 bmin_z = _mm_set1_ps(node.min[2]);
        __m128 bmax_x = _mm_set1_ps(node.max[0]);
        __m128 bmax_y = _mm_set1_ps(node.max[1]);
        __m128 bmax_z = _mm_set1_ps(node.max[2]);

        __m128 dx = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_x, xs), _mm_sub_ps(xs, bmax_x)));
        __m128 dy = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_y, ys), _mm_sub_ps(ys, bmax_y)));
        __m128 dz = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_z, zs), _mm_sub_ps(zs, bmax_z)));

        __m128 d_box_sq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy)), _mm_mul_ps(dz, dz));
        __m128 res_sq = _mm_mul_ps(res, res);
        __m128 inside = _mm_cmple_ps(d_box_sq, res_sq);

        // Zero out lanes not at this node
        for (int i = 0; i < 4; i++)
            if (ni[i] != min_ni) ((f32*)&inside)[i] = 0.0f;

        int mask = _mm_movemask_ps(inside);

        if (mask == 0) {
            for (int i = 0; i < 4; i++)
                if (ni[i] == min_ni) ni[i] = (u32)node.skip_index;
            continue;
        }

        if (node.flags & BVH_FLAG_LEAF) {
            for (uint16_t gn_idx = 0; gn_idx < node.node_count; gn_idx++) {
                u32 gi = node.first_node + gn_idx;
                if (gi >= sc.header->node_count) break;
                const auto& g = gn[gi];

                // SSE matrix transform for all 4 points
                __m128 lxs, lys, lzs;
                applyMatrix4(g.local_transform, xs, ys, zs, lxs, lys, lzs);

                f32 lx[4], ly[4], lz[4];
                _mm_storeu_ps(lx, lxs);
                _mm_storeu_ps(ly, lys);
                _mm_storeu_ps(lz, lzs);

                f32 d[4];
                if (!sc.jit_functions.empty() && sc.jit_functions[gi]) {
                    sc.jit_functions[gi](lx, ly, lz, w, d, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                } else {
                    execBcRaw4(bc + g.bytecode_offset, g.bytecode_length,
                              lx, ly, lz, w, d);
                }
                // Scale compensation: divide local-space distance by local→world scale
                { f32 s = extractScale(g.local_transform); if (s > 1e-8f) { __m128 sf = _mm_set1_ps(1.0f / s); for (int _i = 0; _i < 4; _i++) d[_i] *= sf.m128_f32[_i]; } }

                __m128 dd = _mm_loadu_ps(d);
                __m128 cmp = _mm_cmplt_ps(dd, res);
                __m128 sel = _mm_and_ps(inside, cmp);

                res = _mm_or_ps(_mm_and_ps(sel, dd),
                               _mm_andnot_ps(sel, res));

                __m128i sel_i = _mm_castps_si128(sel);
                __m128i mv = _mm_set1_epi32((int)g.material_id);
                best_mat = _mm_or_si128(
                    _mm_and_si128(sel_i, mv),
                    _mm_andnot_si128(sel_i, best_mat));

                // Early exit: if all inside pixels are deep inside surface (res < -0.01),
                // skip remaining graph nodes — subsequent steps will converge anyway
                if (gn_idx + 1 < node.node_count) {
                    __m128 early_neg = _mm_cmplt_ps(res, _mm_set1_ps(-0.01f));
                    int early_mask = _mm_movemask_ps(_mm_and_ps(inside, early_neg));
                    if (early_mask == mask) break;
                }
            }

            for (int i = 0; i < 4; i++)
                if (ni[i] == min_ni) ni[i] = (u32)node.skip_index;

        } else {
            // Use d_min for subtree culling: if we already found a surface closer
            // than this node's minimum safe distance, skip entire subtree
            f32 cur_res[4];
            _mm_storeu_ps(cur_res, res);
            f32 safe_d_min = node.d_min > 0.005f ? 0.0f : node.d_min; // Defensive: degenerated d_min mitigation
            for (int i = 0; i < 4; i++) {
                if (ni[i] == min_ni) {
                    if (((f32*)&inside)[i] != 0.0f) {
                        if (cur_res[i] < safe_d_min)
                            ni[i] = (u32)node.skip_index;
                        else
                            ni[i]++;
                    } else {
                        ni[i] = (u32)node.skip_index;
                    }
                }
            }
        }

        // Update done flags
        remaining = 0;
        for (int i = 0; i < 4; i++) {
            if (ni[i] >= bvh_count) done[i] = true;
            if (!done[i]) remaining++;
        }
    }

    _mm_storeu_ps(result, res);
    _mm_storeu_si128((__m128i*)material, best_mat);
}

// ============================================================================
// brickSample4 — 4-wide brick trilinear sample (scalar batch, shared reads)
// ============================================================================

inline void brickSample4(const BrickMap& bm,
                         const f32 px[4], const f32 py[4], const f32 pz[4],
                         const bool active[4], f32 result[4]) {
    int ix[4], iy[4], iz[4];
    bool same_cell = true;
    int ref_lane = -1;
    bool oob[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        result[i] = 1e9f;
        if (!active[i]) continue;
        f32 gx = (px[i] - bm.origin.x) * bm.inv_top_cell_size;
        f32 gy = (py[i] - bm.origin.y) * bm.inv_top_cell_size;
        f32 gz = (pz[i] - bm.origin.z) * bm.inv_top_cell_size;
        ix[i] = (int)std::floor(gx);
        iy[i] = (int)std::floor(gy);
        iz[i] = (int)std::floor(gz);
        if (ix[i] < 0 || ix[i] >= (int)BrickMap::TOP_RES ||
            iy[i] < 0 || iy[i] >= (int)BrickMap::TOP_RES ||
            iz[i] < 0 || iz[i] >= (int)BrickMap::TOP_RES) {
            oob[i] = true;
            continue;
        }
        if (ref_lane < 0) {
            ref_lane = i;
        } else if (ix[i] != ix[ref_lane] || iy[i] != iy[ref_lane] || iz[i] != iz[ref_lane]) {
            same_cell = false;
        }
    }
    if (ref_lane < 0) return;

    if (same_cell) {
        u32 bi = bm.top_indices[iz[ref_lane] * BrickMap::TOP_RES * BrickMap::TOP_RES +
                                iy[ref_lane] * BrickMap::TOP_RES + ix[ref_lane]];
        if (bi == BrickMap::SENTINEL) return;
        const float* brick = bm.bricks + bi * BrickMap::BRICK_VOXELS;
        int r = (int)BrickMap::BRICK_RES;
        for (int i = 0; i < 4; i++) {
            if (!active[i] || oob[i]) continue;
            f32 gx = (px[i] - bm.origin.x) * bm.inv_top_cell_size;
            f32 gy = (py[i] - bm.origin.y) * bm.inv_top_cell_size;
            f32 gz = (pz[i] - bm.origin.z) * bm.inv_top_cell_size;
            f32 fx = (gx - ix[i]) * (f32)r;
            f32 fy = (gy - iy[i]) * (f32)r;
            f32 fz = (gz - iz[i]) * (f32)r;
            fx = std::fmax(0.0f, std::fmin(fx, (f32)(r - 2 + 0.999f)));
            fy = std::fmax(0.0f, std::fmin(fy, (f32)(r - 2 + 0.999f)));
            fz = std::fmax(0.0f, std::fmin(fz, (f32)(r - 2 + 0.999f)));
            int pi = (int)fx, pj = (int)fy, pk = (int)fz;
            f32 xf = fx - pi, yf = fy - pj, zf = fz - pk;
            f32 c000 = brick[pk*r*r + pj*r + pi];
            f32 c100 = brick[pk*r*r + pj*r + pi+1];
            f32 c010 = brick[pk*r*r + (pj+1)*r + pi];
            f32 c110 = brick[pk*r*r + (pj+1)*r + pi+1];
            f32 c001 = brick[(pk+1)*r*r + pj*r + pi];
            f32 c101 = brick[(pk+1)*r*r + pj*r + pi+1];
            f32 c011 = brick[(pk+1)*r*r + (pj+1)*r + pi];
            f32 c111 = brick[(pk+1)*r*r + (pj+1)*r + pi+1];
            result[i] = lerp(lerp(lerp(c000, c100, xf),
                                  lerp(c010, c110, xf), yf),
                             lerp(lerp(c001, c101, xf),
                                  lerp(c011, c111, xf), yf), zf);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (!active[i] || oob[i]) continue;
            result[i] = brickSample(bm, {px[i], py[i], pz[i]});
        }
    }
}

// ============================================================================
// evalBrickHybrid4 — 4-wide SDF eval: brick cache + SIMD BVH fallback + SIMD dyn nodes
// Uses bvhEval4 (SIMD) for near-surface lanes and execBcRaw4 (SIMD)
// for dynamic node evaluation in far lanes.
// ============================================================================

inline void evalBrickHybrid4(const OntScene& sc, f32 w,
                              const BrickMap& bm,
                              f32 px[4], f32 py[4], f32 pz[4],
                              const bool active[4], f32 result[4], u32 material[4],
                              f32 hit_eps = 0.001f,
                              BrickProfiler* prof = nullptr) {
    brickSample4(bm, px, py, pz, active, result);

    // Identify exactly which lanes need BVH fallback (near-surface)
    bool near_surface[4] = {false, false, false, false};
    bool any_bvh = false;
    for (int i = 0; i < 4; i++) {
        near_surface[i] = active[i] && result[i] < hit_eps * BRICK_MARGIN_MULT;
        if (near_surface[i]) any_bvh = true;
    }

    if (any_bvh) {
        f32 bvh_res[4];
        u32 bvh_mat[4];
        f32 max_search = hit_eps * BRICK_MARGIN_MULT * 2.0f;
        f32 search_dist[4];
        for (int i = 0; i < 4; i++)
            search_dist[i] = near_surface[i] ? max_search : 1e9f;
        bvhEval4(sc, px, py, pz, w, bvh_res, bvh_mat, search_dist, near_surface);
        for (int i = 0; i < 4; i++)
            if (near_surface[i]) { result[i] = bvh_res[i]; material[i] = bvh_mat[i]; }
    }
    
    if (bm.dyn_node_count > 0) {
        // All lanes far from surface — brick cached static + SIMD dynamic nodes only
        __m128 xs = _mm_loadu_ps(px);
        __m128 ys = _mm_loadu_ps(py);
        __m128 zs = _mm_loadu_ps(pz);
        __m128 zero = _mm_setzero_ps();
        for (u32 di = 0; di < bm.dyn_node_count; di++) {
            u32 gi = bm.dyn_node_indices[di];
            const auto& g = sc.graph_nodes[gi];
            
            // SIMD AABB check
            __m128 bmin_x = _mm_set1_ps(g.bbox_min[0]);
            __m128 bmin_y = _mm_set1_ps(g.bbox_min[1]);
            __m128 bmin_z = _mm_set1_ps(g.bbox_min[2]);
            __m128 bmax_x = _mm_set1_ps(g.bbox_max[0]);
            __m128 bmax_y = _mm_set1_ps(g.bbox_max[1]);
            __m128 bmax_z = _mm_set1_ps(g.bbox_max[2]);
            
            __m128 dx = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_x, xs), _mm_sub_ps(xs, bmax_x)));
            __m128 dy = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_y, ys), _mm_sub_ps(ys, bmax_y)));
            __m128 dz = _mm_max_ps(zero, _mm_max_ps(_mm_sub_ps(bmin_z, zs), _mm_sub_ps(zs, bmax_z)));
            
            __m128 d_box_sq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(dx, dx), _mm_mul_ps(dy, dy)), _mm_mul_ps(dz, dz));
            __m128 res_sq = _mm_mul_ps(_mm_loadu_ps(result), _mm_loadu_ps(result));
            __m128 inside = _mm_cmple_ps(d_box_sq, res_sq);
            
            int mask = _mm_movemask_ps(inside);
            if (mask == 0) continue; // All 4 points are too far from this dynamic node
            
            __m128 lxs, lys, lzs;
            applyMatrix4(g.local_transform, xs, ys, zs, lxs, lys, lzs);
            f32 lx[4], ly[4], lz[4], d[4];
            _mm_storeu_ps(lx, lxs); _mm_storeu_ps(ly, lys); _mm_storeu_ps(lz, lzs);
            if (!sc.jit_functions.empty() && sc.jit_functions[gi]) {
                sc.jit_functions[gi](lx, ly, lz, w, d, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            } else {
                execBcRaw4(sc.bytecode + g.bytecode_offset, g.bytecode_length,
                          lx, ly, lz, w, d);
            }
            // Scale compensation
            { f32 s = extractScale(g.local_transform); if (s > 1e-8f) { __m128 sf = _mm_set1_ps(1.0f / s); __m128 dd = _mm_loadu_ps(d); dd = _mm_mul_ps(dd, sf); _mm_storeu_ps(d, dd); } }
            for (int i = 0; i < 4; i++)
                if (active[i] && d[i] < result[i])
                    { result[i] = d[i]; material[i] = g.material_id; }
        }
    }
#ifdef BRICK_PROFILE
    if (prof) {
        for (int i = 0; i < 4; i++) {
            if (!active[i]) continue;
            prof->total_calls++;
            if (any_bvh) prof->bvh_calls++;
            else prof->brick_calls++;
        }
    }
#endif
}

// ============================================================================
// bvhNormal4 — SIMD batch normal computation
// Computes normals for 4 hit points using bvhEval4 (4 calls instead of 16)
// ============================================================================

inline void bvhNormal4(const OntScene& sc,
                        f32 px[4], f32 py[4], f32 pz[4], f32 w,
                        f32 nx[4], f32 ny[4], f32 nz[4],
                        const BrickMap* bm = nullptr) {
    const f32 eps = 0.001f;
    f32 d[4];
    u32 dummy_mat[4];
    bool active_all[4] = {true, true, true, true};

    auto eval4 = [&](f32* qx, f32* qy, f32* qz, f32* out) {
        if (bm && bm->valid) {
            evalBrickHybrid4(sc, w, *bm, qx, qy, qz, active_all, out, dummy_mat, 0.001f);
        } else {
            bvhEval4(sc, qx, qy, qz, w, out, dummy_mat);
        }
    };

    eval4(px, py, pz, d);

    f32 qx[4], qy[4], qz[4], dd[4];
    for (int i = 0; i < 4; i++) { qx[i] = px[i] + eps; qy[i] = py[i]; qz[i] = pz[i]; }
    eval4(qx, qy, qz, dd);
    for (int i = 0; i < 4; i++) nx[i] = dd[i] - d[i];

    for (int i = 0; i < 4; i++) { qx[i] = px[i]; qy[i] = py[i] + eps; qz[i] = pz[i]; }
    eval4(qx, qy, qz, dd);
    for (int i = 0; i < 4; i++) ny[i] = dd[i] - d[i];

    for (int i = 0; i < 4; i++) { qx[i] = px[i]; qy[i] = py[i]; qz[i] = pz[i] + eps; }
    eval4(qx, qy, qz, dd);
    for (int i = 0; i < 4; i++) nz[i] = dd[i] - d[i];

    for (int i = 0; i < 4; i++) {
        f32 len = std::sqrt(nx[i]*nx[i] + ny[i]*ny[i] + nz[i]*nz[i]);
        if (len > 1e-8f) { nx[i] /= len; ny[i] /= len; nz[i] /= len; }
    }
}

// ============================================================================
// sdfAO4 — SIMD batch ambient occlusion
// Computes AO for 4 hit points using bvhEval4 (4 calls instead of 16)
// ============================================================================

inline void sdfAO4(const OntScene& sc,
                   f32 px[4], f32 py[4], f32 pz[4], f32 w,
                   f32 nx[4], f32 ny[4], f32 nz[4],
                   f32 ao[4],
                   const BrickMap* bm = nullptr) {
    bool active_all[4] = {true, true, true, true};
    u32 dummy_mat[4];
    for (int i = 0; i < 4; i++) ao[i] = 0.0f;

    for (int step = 1; step <= 4; step++) {
        f32 d_step = (f32)step * 0.02f;
        f32 qx[4], qy[4], qz[4], dist[4];
        for (int i = 0; i < 4; i++) {
            qx[i] = px[i] + nx[i] * d_step;
            qy[i] = py[i] + ny[i] * d_step;
            qz[i] = pz[i] + nz[i] * d_step;
        }
        if (bm && bm->valid) {
            evalBrickHybrid4(sc, w, *bm, qx, qy, qz, active_all, dist, dummy_mat, 0.001f);
        } else {
            bvhEval4(sc, qx, qy, qz, w, dist, dummy_mat);
        }
        for (int i = 0; i < 4; i++)
            ao[i] += (d_step - dist[i]) / d_step;
    }

    for (int i = 0; i < 4; i++)
        ao[i] = 1.0f - std::fmin(std::fmax(ao[i], 0.0f), 1.0f);
}

// ============================================================================
// renderOntSceneSIMD — single-thread SIMD packet renderer
// Processes 4 adjacent pixels as a packet through raymarching + SIMD SDF eval
// ============================================================================

inline void renderOntSceneSIMD(const OntScene& sc, Frame& fb, f32 w,
                                 const Light* lights, u32 light_count,
                                 Vec3 bg_color, const PipelineConfig& pl,
                                 const BrickMap* bm = nullptr,
                                 BrickProfiler* prof = nullptr) {
    const int width = fb.width, height = fb.height;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 4-pixel packets
        for (; x + 3 < width; x += 4) {
            // Generate 4 rays
            f32 ox[4], oy[4], oz[4];
            f32 dx[4], dy[4], dz[4];
            Vec3 cam_pos = sc.camera.position;
            for (int i = 0; i < 4; i++) {
                ox[i] = cam_pos.x; oy[i] = cam_pos.y; oz[i] = cam_pos.z;
                Vec3 dir = getRayDir(sc.camera, (f32)(x + i), (f32)y, (f32)width, (f32)height);
                dx[i] = dir.x; dy[i] = dir.y; dz[i] = dir.z;
            }

            // 4-ray packet marching
            bool hit[4] = {false};
            f32 px[4], py[4], pz[4];
            f32 nx[4], ny[4], nz[4];
            u32 mat_id[4] = {0xFFFFFFFF};
            f32 t_dist[4];

            // Per-ray state
            f32 t[4];
            bool active[4] = {true, true, true, true};
            int active_count = 4;

            // Ray-AABB intersection for each ray
            const auto* bvh = sc.bvh_nodes;
            u32 bvh_count = sc.header->bvh_count;

            for (int i = 0; i < 4; i++) {
                Vec3 ro = {ox[i], oy[i], oz[i]};
                Vec3 rd = {dx[i], dy[i], dz[i]};
                t[i] = pl.trace_t_min;

                if (bvh_count > 0) {
                    const auto& root = bvh[0];
                    f32 tmin = 0.0f, tmax = pl.trace_t_max;
                    for (int a = 0; a < 3; a++) {
                        f32 origin = (&ro.x)[a];
                        f32 dir = (&rd.x)[a];
                        f32 bmin = root.min[a];
                        f32 bmax = root.max[a];
                        if (std::abs(dir) < 1e-8f) {
                            if (origin < bmin || origin > bmax) { t[i] = tmax + 1; active[i] = false; break; }
                        } else {
                            f32 inv_dir = 1.0f / dir;
                            f32 t1 = (bmin - origin) * inv_dir;
                            f32 t2 = (bmax - origin) * inv_dir;
                            if (t1 > t2) std::swap(t1, t2);
                            if (t1 > tmin) tmin = t1;
                            if (t2 < tmax) tmax = t2;
                            if (tmin > tmax) { t[i] = tmax + 1; active[i] = false; break; }
                        }
                    }
                    if (active[i]) t[i] = std::fmax(tmin, pl.trace_t_min);
                }
            }

            // Sphere tracing loop — all 4 rays in lockstep
            for (u32 step = 0; step < (u32)pl.trace_max_steps && active_count > 0; step++) {
                // Current positions for all active rays
                for (int i = 0; i < 4; i++) {
                    if (!active[i]) continue;
                    px[i] = ox[i] + dx[i] * t[i];
                    py[i] = oy[i] + dy[i] * t[i];
                    pz[i] = oz[i] + dz[i] * t[i];
                }

                // For inactive rays, keep hit positions (don't zero) to preserve
                // hit point for normals/AO; zero only missed/dead rays.
                for (int i = 0; i < 4; i++)
                    if (!active[i] && !hit[i]) { px[i] = 0; py[i] = 0; pz[i] = 0; }

                // SDF evaluation for all 4 points (brick-hybrid or SIMD BVH)
                f32 d[4];
                u32 mtmp[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                if (bm && bm->valid) {
                    evalBrickHybrid4(sc, w, *bm, px, py, pz, active, d, mtmp, pl.trace_hit_threshold, prof);
                } else {
                    bvhEval4(sc, px, py, pz, w, d, mtmp);
                }

                // Check hit for each active ray
                for (int i = 0; i < 4; i++) {
                    if (!active[i]) continue;
                    f32 eps = pl.trace_hit_threshold * (1.0f + t[i] * 0.01f);
                    if (std::abs(d[i]) < eps) {
                        hit[i] = true;
                        t_dist[i] = t[i];
                        mat_id[i] = mtmp[i];
                        active[i] = false;
                        active_count--;
                    }
                }

                // Advance t for remaining active rays (fudge factor 0.8 for metric error absorption)
                for (int i = 0; i < 4; i++) {
                    if (!active[i]) continue;
                    t[i] += d[i] * 0.8f;
                    if (t[i] > pl.trace_t_max) { active[i] = false; active_count--; }
                }
            }

            // SIMD batch normal + AO for all 4 pixels (uses brick map if available)
            bvhNormal4(sc, px, py, pz, w, nx, ny, nz, bm);
            f32 ao[4];
            sdfAO4(sc, px, py, pz, w, nx, ny, nz, ao, bm);

            // Shade each pixel
            for (int i = 0; i < 4; i++) {
                Vec3 c;
                if (hit[i]) {
                    Vec3 p = {px[i], py[i], pz[i]};
                    Vec3 n = {nx[i], ny[i], nz[i]};
                    Vec3 view_dir = normalize(cam_pos - p);
                    if (lights) {
                        c = shadeOnt(sc, p, n, view_dir, mat_id[i], pl, ao[i], lights, light_count);
                    } else {
                        c = shadeOnt(sc, p, n, view_dir, mat_id[i], pl, ao[i]);
                    }
                } else {
                    Vec3 sky_bottom = {bg_color.x * 0.3f, bg_color.y * 0.3f, bg_color.z * 0.3f};
                    Vec3 sky_horizon = bg_color;
                    Vec3 sky_top = {bg_color.x * 1.5f, bg_color.y * 1.5f, bg_color.z * 1.5f};
                    f32 sky_t = 0.5f + 0.5f * dy[i];
                    if (dy[i] > 0.0f) {
                        c.x = lerp(sky_horizon.x, sky_top.x, sky_t * 2.0f);
                        c.y = lerp(sky_horizon.y, sky_top.y, sky_t * 2.0f);
                        c.z = lerp(sky_horizon.z, sky_top.z, sky_t * 2.0f);
                    } else {
                        c = sky_bottom;
                    }
                }
                applyPost(c, pl);
                fb.pixels[y * width + x + i] = Frame::toRgba(c.x, c.y, c.z);
            }
        }

        // Remaining pixels (width not multiple of 4)
        for (; x < width; x++) {
            renderOntPixel(sc, fb, w, lights, light_count, bg_color, pl, x, y);
        }
    }
}

// ============================================================================
// renderOntSceneMTSIMD — multi-threaded SIMD packet renderer
// Same as renderOntSceneMT but uses SIMD packet per-thread
// ============================================================================

inline void renderOntSceneMTSIMD(const OntScene& sc, Frame& fb, f32 w,
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
        BrickProfiler single_prof;
        renderOntSceneSIMD(sc, fb, w, lights, light_count, bg_color, pl, bm, &single_prof);
#ifdef BRICK_PROFILE
        brickProfilerPrint(single_prof, "frame");
#endif
        return;
    }

    int rows_per = fb.height / thread_count;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

#ifdef BRICK_PROFILE
    std::vector<BrickProfiler> profilers(thread_count);
#endif

    for (int i = 0; i < thread_count; i++) {
        int y0 = i * rows_per;
        int y1 = (i == thread_count - 1) ? fb.height : y0 + rows_per;

#ifdef BRICK_PROFILE
        BrickProfiler* thread_prof = &profilers[i];
#else
        BrickProfiler* thread_prof = nullptr;
#endif

        threads.emplace_back([&sc, &fb, w, lights, light_count, bg_color, &pl, y0, y1, bm, thread_prof]() {
            // Each thread gets a sub-frame view
            Frame sub_fb;
            sub_fb.pixels = fb.pixels;
            sub_fb.width = fb.width;
            sub_fb.height = y1 - y0;

            // We can't easily pass a sub-view to renderOntSceneSIMD because it expects
            // the full fb. Instead, we create a wrapper that renders only rows [y0, y1).
            // For now, render each row with SIMD packet approach for the row.
            const int width = fb.width;
            for (int y = y0; y < y1; y++) {
                int x = 0;
                for (; x + 3 < width; x += 4) {
                    f32 ox[4], oy[4], oz[4];
                    f32 dx[4], dy[4], dz[4];
                    Vec3 cam_pos = sc.camera.position;
                    for (int pi = 0; pi < 4; pi++) {
                        ox[pi] = cam_pos.x; oy[pi] = cam_pos.y; oz[pi] = cam_pos.z;
                        Vec3 dir = getRayDir(sc.camera, (f32)(x + pi), (f32)y, (f32)width, (f32)fb.height);
                        dx[pi] = dir.x; dy[pi] = dir.y; dz[pi] = dir.z;
                    }

                    // [Same packet marching as renderOntSceneSIMD but inline for thread safety]
                    bool hit[4] = {false};
                    f32 px[4], py[4], pz[4];
                    f32 nx[4], ny[4], nz[4];
                    u32 mat_id[4] = {0xFFFFFFFF};
                    f32 t[4];
                    bool active[4] = {true, true, true, true};
                    int active_count = 4;

                    const auto* bvh = sc.bvh_nodes;
                    u32 bvh_count = sc.header->bvh_count;

                    for (int pi = 0; pi < 4; pi++) {
                        t[pi] = pl.trace_t_min;
                        if (bvh_count > 0) {
                            const auto& root = bvh[0];
                            f32 tmin = 0.0f, tmax = pl.trace_t_max;
                            for (int a = 0; a < 3; a++) {
                                f32 origin = (&ox[pi])[a];
                                f32 dir = (&dx[pi])[a];
                                f32 bmin = root.min[a], bmax = root.max[a];
                                if (std::abs(dir) < 1e-8f) {
                                    if (origin < bmin || origin > bmax) { t[pi] = tmax + 1; active[pi] = false; break; }
                                } else {
                                    f32 inv_dir = 1.0f / dir;
                                    f32 t1 = (bmin - origin) * inv_dir;
                                    f32 t2 = (bmax - origin) * inv_dir;
                                    if (t1 > t2) std::swap(t1, t2);
                                    if (t1 > tmin) tmin = t1;
                                    if (t2 < tmax) tmax = t2;
                                    if (tmin > tmax) { t[pi] = tmax + 1; active[pi] = false; break; }
                                }
                            }
                            if (active[pi]) t[pi] = std::fmax(tmin, pl.trace_t_min);
                        }
                    }

                    for (u32 step = 0; step < (u32)pl.trace_max_steps && active_count > 0; step++) {
                        for (int pi = 0; pi < 4; pi++) {
                            if (!active[pi]) continue;
                            px[pi] = ox[pi] + dx[pi] * t[pi];
                            py[pi] = oy[pi] + dy[pi] * t[pi];
                            pz[pi] = oz[pi] + dz[pi] * t[pi];
                        }
                        for (int pi = 0; pi < 4; pi++)
                            if (!active[pi] && !hit[pi]) { px[pi] = 0; py[pi] = 0; pz[pi] = 0; }

                        f32 d[4];
                        u32 mtmp[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
                        if (bm && bm->valid) {
                            evalBrickHybrid4(sc, w, *bm, px, py, pz, active, d, mtmp, pl.trace_hit_threshold, thread_prof);
                        } else {
                            bvhEval4(sc, px, py, pz, w, d, mtmp);
                        }

                        for (int pi = 0; pi < 4; pi++) {
                            if (!active[pi]) continue;
                            f32 eps = pl.trace_hit_threshold * (1.0f + t[pi] * 0.01f);
                            if (std::abs(d[pi]) < eps) {
                                hit[pi] = true;
                                mat_id[pi] = mtmp[pi];
                                active[pi] = false;
                                active_count--;
                            }
                        }
                        for (int pi = 0; pi < 4; pi++) {
                            if (!active[pi]) continue;
                            t[pi] += d[pi] * 0.8f;
                            if (t[pi] > pl.trace_t_max) { active[pi] = false; active_count--; }
                        }
                    }

                    // SIMD batch normal + AO for all 4 pixels (uses brick map if available)
                    bvhNormal4(sc, px, py, pz, w, nx, ny, nz, bm);
                    f32 ao[4];
                    sdfAO4(sc, px, py, pz, w, nx, ny, nz, ao, bm);

                    for (int pi = 0; pi < 4; pi++) {
                        Vec3 c;
                        if (hit[pi]) {
                            Vec3 p = {px[pi], py[pi], pz[pi]};
                            Vec3 n = {nx[pi], ny[pi], nz[pi]};
                            Vec3 view_dir = normalize(cam_pos - p);
                            c = lights
                                ? shadeOnt(sc, p, n, view_dir, mat_id[pi], pl, ao[pi], lights, light_count)
                                : shadeOnt(sc, p, n, view_dir, mat_id[pi], pl, ao[pi]);
                        } else {
                            Vec3 sky_bottom = {bg_color.x * 0.3f, bg_color.y * 0.3f, bg_color.z * 0.3f};
                            Vec3 sky_horizon = bg_color;
                            Vec3 sky_top = {bg_color.x * 1.5f, bg_color.y * 1.5f, bg_color.z * 1.5f};
                            f32 sky_t = 0.5f + 0.5f * dy[pi];
                            if (dy[pi] > 0.0f) {
                                c.x = lerp(sky_horizon.x, sky_top.x, sky_t * 2.0f);
                                c.y = lerp(sky_horizon.y, sky_top.y, sky_t * 2.0f);
                                c.z = lerp(sky_horizon.z, sky_top.z, sky_t * 2.0f);
                            } else {
                                c = sky_bottom;
                            }
                        }
                        applyPost(c, pl);
                        fb.pixels[y * width + x + pi] = Frame::toRgba(c.x, c.y, c.z);
                    }
                }
                for (; x < width; x++)
                    renderOntPixel(sc, fb, w, lights, light_count, bg_color, pl, x, y);
            }
        });
    }

    for (auto& t : threads)
        if (t.joinable()) t.join();

#ifdef BRICK_PROFILE
    BrickProfiler total;
    for (auto& p : profilers) {
        total.brick_calls += p.brick_calls;
        total.bvh_calls += p.bvh_calls;
        total.dyn_calls += p.dyn_calls;
        total.total_calls += p.total_calls;
    }
    brickProfilerPrint(total, "frame");
#endif
}

} // namespace mg
