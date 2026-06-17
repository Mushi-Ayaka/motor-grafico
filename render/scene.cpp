#include "scene.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <stdexcept>

namespace mg {

// ============================================================================
// OntScene loading (.ont binary)
// ============================================================================

bool OntScene::loadOnt(const FileMapping& fm) {
    if (fm.size() < (int)sizeof(OntHeader)) return false;

    const uint8_t* data = (const uint8_t*)fm.data();
    const OntHeader* hdr = (const OntHeader*)data;

    if (hdr->magic != ONT_MAGIC) return false;
    if (hdr->version < 1) return false;

    // Copy the entire file into owned memory
    _data.resize(fm.size());
    memcpy(_data.data(), data, fm.size());

    // Set pointers
    header = (const OntHeader*)_data.data();
    bvh_nodes = (const OntBvhNode*)(_data.data() + sizeof(OntHeader));
    graph_nodes = (const OntGraphNode*)(bvh_nodes + header->bvh_count);
    bytecode = (const uint8_t*)(graph_nodes + header->node_count);
    materials = (const OntMaterial*)(bytecode + header->bytecode_size);

    // Scene defaults (camera / resolution from scene-level info not in .ont v1)
    width = 800; height = 600;
    pipeline.trace_hit_threshold = hdr->epsilon;

    return true;
}

bool OntScene::loadObs(const FileMapping& fm) {
    if (fm.size() < (int)sizeof(ObsHeader)) return false;
    const uint8_t* data = (const uint8_t*)fm.data();
    const ObsHeader* hdr = (const ObsHeader*)data;
    if (hdr->magic != OBS_MAGIC || hdr->version < 1) return false;

    // Copy into owned memory
    _obs_data.resize(fm.size());
    memcpy(_obs_data.data(), data, fm.size());
    const ObsHeader* own_hdr = (const ObsHeader*)_obs_data.data();

    OntObservation o;

    if ((own_hdr->flags & OBS_CAMERA) && own_hdr->camera_offset > 0) {
        if (own_hdr->camera_offset + sizeof(ObsCamera) <= _obs_data.size()) {
            const ObsCamera* oc = (const ObsCamera*)(_obs_data.data() + own_hdr->camera_offset);
            o.camera.position = { oc->position[0], oc->position[1], oc->position[2] };
            o.camera.target   = { oc->target[0],   oc->target[1],   oc->target[2] };
            o.camera.up       = { oc->up[0],        oc->up[1],        oc->up[2] };
            o.camera.fov      = oc->fov;
            o.has_camera = true;
        }
    }

    if ((own_hdr->flags & OBS_LIGHTS) && own_hdr->lights_offset > 0) {
        if (own_hdr->lights_offset + sizeof(ObsLightsHeader) <= _obs_data.size()) {
            const ObsLightsHeader* lhdr = (const ObsLightsHeader*)(_obs_data.data() + own_hdr->lights_offset);
            const ObsLight* olights = (const ObsLight*)(lhdr + 1);
            for (uint32_t i = 0; i < lhdr->count; i++) {
                if ((const uint8_t*)(olights + i + 1) > _obs_data.data() + _obs_data.size()) break;
                Light l;
                l.type = (olights[i].type == 1) ? LightType::POINT : LightType::DIRECTIONAL;
                l.direction = { olights[i].direction[0], olights[i].direction[1], olights[i].direction[2] };
                l.position  = { olights[i].position[0],  olights[i].position[1],  olights[i].position[2] };
                l.color     = { olights[i].color[0],     olights[i].color[1],     olights[i].color[2] };
                l.intensity = olights[i].intensity;
                l.falloff   = olights[i].falloff;
                o.lights.push_back(l);
            }
            o.has_lights = !o.lights.empty();
        }
    }

    if ((own_hdr->flags & OBS_TIMELINE) && own_hdr->timeline_offset > 0) {
        if (own_hdr->timeline_offset + sizeof(ObsTimeline) <= _obs_data.size()) {
            const ObsTimeline* tl = (const ObsTimeline*)(_obs_data.data() + own_hdr->timeline_offset);
            o.w_frames = tl->w_frames;
            o.w_min = tl->w_min;
            o.w_max = tl->w_max;
            o.has_timeline = true;
        }
    }

    if ((own_hdr->flags & OBS_BACKGROUND) && own_hdr->background_offset > 0) {
        if (own_hdr->background_offset + 12 <= _obs_data.size()) {
            const float* bg = (const float*)(_obs_data.data() + own_hdr->background_offset);
            o.background = { bg[0], bg[1], bg[2] };
            o.has_background = true;
        }
    }

    if ((own_hdr->flags & OBS_RESOLUTION) && own_hdr->resolution_offset > 0) {
        if (own_hdr->resolution_offset + 8 <= _obs_data.size()) {
            const uint32_t* res = (const uint32_t*)(_obs_data.data() + own_hdr->resolution_offset);
            o.width = res[0];
            o.height = res[1];
            o.has_resolution = true;
        }
    }

    obs = o;
    has_obs = true;
    return true;
}

void OntScene::applyObs() {
    if (!has_obs) return;
    if (obs.has_camera) {
        camera = obs.camera;
    }
    if (obs.has_background) {
        background = obs.background;
    }
    if (obs.has_resolution) {
        width = obs.width;
        height = obs.height;
    }
    // Lights are accessed via obs.lights by the renderer
}

void OntScene::loadDefault() {
    // Create a simple sphere scene in .ont format
    struct {
        OntHeader hdr;
        OntBvhNode bvh;
        OntGraphNode gn;
        uint8_t bc[15]; // CONST 1.0, VAR_X, VAR_X, MUL, VAR_Y, VAR_Y, MUL, ADD, VAR_Z, VAR_Z, MUL, ADD, SQRT, CONST -1.0, ADD, END
        OntMaterial mat;
    } ont;

    memset(&ont, 0, sizeof(ont));
    ont.hdr.magic = ONT_MAGIC;
    ont.hdr.version = 1;
    ont.hdr.epsilon = 0.001f;
    ont.hdr.node_count = 1;
    ont.hdr.bvh_count = 1;
    ont.hdr.material_count = 1;
    ont.hdr.bytecode_size = 15;
    ont.hdr.scene_aabb_min[0] = ont.hdr.scene_aabb_min[1] = ont.hdr.scene_aabb_min[2] = -2;
    ont.hdr.scene_aabb_max[0] = ont.hdr.scene_aabb_max[1] = ont.hdr.scene_aabb_max[2] = 2;

    // BVH root (leaf containing all nodes)
    for (int i = 0; i < 3; i++) {
        ont.bvh.min[i] = -2;
        ont.bvh.max[i] = 2;
    }
    ont.bvh.d_min = 0.01f;
    ont.bvh.L = 1.0f;
    ont.bvh.skip_index = 1;
    ont.bvh.first_node = 0;
    ont.bvh.node_count = 1;
    ont.bvh.flags = BVH_FLAG_LEAF;

    // Graph node: sphere sqrt(x*x+y*y+z*z)-1.0
    ont.gn.local_transform[0] = 1;
    ont.gn.local_transform[5] = 1;
    ont.gn.local_transform[10] = 1;
    ont.gn.local_transform[15] = 1;
    ont.gn.material_id = 0;
    ont.gn.bytecode_offset = 0;
    ont.gn.bytecode_length = 15;
    ont.gn.bbox_min[0] = ont.gn.bbox_min[1] = ont.gn.bbox_min[2] = -1;
    ont.gn.bbox_max[0] = ont.gn.bbox_max[1] = ont.gn.bbox_max[2] = 1;

    // Bytecode: sqrt(x*x + y*y + z*z) - 1.0
    // CONST 1.0, VAR_X, VAR_X, MUL, VAR_Y, VAR_Y, MUL, ADD, VAR_Z, VAR_Z, MUL, ADD, SQRT, CONST -1.0, ADD, END = 16 instructions
    // Actually: PUSH_CONST 1.0, MUL x x, MUL y y, ADD, MUL z z, ADD, SQRT, SUB 1.0... simpler:
    // Just do: sqrt(x*x + y*y + z*z) - 1.0 as bytecode
    uint8_t bc[] = {
        ONT_CONST, 0,0,0,0,  // placeholder 1.0
        ONT_VAR_X, ONT_VAR_X, ONT_MUL,
        ONT_VAR_Y, ONT_VAR_Y, ONT_MUL, ONT_ADD,
        ONT_VAR_Z, ONT_VAR_Z, ONT_MUL, ONT_ADD,
        ONT_SQRT,
        ONT_CONST, 0,0,0,0,  // placeholder -1.0
        ONT_ADD,
        ONT_END
    };
    uint8_t raw_bc[] = {
        ONT_CONST, 0,0,0,0,  // 5: 1.0
        ONT_VAR_X,            // 6
        ONT_VAR_X,            // 7
        ONT_MUL,              // 8
        ONT_VAR_Y,            // 9
        ONT_VAR_Y,            // 10
        ONT_MUL,              // 11
        ONT_ADD,              // 12
        ONT_VAR_Z,            // 13
        ONT_VAR_Z,            // 14
        ONT_MUL,              // 15
        ONT_ADD,              // 16
        ONT_SQRT,             // 17
        ONT_CONST, 0,0,0,0,  // 22: -1.0
        ONT_ADD,              // 23
        ONT_END               // 24
    };
    { float v = 1.0f; memcpy(&raw_bc[1], &v, 4); }
    { float v = -1.0f; memcpy(&raw_bc[18], &v, 4); }

    ont.hdr.bytecode_size = sizeof(raw_bc);
    memcpy(ont.bc, raw_bc, sizeof(raw_bc));

    // Material: red-ish
    ont.mat.id = 0;
    ont.mat.base_color[0] = 0.8f;
    ont.mat.base_color[1] = 0.2f;
    ont.mat.base_color[2] = 0.2f;
    ont.mat.base_color[3] = 1.0f;
    ont.mat.roughness = 0.3f;
    ont.mat.metallic = 0.0f;
    ont.mat.opacity = 1.0f;

    _data.resize(sizeof(ont));
    memcpy(_data.data(), &ont, sizeof(ont));
    header = (const OntHeader*)_data.data();
    bvh_nodes = (const OntBvhNode*)(_data.data() + sizeof(OntHeader));
    graph_nodes = (const OntGraphNode*)(bvh_nodes + header->bvh_count);
    bytecode = (const uint8_t*)(graph_nodes + header->node_count);
    materials = (const OntMaterial*)(bytecode + header->bytecode_size);
}

// ============================================================================
// Legacy Scene loading (JSON .rih)
// ============================================================================

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
                else if (key == "bc") {
                    expect('[');
                    while (peek() != ']') {
                        e.bytecode.push_back(readNumber());
                        match(',');
                    }
                    advance();
                }
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
                        else if (mk == "tensor") {
                            jr.expect('[');
                            f32 tensor[8] = {};
                            tensor[0] = jr.readNumber();
                            for (int ti = 1; ti < 8; ti++) { jr.match(','); tensor[ti] = jr.readNumber(); }
                            jr.expect(']');
                            m.base_color = {tensor[4], tensor[5], tensor[6]};
                            m.opacity = tensor[7];
                        }
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
                        else if (nk == "material_r") n.material_expr[0] = jr.readString();
                        else if (nk == "material_g") n.material_expr[1] = jr.readString();
                        else if (nk == "material_b") n.material_expr[2] = jr.readString();
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
            else if (key == "pipeline") {
                jr.expect('{');
                while (jr.peek() != '}') {
                    std::string pk = jr.readString();
                    jr.match(':');
                    if (pk == "trace") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string tk = jr.readString();
                            jr.match(':');
                            if (tk == "max_steps") pipeline.trace_max_steps = (int)jr.readNumber();
                            else if (tk == "hit_threshold") pipeline.trace_hit_threshold = jr.readNumber();
                            else if (tk == "t_min") pipeline.trace_t_min = jr.readNumber();
                            else if (tk == "t_max") pipeline.trace_t_max = jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else if (pk == "shade") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string sk = jr.readString();
                            jr.match(':');
                            if (sk == "ambient") pipeline.shade_ambient = jr.readNumber();
                            else if (sk == "diffuse") pipeline.shade_diffuse = jr.readNumber();
                            else if (sk == "specular") pipeline.shade_specular = jr.readNumber();
                            else if (sk == "spec_power") pipeline.shade_spec_power = jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else if (pk == "shadow") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string sk = jr.readString();
                            jr.match(':');
                            if (sk == "enabled") pipeline.shadow_enabled = jr.readBool();
                            else if (sk == "steps") pipeline.shadow_steps = (int)jr.readNumber();
                            else if (sk == "max_dist") pipeline.shadow_max_dist = jr.readNumber();
                            else if (sk == "bias") pipeline.shadow_bias = jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else if (pk == "post") {
                        jr.expect('{');
                        while (jr.peek() != '}') {
                            std::string pok = jr.readString();
                            jr.match(':');
                            if (pok == "tonemap") pipeline.post_tonemap = jr.readBool();
                            else if (pok == "gamma") pipeline.post_gamma = jr.readBool();
                            else if (pok == "exposure") pipeline.post_exposure = jr.readNumber();
                            else jr.skipValue();
                            jr.match(',');
                        }
                        jr.advance();
                    }
                    else jr.skipValue();
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
