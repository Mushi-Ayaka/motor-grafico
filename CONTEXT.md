# Contexto Motor Grafico — 2026-10-14

## Estado Actual

Motor grafico SDF con lenguaje de programacion propio (Hermetico/Herm), rendering via Vulkan.
Arquitectura: herm source → parse → AST → RIH bytecode → GPU compute shaders → pixel output.

## Pipeline GPU Actual

```
herm source → [CPU] compile → RIH bytecode
  → [GPU F1] ri_optimizer.comp (constant folding + algebraic simplify + dead code detection)
  → [GPU F3] spirv_gen.comp (compact bytecode, cull dead nodes, prepare for shader gen)
  → [CPU F3] SpirvGen::generate() (bytecode→GLSL, dedup, assemble specialized shader)
  → [CPU] glslc.exe → SPIR-V → vkCreateComputePipelines
  → [GPU] ray_march.comp (BVH traversal + inline SDF eval + PBR shading)
```

## Fases Completadas

### F0: Estabilizacion (5 fixes)
- **F0.1**: Color expr fix — literals vs expressions now handled correctly
- **F0.2**: FixedTimestep — W = tick_count * DT, no drift, deterministic
- **F0.3**: Slot 0 header fix — tensor_buffer_size = (N+1)*8*4
- Tests: `test_f0.cpp` (24/24 pass)

### F1: GPU RIH Optimizer
- `render/ri_optimizer.comp` — compute shader, 1 thread/node, constant folding + dead node detection
- `render/ri_optimizer.h/cpp` — CPU implementation: `optimizeBytecode()`, `optimizeOntScene()`
- Wired into `compileHermToOntScene()` — runs automatically on compilation
- Tests: `test_f1.cpp` (10/10 pass)
- Shader: `build/shaders/ri_optimizer.spv` (17KB)

### F2: RAM Persistent Caches
- `render/ram_cache.h/cpp` — `RamCache<K,V>` template, LRU eviction, thread-safe
- `build/` dir cache, `shader_cache` for compiled SPIR-V
- Tests: 21 tests pass

### F3: GPU SDF Shader Generation
- `render/spirv_gen.comp` — GPU compute shader: validates bytecode, marks culled nodes, compacts live bytecode into output SSBO
- `render/spirv_gen.h/cpp` — CPU assembly path: `SpirvGen::generate()` produces specialized GLSL with inline SDF functions (no VM)
  - Bytecode deduplication: identical SDFs share functions
  - Skips culled nodes (bit2 in pad[0])
- Tests: `test_f3.cpp` (16/16 pass)
- Shader: `build/shaders/spirv_gen.spv`

### F6: Escenas benchmark + stats (2026-10-09)
- **5 escenas `.herm`** en `escenas/` (dificultad creciente, todas en un cuarto):
  01_bodega_baja (480×270, 4 nodos), 02_sala (640×360, 10), 03_galeria (854×480, 19),
  04_museo (1024×576, 28), 05_catedral (1280×720, 35). Resolución de la escena (`axes`) guía el render.
- **`--stats` en visor**: `stats/runs.csv` (append, una fila por run con media/mediana/desv/p95/p99/cv/fps),
  `stats/run_<ts>_<escena>.csv` (por frame) y screenshot `stats/shot_<ts>_<escena>.bmp`.
  Sleep desactivado con --stats; timing por QPC (`render_ms_qpc`). Resolución de escritura: población (÷N).
- **`.bat` de doble clic** (raíz): `correr_01...05.bat` (interactivo con stats al cerrar) y `benchmark_las_5.bat` (auto ~10s).
- **Present mode MAILBOX > IMMEDIATE > FIFO** (FIFO capaba todo a 16.7ms).
- Ladder medido (RTX 3060, render_ms media): 01=1.14, 02=1.55, 03=2.67, 04=3.78, 05=7.70 ms.

### Fixes críticos 2026-10-09 (bugs de formato/orden)
1. **`resolveSdfTree` violaba contrato rih.h** ("index 0 = root"): pushea hijos primero, raíz al final →
   todos los consumidores (`herm_ont` bytecode/AABB/d_min, `herm_render`, `herm_bridge`, F3) compilaban
   **el primer hijo** como raíz → `subtract` perdía el hijo B → habitaciones eran bloques sólidos (gris plano).
   **Fix**: self-first push en `lenguaje-hermetico/herm/herm_resolver.cpp` (slot reservado, se rellena al final).
2. **`tensor[7]` (alpha) en 0 si no hay `opacity:`** → `optimizeOntScene` (F1) cull-eaba todos los nodos
   (`pad[0]|=0x4`) en el path in-memory. **Fix**: default `1.0` en `resolveMaterial`.
3. **`herm_bridge` emitía bytecode compacto** (op + payload solo en CONST) pero TODO el consumidor
   (execBcRaw, glsl_gen, spirv_gen, ri_optimizer `/5`) espera **5 bytes fijos por instrucción**.
   **Fix**: `BytecodeBuilder::emit` emite op + 4 bytes de padding. Formato canónico = 5B/instrucción.
4. Tests: F0 24/24, F1 10/10, F3 16/16 (tras fixes). `contrato/ont.h` sincronizado con layout del motor
   (fix crash 0xC0000005 en loadOnt: tensor_buffer_size + tensor_slot). Parser acepta `type: directional`.
5. Tool de debug: `tools/dump_glsl.cpp` (+`build/dump_glsl.exe`) — vuelca bytecode y GLSL especializado de un `.ont`.

## Fases Pendientes

### Track A: Madurez del pipeline Vulkan (✅ COMPLETO 2026-10-11)
Ver `docs/19-pipeline-madurez-optimizacion.md` para detalles completos.
- **A1**: GPU timestamps (`VkQueryPool` + `vkCmdWriteTimestamp`) → columna `gpu_ms` en `runs.csv`
- **A2**: Stats honestas (`frames_render`/`frames_skip`) + fix fence-reset-before-acquire + caller maneja `return false`
- **A3**: Frames in flight = 2 + UBO ring (dynamic offset, `UBO_STRIDE=128`) + imgui fence per-frame
- **A4**: stderr en `.bat` + nvidia-smi polling 1Hz en `--stats`
- **A5**: Destrucción diferida en resize (PENDIENTE)

**Ladder post-Track-A (gpu_ms mediana)**: 01=0.238, 02=0.817, 03=2.420, 04=3.609, 05=7.573 ms.
GPU util 2-9%, power 28-67W (TGP 115W). GPU-bound en 04/05, CPU-bound en 01/02.

### UI ImGui: Fix overlay + sistema de ventanas (✅ 2026-10-14, v0.35-beta)
Ver `docs/20-ui-ventanas-layout.md` para detalles completos.
- **UI.1/2**: Fix UI invisible — `initImGui` destruía la font texture prematuramente (`TexID=0`); agregado `ImGui_ImplVulkan_NewFrame()`
- **UI.3**: Fix assert `Missing EndChild` — `ImGui::End()` dentro de `if (Begin())` en Viewport/Properties/Editor
- **UI.4**: Hotkey **F2** = oculta overlay ImGui (barrera → PRESENT_SRC directo, video limpio)
- **UI.5**: Menú **Window** con checkboxes (10 ventanas; Export=stub deshabilitado)
- **UI.6**: **Layout default dockeado** (DockBuilder): Editor .herm izq 25%, Properties der 25%, Console abajo 28%, Viewport centro. Persistido en `imgui.ini`
- **UI.7**: Paneles fuera del layout nacen ocultos (`visible=false` en headers)

### F4: Dynamic Pruner GPU
- Compute shader para frustum/occlusion culling en GPU
- Reemplazar CPU-side BVH culling
- Archivo: `render/dynamic_pruner.comp`

### F5: Render Limpio
- Normal analitica (reemplazar finite differences)
- BrickMap GPU (sparse voxel octree para SDF caching)
- d_min real en BVH (reemplazar bounding sphere approx)

## Archivos Clave

| Archivo | Funcion |
|---------|---------|
| `render/scene.h` | OntScene, OntGraphNode, OntOpcode, OntMaterial structs |
| `render/ri_optimizer.h/cpp` | F1: RIH bytecode optimizer |
| `render/ri_optimizer.comp` | F1: GPU compute shader |
| `render/spirv_gen.h/cpp` | F3: SDF shader generator (CPU) |
| `render/spirv_gen.comp` | F3: GPU bytecode compaction |
| `render/glsl_gen.h/cpp` | GLSL shader assembly (CPU reference) |
| `render/ray_march.comp` | Main ray march compute shader (fallback VM path) |
| `render/bytecode_vm.h` | CPU bytecode executor (execBcRaw) |
| `render/vulkan_pipeline.cpp` | Pipeline creation, createPipelineFromSpv() |
| `core/herm_bridge.h/cpp` | Herm→OntScene conversion, compileHermToOntScene() |
| `render/ram_cache.h` | F2: Persistent cache template |

## Build System

- `build.bat` — builds visor.exe + test_ont_bridge.exe
- `build_libherm.bat` — herm sources → `build/libherm.lib` (rebuild obligatorio tras tocar `lenguaje-hermetico/herm/*.cpp` o `contrato/*`)
- `..\lenguaje-hermetico\herm\build.bat` — herm.exe → `..\lenguaje-hermetico\build\herm.exe` (la copia en `herm\herm.exe` es STALE; siempre la de `build\`)
- `tests/build_f0_test.bat` — F0 tests (build; el .exe va en `build/`)
- `tests/build_f1_test.bat` — F1 tests
- `tests/build_f3_test.bat` — F3 tests
- Shaders compiled with: `glslc.exe render/*.comp -o build/shaders/*.spv`
- Compilar escena: `..\lenguaje-hermetico\build\herm.exe "escenas\X.herm" --ont`

## Deuda Tecnica

1. **ray_march.comp evalLeaf() es stub** — retorna 1e9 siempre. El VM path no esta implementado en GPU. El specialized path (F3) genera shaders inline pero no esta wired al visor ainda.
2. **Material opacity**: parser/soporte `opacity: X` + default 1.0 (fix 2026-10-09). El shading GPU aún usa el fijo `ldir` — las luces `type: directional` de la escena no afectan el render.
3. **OntGraphNode.flags no existe** — se usa `pad[0]` como flags byte (bit2=culled). Considerar agregar campo real.
4. **glslc.exe como subprocess** — dependencia externa para compilar GLSL→SPIR-V. Podria moverse a runtime compilation con shaderc library.
5. **F4/F5 no implementados** — frustum culling y analytic normals son prioridad para performance.
6. **spirv_gen.comp solo valida y compacta** — la generacion de GLSL completa es CPU-side (SpirvGen::generate). La generacion pura de SPIR-V en GPU queda como aspiracion.
