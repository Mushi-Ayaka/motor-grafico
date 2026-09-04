#include "profiler_panel.h"
#include "imgui.h"
#include <cstdio>

namespace mg {

void ProfilerPanel::draw() {
    if (!visible) return;
    if (ImGui::Begin("Profiler", &visible)) {
        // FPS + frame time
        ImGui::Text("FPS: %u", stats.fps);
        ImGui::SameLine(120);
        ImGui::Text("Frame: %u ms", stats.render_ms);

        // Pump time
        ImGui::Text("Pump: %u ms", stats.pump_ms);
        ImGui::SameLine(120);
        ImGui::Text("Scale: %.2f", stats.render_scale);

        ImGui::Separator();

        // Viewport
        ImGui::Text("Viewport: %ux%u", stats.viewport_w, stats.viewport_h);
        ImGui::SameLine(120);
        ImGui::Text("Mode: %s", stats.ont_mode ? "GPU" : "CPU");

        ImGui::Separator();

        // Scene stats
        ImGui::Text("Nodes: %u", stats.node_count);
        ImGui::SameLine(120);
        ImGui::Text("Materials: %u", stats.material_count);
        ImGui::Text("Bytecode: %u bytes", stats.bytecode_size);

        ImGui::Separator();

        // Frame time graph
        ImGui::Text("Frame Time (ms):");
        float max_ms = 0.0f;
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (frame_ms_history[i] > max_ms) max_ms = frame_ms_history[i];
        }
        if (max_ms < 1.0f) max_ms = 1.0f;

        char overlay[32];
        snprintf(overlay, sizeof(overlay), "%.1f ms", frame_ms_history[(history_idx - 1 + HISTORY_SIZE) % HISTORY_SIZE]);
        ImGui::PlotLines("##frametime", frame_ms_history, HISTORY_SIZE, history_idx, overlay, 0.0f, max_ms * 1.2f, ImVec2(0, 60));
    }
    ImGui::End();
}

} // namespace mg
