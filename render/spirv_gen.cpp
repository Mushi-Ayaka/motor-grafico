// spirv_gen.cpp — F3: GPU-accelerated SDF shader generation (CPU assembly path)
// Takes optimized OntScene and produces a specialized GLSL compute shader
// with inline SDF functions (no VM stack-based evaluation).
#include "spirv_gen.h"
#include "scene.h"
#include <map>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cmath>

namespace mg {

// ============================================================================
// BYTECODE DECOMPILER (postfix -> infix GLSL expression)
// ============================================================================
std::string SpirvGen::decompileBytecode(const uint8_t* bc, uint32_t length) {
    std::vector<std::string> stack;

    auto pop = [&]() -> std::string {
        if (stack.empty()) return std::string("0.0");
        std::string v = std::move(stack.back());
        stack.pop_back();
        return v;
    };

    uint32_t pc = 0;
    while (pc < length && stack.size() < 512) {
        if (pc + 5 > length) break;
        uint8_t op = bc[pc];
        float c = 0.0f;
        if (op == ONT_CONST) {
            memcpy(&c, &bc[pc + 1], 4);
        }
        pc += 5;

        switch (op) {
            case ONT_CONST: {
                char buf[32];
                if (c == float(int(c)) && fabsf(c) < 1e9f) {
                    snprintf(buf, sizeof(buf), "%d", int(c));
                } else {
                    snprintf(buf, sizeof(buf), "%.9g", c);
                }
                stack.emplace_back(buf);
                break;
            }
            case ONT_VAR_X:  stack.emplace_back("p.x"); break;
            case ONT_VAR_Y:  stack.emplace_back("p.y"); break;
            case ONT_VAR_Z:  stack.emplace_back("p.z"); break;
            case ONT_VAR_W:  stack.emplace_back("w"); break;
            case ONT_VAR_NX: stack.emplace_back("n.x"); break;
            case ONT_VAR_NY: stack.emplace_back("n.y"); break;
            case ONT_VAR_NZ: stack.emplace_back("n.z"); break;
            case ONT_VAR_VX: stack.emplace_back("v.x"); break;
            case ONT_VAR_VY: stack.emplace_back("v.y"); break;
            case ONT_VAR_VZ: stack.emplace_back("v.z"); break;
            case ONT_VAR_LX: stack.emplace_back("l.x"); break;
            case ONT_VAR_LY: stack.emplace_back("l.y"); break;
            case ONT_VAR_LZ: stack.emplace_back("l.z"); break;

            case ONT_ADD: { auto b = pop(), a = pop(); stack.emplace_back("(" + a + " + " + b + ")"); break; }
            case ONT_SUB: { auto b = pop(), a = pop(); stack.emplace_back("(" + a + " - " + b + ")"); break; }
            case ONT_MUL: { auto b = pop(), a = pop(); stack.emplace_back("(" + a + " * " + b + ")"); break; }
            case ONT_DIV: { auto b = pop(), a = pop(); stack.emplace_back("(" + a + " / " + b + ")"); break; }
            case ONT_MOD: { auto b = pop(), a = pop(); stack.emplace_back("(mod(" + a + ", " + b + "))"); break; }
            case ONT_NEG: { auto a = pop(); stack.emplace_back("(-" + a + ")"); break; }
            case ONT_POW: { auto b = pop(), a = pop(); stack.emplace_back("(pow(abs(" + a + "), " + b + "))"); break; }

            case ONT_SIN:  { auto a = pop(); stack.emplace_back("(sin(" + a + "))"); break; }
            case ONT_COS:  { auto a = pop(); stack.emplace_back("(cos(" + a + "))"); break; }
            case ONT_TAN:  { auto a = pop(); stack.emplace_back("(tan(" + a + "))"); break; }
            case ONT_ABS:  { auto a = pop(); stack.emplace_back("(abs(" + a + "))"); break; }
            case ONT_SQRT: { auto a = pop(); stack.emplace_back("(sqrt(abs(" + a + ")))"); break; }
            case ONT_FLOOR:{ auto a = pop(); stack.emplace_back("(floor(" + a + "))"); break; }
            case ONT_CEIL: { auto a = pop(); stack.emplace_back("(ceil(" + a + "))"); break; }

            case ONT_MIN: { auto b = pop(), a = pop(); stack.emplace_back("(min(" + a + ", " + b + "))"); break; }
            case ONT_MAX: { auto b = pop(), a = pop(); stack.emplace_back("(max(" + a + ", " + b + "))"); break; }

            case ONT_CLAMP: {
                auto hi = pop(), lo = pop(), v = pop();
                stack.emplace_back("(clamp(" + v + ", " + lo + ", " + hi + "))");
                break;
            }
            case ONT_LERP:
            case ONT_MIX: {
                auto t = pop(), b = pop(), a = pop();
                stack.emplace_back("(mix(" + a + ", " + b + ", " + t + "))");
                break;
            }

            case ONT_SAMPLE:
                stack.emplace_back("0.0");
                break;

            case ONT_END:
                goto done;

            default:
                stack.emplace_back("0.0");
                break;
        }
    }
done:
    return stack.empty() ? std::string("0.0") : stack.back();
}

// ============================================================================
// MATRIX -> GLSL STRING
// ============================================================================
static std::string mat4ToString(const float* m) {
    std::ostringstream os;
    os << "mat4(";
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            if (col > 0 || row > 0) os << ", ";
            os << m[col * 4 + row];
        }
    }
    os << ")";
    return os.str();
}

// ============================================================================
// F3 GENERATE (optimized path: skip dead nodes, deduplicate bytecode)
// ============================================================================
SpirvGenResult SpirvGen::generate(const OntScene& scene) {
    if (!scene.header || !scene.bytecode || !scene.graph_nodes || !scene.bvh_nodes) {
        return {std::string(), false, "Invalid OntScene: null pointers"};
    }

    uint32_t node_count = scene.header->node_count;
    uint32_t bvh_count  = scene.header->bvh_count;
    const uint8_t* bc   = scene.bytecode;
    uint32_t bc_size    = scene.header->bytecode_size;

    std::map<std::string, int> bc_to_func;
    std::ostringstream sdf_functions;
    sdf_functions << "\n// ===== F3 GENERATED SDF FUNCTIONS (GPU-optimized, deduplicated) =====\n";

    uint32_t func_count = 0;
    uint32_t nodes_processed = 0;

    for (uint32_t gi = 0; gi < node_count; gi++) {
        const OntGraphNode& gn = scene.graph_nodes[gi];

        // F3 optimization: skip culled nodes (bit2 in pad[0])
        if (gn.pad[0] & 0x4) continue;

        uint32_t off = gn.bytecode_offset;
        uint32_t len = gn.bytecode_length;

        if (off + len > bc_size) continue;
        if (len == 0) continue;

        nodes_processed++;

        std::string key((const char*)(bc + off), len);
        auto it = bc_to_func.find(key);
        if (it == bc_to_func.end()) {
            int fid = func_count++;
            bc_to_func[key] = fid;

            std::string expr = decompileBytecode(bc + off, len);
            sdf_functions << "float sdf_" << fid << "(vec3 p, float w) { return " << expr << "; }\n";
        }
    }

    // ------------------------------------------------------------------
    // Phase 2: Generate eval_leaf functions per BVH leaf
    // ------------------------------------------------------------------
    std::ostringstream eval_leaves;
    eval_leaves << "\n// ===== EVAL LEAF FUNCTIONS (F3 generated) =====\n";

    for (uint32_t gi = 0; gi < node_count; gi++) {
        const OntGraphNode& gn = scene.graph_nodes[gi];
        if (gn.pad[0] & 0x4) continue;

        uint32_t off = gn.bytecode_offset;
        uint32_t len = gn.bytecode_length;
        if (off + len > bc_size || len == 0) continue;

        std::string key((const char*)(bc + off), len);
        int fid = bc_to_func[key];

        eval_leaves << "float evalLeaf_" << gi << "(vec3 p, float w) {\n";
        eval_leaves << "    vec3 tp = " << mat4ToString(gn.local_transform) << " * vec4(p, 1.0);\n";
        eval_leaves << "    return sdf_" << fid << "(tp, w);\n";
        eval_leaves << "}\n";
    }

    // ------------------------------------------------------------------
    // Phase 3: evalLeaf() switch
    // ------------------------------------------------------------------
    std::ostringstream eval_switch;
    eval_switch << "\nfloat evalLeaf(vec3 p, float w) {\n";

    bool first = true;
    for (uint32_t gi = 0; gi < node_count; gi++) {
        const OntGraphNode& gn = scene.graph_nodes[gi];
        if (gn.pad[0] & 0x4) continue;
        if (gn.bytecode_length == 0) continue;

        eval_switch << (first ? "    " : "    else ") << "if (leafId == " << gi << ") return evalLeaf_" << gi << "(p, w);\n";
        first = false;
    }
    eval_switch << "    return 1e9;\n}\n";

    // ------------------------------------------------------------------
    // Phase 4: Assemble final shader
    // ------------------------------------------------------------------
    std::ostringstream shader;
    shader << R"(// Auto-generated compute shader (F3 — GPU-accelerated, VM-free)
#version 450
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std430) readonly buffer BvhNodes { float bvh[]; };
layout(set = 0, binding = 1, std430) readonly buffer Materials { float materials[]; };
layout(set = 0, binding = 2) uniform UBO {
    mat4 view;
    vec4 cam_pos;
    vec4 cam_dir;
    uint width;
    uint height;
    float time;
    float epsilon;
    uint node_count;
    uint bvh_count;
    float pad0, pad1;
} ubo;
layout(set = 0, binding = 3, std430) buffer Pixels { uint pixels[]; };
layout(set = 0, binding = 4, std430) readonly buffer Tensors { float tensors[]; };

#define PI 3.14159265359
#define MAX_STEPS 80
#define MAX_DIST 100.0

float sceneSdf(vec3 p, float w);

)" << sdf_functions.str() << eval_leaves.str() << eval_switch.str() << R"(

float sceneSdf(vec3 p, float w) {
    float d = 1e9;
    for (uint i = 0; i < ubo.node_count; i++) {
        float di = evalLeaf(i, p, w);
        d = min(d, di);
    }
    return d;
}

vec3 calcNormal(vec3 p, float w) {
    float e = 0.001;
    return normalize(vec3(
        sceneSdf(p + vec3(e, 0, 0), w) - sceneSdf(p - vec3(e, 0, 0), w),
        sceneSdf(p + vec3(0, e, 0), w) - sceneSdf(p - vec3(0, e, 0), w),
        sceneSdf(p + vec3(0, 0, e), w) - sceneSdf(p - vec3(0, 0, e), w)
    ));
}

float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;
    for (int i = 0; i < 40; i++) {
        float h = sceneSdf(ro + rd * t, 0.0);
        if (h < 0.0005) return 0.0;
        res = min(res, k * h / t);
        t += clamp(h, 0.01, 0.5);
        if (t > maxt) break;
    }
    return res;
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i);
        float d = sceneSdf(p + h * n, 0.0);
        occ += (h - d) * sca;
        sca *= 0.95;
    }
    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}

vec3 shadeOnt(vec3 p, vec3 n, vec3 rd, uint matId) {
    float off = float(matId) * 12.0;
    vec3 base = vec3(materials[int(off)], materials[int(off+1)], materials[int(off+2)]);
    float rough = materials[int(off+3)];
    float metal = materials[int(off+4)];
    vec3 emi = vec3(materials[int(off+5)], materials[int(off+6)], materials[int(off+7)]);
    float opa = materials[int(off+8)];

    vec3 lig1 = normalize(vec3(1.0, 2.0, 1.5));
    vec3 lig2 = normalize(vec3(-1.0, 0.5, -0.5));

    float dif1 = max(dot(n, lig1), 0.0);
    float dif2 = max(dot(n, lig2), 0.0) * 0.3;

    vec3 hal1 = normalize(lig1 - rd);
    float spe1 = pow(max(dot(n, hal1), 0.0), 32.0 / (rough * rough + 0.01));

    float sha = softShadow(p, lig1, 0.01, 10.0, 16.0);
    float ao = calcAO(p, n);

    vec3 diff = base * (1.0 - metal);
    vec3 spec = mix(vec3(1.0), base, metal) * spe1;

    vec3 col = diff * (dif1 * sha + dif2) * ao + spec * sha * ao + emi;

    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
    col += vec3(0.04) * fres * ao;

    return col;
}

void main() {
    uint px = gl_GlobalInvocationID.x;
    uint py = gl_GlobalInvocationID.y;
    if (px >= ubo.width || py >= ubo.height) return;

    float aspect = float(ubo.width) / float(ubo.height);
    float u = (float(px) + 0.5) / float(ubo.width) * 2.0 - 1.0;
    float v = (float(py) + 0.5) / float(ubo.height) * 2.0 - 1.0;

    vec3 right = normalize(cross(vec3(ubo.view[0][2], ubo.view[1][2], ubo.view[2][2]), vec3(0, 1, 0)));
    vec3 up = cross(vec3(ubo.view[0][2], ubo.view[1][2], ubo.view[2][2]), right);

    vec3 ro = ubo.cam_pos.xyz;
    vec3 rd = normalize(vec3(ubo.view[0][2], ubo.view[1][2], ubo.view[2][2]) + right * u * aspect + up * v);

    float t = 0.0;
    bool hit = false;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        float d = sceneSdf(p, ubo.time);
        if (d < ubo.epsilon) { hit = true; break; }
        t += d;
        if (t > MAX_DIST) break;
    }

    vec3 color;
    if (hit) {
        vec3 p = ro + rd * t;
        vec3 n = calcNormal(p, ubo.time);
        // Find which node was hit
        uint hitNode = 0;
        float bestD = 1e9;
        for (uint i = 0; i < ubo.node_count; i++) {
            float di = evalLeaf(i, p, ubo.time);
            if (di < bestD) { bestD = di; hitNode = i; }
        }
        uint matId = uint(materials[int(float(hitNode) * 12.0 + 9.0)]);
        color = shadeOnt(p, n, -rd, matId);
    } else {
        color = vec3(0.1, 0.1, 0.2);
    }

    pixels[py * ubo.width + px] = packRgba(color);
}

vec4 packRgba(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return vec4(c, 1.0);
}
)";

    SpirvGenResult result;
    result.glsl_source = shader.str();
    result.ok = true;
    result.nodes_processed = nodes_processed;
    result.unique_sdfs = func_count;
    return result;
}

} // namespace mg
