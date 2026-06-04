#pragma once
#include "../os/os.h"
#include "../render/scene.h"
#include "scene_graph.h"
#include <vector>
#include <algorithm>

namespace mg {
namespace scene {

// ============================================================================
// BvhNode — BVH acceleration structure
// ============================================================================

struct BvhNode {
    Aabb bounds;
    u32  left    = 0xFFFFFFFF;
    u32  right   = 0xFFFFFFFF;
    u32  node_idx = 0xFFFFFFFF; // leaf: scene node index
    bool is_leaf = false;
};

struct Bvh {
    std::vector<BvhNode> nodes;
    u32 root = 0xFFFFFFFF;

    void clear() { nodes.clear(); root = 0xFFFFFFFF; }

    void build(SceneGraph& graph, const Scene& scene) {
        clear();
        if (graph.nodes.empty()) return;

        // Collect all enabled SDF leaf nodes
        std::vector<u32> leaves;
        for (u32 i = 0; i < (u32)graph.nodes.size(); i++) {
            if (!graph.nodes[i].enabled) continue;
            if (scene.nodes[i].type == NodeType::GROUP) continue;
            if (scene.nodes[i].type == NodeType::INSTANCE) continue;
            if (scene.nodes[i].is_compound_child) continue;
            leaves.push_back(i);
        }

        if (leaves.empty()) return;

        // Ensure AABBs are valid
        for (u32 idx : leaves) {
            if (!graph.nodes[idx].aabb_valid)
                graph.computeLocalAabb(idx, scene);
            graph.getWorldAabb(idx);
        }

        // Build top-down
        nodes.reserve(leaves.size() * 2);
        root = buildRecursive(graph, leaves.data(), (u32)leaves.size(), 0);
    }

    u32 buildRecursive(SceneGraph& graph, u32* indices, u32 count, u32 depth) {
        BvhNode node;
        node.is_leaf = (count <= 4 || depth >= 16);

        if (node.is_leaf) {
            for (u32 i = 0; i < count; i++) {
                node.bounds.expand(graph.nodes[indices[i]].world_aabb);
            }
            node.node_idx = indices[0];
            nodes.push_back(node);
            return (u32)nodes.size() - 1;
        }

        // Compute centroid bounds for splitting
        Aabb centroid;
        for (u32 i = 0; i < count; i++) {
            Vec3 c = graph.nodes[indices[i]].world_translate;
            centroid.expand(c);
        }
        Vec3 ce = {centroid.max.x - centroid.min.x,
                   centroid.max.y - centroid.min.y,
                   centroid.max.z - centroid.min.z};

        // Split along largest axis
        u32 axis = 0;
        if (ce.y > ce.x && ce.y > ce.z) axis = 1;
        if (ce.z > ce.x && ce.z > ce.y) axis = 2;

        f32 mid = 0;
        if (axis == 0) mid = (centroid.min.x + centroid.max.x) * 0.5f;
        else if (axis == 1) mid = (centroid.min.y + centroid.max.y) * 0.5f;
        else mid = (centroid.min.z + centroid.max.z) * 0.5f;

        // Partition
        u32 split = 0;
        for (u32 i = 0; i < count; i++) {
            f32 val = (axis == 0) ? graph.nodes[indices[i]].world_translate.x :
                       (axis == 1) ? graph.nodes[indices[i]].world_translate.y :
                       graph.nodes[indices[i]].world_translate.z;
            if (val < mid) {
                std::swap(indices[i], indices[split]);
                split++;
            }
        }

        // If all on one side, split in half
        if (split == 0 || split == count) split = count / 2;

        u32 left_idx  = buildRecursive(graph, indices, split, depth + 1);
        u32 right_idx = buildRecursive(graph, indices + split, count - split, depth + 1);

        node.left  = left_idx;
        node.right = right_idx;
        node.bounds.expand(nodes[left_idx].bounds);
        node.bounds.expand(nodes[right_idx].bounds);

        nodes.push_back(node);
        return (u32)nodes.size() - 1;
    }

    // Query: return all scene node indices intersected by ray
    void query(const Vec3& ro, const Vec3& rd, std::vector<u32>& result) const {
        result.clear();
        if (root == 0xFFFFFFFF || nodes.empty()) return;
        queryRecursive(ro, rd, root, result);
    }

    void queryRecursive(const Vec3& ro, const Vec3& rd, u32 node_idx,
                        std::vector<u32>& result) const {
        if (node_idx >= nodes.size()) return;
        const BvhNode& n = nodes[node_idx];

        f32 tmin, tmax;
        if (!n.bounds.intersect(ro, rd, tmin, tmax)) return;

        if (n.is_leaf) {
            result.push_back(n.node_idx);
            return;
        }

        queryRecursive(ro, rd, n.left, result);
        queryRecursive(ro, rd, n.right, result);
    }
};

} // namespace scene
} // namespace mg
