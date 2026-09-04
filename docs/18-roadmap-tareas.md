---
title: Roadmap de tareas - Editor motor-grafico
tags: [motor-grafico, ui-ux, roadmap, tareas]
created: 2026-08-28
---

# Roadmap de tareas — Editor `motor-grafico` (v1 → post-v2)

Plan ordenado y con dependencias derivado de `docs/16` (brief), `docs/17a/17b/17c`
(diseño del agente) y verificación directa del repo. Visión aceptada: el editor es
la **IDE del compilador hermético tensor/SDF** (no "motor tipo Unity" literal).
Live-compile resuelto como **librería linkable** (`libherm`).

## Convenciones
- `T-Fx` = prerrequisitos / deuda técnica.
- `T-1xx` = Fase 1 (shell + edición).
- `T-2xx` = Fase 2. `T-3xx` = post-v2. `T-Dx` = correcciones a los docs.
- "Bloquea" = no se puede empezar hasta completar la tarea nombrada.
- "Ghosted" = control presente pero deshabilitado hasta que su dependencia exista.

---

## 0. Prerrequisitos / Deuda técnica

| ID | Tarea | Detalle / archivos | Bloquea |
|----|-------|--------------------|---------|
| **T-F5** | Vendorizar ImGui (rama `docking`) | Clonar `ocornut/imgui` @ `docking` en `external/imgui`; añadir a `setup_deps.bat`. | Todo lo visual (T-101…T-116). |
| **T-F6** | Integrar `ImGui_ImplVulkan` en `vk_ctx` | Render 3D → `VkImage` offscreen → `ImGui::Image` + pasada UI. | T-101…T-102. |
| **T-F1.1** | Build `libherm` (static lib) | Compilar `deps/lenguaje-hermetico/herm/*.cpp` + `contrato/*.h`. Exponer `compile(src) -> herm::Rih`. | T-110, T-111, T-113, T-115. |
| **T-F1.2** | Editor: `.herm` → `herm::Rih` → `SceneGraph` | Llamar `libherm` ante edición; pasar `herm::Rih` a `core/rih_reader.cpp` (`loadRihFromString`) para reconstruir `SceneGraph` y re-renderizar (hot reload). | T-110, T-111, T-113, T-115. |
| **T-F1.3** | (Verificar) emisión `.ont` desde RIH | El motor carga `.ont` (BVH 4D GPU) vía `renderer.loadOnt`. Confirmar/implementar writer `.ont` desde `herm::Rih` para release GPU. | Nada en v1 (live-preview usa RIH directo). |
| **T-F2** | Incluir ABI 1×8 compartido | Ya definido en `deps/lenguaje-hermetico/contrato/rih.h` (`Material::tensor[8]` = `[x,y,z,w,r,g,b,a]`, binario `.rib` MAGIC `RIHH`). Solo añadir `contrato/` como include path del editor. | T-112, T-120, T-205. |
| **T-F3** | Definir formato `.mgproj` | JSON con `schema_version` + refs a `.ont`/`.herm`/scripts. | T-114. |
| **T-F4** | Corregir bug color dinámico en writer `.ont` | `render/scene.h` / writer de `lenguaje-hermetico`: material con color dinámico renderiza blanco (Fresnel). | T-107 (color real). |

> Nota de verificación: el motor **ya consume RIH en memoria** (`core/rih_reader.cpp`,
> `core/sdf_eval.cpp` usan `herm::Rih`/`rih.nodes`). Por eso T-F1 no requiere writer
> `.ont` para el live-preview (CPU ni Vulkan evalúan RIH/SDF directo).

---

## 1. Fase 0 — arranque paralelo
Ejecutar T-F5, T-F6 y T-F1.1/T-F1.2 en paralelo (son independientes entre sí).

1. T-F5 → T-F6 (la UI no existe sin ImGui).
2. T-F1.1 → T-F1.2 (live-compile).

## 2. Fase 1 — Shell + edición (depende de T-F5/T-F6 + T-F1.1/2)

| ID | Módulo (17b) | Tarea | Estado / depende |
|----|--------------|-------|------------------|
| **T-101** | M1–M3 | MenuBar funcional (File/Edit/View/Help) + Toolbar (Mode/Camera/Scale/FPS) + DockSpace | **✅ DONE** (P/I/K, Multi-viewport, Command Palette para v2) |
| **T-102** | — | Viewport muestra render vía `ImGui::Image` | **✅ DONE** (VkImage offscreen, descriptor propio, aspect ratio) |
| **T-103** | M11 | Árbol Ontológico F/Q/B (tree jerárquico, selección, enable/disable) | **✅ DONE** (M12 Inspector y M13 Gizmos para v2) |
| **T-104** | M12 | Inspector F/Q/B editable (dragfloat, color picker, sliders) | **✅ DONE** (sync SceneNode + Node) |
| **T-105** | M13 | Gizmos overlay 2D proyectados (Move/Rotate/Scale, drag, snap) | **✅ DONE** |
| T-106 | M14 | Snap/Grid | T-102. |
| T-107 | M17 | Material PBR editor | *Ghosted* hasta T-F4. |
| T-108 | M19 | Lighting/Environment | *Ghosted* (GPU 1 luz hoy). |
| T-109 | M20 | Render Settings | *Ghosted* AO/sombras. |
| **T-110** | §1 17c | Scheduler + Input Bus + Anomaly Gate (debounce 30ms, AST/bytecode validation) | **✅ DONE** |
| **T-111** | M8 | Code Editor `.herm` + live-compile (line numbers, file I/O, error markers, Ctrl+S) | **✅ DONE** |
| **T-116** | M27 | Profiler Panel (FPS, ms/frame, gráfico histórico, scene stats) | **✅ DONE** (M26 Console ya en T-113) |

## 3. Fase 1.5 — depende de Fase 1

| ID | Módulo | Tarea | Depende |
|----|--------|-------|---------|
| **T-112** | M29 | Tensor Inspector (tensor 1×8, history plots, anomaly detection) | **✅ DONE** (Readback/Staging para v2) |
| **T-113** | M10/M26 | Scene Validation estática + Console/Log panel | **✅ DONE** |
| **T-115** | M28 | Undo/Redo por diffs de AST (snapshot-based, Ctrl+Z/Y) | **✅ DONE** |
| T-118 | M22 | W-Scrubber (timeline W) | T-103. |
| T-119 | M23 | Play/Run Systems (esqueleto) | motor de systems (futuro). |
| T-120 | M36 | Scripting Bridge Python | T-F2. |
| T-121 | §3 17c | ray-SDF 3D picking + ID pass | T-102. |

## 4. Fase 2 — desbloquea toggles y systems

| ID | Módulo | Tarea | Desbloquea |
|----|--------|-------|-----------|
| T-201 | — | Pipeline GPU: multi-luz, color dinámico, AO/sombras | toggles reales M19/M20, y T-F4. |
| T-202 | §7 17c | `subtree-load`/`patch` en Bytecode VM | hot-swap de subárbol. |
| T-203 | M18 | Material Node Graph | — |
| T-204 | M33 | Systems / Game-logic | T-119. |
| T-205 | M34 | Audio System | T-F2. |

## 5. Post-v2 (north-star)
T-301 SDF-UI (M35) · T-302 ray-SDF 2D · T-303 bridges C hot-reload · T-304 export multiplataforma.

## 6. Correcciones a los docs (al ejecutar)
- **T-D1**: en `17c` §1, `core/scene_graph.h` → `scene/scene_graph.h`; `render/vulkan/` no existe (es `render/vulkan_core.cpp` / `render/vulkan_pipeline.cpp`).
- **T-D2**: en `17c` §11, los 130 MB son del renderer SDF (manifiesto `motor gráfico.txt`), no de GTE→TB.
- (T-D3 retirada: ABI 1×8 ya existe en `contrato/rih.h`.)

## 7. Mapa de condicionales
- M9 (Node Graph) → sin T-F1.2 no hay AST runtime.
- M19/M20 (luz/AO reales) → sin T-201 siguen *ghosted*.
- M29/M36/M34 → sin T-F2 (include `contrato/`) no hay tensores compartidos.
- M23/M33 → sin motor de systems no hay Play real.
- T-110/T-111/T-113/T-115 → sin T-F1.2 no hay live-compile.

## 8. Estado
- ✅ docs/18 creado.
- ✅ T-F5: ImGui vendored en `external/imgui` (fijado a tag `v1.91.9-docking` para tener API clásica de Vulkan + docking). `setup_deps.bat` lo clona.
- ✅ T-F1.1: `build/libherm.lib` reconstruido con `/MD` + `herm_compile.cpp` (orquestador `compileFromString`/`compile` IMPLEMENTADO: Lexer→Parser→Resolver→writeRihJson/Binary; `herm.h` tenía solo stubs). Se vendorizó la fuente real (repo hermano `lenguaje-hermetico`) y se añadió retrocompat al parser (material `base_color`→tensor rgb, light `type: directional/point`). **El compilador FUNCIONA**: `compileFromString` compila y escribe `.rih` para TODOS los ejemplos (`bodegon`, `luces`, `sdf_expr`, `def_let`, `compound`). `f64` no definido en headers de herm (se define en `herm_compile.cpp` como `double`). API: `herm::compileFromString(src,cfg)`, `herm::writeRihJson/Binary`.
- ✅ T-F6: `ImGui_ImplVulkan` integrado en `vk_ctx` (initImGui/shutdownImGui/createImguiFramebuffers + overlay en `drawFrame`); `visor_app.cpp` dockspace + paneles; `window_manager.cpp` reenvía input. Compila/enlaza (Build OK). Pendiente verificación visual en GPU.
  - Decisión: ImGui fijado a `v1.91.9-docking` porque `docking` HEAD usa dynamic rendering (rompe API clásica) y release `v1.91.9` no trae docking. `WndProcHandler` requiere forward-declaration (está en `#if 0` en el header). `CreateFontsTexture()` sin arg; `DestroyFontsTexture()` (no `DestroyFontUploadObjects`).
- ✅ T-F1.2: Compilador hermético funciona (ver T-F1.1). Puente `herm::Rih` (tensor 1×8) → `mg::Scene` (PBR) implementado en `core/herm_bridge.cpp` + cableado en el editor. **Puente `herm::Rih → mg::OntScene` (GPU/Vulkan)** implementado: compilador bytecode SdfNode→OntOpcode, constructor BVH, conversor material, ensamblaje OntScene binario en memoria. Verificado con `tools/test_ont_bridge.cpp` (todos los ejemplos). **T-F1.2 COMPLETO** (CPU + GPU paths).
- ✅ T-111: Code Editor `.herm` implementado en `visor/herm_editor.h/.cpp`: toolbar (New/Open/Save/Save As/Compile), line numbers gutter, syntax highlighting por keywords (colores VS Code), error markers inline, Ctrl+S para compile. Wire en `visor_app.cpp` con compile callback estático. Build OK con `comdlg32.lib` + `shell32.lib`.
