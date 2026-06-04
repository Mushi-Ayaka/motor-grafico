#include "rih_reader.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {

// Minimal JSON reader — enough for .rih format
struct JsonReader {
    const std::string& s;
    size_t pos = 0;

    JsonReader(const std::string& s) : s(s) {}

    char peek() { while (pos < s.size() && std::isspace(s[pos])) pos++; return pos < s.size() ? s[pos] : '\0'; }
    char advance() { return pos < s.size() ? s[pos++] : '\0'; }
    void expect(char c) {
        skipWs();
        if (advance() != c) throw std::runtime_error(std::string("expected '") + c + "' at pos " + std::to_string(pos));
    }
    void skipWs() { while (pos < s.size() && std::isspace(s[pos])) pos++; }

    std::string readString() {
        skipWs();
        if (advance() != '"') throw std::runtime_error("expected string at pos " + std::to_string(pos-1));
        std::string r;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\') { pos++; if (pos < s.size()) r += advance(); }
            else r += advance();
        }
        if (pos < s.size()) advance(); // closing "
        return r;
    }

    f32 readNumber() {
        skipWs();
        std::string val;
        if (peek() == '-') val += advance();
        while (pos < s.size() && std::isdigit(s[pos])) val += advance();
        if (peek() == '.') { val += advance(); while (pos < s.size() && std::isdigit(s[pos])) val += advance(); }
        return val.empty() ? 0.0f : std::stof(val);
    }

    bool readBool() {
        skipWs();
        if (s.substr(pos, 4) == "true") { pos += 4; return true; }
        if (s.substr(pos, 5) == "false") { pos += 5; return false; }
        throw std::runtime_error("expected bool at pos " + std::to_string(pos));
    }

    void skipValue() {
        skipWs();
        char c = peek();
        if (c == '"') readString();
        else if (c == '[') { advance(); while (peek() != ']') { skipValue(); if (peek() == ',') advance(); } advance(); }
        else if (c == '{') { advance(); while (peek() != '}') { skipValue(); if (peek() == ',') advance(); } advance(); }
        else if (c == 't' || c == 'f') readBool();
        else readNumber();
    }

    // Read Vec3 array [x, y, z]
    Vec3 readVec3() {
        skipWs();
        expect('[');
        f32 x = readNumber(); skipWs(); expect(','); skipWs();
        f32 y = readNumber(); skipWs(); expect(','); skipWs();
        f32 z = readNumber(); skipWs();
        expect(']');
        return {x, y, z};
    }

    // Read Expr
    Expr readExpr() {
        skipWs();
        Expr e;
        if (peek() == '{') {
            advance(); // {
            while (peek() != '}') {
                skipWs();
                std::string key = readString();
                skipWs(); expect(':');
                if (key == "expr") {
                    e.is_expr = true;
                    e.expression = readString();
                } else {
                    skipValue();
                }
                if (peek() == ',') advance();
            }
            advance(); // }
        } else {
            e.is_expr = false;
            e.constant = readNumber();
        }
        return e;
    }
};

static bool loadRihFromString(const std::string& json, Rih& out) {
    JsonReader jr(json);
    jr.skipWs();
    jr.expect('{');

    while (jr.peek() != '}') {
        std::string key = jr.readString();
        jr.skipWs(); jr.expect(':');
        jr.skipWs();

        if (key == "version") {
            out.version = (u32)jr.readNumber();
        }
        else if (key == "scene") {
            jr.expect('{');
            while (jr.peek() != '}') {
                std::string sk = jr.readString();
                jr.skipWs(); jr.expect(':');
                jr.skipWs();

                if (sk == "axes") {
                    jr.expect('{');
                    while (jr.peek() != '}') {
                        std::string ak = jr.readString();
                        jr.skipWs(); jr.expect(':');
                        if (ak == "X") out.width = (u32)jr.readNumber();
                        else if (ak == "Y") out.height = (u32)jr.readNumber();
                        else if (ak == "W") out.frames = (u32)jr.readNumber();
                        else jr.skipValue();
                        if (jr.peek() == ',') jr.advance();
                    }
                    jr.advance();
                }
                else if (sk == "camera") {
                    jr.expect('{');
                    while (jr.peek() != '}') {
                        std::string ck = jr.readString();
                        jr.skipWs(); jr.expect(':');
                        if (ck == "position") out.camera.position = jr.readVec3();
                        else if (ck == "target") out.camera.target = jr.readVec3();
                        else if (ck == "up") out.camera.up = jr.readVec3();
                        else if (ck == "fov") out.camera.fov = jr.readNumber();
                        else jr.skipValue();
                        if (jr.peek() == ',') jr.advance();
                    }
                    jr.advance();
                }
                else if (sk == "background") {
                    auto v = jr.readVec3();
                    out.background = {v.x, v.y, v.z};
                }
                else jr.skipValue();

                if (jr.peek() == ',') jr.advance();
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
                    jr.skipWs(); jr.expect(':');
                    if (mk == "id") m.id = (u32)jr.readNumber();
                    else if (mk == "name") m.name = jr.readString();
                    else if (mk == "base_color") { auto v = jr.readVec3(); m.base_color = {v.x,v.y,v.z}; }
                    else if (mk == "roughness") m.roughness = jr.readNumber();
                    else if (mk == "metallic") m.metallic = jr.readNumber();
                    else if (mk == "emission") { auto v = jr.readVec3(); m.emission = {v.x,v.y,v.z}; }
                    else if (mk == "ior") m.ior = jr.readNumber();
                    else if (mk == "opacity") m.opacity = jr.readNumber();
                    else jr.skipValue();
                    if (jr.peek() == ',') jr.advance();
                }
                jr.advance();
                out.materials.push_back(m);
                if (jr.peek() == ',') jr.advance();
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
                    jr.skipWs(); jr.expect(':');

                    if (nk == "id") n.id = (u32)jr.readNumber();
                    else if (nk == "name") n.name = jr.readString();
                    else if (nk == "type") {
                        std::string t = jr.readString();
                        if (t == "sdf") n.type = NodeType::SDF;
                        else if (t == "group") n.type = NodeType::GROUP;
                        else if (t == "instance") n.type = NodeType::INSTANCE;
                    }
                    else if (nk == "mode") {
                        std::string m = jr.readString();
                        n.mode = (m == "volume") ? NodeMode::VOLUME : NodeMode::SOLID;
                    }
                    else if (nk == "material") n.material_id = (u32)jr.readNumber();
                    else if (nk == "density_scale") n.density_scale = jr.readNumber();
                    else if (nk == "blend_mode") n.blend_mode = (u32)jr.readNumber();

                    else if (nk == "transform") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string tk = jr.readString();
                            jr.skipWs(); jr.expect(':');
                            if (tk == "translate") n.translate = jr.readVec3();
                            else if (tk == "rotate") n.rotate = jr.readVec3();
                            else if (tk == "scale") n.scale = jr.readVec3();
                            else jr.skipValue();
                            if (jr.peek() == ',') jr.advance();
                        }
                        jr.advance();
                    }

                    else if (nk == "sdf") {
                        if (jr.peek() == '[') {
                            // Array-form compound SDF: expand into child nodes
                            jr.advance(); // [
                            std::vector<Node> temp_nodes;
                            std::vector<u32> child_ids;
                            while (jr.peek() != ']') {
                                temp_nodes.emplace_back();
                                Node& sn = temp_nodes.back();
                                sn.type = NodeType::SDF;
                                jr.expect('{');
                                while (jr.peek() != '}') {
                                    std::string sk = jr.readString();
                                    jr.skipWs(); jr.expect(':');
                                    if (sk == "type") sn.sdf.sdf_type = jr.readString();
                                    else if (sk == "params") {
                                        jr.expect('[');
                                        for (int i = 0; i < 4; i++) {
                                            if (jr.peek() == ',') jr.advance();
                                            sn.sdf.params[i] = jr.readExpr();
                                        }
                                        jr.expect(']');
                                    }
                                    else if (sk == "child_a" || sk == "child_b") {
                                        // Remember indices, remap later
                                        u32 cid = (u32)jr.readNumber();
                                        if (sk == "child_a") sn.sdf.child_a = cid;
                                        else sn.sdf.child_b = cid;
                                    }
                                    else if (sk == "k") {
                                        sn.sdf.params[0] = jr.readExpr(); // k -> params[0]
                                    }
                                    else jr.skipValue();
                                    if (jr.peek() == ',') jr.advance();
                                }
                                jr.advance();
                                if (jr.peek() == ',') jr.advance();
                            }
                            jr.advance(); // ]
                            // Assign real IDs and add to parent
                            u32 base = (u32)out.nodes.size();
                            for (u32 i = 0; i < (u32)temp_nodes.size(); i++) {
                                u32 real_id = base + i;
                                temp_nodes[i].id = real_id;
                                temp_nodes[i].is_compound_child = true; // skip in top-level eval
                                // Propagate material/density/blend from parent
                                temp_nodes[i].material_id = n.material_id;
                                temp_nodes[i].density_scale = n.density_scale;
                                temp_nodes[i].blend_mode = n.blend_mode;
                                // Remap child_a/child_b from array index to real id
                                if (temp_nodes[i].sdf.child_a < temp_nodes.size())
                                    temp_nodes[i].sdf.child_a = base + temp_nodes[i].sdf.child_a;
                                if (temp_nodes[i].sdf.child_b < temp_nodes.size())
                                    temp_nodes[i].sdf.child_b = base + temp_nodes[i].sdf.child_b;
                                child_ids.push_back(real_id);
                                out.nodes.push_back(temp_nodes[i]);
                            }
                            // Make parent a GROUP with these children
                            n.type = NodeType::GROUP;
                            n.children = child_ids;
                            n.sdf = SdfNode(); // clear
                        } else {
                            // Single SDF object
                            jr.expect('{');
                            while (jr.peek() != '}') {
                                std::string sk = jr.readString();
                                jr.skipWs(); jr.expect(':');
                                if (sk == "type") n.sdf.sdf_type = jr.readString();
                                else if (sk == "params") {
                                    jr.expect('[');
                                    for (int i = 0; i < 4; i++) {
                                        if (jr.peek() == ',') jr.advance();
                                        if (i < 4) n.sdf.params[i] = jr.readExpr();
                                    }
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
                                        if (jr.peek() == ',') jr.advance();
                                    }
                                    jr.advance();
                                }
                                else if (sk == "k") {
                                    n.sdf.params[0] = jr.readExpr(); // k -> params[0]
                                }
                                else if (sk == "displace_expr") n.sdf.displace_expr = jr.readString();
                                else jr.skipValue();
                                if (jr.peek() == ',') jr.advance();
                            }
                            jr.advance();
                        }
                    }

                    else if (nk == "children") {
                        jr.expect('[');
                        while (jr.peek() != ']') {
                            n.children.push_back((u32)jr.readNumber());
                            if (jr.peek() == ',') jr.advance();
                        }
                        jr.advance();
                    }

                    else if (nk == "def") n.def_id = (u32)jr.readNumber();
                    else jr.skipValue();

                    if (jr.peek() == ',') jr.advance();
                }
                jr.advance();
                out.nodes.push_back(n);
                if (jr.peek() == ',') jr.advance();
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
                    jr.skipWs(); jr.expect(':');
                    if (lk == "name") l.name = jr.readString();
                    else if (lk == "type") l.type = (jr.readString() == "point") ? LightType::POINT : LightType::DIRECTIONAL;
                    else if (lk == "direction") l.direction = jr.readVec3();
                    else if (lk == "position") l.position = jr.readVec3();
                    else if (lk == "color") { auto v = jr.readVec3(); l.color = {v.x,v.y,v.z}; }
                    else if (lk == "intensity") l.intensity = jr.readNumber();
                    else if (lk == "falloff") l.falloff = jr.readNumber();
                    else jr.skipValue();
                    if (jr.peek() == ',') jr.advance();
                }
                jr.advance();
                out.lights.push_back(l);
                if (jr.peek() == ',') jr.advance();
            }
            jr.advance();
        }
        else jr.skipValue();

        if (jr.peek() == ',') jr.advance();
    }
    jr.advance();

    // Default light if none
    if (out.lights.empty()) {
        Light def;
        def.name = "default";
        out.lights.push_back(def);
    }

    return true;
}

bool loadRih(const std::string& path, Rih& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    return loadRihFromString(ss.str(), out);
}

bool loadRihW(const wchar_t* path, Rih& out) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD size = GetFileSize(h, nullptr);
    std::string content;
    content.resize(size);
    DWORD read = 0;
    ReadFile(h, &content[0], size, &read, nullptr);
    CloseHandle(h);
    return loadRihFromString(content, out);
}

} // namespace mg
