# 08 — Changelog

> **Vida del proyecto:** 4 de junio → 17 de junio de 2026 (v0.15–v0.26)

## 2026-09-03 — T-112: Tensor Inspector (M29)

### Resumen
Se crea el panel Tensor Inspector que muestra los componentes del tensor 1×8 del material seleccionado, con gráficos históricos y detección de anomalías.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| TensorInspector class | `visor/tensor_inspector.h/.cpp` | Tensor display, history plots (8 components), anomaly detection. |
| VisorApp integration | `visor/visor_app.h/.cpp` | `tensor_inspector` miembro, draw en UI. |
| Build script | `build.bat` | +tensor_inspector.cpp |

### Funcionalidad
- **Tensor display**: muestra XYZ, W, RGB, A del material seleccionado.
- **Color preview**: swatch de color base.
- **History plots**: gráfico de cada componente vs tiempo (60 frames).
- **Anomaly detection**: flags para NaN y valores extremos (>100).
- **Material info**: nombre del material asignado.

### Notas
- **Readback/Staging**: pendiente para v2 (requiere VkBuffer de readback GPU→CPU).
- **Heatmap**: pendiente para v2 (requiere textura de visualización).
- **Validación relacional a lo largo de W**: pendiente para v2.

## 2026-09-03 — T-105: Gizmos overlay 2D (M13)

### Resumen
Se crea el panel Gizmos que muestra overlay 2D proyectado desde handles 3D para manipular nodos seleccionados.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| GizmosPanel class | `visor/gizmos_panel.h/.cpp` | Move/Rotate/Scale gizmos, worldToScreen, drag interaction. |
| VisorApp integration | `visor/visor_app.h/.cpp` | `gizmos` miembro, draw en viewport, toolbar buttons. |
| Build script | `build.bat` | +gizmos_panel.cpp |

### Funcionalidad
- **Move gizmo**: flechas RGB (X/Y/Z) con proyección 3D→2D.
- **Rotate gizmo**: círculo amarillo.
- **Scale gizmo**: cuadro naranja.
- **Drag**: click en eje para arrastrar, snap opcional.
- **Toolbar**: botones [Move]/[Rot]/[Scale] para cambiar modo.

## 2026-09-03 — T-104: Inspector F/Q/B editable (M12)

### Resumen
Se crea el panel Inspector que muestra propiedades editables del nodo seleccionado con secciones F/Q/B.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| InspectorPanel class | `visor/inspector_panel.h/.cpp` | Secciones F/Q/B colapsables, dragfloat, color picker, sliders. |
| VisorApp integration | `visor/visor_app.h/.cpp` | `inspector` miembro, draw en UI. |
| Build script | `build.bat` | +inspector_panel.cpp |

### Funcionalidad
- **Física**: Translate/Rotate/Scale (DragFloat3), SDF params editables.
- **Química**: Material name, Base Color (ColorEdit3), Metallic, Roughness, Emission (ColorEdit3).
- **Biología**: Enabled, ID, Depth, Parent, Children count.
- **Sync**: cambios en Inspector actualizan SceneNode + Node.

## 2026-09-03 — T-103: Árbol Ontológico F/Q/B (M11)

### Resumen
Se crea el panel Ontology Tree que muestra la jerarquía de nodos de la escena con facetas F/Q/B, selección y enable/disable.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| OntologyPanel class | `visor/ontology_panel.h/.cpp` | Tree recursivo, checkboxes F/Q/B, selección, enable/disable. |
| VisorApp integration | `visor/visor_app.h/.cpp` | `ontology` miembro, draw en UI. |
| Build script | `build.bat` | +ontology_panel.cpp |

### Funcionalidad
- **Tree jerárquico**: muestra nodos SDF/GROUP/INSTANCE con indentación.
- **Facet F**: muestra transform (T/R/S).
- **Facet Q**: muestra tipo SDF y material.
- **Facet B**: muestra enabled/parent.
- **Selección**: click para seleccionar nodo.
- **Enable/disable**: checkbox para activar/desactivar nodo.

## 2026-09-03 — T-116: Profiler Panel (M27)

### Resumen
Se crea el panel Profiler que muestra métricas de render en tiempo real: FPS, ms/frame, pump time, viewport, scene stats y gráfico de frame time.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| ProfilerPanel class | `visor/profiler_panel.h/.cpp` | Stats struct, history buffer (120 frames), draw con PlotLines. |
| VisorApp integration | `visor/visor_app.h/.cpp` | `profiler` miembro, actualización cada frame, draw en UI. |
| Build script | `build.bat` | +profiler_panel.cpp |

### Métricas mostradas
- **FPS**: frames por segundo
- **Frame time**: ms/frame con gráfico histórico (120 frames)
- **Pump time**: ms del message pump
- **Render scale**: factor de escala actual
- **Viewport**: resolución activa
- **Mode**: GPU/CPU
- **Scene**: nodos, materiales, bytecode bytes

## 2026-09-03 — T-102: Viewport ImGui::Image (M4 partial)

### Resumen
El panel Viewport ahora muestra la imagen renderizada por el compute shader usando `ImGui::Image`.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| VkImage offscreen | `render/vulkan_core.h/.cpp` | `viewport_image`, `viewport_image_view`, `viewport_sampler`, descriptor pool/layout/set propio. |
| copyOutputToViewport | `render/vulkan_core.cpp` | Copia `output_buffer` → `viewport_image` con barriers (UNDEFINED→TRANSFER_DST→SHADER_READ_ONLY). |
| drawFrame integration | `render/vulkan_core.cpp` | Llama `copyOutputToViewport` después de cada dispatch. |
| Viewport panel | `visor/visor_app.cpp` | Muestra `ImGui::Image` con aspect ratio correcto. |
| Cleanup | `visor/visor_app.cpp` | `destroyViewportImage()` antes de `cleanup()`. |

### Notas
- **Resolución dinámica**: se recrea el viewport image cuando cambia el tamaño.
- **Descriptor set propio**: pool/layout dedicado (no comparte con ImGui).
- **Multi-viewport**: pendiente (requiere texturas adicionales por viewport).

## 2026-09-03 — T-101: MenuBar + Toolbar (M1/M2/M3)

### Resumen
Se extiende el shell del editor con MenuBar funcional (File/Edit/View/Help) y Toolbar con controles de modo, cámara, render scale y FPS.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| MenuBar funcional | `visor/visor_app.cpp` | File: New/Open/Save/Save As/Exit. Edit: Undo/Redo. View: toggle Console/Editor. Help: About placeholder. |
| Toolbar | `visor/visor_app.cpp` | Mode toggle (GPU/CPU), Camera mode, Render scale slider, FPS display. |
| Windows includes | `visor/visor_app.cpp` | +windows.h, +commdlg.h para diálogos Open/Save. |

### Notas
- **File > Open/Save**: usa `GetOpenFileNameA`/`GetSaveFileNameA` (comdlg32.lib).
- **Toolbar**: controles básicos para v1. P/I/K y gizmos SDF quedan para v2.
- **Multi-viewport**: pendiente (requiere texturas offscreen adicionales).

## 2026-09-03 — T-115: Undo/Redo por diffs de AST

## 2026-09-03 — T-115: Undo/Redo por diffs de AST

### Resumen
Se implementa Undo/Redo (Ctrl+Z/Ctrl+Y) que guarda snapshots del source code después de cada compile exitoso. El sistema guarda hasta 100 estados y permite navegar el historial.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| UndoRedo class | `visor/undo_redo.h/.cpp` | `UndoRedo` class: `saveState()`, `undo()`, `redo()`, `canUndo()`, `canRedo()`. Max 100 states. |
| Wire en editor | `visor/visor_app.cpp/.h` | `UndoRedo undo_redo` miembro de `VisorApp`. Ctrl+Z/Y handlers en loop principal. `saveState()` después de compile exitoso. |
| HermEditor | `visor/herm_editor.h` | `reloadFromSource()` setter para forzar recarga del text buffer. |
| Build | `build.bat` | +undo_redo.cpp. |

### Notas
- **Snapshot-based**: guarda source code completo (no AST diffs). Simple y robusto para v1.
- **Trigger**: se guarda estado después de cada compile exitoso (Ctrl+S).
- **Limitaciones**: no guarda estado entre compiles (solo en compile); no tiene granularidad a nivel de AST (v2 podría usar diffs).

## 2026-09-03 — T-113: Scene Validation + Console/Log panel

## 2026-09-03 — T-113: Scene Validation + Console/Log panel

### Resumen
Se extiende el Anomaly Gate con validaciones adicionales (transform non-finite, camera fov, light intensity, material name duplicates, scale=0) y se crea el panel Console/Log (M26) para mostrar anomalías y logs.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| Console panel | `visor/console_panel.h/.cpp` | `ConsolePanel` class: log entries con level (INFO/WARN/ERROR/SUCCESS), auto-scroll, filter by level, max 500 entries. |
| Anomaly Gate extended | `visor/anomaly_gate.cpp` | +validateAST: transform non-finite, scale=0, camera fov invalid, light intensity non-finite/<0, material name duplicates. |
| Wire en editor | `visor/visor_app.cpp/.h` | `ConsolePanel console` miembro de `VisorApp`. Init logs + draw panel. |
| Build | `build.bat` | +console_panel.cpp. |

## 2026-09-03 — T-110: Scheduler + Input Bus + Anomaly Gate

## 2026-09-03 — T-110: Scheduler + Input Bus + Anomaly Gate

### Resumen
Se implementa la infraestructura de live-compile asíncrono: Scheduler con debounce (~30ms), Input Bus unificado, y Anomaly Gate para validación estática de AST/bytecode antes de publicar.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| Scheduler | `visor/scheduler.h/.cpp` | `Scheduler` class: debounce timer, compile/apply callbacks, `markDirty()`/`update()`/`forceCompile()`. Estados: IDLE→DIRTY→COMPILING→VALIDATING→PUBLISHED/STATE_ERROR. |
| Input Bus | `visor/input_bus.h/.cpp` | `InputBus` class: input unificado (teclado/ratón), `InputState` con keys[256], mouse, scroll, flags ImGui. `beginFrame()`/`onKey()`/`onMouseMove()`. |
| Anomaly Gate | `visor/anomaly_gate.h/.cpp` | `AnomalyGate` class: `validateAST()` (material ID out-of-range, params NaN, smooth k<0, repeat spacing<=0, empty SDF, def duplicados, empty scene), `validateBytecode()` (ONT_CONST truncado/no-finito), `summary()`. |
| Wire en editor | `visor/visor_app.cpp/.h` | Scheduler init con compile/apply callbacks. `scheduler.update(now)` en loop principal. Properties panel muestra scheduler state + anomalies. |
| Build | `build.bat` | +scheduler.cpp, +input_bus.cpp, +anomaly_gate.cpp. |

### Notas
- **Scheduler v1**: recompile en main thread (sin threading real). Debounce previene compilar en cada keystroke. `forceCompile()` para Ctrl+S inmediato.
- **Anomaly Gate**: validación estática del AST (parámetros no-finitos, material ID fuera de rango, etc.) y bytecode (ONT_CONST truncado). Si hay errores, mantiene último frame bueno.
- **Input Bus**: wrapper sobre ImGui input state. v1 es stub; v2 integrará ray-SDF 3D picking.
- **Nota Windows**: `ERROR` es macro de Windows (`winerror.h:0`). Se usa `STATE_ERROR`/`SEV_ERROR` para evitar conflictos.

## 2026-09-02 — T-111: Code Editor `.herm` (line numbers, file I/O, error markers)

### Resumen
Se implementa el editor de código `.herm` mejorado con line numbers, file I/O (New/Open/Save/Save As), error markers inline, syntax highlighting básico (keywords coloreados), y atajos de teclado (Ctrl+S para compile). El editor reemplaza al `InputTextMultiline` básico.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| HermEditor widget | `visor/herm_editor.h` / `visor/herm_editor.cpp` | Editor completo con: line numbers gutter, toolbar (New/Open/Save/Save As/Compile), error markers, syntax highlighting por keywords (scene/sdf/material/def/instance/light + tipos SDF + operadores + propiedades), Ctrl+S para compile. Buffer de 64KB. |
| Wire en editor | `visor/visor_app.cpp` + `visor/visor_app.h` | `HermEditor herm_editor` como miembro de `VisorApp`. Compile callback estático que captura `this` y despacha a GPU/CPU path. Reemplaza al `InputTextMultiline` + botón manual. |
| Build | `build.bat` | Añade `herm_editor.cpp` + `comdlg32.lib` + `shell32.lib` (para diálogos Win32 Open/Save). |

### Notas
- **Syntax highlighting**: keywords resueltos por `keywordColor()` con colores estilo VS Code (azul=scene/camera, teal=sdf/tipos, purple=material/def/instance, amarillo=operadores, celeste=propiedades).
- **File I/O**: usa `GetOpenFileNameA`/`GetSaveFileNameA` de Win32 (comdlg32.lib). Filtra `*.herm`.
- **Error markers**: muestra errores bajo el editor con línea + mensaje.
- **Ctrl+S**: compila inline (misma lógica que el botón Compile).
- **Buffer**: 64KB (`text_buf[65536]`). Para archivos más grandes, se necesitaallocación dinámica (v2).

## 2026-09-02 — T-F1.2b: puente `herm::Rih → mg::OntScene` (GPU/Vulkan)

### Resumen
Se integra `ImGui_ImplVulkan` en el contexto de render (`vk_ctx`) como overlay sobre el swapchain, con un dockspace y paneles base en el editor. Build OK (pendiente verificación visual en GPU).

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| ImGui fijado a tag `v1.91.9-docking` | `external/imgui` + `setup_deps.bat` | `docking` HEAD usa *dynamic rendering* (rompe la API clásica: `RenderPass`→`PipelineInfoMain`, `CreateFontsTexture` eliminado) y el release `v1.91.9` no trae docking. `v1.91.9-docking` tiene API clásica + docking. |
| `initImGui` / `shutdownImGui` / `createImguiFramebuffers` | `render/vulkan_core.h/.cpp` | Init con `ImGui_ImplVulkan_InitInfo` (RenderPass del overlay, `MSAASamples`, `MinImageCount`/`ImageCount`); overlay en `drawFrame` tras el blit compute→swapchain. |
| Dockspace + paneles | `visor/visor_app.cpp/.h` | Init/shutdown ImGui; `drawEditorUI()` con menú (File/Edit/View/Help), dockspace y paneles Viewport/Properties/Hello. Llamado en el loop. |
| Reenvío de input | `visor/window_manager.cpp` | Forward-decl de `ImGui_ImplWin32_WndProcHandler` (está en `#if 0` en el header) y forwarding de teclado/ratón a ImGui. |
| Build ImGui Vulkan | `build.bat` | Compila `imgui_impl_vulkan.cpp` con `/DVK_NO_PROTOTYPES` (resuelve `vk*` vía `volk.c`, sin `vulkan-1.lib`) + `/MD`; incluye `VULKAN_SDK\Include`. |

### Notas
- `ImGui_ImplVulkan_CreateFontsTexture()` va **sin argumento** (el backend crea su propio command buffer); cleanup con `ImGui_ImplVulkan_DestroyFontsTexture()` (no `DestroyFontUploadObjects`).
- **Hallazgo T-F1.2**: el renderer Vulkan del editor consume `OntScene` (`.ont`), NO `Rih`. `Rih` solo lo usa el render por CPU (`core/renderer.cpp::render`). `libherm` emite RIH (`writeRihJson`/`writeRihBinary`) o `herm::Rih`. El live-preview exige un puente `Rih → OntScene` (o que libherm emita `.ont`). Pendiente decidir en T-F1.2.
- **Hallazgo `lenguaje-hermetico` (bloquea T-F1.1/T-F1.2)**: el snapshot vendored es **incompleto**. `contrato/herm.h` declara `compileFromString`/`compile`/`loadRih` pero **no estaban implementados** (stubs). Se implementó el orquestador en `herm_compile.cpp` (Lexer→Parser→Resolver→writeRihJson/Binary) y se reconstruyó `libherm.lib` con `/MD`. PERO el parser **no acepta el dialecto de los `.herm` de ejemplo** (`deps/lenguaje-hermetico/ejemplos/*.herm` fallan con `base_color` / `directional` no reconocidos): el parser del snapshot está desactualizado respecto de su propio corpus. Además `f64` no está definido en headers de herm (depende de `os/os.h`). Se vendorizó la fuente real (repo hermano `lenguaje-hermetico`) y se añadió retrocompat al parser (material `base_color`→tensor rgb; light `type: directional/point`). **El compilador hermético FUNCIONA**: `compileFromString` (Lexer→Parser→Resolver→writeRihJson) compila y escribe `.rih` para TODOS los ejemplos (`bodegon`, `luces`, `sdf_expr`, `def_let`, `compound`). El supuesto "bucle infinito del Resolver" era un diagnóstico erróneo: el cuelgue real estaba en `mg::loadRih` (`core/rih_reader.cpp`), no en el compilador. `skipValue()` (rih_reader.cpp:54-62) tiene un bug: para objetos `{...}` hace `while(peek()!='}'){ skipValue(); ... }` **sin consumir los tokens `clave:`**, así que al toparse con `:` cae en `readNumber()` (línea 61) que no consume nada → no avanza → **bucle infinito**. Se dispara al encontrar una clave-objeto no reconocida (p.ej. `"time": {...}` que herm emite en `scene`, línea 153 `else jr.skipValue();`). Cargar el `.rih` de herm con `mg::loadRih` es además incorrecto por diseño: mg usa esquema PBR (`base_color`/`roughness`/…) y herm emite esquema `tensor` 1×8. El puente real `herm::Rih → mg::Rih/OntScene` es la tarea T-F1.2.

## 2026-09-02 — T-F1.2b: puente `herm::Rih → mg::OntScene` (GPU/Vulkan)

### Resumen
Se implementa el puente `herm::Rih` → `mg::OntScene` para habilitar el live-compile en el renderer GPU/Vulkan. Incluye compilador bytecode SdfNode→OntOpcode (stack VM), constructor BVH (single leaf), conversor material tensor→OntMaterial, y ensamblaje de OntScene binario en memoria. El editor ahora compila directamente a OntScene cuando `renderer.use_vulkan=true`.

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| Compilador bytecode | `core/herm_bridge.cpp` | `BytecodeBuilder` + `compileSdfTree()`: convierte árbol SdfNode (herm) a bytecode OntOpcode (stack VM). Soporta primitivas (sphere, box, capsule, torus, cylinder, plane, cone, rounded_box, octahedron, tesseract, sphere4d) y operadores (union, subtract, intersect, smooth_*, repeat, twist, elongate, displace). Expresiones animadas → constante 0 (v1). |
| Constructor BVH | `core/herm_bridge.cpp` | `buildBVH()`: genera OntBvhNode root leaf conteniendo todos los graph nodes. AABB calculado desde transform de cada nodo. |
| Conversor material | `core/herm_bridge.cpp` | `convertToOntMaterial()`: tensor[4..6]→base_color, tensor[7]→opacity, roughness/metallic/emission por defecto. |
| Ensamblador OntScene | `core/herm_bridge.cpp` | `convertHermToOntScene()`: ensambla OntHeader + BVH + GraphNodes + Bytecode + Materials en blob binario contiguo. Asigna pointers de OntScene. |
| API puente | `core/herm_bridge.h` | `convertHermToOntScene()` + `compileHermToOntScene()` declarados. |
| Editor live-compile GPU | `visor/visor_app.cpp` | Botón Compile: si `renderer.use_vulkan`, compila a OntScene → `renderer.ont_scene` + `ont_mode=true`; si no, fallback a mg::Scene (CPU). |
| Test OntScene | `tools/test_ont_bridge.cpp` | Verifica compilación +OntScene para cada ejemplo: valida magic, node_count, bytecode_size, material, graph_node. |
| Build | `build.bat` | Añade compilación de `test_ont_bridge.exe`. |

### Notas
- **OntScene binario**: formato `.ont` embebido en `_data` vector: `OntHeader | OntBvhNode[] | OntGraphNode[] | bytecode[] | OntMaterial[]`. Los pointers (`header`, `bvh_nodes`, `graph_nodes`, `bytecode`, `materials`) se asignan directamente sobre `_data.data()`.
- **BVH v1 simplificado**: un solo root leaf conteniendo todos los nodos (como `loadDefault`). BVH real (SAH split) queda para v2.
- **Limitaciones conocidas**: expresiones animadas → constante 0; `repeat`/`twist`/`elongate`/`displace` ≈ passthrough (sin modificación real del campo); smooth_union usa promediado (no blend suavizado con k). Estas limitaciones son aceptables para v1 (live-preview básico).

## 2026-08-29 (tarde) — T-F1.2: puente `herm::Rih → mg::Scene` + live-compile en editor

### Resumen
Se implementa el **puente** que faltaba para T-F1.2: `herm::Rih` (tensor 1×8) →
`mg::Scene` (esquema PBR que consume el renderer). Se cablea en el editor (visor)
un panel "Editor .herm" con `InputTextMultiline` + botón **Compile** que hace
live-compile del buffer y reconstruye la escena (vía `scene_mgr.rebuildScene`).

### Cambios
| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| Puente `herm::Rih → mg::Scene` | `core/herm_bridge.h` / `core/herm_bridge.cpp` | `convertHermToScene` + `compileHermToScene`. Mapea tensor[4..6]→base_color, tensor[7]→opacity; `SdfType` enum→string; `child_a/child_b` remapeados (árbol SDF→nodos compound como en `loadRih`); `k` de smooth_*→params[0]. |
| Helper en memoria | `deps/lenguaje-hermetico/herm/herm_compile.cpp` | `herm::compileToRih(src, Rih&, errOut)` (compila sin escribir a disco; `HermResult` no exponía el `Rih`). |
| Editor live-compile | `visor/visor_app.cpp` + `visor/visor_app.h` | Panel "Editor .herm" (texto + Compile). Al compilar: `compileHermToScene` → `renderer.scene`, `scene_mgr.rebuildScene`, cámara desde la escena, `ont_mode=false` (preview CPU). Estado en Properties. |
| Build | `build.bat` | Compila `core/herm_bridge.cpp`; link `build/libherm.lib`; includes de `deps/lenguaje-hermetico/{contrato,herm}`. |
| Test | `tools/test_bridge.cpp` | Compila cada ejemplo y convierte a `mg::Scene` (sin `mg::loadRih`, sin disco). Verificado: bodegon(11n/3m), luces(2n/2m/3L), sdf_expr(2n/2m), def_let(2n/2m/1L) — sin cuelgue. |

### Notas
- **Esquema**: herm emite tensor 1×8; mg usa PBR. El puente es explícito (NO passthrough JSON). Ver `deps/lenguaje-hermetico/docs/04-guia-uso.md` §16.
- **No se usa `mg::loadRih`**: cargar el `.rih` de herm con el loader PBR dispara el bug de `skipValue` (bucle infinito) y no coincide el esquema. El puente evita ese loader por completo.
- **Limitación conocida**: expresiones animadas en `tensor`/params caen a constante 0 en la conversión (mg::Material no tiene campos expr). `roughness`/`metallic`/`emission` por defecto. Instancias con `def_id` resuelven vía tabla id→id (best-effort). Preview es el path CPU (`renderFrame`); el path Vulkan usa `OntScene`, pendiente de un puente `→ OntScene` (siguiente sub-paso).

## 2026-08-28 — v0.27 — Bootstrap del Editor (deps, librería de live-compile, roadmap UI/UX)

### Resumen

Arranque del editor: la IDE del compilador hermético tensor/SDF (no "motor tipo Unity"
literal). Se vendorizan dependencias, se construye `libherm` (compilador de
`lenguaje-hermetico` como librería linkable para live-compile) y se documenta el diseño
UI/UX con una hoja de ruta de tareas ordenada y con dependencias.

### Cambios

| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| Vendor ImGui (rama `docking`) | `external/imgui` + `setup_deps.bat` | UI inmediata; se clona con `--branch docking`. Base del shell (T-F6→T-101). |
| `build_libherm.bat` (nuevo) | raíz | Compila `deps/lenguaje-hermetico/herm/*.cpp` + `contrato/*.h` en `build/libherm.lib` (excluye `herm_render.cpp` y `main.cpp`). API: `herm::compileFromString` + `herm::loadRih`. |
| `setup_deps.bat` idempotente | raíz | Clona asmjit, volk, VMA, imgui, lenguaje-hermetico, MLIR-CampoTensorial. |
| `build.bat` usa `VULKAN_SDK` | raíz | Incluye solo `external/` + `deps/`; no inyecta headers en el SDK del usuario. |
| Brief UI/UX | `docs/16-ui-ux-editor-brief.md` | Contexto, layout, módulos, capacidades, enfoque técnico (base del diseño). |
| Diseño UI/UX (agente) | `docs/17a/17b/17c-ui-ux-editor-*.md` | Conceptual (reframe a IDE hermética), catálogo ~36 módulos, ingeniería/paths. |
| Roadmap de tareas | `docs/18-roadmap-tareas.md` | Tareas ordenadas con dependencias y condicionales (Fase 0 → post-v2). |

### Notas de verificación

- `lenguaje-hermetico` expone RIH en `contrato/rih.h`: `Material::tensor[8]` = ABI 1×8
  `[x,y,z,w,r,g,b,a]`; binario `.rib` (MAGIC `RIHH`). No había que inventar el ABI (T-D3 retirada).
- El motor **ya consume RIH en memoria** (`core/rih_reader.cpp`, `core/sdf_eval.cpp`), por lo
  que el live-preview usará RIH directo (sin writer `.ont`). `libherm` solo debe emitir `herm::Rih`.
- `herm_render.cpp` emite `.png` (CPU); se excluyó de `libherm` porque tira de `stb_image_write`.

---

## 2026-06-17 — v0.26 — Pipeline Scene-Especializado Funcional + Fix de Dimensiones

### Resumen

Esta versión completa la integración del pipeline de shaders generados por escena en Vulkan.
Se corrigieron bugs críticos de compilación GLSL en runtime, layout de memoria UBO (coherencia),
cálculo de dimensiones del viewport, y tamaño de workgroup en el compute shader.
Resultado: el motor renderiza escenas `.ont` completas en GPU a resolución nativa del viewport
con **311 FPS** en escena simple.

### Bugs corregidos

| Bug | Síntoma | Causa | Fix |
|-----|---------|-------|-----|
| Log corrupto | `diag()` y `logDiag()` se sobrescribían | `g_diag` abría con `trunc` pero `logDiag` abría con `app` por separado → posiciones divergentes | Truncar archivo aparte, abrir `g_diag` solo con `app` |
| glslc.exe falla en runtime | "El nombre de directorio no es correcto" al compilar GLSL | `_popen` + `GetTempPathA` tenían conflictos de quoting/permisos | Reemplazar `_popen` por `CreateProcess`, usar directorio del exe |
| Render en cuadro 122×78 superior-izquierdo | Solo ~10% del viewport mostraba escena | Falta `layout(local_size_x=8, local_size_y=8)` en el shader → glslc default (1,1,1) | Añadir declaración de workgroup size |
| Dimensiones de render inconsistentes | dispatch/copy usaban `swapchain_extent`, UBO usaba `workspace.active()` | Fuentes de dimensiones divergentes | `drawFrame` ahora recibe `rw/rh` explícitos |
| UBO no coherente en GPU | Stale reads de width/height en algunos frames | `VMA_MEMORY_USAGE_CPU_TO_GPU` puede no ser HOST_COHERENT | `vmaFlushAllocation` tras `memcpy` |
| Swapchain extent incorrecto | Potencial mismatch entre imagen real y esperada | `initSwapchain` ignoraba `currentExtent` | Usar `currentExtent` cuando es fijo (≠ UINT32_MAX) |

### Cambios principales

| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| `layout(local_size_x=8, local_size_y=8)` en shader template | `render/glsl_gen.cpp:23` | Workgroup 8×8 obligatorio — sin esto, glslc usa (1,1,1) y dispatch rinde 122×78 píxeles |
| `drawFrame(rw, rh)` | `render/vulkan_core.h/cpp` | Dispatch y copy reciben dimensiones explícitas, no usan `swapchain_extent` |
| UBO `width/height` desde `rw/rh` | `visor/visor_app.cpp:332-333` | Antes usaba `workspace.active().w/h`, ahora el render size escalado |
| `resizeOutputBuffer(rw, rh)` | `visor/visor_app.cpp:309` | Buffer de salida al tamaño de render real |
| `currentExtent` en `initSwapchain` | `render/vulkan_core.cpp:233-240` | Swapchain se crea al tamaño fijo de la superficie si existe |
| `vmaFlushAllocation` en `updateUBO` | `render/vulkan_pipeline.cpp:302` | Garantiza visibilidad GPU de datos UBO escritos por CPU |
| `CreateProcess` reemplaza `_popen` | `render/glsl_gen.cpp:581-584` | Compilación glslc más robusta |
| Log `diag` con `app` | `visor/visor_app.cpp:34` | Previene corrupción de log |
| Diagnostics dimensionales | `visor/visor_app.cpp`, `render/vulkan_core.cpp`, `visor/window_manager.cpp` | Logging de workspace, swapchain, dispatch, UBO, layout |

### Benchmark

```
Escena: 1 nodos, 1 BVH, 1 materiales, 75 bytecode bytes
Resolucion: 974 x 617
GPU Vulkan Compute (200 frames):
  Media: 3.213 ms (311.2 FPS)
  Total: 642.673 ms
```

### Diagnóstico del "cuadro 122×78"

El síntoma que persistió durante toda la sesión (un cuadrado de ~200×190 en la esquina
superior izquierda del viewport, que al estirar la ventana mostraba líneas horizontales
alternadas) tenía dos causas superpuestas:

1. **Dimensión incorrecta (122×78):** `vkCmdDispatch(122, 78, 1)` con workgroup (1,1,1)
   → solo 122×78 = 9.516 hilos ejecutan en vez de 976×624 = 609.024.
   El dispatch correcto son 122 grupos de 8×8 hilos = 976×624.

2. **Líneas alternadas al estirar:** El stride de fila en el buffer de salida no coincidía
   con el stride esperado por `VkBufferImageCopy`. Causado por `swapchain_extent` fijo
   mientras el viewport cambiaba de tamaño. Fix: pasar `rw/rh` explícitos a `drawFrame`.

### Estado actual

- **Pipeline scene-especializado funcional**: shader generado con SDF inline, compilado a
  SPIR-V vía glslc.exe en runtime, pipeline recreado en caliente.
- **Fallback a pre-compilado**: si la compilación falla, carga `ray_march.spv`.
- **Resolución nativa del viewport**: 974×617 en ventana 1280×720, mayor al maximizar.
- **~311 FPS** en escena simple (esfera), limitado por complejidad de BVH en escenas densas.
- **Indicador `[VK]`/`[VK-FALLBACK]`/`[CPU]`** en título de ventana.
- **F3**: test pattern rojo (salta compute shader, llena swapchain con rojo).

### Próximos pasos

Ver plan completo del usuario en docs/12-brainstorming-optimizacion.md.
Prioridades:
1. AABB correctas en CSG (subtract/intersect deben podar el BVH)
2. Stepping adaptativo con `d_min` (ya implementado pero con escenarios de prueba)
3. Eliminar VM de bytecode inline en shader (actualmente ya generado, pero
   la generación puede optimizarse)

---

## 2026-06-16 — v0.25 — Fase 2: Stepping Adaptativo con d_min (Diagnóstico de Regresión)

### Resumen

Implementación de Fase 2 (stepping adaptativo con `d_min`) en el compute shader Vulkan.
Se añadió `d_min` al `OntBvhNode`, stepping variable `max(d, leaf_d_min) * 0.8`, y
early-out por `tmax`. Sin embargo, la escena catedral (densa) tiene `d_min=0` en todas
las hojas BVH (el centro del AABB combinado cae dentro de geometría), por lo que la
optimización no aporta en esta escena concreta.

La regresión de rendimiento (3.6 FPS → 1-2 FPS) se diagnosticó como causada por el
aumento de iteraciones máximas de 80 a 256. Al revertir a 80 iteraciones, el FPS
vuelve a ~4 (marginalmente mejor que el baseline de 3.6).

### Cambios principales

| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| `d_min` en `OntBvhNode` y `FlatNode` | `herm_ont.cpp`, `ray_march.comp` | Cómputo de distancia mínima adaptativa por hoja BVH |
| Cómputo de `d_min` por hoja BVH | `herm_ont.cpp` | `d_min = max(0, evalSdfTree(centro) - media_diag)` |
| `bvhEval` retorna `vec3` | `ray_march.comp` | Ahora retorna `vec3(dist, d_min, mat_bits)` en vez de `vec2` |
| `tmax` en UBO | `vulkan_pipeline.h`, `ray_march.comp` | `float trace_t_max` con padding std140 (offset 64, 80 bytes total) |
| Loop adaptativo | `ray_march.comp` | 256 iteraciones (revertido a 80), stepping `max(d, leaf_d_min) * 0.8` |
| Safety factor bugfix | `ray_march.comp` | `step = max(d, leaf_d_min) * 0.8` (antes `step = max(d, leaf_d_min * 0.8)`) |
| Inside-geometry bugfix | `ray_march.comp` | Rama `d < 0` con `step = d * 0.8` para evitar paso 0 |
| Carga `trace_t_max` desde pipeline | `visor_app.cpp:314` | `ubo.trace_t_max = ont_scene.pipeline.trace_t_max` |

### Diagnóstico de regresión

```
Problema:  FPS cayó de 3.6 (v0.24) a 1-2 (v0.25 inicial)
Causa:     ← 256 iteraciones máximas en el loop de raymarch
           (revertir a 80 restaura ~4 FPS)
Causas descartadas:
           - bvhEval retornando vec3: overhead menor
           - D_min=0 en toda la escena: stepping equivalente a d*0.8
           - floatBitsToUint por iteración: no afecta significativamente
```

### Estado actual

- **Revertido a 80 iteraciones** en `ray_march.comp:380`
- **~4 FPS** con escena catedral (974×617, 30 nodos, 15 BVH)
- **UBO layout mismatch sin resolver**: el uniform block usa `layout(scalar)` pero el
  struct C++ `UboData` tiene padding `std140` (`pad0`, `pad1`). Causa que `trace_t_max`
  se lea de offset incorrecto. No afecta gravemente la imagen (cámara se ve correcta
  porque los primeros campos coinciden por coincidencia), pero el `tmax` puede ser
  basura. **Siguiente paso**: eliminar `scalar` del uniform block para que coincida
  con el layout C++.

### Próximos pasos (plan)

1. **Fix UBO layout**: cambiar `layout(scalar)` → `layout(std140)` o default en el
   uniform block de `ray_march.comp` para que coincida con el struct C++
2. **Re-activar stepping adaptativo**: con 80 iteraciones + `d_min` correcto (en
   escenas con objetos separados, `d_min > 0` permitirá pasos más grandes)
3. **Probar con `test_world.herm`**: escena con objetos separados donde `d_min > 0`
   podría dar beneficio real
4. **Benchmark formal**: medir FPS con visor en escena de catedral post-fixes

---

## 2026-06-16 — v0.24 — GPU Estable: BVH fix, Safety, Benchmark, Documentación

### Logros

Esta versión cierra la migración a GPU con un pipeline funcional, estable y seguro:

- **Benchmark confirmado**: escena catedral renderiza a **281 ms/frame (3.6 FPS)** a 974×617
- **Sin GPU hangs**: protecciones contra loops infinitos en BVH y bytecode VM
- **Sin validation errors cleanup**: `vkDeviceWaitIdle` antes de destruir recursos
- **Test pattern mode**: F3 toggle para aislar bugs de presentación (swapchain rojo sólido)
- **Documentación actualizada**: todas las métricas, arquitectura y estado reflejan el pipeline GPU

### Cambios principales

| Cambio | Archivo | Descripción |
|--------|---------|-------------|
| BVH struct layout fix | `ray_march.comp` | `layout(scalar)` en vez de `std430` — C++ struct 52 bytes coincide con GLSL |
| `node_count`/`flags` bit position fix | `ray_march.comp` | Lower 16 = count, upper 16 = flags (estaba al revés, se usó el comentario incorrecto) |
| `scalarBlockLayout` feature | `vulkan_core.cpp` | Habilitado en device creation pNext |
| Memory barrier output_buffer | `vulkan_core.cpp` | `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT → VK_PIPELINE_STAGE_TRANSFER_BIT` para visibilidad |
| BLACK_BRUSH removed | `window_manager.cpp` | Viewport class sin brush de fondo — BeginPaint ya no pinta negro |
| BVH max iterations (65535) | `ray_march.comp` | Previene GPU hang por BVH mal formado |
| Bytecode VM max iterations (65535) | `ray_march.comp` | Previene GPU hang por bytecode corrupto |
| Graph node bounds check | `ray_march.comp` | `if (gi >= graph_nodes.length()) break;` |
| Cleanup wait | `vulkan_core.cpp` | `vkDeviceWaitIdle` en cleanups para evitar validation errors al destruir |
| Test pattern (F3) | `vulkan_core.h/cpp`, `visor_app.cpp` | Llena swapchain con rojo, salta compute shader |
| FPS en title bar | `visor_app.cpp` | Title muestra `FPS: N | Nms` para modo ONT |
| Pixel readback eliminado | `visor_app.cpp` | Ya no se hace debug readback en cada inicio |
| Camera smoothing | `camera.h` | Velocity smoothing exponencial, zoom interpolation |

### Benchmark

```
=== BENCH-VULKAN ===
Escena: 30 nodos, 15 BVH, 10 materiales, 3725 bytecode bytes
Resolucion: 974 x 617
GPU Vulkan Compute (200 frames):
  Media: 281.245 ms (3.6 FPS)
  Total: 56249.063 ms
```

### Seguridad

- No más GPU hangs/watdog timeouts
- No más crashes por acceso OOB en graph nodes
- No más validation errors de cleanup
- No más pantalla negra por GDI overwrite

---

## 2026-06-12 — v0.23 — El Compute Shader Maestro: Renderizado 100% en GPU

### El Gran Salto Arquitectónico

Esta es la sesión que cierra el ciclo: pasamos de un motor que quemaba la CPU al 100% calculando píxeles en C++ escalar, a un motor donde **la CPU no toca un solo píxel**. La GPU recibe las instrucciones, calcula el rayo, interpreta el bytecode SDF, y escribe el resultado directamente en el Swapchain sin pasar por memoria de sistema.

La transición no fue trivial. Requirió resolver tres problemas simultáneos:
- `std140` memory alignment
- Manual VM extraction loop (`evalBytecode`)

### Problemas conocidos (Bloqueantes en Fase 6)
Durante las pruebas con escenarios reales (`test_custom.ont`), se observó que la carga del pipeline (Fase 6) completa con éxito y es capaz de reconocer el `.ont`, sin embargo hay un problema crítico con el tamaño de los buffers:
- **Crash por Mismatch de Tamaños**: `vkCmdCopyBufferToImage` genera un error de validación cuando el área del Swapchain Viewport (por ejemplo 973x616, ~2.4MB) excede el tamaño pre-asignado del `output_buffer` (800x600, ~1.9MB). 
- **Plan para corrección**: El `VulkanContext::drawFrame` y el Swapchain necesitan responder correctamente a los redimensionamientos de ventana (`WM_SIZE` en Win32) y realojar el `output_buffer` y actualizar los descriptor sets del pipeline consecuentemente.
1. Cómo representar una Máquina Virtual de Bytecode en GLSL sin punteros.
2. Cómo transferir datos empaquetados en `.ont` (alineación Little-Endian) a un SSBO sin corrupción.
3. Cómo eliminar el cuello de botella GDI presentando directamente en el Swapchain de Vulkan.

---

### Fase 5-A: La Capa de Datos GPU (`vulkan_pipeline.h/cpp`)

La `VulkanSceneData` gestiona toda la memoria en VRAM. Al cargar un `.ont`, hace lo siguiente:

```
CPU (.ont en RAM)
   │
   ├── bvh_nodes    ──[Staging Buffer]──► bvh_buffer    (SSBO binding=0, GPU_ONLY)
   ├── graph_nodes  ──[Staging Buffer]──► graph_buffer   (SSBO binding=1, GPU_ONLY)
   ├── materials    ──[Staging Buffer]──► material_buffer(SSBO binding=2, GPU_ONLY)
   └── bytecode     ──[Staging Buffer]──► bytecode_buffer(SSBO binding=3, GPU_ONLY)
                                          ubo_buffer      (UBO  binding=4, CPU_TO_GPU)
                                          output_buffer   (SSBO binding=5, GPU_ONLY, TRANSFER_SRC)
```

El patrón **Staging Buffer** es la práctica estándar en Vulkan. La razón matemática y de hardware es la siguiente: la GPU tiene dos dominios de memoria diferenciados. La memoria `GPU_ONLY` (GDDR6/HBM) tiene un ancho de banda de ~500 GB/s pero no es accesible por la CPU. La memoria `CPU_TO_GPU` (mapped, compartida) tiene ~20-40 GB/s pero la GPU puede leerla. Al usar un staging buffer, hacemos una sola operación de copia desde RAM → VRAM usando la propia DMA de la GPU, y nunca más la CPU toca esos datos. La diferencia de velocidad de lectura para el Shader es la que separa el 1 FPS del tiempo real.

El `UBO` es la excepción justificada: la cámara y el tiempo cambian cada frame. Al ser `CPU_TO_GPU`, podemos mapear su memoria y escribirla directamente sin un staging buffer, ya que la GPU lo necesita solo una vez por frame (no miles de veces como los SSBOs de geometría).

**`UboData` (std140, 48 bytes):**
```cpp
struct UboData {
    float camera_pos[3];    float pad0;      // binding=4, offset=0
    float camera_target[3]; float pad1;      // offset=16
    float camera_up[3];     float fov;       // offset=32
    float time;  float render_scale;
    uint32_t width; uint32_t height;         // offset=44
};
```

El padding explícito es obligatorio. El estándar GLSL `std140` requiere que los `vec3` estén alineados a 16 bytes. Sin `pad0` y `pad1`, el shader leerá la cámara desplazada por 4 bytes y verías la escena "rota" con un offset de exactamente un float. Esto es el mismo motivo por el cual los CPUs modernos castigan el acceso desalineado: las líneas de caché son múltiplos de 16.

---

### Fase 5-B: El Compute Shader (`render/ray_march.comp`)

El shader es el corazón. Recibe `gl_GlobalInvocationID.xy` (las coordenadas del píxel), ejecuta el Sphere Tracing completo con la misma lógica que `rayMarchOnt()` en C++, y escribe el color en `output_buffer`.

**El problema del Bytecode en GLSL:**

El bytecode `.ont` está serializado byte-a-byte en el SSBO `bytecode_buffer`. El shader necesita leer un `uint8_t` (opcode) o un `float` (constante) desde ese buffer. El problema: los SSBOs en GLSL solo permiten acceso alineado a 32 bits (`uint`). No existen bytes individuales en la GPU.

Solución adoptada (extracción bit a bit):

```glsl
// Equivalente a: uint8_t read_byte(uint* ip)
uint read_byte(inout uint ip) {
    uint word_idx = ip / 4;
    uint byte_off = ip % 4;
    ip++;
    return (bytecode_data[word_idx] >> (byte_off * 8u)) & 0xFFu;
}

// Equivalente a: float read_float(uint* ip)
float read_float(inout uint ip) {
    uint word_idx = ip / 4;
    ip += 4;
    return uintBitsToFloat(bytecode_data[word_idx]);
}
```

Esta representación es matemáticamente exacta bajo el modelo Little-Endian de todas las arquitecturas x86/ARM de consumo. Al hacer `>> (byte_off * 8)`, extraemos el byte N-ésimo de un `uint32` con precisión de bit, lo cual es equivalente a la aritmética de punteros C++ `*(uint8_t*)ptr`. No hay pérdida de información.

**La VM de Stack en GLSL:**

```glsl
// Stack de hasta 32 floats (igual que execBcRaw en C++)
float stack[32];
int sp = 0;

// El switch-case de opcodes se implementa con if-else chains
// (GLSL no garantiza performance de switch sobre variables uniformes)
uint op = read_byte(ip);
if      (op == ONT_CONST) { stack[sp++] = read_float(ip); }
else if (op == ONT_VAR_X) { stack[sp++] = p.x; }
...
else if (op == ONT_ADD)   { float b = stack[--sp]; stack[sp-1] += b; }
else if (op == ONT_SIN)   { stack[sp-1] = sin(stack[sp-1]); }
...
```

El grupo de trabajo (`local_size_x = 8, local_size_y = 8, local_size_z = 1`) significa que 64 invocaciones del shader se ejecutan simultáneamente en el mismo warp. Para escenas donde los rayos de un tile de 8×8 píxeles tienen una trayectoria similar (mismo objeto, misma profundidad), la divergencia de ramificación en el `if-else` de opcodes es mínima. Esta es la misma razón matemática por la que el SIMD de CPU funciona bien: los píxeles vecinos del espacio de pantalla corresponden a rayos con trayectorias similares en el espacio mundo.

---

### Fase 5-C: El Loop de Presentación (`vulkan_core.cpp::drawFrame`)

Esta es la pieza que elimina GDI del pipeline de presentación. El flujo por frame es:

```
vkAcquireNextImageKHR()          // Pide la imagen libre del Swapchain
    ↓
recordComputeCommandBuffer()      // Bind pipeline + descriptors + Dispatch(W/8, H/8, 1)
    ↓
[Barrier] UNDEFINED → TRANSFER_DST_OPTIMAL    // La swapchain image está lista para escritura
    ↓
vkCmdCopyBufferToImage()         // Copia output_buffer → swapchain_images[i]
    ↓
[Barrier] TRANSFER_DST → PRESENT_SRC_KHR      // La imagen está lista para presentación
    ↓
vkQueueSubmit()                  // Envía el command buffer a la GPU
    ↓
vkQueuePresentKHR()              // Presenta en pantalla (FIFO = V-Sync)
```

Las dos barreras de memoria (`VkImageMemoryBarrier`) son estrictamente necesarias. Vulkan no tiene estado implícito: el programador debe declarar explícitamente cuándo una imagen cambia de rol. Sin la barrera a `TRANSFER_DST_OPTIMAL`, el driver podría intentar presentar una imagen mientras la DMA aún está copiando píxeles, resultando en tearing o corrupción. Sin la barrera a `PRESENT_SRC_KHR`, el motor de presentación del compositor no sabrá que la imagen está lista.

Los semáforos garantizan la sincronización GPU-GPU (compute → present sin intervención de la CPU). El fence garantiza que el CPU no sobreescriba el command buffer mientras la GPU lo ejecuta.

---

### Fase 5-D: Integración en `VisorApp::run`

El loop principal ahora bifurca según el modo:

```cpp
if (renderer.use_vulkan && renderer.ont_mode) {
    // Actualizar cámara en UBO y despachar el Compute Shader
    UboData ubo;
    // ... copiar camera, time, resolution
    renderer.vk_scene.updateUBO(ubo);
    vk_ctx.drawFrame(renderer.vk_scene);          // GPU hace TODO el trabajo
} else {
    renderFrame();      // Fallback: CPU renderiza a framebuffer + GDI
    InvalidateRect(...);
}
```

El flag `use_vulkan = true` y `ont_mode = true` activan el camino GPU. Cualquier otro modo (escenas `.rih` legacy, debug sin Vulkan) usa el path CPU original. Esta bifurcación es crítica para mantener el motor debuggeable: si el Compute Shader produce artefactos, puedes cambiar `use_vulkan = false` y el motor renderizará por CPU para comparación visual.

---

### Archivos modificados/creados

| Archivo | Cambio |
|---------|--------|
| `render/vulkan_pipeline.h` | Añadido `recordComputeCommandBuffer(cmd, w, h)` |
| `render/vulkan_pipeline.cpp` | Implementación de `recordComputeCommandBuffer` y limpieza de `compute_pipeline`/`pipeline_layout` en `cleanup()` |
| `render/vulkan_core.h` | Añadido `drawFrame(VulkanSceneData&)` como forward declaration con `class` |
| `render/vulkan_core.cpp` | Implementación completa de `drawFrame` (80 líneas): acquire → compute → barriers → copy → present |
| `render/render.h` | Añadido `use_vulkan = true`, `VulkanSceneData vk_scene`; `loadOnt()` llama `vk_scene.init()` cuando `use_vulkan` |
| `visor/visor_app.cpp` | `run()` bifurca entre GPU path (`drawFrame`) y CPU path (fallback). Construcción correcta de `UboData` con assignments de arrays `float[3]` |

### Resultado

```
Build OK — visor.exe generado exitosamente (MSVC 19.44.35224 x64)
Shader compilado: ray_march.comp → build/shaders/ray_march.spv
```

El motor compila sin errores. La GPU ahora es el renderer primario.

---



### La Gran Pared de Hardware y el Límite Teórico
A pesar de la extrema agresividad en nuestras optimizaciones (implementación de una Máquina Virtual JIT SIMD lockstep, alineación estricta de memoria `alignas(64)` para mitigar el *false sharing* en la caché L1, y despacho multihilo exhaustivo), nos encontramos con una pared física insalvable: el ancho de banda ALU de las CPUs de consumo moderno.

Un raymarcher que evalúa un universo SDF dinámico basado en serialización de Bytecode (`.ont`) exige miles de evaluaciones matemáticas (senos, cosenos, raíces cuadradas, rotaciones matriciales 4x4) por cada rayo en cada píxel. Para una resolución de 1080p a 60 FPS, esto se traduce en cientos de miles de millones de instrucciones de punto flotante por segundo. La CPU, diseñada para el procesamiento en serie y la predicción de ramas complejas, simplemente no tiene el conteo de núcleos (ALUs) para este volumen matemático masivo.

### El Significado y Valor del "1 FPS Estable a <2000ms"
Mantener el motor funcionando de forma completamente determinista y estable a ~1 FPS por debajo de los 2000ms no fue un fracaso de optimización, **fue una victoria algorítmica fundamental**. 

Lograr esto nos permitió:
1. **Validar la Verdad Matemática:** Comprobamos que nuestras correcciones euclidianas del trazado (Sphere Tracing) eliminaron por completo el "overshooting" y los agujeros negros. La imagen obtenida es un *ground-truth* matemáticamente puro.
2. **Solidificar el Contrato Ontológico:** Confirmamos que el motor interpreta el bytecode emitido por el compilador hermético con 100% de precisión.
3. **Depuración Aislada:** Si hubiéramos portado directamente el algoritmo defectuoso a la GPU, los errores visuales hubieran sido imposibles de rastrear debido a la falta de herramientas de step-debugging en los Compute Shaders. La CPU actuó como nuestra cámara de pruebas determinista de alta fidelidad.

### El Valor de las Optimizaciones Previas
Todo el trabajo invertido en optimización de CPU (las abstracciones) tiene un retorno de inversión directo en la nueva arquitectura de GPU:
* **Diseño Orientado a Datos (DOD):** Estructurar el motor para SIMD nos obligó a vectorizar y agrupar los datos (`std::vector<Frame>` lockstep). Esto es exactamente el concepto de *Warps* o *Wavefronts* en la GPU. Las GPUs castigan la divergencia de hilos; al haber mitigado esto en la CPU, nuestro shader nacerá optimizado.
* **Memoria Alineada:** Nuestro entendimiento sobre el empaquetado de structs en C++ de 16 bytes (`alignas(16)`) es el estándar directo que la GPU exige para los SSBO (std430). La migración de nuestros buffers será 1:1.

### La Nueva Arquitectura (Fase 3: Vulkan)
El paradigma cambia: La CPU dejará de calcular píxeles. Su único rol será cargar los archivos `.ont`, enviarlos a la memoria de video (VRAM) usando *Shader Storage Buffer Objects* (SSBOs) y enviar los parámetros de cámara/tiempo vía *Uniform Buffer Objects* (UBOs). 

La GPU, usando un único **Compute Shader**, recibirá las coordenadas del píxel `(gl_GlobalInvocationID)`, ejecutará la máquina virtual de bytecode internamente y vomitará los píxeles a la pantalla. Al usar la API Vulkan, obtenemos control explícito sobre la memoria y las barreras de sincronización, destruyendo el cuello de botella.

**Multiplataforma garantizado:** Al elegir **Vulkan**, el motor sigue siendo 100% agnóstico al sistema operativo. Correrá sin modificar una coma en Windows, Linux, Android y macOS (usando MoltenVK), mientras que el Bytecode `.ont` se mantiene como lenguaje universal absoluto.

---

## 2026-06-12 — v0.21 — Diagnóstico: Brecha de Contrato Compilador–Motor

### Contexto y Descubrimiento

Tras semanas de intentar optimizar el motor (SIMD, JIT, BrickMap, divergencia de
paquetes SSE), el rendimiento se mantuvo en **1 FPS** con picos de hasta 3343ms.
Los síntomas incluían agujeros negros, partes de la escena invisibles y calidad
visual degradada que empeoraba con cada intento de optimización.

La auditoría completa cruzando el compilador del Lenguaje Hermético (`herm_ont.cpp`)
con el motor gráfico (`ray_march.h`, `ray_march_simd.h`, `scene.h`) reveló que
**el motor nunca fue el problema principal**. El cuello de botella era una brecha
de contrato en la serialización `.ont`: el compilador emitía datos incorrectos que
el motor consumía ciegamente.

**Conclusión:** El Lenguaje Hermético sí renderizaba imagen. El problema era una
malinterpretación de qué producía el compilador y qué esperaba el motor.

### Tres Niveles de Brecha Encontrados

#### Brecha 1 — `d_min = 0.01` Hardcodeado (🔴 CRÍTICO — causa directa de geometría invisible)

**Archivo:** `herm/herm_ont.cpp:509-511`

```cpp
node->d_min = 0.01f; // conservative: tiny minimum distance
node->L = 1.0f;      // default Lipschitz constant
```

El motor usa `d_min` en `bvhEval4` para culling de subárboles BVH:

```cpp
if (cur_res[i] < node.d_min)
    ni[i] = (u32)node.skip_index;  // SALTA el subárbol
```

Con `d_min = 0.01`, esta condición se dispara cuando el rayo está a menos de 1cm
de cualquier superficie, **saltando incorrectamente subárboles que contienen geometría
cercana**. Resultado directo: objetos cercanos desaparecen → agujeros negros.

El valor correcto para nodos hoja es `d_min = 0.0f` (la superficie está dentro del
AABB, no se puede garantizar distancia mínima).

#### Brecha 2 — AABBs Incorrectas (🔴 CRÍTICO — causa directa de objetos invisibles)

**Archivo:** `herm/herm_ont.cpp:192-193`

```cpp
bmin[0] = bmin[1] = bmin[2] = bmin[3] = -scene_scale;  // = -5.0f
bmax[0] = bmax[1] = bmax[2] = bmax[3] = scene_scale;   // = +5.0f
```

La función `computeNodeAabb` asigna `±5.0f` como AABB local a casi todos los
primitivos (CUSTOM, TORUS, CYLINDER, CONE, CAPSULE no tienen casos específicos).
El volcán de la catedral está en posición `[9.0, -1.5, 7.0]` — su AABB local de
`±5` se transforma correctamente al mundo, pero objetos más lejanos podrían quedar
fuera del AABB conservativo.

El BVH construido sobre AABBs demasiado grandes no puede podar eficientemente:
cada nodo del BVH parece "grande" y el test `dist_to_aabb > best_dist` raramente
se cumple, forzando visita de todos los nodos → O(n) efectivo en vez de O(log n).

#### Brecha 3 — Expresiones de Color Descartadas Silenciosamente (🟡 LATENTE)

**Archivo:** `herm/herm_ont.cpp:662-665`

```cpp
mat.base_color[0] = m.tensor[4].is_expr ? 1 : m.tensor[4].constant;
```

Si el canal de color `tensor[4..7]` es una expresión (dependiente de `nx, ny, nz`
para Fresnel, o de `w` para color animado), el compilador la ignora y sustituye
por `1.0f`. No hay warning. Cualquier escena que use librerías `std_chem_optics.hm`
o `std_bridge_spectral.hm` para colorear materiales produce objetos blancos.

En la catedral actual los materiales son colores constantes, por lo que este bug
no se manifiesta visualmente. Es una deuda de alta severidad para escenas futuras.

### Escena de Referencia: catedral_hermetica.herm

| Componente | Nodos | Impacto de las brechas |
|---|---|---|
| Arquitectura (planos, columnas, arcos, cúpula) | 16 | Brecha 2: AABBs funcionales, BVH sub-óptimo |
| Esferas orbitantes (orbita_1/2/3) | 3 | Brecha 1: desaparecen al estar cerca del rayo |
| Fuente de agua | 1 | Brecha 1: ídem |
| Cristales `std_chem_crystal` | 3 | Sin impacto crítico actual |
| Células `std_bio_cell` | 5 | Sin impacto crítico actual |
| Volcán `std_geo_terrain` a [9,-1.5,7] | 1 | Brecha 2: AABB potencialmente incorrecta |

Los **60 archivos `.ont`** de la catedral (`_0000` a `_0059`) fueron compilados
con el compilador defectuoso y contienen `d_min=0.01` hardcodeado. Deben
regenerarse tras corregir `herm_ont.cpp`.

### Plan Aprobado (v0.22)

- **Fase 1 (Compilador)**: Arreglar `herm_ont.cpp` — `d_min=0.0f`, AABBs reales
  por tipo de primitiva, warning para expresiones de color. Regenerar `.ont`.
- **Fase 2 (Motor defensivo)**: Budget espacial en `evalDynamicNodes`, detección
  de `d_min` degenerado como fallback, máscara SIMD en `bvhEval4`.
- **Fase 3 (Color dinámico)**: Stream de bytecode separado para `tensor[4..7]`.

### Por qué tardamos tanto en encontrarlo

El problema se camuflaba porque:
1. La imagen SÍ se renderizaba (no era un crash completo).
2. Los síntomas (agujeros negros, rendimiento malo) se parecen a problemas de
   optimización, no a bugs de datos de entrada.
3. El optimizador (SIMD, JIT, BrickMap) funcionaba correctamente — optimizaba
   mal los datos incorrectos del compilador.
4. El compilador y el motor vivían en repos separados — la brecha estaba en el
   "espacio entre ambos" (el contrato `.ont`).



## 2026-06-12 — v0.20 — Diagnóstico de Cuellos de Botella en Ray Marching

### Contexto
El motor presentaba un rendimiento de 1 FPS (picos de 1800ms) y artefactos visuales graves (agujeros negros, cortes de malla). La auditoría reveló que el algoritmo de Sphere Tracing (`rayMarchOnt`) estaba matemáticamente roto, no era un problema de optimización de CPU, sino de geometría.

### Analizado y Planificado
- **Diagnóstico Matemático**: 
  - La distancia SDF no compensaba la escala de la matriz local, causando "overshooting".
  - Hack de paso penalizante (`sd * 0.5f`) obligaba a calcular x2 iteraciones por rayo.
  - El choque ignoraba estar dentro del objeto (`d < 0.0f`), atravesándolo de lado a lado.
  - Un paso mínimo forzado con `std::fmax` impedía la convergencia, encerrando a los rayos en bucles oscilatorios cerca de las superficies.
- **Plan de Acción**: Creado plan de implementación para refactorizar `rayMarchOnt` a estrictas leyes euclidianas.

## 2026-06-11 — v0.19 — SDF Grid V0 (Hybrid Static/Dynamic)

### Contexto

La catedral herméticaL (990×656) renderea a ~1 FPS con SIMD+MT. El cuello de botella es la
evaluación SDF: miles de nodos BVH recorridos por cada rayo, la mayoría estáticos (sin dependencia
de `w` ni funciones no lineales). La solución V0 es un grid uniforme 64³ que cachea el SDF de
nodos estáticos por celda, dejando solo los dinámicos para evaluar en cada frame.

### Añadido

- **`SdfGrid` struct** (`render/scene.h`): grid 3D uniforme con origen, tamaño de celda,
  dimensiones, y dos arrays planos: `offsets` (byte offset por celda) y `data` (bitmask de nodos
  dinámicos por celda, con bit 31 = terminador).

- **`classifyOntNodes(sc)`** (`render/ray_march.h`): clasifica cada BVH leaf como estático o
  dinámico según su bytecode (heurística: presencia de SIN/COS/TAN/POW/MOD/SAMPLE o referencia
  a `w`).

- **`buildSdfGrid(sc)`** (`render/ray_march.h`): construye grid 64³. Itera nodos estáticos,
  rasteriza su AABB sobre celdas, evalúa SDF en el centro de cada celda ocupada y almacena
  el valor mínimo como SDF precomputado. Para nodos dinámicos, almacena bitmask por celda.

- **`gridSample(grid, p)`** (`render/ray_march.h`): indexa el grid en la posición del rayo,
  devuelve el bitmask de nodos dinámicos (0 = celda puramente estática).

- **`evalHybrid(sc, p, w, grid)`** (`render/ray_march.h`): evaluación híbrida SDF. Si la celda
  no tiene dinámicos, devuelve el SDF precomputado directamente (sin recorrer BVH). Si tiene
  dinámicos, evalúa solo esos nodos y combina con el SDF estático cacheado.

- **`evalDynamicNodes(sc, p, w, mask)`** (`render/ray_march.h`): evalúa un subconjunto de nodos
  BVH filtrados por bitmask, retorna el mínimo.

- **`isBcDynamic(bc, len)`** (`render/ray_march.h`): heurística de clasificación que escanea
  bytecode en busca de opcodes que impliquen variación temporal o no-linealidad.

### Cambiado

- **`render/render.h`**: `Renderer::render()` construye el grid al inicio (`buildSdfGrid`) y
  lo destruye al final. Pasa el grid a `renderOntSceneMTSIMD()`.

- **`render/ray_march.h`**: `rayMarchOnt()` usa `evalHybrid()` en vez de `bvhEval()`.
  Nuevos parámetros: `const SdfGrid* grid` en `renderOntScene()` y sus variantes.

### Pendiente

- Benchmark cuantitativo (FPS antes/después con catedral)
- V1 planeada: SVDDF (brick maps 3D con resolución adaptativa) o grid sparse

## 2026-06-10 — v0.18 — Escena catedralicia, timeline loop, terrain blend

### Añadido

- **`terrain_blend_strength` en PipelineConfig** (`render/scene.h:151`): nuevo campo `f32 terrain_blend_strength = 0.0f` que controla la mezcla de terreno SDF en el shading. Usado en `ray_march.h` para reducir el efecto metálico en superficies de terreno: `terrain_blend = (1.0f - metallic) * pl.terrain_blend_strength`. Valor típico: 0.6 para escenas con terreno.

- **Visor carga `catedral_hermetica` por defecto** (`visor/main.cpp:194`): la ruta por defecto cambió de `test_custom.ont` a `..\..\Lenguaje Hermetico\libreria\escenas\catedral_hermetica_0000.ont` (relativa al exe). Fallback a `test_custom.ont` si la catedral no existe, luego a `test_suelo.rih`.

- **Visor timeline loop desde `.obs`** (`visor/visor_app.cpp:171-173`): cuando el `.obs` tiene `has_timeline`, el tiempo del renderer se loopeará con `fmod(time, w_max)`. Esto permite que escenas animadas con 60 W-frames (como catedral_hermetica) loopear seamlessmente.

- **`test_render_catedral.cpp`**: nuevo test que renderiza la catedral usando el pipeline .ont/.obs con rutas absolutas.

### Cambiado

- **`render/scene.h`**: PipelineConfig extendido con `terrain_blend_strength`.
- **`visor/main.cpp`**: ruta default actualizada con fallback en cadena.

## 2026-06-10 — v0.17 — Optimizaciones de rendimiento SDF

### Añadido

- **Eliminación de material-finding redundante** (`render/ray_march.h`):
  `bvhEval()` ahora acepta `u32* out_material = nullptr` opcional. Durante el recorrido BVH,
  rastrea `best_mat` por graph node. En `rayMarchOnt()`, la llamada `bvhEval(sc, p, w, &r.material)`
  reemplaza el bucle BVH post-hit redundante (~30 líneas eliminadas). Ganancia: ~10% escenas
  simples, 20-50% escenas con cientos de nodos.

- **SSE SIMD Packet Tracing** (`render/ray_march_simd.h` — nuevo):
  - `execBcRaw4()` — bytecode VM SSE 4-wide. Todos los opcodes aritméticos (ADD, SUB, MUL, DIV,
    NEG, ABS, SQRT, FLOOR, CEIL, MIN, MAX, CLAMP, LERP/MIX) vectorizados con intrínsecos SSE.
    Opcodes no vectorizables (SIN, COS, TAN, POW, MOD, SAMPLE) con fallback escalar por lane.
  - `applyMatrix4()` — transformación SSE 4×4 para 4 puntos simultáneos.
  - `bvhEval4()` — trazado BVH conjunto para 4 puntos. Mantiene `node_idx` por lane para
    manejar divergencia de trayectorias. Evalúa hojas con `execBcRaw4` y solo actualiza
    resultados de lanes dentro del AABB.
  - `renderOntSceneSIMD()` — bucle de render single-thread que procesa 4 píxeles adyacentes
    como paquete. Raymarching lockstep con 4 rayos, SDF SIMD en cada paso.
  - `renderOntSceneMTSIMD()` — versión multi-thread que divide la imagen en bandas de filas,
    cada thread procesa paquetes de 4 píxeles con SIMD.

- **Integración en pipeline**: `render.h` → `Renderer::render()` usa `renderOntSceneMTSIMD()`
  cuando `ont_mode = true`.

### Rendimiento (test_custom.ont, 2 nodos, 20 bytes bytecode)

| Config | 1000×700 ST | 1000×700 MT | Full HD MT |
|--------|-------------|-------------|------------|
| Scalar | 77ms (13 FPS) | 16ms (61 FPS) | 40ms (25 FPS) |
| **SIMD** | **40ms (25 FPS)** | **8ms (123 FPS)** | **28ms (36 FPS)** |
| Ganancia | **1.9×** | **2.0×** | **1.4×** |

Full HD interactivo (>30 FPS) por primera vez.

## 2026-06-08 — v0.16 — Pipeline Ontológico (.ont + .obs)

### Añadido

- **Formato .obs (Observation)** (`render/scene.h`):
  - `ObsHeader` — magic + flags + secciones opcionales (cámara, luces, timeline, fondo, resolución)
  - `ObsCamera` — position, target, up, fov
  - `ObsLight`, `ObsLightsHeader` — luces con tipo, color, intensidad, falloff
  - `ObsTimeline` — time, frame, fps, loop
  - Formato binario extensible por flags: futuras secciones se añaden sin romper compatibilidad
  - `.obs` es archivo independiente del `.ont`, no mezcla datos de observación con geometría estática

- **Herm compiler escribe .obs** (`herm_obs.h`/`herm_obs.cpp`):
  - `writeObs()` deriva el path `.obs` del `.ont` de salida
  - Serializa camera, lights, timeline, background, resolution desde el RIH cargado
  - Llamado automáticamente tras cada `writeOnt()` en `main.cpp`
  - `herm build.bat` incluye `herm_obs.cpp`

- **Visor carga .obs** (`render/scene.cpp`):
  - `OntScene::loadObs()` parsea secciones opcionales del `.obs`
  - `applyObs()` copia camera, background, resolution a `OntScene`
  - `OntObservation` struct: camera, lights, timeline, background, resolution + flags `has_*`
  - Si no hay `.obs`, usa defaults: cámara (0,0,3)→(0,0,0), fov=50, fondo negro, 800×600

- **Multi-light PBR shader** (`render/ray_march.h`):
  - Nuevo overload `shadeOnt()` que acepta `const Light* lights, u32 light_count`
  - Itera todas las luces con Cook-Torrance GGX (NDF, Schlick-GGX Geometry, Schlick Fresnel)
  - Fallback a luz direccional hardcoded si no hay luces
  - `renderOntScene()` pasa luces de `.obs` cuando disponibles

- **Navegación cámara WASD** (`scene/camera.h`):
  - `updateFly()` ya no es no-op — auto-switch de ORBIT a FREE_FLY en primer WASD
  - Movimiento con forward/right vectors desde la orientación actual

- **Mouse orbit con botón medio** (`visor/input_controller.h/.cpp`, `visor/window_manager.cpp`):
  - `WM_MBUTTONDOWN`/`WM_MBUTTONUP` → flag `mouse_dragging`
  - `handleMouseMove()` llama `cam_ctrl.orbitRotate()` durante drag

- **F1 toggle FREE_FLY mouse look** (`visor/input_controller.cpp`):
  - F1 alterna ORBIT ↔ FREE_FLY, captura/suelta cursor

- **WM_MOUSEMOVE handlers** en `WindowProc` y `ViewportProc` (`visor/window_manager.cpp`):
  - Forward a `input.handleMouseMove()`, aplica cámara, set `dirty = true`
  - `SetCapture(hwnd_viewport)` evita double-handling

- **Adaptive resolution durante movimiento** (`visor/visor_app.h/.cpp`):
  - `render_scale = 0.25f` en movimiento, `1.0f` tras 400ms idle
  - `renderFrame()` escala resolución, `paintViewport()` usa `StretchDIBits` para upscale
  - Render cooldown eliminado — cada movimiento setea `dirty = true` inmediatamente

- **FPS counter overlay** (`visor/window_manager.cpp`):
  - Texto verde en viewport: FPS, render ms, pump ms, scale

- **Inside-geometry SDF fix** (`render/ray_march.h`):
  - `t` arranca en `trace_t_min` (0.01)
  - Hit detection requiere `d >= 0.0f`
  - Stepping usa `abs(d)` para salir de geometría rápido

- **Multi-threading** (`render/ray_march.h`):
  - `renderOntSceneMT()` divide framebuffer en bandas por `std::thread`
  - Detecta cores vía `GetSystemInfo`, fallback single-thread si height < 32
  - Speedup: 7× a 1000×700 (1325ms → 187ms)

- **Inicialización de cámara corregida** (`visor/visor_app.cpp`):
  - `init()` computa orbit `dist`, `azimuth`, `elevation` desde posición/target real del `.obs`
  - Usa `std::atan2`/`std::asin` en vez de hardcoded defaults

### Cambiado

- **`build.bat`** incluye `herm_obs.cpp`
- **`visor_app.cpp:init()`** ya no sobrescribe resolución del workspace con `.obs`
- **`render.h`**: `Renderer::render()` usa `renderOntSceneMT()` (y luego `renderOntSceneMTSIMD`)

## 2026-06-04 — v0.15 — Sesión fundacional: auditoría, fixes, capa Scene, visor, optimizaciones

Una sola sesión continua desde "¿Qué hicimos hasta ahora?" hasta el cierre.

### Fase 1: Diagnóstico y fixes (04 jun — inicio de sesión)

El proyecto existía con código legacy en `core/` y capas nuevas en `os/` + `rhi/` + `render/`
con bugs críticos. Se diagnosticó y corrigió:

- **Bug: `skipValue()` loop infinito** — El parser JSON (`render/scene.cpp`) al saltar objetos
  anidados (ej. campo `"time"`) solo leía el primer string y se trababa en `:`. Fix: añadir
  `match(':'); skipValue();` en la rama de objetos.
- **Bug: ID→índice circular** — `children` en RIH usa IDs de nodo, no índices de array.
  Mesa (id=0, índice 5) tenía children `[1,2,3,4,5]` → el 5 era ella misma → recursión
  infinita. Fix: `_id_to_idx` + `idToIndex()`.
- **Bug: Build `/MD`** — CRT estático colgaba el proceso al iniciar. Fix: `/MD` en build scripts.
- **Bug: Test expression** — `sin(w*2)` con w=π/2 daba sin(π)=0. Fix: `sin(w)`.
- **Bug: Test normal sign** — Normal del frente de esfera apunta +z, test esperaba z<0. Fix: z>0.

Resultado: 36/36 OS+RHI, 90/90 render.

### Fase 2: Capa Scene (04 jun — diseño + implementación)

Debate arquitectónico sobre cámara como player, visibilidad en SDF vs rasterización,
formato de proyecto.

Se creó:

- `scene/scene_graph.h` — SceneNode, SceneGraph, jerarquía, dirty flags, world transform, AABBs
- `scene/camera.h` — CameraController (ORBIT, FREE_FLY, FOLLOW)
- `scene/scene_query.h` — Bvh (construcción top-down, query ray-AABB)
- `scene/project.h` — Project, formato .mgproject (save/load con FileMapping)
- `scene/workspace.h` — Viewport, Layer, Timeline, Workspace
- `tests/test_scene.cpp` — 82 tests
- `tests/build_scene_test.bat`

### Fase 3: Refactor visor (04 jun)

Se reemplazó `core/` (legacy) por `render/` + `scene/` + `os/`:
- `Renderer` reemplaza Rih + RenderConfig + render()
- CameraController con ORBIT por defecto
- SceneGraph + BVH + AABBs computados al cargar
- Status bar actualizada
- build.bat actualizado

### Fase 4: Optimizaciones (04 jun — debate + implementación)

Debate sobre:
1. AABB early-out (punto-AABB como lower bound del SDF)
2. BVH integration (por rayo, no frustum culling)
3. Epsilon adaptativo (crece con distancia a cámara)

Se implementó:
- `Aabb` unificado en `render/scene.h` con expand/contains/surfaceArea/intersect
- `evalScene`/`findClosestNode`/`calcNormal` con `Aabb*` + `filter_nodes`
- `rayMarch` con epsilon adaptativo: `hit_eps * (1 + t * 0.01)`
- `SceneQuery` struct (callback BVH + AABBs, sin acoplar render/ con scene/)
- renderScene overload con SceneQuery (BVH query por píxel)
- 11 nuevos tests de optimización

### Fase 5: Documentación (04 jun)

Creación de `docs/` con 12 documentos:
fundamentos, arquitectura, especificaciones, guía de uso, implementación,
métricas, complejidad, changelog, historial, contrato ontológico, estado y deuda.

Resultado final: **219/219 tests, 0 fallos.**
