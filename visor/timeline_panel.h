#pragma once
#include <cstdint>
#include "../scene/workspace.h"

namespace mg {

class TimelinePanel {
public:
    bool visible = true;

    void draw(scene::Timeline& timeline);

private:
    bool playing_held = false;
};

} // namespace mg
