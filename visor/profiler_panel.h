#pragma once
#include <cstdint>
#include <cstring>

namespace mg {

struct ProfilerStats {
    uint32_t fps = 0;
    uint32_t render_ms = 0;
    uint32_t pump_ms = 0;
    float render_scale = 1.0f;
    uint32_t viewport_w = 0;
    uint32_t viewport_h = 0;
    uint32_t node_count = 0;
    uint32_t material_count = 0;
    uint32_t bytecode_size = 0;
    bool ont_mode = false;
};

class ProfilerPanel {
public:
    bool visible = true;
    ProfilerStats stats;

    // History for mini graph (last 120 frames)
    static constexpr int HISTORY_SIZE = 120;
    float frame_ms_history[HISTORY_SIZE] = {};
    int history_idx = 0;

    void update(const ProfilerStats& s) {
        stats = s;
        frame_ms_history[history_idx] = (float)s.render_ms;
        history_idx = (history_idx + 1) % HISTORY_SIZE;
    }

    void draw();
};

} // namespace mg
