#pragma once

#include <cstdint>
#include <cstring>
#include "../visor/input_bus.h"

namespace mg {

namespace scene {
struct SceneGraph;
}

struct SystemContext_v1 {
    float dt;                          // ΔW = 1/60
    InputSnapshot input;               // snapshot inmutable durante tick
    const float (*tensor_read)[8];     // tensor_snapshot (read-only)
    float (*tensor_delta)[8];          // tensor_delta (write-only, suma atómica)
    scene::SceneGraph* graph;          // mutación de topología (add/remove/reparent)
    uint32_t node_count;               // slots válidos en tensor_buffer
};

using SystemTickFn = void (*)(SystemContext_v1*);

// CameraSystem: slot 0 reservado. Escribe camera.transform en tensor_delta[0].
void camera_system_tick(SystemContext_v1* ctx);

// System registration
struct RegisteredSystem {
    const char* name;
    SystemTickFn tick;
    uint32_t slot;  // tensor slot (0 for camera)
};

// Default systems (v1 stubs)
static const RegisteredSystem DEFAULT_SYSTEMS[] = {
    {"camera_system", camera_system_tick, 0}
};

static constexpr uint32_t DEFAULT_SYSTEM_COUNT = sizeof(DEFAULT_SYSTEMS) / sizeof(DEFAULT_SYSTEMS[0]);

} // namespace mg
