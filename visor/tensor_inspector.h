#pragma once
#include <cstdint>
#include "../scene/scene_graph.h"
#include "../render/scene.h"
#include "ontology_panel.h"

namespace mg {

class TensorInspector {
public:
    bool visible = true;

    // History for component plots (last 60 values)
    static constexpr int HISTORY_SIZE = 60;
    float history[8][HISTORY_SIZE] = {};
    int history_idx = 0;

    void draw(OntologyPanel& ontology, scene::SceneGraph& graph, Scene& scene);
    void updateHistory(float tensor[8]);
};

} // namespace mg
