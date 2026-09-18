#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "anomaly_gate.h"

namespace mg {

static constexpr uint32_t MAX_TENSOR_SLOTS = 65536;

struct CompileResult {
    bool ok = false;
    std::string error;
    uint32_t nodes = 0;
    uint32_t materials = 0;
    uint32_t bytecode_bytes = 0;
    uint32_t tensor_slot_count = 0;  // N_nodes + 1 (camera slot 0)
    std::vector<Anomaly> anomalies;  // F0.5: Anomaly Gate results
};

using CompileFunc = std::function<CompileResult(const std::string& source)>;
using ApplyFunc = std::function<void(const CompileResult& result)>;

class Scheduler {
public:
    uint32_t debounce_ms = 30;

    enum class State : uint8_t {
        IDLE,
        DIRTY,
        COMPILING,
        VALIDATING,
        PUBLISHED,
        STATE_ERROR
    };

    State state = State::IDLE;
    CompileResult last_good;
    CompileResult current;

    void init(CompileFunc compile_fn, ApplyFunc apply_fn);
    void markDirty();
    void update(uint32_t now_ms);
    CompileResult forceCompile(const std::string& source);
    static bool validate(const CompileResult& result);
    uint32_t timeSinceEdit(uint32_t now_ms) const;

private:
    CompileFunc compile_fn;
    ApplyFunc apply_fn;
    uint32_t last_edit_ms = 0;
    bool dirty = false;
};

} // namespace mg
