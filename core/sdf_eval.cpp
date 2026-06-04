#include "sdf_eval.h"
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <algorithm>

namespace mg {

// ============================================================================
// Expression evaluator with x,y,z,w context
// ============================================================================

class ExprEval {
public:
    ExprEval(const std::string& s, float x, float y, float z, float w)
        : s(s), x(x), y(y), z(z), w(w), pos(0) {}

    float parse() {
        skipWs();
        float r = expr();
        skipWs();
        if (pos < s.size())
            throw std::runtime_error("bad expr '" + s + "'");
        return r;
    }

private:
    const std::string& s;
    float x, y, z, w;
    size_t pos;

    char peek() { return pos < s.size() ? s[pos] : '\0'; }
    char advance() { return pos < s.size() ? s[pos++] : '\0'; }
    void skipWs() { while (pos < s.size() && std::isspace(s[pos])) pos++; }

    float expr() {
        float r = term();
        skipWs();
        while (peek() == '+' || peek() == '-') {
            char op = advance(); skipWs(); float rhs = term(); r = (op == '+') ? r + rhs : r - rhs;
        }
        return r;
    }

    float term() {
        float r = unary();
        skipWs();
        while (peek() == '*' || peek() == '/') {
            char op = advance(); skipWs(); float rhs = unary(); r = (op == '*') ? r * rhs : r / rhs;
        }
        return r;
    }

    float unary() {
        skipWs();
        if (peek() == '-') { advance(); return -unary(); }
        if (peek() == '+') { advance(); return unary(); }
        return power();
    }

    float power() {
        float r = atom();
        skipWs();
        if (peek() == '^') { advance(); skipWs(); r = std::pow(r, unary()); }
        return r;
    }

    float atom() {
        skipWs();
        char c = peek();

        if (c == '(') {
            advance(); float r = expr(); skipWs(); if (peek() == ')') advance(); return r;
        }

        if (std::isdigit(c) || c == '.') {
            std::string val;
            if (peek() == '-') val += advance();
            while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.')) val += advance();
            return val.empty() ? 0.0f : std::stof(val);
        }

        if (std::isalpha(c) || c == '_') {
            std::string name;
            while (pos < s.size() && (std::isalnum(s[pos]) || s[pos] == '_')) name += advance();

            // Constants
            if (name == "pi") return 3.14159265358979323846f;
            if (name == "e")  return 2.71828182845904523536f;

            // Variables — x,y,z for point, w for time
            if (name == "x") return x;
            if (name == "y") return y;
            if (name == "z") return z;
            if (name == "w" || name == "t") return w;

            // Functions
            skipWs();
            bool hasParen = (peek() == '(');
            if (hasParen) advance();

            auto parseArg = [&]() -> float { skipWs(); return expr(); };

            if (name == "abs")   { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::abs(a); }
            if (name == "sin")   { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::sin(a); }
            if (name == "cos")   { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::cos(a); }
            if (name == "tan")   { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::tan(a); }
            if (name == "sqrt")  { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::sqrt(a); }
            if (name == "floor") { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::floor(a); }
            if (name == "ceil")  { float a = parseArg(); if (hasParen && peek() == ')') advance(); return std::ceil(a); }
            if (name == "length") {
                // length of 3 coords: length(x, y, z) or length(v)
                float a = parseArg(); skipWs();
                if (peek() == ',') { advance(); skipWs(); float b = parseArg(); skipWs(); if (peek() == ',') advance(); skipWs(); float c2 = parseArg(); if (hasParen && peek() == ')') advance(); return std::sqrt(a*a+b*b+c2*c2); }
                if (hasParen && peek() == ')') advance(); return std::abs(a);
            }
            if (name == "max")   { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); if (hasParen && peek() == ')') advance(); return std::fmax(a,b); }
            if (name == "min")   { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); if (hasParen && peek() == ')') advance(); return std::fmin(a,b); }
            if (name == "pow")   { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); if (hasParen && peek() == ')') advance(); return std::pow(a,b); }
            if (name == "clamp") { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); skipWs(); if (peek() == ',') advance(); float c2 = parseArg(); if (hasParen && peek() == ')') advance(); return std::fmax(std::fmin(c2, b), a); }
            if (name == "mix" || name == "lerp") { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); skipWs(); if (peek() == ',') advance(); float t = parseArg(); if (hasParen && peek() == ')') advance(); return a + (b - a) * t; }
            if (name == "dot")   { float a = parseArg(); skipWs(); if (peek() == ',') advance(); float b = parseArg(); skipWs(); if (peek() == ',') advance(); float c2 = parseArg(); skipWs(); float d = parseArg(); if (hasParen && peek() == ')') advance(); return a*b+c2*d; }
            if (name == "normalize") { /* simplified — just returns arg */ float a = parseArg(); if (hasParen && peek() == ')') advance(); return a; }

            if (hasParen) { while (peek() != ')' && pos < s.size()) advance(); if (peek() == ')') advance(); }
            return 0.0f;
        }

        return 0.0f;
    }
};

float evalExpr(const std::string& expr, float x, float y, float z, float w) {
    if (expr.empty()) return 0.0f;
    try {
        ExprEval parser(expr, x, y, z, w);
        return parser.parse();
    } catch (...) {
        return 0.0f;
    }
}

float evalSdfExpr(const Expr& e, float x, float y, float z, float w) {
    if (e.is_expr)
        return evalExpr(e.expression, x, y, z, w);
    return e.constant;
}

// ============================================================================
// SDF primitives — evaluated with x,y,z
// ============================================================================

static float sdSphere(const Vec3& p, float r) {
    return length(p) - r;
}

static float sdBox(const Vec3& p, const Vec3& s) {
    Vec3 q = {std::abs(p.x) - s.x, std::abs(p.y) - s.y, std::abs(p.z) - s.z};
    return length(Vec3{std::fmax(q.x,0.0f), std::fmax(q.y,0.0f), std::fmax(q.z,0.0f)}) + std::fmin(std::fmax(q.x,std::fmax(q.y,q.z)), 0.0f);
}

static float sdCylinder(const Vec3& p, float r, float h) {
    float d = length(Vec3{p.x, 0, p.z}) - r;
    return std::fmax(d, std::abs(p.y) - h * 0.5f);
}

static float sdTorus(const Vec3& p, float r_major, float r_minor) {
    float qx = length(Vec3{p.x, 0, p.z}) - r_major;
    float qy = p.y;
    return std::sqrt(qx*qx + qy*qy) - r_minor;
}

static float sdPlane(const Vec3& p, float ny, float d) {
    return p.y + d; // default y-up
}

static float sdCone(const Vec3& p, float r, float h) {
    float q = length(Vec3{p.x, 0, p.z});
    float d = std::sqrt(2.0f) * std::fmax(
        (q >= r * (1.0f - p.y / h)) ? q - r * (1.0f - p.y / h) : h - p.y,
        -p.y
    );
    return d;
}

static float opUnion(float a, float b) { return std::fmin(a, b); }
static float opSubtract(float a, float b) { return std::fmax(a, -b); }
static float opIntersect(float a, float b) { return std::fmax(a, b); }
static float opSmoothUnion(float a, float b, float k) {
    float h = std::fmax(k - std::abs(a - b), 0.0f) / k;
    return std::fmin(a, b) - h * h * h * k * (1.0f / 6.0f);
}

// ============================================================================
// Full SDF tree evaluation
// ============================================================================

// Apply transform to point
static Vec3 applyTransform(const Vec3& p, const Vec3& translate, const Vec3& rotate, const Vec3& scale) {
    // Translate
    Vec3 tp = {p.x - translate.x, p.y - translate.y, p.z - translate.z};
    // Rotate (simple Euler, for prototype)
    float rx = rotate.x * 3.14159265f / 180.0f;
    float ry = rotate.y * 3.14159265f / 180.0f;
    float rz = rotate.z * 3.14159265f / 180.0f;
    // Z rot
    float cz = std::cos(rz), sz = std::sin(rz);
    Vec3 rp = {tp.x*cz - tp.y*sz, tp.x*sz + tp.y*cz, tp.z};
    // Y rot
    float cy = std::cos(ry), sy = std::sin(ry);
    rp = {rp.x*cy + rp.z*sy, rp.y, -rp.x*sy + rp.z*cy};
    // X rot
    float cx = std::cos(rx), sx = std::sin(rx);
    rp = {rp.x, rp.y*cx - rp.z*sx, rp.y*sx + rp.z*cx};
    // Scale
    return Vec3{rp.x / scale.x, rp.y / scale.y, rp.z / scale.z};
}

// Compute normal at point using scene-level evaluation
Vec3 calcNormal(const Rih& rih, const Vec3& p, float w, const f32* transforms) {
    const float eps = 0.001f;
    float d = evalScene(rih, p, w, transforms);
    float dx = evalScene(rih, Vec3{p.x + eps, p.y, p.z}, w, transforms);
    float dy = evalScene(rih, Vec3{p.x, p.y + eps, p.z}, w, transforms);
    float dz = evalScene(rih, Vec3{p.x, p.y, p.z + eps}, w, transforms);
    return normalize(Vec3{dx - d, dy - d, dz - d});
}

float evalSdfTree(const Rih& rih, u32 node_idx, const Vec3& p, float w, const f32* transforms) {
    if (node_idx >= rih.nodes.size()) return 1e9;
    const Node& node = rih.nodes[node_idx];

    // Apply pre-computed transform
    Vec3 local_p = p;
    if (transforms) {
        local_p.x -= transforms[node_idx * 3 + 0];
        local_p.y -= transforms[node_idx * 3 + 1];
        local_p.z -= transforms[node_idx * 3 + 2];
    } else {
        local_p = applyTransform(p, node.translate, node.rotate, node.scale);
    }

    if (node.type == NodeType::GROUP) {
        float d = 1e9;
        for (u32 child : node.children)
            d = std::fmin(d, evalSdfTree(rih, child, local_p, w, transforms));
        return d;
    }

    if (node.type == NodeType::INSTANCE) {
        if (node.def_id < rih.nodes.size())
            return evalSdfTree(rih, node.def_id, local_p, w, transforms);
        return 1e9;
    }

    // SDF node
    const SdfNode& sdf = node.sdf;
    float d = 1e9;

    // Evaluate params from expressions
    auto p0 = evalSdfExpr(sdf.params[0], local_p.x, local_p.y, local_p.z, w);
    auto p1 = evalSdfExpr(sdf.params[1], local_p.x, local_p.y, local_p.z, w);
    auto p2 = evalSdfExpr(sdf.params[2], local_p.x, local_p.y, local_p.z, w);
    auto p3 = evalSdfExpr(sdf.params[3], local_p.x, local_p.y, local_p.z, w);

    const std::string& t = sdf.sdf_type;

    if (t == "sphere")   d = sdSphere(local_p, p0);
    else if (t == "box") d = sdBox(local_p, {p0, p1, p2});
    else if (t == "cylinder") d = sdCylinder(local_p, p0, p1);
    else if (t == "torus") d = sdTorus(local_p, p0, p1);
    else if (t == "plane") d = sdPlane(local_p, 1.0f, p0);
    else if (t == "cone") d = sdCone(local_p, p0, p1);
    else if (t == "capsule") { float a = p1; /* simplified */ d = length(local_p) - a; }
    else if (t == "rounded_box") d = sdBox(local_p, {p0, p1, p2}) - p3;

    // Boolean operators
    else if (t == "union") {
        d = opUnion(evalSdfTree(rih, sdf.child_a, local_p, w, transforms),
                    evalSdfTree(rih, sdf.child_b, local_p, w, transforms));
    }
    else if (t == "subtract") {
        d = opSubtract(evalSdfTree(rih, sdf.child_a, local_p, w, transforms),
                       evalSdfTree(rih, sdf.child_b, local_p, w, transforms));
    }
    else if (t == "intersect") {
        d = opIntersect(evalSdfTree(rih, sdf.child_a, local_p, w, transforms),
                        evalSdfTree(rih, sdf.child_b, local_p, w, transforms));
    }
    else if (t == "smooth_union") {
        d = opSmoothUnion(evalSdfTree(rih, sdf.child_a, local_p, w, transforms),
                          evalSdfTree(rih, sdf.child_b, local_p, w, transforms), p0);
    }

    // Displace
    if (!sdf.displace_expr.empty()) {
        float disp = evalExpr(sdf.displace_expr, local_p.x, local_p.y, local_p.z, w);
        d += disp;
    }

    return d;
}

// Evaluate ALL nodes in the scene
float evalScene(const Rih& rih, const Vec3& p, float w, const f32* transforms) {
    float d = 1e9f;
    for (u32 i = 0; i < rih.nodes.size(); i++) {
        if (rih.nodes[i].is_compound_child) continue;
        float nd = evalSdfTree(rih, i, p, w, transforms);
        d = std::fmin(d, nd);
    }
    return d;
}

// Which node is closest at point p?
u32 findClosestNode(const Rih& rih, const Vec3& p, float w, const f32* transforms) {
    u32 best_idx = 0xFFFFFFFF;
    float best_d = 1e9f;
    for (u32 i = 0; i < rih.nodes.size(); i++) {
        if (rih.nodes[i].is_compound_child) continue;
        float d = evalSdfTree(rih, i, p, w, transforms);
        if (d < best_d) {
            best_d = d;
            best_idx = i;
        }
    }
    return best_idx;
}

// Find which material is at point p (the closest SDF)
u32 findMaterial(const Rih& rih, const Vec3& p, float w, const f32* transforms) {
    u32 closest = findClosestNode(rih, p, w, transforms);
    if (closest < rih.nodes.size())
        return rih.nodes[closest].material_id;
    return 0xFFFFFFFF;
}

} // namespace mg
