#pragma once
#include <cstdint>
#include "../scene/workspace.h"

namespace mg {

class TimelinePanel {
public:
    bool visible = false; // v0.35: oculto por default (se activa desde menú Window)

    void draw(scene::Timeline& timeline);

private:
    bool playing_held = false;
};

} // namespace mg
