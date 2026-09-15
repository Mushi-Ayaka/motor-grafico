#pragma once

#include <cstdint>

struct FixedTimestep {
    static constexpr float DT = 1.0f / 60.0f;   // ΔW = 1/60
    float accumulator = 0.0f;
    float alpha = 0.0f;                         // interpolación [0,1] para render
    uint32_t tick_count = 0;                    // contador de ticks lógicos

    void advance(float real_dt) {
        accumulator += real_dt;
        while (accumulator >= DT) {
            accumulator -= DT;
            tick_count++;
        }
        alpha = accumulator / DT;
    }

    float currentW() const {
        return (float)tick_count * DT;
    }

    void reset() {
        accumulator = 0.0f;
        alpha = 0.0f;
        tick_count = 0;
    }
};
