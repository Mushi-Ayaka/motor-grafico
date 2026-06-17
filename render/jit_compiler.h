#pragma once
#include <cstdint>
#include <vector>

namespace mg {

// JIT compiled function signature
typedef void (*SdfJitFunc4)(
    const float* x, const float* y, const float* z, float w,
    float* result, int depth,
    const float* nx, const float* ny, const float* nz,
    const float* vx, const float* vy, const float* vz,
    const float* lx, const float* ly, const float* lz
);

struct OntScene;

class JitCompiler {
public:
    JitCompiler();
    ~JitCompiler();

    bool init();
    
    // Compiles the bytecode of a graph node into native x86-64 code.
    // Returns the function pointer, or nullptr if compilation failed.
    SdfJitFunc4 compileNode(const uint8_t* bytecode, size_t length, uint32_t node_idx);

    // Iterates through scene graph_nodes and compiles them, filling scene.jit_functions
    void compileScene(OntScene& scene);

private:
    struct Impl;
    Impl* _impl = nullptr;
};

} // namespace mg
