#include "timeline_panel.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>

namespace mg {

void TimelinePanel::draw(scene::Timeline& timeline) {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(600, 80), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Timeline", &visible)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Transport controls
        float button_width = 30.0f;
        float spacing = 4.0f;

        // Rewind button
        if (ImGui::Button("<<", ImVec2(button_width, 0))) {
            timeline.gotoFrame(timeline.start_frame);
        }
        ImGui::SameLine();

        // Play/Pause button
        const char* play_label = timeline.playing ? "||" : ">";
        if (ImGui::Button(play_label, ImVec2(button_width, 0))) {
            timeline.playing = !timeline.playing;
        }
        ImGui::SameLine();

        // Stop button
        if (ImGui::Button("[]", ImVec2(button_width, 0))) {
            timeline.playing = false;
            timeline.gotoFrame(timeline.start_frame);
        }
        ImGui::SameLine();

        // Loop toggle
        ImGui::Checkbox("Loop", &timeline.loop);
        ImGui::SameLine();

        // Playback speed
        ImGui::SetNextItemWidth(80.0f);
        ImGui::SliderFloat("Speed", &timeline.playback_speed, 0.1f, 4.0f, "%.1fx");
        ImGui::SameLine();

        // FPS
        ImGui::SetNextItemWidth(60.0f);
        ImGui::InputFloat("FPS", &timeline.fps, 1.0f, 10.0f, "%.0f");
        ImGui::SameLine();

        // Current frame display
        ImGui::Text("Frame: %u / %u", timeline.current_frame, timeline.end_frame);

        // W-Scrubber (slider)
        float w = timeline.getW();
        ImGui::SetNextItemWidth(avail.x - 200.0f);
        if (ImGui::SliderFloat("W", &w, 0.0f, (float)timeline.end_frame / timeline.fps, "%.3f")) {
            timeline.current_time = w;
            timeline.current_frame = (u32)(w * timeline.fps);
            timeline.playing = false; // pause on manual scrub
        }

        // Frame range inputs
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::InputInt("Start", (int*)&timeline.start_frame, 0, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::InputInt("End", (int*)&timeline.end_frame, 0, 0);

    }
    ImGui::End();
}

} // namespace mg
