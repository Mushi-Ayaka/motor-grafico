#include "glsl_gen.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>

namespace mg {

// ============================================================================
// Template parts of the compute shader (fixed for all scenes)
// ============================================================================

static const char* kVersion = R"(#version 450
#extension GL_EXT_scalar_block_layout : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// ============================================================================
// STRUCTS
// ============================================================================
struct OntBvhNode {
    vec4 min_bounds;
    vec4 max_bounds;
    float d_min;
    float L;
    int skip_index;
    uint first_node;
    uint node_count_flags; // lower 16 = count, upper 16 = flags
};

struct OntMaterial {
    uint id;
    vec4 base_color;
    float roughness;
    float metallic;
    vec3 emission;
    float opacity;
    uint reserved[4];
};

struct UboData {
    vec3 camera_pos;
    vec3 camera_target;
    vec3 camera_up;
    float fov;
    float time;
    float render_scale;
    uint width;
    uint height;
    float tmax;
};

// ============================================================================
// BINDINGS
// ============================================================================
layout(scalar, binding = 0) readonly buffer BvhBuffer     { OntBvhNode bvh_nodes[]; };
layout(scalar, binding = 1) readonly buffer MaterialBuffer { OntMaterial materials[]; };
layout(binding = 2)          uniform      UboBuffer        { UboData ubo; };
layout(scalar, binding = 3)  writeonly    buffer OutputBuffer { uint outPixels[]; };

)";

static const char* kBvhEval = R"(
// ============================================================================
// BVH EVALUATION (uses generated evalLeaf)
// ============================================================================
vec3 bvhEval(vec3 p, float w) {
    float result = 1e9;
    float min_leaf_d_min = 1e9;
    uint best_mat = 0xFFFFFFFF;

    uint node_idx = 0;
    uint bvh_iters = 0;
    while (node_idx < bvh_nodes.length() && bvh_iters < 65535u) {
        bvh_iters++;
        OntBvhNode node = bvh_nodes[node_idx];

        // AABB distance test
        vec3 bmin = node.min_bounds.xyz;
        vec3 bmax = node.max_bounds.xyz;
        vec3 pd = max(vec3(0.0), max(bmin - p, p - bmax));
        float dist_sq = dot(pd, pd);

        if (dist_sq > result * result) {
            node_idx = uint(node.skip_index);
            continue;
        }

        uint count = node.node_count_flags & 0xFFFF;
        uint flags_ = node.node_count_flags >> 16;
        if ((flags_ & 1u) != 0) {
            // Leaf: use generated function
            if (node.d_min < min_leaf_d_min) min_leaf_d_min = node.d_min;

            vec2 leaf_res = evalLeaf(node_idx, p, w);
            float d = leaf_res.x;

            if (d < result) {
                result = d;
                best_mat = floatBitsToUint(leaf_res.y);
            }

            node_idx = uint(node.skip_index);
        } else {
            node_idx++;
        }
    }
    return vec3(result, min_leaf_d_min, uintBitsToFloat(best_mat));
}
)";

static const char* kBvhNormal = R"(
// ============================================================================
// NORMAL (finite differences)
// ============================================================================
vec3 bvhNormal(vec3 p, float w) {
    float eps = 0.001;
    float d = bvhEval(p, w).x;
    float dx = bvhEval(p + vec3(eps, 0, 0), w).x;
    float dy = bvhEval(p + vec3(0, eps, 0), w).x;
    float dz = bvhEval(p + vec3(0, 0, eps), w).x;
    return normalize(vec3(dx - d, dy - d, dz - d));
}
)";

static const char* kShadeOnt = R"(
// ============================================================================
// PBR SHADING (Cook-Torrance GGX)
// ============================================================================
vec3 shadeOnt(vec3 p, vec3 n, vec3 view_dir, uint mat_id) {
    if (mat_id >= materials.length()) return vec3(0.0);
    OntMaterial mat = materials[mat_id];

    vec3 albedo = mat.base_color.xyz;
    float roughness = max(mat.roughness, 0.001);
    float metallic = clamp(mat.metallic, 0.0, 1.0);

    vec3 ldir = normalize(vec3(0.6, -0.6, 0.4));
    vec3 light_col = vec3(1.0, 0.92, 0.8);
    vec3 half_v = normalize(ldir + view_dir);

    float ndotl = max(dot(n, ldir), 0.0);
    float ndotv = max(dot(n, view_dir), 0.001);
    float ndoth = max(dot(n, half_v), 0.001);
    float hdotv = max(dot(half_v, view_dir), 0.001);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // GGX NDF
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    float ndf = a2 / (3.14159265 * denom * denom);

    // Schlick-GGX Geometry
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G1 = ndotl / (ndotl * (1.0 - k) + k);
    float G2 = ndotv / (ndotv * (1.0 - k) + k);
    float G = G1 * G2;

    // Schlick Fresnel
    float Fc = pow(1.0 - hdotv, 5.0);
    vec3 F = F0 + (1.0 - F0) * Fc;

    vec3 specular = (ndf * G * F) / (4.0 * ndotv * ndotl + 0.0001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = albedo * kD * ndotl / 3.14159265;

    // Ambient
    float ambient_up = 0.5 + 0.5 * n.y;
    float ambient_down = 0.5 - 0.5 * n.y;
    vec3 ambient = (vec3(0.4, 0.6, 0.9) * ambient_up + vec3(0.15, 0.1, 0.05) * ambient_down) * albedo * 0.1 * (1.0 - metallic);

    vec3 color = (diffuse + specular * 0.3) * light_col + ambient + mat.emission;

    // Exposure and tone mapping
    color *= 2.0;
    color = color / (1.0 + color);
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    return color;
}
)";

static const char* kPackRgba = R"(
// ============================================================================
// PACK RGBA (B8G8R8A8 for GDI)
// ============================================================================
uint packRgba(vec3 c) {
    uint r = uint(clamp(c.r * 255.0, 0.0, 255.0));
    uint g = uint(clamp(c.g * 255.0, 0.0, 255.0));
    uint b = uint(clamp(c.b * 255.0, 0.0, 255.0));
    uint a = 255;
    return (b) | (g << 8) | (r << 16) | (a << 24);
}
)";

static const char* kMain = R"(
// ============================================================================
// MAIN — Raymarching loop
// ============================================================================
void main() {
    uint px = gl_GlobalInvocationID.x;
    uint py = gl_GlobalInvocationID.y;

    if (px >= ubo.width || py >= ubo.height) return;

    float aspect = float(ubo.width) / float(ubo.height);
    float fov_tan = tan(ubo.fov * 3.14159265 / 360.0);

    vec3 fwd = normalize(ubo.camera_target - ubo.camera_pos);
    vec3 right = normalize(cross(fwd, ubo.camera_up));
    vec3 up = cross(right, fwd);

    float sx = (2.0 * (float(px) + 0.5) / float(ubo.width) - 1.0) * aspect * fov_tan;
    float sy = (1.0 - 2.0 * (float(py) + 0.5) / float(ubo.height)) * fov_tan;

    vec3 ray_dir = normalize(fwd + right * sx + up * sy);
    vec3 ray_origin = ubo.camera_pos;

    float t = 0.01;
    float tmax = ubo.tmax;
    bool hit = false;
    uint hit_mat = 0xFFFFFFFF;

    for (int i = 0; i < 80; i++) {
        vec3 p = ray_origin + ray_dir * t;

        vec3 res = bvhEval(p, ubo.time);
        float d = res.x;
        float leaf_d_min = res.y;
        hit_mat = floatBitsToUint(res.z);

        float step;
        if (d > 0.0) {
            step = max(d, leaf_d_min) * 0.8;
        } else {
            step = d * 0.8;
        }
        t += step;

        float eps = 0.001 * (1.0 + t * 0.01);
        if (abs(d) < eps) {
            hit = true;
            break;
        }
        if (t > tmax) break;
    }

    vec3 color = vec3(0.0);
    if (hit) {
        vec3 p = ray_origin + ray_dir * t;
        vec3 n = bvhNormal(p, ubo.time);
        color = shadeOnt(p, n, -ray_dir, hit_mat);
    } else {
        color = vec3(0.1, 0.1, 0.2);
    }

    outPixels[py * ubo.width + px] = packRgba(color);
}
)";

// ============================================================================
// BYTECODE DECOMPILER (postfix → infix GLSL expression)
// ============================================================================
static std::string decompileBytecode(const uint8_t* bc, uint32_t length) {
    std::vector<std::string> stack;

    auto pop = [&stack]() -> std::string {
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
                if (c == float(int(c))) {
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
// MATRIX → GLSL STRING
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
// GENERATE
// ============================================================================
GlslGenResult GlslGen::generate(const OntScene& scene) {
    if (!scene.header || !scene.bytecode || !scene.graph_nodes || !scene.bvh_nodes) {
        return {std::string(), false, "Invalid OntScene: null pointers"};
    }

    uint32_t node_count  = scene.header->node_count;
    uint32_t bvh_count   = scene.header->bvh_count;
    const uint8_t* bc    = scene.bytecode;
    uint32_t bc_size     = scene.header->bytecode_size;

    // ------------------------------------------------------------------
    // Phase 1: Deduplicate bytecode → generate SDF functions
    // ------------------------------------------------------------------
    std::map<std::string, int> bc_to_func;
    std::ostringstream sdf_functions;
    sdf_functions << "\n// ===== GENERATED SDF FUNCTIONS (deduplicated) =====\n";

    for (uint32_t gi = 0; gi < node_count; gi++) {
        const OntGraphNode& gn = scene.graph_nodes[gi];
        uint32_t off = gn.bytecode_offset;
        uint32_t len = gn.bytecode_length;

        if (off + len > bc_size) continue;
        if (len == 0) continue;

        std::string key((const char*)(bc + off), len);
        if (bc_to_func.find(key) == bc_to_func.end()) {
            int func_idx = (int)bc_to_func.size();
            bc_to_func[key] = func_idx;

            std::string expr = decompileBytecode(bc + off, len);

            sdf_functions << "float sdf_" << func_idx << "(vec3 p, float w) {\n";
            sdf_functions << "    return " << expr << ";\n";
            sdf_functions << "}\n\n";
        }
    }

    // ------------------------------------------------------------------
    // Phase 2: For each BVH leaf, generate eval_leaf_N
    // ------------------------------------------------------------------
    std::ostringstream leaf_functions;
    leaf_functions << "// ===== GENERATED LEAF FUNCTIONS =====\n";

    std::vector<uint32_t> leaf_indices;

    for (uint32_t bi = 0; bi < bvh_count; bi++) {
        const OntBvhNode& node = scene.bvh_nodes[bi];
        if (!(node.flags & BVH_FLAG_LEAF)) continue;

        leaf_indices.push_back(bi);
        uint32_t first = node.first_node;
        uint32_t count = node.node_count;

        leaf_functions << "vec2 eval_leaf_" << bi << "(vec3 p, float w) {\n";
        leaf_functions << "    float best_d = 1e9;\n";
        leaf_functions << "    uint best_mat = 0xFFFFFFFF;\n";

        for (uint32_t i = 0; i < count; i++) {
            uint32_t gi = first + i;
            const OntGraphNode& gn = scene.graph_nodes[gi];

            std::string mat_str = mat4ToString(gn.local_transform);
            float scale = sqrt(gn.local_transform[0] * gn.local_transform[0] +
                               gn.local_transform[1] * gn.local_transform[1] +
                               gn.local_transform[2] * gn.local_transform[2]);

            std::string bc_key((const char*)(bc + gn.bytecode_offset), gn.bytecode_length);
            auto it = bc_to_func.find(bc_key);
            int func_idx = (it != bc_to_func.end()) ? it->second : 0;

            leaf_functions << "    // Graph node " << gi << " (mat_id=" << gn.material_id << ")\n";
            leaf_functions << "    { const mat4 t = " << mat_str << ";\n";
            leaf_functions << "      vec3 lp = (t * vec4(p, 1.0)).xyz;\n";
            leaf_functions << "      float d = sdf_" << func_idx << "(lp, w)";
            if (scale > 1e-8f) leaf_functions << " / " << scale;
            leaf_functions << ";\n";
            if (i == 0) {
                leaf_functions << "      best_d = d; best_mat = " << gn.material_id << "u;\n";
            } else {
                leaf_functions << "      if (d < best_d) { best_d = d; best_mat = " << gn.material_id << "u; }\n";
            }
            leaf_functions << "    }\n";
        }

        leaf_functions << "    return vec2(best_d, uintBitsToFloat(best_mat));\n";
        leaf_functions << "}\n\n";
    }

    // ------------------------------------------------------------------
    // Phase 3: Generate switch dispatcher
    // ------------------------------------------------------------------
    std::ostringstream dispatch;
    dispatch << "// ===== GENERATED EVAL LEAF DISPATCHER =====\n";
    dispatch << "vec2 evalLeaf(uint leaf_idx, vec3 p, float w) {\n";
    dispatch << "    switch (leaf_idx) {\n";
    for (uint32_t li : leaf_indices) {
        dispatch << "        case " << li << "u: return eval_leaf_" << li << "(p, w);\n";
    }
    dispatch << "        default: return vec2(1e9, 0.0);\n";
    dispatch << "    }\n";
    dispatch << "}\n";

    // ------------------------------------------------------------------
    // Phase 4: Assemble full shader
    // ------------------------------------------------------------------
    std::ostringstream full;
    full << kVersion;
    full << sdf_functions.str();
    full << leaf_functions.str();
    full << dispatch.str();
    full << kBvhEval;
    full << kBvhNormal;
    full << kShadeOnt;
    full << kPackRgba;
    full << kMain;

    return {full.str(), true, std::string()};
}

// ============================================================================
// GLSL → SPIR-V compilation via glslc.exe subprocess
// ============================================================================
bool compileGlslToSpv(const std::string& glsl_source,
                      std::vector<uint32_t>& out_spv,
                      std::string& out_error) {
    // Use exe directory for temp files — avoids temp directory path issues
    char exe_dir[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_dir, MAX_PATH);
    char* last = strrchr(exe_dir, '\\');
    if (last) *(last + 1) = '\0';

    std::string base = std::string(exe_dir) + "mg_scene_";
    std::string glsl_path = base + ".comp";
    std::string spv_path  = base + ".spv";

    // Remove leftovers from previous runs
    DeleteFileA(glsl_path.c_str());
    DeleteFileA(spv_path.c_str());

    {
        std::ofstream f(glsl_path, std::ios::binary);
        if (!f.is_open()) {
            out_error = "Cannot write GLSL file to exe directory";
            return false;
        }
        f.write(glsl_source.data(), (std::streamsize)glsl_source.size());
        f.close();
    }

    // Verify GLSL file was written
    {
        std::ifstream test(glsl_path, std::ios::binary | std::ios::ate);
        if (!test.is_open() || test.tellg() == 0) {
            out_error = "GLSL file was not written correctly";
            DeleteFileA(glsl_path.c_str());
            return false;
        }
    }

    // Debug copy
    {
        std::string debug_path = std::string(exe_dir) + "last_gen.comp";
        std::ofstream df(debug_path, std::ios::binary);
        if (df.is_open()) {
            df.write(glsl_source.data(), (std::streamsize)glsl_source.size());
        }
    }

    // Compile with glslc.exe via CreateProcess (more robust than _popen)
    std::string spv_path_native = spv_path;
    std::string glsl_path_native = glsl_path;

    // Build command line for CreateProcess
    std::string args = "\"C:\\VulkanSDK\\1.4.350.0\\Bin\\glslc.exe\" \"" +
                       glsl_path_native + "\" -o \"" + spv_path_native + "\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRd, hStdoutWr;
    CreatePipe(&hStdoutRd, &hStdoutWr, &sa, 0);
    SetHandleInformation(hStdoutRd, HANDLE_FLAG_INHERIT, 0);

    HANDLE hStderrWr;
    DuplicateHandle(GetCurrentProcess(), hStdoutWr, GetCurrentProcess(), &hStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS);

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdoutWr;
    si.hStdError = hStderrWr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    // Writeable temp copy of args for CreateProcess
    std::vector<char> args_buf(args.begin(), args.end());
    args_buf.push_back('\0');

    bool created = CreateProcessA(
        "C:\\VulkanSDK\\1.4.350.0\\Bin\\glslc.exe",
        args_buf.data(),
        nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);

    CloseHandle(hStdoutWr);
    CloseHandle(hStderrWr);

    if (!created) {
        out_error = "CreateProcess glslc.exe failed (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(hStdoutRd);
        DeleteFileA(glsl_path.c_str());
        DeleteFileA(spv_path.c_str());
        return false;
    }

    // Read glslc output
    std::string glslc_output;
    {
        char buf[4096];
        DWORD read = 0;
        while (ReadFile(hStdoutRd, buf, sizeof(buf), &read, nullptr) && read > 0) {
            glslc_output.append(buf, read);
        }
    }
    CloseHandle(hStdoutRd);

    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        out_error = "glslc.exe error code " + std::to_string(exit_code) + ": " + glslc_output;
        // Check if SPV was produced despite error
        std::ifstream test_spv(spv_path, std::ios::binary | std::ios::ate);
        if (!test_spv.is_open()) {
            DeleteFileA(glsl_path.c_str());
            DeleteFileA(spv_path.c_str());
            return false;
        }
        test_spv.close();
    }

    // Read SPIR-V
    std::ifstream spv_file(spv_path, std::ios::binary | std::ios::ate);
    if (!spv_file.is_open()) {
        out_error = "Cannot read compiled SPIR-V output";
        DeleteFileA(glsl_path.c_str());
        DeleteFileA(spv_path.c_str());
        return false;
    }

    size_t file_size = (size_t)spv_file.tellg();
    spv_file.seekg(0);
    std::vector<char> raw(file_size);
    spv_file.read(raw.data(), file_size);
    spv_file.close();

    DeleteFileA(glsl_path.c_str());
    DeleteFileA(spv_path.c_str());

    if (file_size % 4 != 0) {
        out_error = "SPIR-V file size not multiple of 4";
        return false;
    }
    out_spv.resize(file_size / 4);
    memcpy(out_spv.data(), raw.data(), file_size);

    return true;
}

} // namespace mg
