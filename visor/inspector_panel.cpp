#include "inspector_panel.h"
#include "imgui.h"
#include <cstdio>

namespace mg {

void InspectorPanel::drawFisica(scene::SceneNode& sn, Node& rn) {
    if (ImGui::CollapsingHeader("Física", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Transform
        ImGui::Text("Transform");
        bool changed = false;

        float t[3] = {sn.local_translate.x, sn.local_translate.y, sn.local_translate.z};
        if (ImGui::DragFloat3("Translate", t, 0.1f)) {
            sn.local_translate = {t[0], t[1], t[2]};
            rn.translate = sn.local_translate;
            changed = true;
        }

        float r[3] = {sn.local_rotate.x, sn.local_rotate.y, sn.local_rotate.z};
        if (ImGui::DragFloat3("Rotate", r, 0.5f)) {
            sn.local_rotate = {r[0], r[1], r[2]};
            rn.rotate = sn.local_rotate;
            changed = true;
        }

        float s[3] = {sn.local_scale.x, sn.local_scale.y, sn.local_scale.z};
        if (ImGui::DragFloat3("Scale", s, 0.05f)) {
            sn.local_scale = {s[0], s[1], s[2]};
            rn.scale = sn.local_scale;
            changed = true;
        }

        if (changed) {
            sn.transform_dirty = true;
            sn.aabb_dirty = true;
        }

        ImGui::Separator();

        // Shape info (read-only for now)
        if (rn.type == NodeType::SDF) {
            ImGui::Text("SDF Type: %s", rn.sdf.sdf_type.c_str());
            for (int i = 0; i < 4; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Param[%d]", i);
                float val = rn.sdf.params[i].is_expr ? 1.0f : rn.sdf.params[i].constant;
                if (ImGui::DragFloat(label, &val, 0.01f)) {
                    rn.sdf.params[i].constant = val;
                    rn.sdf.params[i].is_expr = false;
                }
            }
        }
    }
}

void InspectorPanel::drawQuimica(Node& rn, Scene& scene) {
    if (ImGui::CollapsingHeader("Química", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (rn.material_id != 0xFFFFFFFF && rn.material_id < (uint32_t)scene.materials.size()) {
            auto& mat = scene.materials[rn.material_id];

            ImGui::Text("Material: %s", mat.name.c_str());

            // Base color as RGB floats
            float col[3] = {mat.base_color.x, mat.base_color.y, mat.base_color.z};
            if (ImGui::ColorEdit3("Base Color", col)) {
                mat.base_color = {col[0], col[1], col[2]};
            }

            // PBR sliders
            float metallic = mat.metallic;
            if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
                mat.metallic = metallic;
            }

            float roughness = mat.roughness;
            if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
                mat.roughness = roughness;
            }

            float em[3] = {mat.emission.x, mat.emission.y, mat.emission.z};
            if (ImGui::ColorEdit3("Emission", em)) {
                mat.emission = {em[0], em[1], em[2]};
            }
        } else {
            ImGui::TextDisabled("No material assigned");
        }
    }
}

void InspectorPanel::drawBiologia(scene::SceneNode& sn) {
    if (ImGui::CollapsingHeader("Biología", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Enabled state
        bool en = sn.enabled;
        if (ImGui::Checkbox("Enabled", &en)) {
            sn.enabled = en;
        }

        // Hierarchy info
        ImGui::Text("ID: %u", sn.id);
        ImGui::Text("Depth: %u", sn.depth);
        if (sn.parent != 0xFFFFFFFF) {
            ImGui::Text("Parent: %u", sn.parent);
        } else {
            ImGui::Text("Parent: ROOT");
        }

        // Child count
        uint32_t child_count = 0;
        uint32_t child = sn.first_child;
        while (child != 0xFFFFFFFF) {
            child_count++;
            child = 0xFFFFFFFF; // prevent infinite loop for now
        }
        ImGui::Text("Children: %u", child_count);
    }
}

void InspectorPanel::draw(OntologyPanel& ontology, scene::SceneGraph& graph, Scene& scene) {
    if (!visible) return;

    if (ImGui::Begin("Inspector", &visible)) {
        int32_t sel = ontology.selected_node;
        if (sel < 0 || sel >= (int32_t)graph.nodes.size() || sel >= (int32_t)scene.nodes.size()) {
            ImGui::TextDisabled("No node selected");
            ImGui::End();
            return;
        }

        scene::SceneNode& sn = graph.nodes[sel];
        Node& rn = scene.nodes[sel];

        // Node header
        const char* type_str = "?";
        switch (rn.type) {
            case NodeType::SDF:      type_str = "SDF"; break;
            case NodeType::GROUP:    type_str = "GROUP"; break;
            case NodeType::INSTANCE: type_str = "INSTANCE"; break;
        }
        ImGui::Text("[%s] Node %d", type_str, sel);
        ImGui::Separator();

        // F/Q/B sections
        drawFisica(sn, rn);
        drawQuimica(rn, scene);
        drawBiologia(sn);

        // Mark graph dirty if anything changed
        if (ImGui::IsWindowFocused()) {
            graph.markDirty(sel);
        }
    }
    ImGui::End();
}

} // namespace mg
