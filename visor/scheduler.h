#pragma once
#include <cstdint>
#include <string>
#include <functional>

namespace mg {

struct CompileResult {
    bool ok = false;
    std::string error;
    uint32_t nodes = 0;
    uint32_t materials = 0;
    uint32_t bytecode_bytes = 0;
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
