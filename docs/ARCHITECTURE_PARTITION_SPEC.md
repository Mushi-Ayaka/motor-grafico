# ARCHITECTURE PARTITION SPEC — CPU / GPU / RAM (v1.0 propuesta)

> Estado: PROPUESTA para discutir con pinzas. No reemplaza nada: reestructura y agrega.
> Regla: sin modificar codigo en esta fase. El agent builder audita y rellena resultados.
> Fecha: 2026-09-18.

## 0. Principio rector (acordado)

1. Nada se reemplaza: se reestructura y se agrega.
2. CPU, GPU y RAM tienen funciones diferentes; el orden elegido debe ser el que mas rendimiento da.
3. La RAM esta casi olvidada en el motor actual: se propone usarla como cache persistente + staging, manteniendo como beneficio que el uso de RAM siga siendo bajo.
4. El modulo de rasterizacion de la GPU hoy no se aprovecha: se propone usarlo para traduccion + calculos del motor logico.
5. El motor logico contiene el exe del lenguaje hermetico (fase de traduccion), usa toda la GPU para simplificar problemas matematicos, luego poda de codigo, luego JIT, y cierra su ciclo entregando SPIR-V para mostrar la imagen y completar el ciclo.

## 1. Particion de responsabilidades

### 1.1 CPU (host) — control, API, determinismo, orquestacion

- Parsing `.herm` -> AST (libherm actual, C++ maduro, branching irregular).
- Analisis semantico, type checking, generacion de RIH.
- Scene graph, BVH build/update (pointer chasing).
- Systems v1 tick determinista (delta_W = 1/60, bajo paralelismo).
- Vulkan API: pipeline creation, memory management, submit, sync.
- ImGui, Input, Window management.
- RAM caches: SPIR-V modules, JIT functions, IR optimizado, undo history.
- Orquestacion: decide QUE va a GPU cada frame (mascaras de poda, dirty sets).

### 1.2 GPU (device) — throughput masivo, rasterizacion, compute

- Optimizacion de RIH en compute (constant folding, DCE, algebraic simplify).
- Bytecode -> SPIR-V por nodo (1 thread por nodo).
- Poda dinamica por frame (lee tensor_delta, frustum, BVH -> PruneMask).
- Traduccion asistida por rasterizacion: tessellation + fragment + atomics para simplificar evaluaciones matematicas masivas (patron inspirado en paper SIGGRAPH 26 de SDF vectorial por strokes; ver seccion 7).
- Ray marching final con SPIR-V inlinado (sin VM stack-based), normal analitica, BrickMap en GPU.
- Readback minimo on-demand: tensor_staging[2] + output_staging.

### 1.3 RAM (system memory) — cache persistente + staging, uso bajo

- Persistent SPIR-V cache (disco + memoria): key = hash(bytecode + config).
- JIT x86-64 cache (asmjit) como fallback para CPU path.
- Optimized IR cache: RIH post-optimizer reutilizable.
- Scene graph + BVH master en CPU, mirror a GPU via staging.
- Frame history: tensor_snapshot de N frames para debug/determinismo.
- Streaming buffer: cambios `.herm` -> parse incremental.
- Restriccion: el uso de RAM debe seguir siendo bajo; medir pico (MB) en cada fase.

## 2. Ciclo completo propuesto (orden a discutir con pinzas)

```text
.herm source (RAM streaming buffer)
  |
  v (CPU) PARSING + SEMANTICA [libherm existente, sin cambios funcionales]
AST -> RIH (JSON/IR plano en RAM)
  |
  v (CPU->GPU upload via staging) RIH plano a SSBO
GPU: MOTOR LOGICO — FASE A: TRADUCCION ASISTIDA POR RASTERIZACION
  - Tessellation/fragment/compute evaluan expresiones y simplifican matematica masiva
  - Exe hermetico corre esta fase en GPU (port parcial: solo evaluacion/expansion)
  - Output: IR expandido + evaluado en VRAM
  |
  v (GPU) MOTOR LOGICO — FASE B: PODA DE CODIGO (compute masivo)
  - DCE, constant folding, algebraic simplify, opcode reorder, dedup
  - Output: bytecode_pool optimizado + PruneMask estatica
  |
  v (GPU) MOTOR LOGICO — FASE C: JIT -> SPIR-V
  - 1 thread por nodo genera modulo SPIR-V
  - Cache en VRAM + write-through a RAM (persistent cache)
  - Output: SPIR-V listo
  |
  v (CPU) vkCreateComputePipelines (Vulkan API exige host)
GPU: RENDER (ray_march con funciones inlinadas, sin VM)
  |
  v (GPU->RAM on-demand) tensor_staging[2] + output_staging
CPU: TensorInspector, Anomaly Gate capa (b), SHA-256 determinismo, Console
```

### 2.1 Puntos a decidir con pinzas (orden por rendimiento, no por pureza)

| # | Decision abierta | Opcion CPU | Opcion GPU | Criterio de eleccion |
|---|------------------|------------|------------|----------------------|
| D1 | Donde se evalua constant folding de expresiones | herm_bridge.cpp (serial) | ri_optimizer.comp (masivo) | Medir: #expresiones tipicas por escena; si >1K, GPU gana |
| D2 | Donde se genera SPIR-V | glsl_gen.cpp + shaderc offline | spirv_gen.comp en GPU | Medir: nodos por escena; si >64, GPU gana |
| D3 | Donde vive el BVH master | scene_graph.h + scene_query.h (CPU) | LBVH en GPU con mirror | Mantener master CPU; GPU solo traversal + refit |
| D4 | Parsing completo en GPU | No (5-10% del tiempo) | Solo si live-coding masivo lo justifica | Profiling antes de portar |
| D5 | JIT x86-64 (asmjit) | Mantener como fallback CPU path | No portar a GPU | CPU path lo necesita; GPU genera SPIR-V, no x86 |
| D6 | Poda dinamica | Scheduler CPU (mascaras) | dynamic_pruner.comp (compute) | Hibrido: GPU calcula, CPU orquesta |
| D7 | Uso de RAM | Cache persistente + staging + history | - | Techo: pico RAM < 512 MB en escena catedral |

## 3. Interfaces concretas (contratos para el builder, sin implementar aun)

### 3.1 SSBO layouts propuestos

```cpp
// GPU input: RIH plano (Fase A)
struct GpuRihNode {
  uint32_t sdf_type;      // 0..17 primitivas, 18..21 booleanas
  uint32_t material_id;
  float    transform[16]; // row-major
  uint32_t bytecode_offset;
  uint32_t bytecode_len;
  float    bbox_min[3];
  float    bbox_max[3];
  uint32_t flags;         // bit0: dynamic, bit1: dirty, bit2: culled
};

// GPU output Fase B: bytecode optimizado + mascara estatica
struct GpuBytecodePool {
  uint32_t opcodes[];     // ops 0..34, reordenados por coherencia
  float    params[];      // pool de parametros separado
};

// GPU output poda dinamica (Fase runtime)
struct GpuPruneMasks {
  uint64_t freeze_bits[1024]; // 65536 slots / 64
  uint64_t cull_bits[1024];
};
```

### 3.2 Compute shader signatures propuestas

```glsl
// ri_optimizer.comp — 1 thread por nodo + 1 thread por expresion
layout(local_size_x = 256) in;
layout(set = 0, binding = 0) readonly buffer RihIn { GpuRihNode nodes[]; };
layout(set = 0, binding = 1) writeonly buffer RihOut { GpuRihNode opt_nodes[]; };
layout(set = 0, binding = 2) buffer BytecodePool { uint opcodes[]; };
// Reglas: fold literales, elimina scale==0 / volumen nulo, reordena opcodes.

// spirv_gen.comp — 1 thread por nodo vivo
layout(local_size_x = 64) in;
layout(set = 0, binding = 0) readonly buffer BytecodeIn { uint opcodes[]; };
layout(set = 0, binding = 1) writeonly buffer SpirvOut { uint spirv_words[]; };
// Cache: key = hash(bytecode_nodo + config); hit -> copia de VRAM cache.

// dynamic_pruner.comp — 1 thread por slot tensor
layout(local_size_x = 256) in;
layout(set = 0, binding = 0) readonly buffer Snap { float snapshot[]; };
layout(set = 0, binding = 1) readonly buffer Delta { float delta[]; };
layout(set = 0, binding = 2) buffer Masks { uint64_t freeze_bits[]; uint64_t cull_bits[]; };
// Regla: |delta| < eps -> FREEZE; fuera de frustum + !dynamic -> CULL.
```

### 3.3 RAM cache formats propuestos

```text
~/.cache/motor-grafico/
  spirv/<sha256(bytecode+config)>.spv   # modulo SPIR-V por nodo o escena
  ir/<sha256(rih_source)>.rih.opt       # RIH optimizado serializado
  jit/<sha256(bytecode)>.jit            # funcion asmjit serializada (fallback CPU)
  meta/<hash>.json                      # {nodos, timestamp, hit_count, bytes}
```

## 4. Fases de implementacion (Sprints propuestos, orden por rendimiento)

| Fase | Alcance | Criterio de entrada | Criterio de salida (DoD) |
|------|---------|---------------------|--------------------------|
| F0 | Estabilizar actual: color expr, W=W_frame, slot 0, readback, Anomaly Gate (a) | Spec v2.4 gaps listos | Baseline medible: catedral compila, render CPU+GPU sin basura de plumbing |
| F1 | GPU RIH Optimizer (ri_optimizer.comp) + wiring herm_bridge | F0 done | Bytecode_pool -30% a -50% ops vs baseline; sin regresion visual |
| F2 | RAM persistent caches (spirv/ir/jit + meta) | F0 done (paralelo a F1) | Recompilacion incremental: cambio 1 nodo -> reuso >=90% |
| F3 | GPU Bytecode->SPIR-V Gen (spirv_gen.comp) + vkCreateComputePipelines | F1 done | Shader por escena generado en GPU; sin VM stack-based por defecto (fallback se conserva) |
| F4 | Dynamic Pruner GPU (dynamic_pruner.comp) + Scheduler wiring | F1+F3 done | Frames estaticos: nodos freeze >=Y%; FPS no regresa |
| F5 | Render limpio: normal analitica, BrickMap GPU, d_min real por AABB | F3 done | 4x normal-eval eliminado; sphere tracing exponencial restaurado |
| F6 | Solo si profiling lo pide: parsing GPU / LBVH GPU | F1-F5 medidas | Speedup probado >20% en fase portada, o se descarta |

## 5. Resultados esperados — planillas (EN BLANCO: las rellena el agent builder con auditoria)

### 5.1 Planilla P0 — Baseline actual (medir ANTES de tocar nada)

> **C4 (condicion):** P0 se mide en ambas escenas: catedral (benchmark historico) + optimizer_test (escena nueva).

#### P0 Raw — Pre-fix (capturar antes de cualquier cambio en F0)

| Metrica | Escena | Valor medido | Unidad | Notas / comando usado |
|---------|--------|--------------|--------|-----------------------|
| FPS GPU path | catedral_hermetica (30 nodos, 974x617) | _pendiente_ | fps | |
| ms/frame GPU path | catedral_hermetica | _pendiente_ | ms | |
| FPS CPU path | catedral_hermetica | _pendiente_ | fps | |
| ms/frame CPU path | catedral_hermetica | _pendiente_ | ms | |
| # ops bytecode total | catedral_hermetica | _pendiente_ | ops | contar via test_dump_scene |
| # nodos vivos / totales | catedral_hermetica | _pendiente_ | nodos | |
| VRAM pico | catedral_hermetica | _pendiente_ | MB | |
| RAM pico (proceso visor) | catedral_hermetica | _pendiente_ | MB | debe seguir bajo |
| Tiempo compile .herm->RIH | catedral_hermetica | _pendiente_ | ms | fase CPU parsing |
| Tiempo RIH->bytecode | catedral_hermetica | _pendiente_ | ms | |
| Tiempo JIT total (todos los nodos) | catedral_hermetica | _pendiente_ | ms | |
| Tiempo upload SSBO | catedral_hermetica | _pendiente_ | ms | staging -> device |
| Tiempo dispatch + present | catedral_hermetica | _pendiente_ | ms | solo GPU exec |
| SHA-256 framebuffer estable 60 ticks | escena test minima | _pendiente_ | si/no | determinismo |
| FPS GPU path | optimizer_test (5-10 nodos) | _pendiente_ | fps | escena nueva F1 |
| ms/frame GPU path | optimizer_test | _pendiente_ | ms | |
| # ops bytecode total | optimizer_test | _pendiente_ | ops | |

#### P0 Estabilizada — Post-fix (capturar al cerrar F0, despues de todos los fixes)

| Metrica | Escena | Valor medido | Unidad | Delta vs Raw | Notas |
|---------|--------|--------------|--------|--------------|-------|
| FPS GPU path | catedral_hermetica | _pendiente_ | fps | | |
| ms/frame GPU path | catedral_hermetica | _pendiente_ | ms | | |
| FPS GPU path | optimizer_test | _pendiente_ | fps | | |
| SHA-256 framebuffer estable 60 ticks | escena test minima | _pendiente_ | si/no | | determinismo |

### 5.2 Planilla P1 — Tras F1 (GPU RIH Optimizer)

| Metrica | Esperado (hipotesis) | Medido (builder) | Delta % | Pasa (si/no) |
|---------|----------------------|------------------|---------|--------------|
| # ops bytecode vs P0 | -30% a -50% | _pendiente_ | | |
| ms/frame GPU vs P0 | mejora | _pendiente_ | | |
| ms/frame CPU vs P0 | mejora | _pendiente_ | | |
| Fidelidad visual vs P0 (diff pixeles) | 0 diff o <0.1% | _pendiente_ | | |
| RAM pico vs P0 | sin aumento significativo | _pendiente_ | | |

### 5.3 Planilla P2 — Tras F2 (RAM caches)

| Metrica | Esperado (hipotesis) | Medido (builder) | Delta % | Pasa (si/no) |
|---------|----------------------|------------------|---------|--------------|
| Recompila 1 nodo: tiempo vs full | reuso >=90% | _pendiente_ | | |
| Hit rate SPIR-V cache (2da corrida) | >=90% | _pendiente_ | | |
| Hit rate IR cache (2da corrida) | >=90% | _pendiente_ | | |
| RAM pico vs P0 | < 512 MB catedral | _pendiente_ | | |
| Disco cache total | < 200 MB | _pendiente_ | | |

### 5.4 Planilla P3 — Tras F3 (SPIR-V Gen en GPU)

| Metrica | Esperado (hipotesis) | Medido (builder) | Delta % | Pasa (si/no) |
|---------|----------------------|------------------|---------|--------------|
| Tiempo gen SPIR-V vs JIT x86 total | menor | _pendiente_ | | |
| Warp divergence (contador profiler) | reduce | _pendiente_ | | Nsight / RenderDoc |
| ms/frame GPU vs P1 | mejora | _pendiente_ | | |
| VM stack-based aun presente | no | _pendiente_ | | si/no |

### 5.5 Planilla P4 — Tras F4 (Dynamic Pruner)

| Metrica | Esperado (hipotesis) | Medido (builder) | Delta % | Pasa (si/no) |
|---------|----------------------|------------------|---------|--------------|
| Nodos freeze en escena estatica | >=Y% (definir Y tras P0) | _pendiente_ | | |
| ms/frame escena estatica vs P3 | mejora o igual | _pendiente_ | | |
| ms/frame escena dinamica vs P3 | sin regresion | _pendiente_ | | |
| Correctitud (freeze no rompe anim) | 0 regresiones | _pendiente_ | | |

### 5.6 Planilla P5 — Tras F5 (render limpio)

| Metrica | Esperado (hipotesis) | Medido (builder) | Delta % | Pasa (si/no) |
|---------|----------------------|------------------|---------|--------------|
| Evals SDF por pixel hit (promedio) | 1 (vs 5 actuales) | _pendiente_ | | normal analitica |
| Iteraciones sphere tracing (promedio) | reduce (d_min real) | _pendiente_ | | |
| ms/frame GPU vs P4 | mejora | _pendiente_ | | |
| FPS GPU catedral 974x617 | objetivo: >30 | _pendiente_ | fps | |

## 6. Lo obtenido (EN BLANCO: auditoria del agent builder)

> El builder rellena aqui lo que encuentre al auditar cada fase. No borrar las planillas de seccion 5; copiar el valor medido y agregar evidencia.

### 6.1 Hallazgos de auditoria P0 (baseline)

| Archivo auditado | Lineas aprox | Lo encontrado (resumen) | Evidencia (ruta:linea) |
|------------------|--------------|-------------------------|------------------------|
| _pendiente_ | | | |
| _pendiente_ | | | |

### 6.2 Decisiones D1-D7 resueltas con datos

| Decision | Datos medidos | Resolucion (CPU/GPU/hibrido) | Justificacion |
|----------|---------------|------------------------------|---------------|
| D1 | _pendiente_ | | |
| D2 | _pendiente_ | | |
| D3 | _pendiente_ | | |
| D4 | _pendiente_ | | |
| D5 | _pendiente_ | | |
| D6 | _pendiente_ | | |
| D7 | _pendiente_ | | |

### 6.3 Riesgos confirmados / descartados

| Riesgo | Estado (confirmado/descartado/pendiente) | Evidencia |
|--------|------------------------------------------|-----------|
| Warp divergence por VM stack-based | _pendiente_ | |
| 5x evals por normal finite-diff | _pendiente_ | |
| d_min=0.01f rompe sphere tracing | _pendiente_ | |
| renderer.time no determinista | _pendiente_ | |
| Color expr -> blanco | _pendiente_ | |
| Slot 0 no reservado | _pendiente_ | |
| Readback ausente | _pendiente_ | |
| RAM pico excesivo | _pendiente_ | |

## 7. Referencia paper SIGGRAPH 26 (contexto, no copia)

- Paper: "Real-Time GPU Vector Graphics SDF Generation Based on Quadratic Stroke Rendering" (SIGGRAPH Conference Papers '26, Art. 121, pp. 1-10, DOI 10.1145/3799902.3811177).
- Idea reutilizable como PATRON (no como codigo): pipeline GPU-residente con tessellation -> fragment -> atomics (imageAtomicMin sobre textura uint con encoding ordenado por |d|) para evaluaciones masivas paralelas + sign resolution local en joints.
- Mapeo propuesto a nuestro motor logico: stroke = nodo SDF / curva; joint = operacion booleana / discontinuidad; atomicMin = reduccion de candidatos (mejor distancia / mejor camino de optimizacion); regeneracion por frame = traduccion incremental de nodos dirty.
- Diferencia clave: el paper genera SDF 2D de vector graphics; nosotros lo adaptamos como patron para TRADUCCION + SIMPLIFICACION matematica masiva dentro del motor logico (Fase A), antes de poda/JIT/SPIR-V.

---

## 8. Plan de ejecución detallado (aprobado con condiciones C1-C5)

> Condiciones obligatorias del agent planner:
> - **C1**: Cada claim "YA existe" debe citarse en §6.1 con ruta:línea exacta + hash de commit. Sin evidencia, el fix se implementa igual.
> - **C2**: F0.2 no puede ser "solo vigilar". Test obligatorio de W en pasos 1/60.
> - **C3**: P0 se mide en F0, no en F1. Captura P0 raw antes de fixes + P0 estabilizada al cerrar F0.
> - **C4**: P0 en ambas escenas: catedral + optimizer_test.
> - **C5**: No borrar ray_march.comp ni glsl_gen.cpp. Solo dejar de usar por defecto tras flag/build option.

### 8.1 Orden de ejecución

```
F0 (estabilizar) ──┬── F1 (optimizer) ──┬── F3 (SPIR-V gen) ──┬── F5 (render limpio)
                   │                    │                      │
                   └── F2 (caches) ─────┘── F4 (pruner) ──────┘
                      (paralelo a F1)
```

### 8.2 FASE F0 — Estabilizar actual (5 fixes)

**Criterio de entrada**: Spec v2.4 gaps listos
**Criterio de salida**: Baseline medible: catedral compila, render CPU+GPU sin basura de plumbing

#### F0.1: Color expr fix
- **Archivo**: `core/herm_bridge.cpp` L586-598
- **Bug**: `evalExprConst(m.tensor[4..7])` retorna 0.0f cuando `is_expr=true`
- **Fix**: Cuando `is_expr=true`, parsear la expression string y evaluar si es literal numérico. Si es literal → usar valor. Si es expresión dinámica → dejar 0.0f (v2)
- **Test**: Extender `tests/test_render.cpp` — Material con tensor[4] literal = color correcto, tensor[4] expression = fallback 0.0f
- **Auditoría**: Verificar que el color renderiza correctamente

#### F0.2: W=W_frame test obligatorio (C2)
- **Estado**: Código actual es correcto (`fixed_ts.currentW()` en `visor/visor_app.cpp:349`)
- **Fix**: Ninguno (código correcto)
- **Test OBLIGATORIO**: Agregar test en `tests/test_render.cpp` — W avanza en pasos exactos de 1/60, sin drift
- **Auditoría**: Test pasa, sin regressiones futuras

#### F0.3: Slot 0 header fix
- **Archivo**: `core/herm_bridge.cpp` L704-711
- **Bug**: `hdr.tensor_buffer_size` nunca se setea (queda en 0 al serializar .ont)
- **Fix**: Agregar `hdr.tensor_buffer_size = (node_count + 1) * 8 * sizeof(float);`
- **Test**: Extender `tests/test_scene.cpp` — OntHeader serializado tiene tensor_buffer_size correcto
- **Auditoría**: Verificar que el .ont serializado es consistente

#### F0.4: Readback mejoras
- **Archivos**: `render/vulkan_pipeline.cpp`, `render/vulkan_core.cpp`
- **Mejoras**:
  - SHA-256 cada 60 ticks para determinismo
  - On-demand throttle (no cada frame)
  - Indicador "Frame N-1" en TensorInspector
- **Test**: Extender `tests/test_render.cpp` — SHA-256 de framebuffer estable
- **Auditoría**: Verificar que el readback funciona y el SHA-256 es estable

#### F0.5: Anomaly Gate wire + checks
- **Archivos**: `visor/scheduler.cpp`, `visor/anomaly_gate.h/cpp`
- **Fix**: `Scheduler::validate()` debe llamar a `AnomalyGate::validateAST()`, `validateBytecode()`, `validateTensorSlots()`
- **Checks faltantes**: d_min validation, AABB validation, color validation
- **Test**: Extender `tests/test_scene.cpp` — AnomalyGate detecta nodos degenerados
- **Auditoría**: Verificar que el gate bloquea export con reporte

#### F0: Escenas de test
- **optimizer_test.herm**: Escena con 5-10 nodos, constant folding, dead code, 1 nodo dinámico (dependiente de W), 1 smooth_union (S3)
- **catedral**: Benchmark histórico (30 nodos, 974x617)

#### F0: Tests
- **Archivos**: Extender `tests/test_render.cpp` y `tests/test_scene.cpp`
- **Regresiones**: color literal, W determinista (pasos 1/60), slot 0 tamaño, readback SHA-256, gate capa (a)

### 8.3 FASE F1 — GPU RIH Optimizer

**Criterio de entrada**: F0 done
**Criterio de salida**: Bytecode_pool -30% a -50% ops vs baseline; sin regresión visual

#### F1.1: ri_optimizer.comp
- **Archivo nuevo**: `render/ri_optimizer.comp`
- **Compute shader**: 1 thread por nodo, constant folding, DCE, algebraic simplify
- **SSBO layouts**: Usar `GpuRihNode` del spec §3.1

#### F1.2: Wiring herm_bridge
- **Archivo**: `core/herm_bridge.cpp`
- **Acción**: Después de `convertHermToOntScene`, dispatch `ri_optimizer.comp`
- **Fallback** (S1): Fallo de `vkCreateComputePipelines`, no chequeo de capacidades — compute es universal en Vulkan 1.2

#### F1.3: Baseline measurements (P0)
- **C3**: P0 raw se mide en F0 ANTES de cualquier fix, no en F1
- **F1**: Solo confirma que los fixes de F0 no regresaron performance

#### Tests F1
- **Archivo nuevo**: `tests/test_f1_ri_optimizer.cpp`
- **Registrar en**: `tests/build_test.bat`

### 8.4 FASE F2 — RAM persistent caches (paralelo a F1)

**Criterio de entrada**: F0 done
**Criterio de salida**: Recompilación incremental: cambio 1 nodo → reuso >=90%

#### F2.1-F2.5: Cache structure
- SPIR-V cache, IR cache, JIT cache, Meta cache
- Ruta: `%LOCALAPPDATA%/motor-grafico/cache/`

#### Tests F2
- **Archivo nuevo**: `tests/test_f2_caches.cpp`

### 8.5 FASE F3 — GPU Bytecode→SPIR-V Gen

**Criterio de entrada**: F1 done
**Criterio de salida**: Shader por escena generado en GPU; sin VM stack-based por defecto

#### F3.1: spirv_gen.comp
- **Archivo nuevo**: `render/spirv_gen.comp`

#### F3.2: vkCreateComputePipelines wiring
- **glsl_gen.cpp se conserva como fallback explícito** (C5)

#### F3.3: VM como fallback
- **ray_march.comp se conserva** (C5)
- **Acción**: Flag/build option para usar VM vs shader generado
- **Default**: shader generado (sin VM)

#### Tests F3
- **Archivo nuevo**: `tests/test_f3_spirv_gen.cpp`

### 8.6 FASE F4 — Dynamic Pruner GPU

**Criterio de entrada**: F1+F3 done
**Criterio de salida**: Frames estáticos: nodos freeze >=Y%; FPS no regresa

#### F4.1: dynamic_pruner.comp
- **Archivo nuevo**: `render/dynamic_pruner.comp`

#### F4.2-F4.3: Scheduler wiring + Render integration

#### Tests F4
- **Archivo nuevo**: `tests/test_f4_pruner.cpp`

### 8.7 FASE F5 — Render limpio

**Criterio de entrada**: F3 done
**Criterio de salida**: 4x normal-eval eliminado; sphere tracing exponencial restaurado

#### F5.1: Normal analítica
- Reemplazar finite differences (4 evals) con normal analítica (1 eval)

#### F5.2: BrickMap GPU
- Migrar BrickMap de CPU a GPU

#### F5.3: d_min real por AABB
- **S2**: Dos cambios coordinados — compiler fix con sampling 8³ + defensivo en motor
- Citar `implementation_plan.md`

#### Tests F5
- **Archivo nuevo**: `tests/test_f5_render.cpp` (S3: no extender el de F3)

### 8.8 FASE F6 — Solo si profiling lo pide

- **Criterio**: F1-F5 medidas, speedup >20% probado
- **Por ahora**: No planificar implementación

### 8.9 Decisiones D1-D7

| Decisión | Estado | Resolución |
|----------|--------|------------|
| D1 (folding CPU vs GPU) | **Medir en F0, decidir en F1** | Pendiente datos |
| D2 (SPIR-V CPU vs GPU) | **Medir en F1, decidir en F3** | Pendiente datos |
| D3 (BVH master en CPU) | **Ya decidido** | CPU master, GPU solo traversal + refit |
| D4 (parsing GPU) | **Ya decidido** | No portar salvo speedup >20% probado |
| D5 (JIT asmjit) | **Ya decidido** | Mantener como fallback CPU path |
| D6 (pruner CPU vs GPU) | **Medir en F3, decidir en F4** | Pendiente datos |
| D7 (RAM <512MB) | **Ya decidido** | Techo: pico RAM < 512MB |

---

*Fin del spec v1.0 con plan de ejecución. Siguiente paso: ejecutar F0.*
