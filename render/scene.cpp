#include "scene.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <stdexcept>

namespace mg {

// ============================================================================
// Minimal JSON reader
// ============================================================================
struct JsonReader {
    const char* s;
    size_t pos;
    size_t len;

    JsonReader(const char* s, size_t len) : s(s), pos(0), len(len) {}

    char peek() { skipWs(); return pos < len ? s[pos] : '\0'; }
    char advance() { return pos < len ? s[pos++] : '\0'; }
    void skipWs() { while (pos < len && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) pos++; }
    void expect(char c) {
        skipWs();
        if (pos >= len || s[pos] != c) throw std::runtime_error("expected char");
        pos++;
    }
    bool match(char c) {
        skipWs();
        if (pos < len && s[pos] == c) { pos++; return true; }
        return false;
    }

    std::string readString() {
        skipWs();
        if (pos >= len || s[pos] != '"') throw std::runtime_error("expected string");
        pos++;
        std::string r;
        while (pos < len && s[pos] != '"') {
            if (s[pos] == '\\') { pos++; if (pos < len) r += advance(); }
            else r += advance();
        }
        if (pos < len) pos++;
        return r;
    }

    f32 readNumber() {
        skipWs();
        std::string val;
        if (pos < len && s[pos] == '-') val += advance();
        while (pos < len && std::isdigit(s[pos])) val += advance();
        if (pos < len && s[pos] == '.') { val += advance(); while (pos < len && std::isdigit(s[pos])) val += advance(); }
        if (pos < len && (s[pos] == 'e' || s[pos] == 'E')) { val += advance(); if (pos < len && (s[pos] == '+' || s[pos] == '-')) val += advance(); while (pos < len && std::isdigit(s[pos])) val += advance(); }
        return val.empty() ? 0.0f : (f32)std::atof(val.c_str());
    }

    bool readBool() {
        skipWs();
        if (pos + 3 < len && s[pos] == 't' && s[pos+1] == 'r' && s[pos+2] == 'u' && s[pos+3] == 'e') { pos += 4; return true; }
        if (pos + 4 < len && s[pos] == 'f' && s[pos+1] == 'a' && s[pos+2] == 'l' && s[pos+3] == 's' && s[pos+4] == 'e') { pos += 5; return false; }
        throw std::runtime_error("expected bool");
    }

    void skipValue() {
        skipWs();
        if (pos >= len) return;
        if (s[pos] == '"') readString();
    else if (s[pos] == '[') { advance(); while (peek() != ']') { skipValue(); match(','); } advance(); }
    else if (s[pos] == '{') { advance(); while (peek() != '}') { skipValue(); match(':'); skipValue(); match(','); } advance(); }
        else if (s[pos] == 't' || s[pos] == 'f') readBool();
        else readNumber();
    }

    Vec3 readVec3() {
        expect('[');
        f32 x = readNumber(); match(','); f32 y = readNumber(); match(','); f32 z = readNumber();
        expect(']');
        return {x, y, z};
    }

    Expr readExpr() {
        skipWs();
        Expr e;
        if (s[pos] == '{') {
            advance();
            while (peek() != '}') {
                std::string key = readString(); match(':');
                if (key == "expr") { e.is_expr = true; e.expression = readString(); }
                else skipValue();
                match(',');
            }
            advance();
        } else {
            e.is_expr = false;
            e.constant = readNumber();
        }
        return e;
    }
};

// ============================================================================
// Scene loading
// ============================================================================

bool Scene::load(const FileMapping& fm) {
    const char* data = (const char*)fm.data();
    size_t size = fm.size();
    if (!data || size == 0) return false;

    try {
        JsonReader jr(data, size);
        jr.expect('{');

        while (jr.peek() != '}') {
            std::string key = jr.readString();
            jr.match(':');

            if (key == "version") {
                version = (u32)jr.readNumber();
            }
            else if (key == "scene") {
                jr.expect('{');
                while (jr.peek() != '}') {
                    std::string sk = jr.readString();
                    jr.match(':');
                    if (sk == "axes") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string ak = jr.readString();
                            jr.match(':');
                            if (ak == "X") width = (u32)jr.readNumber();
                            else if (ak == "Y") height = (u32)jr.readNumber();
                            else if (ak == "W") frames = (u32)jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else if (sk == "camera") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string ck = jr.readString();
                            jr.match(':');
                            if (ck == "position") camera.position = jr.readVec3();
                            else if (ck == "target") camera.target = jr.readVec3();
                            else if (ck == "up") camera.up = jr.readVec3();
                            else if (ck == "fov") camera.fov = jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else if (sk == "background") {
                        auto v = jr.readVec3();
                        background = {v.x, v.y, v.z};
                    }
                    else jr.skipValue();
                    jr.match(',');
                }
                jr.advance();
            }
            else if (key == "materials") {
                jr.expect('[');
                while (jr.peek() != ']') {
                    jr.expect('{');
                    Material m;
                    while (jr.peek() != '}') {
                        std::string mk = jr.readString();
                        jr.match(':');
                        if (mk == "id") m.id = (u32)jr.readNumber();
                        else if (mk == "name") m.name = jr.readString();
                        else if (mk == "base_color") { auto v = jr.readVec3(); m.base_color = {v.x,v.y,v.z}; }
                        else if (mk == "roughness") m.roughness = jr.readNumber();
                        else if (mk == "metallic") m.metallic = jr.readNumber();
                        else if (mk == "emission") { auto v = jr.readVec3(); m.emission = {v.x,v.y,v.z}; }
                        else if (mk == "ior") m.ior = jr.readNumber();
                        else if (mk == "opacity") m.opacity = jr.readNumber();
                        else jr.skipValue();
                        jr.match(',');
                    }
                    jr.advance();
                    materials.push_back(m);
                    jr.match(',');
                }
                jr.advance();
            }
            else if (key == "nodes") {
                jr.expect('[');
                while (jr.peek() != ']') {
                    jr.expect('{');
                    Node n;
                    while (jr.peek() != '}') {
                        std::string nk = jr.readString();
                        jr.match(':');
                        if (nk == "id") n.id = (u32)jr.readNumber();
                        else if (nk == "name") n.name = jr.readString();
                        else if (nk == "type") {
                            std::string t = jr.readString();
                            n.type = (t == "group") ? NodeType::GROUP :
                                     (t == "instance") ? NodeType::INSTANCE : NodeType::SDF;
                        }
                        else if (nk == "mode") n.mode = (jr.readString() == "volume") ? NodeMode::VOLUME : NodeMode::SOLID;
                        else if (nk == "material") n.material_id = (u32)jr.readNumber();
                        else if (nk == "density_scale") n.density_scale = jr.readNumber();
                        else if (nk == "blend_mode") n.blend_mode = (u32)jr.readNumber();
                        else if (nk == "transform") {
                            jr.expect('{');
                            while (jr.peek() != '}') {
                                std::string tk = jr.readString();
                                jr.match(':');
                                if (tk == "translate") n.translate = jr.readVec3();
                                else if (tk == "rotate") n.rotate = jr.readVec3();
                                else if (tk == "scale") n.scale = jr.readVec3();
                                else jr.skipValue();
                                jr.match(',');
                            }
                            jr.advance();
                        }
                        else if (nk == "sdf") {
                            if (jr.peek() == '[') {
                                // Array-form compound
                                jr.advance();
                                std::vector<Node> temp;
                                std::vector<u32> child_ids;
                                while (jr.peek() != ']') {
                                    temp.emplace_back();
                                    Node& sn = temp.back();
                                    sn.type = NodeType::SDF;
                                    jr.expect('{');
                                    while (jr.peek() != '}') {
                                        std::string sk = jr.readString();
                                        jr.match(':');
                                        if (sk == "type") sn.sdf.sdf_type = jr.readString();
                                        else if (sk == "params") {
                                            jr.expect('[');
                                            for (int i = 0; i < 4; i++) { jr.match(','); sn.sdf.params[i] = jr.readExpr(); }
                                            jr.expect(']');
                                        }
                                        else if (sk == "child_a" || sk == "child_b") {
                                            u32 cid = (u32)jr.readNumber();
                                            if (sk == "child_a") sn.sdf.child_a = cid;
                                            else sn.sdf.child_b = cid;
                                        }
                                        else if (sk == "k") { sn.sdf.params[0] = jr.readExpr(); }
                                        else jr.skipValue();
                                        jr.match(',');
                                    }
                                    jr.advance();
                                    jr.match(',');
                                }
                                jr.advance();
                                u32 base = (u32)nodes.size();
                                for (u32 i = 0; i < (u32)temp.size(); i++) {
                                    u32 real_id = base + i;
                                    temp[i].id = real_id;
                                    temp[i].is_compound_child = true;
                                    temp[i].material_id = n.material_id;
                                    temp[i].density_scale = n.density_scale;
                                    temp[i].blend_mode = n.blend_mode;
                                    if (temp[i].sdf.child_a < (u32)temp.size())
                                        temp[i].sdf.child_a = base + temp[i].sdf.child_a;
                                    if (temp[i].sdf.child_b < (u32)temp.size())
                                        temp[i].sdf.child_b = base + temp[i].sdf.child_b;
                                    child_ids.push_back(real_id);
                                    nodes.push_back(temp[i]);
                                    _id_to_idx[real_id] = (u32)nodes.size() - 1;
                                }
                                n.type = NodeType::GROUP;
                                n.children = child_ids;
                                n.sdf = SdfNode();
                            } else {
                                jr.expect('{');
                                while (jr.peek() != '}') {
                                    std::string sk = jr.readString();
                                    jr.match(':');
                                    if (sk == "type") n.sdf.sdf_type = jr.readString();
                                    else if (sk == "params") {
                                        jr.expect('[');
                                        for (int i = 0; i < 4; i++) { jr.match(','); n.sdf.params[i] = jr.readExpr(); }
                                        jr.expect(']');
                                    }
                                    else if (sk == "children") {
                                        jr.expect('[');
                                        int ci = 0;
                                        while (jr.peek() != ']') {
                                            u32 cid = (u32)jr.readNumber();
                                            if (ci == 0) n.sdf.child_a = cid;
                                            else if (ci == 1) n.sdf.child_b = cid;
                                            ci++;
                                            jr.match(',');
                                        }
                                        jr.advance();
                                    }
                                    else if (sk == "k") n.sdf.params[0] = jr.readExpr();
                                    else if (sk == "displace_expr") n.sdf.displace_expr = jr.readString();
                                    else jr.skipValue();
                                    jr.match(',');
                                }
                                jr.advance();
                            }
                        }
                        else if (nk == "children") {
                            jr.expect('[');
                            while (jr.peek() != ']') {
                                u32 cid = (u32)jr.readNumber();
                                n.children.push_back(idToIndex(cid));
                                jr.match(',');
                            }
                            jr.advance();
                        }
                        else if (nk == "def") {
                            u32 def_id_val = (u32)jr.readNumber();
                            n.def_id = idToIndex(def_id_val);
                        }
                        else jr.skipValue();
                        jr.match(',');
                    }
                    jr.advance();
                    nodes.push_back(n);
                    _id_to_idx[n.id] = (u32)nodes.size() - 1;
                    jr.match(',');
                }
                jr.advance();
            }
            else if (key == "lights") {
                jr.expect('[');
                while (jr.peek() != ']') {
                    jr.expect('{');
                    Light l;
                    while (jr.peek() != '}') {
                        std::string lk = jr.readString();
                        jr.match(':');
                        if (lk == "name") l.name = jr.readString();
                        else if (lk == "type") l.type = (jr.readString() == "point") ? LightType::POINT : LightType::DIRECTIONAL;
                        else if (lk == "direction") l.direction = jr.readVec3();
                        else if (lk == "position") l.position = jr.readVec3();
                        else if (lk == "color") { auto v = jr.readVec3(); l.color = {v.x,v.y,v.z}; }
                        else if (lk == "intensity") l.intensity = jr.readNumber();
                        else if (lk == "falloff") l.falloff = jr.readNumber();
                        else jr.skipValue();
                        jr.match(',');
                    }
                    jr.advance();
                    lights.push_back(l);
                    jr.match(',');
                }
                jr.advance();
            }
            else jr.skipValue();
            jr.match(',');
        }
        jr.advance();

    // Default light if none
    if (lights.empty()) {
        Light def;
        def.name = "default";
        lights.push_back(def);
    }
    return true;
    } catch (const std::exception& e) {
        (void)e;
        return false;
    }
}

u32 Scene::idToIndex(u32 id) const {
    auto it = _id_to_idx.find(id);
    return (it != _id_to_idx.end()) ? it->second : 0xFFFFFFFF;
}

void Scene::loadDefault() {
    width = 400; height = 300;
    Material mat;
    mat.id = 0;
    mat.name = "default";
    mat.base_color = {1, 0.5f, 0.2f};
    mat.roughness = 0.3f;
    mat.metallic = 1.0f;
    materials.push_back(mat);
    Node node;
    node.id = 0;
    node.name = "sphere";
    node.type = NodeType::SDF;
    node.material_id = 0;
    node.sdf.sdf_type = "sphere";
    node.sdf.params[0].is_expr = false;
    node.sdf.params[0].constant = 1.0f;
    nodes.push_back(node);
    Light def;
    def.name = "default";
    def.type = LightType::DIRECTIONAL;
    def.direction = {0.3f, 0.8f, 0.5f};
    lights.push_back(def);
}

int Scene::stats(char* buf, int size) const {
    return snprintf(buf, size,
        "nodes=%zu materials=%zu lights=%zu resolution=%ux%u",
        nodes.size(), materials.size(), lights.size(), width, height);
}

} // namespace mg
