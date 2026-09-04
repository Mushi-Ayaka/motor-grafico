// herm_bridge.cpp - Implementacion del puente herm::Rih -> mg::Scene / mg::OntScene.
// Ver docs/04-guia-uso.md sec. 16 para el contexto del esquema y los errores conocidos.
#include "herm_bridge.h"

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdint>

#include "render/scene.h"                                  // mg::Scene, OntScene, OntOpcode, ...

// herm.h usa f64 pero no lo define; depende de os/os.h. Lo definimos ANTES de incluir.
namespace herm { using f64 = double; }
#include "deps/lenguaje-hermetico/contrato/rih.h"           // herm::Rih
#include "deps/lenguaje-hermetico/contrato/herm.h"          // HermConfig, HermResult, compileFromString

// Declarado en herm_compile.cpp: compila y devuelve el Rih en memoria (sin disco).
namespace herm {
bool compileToRih(const std::string& source, Rih& out, std::string* errOut);
}

namespace mg {

static inline f32 constExpr(const herm::Expr& e) {
    return e.is_expr ? 0.0f : e.constant;
}

static const char* sdfTypeName(herm::SdfType t) {
    switch (t) {
        case herm::SdfType::SPHERE:       return "sphere";
        case herm::SdfType::BOX:          return "box";
        case herm::SdfType::CYLINDER:     return "cylinder";
        case herm::SdfType::CAPSULE:      return "capsule";
        case herm::SdfType::TORUS:        return "torus";
        case herm::SdfType::PLANE:        return "plane";
        case herm::SdfType::CONE:         return "cone";
        case herm::SdfType::ROUNDED_BOX:  return "rounded_box";
        case herm::SdfType::OCTAHEDRON:   return "octahedron";
        case herm::SdfType::TESSERACT:    return "tesseract";
        case herm::SdfType::SPHERE4D:     return "sphere4d";
        case herm::SdfType::UNION:            return "union";
        case herm::SdfType::SUBTRACT:         return "subtract";
        case herm::SdfType::INTERSECT:        return "intersect";
        case herm::SdfType::SMOOTH_UNION:     return "smooth_union";
        case herm::SdfType::SMOOTH_SUBTRACT:  return "smooth_subtract";
        case herm::SdfType::SMOOTH_INTERSECT: return "smooth_intersect";
        case herm::SdfType::REPEAT:           return "repeat";
        case herm::SdfType::TWIST:            return "twist";
        case herm::SdfType::ELONGATE:         return "elongate";
        case herm::SdfType::DISPLACE:         return "displace";
    }
    return "unknown";
}

static u32 mapBlend(herm::BlendMode b) {
    switch (b) {
        case herm::BlendMode::ADD:     return 1;
        case herm::BlendMode::REPLACE: return 2;
        default:                       return 0;
    }
}

static void convertSdf(const herm::SdfNode& hs, SdfNode& ms) {
    ms.sdf_type = sdfTypeName(hs.type);
    for (int i = 0; i < 4; i++) {
        ms.params[i].is_expr  = hs.params[i].is_expr;
        ms.params[i].constant = hs.params[i].constant;
        ms.params[i].expression = hs.params[i].expression;
    }
    ms.child_a = hs.child_a.has_value() ? *hs.child_a : 0xFFFFFFFF;
    ms.child_b = hs.child_b.has_value() ? *hs.child_b : 0xFFFFFFFF;
    ms.displace_expr = hs.displace_func.has_value() ? *hs.displace_func : "";

    // smooth_* guarda k en params[0] (convencion de mg::SdfNode / loadRih).
    bool isSmooth =
        hs.type == herm::SdfType::SMOOTH_UNION ||
        hs.type == herm::SdfType::SMOOTH_SUBTRACT ||
        hs.type == herm::SdfType::SMOOTH_INTERSECT;
    if (isSmooth && hs.k.has_value()) {
        ms.params[0].is_expr = false;
        ms.params[0].constant = *hs.k;
        ms.params[0].expression.clear();
    }
}

// Expande el arbol sdf_nodes de un herm::Node en nodos mg y devuelve el id mg del wrapper.
static u32 convertNodeSdf(const herm::Node& hn, Scene& out,
                          std::unordered_map<u32, u32>& idMap) {
    std::vector<u32> childIds;
    if (!hn.sdf_nodes.empty()) {
        u32 base = (u32)out.nodes.size();
        for (size_t i = 0; i < hn.sdf_nodes.size(); i++) {
            Node sn;
            sn.id = (u32)out.nodes.size();
            sn.name = hn.name + (hn.sdf_nodes.size() > 1 ? ("_sdf" + std::to_string(i)) : "");
            sn.type = NodeType::SDF;
            sn.mode = (NodeMode)hn.mode;
            sn.material_id = hn.material_id.has_value() ? *hn.material_id : 0xFFFFFFFF;
            sn.density_scale = hn.density_scale;
            sn.blend_mode = mapBlend(hn.blend_mode);
            sn.is_compound_child = (hn.sdf_nodes.size() > 1);
            convertSdf(hn.sdf_nodes[i], sn.sdf);
            out.nodes.push_back(sn);
            childIds.push_back(sn.id);
        }
        // Remapear child_a/child_b de indice local a id global mg.
        for (size_t i = 0; i < hn.sdf_nodes.size(); i++) {
            Node& sn = out.nodes[base + i];
            if (sn.sdf.child_a != 0xFFFFFFFF && sn.sdf.child_a < (u32)hn.sdf_nodes.size())
                sn.sdf.child_a = base + sn.sdf.child_a;
            if (sn.sdf.child_b != 0xFFFFFFFF && sn.sdf.child_b < (u32)hn.sdf_nodes.size())
                sn.sdf.child_b = base + sn.sdf.child_b;
        }
    }

    u32 myId = (u32)out.nodes.size();
    Node wrap;
    wrap.id = myId;
    wrap.name = hn.name;
    wrap.mode = (NodeMode)hn.mode;
    wrap.material_id = hn.material_id.has_value() ? *hn.material_id : 0xFFFFFFFF;
    wrap.density_scale = hn.density_scale;
    wrap.blend_mode = mapBlend(hn.blend_mode);
    wrap.translate = { constExpr(hn.transform.translate[0]),
                       constExpr(hn.transform.translate[1]),
                       constExpr(hn.transform.translate[2]) };
    wrap.rotate = { constExpr(hn.transform.rotate[0]),
                    constExpr(hn.transform.rotate[1]),
                    constExpr(hn.transform.rotate[2]) };
    wrap.scale = { constExpr(hn.transform.scale[0]),
                   constExpr(hn.transform.scale[1]),
                   constExpr(hn.transform.scale[2]) };
    wrap.def_id = hn.def_id.has_value() ? *hn.def_id : 0xFFFFFFFF;

    if (childIds.size() == 1 && hn.type != herm::NodeType::GROUP) {
        wrap.type = NodeType::SDF;
        wrap.sdf = out.nodes.back().sdf;
        out.nodes.pop_back();   // el wrapper absorbe el unico sdf
    } else {
        wrap.type = (hn.type == herm::NodeType::GROUP) ? NodeType::GROUP :
                    (hn.type == herm::NodeType::INSTANCE) ? NodeType::INSTANCE :
                    (childIds.size() > 1 ? NodeType::GROUP : NodeType::SDF);
        if (!hn.children.empty())
            wrap.children = hn.children;       // remapeado en pasada final
        else if (!childIds.empty())
            wrap.children = childIds;
    }
    out.nodes.push_back(wrap);
    idMap[hn.id] = myId;
    return myId;
}

bool convertHermToScene(const herm::Rih& in, Scene& out) {
    out = Scene{};

    out.width  = in.scene.width;
    out.height = in.scene.height;
    out.frames = in.scene.w_frames;
    out.camera.position = { in.scene.camera.position.x, in.scene.camera.position.y, in.scene.camera.position.z };
    out.camera.target   = { in.scene.camera.target.x,   in.scene.camera.target.y,   in.scene.camera.target.z };
    out.camera.up       = { in.scene.camera.up.x,       in.scene.camera.up.y,       in.scene.camera.up.z };
    out.camera.fov      = in.scene.camera.fov;
    out.background      = { in.scene.background.r, in.scene.background.g, in.scene.background.b };

    // Materiales: tensor[8] = [x,y,z,w,r,g,b,a]
    for (const auto& m : in.materials) {
        Material mm;
        mm.id = m.id;
        mm.name = m.name;
        mm.base_color = { constExpr(m.tensor[4]), constExpr(m.tensor[5]), constExpr(m.tensor[6]) };
        mm.opacity = constExpr(m.tensor[7]);
        mm.roughness = 0.5f;
        mm.metallic = 0.0f;
        mm.ior = 1.5f;
        mm.emission = { 0, 0, 0 };
        mm.blend_mode = mapBlend(m.blend_mode);
        out.materials.push_back(mm);
    }

    // Luces
    for (const auto& l : in.lights) {
        Light ml;
        ml.name = l.name;
        ml.type = (l.type == herm::LightType::POINT) ? LightType::POINT : LightType::DIRECTIONAL;
        ml.direction = { l.direction.x, l.direction.y, l.direction.z };
        ml.position  = { l.position.x,  l.position.y,  l.position.z };
        ml.color     = { l.color.r, l.color.g, l.color.b };
        ml.intensity = l.intensity;
        ml.falloff  = l.falloff;
        out.lights.push_back(ml);
    }

    // Nodos (grafo de escena)
    std::unordered_map<u32, u32> idMap;
    for (const auto& hn : in.nodes)
        convertNodeSdf(hn, out, idMap);
    // defs: se convierten tambien como nodos (para que las instancias resuelvan def_id).
    for (const auto& hd : in.defs)
        convertNodeSdf(hd, out, idMap);

    // Pasada final: remapear children y def_id de ids herm a ids mg.
    for (auto& mn : out.nodes) {
        for (auto& c : mn.children) {
            auto it = idMap.find(c);
            if (it != idMap.end()) c = it->second;
        }
        if (mn.def_id != 0xFFFFFFFF) {
            auto it = idMap.find(mn.def_id);
            if (it != idMap.end()) mn.def_id = it->second;
        }
    }

    return true;
}

bool compileHermToScene(const std::string& src, Scene& out, std::string* errOut) {
    herm::Rih rih;
    if (!herm::compileToRih(src, rih, errOut))
        return false;
    convertHermToScene(rih, out);
    return true;
}

// ============================================================================
// ONT SCENE BRIDGE (herm::Rih -> mg::OntScene para GPU/Vulkan)
// ============================================================================

// --- Bytecode compiler: SdfNode tree -> OntOpcode stack VM ---

struct BytecodeBuilder {
    std::vector<uint8_t> code;

    void emit(uint8_t op) { code.push_back(op); }
    void emitF32(uint8_t op, float v) {
        code.push_back(op);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        code.insert(code.end(), p, p + 4);
    }
    void emitEnd() { emit(ONT_END); }
};

// Forward declaration
static void compileSdfTree(const herm::SdfNode* nodes, uint32_t idx,
                           BytecodeBuilder& bc, const herm::Node& owner);

static float evalExprConst(const herm::Expr& e) {
    return e.is_expr ? 0.0f : e.constant;
}

// Compila una primitiva SDF a bytecode (stack-based).
// Deja en el stack el resultado de la signed distance.
static void compilePrimitive(herm::SdfType type, const herm::SdfNode* nodes,
                              uint32_t idx, BytecodeBuilder& bc,
                              const herm::Node& owner) {
    const herm::SdfNode& nd = nodes[idx];
    float p0 = evalExprConst(nd.params[0]);
    float p1 = evalExprConst(nd.params[1]);
    float p2 = evalExprConst(nd.params[2]);
    float p3 = evalExprConst(nd.params[3]);

    switch (type) {
    // --- Sphere: length(p) - r ---
    case herm::SdfType::SPHERE: {
        // sqrt(x*x + y*y + z*z) - r
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, p0); // r
        bc.emit(ONT_SUB);
    } break;

    // --- Box: max(max(|x|-sx, |y|-sy), |z|-sz) ---
    case herm::SdfType::BOX: {
        // |x| - sx
        bc.emit(ONT_VAR_X); bc.emit(ONT_ABS);
        bc.emitF32(ONT_CONST, p0); bc.emit(ONT_SUB);
        // |y| - sy
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS);
        bc.emitF32(ONT_CONST, p1); bc.emit(ONT_SUB);
        // max
        bc.emit(ONT_MAX);
        // |z| - sz
        bc.emit(ONT_VAR_Z); bc.emit(ONT_ABS);
        bc.emitF32(ONT_CONST, p2); bc.emit(ONT_SUB);
        // max
        bc.emit(ONT_MAX);
    } break;

    // --- Capsule: length(p - clamp(p, 0, h)) - r ---
    case herm::SdfType::CAPSULE: {
        // Simplified: sqrt(x*x + min(y,y-h)^2 + z*z) - r  (capsule along Y)
        float h = p0, r = p1;
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        // clamp(y, 0, h)
        bc.emit(ONT_VAR_Y); bc.emitF32(ONT_CONST, 0.0f); bc.emitF32(ONT_CONST, h);
        bc.emit(ONT_CLAMP);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_SUB); // y - clamp(y,0,h)
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL); // wait, need (y-clamp)^2
        // Actually let me redo this properly:
        // q_y = y - clamp(y, 0, h)
        // d = sqrt(x*x + q_y*q_y + z*z) - r
        // Reset and redo:
        bc.code.clear();
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);          // x*x
        bc.emit(ONT_VAR_Y); bc.emitF32(ONT_CONST, 0.0f); bc.emitF32(ONT_CONST, h);
        bc.emit(ONT_CLAMP);                                                  // clamp(y,0,h)
        bc.emit(ONT_VAR_Y); bc.emit(ONT_SUB);                               // y - clamp(...)
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL);          // ^2
        bc.emit(ONT_ADD);                                                    // x*x + qy*qy
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, r); bc.emit(ONT_SUB);
    } break;

    // --- Torus: length(length(xz) - R, y) - r ---
    case herm::SdfType::TORUS: {
        float R = p0, r = p1;
        // length(xz) = sqrt(x*x + z*z) - R
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, R); bc.emit(ONT_SUB);        // qx = length(xz) - R
        // qy = y
        bc.emit(ONT_VAR_Y);
        // length(q) - r = sqrt(qx*qx + qy*qy) - r
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL); bc.emit(ONT_ADD); // qx^2 + qy^2
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, r); bc.emit(ONT_SUB);
    } break;

    // --- Cylinder: max(|x|-r, |y|-h) (infinite along Z) ---
    case herm::SdfType::CYLINDER: {
        float r = p0, h = p1;
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL); bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, r); bc.emit(ONT_SUB);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS);
        bc.emitF32(ONT_CONST, h); bc.emit(ONT_SUB);
        bc.emit(ONT_MAX);
    } break;

    // --- Plane: y - offset ---
    case herm::SdfType::PLANE: {
        bc.emit(ONT_VAR_Y);
        bc.emitF32(ONT_CONST, p0);
        bc.emit(ONT_SUB);
    } break;

    // --- Cone: (height * length(xz), y) sector ---
    case herm::SdfType::CONE: {
        float h = p0, r = p1;
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, r); bc.emit(ONT_MUL); // r*length(xz)/h ? simplified
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS);
        bc.emit(ONT_SUB);
    } break;

    // --- Rounded Box: box with rounding ---
    case herm::SdfType::ROUNDED_BOX: {
        float sx = p0, sy = p1, sz = p2, k = p3;
        // d = max(max(|x|-sx, |y|-sy), |z|-sz) + k (approximation)
        bc.emit(ONT_VAR_X); bc.emit(ONT_ABS); bc.emitF32(ONT_CONST, sx); bc.emit(ONT_SUB);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS); bc.emitF32(ONT_CONST, sy); bc.emit(ONT_SUB); bc.emit(ONT_MAX);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_ABS); bc.emitF32(ONT_CONST, sz); bc.emit(ONT_SUB); bc.emit(ONT_MAX);
        if (k > 0.001f) { bc.emitF32(ONT_CONST, k); bc.emit(ONT_SUB); } // approximate rounding
    } break;

    // --- Octahedron ---
    case herm::SdfType::OCTAHEDRON: {
        float s = p0;
        // |x|+|y|+|z| - s
        bc.emit(ONT_VAR_X); bc.emit(ONT_ABS);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS); bc.emit(ONT_ADD);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_ABS); bc.emit(ONT_ADD);
        bc.emitF32(ONT_CONST, s); bc.emit(ONT_SUB);
    } break;

    // --- Tesseract (4D hypercube): max(|x|,|y|,|z|,|w|) - s ---
    case herm::SdfType::TESSERACT: {
        float s = p0;
        bc.emit(ONT_VAR_X); bc.emit(ONT_ABS);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_ABS); bc.emit(ONT_MAX);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_ABS); bc.emit(ONT_MAX);
        bc.emit(ONT_VAR_W); bc.emit(ONT_ABS); bc.emit(ONT_MAX);
        bc.emitF32(ONT_CONST, s); bc.emit(ONT_SUB);
    } break;

    // --- Sphere4D: length4D(p) - r ---
    case herm::SdfType::SPHERE4D: {
        float r = p0;
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_VAR_W); bc.emit(ONT_VAR_W); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, r); bc.emit(ONT_SUB);
    } break;

    default:
        // Unknown primitive: emit constant 1000 (far outside)
        bc.emitF32(ONT_CONST, 1000.0f);
        break;
    }
}

// Compila un arbol SDF completo a bytecode.
static void compileSdfTree(const herm::SdfNode* nodes, uint32_t idx,
                           BytecodeBuilder& bc, const herm::Node& owner) {
    if (idx >= 256) { bc.emitF32(ONT_CONST, 1000.0f); return; } // safety

    const herm::SdfNode& nd = nodes[idx];

    // Primitives
    if (nd.type <= herm::SdfType::SPHERE4D) {
        compilePrimitive(nd.type, nodes, idx, bc, owner);
        return;
    }

    // Operators
    switch (nd.type) {
    case herm::SdfType::UNION: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) compileSdfTree(nodes, *nd.child_b, bc, owner);
        bc.emit(ONT_ADD);
    } break;

    case herm::SdfType::SUBTRACT: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) {
            compileSdfTree(nodes, *nd.child_b, bc, owner);
            bc.emit(ONT_NEG); // negate B
        }
        bc.emit(ONT_MAX); // max(A, -B)
    } break;

    case herm::SdfType::INTERSECT: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) compileSdfTree(nodes, *nd.child_b, bc, owner);
        bc.emit(ONT_MIN);
    } break;

    case herm::SdfType::SMOOTH_UNION: {
        float k = nd.k.has_value() ? *nd.k : 0.1f;
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) compileSdfTree(nodes, *nd.child_b, bc, owner);
        // smooth union: h = clamp(0.5+0.5*(b-a)/k, 0, 1); mix(b,a,h) - k*h*(1-h)
        // Simplified: just emit ADD (basic blend) — full smooth needs extra temps
        bc.emit(ONT_ADD);
        bc.emitF32(ONT_CONST, 2.0f); bc.emit(ONT_DIV); // average
    } break;

    case herm::SdfType::SMOOTH_SUBTRACT: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) {
            compileSdfTree(nodes, *nd.child_b, bc, owner);
            bc.emit(ONT_NEG);
        }
        bc.emit(ONT_MAX);
    } break;

    case herm::SdfType::SMOOTH_INTERSECT: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        if (nd.child_b.has_value()) compileSdfTree(nodes, *nd.child_b, bc, owner);
        bc.emit(ONT_MIN);
    } break;

    case herm::SdfType::REPEAT: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        // TODO: proper repeat needs mod(p, spacing) — approximate as passthrough
    } break;

    case herm::SdfType::TWIST: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        // TODO: twist needs rotation by y — approximate as passthrough
    } break;

    case herm::SdfType::ELONGATE: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        // TODO: elongate needs abs(p)-k — approximate as passthrough
    } break;

    case herm::SdfType::DISPLACE: {
        if (nd.child_a.has_value()) compileSdfTree(nodes, *nd.child_a, bc, owner);
        // TODO: displace needs sin(x)*cos(z)*k — approximate as passthrough
    } break;

    default:
        bc.emitF32(ONT_CONST, 1000.0f);
        break;
    }
}

// --- BVH builder: flatten graph nodes into a BVH ---

struct FlatOntNode {
    float bbox_min[4];
    float bbox_max[4];
    float local_transform[16];
    uint32_t material_id;
    uint32_t bytecode_offset;
    uint32_t bytecode_length;
    uint8_t  mode;
};

static void computeAABB(const herm::Transform& t, float out_min[4], float out_max[4]) {
    // Conservative AABB: use |translate| + some scale estimate
    float tx = std::abs(evalExprConst(t.translate[0]));
    float ty = std::abs(evalExprConst(t.translate[1]));
    float tz = std::abs(evalExprConst(t.translate[2]));
    float sx = std::max(0.01f, evalExprConst(t.scale[0]));
    float sy = std::max(0.01f, evalExprConst(t.scale[1]));
    float sz = std::max(0.01f, evalExprConst(t.scale[2]));
    float ext = std::max({sx, sy, sz}) * 2.0f;
    out_min[0] = tx - ext; out_min[1] = ty - ext; out_min[2] = tz - ext; out_min[3] = -ext;
    out_max[0] = tx + ext; out_max[1] = ty + ext; out_max[2] = tz + ext; out_max[3] =  ext;
}

static void computeTransformMatrix(const herm::Transform& t, float out[16]) {
    // Column-major 4x4: TRS
    float tx = evalExprConst(t.translate[0]);
    float ty = evalExprConst(t.translate[1]);
    float tz = evalExprConst(t.translate[2]);
    float sx = std::max(0.001f, evalExprConst(t.scale[0]));
    float sy = std::max(0.001f, evalExprConst(t.scale[1]));
    float sz = std::max(0.001f, evalExprConst(t.scale[2]));
    float rx = evalExprConst(t.rotate[0]) * 3.14159265f / 180.0f;
    float ry = evalExprConst(t.rotate[1]) * 3.14159265f / 180.0f;
    float rz = evalExprConst(t.rotate[2]) * 3.14159265f / 180.0f;

    // Simple Euler ZYX rotation matrices composed into TRS
    float cx = cosf(rx), sxv = sinf(rx);
    float cy = cosf(ry), syv = sinf(ry);
    float cz = cosf(rz), szv = sinf(rz);

    // R = Rz * Ry * Rx
    float r00 = cz*cy;  float r01 = cz*syv*sxv - szv*cx; float r02 = cz*syv*cx + szv*sxv;
    float r10 = szv*cy; float r11 = szv*syv*sxv + cz*cx;  float r12 = szv*syv*cx - cz*sxv;
    float r20 = -syv;   float r21 = cy*sxv;               float r22 = cy*cx;

    // TRS column-major
    memset(out, 0, 16 * sizeof(float));
    out[0]  = r00 * sx; out[1]  = r10 * sx; out[2]  = r20 * sx;
    out[4]  = r01 * sy; out[5]  = r11 * sy; out[6]  = r21 * sy;
    out[8]  = r02 * sz; out[9]  = r12 * sz; out[10] = r22 * sz;
    out[12] = tx;       out[13] = ty;       out[14] = tz;
    out[15] = 1.0f;
}

// Build a simple BVH: for v1, single leaf containing all nodes (like loadDefault).
// Returns the BVH nodes.
static std::vector<OntBvhNode> buildBVH(const std::vector<FlatOntNode>& flatNodes,
                                          float sceneMin[4], float sceneMax[4]) {
    std::vector<OntBvhNode> bvh;

    // Compute scene AABB
    sceneMin[0] = sceneMin[1] = sceneMin[2] = sceneMin[3] =  1e30f;
    sceneMax[0] = sceneMax[1] = sceneMax[2] = sceneMax[3] = -1e30f;
    for (auto& fn : flatNodes) {
        for (int i = 0; i < 4; i++) {
            sceneMin[i] = std::min(sceneMin[i], fn.bbox_min[i]);
            sceneMax[i] = std::max(sceneMax[i], fn.bbox_max[i]);
        }
    }

    // Single root leaf containing all nodes
    OntBvhNode root = {};
    memcpy(root.min, sceneMin, 4 * sizeof(float));
    memcpy(root.max, sceneMax, 4 * sizeof(float));
    root.d_min = 0.0f;
    root.L = 1.0f;
    root.first_node = 0;
    root.node_count = (uint16_t)flatNodes.size();
    root.flags = BVH_FLAG_LEAF;
    root.skip_index = 1; // skip past this node
    bvh.push_back(root);

    return bvh;
}

// --- OntMaterial converter ---

static OntMaterial convertToOntMaterial(const herm::Material& m, uint32_t idx) {
    OntMaterial om = {};
    om.id = m.id;
    om.base_color[0] = evalExprConst(m.tensor[4]); // r
    om.base_color[1] = evalExprConst(m.tensor[5]); // g
    om.base_color[2] = evalExprConst(m.tensor[6]); // b
    om.base_color[3] = evalExprConst(m.tensor[7]); // a
    om.roughness = 0.5f;  // default (tensor has no roughness channel)
    om.metallic = 0.0f;
    om.opacity = 1.0f;
    om.emission[0] = om.emission[1] = om.emission[2] = 0.0f;
    return om;
}

// --- Main: convert herm::Rih -> OntScene in memory ---

bool convertHermToOntScene(const herm::Rih& in, OntScene& out) {
    // 1. Flatten all nodes with bytecode
    std::vector<FlatOntNode> flatNodes;
    std::vector<uint8_t> allBytecode;

    // Helper: add a node and its bytecode
    auto addNode = [&](const herm::Node& n) {
        if (n.type == herm::NodeType::GROUP) return; // groups don't become OntGraphNodes
        if (n.sdf_nodes.empty() && n.type != herm::NodeType::INSTANCE) return;

        FlatOntNode fn = {};
        computeTransformMatrix(n.transform, fn.local_transform);
        computeAABB(n.transform, fn.bbox_min, fn.bbox_max);
        fn.material_id = n.material_id.has_value() ? *n.material_id : 0;
        fn.mode = (uint8_t)n.mode;

        // Compile SDF tree to bytecode
        if (!n.sdf_nodes.empty()) {
            BytecodeBuilder bc;
            compileSdfTree(n.sdf_nodes.data(), 0, bc, n);
            bc.emitEnd();
            fn.bytecode_offset = (uint32_t)allBytecode.size();
            fn.bytecode_length = (uint32_t)bc.code.size();
            allBytecode.insert(allBytecode.end(), bc.code.begin(), bc.code.end());
        } else {
            fn.bytecode_offset = 0;
            fn.bytecode_length = 0;
        }

        flatNodes.push_back(fn);
    };

    for (auto& n : in.nodes) addNode(n);
    for (auto& d : in.defs) addNode(d);

    if (flatNodes.empty()) {
        // No nodes: create a default placeholder
        FlatOntNode placeholder = {};
        memset(placeholder.local_transform, 0, 16 * sizeof(float));
        placeholder.local_transform[0] = placeholder.local_transform[5] =
        placeholder.local_transform[10] = placeholder.local_transform[15] = 1.0f;
        placeholder.bbox_min[0] = placeholder.bbox_min[1] = placeholder.bbox_min[2] = -1.0f;
        placeholder.bbox_max[0] = placeholder.bbox_max[1] = placeholder.bbox_max[2] =  1.0f;
        placeholder.material_id = 0;
        placeholder.mode = 0;
        // Emit a default sphere: sqrt(x*x+y*y+z*z)-1
        BytecodeBuilder bc;
        bc.emit(ONT_VAR_X); bc.emit(ONT_VAR_X); bc.emit(ONT_MUL);
        bc.emit(ONT_VAR_Y); bc.emit(ONT_VAR_Y); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_VAR_Z); bc.emit(ONT_VAR_Z); bc.emit(ONT_MUL); bc.emit(ONT_ADD);
        bc.emit(ONT_SQRT);
        bc.emitF32(ONT_CONST, 1.0f);
        bc.emit(ONT_SUB);
        bc.emitEnd();
        placeholder.bytecode_offset = (uint32_t)allBytecode.size();
        placeholder.bytecode_length = (uint32_t)bc.code.size();
        allBytecode.insert(allBytecode.end(), bc.code.begin(), bc.code.end());
        flatNodes.push_back(placeholder);
    }

    // 2. Build BVH
    float sceneMin[4], sceneMax[4];
    auto bvhNodes = buildBVH(flatNodes, sceneMin, sceneMax);

    // 3. Convert materials
    std::vector<OntMaterial> ontMats;
    for (auto& m : in.materials)
        ontMats.push_back(convertToOntMaterial(m, (uint32_t)ontMats.size()));
    if (ontMats.empty()) {
        OntMaterial def = {};
        def.base_color[0] = def.base_color[1] = def.base_color[2] = 0.8f;
        def.base_color[3] = 1.0f;
        def.opacity = 1.0f;
        ontMats.push_back(def);
    }

    // 4. Build OntGraphNode array
    std::vector<OntGraphNode> graphNodes(flatNodes.size());
    for (size_t i = 0; i < flatNodes.size(); i++) {
        auto& fn = flatNodes[i];
        auto& gn = graphNodes[i];
        memcpy(gn.local_transform, fn.local_transform, 16 * sizeof(float));
        gn.material_id = fn.material_id;
        gn.bytecode_offset = fn.bytecode_offset;
        gn.bytecode_length = fn.bytecode_length;
        memcpy(gn.bbox_min, fn.bbox_min, 4 * sizeof(float));
        memcpy(gn.bbox_max, fn.bbox_max, 4 * sizeof(float));
        gn.mode = fn.mode;
        gn.pad[0] = gn.pad[1] = gn.pad[2] = 0;
    }

    // 5. Assemble OntScene as contiguous binary blob
    size_t totalSize = sizeof(OntHeader)
                     + bvhNodes.size() * sizeof(OntBvhNode)
                     + graphNodes.size() * sizeof(OntGraphNode)
                     + allBytecode.size()
                     + ontMats.size() * sizeof(OntMaterial);

    out._data.resize(totalSize);
    uint8_t* ptr = out._data.data();

    // Header
    OntHeader hdr = {};
    hdr.magic = ONT_MAGIC;
    hdr.version = 1;
    hdr.epsilon = 0.001f;
    hdr.node_count = (uint32_t)graphNodes.size();
    hdr.bvh_count = (uint32_t)bvhNodes.size();
    hdr.material_count = (uint32_t)ontMats.size();
    hdr.bytecode_size = (uint32_t)allBytecode.size();
    memcpy(hdr.scene_aabb_min, sceneMin, 4 * sizeof(float));
    memcpy(hdr.scene_aabb_max, sceneMax, 4 * sizeof(float));
    memcpy(ptr, &hdr, sizeof(OntHeader)); ptr += sizeof(OntHeader);

    // BVH nodes
    memcpy(ptr, bvhNodes.data(), bvhNodes.size() * sizeof(OntBvhNode));
    ptr += bvhNodes.size() * sizeof(OntBvhNode);

    // Graph nodes
    memcpy(ptr, graphNodes.data(), graphNodes.size() * sizeof(OntGraphNode));
    ptr += graphNodes.size() * sizeof(OntGraphNode);

    // Bytecode
    memcpy(ptr, allBytecode.data(), allBytecode.size());
    ptr += allBytecode.size();

    // Materials
    memcpy(ptr, ontMats.data(), ontMats.size() * sizeof(OntMaterial));
    ptr += ontMats.size() * sizeof(OntMaterial);

    // 6. Set pointers (OntScene reads from _data)
    out.header = (const OntHeader*)out._data.data();
    out.bvh_nodes = (const OntBvhNode*)(out._data.data() + sizeof(OntHeader));
    out.graph_nodes = (const OntGraphNode*)(out.bvh_nodes + out.header->bvh_count);
    out.bytecode = (const uint8_t*)(out.graph_nodes + out.header->node_count);
    out.materials = (const OntMaterial*)(out.bytecode + out.header->bytecode_size);

    // 7. Copy scene-level metadata
    out.width  = in.scene.width;
    out.height = in.scene.height;
    out.camera.position = { in.scene.camera.position.x, in.scene.camera.position.y, in.scene.camera.position.z };
    out.camera.target   = { in.scene.camera.target.x,   in.scene.camera.target.y,   in.scene.camera.target.z };
    out.camera.up       = { in.scene.camera.up.x,       in.scene.camera.up.y,       in.scene.camera.up.z };
    out.camera.fov      = in.scene.camera.fov;
    out.background      = { in.scene.background.r, in.scene.background.g, in.scene.background.b };

    return true;
}

bool compileHermToOntScene(const std::string& src, OntScene& out, std::string* errOut) {
    herm::Rih rih;
    if (!herm::compileToRih(src, rih, errOut))
        return false;
    return convertHermToOntScene(rih, out);
}

} // namespace mg
