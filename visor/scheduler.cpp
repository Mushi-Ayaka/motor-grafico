// scheduler.cpp - T-110: Scheduler + Anomaly Gate para live-compile.
#include "scheduler.h"
#include <cmath>
#include <cstring>

namespace mg {

void Scheduler::init(CompileFunc compile_fn, ApplyFunc apply_fn) {
    this->compile_fn = compile_fn;
    this->apply_fn = apply_fn;
    state = State::IDLE;
    dirty = false;
    last_edit_ms = 0;
}

void Scheduler::markDirty() {
    dirty = true;
    state = State::DIRTY;
}

uint32_t Scheduler::timeSinceEdit(uint32_t now_ms) const {
    if (last_edit_ms == 0) return 0;
    return now_ms - last_edit_ms;
}

void Scheduler::update(uint32_t now_ms) {
    if (!dirty || state == State::COMPILING || state == State::VALIDATING)
        return;

    // Debounce: esperar hasta que pasen debounce_ms desde la última edición
    if (state == State::DIRTY) {
        if (last_edit_ms == 0) last_edit_ms = now_ms;
        if (now_ms - last_edit_ms < debounce_ms)
            return; // aún en debounce
    }

    // Compilar
    state = State::COMPILING;
    current = compile_fn("");

    // Anomaly Gate: validar
    state = State::VALIDATING;
    if (validate(current)) {
        // Publicar
        last_good = current;
        state = State::PUBLISHED;
        if (apply_fn) apply_fn(current);
        dirty = false;
    } else {
        // Rechazar: mantener último frame bueno
        state = State::STATE_ERROR;
        dirty = false;
    }
}

CompileResult Scheduler::forceCompile(const std::string& source) {
    state = State::COMPILING;
    current = compile_fn(source);

    state = State::VALIDATING;
    if (validate(current)) {
        last_good = current;
        state = State::PUBLISHED;
        if (apply_fn) apply_fn(current);
        dirty = false;
    } else {
        state = State::STATE_ERROR;
        dirty = false;
    }
    return current;
}

bool Scheduler::validate(const CompileResult& result) {
    if (!result.ok) return false;

    // Anomaly Gate: rechazar si no-finito en bytecode o nodes degenerados
    if (result.nodes == 0) return false;
    if (result.materials == 0) return false;
    if (result.bytecode_bytes == 0) return false;

    // TODO: detectar NaN/Inf en bytecode, recursión sin base, repeat sin dominio
    // Por v1, validación básica es suficiente

    return true;
}

} // namespace mg
