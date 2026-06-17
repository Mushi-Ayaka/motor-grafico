#pragma once
#include "scene.h"
#include "bytecode_vm.h"
#include <cmath>
#include <algorithm>

namespace mg {

// ============================================================================
// Expression evaluator with x,y,z,w context
// ============================================================================
f32 evalExpr(const char* expr, size_t len, f32 x, f32 y, f32 z, f32 w);

inline f32 evalExpr(const std::string& s, f32 x, f32 y, f32 z, f32 w) {
    return evalExpr(s.c_str(), s.size(), x, y, z, w);
}

inline f32 evalSdfExpr(const Expr& e, f32 x, f32 y, f32 z, f32 w) {
    if (!e.bytecode.empty())
        return execBc(e.bytecode, x, y, z, w);
    return e.is_expr ? evalExpr(e.expression, x, y, z, w) : e.constant;
}

// ============================================================================
// SDF primitives
// ============================================================================

inline f32 sdSphere(Vec3 p, f32 r) { return length(p) - r; }

inline f32 sdBox(Vec3 p, Vec3 s) {
    Vec3 q = {std::abs(p.x) - s.x, std::abs(p.y) - s.y, std::abs(p.z) - s.z};
    return length(Vec3{std::fmax(q.x,0.0f), std::fmax(q.y,0.0f), std::fmax(q.z,0.0f)})
           + std::fmin(std::fmax(q.x, std::fmax(q.y, q.z)), 0.0f);
}

inline f32 sdCylinder(Vec3 p, f32 r, f32 h) {
    f32 d = length(Vec3{p.x, 0, p.z}) - r;
    return std::fmax(d, std::abs(p.y) - h * 0.5f);
}

inline f32 sdTorus(Vec3 p, f32 r_major, f32 r_minor) {
    f32 qx = length(Vec3{p.x, 0, p.z}) - r_major;
    return std::sqrt(qx*qx + p.y*p.y) - r_minor;
}

inline f32 sdPlane(Vec3 p, f32 d) { return p.y + d; }

inline f32 sdCone(Vec3 p, f32 r, f32 h) {
    f32 q = length(Vec3{p.x, 0, p.z});
    return std::sqrt(2.0f) * std::fmax(
        (q >= r * (1.0f - p.y / h)) ? q - r * (1.0f - p.y / h) : h - p.y, -p.y);
}

inline f32 opUnion(f32 a, f32 b) { return std::fmin(a, b); }
inline f32 opSubtract(f32 a, f32 b) { return std::fmax(a, -b); }
inline f32 opIntersect(f32 a, f32 b) { return std::fmax(a, b); }
inline f32 opSmoothUnion(f32 a, f32 b, f32 k) {
    f32 h = std::fmax(k - std::abs(a - b), 0.0f) / k;
    return std::fmin(a, b) - h * h * h * k * (1.0f / 6.0f);
}

// ============================================================================
// Transform helpers
// ============================================================================

inline Vec3 applyTransform(Vec3 p, Vec3 t, Vec3 r, Vec3 s) {
    p = {p.x - t.x, p.y - t.y, p.z - t.z};
    f32 rx = r.x * 3.14159265f / 180.0f;
    f32 ry = r.y * 3.14159265f / 180.0f;
    f32 rz = r.z * 3.14159265f / 180.0f;
    f32 cz = std::cos(rz), sz = std::sin(rz);
    Vec3 rp = {p.x*cz - p.y*sz, p.x*sz + p.y*cz, p.z};
    f32 cy = std::cos(ry), sy = std::sin(ry);
    rp = {rp.x*cy + rp.z*sy, rp.y, -rp.x*sy + rp.z*cy};
    f32 cx = std::cos(rx), sx = std::sin(rx);
    rp = {rp.x, rp.y*cx - rp.z*sx, rp.y*sx + rp.z*cx};
    return {rp.x / s.x, rp.y / s.y, rp.z / s.z};
}

// ============================================================================
// SDF tree evaluation
// ============================================================================

f32 evalSdfTree(const Scene& scene, u32 node_idx, Vec3 p, f32 w,
                const f32* transforms = nullptr);

inline f32 evalSdfNode(const Scene& scene, const Node& node, Vec3 p, f32 w,
                        const f32* transforms) {
    const SdfNode& sdf = node.sdf;
    auto p0 = evalSdfExpr(sdf.params[0], p.x, p.y, p.z, w);
    auto p1 = evalSdfExpr(sdf.params[1], p.x, p.y, p.z, w);
    auto p2 = evalSdfExpr(sdf.params[2], p.x, p.y, p.z, w);
    auto p3 = evalSdfExpr(sdf.params[3], p.x, p.y, p.z, w);
    const std::string& t = sdf.sdf_type;

    f32 d = 1e9f;
    if (t == "sphere")   d = sdSphere(p, p0);
    else if (t == "box") d = sdBox(p, {p0, p1, p2});
    else if (t == "cylinder") d = sdCylinder(p, p0, p1);
    else if (t == "torus") d = sdTorus(p, p0, p1);
    else if (t == "plane") d = sdPlane(p, p0);
    else if (t == "cone") d = sdCone(p, p0, p1);
    else if (t == "capsule") d = length(p) - p0;
    else if (t == "rounded_box") d = sdBox(p, {p0, p1, p2}) - p3;
    else if (t == "union")
        d = opUnion(evalSdfTree(scene, sdf.child_a, p, w, transforms),
                    evalSdfTree(scene, sdf.child_b, p, w, transforms));
    else if (t == "subtract")
        d = opSubtract(evalSdfTree(scene, sdf.child_a, p, w, transforms),
                       evalSdfTree(scene, sdf.child_b, p, w, transforms));
    else if (t == "intersect")
        d = opIntersect(evalSdfTree(scene, sdf.child_a, p, w, transforms),
                        evalSdfTree(scene, sdf.child_b, p, w, transforms));
    else if (t == "smooth_union")
        d = opSmoothUnion(evalSdfTree(scene, sdf.child_a, p, w, transforms),
                          evalSdfTree(scene, sdf.child_b, p, w, transforms), p0);
    else if (t == "custom")
        d = evalSdfExpr(sdf.params[0], p.x, p.y, p.z, w);

    if (!sdf.displace_expr.empty())
        d += evalExpr(sdf.displace_expr, p.x, p.y, p.z, w);
    return d;
}

inline f32 evalSdfTree(const Scene& scene, u32 node_idx, Vec3 p, f32 w,
                        const f32* transforms) {
    if (node_idx >= scene.nodes.size()) return 1e9f;
    const Node& node = scene.nodes[node_idx];

    if (transforms) {
        p.x -= transforms[node_idx * 3 + 0];
        p.y -= transforms[node_idx * 3 + 1];
        p.z -= transforms[node_idx * 3 + 2];
    } else {
        p = applyTransform(p, node.translate, node.rotate, node.scale);
    }

    if (node.type == NodeType::GROUP) {
        f32 d = 1e9f;
        for (u32 child : node.children)
            d = std::fmin(d, evalSdfTree(scene, child, p, w, transforms));
        return d;
    }
    if (node.type == NodeType::INSTANCE) {
        if (node.def_id < scene.nodes.size())
            return evalSdfTree(scene, node.def_id, p, w, transforms);
        return 1e9f;
    }
    return evalSdfNode(scene, node, p, w, transforms);
}

// ============================================================================
// Scene-level evaluation
// ============================================================================

inline f32 evalScene(const Scene& scene, Vec3 p, f32 w,
                     const f32* transforms = nullptr,
                     const Aabb* aabbs = nullptr,
                     const u32* filter_nodes = nullptr,
                     u32 filter_count = 0) {
    f32 d = 1e9f;
    u32 count = filter_nodes ? filter_count : (u32)scene.nodes.size();
    for (u32 j = 0; j < count; j++) {
        u32 i = filter_nodes ? filter_nodes[j] : j;
        if (scene.nodes[i].is_compound_child) continue;
        // AABB early-out: distance to AABB is a lower bound of SDF
        if (aabbs) {
            const Aabb& a = aabbs[i];
            f32 dx = (p.x < a.min.x) ? a.min.x - p.x : (p.x > a.max.x) ? p.x - a.max.x : 0;
            f32 dy = (p.y < a.min.y) ? a.min.y - p.y : (p.y > a.max.y) ? p.y - a.max.y : 0;
            f32 dz = (p.z < a.min.z) ? a.min.z - p.z : (p.z > a.max.z) ? p.z - a.max.z : 0;
            f32 dist_to_aabb = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist_to_aabb > std::abs(d) + 0.001f) continue;
        }
        d = std::fmin(d, evalSdfTree(scene, i, p, w, transforms));
    }
    return d;
}

inline u32 findClosestNode(const Scene& scene, Vec3 p, f32 w,
                           const f32* transforms = nullptr,
                           const Aabb* aabbs = nullptr,
                           const u32* filter_nodes = nullptr,
                           u32 filter_count = 0) {
    u32 best = 0xFFFFFFFF;
    f32 best_d = 1e9f;
    u32 count = filter_nodes ? filter_count : (u32)scene.nodes.size();
    for (u32 j = 0; j < count; j++) {
        u32 i = filter_nodes ? filter_nodes[j] : j;
        if (scene.nodes[i].is_compound_child) continue;
        if (aabbs) {
            const Aabb& a = aabbs[i];
            f32 dx = (p.x < a.min.x) ? a.min.x - p.x : (p.x > a.max.x) ? p.x - a.max.x : 0;
            f32 dy = (p.y < a.min.y) ? a.min.y - p.y : (p.y > a.max.y) ? p.y - a.max.y : 0;
            f32 dz = (p.z < a.min.z) ? a.min.z - p.z : (p.z > a.max.z) ? p.z - a.max.z : 0;
            f32 dist_to_aabb = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist_to_aabb > std::abs(best_d) + 0.001f) continue;
        }
        f32 d = evalSdfTree(scene, i, p, w, transforms);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

inline u32 findMaterial(const Scene& scene, Vec3 p, f32 w,
                        const f32* transforms = nullptr,
                        const Aabb* aabbs = nullptr,
                        const u32* filter_nodes = nullptr,
                        u32 filter_count = 0) {
    u32 closest = findClosestNode(scene, p, w, transforms, aabbs, filter_nodes, filter_count);
    return (closest < scene.nodes.size()) ? scene.nodes[closest].material_id : 0xFFFFFFFF;
}

inline Vec3 calcNormal(const Scene& scene, Vec3 p, f32 w,
                       const f32* transforms = nullptr,
                       const Aabb* aabbs = nullptr,
                       const u32* filter_nodes = nullptr,
                       u32 filter_count = 0) {
    const f32 eps = 0.001f;
    f32 d = evalScene(scene, p, w, transforms, aabbs, filter_nodes, filter_count);
    f32 dx = evalScene(scene, {p.x + eps, p.y, p.z}, w, transforms, aabbs, filter_nodes, filter_count);
    f32 dy = evalScene(scene, {p.x, p.y + eps, p.z}, w, transforms, aabbs, filter_nodes, filter_count);
    f32 dz = evalScene(scene, {p.x, p.y, p.z + eps}, w, transforms, aabbs, filter_nodes, filter_count);
    return normalize({dx - d, dy - d, dz - d});
}

} // namespace mg
