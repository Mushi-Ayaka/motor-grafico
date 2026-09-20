// ri_optimizer.cpp — CPU implementation of RIH bytecode optimizer (F1)
#include "ri_optimizer.h"
#include "scene.h"
#include <cstring>
#include <cmath>

namespace mg {

// ============================================================================
// Bytecode optimization helpers
// ============================================================================

// Read a 5-byte opcode entry: [opcode, p0, p1, p2, p3]
// For ONT_CONST, p0..p3 is a float. For others, p0..p3 is padding.
static uint8_t readOpcode(const uint8_t* bc, uint32_t pc) {
    return bc[pc];
}

static float readParam(const uint8_t* bc, uint32_t pc) {
    float f;
    memcpy(&f, &bc[pc + 1], 4);
    return f;
}

static void writeOpcode(uint8_t* bc, uint32_t pc, uint8_t op) {
    bc[pc] = op;
}

static void writeParam(uint8_t* bc, uint32_t pc, float f) {
    memcpy(&bc[pc + 1], &f, 4);
}

// ============================================================================
// Constant folding: fold two CONST + binary op into one CONST
// Pattern: [CONST a] [CONST b] [OP] -> [CONST result]
// Returns true if folding occurred at this position.
// ============================================================================
static bool tryConstantFold(uint8_t* bc, uint32_t& pc, uint32_t end, uint32_t& ops_folded) {
    if (pc + 15 >= end) return false; // need at least 3 entries (15 bytes)

    uint8_t op0 = readOpcode(bc, pc);
    uint8_t op1 = readOpcode(bc, pc + 5);
    uint8_t op2 = readOpcode(bc, pc + 10);

    // Pattern: CONST, CONST, BINARY_OP
    if (op0 != ONT_CONST || op1 != ONT_CONST) return false;

    float a = readParam(bc, pc);
    float b = readParam(bc, pc + 5);
    float result = 0.0f;
    bool valid = false;

    switch (op2) {
    case ONT_ADD: result = a + b; valid = true; break;
    case ONT_SUB: result = a - b; valid = true; break;
    case ONT_MUL: result = a * b; valid = true; break;
    case ONT_DIV: if (fabsf(b) > 1e-10f) { result = a / b; valid = true; } break;
    case ONT_MIN: result = (a < b) ? a : b; valid = true; break;
    case ONT_MAX: result = (a > b) ? a : b; valid = true; break;
    case ONT_POW: result = powf(a, b); valid = true; break;
    default: return false;
    }

    if (!valid) return false;

    // Write CONST at current position, mark next 10 bytes as ONT_END (nop)
    writeOpcode(bc, pc, ONT_CONST);
    writeParam(bc, pc, result);
    writeOpcode(bc, pc + 5, ONT_END); // nop
    writeOpcode(bc, pc + 10, ONT_END); // nop
    pc += 5; // advance past the folded constant
    ops_folded++;
    return true;
}

// ============================================================================
// Algebraic simplify: simplify known patterns
// x + 0 -> x, 0 + x -> x
// x * 1 -> x, 1 * x -> x
// x * 0 -> 0, 0 * x -> 0
// x - 0 -> x
// x / 1 -> x
// ============================================================================
static bool tryAlgebraicSimplify(uint8_t* bc, uint32_t& pc, uint32_t end, uint32_t& ops_simplified) {
    if (pc + 10 >= end) return false;

    uint8_t op0 = readOpcode(bc, pc);
    uint8_t op1 = readOpcode(bc, pc + 5);
    uint8_t op2 = readOpcode(bc, pc + 10);

    // Need CONST, CONST, BINARY_OP
    if (op0 != ONT_CONST || op1 != ONT_CONST) return false;

    float a = readParam(bc, pc);
    float b = readParam(bc, pc + 5);

    bool simplify = false;
    float result = 0.0f;

    switch (op2) {
    case ONT_ADD:
        if (fabsf(a) < 1e-10f) { result = b; simplify = true; } // 0 + x -> x
        if (fabsf(b) < 1e-10f) { result = a; simplify = true; } // x + 0 -> x
        break;
    case ONT_SUB:
        if (fabsf(b) < 1e-10f) { result = a; simplify = true; } // x - 0 -> x
        break;
    case ONT_MUL:
        if (fabsf(a) < 1e-10f || fabsf(b) < 1e-10f) { result = 0.0f; simplify = true; } // x * 0 -> 0
        if (fabsf(a - 1.0f) < 1e-10f) { result = b; simplify = true; } // 1 * x -> x
        if (fabsf(b - 1.0f) < 1e-10f) { result = a; simplify = true; } // x * 1 -> x
        break;
    case ONT_DIV:
        if (fabsf(b - 1.0f) < 1e-10f) { result = a; simplify = true; } // x / 1 -> x
        break;
    default: return false;
    }

    if (!simplify) return false;

    // Write simplified result
    writeOpcode(bc, pc, ONT_CONST);
    writeParam(bc, pc, result);
    writeOpcode(bc, pc + 5, ONT_END); // nop
    writeOpcode(bc, pc + 10, ONT_END); // nop
    pc += 5;
    ops_simplified++;
    return true;
}

// ============================================================================
// Optimize a single bytecode stream
// ============================================================================
uint32_t optimizeBytecode(uint8_t* bytecode, uint32_t length) {
    if (!bytecode || length < 5) return 0;

    uint32_t total_ops = 0;
    uint32_t ops_folded = 0;
    uint32_t ops_simplified = 0;

    // Pass 1: Constant folding + algebraic simplify
    uint32_t pc = 0;
    while (pc + 5 <= length) {
        uint8_t op = readOpcode(bytecode, pc);
        if (op == ONT_END) break;

        // Try constant folding
        if (tryConstantFold(bytecode, pc, length, ops_folded)) {
            total_ops++;
            continue;
        }

        // Try algebraic simplification
        if (tryAlgebraicSimplify(bytecode, pc, length, ops_simplified)) {
            total_ops++;
            continue;
        }

        pc += 5;
        total_ops++;
    }

    return ops_folded + ops_simplified;
}

// ============================================================================
// Check if a node is dead
// ============================================================================
bool isNodeDead(uint32_t sdf_type, const float* transform, uint32_t material_id,
                const float* material_color, float material_opacity) {
    // Dead if scale is zero (transform[0]=transform[5]=transform[10]=0)
    float sx = transform[0];
    float sy = transform[5];
    float sz = transform[10];
    if (fabsf(sx) < 1e-10f && fabsf(sy) < 1e-10f && fabsf(sz) < 1e-10f) return true;

    // Dead if material is fully transparent
    if (material_opacity < 1e-10f) return true;

    return false;
}

// ============================================================================
// Optimize an entire OntScene
// ============================================================================
bool optimizeOntScene(OntScene& scene, OptStats* stats) {
    if (!scene.header || !scene.bytecode || !scene.graph_nodes) return false;

    OptStats local_stats = {};
    local_stats.nodes_total = scene.header->node_count;

    // Optimize each node's bytecode
    for (uint32_t i = 0; i < scene.header->node_count; i++) {
        const auto& node = scene.graph_nodes[i];
        if (node.bytecode_length == 0) continue;

        // Count ops before
        local_stats.ops_before += node.bytecode_length / 5;

        // Optimize
        uint8_t* bc = const_cast<uint8_t*>(scene.bytecode + node.bytecode_offset);
        uint32_t eliminated = optimizeBytecode(bc, node.bytecode_length);
        local_stats.ops_after += (node.bytecode_length / 5) - eliminated;
        local_stats.constants_folded += eliminated;
    }

    // Check for dead nodes (mark with special flag in pad[0])
    // Note: We don't actually remove nodes, just flag them for the pruner
    for (uint32_t i = 0; i < scene.header->node_count; i++) {
        auto& node = const_cast<OntGraphNode&>(scene.graph_nodes[i]);
        const auto& mat = scene.materials[node.material_id];

        if (isNodeDead(0, node.local_transform, node.material_id,
                       mat.base_color, mat.opacity)) {
            local_stats.nodes_eliminated++;
            // Mark node as culled using pad[0] as flags byte
            node.pad[0] |= 0x4; // bit2 = culled
        }
    }

    if (stats) *stats = local_stats;
    return local_stats.ops_before != local_stats.ops_after || local_stats.nodes_eliminated > 0;
}

} // namespace mg
