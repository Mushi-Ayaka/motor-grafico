#pragma once
#include "../os/os.h"
#include "scene.h"
#include <cmath>
#include <cstdint>
#include <vector>
#include <cstring>

namespace mg {

// ============================================================================
// Bytecode VM — lightweight executor for .ont raw bytecode format
// Instruction format: [u8 opcode] [f32 constant] = 5 bytes
// ============================================================================

// Execute .ont raw bytecode
// bc: pointer to bytecode block, bc_len: total bytes
// Returns the SDF distance value
inline f32 execBcRaw(const uint8_t* bc, size_t bc_len, f32 x, f32 y, f32 z, f32 w,
                     f32 nx = 0, f32 ny = 0, f32 nz = 0,
                     f32 vx = 0, f32 vy = 0, f32 vz = 0,
                     f32 lx = 0, f32 ly = 0, f32 lz = 0,
                     int depth = 0) {
    if (!bc || bc_len == 0) return 0.0f;
    f32 stack[128];
    int sp = -1;
    auto push = [&](f32 v) { if (sp < 126) stack[++sp] = v; };
    auto pop = [&]() -> f32 { return sp >= 0 ? stack[sp--] : 0.0f; };

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
            case ONT_CONST:   push(c); break;
            case ONT_VAR_X:   push(x); break;
            case ONT_VAR_Y:   push(y); break;
            case ONT_VAR_Z:   push(z); break;
            case ONT_VAR_W:   push(w); break;
            case ONT_VAR_NX:  push(nx); break;
            case ONT_VAR_NY:  push(ny); break;
            case ONT_VAR_NZ:  push(nz); break;
            case ONT_VAR_VX:  push(vx); break;
            case ONT_VAR_VY:  push(vy); break;
            case ONT_VAR_VZ:  push(vz); break;
            case ONT_VAR_LX:  push(lx); break;
            case ONT_VAR_LY:  push(ly); break;
            case ONT_VAR_LZ:  push(lz); break;
            case ONT_ADD: { f32 b = pop(); f32 a = pop(); push(a + b); } break;
            case ONT_SUB: { f32 b = pop(); f32 a = pop(); push(a - b); } break;
            case ONT_MUL: { f32 b = pop(); f32 a = pop(); push(a * b); } break;
            case ONT_DIV: { f32 b = pop(); f32 a = pop(); push(b != 0 ? a / b : 0); } break;
            case ONT_MOD: { f32 b = pop(); f32 a = pop(); push(b != 0 ? std::fmod(a, b) : 0); } break;
            case ONT_NEG: push(-pop()); break;
            case ONT_POW: { f32 b = pop(); f32 a = pop(); push(std::pow(a, b)); } break;
            case ONT_SIN: push(std::sin(pop())); break;
            case ONT_COS: push(std::cos(pop())); break;
            case ONT_TAN: push(std::tan(pop())); break;
            case ONT_ABS: push(std::abs(pop())); break;
            case ONT_SQRT: push(std::sqrt(pop())); break;
            case ONT_FLOOR: push(std::floor(pop())); break;
            case ONT_CEIL: push(std::ceil(pop())); break;
            case ONT_MIN: { f32 b = pop(); f32 a = pop(); push(std::fmin(a, b)); } break;
            case ONT_MAX: { f32 b = pop(); f32 a = pop(); push(std::fmax(a, b)); } break;
            case ONT_CLAMP: { f32 hi = pop(); f32 lo = pop(); f32 v = pop(); push(std::fmax(std::fmin(v, hi), lo)); } break;
            case ONT_LERP:
            case ONT_MIX: { f32 t = pop(); f32 b = pop(); f32 a = pop(); push(a + (b - a) * t); } break;
            case ONT_SAMPLE: {
                f32 dz = pop(); f32 dy = pop(); f32 dx = pop();
                if (depth >= 1) { push(0); break; }
                push(execBcRaw(bc, bc_len, x+dx, y+dy, z+dz, w,
                               nx, ny, nz, vx, vy, vz, lx, ly, lz, depth+1));
                break;
            }
            case ONT_END: return sp >= 0 ? pop() : 0;
            default: break;
        }
    }
    return sp >= 0 ? pop() : 0;
}

// ============================================================================
// Old bytecode format (f32 pairs) — kept for backward compat
// ============================================================================

enum class BcOp : uint8_t {
    BC_CONST = 0, VAR_X, VAR_Y, VAR_Z, VAR_W,
    VAR_NX, VAR_NY, VAR_NZ,
    VAR_VX, VAR_VY, VAR_VZ,
    VAR_LX, VAR_LY, VAR_LZ,
    ADD, SUB, MUL, DIV, MOD,
    NEG, POW,
    SIN, COS, TAN,
    ABS, SQRT, FLOOR, CEIL,
    MIN, MAX,
    CLAMP, LERP, MIX,
    BC_SAMPLE, BC_END
};

inline f32 execBc(const std::vector<f32>& bc, f32 x, f32 y, f32 z, f32 w,
                  f32 nx = 0, f32 ny = 0, f32 nz = 0,
                  f32 vx = 0, f32 vy = 0, f32 vz = 0,
                  f32 lx = 0, f32 ly = 0, f32 lz = 0,
                  int depth = 0) {
    if (bc.empty()) return 0.0f;
    f32 stack[128];
    int sp = -1;

    auto push = [&](f32 v) { if (sp < 126) stack[++sp] = v; };
    auto pop = [&]() -> f32 { return sp >= 0 ? stack[sp--] : 0.0f; };

    size_t pc = 0;
    while (pc + 1 < bc.size()) {
        BcOp op = (BcOp)((uint8_t)bc[pc]);
        f32 c = bc[pc + 1];
        pc += 2;

        switch (op) {
            case BcOp::BC_CONST:  push(c); break;
            case BcOp::VAR_X:  push(x); break;
            case BcOp::VAR_Y:  push(y); break;
            case BcOp::VAR_Z:  push(z); break;
            case BcOp::VAR_W:  push(w); break;
            case BcOp::VAR_NX: push(nx); break;
            case BcOp::VAR_NY: push(ny); break;
            case BcOp::VAR_NZ: push(nz); break;
            case BcOp::VAR_VX: push(vx); break;
            case BcOp::VAR_VY: push(vy); break;
            case BcOp::VAR_VZ: push(vz); break;
            case BcOp::VAR_LX: push(lx); break;
            case BcOp::VAR_LY: push(ly); break;
            case BcOp::VAR_LZ: push(lz); break;
            case BcOp::ADD:  { f32 b = pop(); f32 a = pop(); push(a + b); } break;
            case BcOp::SUB:  { f32 b = pop(); f32 a = pop(); push(a - b); } break;
            case BcOp::MUL:  { f32 b = pop(); f32 a = pop(); push(a * b); } break;
            case BcOp::DIV:  { f32 b = pop(); f32 a = pop(); push(b != 0 ? a / b : 0); } break;
            case BcOp::MOD:  { f32 b = pop(); f32 a = pop(); push(b != 0 ? std::fmod(a, b) : 0); } break;
            case BcOp::NEG:  push(-pop()); break;
            case BcOp::POW:  { f32 b = pop(); f32 a = pop(); push(std::pow(a, b)); } break;
            case BcOp::SIN:  push(std::sin(pop())); break;
            case BcOp::COS:  push(std::cos(pop())); break;
            case BcOp::TAN:  push(std::tan(pop())); break;
            case BcOp::ABS:  push(std::abs(pop())); break;
            case BcOp::SQRT: push(std::sqrt(pop())); break;
            case BcOp::FLOOR: push(std::floor(pop())); break;
            case BcOp::CEIL: push(std::ceil(pop())); break;
            case BcOp::MIN:  { f32 b = pop(); f32 a = pop(); push(std::fmin(a, b)); } break;
            case BcOp::MAX:  { f32 b = pop(); f32 a = pop(); push(std::fmax(a, b)); } break;
            case BcOp::CLAMP: { f32 hi = pop(); f32 lo = pop(); f32 v = pop(); push(std::fmax(std::fmin(v, hi), lo)); } break;
            case BcOp::MIX:  { f32 t = pop(); f32 b = pop(); f32 a = pop(); push(a + (b - a) * t); } break;
            case BcOp::BC_SAMPLE: {
                f32 dz = pop(); f32 dy = pop(); f32 dx = pop();
                if (depth >= 1) push(0);
                else push(execBc(bc, x+dx, y+dy, z+dz, w, nx, ny, nz, vx, vy, vz, lx, ly, lz, depth+1));
                break;
            }
            case BcOp::BC_END: return sp >= 0 ? pop() : 0;
        }
    }
    return sp >= 0 ? pop() : 0;
}

} // namespace mg
