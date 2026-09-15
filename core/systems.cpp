#include "systems.h"
#include "../visor/input_bus.h"

namespace mg {

void camera_system_tick(SystemContext_v1* ctx) {
    if (!ctx) return;

    const InputSnapshot& input = ctx->input;
    float speed = 5.0f * ctx->dt;

    // Read current camera position from tensor_read[0]
    float px = ctx->tensor_read[0][0];
    float py = ctx->tensor_read[0][1];
    float pz = ctx->tensor_read[0][2];

    // WASD movement
    if (input.keys['W']) pz -= speed;
    if (input.keys['S']) pz += speed;
    if (input.keys['A']) px -= speed;
    if (input.keys['D']) px += speed;
    if (input.keys['Q']) py -= speed;
    if (input.keys['E']) py += speed;

    // Write delta to tensor_delta[0]
    ctx->tensor_delta[0][0] += px - ctx->tensor_read[0][0];
    ctx->tensor_delta[0][1] += py - ctx->tensor_read[0][1];
    ctx->tensor_delta[0][2] += pz - ctx->tensor_read[0][2];
    ctx->tensor_delta[0][3] += 0.0f; // W_frame (shared in v1)
}

} // namespace mg
