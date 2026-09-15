# Systems Architecture v1 — motor-grafico

> **ABI v1 para systems lógicos. CameraSystem es el stub inicial.**

---

## Context

Los systems son funciones puras que transforman el tensor buffer cada tick lógico. No tienen estado propio; todo estado vive en el tensor o en el grafo ontológico.

---

## ABI v1

```cpp
// core/systems.h
#pragma once
#include <cstdint>

struct InputSnapshot {
    bool keys[256];
    float mouse_dx, mouse_dy;
    float scroll_y;
    bool mouse_left, mouse_right, mouse_middle;
};

struct SystemContext_v1 {
    float dt;                          // ΔW = 1/60
    InputSnapshot input;               // snapshot inmutable durante tick
    const float (*tensor_read)[8];     // tensor_snapshot (read-only)
    float (*tensor_delta)[8];          // tensor_delta (write-only, suma atómica)
    struct SceneGraph* graph;          // mutación de topología (add/remove/reparent)
    uint32_t node_count;               // slots válidos en tensor_buffer
};

using SystemTickFn = void (*)(SystemContext_v1*);

// Símbolo exportado: system_tick_v1
// v2 → system_tick_v2, etc. El runtime detecta la versión.
```

---

## Lifecycle

1. **Registro**: `.mgproj` enumera systems: `systems: ["camera_system"]`
2. **Init**: Runtime carga y resuelve `system_tick_v1` por nombre
3. **Tick**: `for (auto& sys : registered_systems) sys.tick(&ctx);`
4. **Aplicación**: `tensor_buffer[slot][c] += tensor_delta[slot][c]` (suma conmutativa)

---

## CameraSystem (v1 Stub)

- **Slot**: 0 (reservado, siempre presente)
- **Input**: `InputSnapshot` (teclado + mouse)
- **Output**: `tensor_delta[0][0..7]` = cámara transform (posición + orientación)
- **Comportamiento**: Mapea WASD → movimiento, mouse → rotación

```cpp
// core/systems.h
void camera_system_tick(SystemContext_v1* ctx);
```

---

## Orden de Ejecución

El orden de registro NO afecta el resultado (suma conmutativa). Cada system escribe en `tensor_delta`. Al final del tick, se aplica la suma.

---

## Versioning

- v1: `system_tick_v1` — CameraSystem stub
- v2: `system_tick_v2` — +PhysicsSystem, +AnimationSystem
- v3: `system_tick_v3` — +ScriptingSystem (Python/C ABI)

El runtime detecta la versión del símbolo y llama a la correspondiente.

---

## Acceptance Criteria

- [ ] CameraSystem escribe en tensor_slot 0
- [ ] Suma conmutativa: orden de registro no afecta resultado
- [ ] .mgproj enumera systems correctamente
- [ ] Runtime carga y ejecuta system_tick_v1
