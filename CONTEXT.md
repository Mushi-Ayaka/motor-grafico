# Contexto Motor Grafico — 2026-09-20

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

## Fases Pendientes

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
- `tests/build_f0_test.bat` — F0 tests
- `tests/build_f1_test.bat` — F1 tests
- `tests/build_f3_test.bat` — F3 tests
- Shaders compiled with: `glslc.exe render/*.comp -o build/shaders/*.spv`

## Deuda Tecnica

1. **ray_march.comp evalLeaf() es stub** — retorna 1e9 siempre. El VM path no esta implementado en GPU. El specialized path (F3) genera shaders inline pero no esta wired al visor ainda.
2. **Material opacity no soportado en herm** — el parser descarta el campo `opacity: X`. Siempre es 1.0.
3. **OntGraphNode.flags no existe** — se usa `pad[0]` como flags byte (bit2=culled). Considerar agregar campo real.
4. **glslc.exe como subprocess** — dependencia externa para compilar GLSL→SPIR-V. Podria moverse a runtime compilation con shaderc library.
5. **F4/F5 no implementados** — frustum culling y analytic normals son prioridad para performance.
6. **spirv_gen.comp solo valida y compacta** — la generacion de GLSL completa es CPU-side (SpirvGen::generate). La generacion pura de SPIR-V en GPU queda como aspiracion.
