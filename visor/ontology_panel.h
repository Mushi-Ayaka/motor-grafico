#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../scene/scene_graph.h"
#include "../render/scene.h"

namespace mg {

struct OntologyNodeInfo {
    uint32_t index = 0;
    std::string name;
    uint32_t depth = 0;
    bool enabled = true;
    bool selected = false;
    bool has_children = false;
};

class OntologyPanel {
public:
    bool visible = true;
    int32_t selected_node = -1;

    // F/Q/B facet visibility (toggles what's shown in tree)
    bool show_facet_f = true;  // Física (transform, forma)
    bool show_facet_q = true;  // Química (material, color)
    bool show_facet_b = true;  // Biología (herencia, ensamblaje)

    void draw(scene::SceneGraph& graph, const Scene& scene);

private:
    void drawNodeRecursive(scene::SceneGraph& graph, const Scene& scene, uint32_t node_idx);
    std::string getNodeLabel(const scene::SceneNode& sn, const Node& rn);
};

} // namespace mg
