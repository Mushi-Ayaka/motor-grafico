> Documento consolidado a partir de evidencia verificada en Fases 0–3.
> Fuentes: `motor-grafico` (repo), `deps/lenguaje-hermetico` (repo hermano), `external/` (dependencias), `README.md`, `build.bat`, `setup_deps.bat`, tests reales.
> Regla aplicada: solo lo que el código dice. Cero interpretación sin respaldo.

---

## 0. Resumen ejecutivo

El proyecto `motor-grafico` es un **runtime de escenas SDF** con dos arquitecturas superpuestas:

**Arquitectura 1 (junio 2026, "sesión única" según README):**
- Ray marching en CPU puro.
- Formato `.rih` (JSON).
- RHI DirectX 11.
- Modelo de datos: `Scene`/`Node`/`Material`.
- Tests en raíz del repo.
- README describe esta arquitectura.

**Arquitectura 2 (estado actual):**
- Ray marching en GPU con Vulkan compute shaders.
- Formato `.ont` (binario) + `.obs`.
- Lenguaje fuente `.herm` (compilado por `deps/lenguaje-hermetico`).
- Modelo de datos: `OntScene`/`OntGraphNode`/`OntMaterial`.
- Editor ImGui con 20 paneles.
- Vulkan en `render/`; D3D11 queda huérfano en `rhi/`.

Ambas coexisten sin marcadores de transición. El README, el build system y las specs no coinciden con el código real en múltiples puntos.

---

## 1. Inventario por módulo

### 1.1 Totales

| Lenguaje | LOC | % |
|---|---|---|
| C++ (`.cpp` + `.h`) | 17,358 | 55.9% |
| GLSL (`.comp`) | 827 | 2.7% |
| Otro (`.rih`, `.md`, `.txt`, `.herm`, `.bat`, `.mmd`, `.ini`, `.ont`, `.obs`) | 12,855 | 41.4% |
| Python | 0 | 0% |
| C puro | 0 | 0% |
| **Total** | **31,040** | 100% |

### 1.2 Módulos

| Módulo | LOC | Estado | Tests |
|---|---|---|---|
| `visor/` | ~4,700 | Activo. `visor_app.cpp` god object 1,416 LOC. | Ninguno |
| `render/` | ~6,400 | Activo. Tres ray marchings (CPU escalar, CPU SIMD, GPU). | `test_render` (no compilable hoy) |
| `scene/` | 1,098 | Activo. Solo headers. | `test_scene` (81/82) |
| `core/` | ~1,500 | Medio activo. 3/5 `.cpp` fuera del build. | `test_f0/f1/f3` (OK) |
| `os/` | 239 | Activo. `Timer` y `Window` solo usados en tests. | `test_os_rhi` (30/31) |
| `rhi/` | 254 | **Huérfano**. D3D11 sin integrar. | Solo en `test_os_rhi` |
| `tools/` | ~430 | Mayormente stale. Solo `test_ont_bridge` en build. | — |
| `tests/` | ~1,100 | 5 suites funcionales + 1 rota. | — |
| `deps/lenguaje-hermetico/` | externo | Activo. Compila `.herm → .ont`. | — |
| `external/` | externo | asmjit, volk, VMA, imgui v1.91.9. | — |

### 1.3 Archivos por módulo

**`visor/` (21 `.cpp`/`.h`, ~4,700 LOC)**:
- `visor_app.cpp` (1,416) + `.h` (122).
- `window_manager.cpp` (503) + `.h` (80).
- `herm_editor.cpp` (320) + `.h` (45).
- `gizmos_panel.cpp` (276) + `.h` (37).
- `anomaly_gate.cpp` (248) + `.h` (27).
- `main.cpp` (269).
- `command_palette.cpp` (134).
- `inspector_panel.cpp` (132).
- `input_controller.cpp` (112).
- `ontology_panel.cpp` (102).
- `tensor_inspector.cpp` (104).
- `source_browser.cpp` (103).
- `viewport_manager.cpp` (92).
- `input_bus.cpp` (11) + `.h` (93).
- `export_dialog.cpp` (77) — stub.
- `scheduler.cpp` (74).
- `console_panel.cpp` (64).
- `scene_manager.cpp` (64).
- `timeline_panel.cpp` (62).
- `undo_redo.cpp` (57) + `.h` (55).
- `profiler_panel.cpp` (41).

**`render/` (21 archivos, ~6,400 LOC)**:
- `.cpp`: `vulkan_core.cpp` (1038), `scene.cpp` (686), `glsl_gen.cpp` (558), `vulkan_pipeline.cpp` (404), `jit_compiler.cpp` (395), `spirv_gen.cpp` (317), `ri_optimizer.cpp` (185), `sdf_eval.cpp` (110), `scratch.cpp` (9).
- `.h`: `ray_march.h` (993), `ray_march_simd.h` (891), `scene.h` (337), `sdf_eval.h` (199), `bytecode_vm.h` (158), `render.h` (128), `vulkan_core.h` (92), `vulkan_pipeline.h` (67), `jit_compiler.h` (29), `glsl_gen.h` (22), `ri_optimizer.h` (26), `spirv_gen.h` (26).
- `.comp`: `ray_march.comp` (184), `ri_optimizer.comp` (128), `spirv_gen.comp` (105).
- `.md`: `ont_format_v1.md` (110), `shader_tensor_spec.md` (96).

**`scene/` (5 headers, 1,098 LOC)**:
- `project.h` (362), `scene_graph.h` (284), `camera.h` (227), `scene_query.h` (117), `workspace.h` (108).

**`core/` (14 archivos, ~1,500 LOC)**:
- Compilados: `herm_bridge.cpp` (720), `systems.cpp` (25).
- **No compilados**: `renderer.cpp` (140), `rih_reader.cpp` (378), `sdf_eval.cpp` (289).
- Headers: `core.h` (100), `herm_bridge.h` (34), `systems.h` (31), `fixed_timestep.h` (24), `renderer.h` (20), `sdf_eval.h` (19), `rih_reader.h` (7).
- Docs: `systems_arch.md` (60), `determinism_spec.md` (82).

**`os/` (5 archivos, 239 LOC)**:
- `os.h` (60), `win32/win32.cpp` (74), `win32/mem.cpp` (45), `win32/file.cpp` (38), `win32/timer.cpp` (22).

**`rhi/` (2 archivos, 254 LOC)**:
- `d3d11.cpp` (211), `rhi.h` (43).

---

## 2. Flujo real

### 2.1 Compilación offline (fuera del motor)

```
.herm ──[herm.exe]──> .rih (JSON) ──[herm.exe]──> .ont (binario)
                                  └─> .obs (metadata)
```

Evidencia:
- `deps/lenguaje-hermetico/herm/main.cpp:32` → `int main(int, char**)` del CLI `herm.exe`.
- `herm_compile.cpp:87` → `compileFromString` (Lexer → Parser → Resolver → Rih).
- `herm_rih.cpp:235` → `writeRihJson`; `:246` → `writeRihBinary`.
- `herm_ont.cpp:1165` → `writeOnt` (escribe `.ont`).
- `herm_obs.cpp:22` → `writeObs`.

**`motor-grafico` no compila `.herm` ni `.ont`. Solo los lee.** El único compilador en proceso es `herm.exe`, binario externo.

### 2.2 Runtime GPU (path principal)

Invocación:
```
visor.exe "escenas\X.ont" --bench-vulkan --stats
```

Trazado:
1. `visor/main.cpp:210` → `WinMain`.
2. `visor_app.cpp:37` → `VisorApp::init`. Llama a `workspace.init()`, `win.init()`, `vk_ctx.init()`, `vk_ctx.initSwapchain()`.
3. `render/scene.cpp:14` → `OntScene::loadOnt(fm)` lee `.ont` desde `FileMapping`.
4. `render/scene.cpp:66` / `:141` → `OntScene::loadObs` + `applyObs` lee `.obs`.
5. `render/render.h:65` → `Renderer::loadOnt` → `VulkanSceneData::init`.
6. `render/vulkan_pipeline.cpp:309` → `initPipeline` carga `ray_march.spv` precompilado.
7. `visor_app.cpp:378` → `VisorApp::run` (bucle).
8. `visor_app.cpp:530` → rama GPU (`renderer.use_vulkan && renderer.ont_mode`).
9. `visor_app.cpp:605` → `VulkanSceneData::updateUBO`.
10. `visor_app.cpp:614` → `drawEditorUI`.
11. `visor_app.cpp:619` → `VulkanContext::drawFrame`:
    - `render/vulkan_core.cpp:643` — acquire swapchain image.
    - `render/vulkan_pipeline.cpp:325` → `recordComputeCommandBuffer` → `vkCmdDispatch((w+7)/8, (h+7)/8, 1)`.
    - `render/vulkan_core.cpp:785` → `vkCmdCopyBufferToImage` (output_buffer → swapchain).
    - `render/vulkan_core.cpp:788` → `copyOutputToViewport` → `viewport_image`.
    - `render/vulkan_core.cpp:791` → `requestReadback` (tensor_buffer → staging).
    - `render/vulkan_core.cpp:846` → `ImGui_ImplVulkan_RenderDrawData`.
    - `render/vulkan_core.cpp:897` → `vkQueuePresentKHR`.

### 2.3 Runtime CPU (bench headless)

Invocación:
```
visor.exe "escenas\X.ont" --bench
```

Trazado:
1. `visor/main.cpp:218` → `benchmark(path)` (guardado con `!bench_vulkan`).
2. `visor/main.cpp:32` → `benchmark()`.
3. `FileMapping` abre el `.ont`.
4. `JitCompiler::compileScene` (`render/jit_compiler.cpp:406`) compila bytecode SDF a código x86 SSE con asmjit.
5. `renderOntSceneMT` o `renderOntSceneMTSIMD` (`render/ray_march.h:875`, `render/ray_march_simd.h:811`).
6. Escribe `benchmark_result.txt`.

### 2.4 Live-compile en editor

```
HermEditor::openFile(path)  (visor/herm_editor.cpp:67)
   ↓
editor_source (string en memoria)
   ↓
mg::compileHermToOntScene(src, out, err)  (core/herm_bridge.cpp:792)
   ↓
herm::compileToRih  (deps/lenguaje-hermetico)
   ↓
mg::convertHermToOntScene  (core/herm_bridge.cpp)
   ↓
mg::optimizeOntScene  (render/ri_optimizer.cpp)
   ↓
OntScene en memoria → mismo path 2.2 desde paso 6
```

### 2.5 Formato `.ont`

**Header** (80 bytes, `pack(1)`, según spec; código con campos extra → 120 bytes reales):

```
magic (4) + version (4) + epsilon (4) + node_count (4) + bvh_count (4) +
material_count (4) + bytecode_size (4) + tensor_buffer_size (4) +
aabb_min[4] (16) + aabb_max[4] (16) + reserved[7] (56)
```

Layout: `OntHeader | OntBvhNode[bvh_count] | OntGraphNode[node_count] | bytecode | OntMaterial[material_count]`.

- `OntBvhNode`: 52 bytes (AABB + d_min + L + skip_index + first_node + node_count + flags).
- `OntGraphNode`: 116 bytes (transform 4x4 + material_id + bytecode_off/len + bbox + tensor_slot + mode).
- `OntMaterial`: 60 bytes (id + color + roughness + metallic + emission + opacity + tensor_slot).

**Discrepancia**: `ont_format_v1.md` dice `OntHeader=80`, `OntGraphNode=100`, `OntMaterial=52`. El código da 120, 116, 60. **Tres de cuatro structs no coinciden**.

### 2.6 Formato `.obs`

`ObsHeader` + secciones opcionales (`OBS_CAMERA`, `OBS_LIGHTS`, `OBS_TIMELINE`, `OBS_BACKGROUND`, `OBS_RESOLUTION`). Sobrescribe campos del `OntScene`. Formato definido en `render/scene.h:211+`.

### 2.7 Shaders

| Archivo | Uso | Compilación |
|---|---|---|
| `render/ray_march.comp` | Fallback genérico | `glslc` en `build.bat:94` → `build/shaders/ray_march.spv` (19520 bytes) |
| `render/spirv_gen.comp` | Generador GPU (F3) | **No compilado por `build.bat`** |
| `render/ri_optimizer.comp` | Optimizador GPU (F1) | **No compilado por `build.bat`** |

Además, `render/glsl_gen.cpp:511` invoca `glslc.exe` en **runtime** vía `CreateProcessA` para generar shaders especializados. Ruta hardcodeada: `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe`. Fallback a `ray_march.spv`.

---

## 3. Dependencias reales

### 3.1 Dependencias declaradas (de `setup_deps.bat`)

| Dep | Versión | Uso en build |
|---|---|---|
| asmjit | HEAD | Sí (`build_asmjit.bat` → `build/asmjit.lib`) |
| volk | HEAD | Sí (`volk.c`) |
| VMA | HEAD | Sí (`external/VulkanMemoryAllocator`) |
| imgui | v1.91.9 | Sí (editor) |
| lenguaje-hermetico | HEAD | Sí (`deps/lenguaje-hermetico`) |
| MLIR-CampoTensorial | HEAD | **No** (clonado, no usado por `build.bat`) |
| Vulkan SDK | 1.4.350.0 | Prereq obligatorio |
| MSVC VS 2022 BuildTools | — | Prereq |

### 3.2 Duplicaciones de dependencias

- `external/vma` (2 archivos, stale) **y** `external/VulkanMemoryAllocator` (273 archivos, activo). El build usa el segundo.

### 3.3 Dependencias silenciosas

- `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe` — invocado en runtime por `render/glsl_gen.cpp:588`.
- `nvidia-smi` — invocado por `VisorApp::pollNvidiaSmi` (`visor_app.cpp:676`).
- Rutas personales hardcodeadas:
  - `visor/main.cpp:280`: `..\..\..\proyectos-index\proyecto de IA artistica\Lenguaje Hermetico\.gitignored\libreria\escenas\catedral_hermetica_0000.ont`.
  - `tools/check_offsets.cpp`, `check_sample.cpp`, `test_dump_scene.cpp`: `C:\Users\Josue B\...`.

### 3.4 Submódulo git roto

`external/asmjit` está indexado como gitlink (`160000`) pero **no existe `.gitmodules`**. `git submodule status` falla. `setup_deps.bat` clona como carpeta normal, chocando con el índice.

---

## 4. Código muerto y duplicaciones

### 4.1 Archivos definidos y no compilados

| Archivo | LOC | Estado |
|---|---|---|
| `core/renderer.cpp` | 140 | No en `build.bat` |
| `core/rih_reader.cpp` | 378 | No en `build.bat` |
| `core/sdf_eval.cpp` | 289 | No en `build.bat` |
| `core/core.h` | 100 | Modelo de datos paralelo, sin consumidor |
| `render/scratch.cpp` | 9 | No en `build.bat` |
| `test_minimo.cpp` | 27 | Entry WinMain suelto, no compilado |
| `test_bvh.cpp` | 51 | No compilado, ruta externa |
| `test_debug.cpp` | 57 | No compilado, ruta externa |
| `test_ont_bmp.cpp` | 89 | No compilado, ruta externa |
| `test_pipeline.cpp` | 28 | No compilado, ruta externa |
| `test_render_catedral.cpp` | 139 | No compilado, ruta externa |
| `test_suelo_test.cpp` | 30 | No compilado, ruta externa |
| `test_write.md` | 1 | Basura |
| `motor grfico.txt` | 70 | Nota suelta |
| `phase6_diag.txt` | 909 | Log de fase 6 |
| `bench2_err.txt`, `bench2.txt`, `output.txt`, `profile.txt` | — | Logs |

### 4.2 Módulos huérfanos

- **`rhi/d3d11.cpp` (211) + `rhi/rhi.h` (43)** — Backend D3D11 completo sin integrar. Solo lo usan `tests/test_os_rhi.cpp`. Nadie en producción.
- **`os::Timer` (22) + `os::Window` (74)** — Solo usados en `tests/test_os_rhi.cpp`. El visor usa `QueryPerformanceCounter` directo y `WindowManager`.

### 4.3 Código no alcanzado

- **`play_mode`** (`visor/input_bus.h:69`) — declarado `= false`, leído en `visor_app.cpp:400`, **nunca asignado a `true`**. Todos los "Systems v1" detrás de este flag son código muerto.
- **`InputBus::onKey/onMouseMove/onMouseButton`** — definidos, **sin llamadas desde `WndProc`**. El `InputSnapshot` llega vacío a `camera_system_tick`.
- **`DEFAULT_SYSTEMS[]`** — array `static const` con un único system (`camera_system_tick`). No hay carga dinámica (`dlopen`/`LoadLibrary`).

### 4.4 Duplicaciones arquitectónicas

- **Tres ray marchings**: `render/ray_march.h` (CPU escalar), `render/ray_march_simd.h` (CPU SIMD+MT), `render/ray_march.comp` (GPU).
- **Dos backends de render**: Vulkan (activo) + D3D11 (huérfano) + CPU JIT (activo en `--bench`).
- **Cuatro modelos de escena**:
  - `core/core.h` — no usado.
  - `render/scene.h` "viejo" (`Scene`, `Node`, `Material`) — usado por `SceneGraph`, `Project`.
  - `render/scene.h` "nuevo" (`OntScene`, `OntGraphNode`, `OntMaterial`) — usado por runtime GPU.
  - `scene/*.h` (`SceneNode`, `SceneGraph`) — usado por editor.
- **Dos BVHs**: `scene::Bvh` (runtime, punteros) y `render::OntBvhNode` (formato `.ont`, stackless).
- **Dos sistemas de cámara**: `scene::CameraController` (editor) y `mg::camera_system_tick` (systems v1, muerto).
- **Dos "W"**: `Timeline::getW() = current_time` y `FixedTimestep::currentW() = tick_count * DT`. Y `Timeline::fps = 30` vs `DT = 1/60`.
- **Dos viewports**: `scene/workspace.h` y `visor/viewport_manager.h`.
- **Dos implementaciones SDF**: `core/sdf_eval.cpp` (no compilado) y `render/sdf_eval.cpp` (compilado).
- **Dos VMA**: `external/vma` y `external/VulkanMemoryAllocator`.
- **Dos "compiladores" de shader**: `glsl_gen.cpp` y `spirv_gen.cpp` (ambos generan GLSL/SPIR-V runtime).

### 4.5 Código de la era anterior

- **`README.md`** describe Arquitectura 1 (CPU ray marching + RIH + D3D11). No menciona Vulkan, `.ont`, `.herm`, `.obs`, `--bench-vulkan`, `--stats`, compute shaders.
- **`test_*.cpp` en raíz** — 6 de 7 usan rutas externas y formato `.rih`. Era anterior.
- **`core/renderer.cpp`, `core/rih_reader.cpp`** — interfaz legacy con `.rih`.

### 4.6 TODOs/FIXMEs

**Concentrados en dos archivos** (el resto del motor no tiene ninguno):

`core/herm_bridge.cpp`:
- Línea 513 — `REPEAT` sin implementar, "passthrough".
- Línea 518 — `TWIST` sin implementar.
- Línea 523 — `ELONGATE` sin implementar.
- Línea 528 — `DISPLACE` sin implementar.

`visor/`:
- `visor_app.cpp:1244` — Export stub.
- `visor_app.cpp:1251` — About TODO.
- `visor_app.cpp:1274` — Camera mode cycle TODO.
- `ontology_panel.cpp:91` — Add node TODO.
- `source_browser.cpp:105` — Create .herm file TODO.
- `export_dialog.cpp:87` — `launch.bat` con `REM TODO: Launch actual game executable`.

### 4.7 Build scripts stale

- `tools/build_visor.bat` — apunta a `C:\Users\Josue B\...\Motor Grafico`.
- `tools/build_script.bat` — archivos inexistentes (`main_win32.cpp`, `brick_map.cpp`).
- `tests/build_benchmark.bat` — linka `build/obj/**/*.obj` que no existen.

---

## 5. Contradicciones con la visión declarada

### 5.1 Contradicciones entre documento y código

| Documento dice | Código dice |
|---|---|
| `.herm → .rih → .ont → GPU` en el motor | El motor solo lee `.ont`. `.herm → .ont` lo hace `herm.exe`. |
| `tensor_buffer` en VRAM, readback, doble buffer | Spec existe (`shader_tensor_spec.md`). Implementación en `render/vulkan_core.cpp`. |
| BVH 4D con AABB + d_min + Lipschitz | `OntBvhNode` tiene `d_min` y `L`. Spec los describe. Sin verificar uso en shader. |
| Systems Python/C con `SystemContext_v1` | Solo C++. `camera_system_tick` único system. Nunca se ejecuta (`play_mode=false`). |
| "Sin strings en runtime" | `glsl_gen.cpp` genera GLSL como strings en runtime. `.mgproj` es JSON. ImGui serializa DockSpace a string. `diag("[PHASE6]")` en producción. |
| "Sin variables globales mutables" | `tensor_buffer` es global mutable. `Arena` es global. `systems.cpp` escribe en `tensor_delta`. |
| "Sin threading" | `ray_march.h` incluye `<thread>`, tiene `renderOntSceneMT`. Autosave usa thread. |
| "Determinismo bit a bit cross-run" | Spec existe (`determinism_spec.md`). `--record`/`--replay` **no se parsean** en `main.cpp`. SHA-256 no implementado. |
| "Sin dependencias externas" | Depende de `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe` y `nvidia-smi` en runtime. |
| "F/Q/B pirámide ontológica" | Solo aparece en `docs/10-contrato-ontologico.md`. No hay código que la implemente. |
| "P/I/K operadores fundamentales" | Solo aparece en docs. No hay código que los implemente. |
| "Universo de bloque 4D" | `W` es un `float` en `FixedTimestep::currentW()`. No hay bloque precompilado. |
| "Editor con 13 módulos" | 21 archivos en `visor/`, mayoría <130 LOC. Varios con TODO. |
| "Exportar juego" | `ExportDialog::runExport` (visor/export_dialog.cpp:59) solo crea directorio + `launch.bat` con `REM TODO`. |
| `ont_format_v1.md`: `OntHeader=80`, `OntGraphNode=100`, `OntMaterial=52` | Código: 120, 116, 60. 3 de 4 structs no coinciden. |
| `README.md`: "219 tests, todos pasando", "CPU ray marching" | Real: `test_scene` 81/82, `test_os_rhi` 30/31. Runtime principal es Vulkan GPU. |
| `systems_arch.md`: SceneGraph con `add/remove/reparent` | `SceneGraph` solo tiene `setParent`. No `add`/`remove`/`reparent`. |
| `systems_arch.md`: `system_tick_v1/v2/v3` con carga por símbolo | Solo `camera_system_tick`. Sin `dlopen`. Sin versionado. |
| `.mgproj` enumera systems | `.mgproj` no tiene campo para enumerar systems. |
| MLIR-CampoTensorial es dependencia | `setup_deps.bat` lo clona; `build.bat` no lo usa. |
| "Reducción de píxeles con MLIR" | Reducción existe (render interno a 480×270 en CSV). No está claro si usa MLIR. |

### 5.2 Contradicciones internas al código

| Fuente A | Fuente B |
|---|---|
| `core/sdf_eval.cpp` | `render/sdf_eval.cpp` |
| `core/renderer.cpp` | `render/render.cpp` (no existe) — `render.h` define `Renderer` |
| `Timeline::fps = 30` | `FixedTimestep::DT = 1/60` |
| `Timeline::getW() = current_time` | `FixedTimestep::currentW() = tick_count * DT` |
| `CameraController` (editor) | `camera_system_tick` (systems v1) |
| `scene::Bvh` (runtime) | `OntBvhNode` (formato) |
| `os::Window` | `visor::WindowManager` |
| `external/vma` | `external/VulkanMemoryAllocator` |
| `check_size.cpp`: `sizeof(OntHeader)==124` | `scene.h:162`: `OntHeader` real 120 bytes |
| README: `.mgproject` | Código: `.mgproj` |

### 5.3 Contradicciones entre arquitecturas

- **Formato**: README dice RIH JSON. Código usa ONT binario.
- **Backend**: README dice D3D11. Código usa Vulkan.
- **Ejecución**: README dice CPU ray marching. Código usa GPU Vulkan.
- **RHI**: README trata D3D11 como activo. `build.bat` no compila `rhi/`.

---

## 6. Estado por pieza

### 6.1 Conservar (activo, coherente, útil)

| Pieza | Motivo |
|---|---|
| `os/` | Capa SO mínima, sin deuda, correcta. |
| `core/fixed_timestep.h` | Implementación completa, coherente, 24 LOC. |
| `core/herm_bridge.cpp` | Puente real `.herm → OntScene`. 4 TODOs SDF pendientes. |
| `render/vulkan_core.cpp` + `vulkan_pipeline.cpp` | Backend GPU activo. |
| `render/scene.cpp` (`OntScene::loadOnt`) | Lector `.ont` funcional. |
| `render/jit_compiler.cpp` | Backend CPU JIT con asmjit. |
| `visor/visor_app.cpp` | Orquestador actual. Candidato a refactor, no a eliminar. |
| `scene/project.h` | `.mgproj` funcional con autosave. |
| `scene/camera.h` | `CameraController` funcional (FOLLOW es stub). |
| `deps/lenguaje-hermetico` | Compilador externo activo. |

### 6.2 Arreglar (funciona, tiene problema)

| Pieza | Problema |
|---|---|
| `render/scene.h` | Dos modelos de datos coexistiendo. Necesita split. |
| `scene/scene_graph.h` | `resolveWorldTransform` suma Euler angles en lugar de componer. |
| `visor/input_bus.cpp` | Setters no conectados al `WndProc`. |
| `visor_app.cpp` (1,416 LOC) | God object. Candidato a fragmentar. |
| `tools/` | 6 de 8 herramientas stale. Limpiar o arreglar. |
| `README.md` | Describe una arquitectura que ya no existe. |
| `ont_format_v1.md` | Tres structs no coinciden con código. |

### 6.3 Eliminar (huérfano, roto, duplicado)

| Pieza | LOC | Motivo |
|---|---|---|
| `rhi/` completo | 254 | Backend D3D11 sin integrar. Solo usado por tests. |
| `os/win32/timer.cpp` | 22 | Solo usado por tests. El visor usa QPC directo. |
| `os/win32/win32.cpp` (`Window`) | 74 | Solo usado por tests. Duplica `WindowManager`. |
| `core/renderer.cpp` | 140 | Fuera del build. Legacy RIH. |
| `core/rih_reader.cpp` | 378 | Fuera del build. Legacy RIH. |
| `core/sdf_eval.cpp` | 289 | Fuera del build. Duplica `render/sdf_eval.cpp`. |
| `core/core.h` | 100 | Modelo de datos paralelo no usado. |
| `render/scratch.cpp` | 9 | No compilado. |
| `external/vma` | 2 | Stale; `VulkanMemoryAllocator` está activo. |
| `test_minimo.cpp` | 27 | Entry huérfano. |
| `test_bvh.cpp` | 51 | Ruta externa, no compilado. |
| `test_debug.cpp` | 57 | Ruta externa, no compilado. |
| `test_ont_bmp.cpp` | 89 | Ruta externa, no compilado. |
| `test_pipeline.cpp` | 28 | Ruta externa, no compilado. |
| `test_render_catedral.cpp` | 139 | Ruta externa, no compilado. |
| `test_suelo_test.cpp` | 30 | Ruta externa, no compilado. |
| `test_write.md` | 1 | Basura. |
| `phase6_diag.txt` | 909 | Log. |
| `motor grfico.txt` | 70 | Nota suelta. |
| `bench2_err.txt` | 30 | Log. |
| Raíz: `*.obj`, `*.exe`, `*.lib` sueltos | — | Basura MSVC. |
| `tools/check_size.cpp` | 33 | Mide structs obsoletos (OntHeader 124 vs 120). |
| `tools/check_offsets.cpp`, `check_sample.cpp`, `test_dump_scene.cpp` | 165 | Rutas personales. |
| `tools/test_bridge.cpp`, `test_roundtrip.cpp` | 63 | Sin `.bat`, legacy. |
| `tools/build_visor.bat`, `build_script.bat` | 14 | Stale. |

### 6.4 Reescribir (diseño roto o sin implementar)

| Pieza | Motivo |
|---|---|
| `core/systems.*` + `systems_arch.md` | ABI v1 descrito no existe en código. Sistema nunca activado. |
| `core/determinism_spec.md` + `input_replay` | Spec sin implementación. `--record`/`--replay` no parseados. |
| `visor/export_dialog.cpp` | Stub de export. |
| `visor/ontology_panel.cpp` | `Add node` es TODO. |
| `render/ri_optimizer.comp` | Fuente sin `.spv`. |
| `render/spirv_gen.comp` | Fuente sin `.spv`. |
| `visor/input_bus.*` | Reconectar a `WndProc` o eliminar. |
| `scene/workspace.h` (`Layer`) | No usado en producción. |
| `scene/workspace.h` (`Timeline`) | Duplica `visor/timeline_panel.cpp`. |
| `core/herm_bridge.cpp` operadores SDF (REPEAT, TWIST, ELONGATE, DISPLACE) | Passthrough. No implementados. |

### 6.5 Verificar (sin evidencia concluyente)

- `BrickMap` (`render/scene.h:307`) — ¿uso real?
- `render/scene.h` `Scene` (viejo) — ¿sigue en uso o es residual?
- `render/scene.h` `OntObservation` — campos y formato.
- `render/bytecode_vm.h` — ¿el shader interpreta el bytecode o el JIT lo compila?
- `render/glsl_gen.cpp` vs `spirv_gen.cpp` — ¿ambos activos o uno reemplaza al otro?
- `SceneQuery` (`ray_march.h:1039`) — ¿qué consulta?
- `render/window_manager.cpp` vs `os::Window` — confirmar que son independientes.

---

## 7. Decisiones pendientes

Antes de tocar nada, hay que resolver:

### 7.1 Arquitectura de datos

- **Unificar los 4 modelos de escena** en uno (o marcar claramente cuál es canónico y eliminar los demás).
- **Decidir si `.rih` se abandona** (ya lo abandonó `herm.exe` al producir `.ont`).
- **Decidir formato de `.ont`**: hacer coincidir spec y código, o actualizar spec.
- **Decidir rol de `.obs`**: ¿sigue siendo metadata opcional o se funde en `.ont`?

### 7.2 Backend de render

- **Decidir el rol de `rhi/d3d11.cpp`**: eliminar, archivar, o integrar como backend alternativo.
- **Decidir el rol de `render/ray_march_simd.h`**: ¿es necesario si `--bench` (JIT) es el path CPU canónico?
- **Decidir si `render/ray_march.h` (CPU escalar) se mantiene** o se elimina tras SIMD/JIT.

### 7.3 Systems y determinismo

- **Decidir si el sistema de systems se implementa de verdad o se elimina.**
- **Decidir si `--record`/`--replay` se implementan o se borran de las specs.**
- **Decidir si `camera_system_tick` se activa o se reemplaza por `CameraController`.**

### 7.4 Build y tests

- **Actualizar `README.md`** para que describa la arquitectura actual.
- **Actualizar `ont_format_v1.md`** para que coincida con `scene.h`.
- **Reparar `test_scene` (81/82)** y **`test_os_rhi` (30/31)**.
- **Reparar `tests/build_benchmark.bat`** o eliminar `benchmark_p0`.
- **Añadir un `build_all.bat` y `test_all.bat`.**
- **Limpiar la raíz** de `*.obj`, `*.exe`, `test_*.cpp`, logs, notas.
- **Resolver el submódulo roto** (`external/asmjit` con `.gitmodules` o como carpeta normal).

### 7.5 Integración con repos hermanos

- **Decidir si MLIR-CampoTensorial se usa o no.** Está clonado pero no en build.
- **Definir el contrato `motor-grafico` ↔ `lenguaje-hermetico`** explícitamente (hoy es acoplamiento por `#include` + `libherm.lib`).

### 7.6 Visión vs realidad

- **Decidir si el motor apunta a "runtime de `.ont`" o a "compilador de universos declarativos".** Son dos proyectos distintos.
- **Decidir si la "reducción de píxeles" (render_w=480, vp_w=974) es intencional o deuda.**
- **Decidir si el modelo 4D (W geométrico) se implementa o se abandona.**
- **Decidir si P/I/K/F/Q/B son un paradigma a implementar o una narrativa a retirar.**

---

## 8. Métricas observadas (evidencia del propio repo)

**Fuente A — `benchmark_result.txt`** (escrito por `--bench-vulkan`):
```
Escena: 35 nodos, 21 BVH, 14 materiales, 3340 bytecode bytes
Resolucion: 1280 x 720
GPU Vulkan Compute (200 frames):
  Media: 18.920 ms (52.9 FPS)
  Total: 3783.933 ms
```

**Fuente B — `stats/runs.csv`** (escrito por `--stats`, fila `01_bodega_baja`):
```
fecha: 2026-10-11 20:47:06
escena: 01_bodega_baja
viewport: 974 x 617
escala: 1.00
render: 480 x 270        ← render interno reducido
nodos: 4, bvh: 1, materiales: 4, bytecode: 385
gpu: NVIDIA RTX 3060 Laptop
frames: 199
fps_media: 69.787
gpu_util_pct: 7.0
gpu_power_w: 28.87
vram_mb: 1113.0
```

**Notas críticas:**
- El render interno es **480×270** con viewport **974×617**. Hay upscale por factor ~2×. Ese upscale es la "reducción de píxeles" mencionada.
- `vram_mb: 1113` para 4 nodos es desproporcionado.
- Los números son de **4 y 35 nodos**. No hay evidencia de escalabilidad.
- "60 FPS estables con 130 MB" del usuario **no coincide con ninguna fuente medida**.

---

## 9. Cierre

Este documento reemplaza cualquier descripción previa del motor. Todo lo que aparece está respaldado por archivo:línea, LOC, o salida real de ejecución.

**Lo que el motor es:**
Un runtime de escenas `.ont` (SDF binario) que carga, compila a código JIT (CPU) o a shader especializado (GPU), y renderiza con ray marching. Tiene editor ImGui con autosave. Los `.herm` los produce `herm.exe` en otro repo.

**Lo que el motor no es (todavía):**
- No es un compilador de universos declarativos.
- No es un motor 4D.
- No implementa P/I/K, F/Q/B, ni el "bloque 4D".
- No implementa systems cargables, ni ABI versionado.
- No tiene determinismo cross-run funcional.
- No exporta juegos.

**El problema estructural:**
El motor pasó por dos arquitecturas sin limpiar la primera. El README, las specs y las decisiones declaradas corresponden a la primera. El código activo corresponde a la segunda. Hay piezas de ambas conviviendo.

**El siguiente paso es decidir**: qué arquitectura es la canónica, qué se conserva de la otra, y qué se elimina. Ese es el trabajo de las Secciones 6 y 7.

---

*Fin del documento. Toda afirmación con archivo:línea está respaldada por evidencia extraída en Fases 0–3.*# MAPA_REAL_MOTOR.md

> Documento consolidado a partir de evidencia verificada en Fases 0–3.
> Fuentes: `motor-grafico` (repo), `deps/lenguaje-hermetico` (repo hermano), `external/` (dependencias), `README.md`, `build.bat`, `setup_deps.bat`, tests reales.
> Regla aplicada: solo lo que el código dice. Cero interpretación sin respaldo.

---

## 0. Resumen ejecutivo

El proyecto `motor-grafico` es un **runtime de escenas SDF** con dos arquitecturas superpuestas:

**Arquitectura 1 (junio 2026, "sesión única" según README):**
- Ray marching en CPU puro.
- Formato `.rih` (JSON).
- RHI DirectX 11.
- Modelo de datos: `Scene`/`Node`/`Material`.
- Tests en raíz del repo.
- README describe esta arquitectura.

**Arquitectura 2 (estado actual):**
- Ray marching en GPU con Vulkan compute shaders.
- Formato `.ont` (binario) + `.obs`.
- Lenguaje fuente `.herm` (compilado por `deps/lenguaje-hermetico`).
- Modelo de datos: `OntScene`/`OntGraphNode`/`OntMaterial`.
- Editor ImGui con 20 paneles.
- Vulkan en `render/`; D3D11 queda huérfano en `rhi/`.

Ambas coexisten sin marcadores de transición. El README, el build system y las specs no coinciden con el código real en múltiples puntos.

---

## 1. Inventario por módulo

### 1.1 Totales

| Lenguaje | LOC | % |
|---|---|---|
| C++ (`.cpp` + `.h`) | 17,358 | 55.9% |
| GLSL (`.comp`) | 827 | 2.7% |
| Otro (`.rih`, `.md`, `.txt`, `.herm`, `.bat`, `.mmd`, `.ini`, `.ont`, `.obs`) | 12,855 | 41.4% |
| Python | 0 | 0% |
| C puro | 0 | 0% |
| **Total** | **31,040** | 100% |

### 1.2 Módulos

| Módulo | LOC | Estado | Tests |
|---|---|---|---|
| `visor/` | ~4,700 | Activo. `visor_app.cpp` god object 1,416 LOC. | Ninguno |
| `render/` | ~6,400 | Activo. Tres ray marchings (CPU escalar, CPU SIMD, GPU). | `test_render` (no compilable hoy) |
| `scene/` | 1,098 | Activo. Solo headers. | `test_scene` (81/82) |
| `core/` | ~1,500 | Medio activo. 3/5 `.cpp` fuera del build. | `test_f0/f1/f3` (OK) |
| `os/` | 239 | Activo. `Timer` y `Window` solo usados en tests. | `test_os_rhi` (30/31) |
| `rhi/` | 254 | **Huérfano**. D3D11 sin integrar. | Solo en `test_os_rhi` |
| `tools/` | ~430 | Mayormente stale. Solo `test_ont_bridge` en build. | — |
| `tests/` | ~1,100 | 5 suites funcionales + 1 rota. | — |
| `deps/lenguaje-hermetico/` | externo | Activo. Compila `.herm → .ont`. | — |
| `external/` | externo | asmjit, volk, VMA, imgui v1.91.9. | — |

### 1.3 Archivos por módulo

**`visor/` (21 `.cpp`/`.h`, ~4,700 LOC)**:
- `visor_app.cpp` (1,416) + `.h` (122).
- `window_manager.cpp` (503) + `.h` (80).
- `herm_editor.cpp` (320) + `.h` (45).
- `gizmos_panel.cpp` (276) + `.h` (37).
- `anomaly_gate.cpp` (248) + `.h` (27).
- `main.cpp` (269).
- `command_palette.cpp` (134).
- `inspector_panel.cpp` (132).
- `input_controller.cpp` (112).
- `ontology_panel.cpp` (102).
- `tensor_inspector.cpp` (104).
- `source_browser.cpp` (103).
- `viewport_manager.cpp` (92).
- `input_bus.cpp` (11) + `.h` (93).
- `export_dialog.cpp` (77) — stub.
- `scheduler.cpp` (74).
- `console_panel.cpp` (64).
- `scene_manager.cpp` (64).
- `timeline_panel.cpp` (62).
- `undo_redo.cpp` (57) + `.h` (55).
- `profiler_panel.cpp` (41).

**`render/` (21 archivos, ~6,400 LOC)**:
- `.cpp`: `vulkan_core.cpp` (1038), `scene.cpp` (686), `glsl_gen.cpp` (558), `vulkan_pipeline.cpp` (404), `jit_compiler.cpp` (395), `spirv_gen.cpp` (317), `ri_optimizer.cpp` (185), `sdf_eval.cpp` (110), `scratch.cpp` (9).
- `.h`: `ray_march.h` (993), `ray_march_simd.h` (891), `scene.h` (337), `sdf_eval.h` (199), `bytecode_vm.h` (158), `render.h` (128), `vulkan_core.h` (92), `vulkan_pipeline.h` (67), `jit_compiler.h` (29), `glsl_gen.h` (22), `ri_optimizer.h` (26), `spirv_gen.h` (26).
- `.comp`: `ray_march.comp` (184), `ri_optimizer.comp` (128), `spirv_gen.comp` (105).
- `.md`: `ont_format_v1.md` (110), `shader_tensor_spec.md` (96).

**`scene/` (5 headers, 1,098 LOC)**:
- `project.h` (362), `scene_graph.h` (284), `camera.h` (227), `scene_query.h` (117), `workspace.h` (108).

**`core/` (14 archivos, ~1,500 LOC)**:
- Compilados: `herm_bridge.cpp` (720), `systems.cpp` (25).
- **No compilados**: `renderer.cpp` (140), `rih_reader.cpp` (378), `sdf_eval.cpp` (289).
- Headers: `core.h` (100), `herm_bridge.h` (34), `systems.h` (31), `fixed_timestep.h` (24), `renderer.h` (20), `sdf_eval.h` (19), `rih_reader.h` (7).
- Docs: `systems_arch.md` (60), `determinism_spec.md` (82).

**`os/` (5 archivos, 239 LOC)**:
- `os.h` (60), `win32/win32.cpp` (74), `win32/mem.cpp` (45), `win32/file.cpp` (38), `win32/timer.cpp` (22).

**`rhi/` (2 archivos, 254 LOC)**:
- `d3d11.cpp` (211), `rhi.h` (43).

---

## 2. Flujo real

### 2.1 Compilación offline (fuera del motor)

```
.herm ──[herm.exe]──> .rih (JSON) ──[herm.exe]──> .ont (binario)
                                  └─> .obs (metadata)
```

Evidencia:
- `deps/lenguaje-hermetico/herm/main.cpp:32` → `int main(int, char**)` del CLI `herm.exe`.
- `herm_compile.cpp:87` → `compileFromString` (Lexer → Parser → Resolver → Rih).
- `herm_rih.cpp:235` → `writeRihJson`; `:246` → `writeRihBinary`.
- `herm_ont.cpp:1165` → `writeOnt` (escribe `.ont`).
- `herm_obs.cpp:22` → `writeObs`.

**`motor-grafico` no compila `.herm` ni `.ont`. Solo los lee.** El único compilador en proceso es `herm.exe`, binario externo.

### 2.2 Runtime GPU (path principal)

Invocación:
```
visor.exe "escenas\X.ont" --bench-vulkan --stats
```

Trazado:
1. `visor/main.cpp:210` → `WinMain`.
2. `visor_app.cpp:37` → `VisorApp::init`. Llama a `workspace.init()`, `win.init()`, `vk_ctx.init()`, `vk_ctx.initSwapchain()`.
3. `render/scene.cpp:14` → `OntScene::loadOnt(fm)` lee `.ont` desde `FileMapping`.
4. `render/scene.cpp:66` / `:141` → `OntScene::loadObs` + `applyObs` lee `.obs`.
5. `render/render.h:65` → `Renderer::loadOnt` → `VulkanSceneData::init`.
6. `render/vulkan_pipeline.cpp:309` → `initPipeline` carga `ray_march.spv` precompilado.
7. `visor_app.cpp:378` → `VisorApp::run` (bucle).
8. `visor_app.cpp:530` → rama GPU (`renderer.use_vulkan && renderer.ont_mode`).
9. `visor_app.cpp:605` → `VulkanSceneData::updateUBO`.
10. `visor_app.cpp:614` → `drawEditorUI`.
11. `visor_app.cpp:619` → `VulkanContext::drawFrame`:
    - `render/vulkan_core.cpp:643` — acquire swapchain image.
    - `render/vulkan_pipeline.cpp:325` → `recordComputeCommandBuffer` → `vkCmdDispatch((w+7)/8, (h+7)/8, 1)`.
    - `render/vulkan_core.cpp:785` → `vkCmdCopyBufferToImage` (output_buffer → swapchain).
    - `render/vulkan_core.cpp:788` → `copyOutputToViewport` → `viewport_image`.
    - `render/vulkan_core.cpp:791` → `requestReadback` (tensor_buffer → staging).
    - `render/vulkan_core.cpp:846` → `ImGui_ImplVulkan_RenderDrawData`.
    - `render/vulkan_core.cpp:897` → `vkQueuePresentKHR`.

### 2.3 Runtime CPU (bench headless)

Invocación:
```
visor.exe "escenas\X.ont" --bench
```

Trazado:
1. `visor/main.cpp:218` → `benchmark(path)` (guardado con `!bench_vulkan`).
2. `visor/main.cpp:32` → `benchmark()`.
3. `FileMapping` abre el `.ont`.
4. `JitCompiler::compileScene` (`render/jit_compiler.cpp:406`) compila bytecode SDF a código x86 SSE con asmjit.
5. `renderOntSceneMT` o `renderOntSceneMTSIMD` (`render/ray_march.h:875`, `render/ray_march_simd.h:811`).
6. Escribe `benchmark_result.txt`.

### 2.4 Live-compile en editor

```
HermEditor::openFile(path)  (visor/herm_editor.cpp:67)
   ↓
editor_source (string en memoria)
   ↓
mg::compileHermToOntScene(src, out, err)  (core/herm_bridge.cpp:792)
   ↓
herm::compileToRih  (deps/lenguaje-hermetico)
   ↓
mg::convertHermToOntScene  (core/herm_bridge.cpp)
   ↓
mg::optimizeOntScene  (render/ri_optimizer.cpp)
   ↓
OntScene en memoria → mismo path 2.2 desde paso 6
```

### 2.5 Formato `.ont`

**Header** (80 bytes, `pack(1)`, según spec; código con campos extra → 120 bytes reales):

```
magic (4) + version (4) + epsilon (4) + node_count (4) + bvh_count (4) +
material_count (4) + bytecode_size (4) + tensor_buffer_size (4) +
aabb_min[4] (16) + aabb_max[4] (16) + reserved[7] (56)
```

Layout: `OntHeader | OntBvhNode[bvh_count] | OntGraphNode[node_count] | bytecode | OntMaterial[material_count]`.

- `OntBvhNode`: 52 bytes (AABB + d_min + L + skip_index + first_node + node_count + flags).
- `OntGraphNode`: 116 bytes (transform 4x4 + material_id + bytecode_off/len + bbox + tensor_slot + mode).
- `OntMaterial`: 60 bytes (id + color + roughness + metallic + emission + opacity + tensor_slot).

**Discrepancia**: `ont_format_v1.md` dice `OntHeader=80`, `OntGraphNode=100`, `OntMaterial=52`. El código da 120, 116, 60. **Tres de cuatro structs no coinciden**.

### 2.6 Formato `.obs`

`ObsHeader` + secciones opcionales (`OBS_CAMERA`, `OBS_LIGHTS`, `OBS_TIMELINE`, `OBS_BACKGROUND`, `OBS_RESOLUTION`). Sobrescribe campos del `OntScene`. Formato definido en `render/scene.h:211+`.

### 2.7 Shaders

| Archivo | Uso | Compilación |
|---|---|---|
| `render/ray_march.comp` | Fallback genérico | `glslc` en `build.bat:94` → `build/shaders/ray_march.spv` (19520 bytes) |
| `render/spirv_gen.comp` | Generador GPU (F3) | **No compilado por `build.bat`** |
| `render/ri_optimizer.comp` | Optimizador GPU (F1) | **No compilado por `build.bat`** |

Además, `render/glsl_gen.cpp:511` invoca `glslc.exe` en **runtime** vía `CreateProcessA` para generar shaders especializados. Ruta hardcodeada: `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe`. Fallback a `ray_march.spv`.

---

## 3. Dependencias reales

### 3.1 Dependencias declaradas (de `setup_deps.bat`)

| Dep | Versión | Uso en build |
|---|---|---|
| asmjit | HEAD | Sí (`build_asmjit.bat` → `build/asmjit.lib`) |
| volk | HEAD | Sí (`volk.c`) |
| VMA | HEAD | Sí (`external/VulkanMemoryAllocator`) |
| imgui | v1.91.9 | Sí (editor) |
| lenguaje-hermetico | HEAD | Sí (`deps/lenguaje-hermetico`) |
| MLIR-CampoTensorial | HEAD | **No** (clonado, no usado por `build.bat`) |
| Vulkan SDK | 1.4.350.0 | Prereq obligatorio |
| MSVC VS 2022 BuildTools | — | Prereq |

### 3.2 Duplicaciones de dependencias

- `external/vma` (2 archivos, stale) **y** `external/VulkanMemoryAllocator` (273 archivos, activo). El build usa el segundo.

### 3.3 Dependencias silenciosas

- `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe` — invocado en runtime por `render/glsl_gen.cpp:588`.
- `nvidia-smi` — invocado por `VisorApp::pollNvidiaSmi` (`visor_app.cpp:676`).
- Rutas personales hardcodeadas:
  - `visor/main.cpp:280`: `..\..\..\proyectos-index\proyecto de IA artistica\Lenguaje Hermetico\.gitignored\libreria\escenas\catedral_hermetica_0000.ont`.
  - `tools/check_offsets.cpp`, `check_sample.cpp`, `test_dump_scene.cpp`: `C:\Users\Josue B\...`.

### 3.4 Submódulo git roto

`external/asmjit` está indexado como gitlink (`160000`) pero **no existe `.gitmodules`**. `git submodule status` falla. `setup_deps.bat` clona como carpeta normal, chocando con el índice.

---

## 4. Código muerto y duplicaciones

### 4.1 Archivos definidos y no compilados

| Archivo | LOC | Estado |
|---|---|---|
| `core/renderer.cpp` | 140 | No en `build.bat` |
| `core/rih_reader.cpp` | 378 | No en `build.bat` |
| `core/sdf_eval.cpp` | 289 | No en `build.bat` |
| `core/core.h` | 100 | Modelo de datos paralelo, sin consumidor |
| `render/scratch.cpp` | 9 | No en `build.bat` |
| `test_minimo.cpp` | 27 | Entry WinMain suelto, no compilado |
| `test_bvh.cpp` | 51 | No compilado, ruta externa |
| `test_debug.cpp` | 57 | No compilado, ruta externa |
| `test_ont_bmp.cpp` | 89 | No compilado, ruta externa |
| `test_pipeline.cpp` | 28 | No compilado, ruta externa |
| `test_render_catedral.cpp` | 139 | No compilado, ruta externa |
| `test_suelo_test.cpp` | 30 | No compilado, ruta externa |
| `test_write.md` | 1 | Basura |
| `motor grfico.txt` | 70 | Nota suelta |
| `phase6_diag.txt` | 909 | Log de fase 6 |
| `bench2_err.txt`, `bench2.txt`, `output.txt`, `profile.txt` | — | Logs |

### 4.2 Módulos huérfanos

- **`rhi/d3d11.cpp` (211) + `rhi/rhi.h` (43)** — Backend D3D11 completo sin integrar. Solo lo usan `tests/test_os_rhi.cpp`. Nadie en producción.
- **`os::Timer` (22) + `os::Window` (74)** — Solo usados en `tests/test_os_rhi.cpp`. El visor usa `QueryPerformanceCounter` directo y `WindowManager`.

### 4.3 Código no alcanzado

- **`play_mode`** (`visor/input_bus.h:69`) — declarado `= false`, leído en `visor_app.cpp:400`, **nunca asignado a `true`**. Todos los "Systems v1" detrás de este flag son código muerto.
- **`InputBus::onKey/onMouseMove/onMouseButton`** — definidos, **sin llamadas desde `WndProc`**. El `InputSnapshot` llega vacío a `camera_system_tick`.
- **`DEFAULT_SYSTEMS[]`** — array `static const` con un único system (`camera_system_tick`). No hay carga dinámica (`dlopen`/`LoadLibrary`).

### 4.4 Duplicaciones arquitectónicas

- **Tres ray marchings**: `render/ray_march.h` (CPU escalar), `render/ray_march_simd.h` (CPU SIMD+MT), `render/ray_march.comp` (GPU).
- **Dos backends de render**: Vulkan (activo) + D3D11 (huérfano) + CPU JIT (activo en `--bench`).
- **Cuatro modelos de escena**:
  - `core/core.h` — no usado.
  - `render/scene.h` "viejo" (`Scene`, `Node`, `Material`) — usado por `SceneGraph`, `Project`.
  - `render/scene.h` "nuevo" (`OntScene`, `OntGraphNode`, `OntMaterial`) — usado por runtime GPU.
  - `scene/*.h` (`SceneNode`, `SceneGraph`) — usado por editor.
- **Dos BVHs**: `scene::Bvh` (runtime, punteros) y `render::OntBvhNode` (formato `.ont`, stackless).
- **Dos sistemas de cámara**: `scene::CameraController` (editor) y `mg::camera_system_tick` (systems v1, muerto).
- **Dos "W"**: `Timeline::getW() = current_time` y `FixedTimestep::currentW() = tick_count * DT`. Y `Timeline::fps = 30` vs `DT = 1/60`.
- **Dos viewports**: `scene/workspace.h` y `visor/viewport_manager.h`.
- **Dos implementaciones SDF**: `core/sdf_eval.cpp` (no compilado) y `render/sdf_eval.cpp` (compilado).
- **Dos VMA**: `external/vma` y `external/VulkanMemoryAllocator`.
- **Dos "compiladores" de shader**: `glsl_gen.cpp` y `spirv_gen.cpp` (ambos generan GLSL/SPIR-V runtime).

### 4.5 Código de la era anterior

- **`README.md`** describe Arquitectura 1 (CPU ray marching + RIH + D3D11). No menciona Vulkan, `.ont`, `.herm`, `.obs`, `--bench-vulkan`, `--stats`, compute shaders.
- **`test_*.cpp` en raíz** — 6 de 7 usan rutas externas y formato `.rih`. Era anterior.
- **`core/renderer.cpp`, `core/rih_reader.cpp`** — interfaz legacy con `.rih`.

### 4.6 TODOs/FIXMEs

**Concentrados en dos archivos** (el resto del motor no tiene ninguno):

`core/herm_bridge.cpp`:
- Línea 513 — `REPEAT` sin implementar, "passthrough".
- Línea 518 — `TWIST` sin implementar.
- Línea 523 — `ELONGATE` sin implementar.
- Línea 528 — `DISPLACE` sin implementar.

`visor/`:
- `visor_app.cpp:1244` — Export stub.
- `visor_app.cpp:1251` — About TODO.
- `visor_app.cpp:1274` — Camera mode cycle TODO.
- `ontology_panel.cpp:91` — Add node TODO.
- `source_browser.cpp:105` — Create .herm file TODO.
- `export_dialog.cpp:87` — `launch.bat` con `REM TODO: Launch actual game executable`.

### 4.7 Build scripts stale

- `tools/build_visor.bat` — apunta a `C:\Users\Josue B\...\Motor Grafico`.
- `tools/build_script.bat` — archivos inexistentes (`main_win32.cpp`, `brick_map.cpp`).
- `tests/build_benchmark.bat` — linka `build/obj/**/*.obj` que no existen.

---

## 5. Contradicciones con la visión declarada

### 5.1 Contradicciones entre documento y código

| Documento dice | Código dice |
|---|---|
| `.herm → .rih → .ont → GPU` en el motor | El motor solo lee `.ont`. `.herm → .ont` lo hace `herm.exe`. |
| `tensor_buffer` en VRAM, readback, doble buffer | Spec existe (`shader_tensor_spec.md`). Implementación en `render/vulkan_core.cpp`. |
| BVH 4D con AABB + d_min + Lipschitz | `OntBvhNode` tiene `d_min` y `L`. Spec los describe. Sin verificar uso en shader. |
| Systems Python/C con `SystemContext_v1` | Solo C++. `camera_system_tick` único system. Nunca se ejecuta (`play_mode=false`). |
| "Sin strings en runtime" | `glsl_gen.cpp` genera GLSL como strings en runtime. `.mgproj` es JSON. ImGui serializa DockSpace a string. `diag("[PHASE6]")` en producción. |
| "Sin variables globales mutables" | `tensor_buffer` es global mutable. `Arena` es global. `systems.cpp` escribe en `tensor_delta`. |
| "Sin threading" | `ray_march.h` incluye `<thread>`, tiene `renderOntSceneMT`. Autosave usa thread. |
| "Determinismo bit a bit cross-run" | Spec existe (`determinism_spec.md`). `--record`/`--replay` **no se parsean** en `main.cpp`. SHA-256 no implementado. |
| "Sin dependencias externas" | Depende de `C:\VulkanSDK\1.4.350.0\Bin\glslc.exe` y `nvidia-smi` en runtime. |
| "F/Q/B pirámide ontológica" | Solo aparece en `docs/10-contrato-ontologico.md`. No hay código que la implemente. |
| "P/I/K operadores fundamentales" | Solo aparece en docs. No hay código que los implemente. |
| "Universo de bloque 4D" | `W` es un `float` en `FixedTimestep::currentW()`. No hay bloque precompilado. |
| "Editor con 13 módulos" | 21 archivos en `visor/`, mayoría <130 LOC. Varios con TODO. |
| "Exportar juego" | `ExportDialog::runExport` (visor/export_dialog.cpp:59) solo crea directorio + `launch.bat` con `REM TODO`. |
| `ont_format_v1.md`: `OntHeader=80`, `OntGraphNode=100`, `OntMaterial=52` | Código: 120, 116, 60. 3 de 4 structs no coinciden. |
| `README.md`: "219 tests, todos pasando", "CPU ray marching" | Real: `test_scene` 81/82, `test_os_rhi` 30/31. Runtime principal es Vulkan GPU. |
| `systems_arch.md`: SceneGraph con `add/remove/reparent` | `SceneGraph` solo tiene `setParent`. No `add`/`remove`/`reparent`. |
| `systems_arch.md`: `system_tick_v1/v2/v3` con carga por símbolo | Solo `camera_system_tick`. Sin `dlopen`. Sin versionado. |
| `.mgproj` enumera systems | `.mgproj` no tiene campo para enumerar systems. |
| MLIR-CampoTensorial es dependencia | `setup_deps.bat` lo clona; `build.bat` no lo usa. |
| "Reducción de píxeles con MLIR" | Reducción existe (render interno a 480×270 en CSV). No está claro si usa MLIR. |

### 5.2 Contradicciones internas al código

| Fuente A | Fuente B |
|---|---|
| `core/sdf_eval.cpp` | `render/sdf_eval.cpp` |
| `core/renderer.cpp` | `render/render.cpp` (no existe) — `render.h` define `Renderer` |
| `Timeline::fps = 30` | `FixedTimestep::DT = 1/60` |
| `Timeline::getW() = current_time` | `FixedTimestep::currentW() = tick_count * DT` |
| `CameraController` (editor) | `camera_system_tick` (systems v1) |
| `scene::Bvh` (runtime) | `OntBvhNode` (formato) |
| `os::Window` | `visor::WindowManager` |
| `external/vma` | `external/VulkanMemoryAllocator` |
| `check_size.cpp`: `sizeof(OntHeader)==124` | `scene.h:162`: `OntHeader` real 120 bytes |
| README: `.mgproject` | Código: `.mgproj` |

### 5.3 Contradicciones entre arquitecturas

- **Formato**: README dice RIH JSON. Código usa ONT binario.
- **Backend**: README dice D3D11. Código usa Vulkan.
- **Ejecución**: README dice CPU ray marching. Código usa GPU Vulkan.
- **RHI**: README trata D3D11 como activo. `build.bat` no compila `rhi/`.

---

## 6. Estado por pieza

### 6.1 Conservar (activo, coherente, útil)

| Pieza | Motivo |
|---|---|
| `os/` | Capa SO mínima, sin deuda, correcta. |
| `core/fixed_timestep.h` | Implementación completa, coherente, 24 LOC. |
| `core/herm_bridge.cpp` | Puente real `.herm → OntScene`. 4 TODOs SDF pendientes. |
| `render/vulkan_core.cpp` + `vulkan_pipeline.cpp` | Backend GPU activo. |
| `render/scene.cpp` (`OntScene::loadOnt`) | Lector `.ont` funcional. |
| `render/jit_compiler.cpp` | Backend CPU JIT con asmjit. |
| `visor/visor_app.cpp` | Orquestador actual. Candidato a refactor, no a eliminar. |
| `scene/project.h` | `.mgproj` funcional con autosave. |
| `scene/camera.h` | `CameraController` funcional (FOLLOW es stub). |
| `deps/lenguaje-hermetico` | Compilador externo activo. |

### 6.2 Arreglar (funciona, tiene problema)

| Pieza | Problema |
|---|---|
| `render/scene.h` | Dos modelos de datos coexistiendo. Necesita split. |
| `scene/scene_graph.h` | `resolveWorldTransform` suma Euler angles en lugar de componer. |
| `visor/input_bus.cpp` | Setters no conectados al `WndProc`. |
| `visor_app.cpp` (1,416 LOC) | God object. Candidato a fragmentar. |
| `tools/` | 6 de 8 herramientas stale. Limpiar o arreglar. |
| `README.md` | Describe una arquitectura que ya no existe. |
| `ont_format_v1.md` | Tres structs no coinciden con código. |

### 6.3 Eliminar (huérfano, roto, duplicado)

| Pieza | LOC | Motivo |
|---|---|---|
| `rhi/` completo | 254 | Backend D3D11 sin integrar. Solo usado por tests. |
| `os/win32/timer.cpp` | 22 | Solo usado por tests. El visor usa QPC directo. |
| `os/win32/win32.cpp` (`Window`) | 74 | Solo usado por tests. Duplica `WindowManager`. |
| `core/renderer.cpp` | 140 | Fuera del build. Legacy RIH. |
| `core/rih_reader.cpp` | 378 | Fuera del build. Legacy RIH. |
| `core/sdf_eval.cpp` | 289 | Fuera del build. Duplica `render/sdf_eval.cpp`. |
| `core/core.h` | 100 | Modelo de datos paralelo no usado. |
| `render/scratch.cpp` | 9 | No compilado. |
| `external/vma` | 2 | Stale; `VulkanMemoryAllocator` está activo. |
| `test_minimo.cpp` | 27 | Entry huérfano. |
| `test_bvh.cpp` | 51 | Ruta externa, no compilado. |
| `test_debug.cpp` | 57 | Ruta externa, no compilado. |
| `test_ont_bmp.cpp` | 89 | Ruta externa, no compilado. |
| `test_pipeline.cpp` | 28 | Ruta externa, no compilado. |
| `test_render_catedral.cpp` | 139 | Ruta externa, no compilado. |
| `test_suelo_test.cpp` | 30 | Ruta externa, no compilado. |
| `test_write.md` | 1 | Basura. |
| `phase6_diag.txt` | 909 | Log. |
| `motor grfico.txt` | 70 | Nota suelta. |
| `bench2_err.txt` | 30 | Log. |
| Raíz: `*.obj`, `*.exe`, `*.lib` sueltos | — | Basura MSVC. |
| `tools/check_size.cpp` | 33 | Mide structs obsoletos (OntHeader 124 vs 120). |
| `tools/check_offsets.cpp`, `check_sample.cpp`, `test_dump_scene.cpp` | 165 | Rutas personales. |
| `tools/test_bridge.cpp`, `test_roundtrip.cpp` | 63 | Sin `.bat`, legacy. |
| `tools/build_visor.bat`, `build_script.bat` | 14 | Stale. |

### 6.4 Reescribir (diseño roto o sin implementar)

| Pieza | Motivo |
|---|---|
| `core/systems.*` + `systems_arch.md` | ABI v1 descrito no existe en código. Sistema nunca activado. |
| `core/determinism_spec.md` + `input_replay` | Spec sin implementación. `--record`/`--replay` no parseados. |
| `visor/export_dialog.cpp` | Stub de export. |
| `visor/ontology_panel.cpp` | `Add node` es TODO. |
| `render/ri_optimizer.comp` | Fuente sin `.spv`. |
| `render/spirv_gen.comp` | Fuente sin `.spv`. |
| `visor/input_bus.*` | Reconectar a `WndProc` o eliminar. |
| `scene/workspace.h` (`Layer`) | No usado en producción. |
| `scene/workspace.h` (`Timeline`) | Duplica `visor/timeline_panel.cpp`. |
| `core/herm_bridge.cpp` operadores SDF (REPEAT, TWIST, ELONGATE, DISPLACE) | Passthrough. No implementados. |

### 6.5 Verificar (sin evidencia concluyente)

- `BrickMap` (`render/scene.h:307`) — ¿uso real?
- `render/scene.h` `Scene` (viejo) — ¿sigue en uso o es residual?
- `render/scene.h` `OntObservation` — campos y formato.
- `render/bytecode_vm.h` — ¿el shader interpreta el bytecode o el JIT lo compila?
- `render/glsl_gen.cpp` vs `spirv_gen.cpp` — ¿ambos activos o uno reemplaza al otro?
- `SceneQuery` (`ray_march.h:1039`) — ¿qué consulta?
- `render/window_manager.cpp` vs `os::Window` — confirmar que son independientes.

---

## 7. Decisiones pendientes

Antes de tocar nada, hay que resolver:

### 7.1 Arquitectura de datos

- **Unificar los 4 modelos de escena** en uno (o marcar claramente cuál es canónico y eliminar los demás).
- **Decidir si `.rih` se abandona** (ya lo abandonó `herm.exe` al producir `.ont`).
- **Decidir formato de `.ont`**: hacer coincidir spec y código, o actualizar spec.
- **Decidir rol de `.obs`**: ¿sigue siendo metadata opcional o se funde en `.ont`?

### 7.2 Backend de render

- **Decidir el rol de `rhi/d3d11.cpp`**: eliminar, archivar, o integrar como backend alternativo.
- **Decidir el rol de `render/ray_march_simd.h`**: ¿es necesario si `--bench` (JIT) es el path CPU canónico?
- **Decidir si `render/ray_march.h` (CPU escalar) se mantiene** o se elimina tras SIMD/JIT.

### 7.3 Systems y determinismo

- **Decidir si el sistema de systems se implementa de verdad o se elimina.**
- **Decidir si `--record`/`--replay` se implementan o se borran de las specs.**
- **Decidir si `camera_system_tick` se activa o se reemplaza por `CameraController`.**

### 7.4 Build y tests

- **Actualizar `README.md`** para que describa la arquitectura actual.
- **Actualizar `ont_format_v1.md`** para que coincida con `scene.h`.
- **Reparar `test_scene` (81/82)** y **`test_os_rhi` (30/31)**.
- **Reparar `tests/build_benchmark.bat`** o eliminar `benchmark_p0`.
- **Añadir un `build_all.bat` y `test_all.bat`.**
- **Limpiar la raíz** de `*.obj`, `*.exe`, `test_*.cpp`, logs, notas.
- **Resolver el submódulo roto** (`external/asmjit` con `.gitmodules` o como carpeta normal).

### 7.5 Integración con repos hermanos

- **Decidir si MLIR-CampoTensorial se usa o no.** Está clonado pero no en build.
- **Definir el contrato `motor-grafico` ↔ `lenguaje-hermetico`** explícitamente (hoy es acoplamiento por `#include` + `libherm.lib`).

### 7.6 Visión vs realidad

- **Decidir si el motor apunta a "runtime de `.ont`" o a "compilador de universos declarativos".** Son dos proyectos distintos.
- **Decidir si la "reducción de píxeles" (render_w=480, vp_w=974) es intencional o deuda.**
- **Decidir si el modelo 4D (W geométrico) se implementa o se abandona.**
- **Decidir si P/I/K/F/Q/B son un paradigma a implementar o una narrativa a retirar.**

---

## 8. Métricas observadas (evidencia del propio repo)

**Fuente A — `benchmark_result.txt`** (escrito por `--bench-vulkan`):
```
Escena: 35 nodos, 21 BVH, 14 materiales, 3340 bytecode bytes
Resolucion: 1280 x 720
GPU Vulkan Compute (200 frames):
  Media: 18.920 ms (52.9 FPS)
  Total: 3783.933 ms
```

**Fuente B — `stats/runs.csv`** (escrito por `--stats`, fila `01_bodega_baja`):
```
fecha: 2026-10-11 20:47:06
escena: 01_bodega_baja
viewport: 974 x 617
escala: 1.00
render: 480 x 270        ← render interno reducido
nodos: 4, bvh: 1, materiales: 4, bytecode: 385
gpu: NVIDIA RTX 3060 Laptop
frames: 199
fps_media: 69.787
gpu_util_pct: 7.0
gpu_power_w: 28.87
vram_mb: 1113.0
```

**Notas críticas:**
- El render interno es **480×270** con viewport **974×617**. Hay upscale por factor ~2×. Ese upscale es la "reducción de píxeles" mencionada.
- `vram_mb: 1113` para 4 nodos es desproporcionado.
- Los números son de **4 y 35 nodos**. No hay evidencia de escalabilidad.
- "60 FPS estables con 130 MB" del usuario **no coincide con ninguna fuente medida**.

---

## 9. Cierre

Este documento reemplaza cualquier descripción previa del motor. Todo lo que aparece está respaldado por archivo:línea, LOC, o salida real de ejecución.

**Lo que el motor es:**
Un runtime de escenas `.ont` (SDF binario) que carga, compila a código JIT (CPU) o a shader especializado (GPU), y renderiza con ray marching. Tiene editor ImGui con autosave. Los `.herm` los produce `herm.exe` en otro repo.

**Lo que el motor no es (todavía):**
- No es un compilador de universos declarativos.
- No es un motor 4D.
- No implementa P/I/K, F/Q/B, ni el "bloque 4D".
- No implementa systems cargables, ni ABI versionado.
- No tiene determinismo cross-run funcional.
- No exporta juegos.

**El problema estructural:**
El motor pasó por dos arquitecturas sin limpiar la primera. El README, las specs y las decisiones declaradas corresponden a la primera. El código activo corresponde a la segunda. Hay piezas de ambas conviviendo.

**El siguiente paso es decidir**: qué arquitectura es la canónica, qué se conserva de la otra, y qué se elimina. Ese es el trabajo de las Secciones 6 y 7.

---

*Fin del documento. Toda afirmación con archivo:línea está respaldada por evidencia extraída en Fases 0–3.*