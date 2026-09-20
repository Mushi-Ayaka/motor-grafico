#pragma once
#include "scene.h"
#include <string>
#include <vector>
#include <cstdint>

namespace mg {

// SpirvGen: GPU-accelerated SDF shader generation (F3).
// Replaces the CPU-only GlslGen path. Generates a specialized compute shader
// per scene with inline SDF functions (no VM stack-based fallback by default).
//
// Pipeline: bytecode pool (SSBO) -> GLSL snippets (SSBO) -> assemble -> compile -> pipeline
struct SpirvGenResult {
    std::string glsl_source;
    bool        ok = false;
    std::string error;
    uint32_t    nodes_processed = 0;
    uint32_t    unique_sdfs     = 0;
};

struct SpirvGen {
    // Generate a specialized GLSL compute shader for a given OntScene.
    // Uses bytecode decompilation to produce inline SDF functions.
    static SpirvGenResult generate(const OntScene& scene);

    // Generate GLSL snippet for a single node's bytecode (postfix -> infix).
    static std::string decompileBytecode(const uint8_t* bc, uint32_t length);
};

} // namespace mg
