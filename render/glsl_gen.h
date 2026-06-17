#pragma once
#include "scene.h"
#include <string>
#include <vector>

namespace mg {

struct GlslGenResult {
    std::string glsl_source;
    bool        ok = false;
    std::string error;
};

// Generate a specialized GLSL compute shader for a given OntScene.
// The generated shader replaces the bytecode VM with inline SDF expressions,
// one function per BVH leaf, dispatched via a switch statement.
struct GlslGen {
    static GlslGenResult generate(const OntScene& scene);
};

// Compile GLSL source string to SPIR-V binary using glslc.exe
// Returns true on success, false with error message in out_error.
bool compileGlslToSpv(const std::string& glsl_source,
                      std::vector<uint32_t>& out_spv,
                      std::string& out_error);

} // namespace mg
