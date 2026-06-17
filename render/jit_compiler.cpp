#include "jit_compiler.h"
#include "scene.h"
#include <asmjit/asmjit.h>
#include <iostream>
#include <math.h>

extern "C" {
    void _cdecl_jit_sin_ps_ref(float* v) {
        v[0] = sinf(v[0]); v[1] = sinf(v[1]); v[2] = sinf(v[2]); v[3] = sinf(v[3]);
    }
    void _cdecl_jit_cos_ps_ref(float* v) {
        v[0] = cosf(v[0]); v[1] = cosf(v[1]); v[2] = cosf(v[2]); v[3] = cosf(v[3]);
    }
    void _cdecl_jit_pow_ps_ref(float* v, const float* y) {
        v[0] = powf(v[0], y[0]); v[1] = powf(v[1], y[1]); v[2] = powf(v[2], y[2]); v[3] = powf(v[3], y[3]);
    }
    void _cdecl_jit_mod_ps_ref(float* v, const float* y) {
        v[0] = fmodf(v[0], y[0]); v[1] = fmodf(v[1], y[1]); v[2] = fmodf(v[2], y[2]); v[3] = fmodf(v[3], y[3]);
    }
}
#include <cstdio>

namespace mg {

struct JitCompiler::Impl {
    asmjit::JitRuntime runtime;
    
    // To keep track of generated functions so we can release them later
    std::vector<SdfJitFunc4> generated_funcs;
    
    ~Impl() {
        for (auto fn : generated_funcs) {
            runtime.release(fn);
        }
    }
};

JitCompiler::JitCompiler() : _impl(new Impl()) {}

JitCompiler::~JitCompiler() {
    delete _impl;
}

bool JitCompiler::init() {
    return true; // JitRuntime initializes itself
}

SdfJitFunc4 JitCompiler::compileNode(const uint8_t* bc, size_t bc_len, uint32_t node_idx) {
    if (!bc || bc_len == 0) return nullptr;

    using namespace asmjit;
    
    CodeHolder code;
    code.init(_impl->runtime.environment());

    x86::Compiler cc(&code);
    
    FuncNode* func = cc.add_func(FuncSignature::build<
        void, 
        const float*, const float*, const float*, float,
        float*, int,
        const float*, const float*, const float*,
        const float*, const float*, const float*,
        const float*, const float*, const float*
    >());

    x86::Gp arg_x = cc.new_gp_ptr("arg_x");
    x86::Gp arg_y = cc.new_gp_ptr("arg_y");
    x86::Gp arg_z = cc.new_gp_ptr("arg_z");
    x86::Vec arg_w = cc.new_xmm_ss("arg_w");
    x86::Gp arg_result = cc.new_gp_ptr("arg_result");

    func->set_arg(0, arg_x);
    func->set_arg(1, arg_y);
    func->set_arg(2, arg_z);
    func->set_arg(3, arg_w);
    func->set_arg(4, arg_result);

    // Load x, y, z arrays (size 4 floats = 16 bytes = 1 Xmm)
    x86::Vec xs = cc.new_xmm_ps("xs");
    x86::Vec ys = cc.new_xmm_ps("ys");
    x86::Vec zs = cc.new_xmm_ps("zs");
    x86::Vec ws = cc.new_xmm_ps("ws");

    cc.movups(xs, x86::ptr(arg_x));
    cc.movups(ys, x86::ptr(arg_y));
    cc.movups(zs, x86::ptr(arg_z));

    // Broadcast w
    cc.shufps(arg_w, arg_w, 0);
    cc.movaps(ws, arg_w);

    // Mask for ABS (clear sign bit)
    x86::Gp gp_abs = cc.new_gp32();
    cc.mov(gp_abs, 0x7FFFFFFF);
    x86::Vec mask_abs = cc.new_xmm_ps();
    cc.movd(mask_abs, gp_abs);
    cc.shufps(mask_abs, mask_abs, 0);

    // Mask for NEG (toggle sign bit)
    x86::Gp gp_neg = cc.new_gp32();
    cc.mov(gp_neg, 0x80000000);
    x86::Vec mask_neg = cc.new_xmm_ps();
    cc.movd(mask_neg, gp_neg);
    cc.shufps(mask_neg, mask_neg, 0);

    std::vector<x86::Vec> stack;
    size_t pc = 0;
    while (pc < bc_len) {
        OntOpcode op = (OntOpcode)bc[pc];
        float c = 0.0f;
        if (op == ONT_CONST) {
            memcpy(&c, bc + pc + 1, sizeof(float));
        }
        pc += 5;

        switch (op) {
            case ONT_CONST: {
                uint32_t c_bits;
                memcpy(&c_bits, &c, 4);
                x86::Gp tmp = cc.new_gp32();
                cc.mov(tmp, c_bits);
                x86::Vec v = cc.new_xmm_ps();
                cc.movd(v, tmp);
                cc.shufps(v, v, 0);
                stack.push_back(v);
            } break;
            case ONT_VAR_X: stack.push_back(xs); break;
            case ONT_VAR_Y: stack.push_back(ys); break;
            case ONT_VAR_Z: stack.push_back(zs); break;
            case ONT_VAR_W: stack.push_back(ws); break;
            case ONT_ADD: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.addps(r, b);
                    stack.push_back(r);
                }
            } break;
            case ONT_SUB: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.subps(r, b);
                    stack.push_back(r);
                }
            } break;
            case ONT_MUL: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.mulps(r, b);
                    stack.push_back(r);
                }
            } break;
            case ONT_DIV: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec d = cc.new_xmm_ps();
                    cc.movaps(d, a);
                    cc.divps(d, b);

                    x86::Vec zero = cc.new_xmm_ps();
                    cc.xorps(zero, zero);
                    x86::Vec m = cc.new_xmm_ps();
                    cc.movaps(m, b);
                    cc.cmpps(m, zero, 0); // 0 = EQ
                    
                    cc.andnps(m, d);
                    stack.push_back(m);
                }
            } break;
            case ONT_SIN: {
                if (stack.size() >= 1) {
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Mem mem = cc.new_stack(16, 16);
                    cc.movaps(mem, a);
                    x86::Gp ptr = cc.new_gp_ptr();
                    cc.lea(ptr, mem);
                    asmjit::InvokeNode* invoke_node;
                    cc.invoke(asmjit::Out(invoke_node), asmjit::imm((void*)_cdecl_jit_sin_ps_ref), asmjit::FuncSignature::build<void, void*>());
                    invoke_node->set_arg(0, ptr);
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, mem);
                    stack.push_back(r);
                }
            } break;
            case ONT_COS: {
                if (stack.size() >= 1) {
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Mem mem = cc.new_stack(16, 16);
                    cc.movaps(mem, a);
                    x86::Gp ptr = cc.new_gp_ptr();
                    cc.lea(ptr, mem);
                    asmjit::InvokeNode* invoke_node;
                    cc.invoke(asmjit::Out(invoke_node), asmjit::imm((void*)_cdecl_jit_cos_ps_ref), asmjit::FuncSignature::build<void, void*>());
                    invoke_node->set_arg(0, ptr);
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, mem);
                    stack.push_back(r);
                }
            } break;
            case ONT_POW: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Mem mem_a = cc.new_stack(16, 16);
                    x86::Mem mem_b = cc.new_stack(16, 16);
                    cc.movaps(mem_a, a);
                    cc.movaps(mem_b, b);
                    x86::Gp ptr_a = cc.new_gp_ptr();
                    x86::Gp ptr_b = cc.new_gp_ptr();
                    cc.lea(ptr_a, mem_a);
                    cc.lea(ptr_b, mem_b);
                    asmjit::InvokeNode* invoke_node;
                    cc.invoke(asmjit::Out(invoke_node), asmjit::imm((void*)_cdecl_jit_pow_ps_ref), asmjit::FuncSignature::build<void, void*, void*>());
                    invoke_node->set_arg(0, ptr_a);
                    invoke_node->set_arg(1, ptr_b);
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, mem_a);
                    stack.push_back(r);
                }
            } break;
            case ONT_MOD: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Mem mem_a = cc.new_stack(16, 16);
                    x86::Mem mem_b = cc.new_stack(16, 16);
                    cc.movaps(mem_a, a);
                    cc.movaps(mem_b, b);
                    x86::Gp ptr_a = cc.new_gp_ptr();
                    x86::Gp ptr_b = cc.new_gp_ptr();
                    cc.lea(ptr_a, mem_a);
                    cc.lea(ptr_b, mem_b);
                    asmjit::InvokeNode* invoke_node;
                    cc.invoke(asmjit::Out(invoke_node), asmjit::imm((void*)_cdecl_jit_mod_ps_ref), asmjit::FuncSignature::build<void, void*, void*>());
                    invoke_node->set_arg(0, ptr_a);
                    invoke_node->set_arg(1, ptr_b);
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, mem_a);
                    stack.push_back(r);
                }
            } break;
            case ONT_MIN: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.minps(r, b);
                    stack.push_back(r);
                }
            } break;
            case ONT_MAX: {
                if (stack.size() >= 2) {
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.maxps(r, b);
                    stack.push_back(r);
                }
            } break;
            case ONT_SQRT: {
                if (stack.size() >= 1) {
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.sqrtps(r, a);
                    stack.push_back(r);
                }
            } break;
            case ONT_ABS: {
                if (stack.size() >= 1) {
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.andps(r, mask_abs);
                    stack.push_back(r);
                }
            } break;
            case ONT_NEG: {
                if (stack.size() >= 1) {
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.xorps(r, mask_neg);
                    stack.push_back(r);
                }
            } break;
            case ONT_CLAMP: {
                if (stack.size() >= 3) {
                    x86::Vec hi = stack.back(); stack.pop_back();
                    x86::Vec lo = stack.back(); stack.pop_back();
                    x86::Vec v = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    cc.movaps(r, v);
                    cc.maxps(r, lo);
                    cc.minps(r, hi);
                    stack.push_back(r);
                }
            } break;
            case ONT_LERP:
            case ONT_MIX: {
                if (stack.size() >= 3) {
                    x86::Vec t = stack.back(); stack.pop_back();
                    x86::Vec b = stack.back(); stack.pop_back();
                    x86::Vec a = stack.back(); stack.pop_back();
                    x86::Vec r = cc.new_xmm_ps();
                    x86::Vec tmp = cc.new_xmm_ps();
                    cc.movaps(r, a);
                    cc.movaps(tmp, b);
                    cc.subps(tmp, a);
                    cc.mulps(tmp, t);
                    cc.addps(r, tmp);
                    stack.push_back(r);
                }
            } break;
            default: break;
        }
    }

    if (!stack.empty()) {
        x86::Vec res = stack.back();
        cc.movups(x86::ptr(arg_result), res);
    }
    
    cc.ret();
    
    cc.end_func();
    cc.finalize();

    SdfJitFunc4 fn = nullptr;
    Error err = _impl->runtime.add(&fn, &code);

    if (err != asmjit::kErrorOk) {
        fprintf(stderr, "[JIT] AsmJit compilation failed for node %u: %s\n", node_idx, DebugUtils::error_as_string(err));
        return nullptr;
    }

    _impl->generated_funcs.push_back(fn);
    return fn;
}

void JitCompiler::compileScene(OntScene& scene) {
    if (!scene.header || !scene.graph_nodes || !scene.bytecode) return;
    
    scene.jit_functions.resize(scene.header->node_count, nullptr);
    
    for (uint32_t i = 0; i < scene.header->node_count; i++) {
        const auto& g = scene.graph_nodes[i];
        if (g.bytecode_length > 0) {
            const uint8_t* bc = scene.bytecode + g.bytecode_offset;
            scene.jit_functions[i] = compileNode(bc, g.bytecode_length, i);
        }
    }
}

} // namespace mg
