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
// Project — .mgproject file format
// ============================================================================
//
// .mgproject is a minimal text format referencing source .herm/.rih files
// and workspace state. It is NOT a scene dump — the scene graph is always
// regenerated from the source files on load.
//
// Format (text, one entry per line):
//   MGPROJECT v1
//   source <path>
//   camera <x> <y> <z> <tx> <ty> <tz> <fov>
//   background <r> <g> <b>
//   time <w>
//   frame <current> <total>
//   viewport <x> <y> <w> <h>
//   END
//
// All paths are relative to the .mgproject file location (or absolute).

struct ProjectSource {
    std::wstring path; // .herm or .rih
};

struct Project {
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

    void setDefault() {
        sources.clear();
        camera = {{0,2,5}, {0,0,0}, {0,1,0}, 60.0f};
        background = {0,0,0};
        current_time = 0;
        current_frame = 0;
        total_frames = 1;
        viewport_x = viewport_y = 0;
        viewport_w = 800;
        viewport_h = 600;
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

    // Save .mgproject as UTF-8 text
    bool save(const wchar_t* path) {
        FILE* f = nullptr;
        if (_wfopen_s(&f, path, L"wb") != 0 || !f) return false;
        // BOM for UTF-8
        const u8 bom[] = {0xEF, 0xBB, 0xBF};
        fwrite(bom, 1, 3, f);

        fprintf(f, "MGPROJECT v1\n");
        for (const auto& src : sources) {
            // Convert wide to UTF-8
            char mbuf[512];
            int n = WideCharToMultiByte(CP_UTF8, 0, src.path.c_str(), -1,
                                         mbuf, (int)sizeof(mbuf), nullptr, nullptr);
            if (n > 0) fprintf(f, "source %s\n", mbuf);
        }
        fprintf(f, "camera %f %f %f %f %f %f %f\n",
                camera.position.x, camera.position.y, camera.position.z,
                camera.target.x, camera.target.y, camera.target.z,
                camera.fov);
        fprintf(f, "background %f %f %f\n",
                background.x, background.y, background.z);
        fprintf(f, "time %f\n", current_time);
        fprintf(f, "frame %u %u\n", current_frame, total_frames);
        fprintf(f, "viewport %d %d %d %d\n",
                viewport_x, viewport_y, viewport_w, viewport_h);
        fprintf(f, "END\n");
        fclose(f);
        return true;
    }

    // Load .mgproject
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

        std::string line;
        while (off < size) {
            line.clear();
            while (off < size && data[off] != '\n') {
                if (data[off] != '\r') line += data[off];
                off++;
            }
            off++; // skip \n
            if (line.empty()) continue;
            if (line == "MGPROJECT v1") continue;
            if (line == "END") break;

            // Parse key-value
            auto space = line.find(' ');
            if (space == std::string::npos) continue;
            std::string key = line.substr(0, space);
            std::string val = line.substr(space + 1);

            if (key == "source") {
                // Convert UTF-8 to wide
                int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, nullptr, 0);
                if (len > 0) {
                    std::wstring wpath((size_t)len, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, &wpath[0], len);
                    if (!wpath.empty() && wpath.back() == L'\0') wpath.pop_back();
                    sources.push_back({wpath});
                }
            } else if (key == "camera") {
                f32 px, py, pz, tx, ty, tz, fov;
                if (sscanf_s(val.c_str(), "%f %f %f %f %f %f %f",
                             &px, &py, &pz, &tx, &ty, &tz, &fov) == 7) {
                    camera.position = {px, py, pz};
                    camera.target   = {tx, ty, tz};
                    camera.fov      = fov;
                }
            } else if (key == "background") {
                f32 r, g, b;
                if (sscanf_s(val.c_str(), "%f %f %f", &r, &g, &b) == 3)
                    background = {r, g, b};
            } else if (key == "time") {
                sscanf_s(val.c_str(), "%f", &current_time);
            } else if (key == "frame") {
                sscanf_s(val.c_str(), "%u %u", &current_frame, &total_frames);
            } else if (key == "viewport") {
                sscanf_s(val.c_str(), "%d %d %d %d",
                         &viewport_x, &viewport_y, &viewport_w, &viewport_h);
            }
        }

        fm.close();
        return true;
    }
};

} // namespace scene
} // namespace mg
