# Determinism Spec — Input Replay & SHA-256

> **Formato de input log, replay, y validación de determinismo cross-run.**

---

## Input Event Format

```cpp
// core/input_replay.h
#pragma pack(push, 1)
struct InputEvent {
    uint32_t tick;      // tick lógico (0, 1, 2...)
    uint8_t  type;      // 0=key, 1=mouse_move, 2=mouse_btn, 3=scroll
    uint8_t  key;       // VK code (si type=0)
    uint8_t  down;      // 0=up, 1=down (NO bool: portable)
    int16_t  dx, dy;    // si type=1/3
};
#pragma pack(pop)
```

**Tamaño**: 10 bytes por evento

---

## Recording

```bash
./visor.exe scene.ont --record input.log
```

- Captura todo input del usuario
- Cada evento se escribe al final del archivo
- `tick` se incrementa cada FixedTimestep::advance
- Formato: secuencia de `InputEvent` binario little-endian

---

## Replay

```bash
./visor.exe scene.ont --replay input.log
```

- Lee eventos del archivo
- Para cada tick lógico, aplica los eventos correspondientes
- No hay input real del usuario
- FixedTimestep avanza sin input externo

---

## Determinism Validation

```bash
# Run 1: grabar
./visor.exe scene.ont --record input.log

# Run 2: reproducir
./visor.exe scene.ont --replay input.log

# Comparar: sha256 del framebuffer RGBA cada 60 ticks
# Resultado: byte-a-byte idéntico (0 tolerancia)
```

---

## SHA-256 Checksum

- Se calcula sobre `output_staging` (framebuffer RGBA completo)
- Cada 60 ticks (= 1 segundo lógico)
- Se guarda en archivo separado: `input.log.sha256`
- Comparación: byte-a-byte, no hash de hash

---

## Threading Constraints

- **NO threads en replay**: todo es single-threaded
- **NO async en record**: input se captura síncrono
- **Determinismo garantizado**: mismos inputs → mismos outputs

---

## FixedTimestep Integration

```cpp
struct FixedTimestep {
    static constexpr float DT = 1.0f / 60.0f;   // ΔW = 1/60
    float accumulator = 0.0f;
    float alpha = 0.0f;

    void advance(float real_dt) {
        accumulator += real_dt;
        while (accumulator >= DT) {
            accumulator -= DT;
            tick();  // avanza W en ΔW, captura/aplica input
        }
        alpha = accumulator / DT;
    }
};
```

---

## Acceptance Criteria

- [ ] InputEvent es 10 bytes, pack(1), little-endian
- [ ] --record captura todo input
- [ ] --replay reproduce sin input real
- [ ] SHA-256 cada 60 ticks
- [ ] Cross-run: byte-a-byte idéntico
- [ ] Single-threaded en replay
- [ ] FixedTimestep integra input capture/apply
