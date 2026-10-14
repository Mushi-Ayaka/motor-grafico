---
title: Plan de madurez del pipeline Vulkan y optimización F4/F5
tags: [motor-grafico, pipeline, vulkan, optimizacion, f4, f5, timestamps, frames-in-flight]
created: 2026-10-11
---

# 19 — Madurez del pipeline Vulkan + Optimización (Tracks A/B/C)

> Contexto heredado de `docs/ARCHITECTURE_PARTITION_SPEC.md` (F0–F6), `docs/12-brainstorming-optimizacion.md`
> y la auditoría del 2026-10-11 sobre las corridas interactivas de las 5 escenas benchmark.
> Este documento consolida el plan aprobado para no perder contexto entre sesiones.

## 0. Diagnóstico que originó este plan (2026-10-11)

### 0.1 Síntoma reportado por el usuario
- En Administrador de tareas: GPU ~100% solo en el motor **3D**; sin Copy, sin Video Encode/Decode,
  sin uso visible de memoria dedicada ni compartida de la NVIDIA.
- Escena 05 mostraba 995 fps en el overlay (engañoso).

### 0.2 Veredicto por síntoma
| Síntoma | Veredicto | Causa |
|---|---|---|
| 3D ~100% | Esperado y bueno | Compute shaders cuentan como motor "3D" en Windows. El ray marcher es compute-bound. |
| Sin Copy | Esperado hoy | Copias de pocos MB por la misma cola; no hay streaming. Aparecerá con BrickMap (B2). |
| Sin Video Encode/Decode | Esperado | No hay export de video (M24 exporta .exe). NVENC = feature futura (C2). |
| Sin memoria dedicada visible | Medio erróneo | `nvidia-smi` confirma 822 MiB en la RTX 3060. Task Manager en Optimus puede mostrar la sección de la iGPU AMD (GPU 0) en vez de la NVIDIA (GPU 1). |
| Memoria baja (822/6144 MiB) | Síntoma real de inmadurez | Falta BrickMap en VRAM, history buffers, caches de SDF (los planes B2/B6). |

### 0.3 Bugs/inmadureces confirmados en código (la causa real del "uso raro")
1. **1 solo frame in flight** (`vulkan_core.cpp`): `drawFrame` espera `in_flight_fence` al principio;
   compute y ImGui van a la **misma cola** (en NVIDIA `graphics_family_idx == compute_family_idx`).
   La GPU muere en los huecos CPU (ImGui + GDI + pump). En escenas chicas el GPU está ocioso ~20% del frame.
2. **Cero GPU timestamps**: no existe ningún `VkQueryPool`/`vkCmdWriteTimestamp` en `render/`.
   `render_ms_qpc` mezcla CPU wall-clock (acquire + ImGui + GDI + GPU). Imposible medir optimizaciones con rigor.
3. **Fence reseteado antes del early-return de acquire** (`vulkan_core.cpp:614-621`): si `vkAcquireNextImageKHR`
   devuelve OUT_OF_DATE/SUBOPTIMAL, se retorna `false` con el fence sin señalar y sin submit → el caller
   (`visor_app.cpp:605`) ignora el return → potencial congelamiento permanente en el próximo `vkWaitForFences(UINT64_MAX)`.
4. **Stats miden iteración de loop, no renders**: `visor_app.cpp:628-641` registra cada vuelta (idle incluido);
   overlay `fps = frame_count` cuenta iteraciones, no renders → "995fps" ficticio, cv=4218%.
5. **UBO mapeado/desmapeado cada frame** (`updateUBO`): debería ser un ring persistentemente mapeado.
6. **`vkDeviceWaitIdle` en `resizeOutputBuffer`**: tumba toda la GPU en cada resize.
7. **Stalls de 1–14s en sesiones interactivas**: `vkAcquireNextImageKHR(UINT64_MAX)` bloquea con la ventana
   minimizada/oculta (DWM deja de compositar). Los frames muertos post-stall (~0.65ms) son el loop spinneando
   sin dispatch real.
8. **El shader hace ~5× el trabajo necesario**: normales por diferencias finitas (3 evals BVH extra por hit),
   sin BrickMap en GPU, sin subgroup early-exit, d_min degenerado en AABBs conservativas.

## 1. Track A — Madurez del pipeline Vulkan (✅ COMPLETO 2026-10-11)

| ID | Tarea | Estado | Archivos |
|----|-------|--------|----------|
| **A1** | GPU timestamps: `VkQueryPool` + `vkCmdWriteTimestamp` bracket al dispatch; columna `gpu_ms` en `runs.csv` | ✅ | `vulkan_core.h/cpp` |
| **A2** | Stats honestas: `frames_render`/`frames_skip`; fix fence-reset-before-acquire; caller maneja `return false` | ✅ | `vulkan_core.cpp`, `visor_app.cpp` |
| **A3** | Frames in flight = 2: per-frame fences/semaphores/command buffers + UBO ring (dynamic offset, stride 128) + imgui fence per-frame | ✅ | `vulkan_core.h/cpp`, `vulkan_pipeline.h/cpp` |
| **A4** | Captura de stderr en `.bat` + polling `nvidia-smi` 1Hz en `--stats` → columnas en `runs.csv` | ✅ | `correr_*.bat`, `benchmark_las_5.bat`, `visor_app.cpp` |
| **A5** | Destrucción diferida (per-frame deletion queue) en vez de `vkDeviceWaitIdle` en resize | ⏳ | `vulkan_pipeline.cpp` |

### Resultados del ladder post-Track-A (RTX 3060 Laptop, 2026-10-11)

| Escena | gpu_ms (med) | gpu_p95 | render_ms (CPU) | util GPU | power |
|--------|-------------|---------|-----------------|----------|-------|
| 01_bodega_baja | **0.238ms** | 0.256ms | 3.097ms | 5% | 28W |
| 02_sala | **0.817ms** | 0.914ms | 1.568ms | 3% | 28W |
| 03_galeria | **2.420ms** | 2.949ms | 2.867ms | 9% | 36W |
| 04_museo | **3.609ms** | 4.365ms | 3.764ms | 2% | 67W |
| 05_catedral | **7.573ms** | 9.770ms | 7.249ms | 2% | 39W |

- `frames_render=200, frames_skip=0` en todas las escenas → stats 100% honestas.
- En escenas complejas (04, 05) `render_ms ≈ gpu_ms` → **GPU-bound**.
- En escenas simples (01, 02) `render_ms >> gpu_ms` → **CPU-bound** por overhead de Vulkan (acquire, submit, present, ImGui).
- **GPU muy subutilizada** (2-9%, 28-67W de 115W TGP). La causa raíz del "995fps" y del uso raro en Task Manager era la medición incorrecta, no la GPU.

### Bugs encontrados y corregidos durante A3
1. **UBO dynamic offset desalineado** (96 bytes, `minUniformBufferOffsetAlignment=64`) → `VK_ERROR_DEVICE_LOST`. Fix: `UBO_STRIDE=128`.
2. **Descriptor UBO range completo** (192 bytes) con dynamic offset 96 → out-of-bounds. Fix: range = `sizeof(UboData)`.
3. **ImGui command buffer single** con 2 frames en vuelo → race condition. Fix: `imgui_command_buffer[FRAMES_IN_FLIGHT]` + `imgui_fence[FRAMES_IN_FLIGHT]`.
4. **Timestamp queries sin reset** → "query not reset" validation error. Fix: `vkCmdResetQueryPool` + flag `timestamp_submitted[f]`.

## 2. Track B — Optimización de shader/escena (hereda F4/F5 + docs/12)

| ID | Tarea | Fuente | Impacto estimado |
|----|-------|--------|------------------|
| **B1** | Normales analíticas (reducir 4 evals → 1) en `glsl_gen.cpp` | `docs/12` §6B, spec F5 | 1.5–2× |
| **B2** | BrickMap en GPU (SSBO/3D texture + sampling en el march) | `docs/12` §6C, spec F5 | 3–10× en escenas grandes |
| **B3** | d_min real / Lipschitz en BVH (muestreo por AABB en compiler) | `implementation_plan.md` A1 | Restaura sphere tracing |
| **B4** | Dynamic Pruner (`dynamic_pruner.comp`: freeze/cull por W-delta + frustum) | spec F4, D6 | Escenas estáticas: nodos vivos −Y% |
| **B5** | Subgroup early-exit en el ray march loop | `docs/12` §6D | 1.2–2× |
| **B6** | Temporal reproyección (depth history buffer) | `docs/12` §2B | 3–5× en cámara lenta |

## 3. Track C — Memoria y features que "encienden" los motores que faltan

| ID | Tarea | Nota |
|----|-------|------|
| **C1** | Verificar hit rates de F2 RAM caches en `runs.csv` | Ya implementado; medir. |
| **C2** | NVENC export (M24 extendido: render→video) | Opcional futuro; hará aparecer Video Encode. No bloquea. |
| **C3** | Streaming de BrickMap cuando B2 entre | VRAM esperada: 822 MiB → ~1.5–2 GiB con escena grande (esperado, no leak). |

## 4. Criterio de éxito integrado

- Ladder de `gpu_ms` (timestamp real, no CPU wall-clock) post-optimización objetivo:
  **<1 / <1.2 / <2 / <2.5 / <4 ms** para 01→05 (baseline actual render_ms: 1.14 / 1.55 / 2.67 / 3.78 / 7.70).
- GPU trabaja ≥90% de la ventana del frame en escenas chicas (verificado con A1+A3).
- `runs.csv` con columnas nuevas: `gpu_ms_*`, `frames_render`, `frames_skip`, `gpu_util_pct`, `gpu_power_w`,
  `gpu_temp_c`, `vram_mb`, `gpu_clock_mhz`.
- Sin frames muertos ni stalls sin diagnosticar en sesiones interactivas.
- Tests F0/F1/F3 siguen en verde (24/10/16).

## 5. Deuda que este plan cierra

- `CONTEXT.md` deuda #1 (evalLeaf stub) y #5 (F4/F5 no implementados) → Track B.
- Deuda de medición: sin GPU timestamps no había forma de distinguir CPU-bound de GPU-bound.
- Bug del "995fps" y las stats de cv ridículo → A2/A4.

## 6. Decisiones técnicas registradas

- **No se borra `ray_march.comp` ni `glsl_gen.cpp`** (condición C5 de la spec): solo se dejan de usar por
  defecto tras flag/build option cuando F3/F5 estén estables.
- **Frames in flight = 2** (no 3): en laptops Optimus con MAILBOX, 2 es el punto óptimo latencia/throughput
  sin buffering excesivo; 3 se evalúa solo si A3 no alcanza el 90% de utilización.
- **Timestamps en la cola de compute** (no en graphics): el trabajo de interés es el dispatch del ray march.
- **nvidia-smi vía `_popen` una vez por segundo** en `--stats` (no por frame): costo despreciable, no contamina
  las mediciones de frame.
