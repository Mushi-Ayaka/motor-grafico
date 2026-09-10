#pragma once
#include "../os/os.h"
#include "../render/scene.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {
namespace scene {

// ============================================================================
// Project — .mgproj file format (JSON, schema_version: 1)
// ============================================================================
//
// .mgproj is a JSON file storing editor state:
//   - schema_version (forward-only migration)
//   - source references (.herm/.ont)
//   - camera, background, time, frame
//   - viewport layout
//   - editor state (cursor, selected node, gizmo mode)
//   - DockSpace layout (ImGui serialization)
//
// All paths are relative to the .mgproj file location.

struct ProjectSource {
    std::wstring path; // .herm or .ont
};

struct Project {
    static constexpr int CURRENT_VERSION = 1;

    int schema_version = CURRENT_VERSION;
    std::vector<ProjectSource> sources;
    Camera  camera;
    Vec3    background     = {0, 0, 0};
    f32     current_time   = 0.0f;
    u32     current_frame  = 0;
    u32     total_frames   = 1;
    int     viewport_x     = 0;
    int     viewport_y     = 0;
    int     viewport_w     = 800;
    int     viewport_h     = 600;

    // Editor state (v1 extension)
    std::string editor_source;      // current .herm code
    int         editor_cursor_line  = 0;
    int         editor_cursor_col   = 0;
    int         selected_node       = -1;
    int         gizmo_mode         = 0; // 0=none, 1=move, 2=rotate, 3=scale
    float       render_scale        = 1.0f;
    bool        auto_scroll         = true;
    int         console_filter      = -1; // -1 = all

    // DockSpace layout (ImGui state, stored as base64 blob)
    std::string dockspace_layout;

    void setDefault() {
        schema_version = CURRENT_VERSION;
        sources.clear();
        camera = {{0,2,5}, {0,0,0}, {0,1,0}, 60.0f};
        background = {0,0,0};
        current_time = 0;
        current_frame = 0;
        total_frames = 1;
        viewport_x = viewport_y = 0;
        viewport_w = 800;
        viewport_h = 600;
        editor_source.clear();
        editor_cursor_line = editor_cursor_col = 0;
        selected_node = -1;
        gizmo_mode = 0;
        render_scale = 1.0f;
        auto_scroll = true;
        console_filter = -1;
        dockspace_layout.clear();
    }

    void applyTo(Scene& scene) {
        scene.camera = camera;
        scene.background = background;
        scene.width  = viewport_w;
        scene.height = viewport_h;
    }

    void applyFrom(const Scene& scene) {
        camera = scene.camera;
        background = scene.background;
        viewport_w = scene.width;
        viewport_h = scene.height;
    }

    // --- JSON write helpers ---
    static void writeIndent(FILE* f, int depth) {
        for (int i = 0; i < depth; i++) fprintf(f, "  ");
    }

    static void writeString(FILE* f, const char* key, const std::string& val, int depth) {
        writeIndent(f, depth);
        fprintf(f, "\"%s\": \"%s\",\n", key, val.c_str());
    }

    static void writeInt(FILE* f, const char* key, int val, int depth) {
        writeIndent(f, depth);
        fprintf(f, "\"%s\": %d,\n", key, val);
    }

    static void writeUInt(FILE* f, const char* key, unsigned int val, int depth) {
        writeIndent(f, depth);
        fprintf(f, "\"%s\": %u,\n", key, val);
    }

    static void writeFloat(FILE* f, const char* key, float val, int depth) {
        writeIndent(f, depth);
        fprintf(f, "\"%s\": %.6f,\n", key, val);
    }

    static void writeBool(FILE* f, const char* key, bool val, int depth) {
        writeIndent(f, depth);
        fprintf(f, "\"%s\": %s,\n", key, val ? "true" : "false");
    }

    // Wide string to UTF-8
    static std::string wideToUtf8(const std::wstring& w) {
        char mbuf[512];
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, mbuf, (int)sizeof(mbuf), nullptr, nullptr);
        return (n > 0) ? std::string(mbuf) : "";
    }

    // UTF-8 to wide string
    static std::wstring utf8ToWide(const std::string& s) {
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring w((size_t)len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
        if (!w.empty() && w.back() == L'\0') w.pop_back();
        return w;
    }

    // Save .mgproj as JSON
    bool save(const wchar_t* path) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"wb") != 0 || !f) return false;

        // BOM for UTF-8
        const u8 bom[] = {0xEF, 0xBB, 0xBF};
        fwrite(bom, 1, 3, f);

        fprintf(f, "{\n");
        writeInt(f, "schema_version", schema_version, 1);

        // Sources array
        writeIndent(f, 1);
        fprintf(f, "\"sources\": [\n");
        for (size_t i = 0; i < sources.size(); i++) {
            writeIndent(f, 2);
            fprintf(f, "\"%s\"%s\n", wideToUtf8(sources[i].path).c_str(),
                    (i + 1 < sources.size()) ? "," : "");
        }
        writeIndent(f, 1);
        fprintf(f, "],\n");

        // Camera
        writeIndent(f, 1);
        fprintf(f, "\"camera\": {\n");
        writeFloat(f, "px", camera.position.x, 2);
        writeFloat(f, "py", camera.position.y, 2);
        writeFloat(f, "pz", camera.position.z, 2);
        writeFloat(f, "tx", camera.target.x, 2);
        writeFloat(f, "ty", camera.target.y, 2);
        writeFloat(f, "tz", camera.target.z, 2);
        writeFloat(f, "fov", camera.fov, 2);
        writeIndent(f, 1);
        fprintf(f, "},\n");

        // Scene state
        writeIndent(f, 1);
        fprintf(f, "\"scene\": {\n");
        writeFloat(f, "bg_r", background.x, 2);
        writeFloat(f, "bg_g", background.y, 2);
        writeFloat(f, "bg_b", background.z, 2);
        writeFloat(f, "time", current_time, 2);
        writeUInt(f, "frame", current_frame, 2);
        writeUInt(f, "total_frames", total_frames, 2);
        writeIndent(f, 1);
        fprintf(f, "},\n");

        // Viewport
        writeIndent(f, 1);
        fprintf(f, "\"viewport\": {\n");
        writeInt(f, "x", viewport_x, 2);
        writeInt(f, "y", viewport_y, 2);
        writeInt(f, "w", viewport_w, 2);
        writeInt(f, "h", viewport_h, 2);
        writeIndent(f, 1);
        fprintf(f, "},\n");

        // Editor state
        writeIndent(f, 1);
        fprintf(f, "\"editor\": {\n");
        writeString(f, "source", editor_source, 2);
        writeInt(f, "cursor_line", editor_cursor_line, 2);
        writeInt(f, "cursor_col", editor_cursor_col, 2);
        writeInt(f, "selected_node", selected_node, 2);
        writeInt(f, "gizmo_mode", gizmo_mode, 2);
        writeFloat(f, "render_scale", render_scale, 2);
        writeBool(f, "auto_scroll", auto_scroll, 2);
        writeInt(f, "console_filter", console_filter, 2);
        writeIndent(f, 1);
        fprintf(f, "},\n");

        // DockSpace layout (base64 blob)
        writeString(f, "dockspace_layout", dockspace_layout, 1);

        fprintf(f, "}\n");
        fclose(f);
        return true;
    }

    // --- JSON read helpers (inline, minimal) ---
    struct JsonReader {
        const char* s;
        size_t pos, len;
        JsonReader(const char* s, size_t len) : s(s), pos(0), len(len) {}
        void skipWhitespace() { while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++; }
        bool peek(char c) { skipWhitespace(); return pos < len && s[pos] == c; }
        bool match(char c) { skipWhitespace(); if (pos < len && s[pos] == c) { pos++; return true; } return false; }
        bool matchStr(const char* str) { skipWhitespace(); size_t l = strlen(str); if (pos + l <= len && memcmp(s + pos, str, l) == 0) { pos += l; return true; } return false; }
        std::string readString() { skipWhitespace(); if (pos >= len || s[pos] != '"') return ""; pos++; std::string r; while (pos < len && s[pos] != '"') { if (s[pos] == '\\') { pos++; if (pos < len) r += s[pos]; } else r += s[pos]; pos++; } if (pos < len) pos++; return r; }
        float readNumber() { skipWhitespace(); char* end; float v = strtof(s + pos, &end); pos = (size_t)(end - s); return v; }
        int readInt() { skipWhitespace(); char* end; int v = (int)strtol(s + pos, &end, 10); pos = (size_t)(end - s); return v; }
        bool readBool() { skipWhitespace(); if (matchStr("true")) return true; if (matchStr("false")) return false; return false; }
        void skipValue() {
            skipWhitespace();
            if (pos >= len) return;
            if (s[pos] == '"') { readString(); return; }
            if (s[pos] == '{') { skipObject(); return; }
            if (s[pos] == '[') { skipArray(); return; }
            if (s[pos] == 't' || s[pos] == 'f') { readBool(); return; }
            // number
            char* end; strtof(s + pos, &end); pos = (size_t)(end - s);
        }
        void skipObject() { if (!match('{')) return; int depth = 1; while (pos < len && depth > 0) { if (s[pos] == '{') depth++; else if (s[pos] == '}') depth--; pos++; } }
        void skipArray() { if (!match('[')) return; int depth = 1; while (pos < len && depth > 0) { if (s[pos] == '[') depth++; else if (s[pos] == ']') depth--; pos++; } }
        std::string readKey() { skipWhitespace(); return readString(); }
        bool expectColon() { return match(':'); }
        bool expectComma() { return match(','); }
    };

    // Load .mgproj from JSON
    bool load(const wchar_t* path) {
        FileMapping fm;
        if (!fm.open(path)) return false;

        const char* data = (const char*)fm.data();
        size_t size = fm.size();
        if (!data || size < 3) { fm.close(); return false; }

        // Skip BOM if present
        size_t off = 0;
        if (size >= 3 && (u8)data[0] == 0xEF && (u8)data[1] == 0xBB && (u8)data[2] == 0xBF)
            off = 3;

        setDefault();

        JsonReader jr(data + off, size - off);
        if (!jr.match('{')) { fm.close(); return false; }

        while (jr.pos < jr.len) {
            jr.skipWhitespace();
            if (jr.peek('}')) { jr.match('}'); break; }

            std::string key = jr.readKey();
            if (!jr.expectColon()) break;

            if (key == "schema_version") {
                schema_version = jr.readInt();
                jr.expectComma();
            } else if (key == "sources") {
                if (jr.match('[')) {
                    while (jr.pos < jr.len && !jr.peek(']')) {
                        std::string src = jr.readString();
                        if (!src.empty()) sources.push_back({utf8ToWide(src)});
                        jr.expectComma();
                    }
                    jr.match(']');
                }
                jr.expectComma();
            } else if (key == "camera") {
                if (jr.match('{')) {
                    while (jr.pos < jr.len && !jr.peek('}')) {
                        std::string ck = jr.readKey();
                        jr.expectColon();
                        if (ck == "px") camera.position.x = jr.readNumber();
                        else if (ck == "py") camera.position.y = jr.readNumber();
                        else if (ck == "pz") camera.position.z = jr.readNumber();
                        else if (ck == "tx") camera.target.x = jr.readNumber();
                        else if (ck == "ty") camera.target.y = jr.readNumber();
                        else if (ck == "tz") camera.target.z = jr.readNumber();
                        else if (ck == "fov") camera.fov = jr.readNumber();
                        else jr.skipValue();
                        jr.expectComma();
                    }
                    jr.match('}');
                }
                jr.expectComma();
            } else if (key == "scene") {
                if (jr.match('{')) {
                    while (jr.pos < jr.len && !jr.peek('}')) {
                        std::string sk = jr.readKey();
                        jr.expectColon();
                        if (sk == "bg_r") background.x = jr.readNumber();
                        else if (sk == "bg_g") background.y = jr.readNumber();
                        else if (sk == "bg_b") background.z = jr.readNumber();
                        else if (sk == "time") current_time = jr.readNumber();
                        else if (sk == "frame") current_frame = (u32)jr.readInt();
                        else if (sk == "total_frames") total_frames = (u32)jr.readInt();
                        else jr.skipValue();
                        jr.expectComma();
                    }
                    jr.match('}');
                }
                jr.expectComma();
            } else if (key == "viewport") {
                if (jr.match('{')) {
                    while (jr.pos < jr.len && !jr.peek('}')) {
                        std::string vk = jr.readKey();
                        jr.expectColon();
                        if (vk == "x") viewport_x = jr.readInt();
                        else if (vk == "y") viewport_y = jr.readInt();
                        else if (vk == "w") viewport_w = jr.readInt();
                        else if (vk == "h") viewport_h = jr.readInt();
                        else jr.skipValue();
                        jr.expectComma();
                    }
                    jr.match('}');
                }
                jr.expectComma();
            } else if (key == "editor") {
                if (jr.match('{')) {
                    while (jr.pos < jr.len && !jr.peek('}')) {
                        std::string ek = jr.readKey();
                        jr.expectColon();
                        if (ek == "source") editor_source = jr.readString();
                        else if (ek == "cursor_line") editor_cursor_line = jr.readInt();
                        else if (ek == "cursor_col") editor_cursor_col = jr.readInt();
                        else if (ek == "selected_node") selected_node = jr.readInt();
                        else if (ek == "gizmo_mode") gizmo_mode = jr.readInt();
                        else if (ek == "render_scale") render_scale = jr.readNumber();
                        else if (ek == "auto_scroll") auto_scroll = jr.readBool();
                        else if (ek == "console_filter") console_filter = jr.readInt();
                        else jr.skipValue();
                        jr.expectComma();
                    }
                    jr.match('}');
                }
                jr.expectComma();
            } else if (key == "dockspace_layout") {
                dockspace_layout = jr.readString();
                jr.expectComma();
            } else {
                jr.skipValue();
                jr.expectComma();
            }
        }

        fm.close();
        return true;
    }

    // Save backup (.mgproj.bak)
    bool saveBackup(const wchar_t* path) {
        std::wstring bak = std::wstring(path) + L".bak";
        return save(bak.c_str());
    }

    // Load backup if main is corrupted
    bool loadWithRecovery(const wchar_t* path) {
        if (load(path)) return true;
        std::wstring bak = std::wstring(path) + L".bak";
        return load(bak.c_str());
    }
};

} // namespace scene
} // namespace mg
