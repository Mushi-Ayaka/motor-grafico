#include "tensor_inspector.h"
#include "imgui.h"
#include <cstdio>
#include <cmath>

namespace mg {

void TensorInspector::updateHistory(float tensor[8]) {
    for (int i = 0; i < 8; i++) {
        history[i][history_idx] = tensor[i];
    }
    history_idx = (history_idx + 1) % HISTORY_SIZE;
}

void TensorInspector::draw(OntologyPanel& ontology, scene::SceneGraph& graph, Scene& scene) {
    if (!visible) return;

    if (ImGui::Begin("Tensor Inspector", &visible)) {
        int32_t sel = ontology.selected_node;
        if (sel < 0 || sel >= (int32_t)graph.nodes.size() || sel >= (int32_t)scene.nodes.size()) {
            ImGui::TextDisabled("No node selected");
            ImGui::End();
            return;
        }

        Node& rn = scene.nodes[sel];

        // Check if node has a material with tensor data
        if (rn.material_id == 0xFFFFFFFF || rn.material_id >= (uint32_t)scene.materials.size()) {
            ImGui::TextDisabled("No material assigned");
            ImGui::End();
            return;
        }

        auto& mat = scene.materials[rn.material_id];

        // Display tensor components as color swatch
        ImGui::Text("Material: %s", mat.name.c_str());
        ImGui::Separator();

        // Tensor 1×8 as RGBA color
        float tensor[8] = {
            mat.base_color.x, mat.base_color.y, mat.base_color.z, 1.0f,
            mat.emission.x, mat.emission.y, mat.emission.z, 1.0f
        };

        // Update history
        updateHistory(tensor);

        // Color preview
        ImVec4 col(mat.base_color.x, mat.base_color.y, mat.base_color.z, 1.0f);
        ImGui::ColorButton("##tensor_color", col, 0, ImVec2(60, 60));

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("XYZ: [%.3f, %.3f, %.3f]", tensor[0], tensor[1], tensor[2]);
        ImGui::Text("W:   %.3f", tensor[3]);
        ImGui::Text("RGB: [%.3f, %.3f, %.3f]", tensor[4], tensor[5], tensor[6]);
        ImGui::Text("A:   %.3f", tensor[7]);
        ImGui::EndGroup();

        ImGui::Separator();

        // Component plots
        ImGui::Text("Component History");
        const char* labels[8] = {"X", "Y", "Z", "W", "R", "G", "B", "A"};
        ImVec4 colors[8] = {
            ImVec4(1,0,0,1), ImVec4(0,1,0,1), ImVec4(0,0,1,1), ImVec4(1,1,0,1),
            ImVec4(1,0.5f,0,1), ImVec4(0,1,0.5f,1), ImVec4(0.5f,0,1,1), ImVec4(1,1,1,1)
        };

        for (int i = 0; i < 8; i++) {
            char overlay[32];
            snprintf(overlay, sizeof(overlay), "%s: %.3f", labels[i], tensor[i]);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, colors[i]);
            ImGui::PlotLines(labels[i], history[i], HISTORY_SIZE, history_idx, overlay, -1.0f, 1.0f, ImVec2(0, 30));
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        // Anomaly detection (simple thresholds)
        ImGui::Text("Anomaly Flags");
        bool has_anomaly = false;

        // Check for NaN
        for (int i = 0; i < 8; i++) {
            if (std::isnan(tensor[i])) {
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "NaN detected in component %d", i);
                has_anomaly = true;
            }
        }

        // Check for extreme values
        for (int i = 0; i < 8; i++) {
            if (std::abs(tensor[i]) > 100.0f) {
                ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "Extreme value in component %d: %.3f", i, tensor[i]);
                has_anomaly = true;
            }
        }

        if (!has_anomaly) {
            ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "No anomalies detected");
        }
    }
    ImGui::End();
}

} // namespace mg
