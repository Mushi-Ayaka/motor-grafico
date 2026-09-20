// ri_optimizer.h — GPU RIH Optimizer (F1)
// Optimiza bytecode SDF: constant folding, dead code elimination, algebraic simplify
#pragma once
#include <cstdint>
#include <vector>

namespace mg {

struct OntScene;

// Optimization statistics
struct OptStats {
    uint32_t nodes_total = 0;
    uint32_t nodes_eliminated = 0;      // dead code (scale=0, invisible material)
    uint32_t ops_before = 0;
    uint32_t ops_after = 0;             // after constant folding
    uint32_t constants_folded = 0;
    uint32_t algebraic_simplified = 0;
};

// Optimize bytecode in an OntScene (CPU path)
// Returns true if any optimization was applied.
bool optimizeOntScene(OntScene& scene, OptStats* stats = nullptr);

// Optimize a single bytecode stream (in-place)
// Returns the number of operations eliminated/folded.
uint32_t optimizeBytecode(uint8_t* bytecode, uint32_t length);

// Check if a node is "dead" (can be eliminated)
bool isNodeDead(uint32_t sdf_type, const float* transform, uint32_t material_id,
                const float* material_color, float material_opacity);

} // namespace mg
