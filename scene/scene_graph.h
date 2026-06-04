#pragma once
#include "../os/os.h"
#include "../render/scene.h"
#include <vector>
#include <cmath>

namespace mg {
namespace scene {

using Aabb = mg::Aabb;

// ============================================================================
// SceneNode — extends render::Node with hierarchy & spatial data
// ============================================================================
struct SceneNode {
    u32     id              = 0;
    u32     parent          = 0xFFFFFFFF;
    u32     first_child     = 0xFFFFFFFF;
    u32     next_sibling    = 0xFFFFFFFF;
    u32     depth           = 0;

    Vec3    local_translate = {0,0,0};
    Vec3    local_rotate    = {0,0,0};
    Vec3    local_scale     = {1,1,1};

    Vec3    world_translate = {0,0,0};
    Vec3    world_rotate    = {0,0,0};
    Vec3    world_scale     = {1,1,1};

    Aabb    local_aabb;
    Aabb    world_aabb;
    bool    aabb_valid      = false;

    bool    transform_dirty = true;
    bool    aabb_dirty      = true;
    bool    enabled         = true;
};

// ============================================================================
// SceneGraph — manages hierarchy, transform propagation, AABBs
// ============================================================================
struct SceneGraph {
    std::vector<SceneNode> nodes;

    void clear() { nodes.clear(); }

    // Resize to match render::Scene node count (1:1 mapping by index)
    void init(const Scene& scene) {
        nodes.resize(scene.nodes.size());
        for (u32 i = 0; i < (u32)nodes.size(); i++) {
            nodes[i].id = i;
            nodes[i].local_translate = scene.nodes[i].translate;
            nodes[i].local_rotate    = scene.nodes[i].rotate;
            nodes[i].local_scale     = scene.nodes[i].scale;
            nodes[i].parent          = 0xFFFFFFFF;
            nodes[i].first_child     = 0xFFFFFFFF;
            nodes[i].next_sibling    = 0xFFFFFFFF;
            nodes[i].transform_dirty = true;
            nodes[i].aabb_dirty      = true;
        }
        // Build hierarchy from render::Node parent/children
        for (u32 i = 0; i < (u32)nodes.size(); i++) {
            const Node& rn = scene.nodes[i];
            // If this node is referenced as a child of a GROUP
            for (u32 j = 0; j < (u32)scene.nodes.size(); j++) {
                for (u32 child : scene.nodes[j].children) {
                    if (child == i && scene.nodes[j].type == NodeType::GROUP) {
                        setParent(i, j);
                    }
                }
            }
        }
    }

    void setParent(u32 node, u32 parent) {
        if (node >= nodes.size() || parent >= nodes.size()) return;
        nodes[node].parent = parent;
        nodes[node].next_sibling = nodes[parent].first_child;
        nodes[parent].first_child = node;
        markDirty(node);
    }

    void markDirty(u32 node) {
        if (node >= nodes.size()) return;
        nodes[node].transform_dirty = true;
        nodes[node].aabb_dirty = true;
        // Propagate to children
        u32 child = nodes[node].first_child;
        while (child != 0xFFFFFFFF) {
            markDirty(child);
            child = nodes[child].next_sibling;
        }
    }

    void setTransform(u32 node, Vec3 t, Vec3 r, Vec3 s) {
        if (node >= nodes.size()) return;
        nodes[node].local_translate = t;
        nodes[node].local_rotate    = r;
        nodes[node].local_scale     = s;
        markDirty(node);
    }

    // Propagate world transforms from root to leaves
    void updateWorldTransforms() {
        for (u32 i = 0; i < (u32)nodes.size(); i++) {
            if (!nodes[i].transform_dirty) continue;
            resolveWorldTransform(i);
        }
    }

    Vec3 resolveWorldTransform(u32 node) {
        if (node >= nodes.size()) return {0,0,0};
        SceneNode& n = nodes[node];
        if (!n.transform_dirty) return n.world_translate;

        if (n.parent == 0xFFFFFFFF || n.parent >= nodes.size()) {
            n.world_translate = n.local_translate;
            n.world_rotate    = n.local_rotate;
            n.world_scale     = n.local_scale;
        } else {
            // Ensure parent is resolved first
            resolveWorldTransform(n.parent);
            const SceneNode& p = nodes[n.parent];
            // Apply parent world rotation to local offset
            f32 prx = p.world_rotate.x * 3.14159265f / 180.0f;
            f32 pry = p.world_rotate.y * 3.14159265f / 180.0f;
            f32 prz = p.world_rotate.z * 3.14159265f / 180.0f;
            f32 cx = std::cos(prx), sx = std::sin(prx);
            f32 cy = std::cos(pry), sy = std::sin(pry);
            f32 cz = std::cos(prz), sz = std::sin(prz);

            Vec3 local = n.local_translate;
            f32 rx = p.world_rotate.x * 3.14159265f / 180.0f;
            local = {local.x*cy*cz + local.y*(-cz*sx*sy + cx*sz) + local.z*(sx*sz + cx*cz*sy),
                     local.x*cy*sz + local.y*(cx*cz + sx*sy*sz) + local.z*(-cz*sx + cx*sy*sz),
                     local.x*(-sy) + local.y*cy*sx + local.z*cx*cy};

            n.world_translate = p.world_translate + Vec3{local.x * p.world_scale.x, local.y * p.world_scale.y, local.z * p.world_scale.z};
            n.world_rotate = {p.world_rotate.x + n.local_rotate.x,
                              p.world_rotate.y + n.local_rotate.y,
                              p.world_rotate.z + n.local_rotate.z};
            n.world_scale = {p.world_scale.x * n.local_scale.x,
                             p.world_scale.y * n.local_scale.y,
                             p.world_scale.z * n.local_scale.z};
        }
        n.transform_dirty = false;
        return n.world_translate;
    }

    // Build AABB for a node based on its SDF primitive (conservative)
    Aabb computeLocalAabb(u32 node_idx, const Scene& scene) {
        Aabb aabb;
        if (node_idx >= scene.nodes.size()) return aabb;

        const Node& rn = scene.nodes[node_idx];
        if (rn.type != NodeType::SDF) {
            // GROUP/INSTANCE: union of children AABBs
            for (u32 child : rn.children) {
                computeLocalAabb(child, scene);
                if (nodes[child].aabb_valid)
                    aabb.expand(nodes[child].local_aabb);
            }
            nodes[node_idx].local_aabb = aabb;
            nodes[node_idx].aabb_valid = true;
            return aabb;
        }

        const std::string& t = rn.sdf.sdf_type;
        f32 r0 = rn.sdf.params[0].is_expr ? 1.0f : rn.sdf.params[0].constant;
        f32 r1 = rn.sdf.params[1].is_expr ? 1.0f : rn.sdf.params[1].constant;
        f32 r2 = rn.sdf.params[2].is_expr ? 1.0f : rn.sdf.params[2].constant;

        if (t == "sphere") {
            f32 rad = std::abs(r0);
            aabb.min = {-rad, -rad, -rad};
            aabb.max = { rad,  rad,  rad};
        } else if (t == "box") {
            aabb.min = {-std::abs(r0), -std::abs(r1), -std::abs(r2)};
            aabb.max = { std::abs(r0),  std::abs(r1),  std::abs(r2)};
        } else if (t == "cylinder") {
            f32 rad = std::abs(r0);
            f32 half = std::abs(r1) * 0.5f;
            aabb.min = {-rad, -half, -rad};
            aabb.max = { rad,  half,  rad};
        } else if (t == "torus") {
            f32 major = std::abs(r0) + std::abs(r1);
            f32 minor = std::abs(r1);
            aabb.min = {-major, -minor, -major};
            aabb.max = { major,  minor,  major};
        } else if (t == "plane") {
            aabb.min = {-100, -100, -100};
            aabb.max = { 100,  100,  100};
        } else if (t == "cone") {
            f32 rad = std::abs(r0);
            f32 hh  = std::abs(r1);
            aabb.min = {-rad, -hh, -rad};
            aabb.max = { rad,  hh,  rad};
        } else {
            // capsule, rounded_box, boolean ops: conservative 2-unit box
            aabb.min = {-2, -2, -2};
            aabb.max = { 2,  2,  2};
        }

        // Apply local transform to AABB
        Vec3 corners[8] = {
            {aabb.min.x, aabb.min.y, aabb.min.z},
            {aabb.max.x, aabb.min.y, aabb.min.z},
            {aabb.min.x, aabb.max.y, aabb.min.z},
            {aabb.max.x, aabb.max.y, aabb.min.z},
            {aabb.min.x, aabb.min.y, aabb.max.z},
            {aabb.max.x, aabb.min.y, aabb.max.z},
            {aabb.min.x, aabb.max.y, aabb.max.z},
            {aabb.max.x, aabb.max.y, aabb.max.z},
        };

        // Apply rotate + scale to each corner, find new bounds
        Aabb transformed;
        for (int i = 0; i < 8; i++) {
            Vec3 p = corners[i];
            // Scale first
            p.x *= std::abs(rn.scale.x);
            p.y *= std::abs(rn.scale.y);
            p.z *= std::abs(rn.scale.z);
            // Then rotate
            f32 rx = rn.rotate.x * 3.14159265f / 180.0f;
            f32 ry = rn.rotate.y * 3.14159265f / 180.0f;
            f32 rz = rn.rotate.z * 3.14159265f / 180.0f;
            f32 cz = std::cos(rz), sz = std::sin(rz);
            Vec3 rp = {p.x*cz - p.y*sz, p.x*sz + p.y*cz, p.z};
            f32 cy = std::cos(ry), sy = std::sin(ry);
            rp = {rp.x*cy + rp.z*sy, rp.y, -rp.x*sy + rp.z*cy};
            f32 cx = std::cos(rx), sx = std::sin(rx);
            rp = {rp.x, rp.y*cx - rp.z*sx, rp.y*sx + rp.z*cx};
            // Then translate
            rp.x += rn.translate.x;
            rp.y += rn.translate.y;
            rp.z += rn.translate.z;
            transformed.expand(rp);
        }

        nodes[node_idx].local_aabb = transformed;
        nodes[node_idx].aabb_valid = true;
        return transformed;
    }

    // Compute world-space AABB from local AABB + world transform
    Aabb computeWorldAabb(u32 node_idx) {
        if (node_idx >= nodes.size()) return {};
        SceneNode& n = nodes[node_idx];
        if (!n.aabb_valid) return n.world_aabb;
        if (!n.aabb_dirty) return n.world_aabb;

        // Transform local AABB corners to world space
        Vec3 corners[8] = {
            {n.local_aabb.min.x, n.local_aabb.min.y, n.local_aabb.min.z},
            {n.local_aabb.max.x, n.local_aabb.min.y, n.local_aabb.min.z},
            {n.local_aabb.min.x, n.local_aabb.max.y, n.local_aabb.min.z},
            {n.local_aabb.max.x, n.local_aabb.max.y, n.local_aabb.min.z},
            {n.local_aabb.min.x, n.local_aabb.min.y, n.local_aabb.max.z},
            {n.local_aabb.max.x, n.local_aabb.min.y, n.local_aabb.max.z},
            {n.local_aabb.min.x, n.local_aabb.max.y, n.local_aabb.max.z},
            {n.local_aabb.max.x, n.local_aabb.max.y, n.local_aabb.max.z},
        };

        Aabb wa;
        for (int i = 0; i < 8; i++) {
            Vec3 p = corners[i];
            f32 rx = n.world_rotate.x * 3.14159265f / 180.0f;
            f32 ry = n.world_rotate.y * 3.14159265f / 180.0f;
            f32 rz = n.world_rotate.z * 3.14159265f / 180.0f;
            f32 cz = std::cos(rz), sz = std::sin(rz);
            Vec3 rp = {p.x*cz - p.y*sz, p.x*sz + p.y*cz, p.z};
            f32 cy = std::cos(ry), sy = std::sin(ry);
            rp = {rp.x*cy + rp.z*sy, rp.y, -rp.x*sy + rp.z*cy};
            f32 cx = std::cos(rx), sx = std::sin(rx);
            rp = {rp.x, rp.y*cx - rp.z*sx, rp.y*sx + rp.z*cx};
            rp.x = rp.x * n.world_scale.x + n.world_translate.x;
            rp.y = rp.y * n.world_scale.y + n.world_translate.y;
            rp.z = rp.z * n.world_scale.z + n.world_translate.z;
            wa.expand(rp);
        }

        n.world_aabb = wa;
        n.aabb_dirty = false;
        return wa;
    }

    // Get world-space AABB (computes if dirty)
    Aabb getWorldAabb(u32 node_idx) {
        if (node_idx >= nodes.size()) return {};
        if (nodes[node_idx].aabb_dirty)
            return computeWorldAabb(node_idx);
        return nodes[node_idx].world_aabb;
    }

    // Gather all enabled leaf SDF nodes within frustum (ray-AABB)
    void collectVisibleNodes(u32 node_idx, const Vec3& ro, const Vec3& rd,
                             std::vector<u32>& result) {
        if (node_idx >= nodes.size()) return;
        if (!nodes[node_idx].enabled) return;

        Aabb wa = getWorldAabb(node_idx);
        f32 tmin, tmax;
        if (!wa.intersect(ro, rd, tmin, tmax)) return;

        result.push_back(node_idx);
    }

    void collectAllVisible(const Vec3& ro, const Vec3& rd,
                           std::vector<u32>& result) {
        result.clear();
        for (u32 i = 0; i < (u32)nodes.size(); i++) {
            collectVisibleNodes(i, ro, rd, result);
        }
    }
};

} // namespace scene
} // namespace mg
