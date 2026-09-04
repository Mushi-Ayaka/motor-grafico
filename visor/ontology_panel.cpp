#include "ontology_panel.h"
#include "imgui.h"
#include <cstdio>

namespace mg {

std::string OntologyPanel::getNodeLabel(const scene::SceneNode& sn, const Node& rn) {
    char buf[128];
    const char* type_str = "?";
    switch (rn.type) {
        case NodeType::SDF:      type_str = "SDF"; break;
        case NodeType::GROUP:    type_str = "GRP"; break;
        case NodeType::INSTANCE: type_str = "INS"; break;
    }
    snprintf(buf, sizeof(buf), "[%s] Node %u", type_str, sn.id);
    return buf;
}

void OntologyPanel::drawNodeRecursive(scene::SceneGraph& graph, const Scene& scn, uint32_t node_idx) {
    if (node_idx >= graph.nodes.size() || node_idx >= scn.nodes.size()) return;

    const scene::SceneNode& sn = graph.nodes[node_idx];
    const Node& rn = scn.nodes[node_idx];

    std::string label = getNodeLabel(sn, rn);
    bool has_children = (sn.first_child != 0xFFFFFFFF);

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;
    if ((int32_t)node_idx == selected_node) flags |= ImGuiTreeNodeFlags_Selected;

    // Push enabled/disabled style
    if (!sn.enabled) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

    bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)node_idx, flags, "%s", label.c_str());

    if (!sn.enabled) ImGui::PopStyleColor();

    // Selection on click
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selected_node = node_idx;
    }

    if (node_open) {
        // Show node info as child items
        if (show_facet_f) {
            ImGui::Text("T: [%.2f, %.2f, %.2f]", sn.local_translate.x, sn.local_translate.y, sn.local_translate.z);
            ImGui::Text("R: [%.2f, %.2f, %.2f]", sn.local_rotate.x, sn.local_rotate.y, sn.local_rotate.z);
            ImGui::Text("S: [%.2f, %.2f, %.2f]", sn.local_scale.x, sn.local_scale.y, sn.local_scale.z);
        }

        if (show_facet_q && rn.type == NodeType::SDF) {
            ImGui::Text("SDF: %s", rn.sdf.sdf_type.c_str());
            if (rn.material_id != 0xFFFFFFFF && rn.material_id < scn.materials.size()) {
                const auto& mat = scn.materials[rn.material_id];
                ImGui::Text("Mat: [%s]", mat.name.c_str());
            }
        }

        if (show_facet_b) {
            ImGui::Text("Enabled: %s", sn.enabled ? "YES" : "NO");
            if (sn.parent != 0xFFFFFFFF) {
                ImGui::Text("Parent: %u", sn.parent);
            }
        }

        // Recurse children
        uint32_t child = sn.first_child;
        while (child != 0xFFFFFFFF) {
            drawNodeRecursive(graph, scn, child);
            child = graph.nodes[child].next_sibling;
        }

        ImGui::TreePop();
    }
}

void OntologyPanel::draw(scene::SceneGraph& graph, const Scene& scn) {
    if (!visible) return;

    if (ImGui::Begin("Ontology Tree", &visible)) {
        // Facet filter checkboxes
        ImGui::Checkbox("F", &show_facet_f);
        ImGui::SameLine();
        ImGui::Checkbox("Q", &show_facet_q);
        ImGui::SameLine();
        ImGui::Checkbox("B", &show_facet_b);
        ImGui::SameLine();
        if (ImGui::Button("+ Node")) {
            // TODO: Add node to scene
        }
        ImGui::Separator();

        if (graph.nodes.empty()) {
            ImGui::TextDisabled("No nodes in scene");
        } else {
            // Find root nodes (no parent)
            for (uint32_t i = 0; i < (uint32_t)graph.nodes.size(); i++) {
                if (graph.nodes[i].parent == 0xFFFFFFFF) {
                    drawNodeRecursive(graph, scn, i);
                }
            }
        }

        // Selection info
        if (selected_node >= 0 && selected_node < (int32_t)graph.nodes.size()) {
            ImGui::Separator();
            ImGui::Text("Selected: Node %d", selected_node);
            const scene::SceneNode& sn = graph.nodes[selected_node];

            // Enable/disable toggle
            bool en = sn.enabled;
            if (ImGui::Checkbox("Enabled", &en)) {
                graph.nodes[selected_node].enabled = en;
                graph.markDirty(selected_node);
            }
        }
    }
    ImGui::End();
}

} // namespace mg
