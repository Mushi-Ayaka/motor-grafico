#pragma once
#include <cstdint>
#include "../scene/scene_graph.h"
#include "../render/scene.h"
#include "ontology_panel.h"

namespace mg {

class InspectorPanel {
public:
    bool visible = true;

    void draw(OntologyPanel& ontology, scene::SceneGraph& graph, Scene& scene);

private:
    void drawFisica(scene::SceneNode& sn, Node& rn);
    void drawQuimica(Node& rn, Scene& scene);
    void drawBiologia(scene::SceneNode& sn);
};

} // namespace mg
