# 17c — Ingeniería y Paths del Editor (`motor-grafico`)

> **Parte 3 de 3.** Asume `17a` (conceptual/layout) y `17b` (módulos). Aquí: arquitectura de live-compile, observabilidad GPU, ray-SDF, bridges, features faltantes y roadmap.
> Referencias de código: `render/vulkan/`, `render/scene.h`, `core/scene_graph.h`, `win32/`, `build.bat`, `deps/MLIR-CampoTensorial`, `.obs`/`.ont`/`.herm`.

---

## 1. Arquitectura de Live-compile (Scheduler + Input Bus)

La "arquitectura especial que ahorra tiempo" pedida en el brief no es subtree-patch (el manifiesto compila bloques en microsegundos → recompile completo de `.ont` basta en v1). Es **separar el mundo de su fuente de input** para que la edición no bloquee el render:

- **Scheduler** (`CameraController` + `Renderer::render` ya loop): despacha recompilación como trabajo asíncrono. La edición marca `markDirty()` en `SceneNode`/AST; el Scheduler coalesce (debounce ~16–50 ms) y recompila `.ont` completo en un hilo; el frame actual usa el bytecode viejo hasta que el nuevo está validado.
- **Input Bus** (único origen de input, interfaz `IInputSource`): unifica teclado/ratón (Win32 + ImGui `WantCaptureMouse/Keyboard`) y **ray-SDF**. El editor y el Play consumen del mismo bus; en modo Play, los Systems leen del bus y escriben tensores. Evita disparidades de input entre edit/play.
- **Anomaly Gate (capa a, M10):** el Scheduler no publica bytecode si el AST es semánticamente degenerado; viewport **conserva el último frame bueno** (sin overlay complejo, por riesgo en HW débil). `trace_t_max` clamp + detección no-finito. La capa (b) corre post-frame con Readback.

**Gap/dependencia:** subtree hot-swap queda para v2 y exige `loadSubtree`/`patch` en la Bytecode VM (§7).

---

## 2. Observabilidad GPU — Readback / Staging (Tensor Inspector, M29)

Sin esto la UI es ciega. Modelo Vulkan:

- **Staging buffer (GPU):** `VkBuffer` `TRANSFER_DST | HOST_VISIBLE` (memoria `HOST_COHERENT` o `HOST_CACHED` con invalidación). GPU escribe tensores 1×8 al final de frame (o al pausar) vía `vkCmdCopyBuffer`/`CopyImageToBuffer`. **Path CPU (SIMD fallback):** no hay staging; el inspector lee los tensores directo de memoria. La UI abstrae una **fuente de tensores** (GPU staging | CPU memoria) con contrato estricto/seguro tipo Java (acceso definido, sin UB).
- **Throttle:** lectura cada N frames o solo en pausa (no por frame en vivo a 60 FPS para todo el framebuffer). El inspector pide "snapshot" → Scheduler dispara copia en el frame siguiente.
- **Fuente de datos (aclara ambigüedad):** el raymarch de GPU emite RGBA (píxeles), **NO** tensores 1×8 por nodo. El Inspector lee 3 cosas distintas: (1) **IR CTP** = artefacto de compilación, estático (igual cada frame); (2) **tensores de System/Bridge** = estado en CPU (lectura directa, sobre todo en path CPU); (3) **campo muestreado en probe points** = re-evalúa `Φ`/estado en puntos de prueba bajo demanda (debug-eval CPU o pass aparte). La capa (b) opera sobre (3), no sobre el framebuffer.
- **Visualización:** numérico (IR CTP y/o System tensors), plot de un componente vs **W** (revela correlaciones relacionales usando probe points), heatmap espacial.
- **Capa (b) anomaly:** detector compara el campo muestreado en probe points a lo largo de W; flag si dos entidades que deberían correlacionar divergen, o aparece no-finito.

**Estado:** FALTA (núcleo). Riesgo: latencia de readback en GPU integrada; mitigar con snapshot bajo demanda.

---

## 3. Ray-SDF (2D UI y 3D Viewport)

- **3D (viewport, M7/M13):** para picking de nodos SDF y gizmos. Lanzar rayo desde cámara contra el campo `Φ(p)` (march adicional o reusar el ya trazado para el píxel bajo el cursor). Hit = nodo más cercano. **ID pass:** buffer de node-ids en una **capa overlay separada** encima del mundo (no se funde con la imagen hermética); habilita picking exacto y gizmos sin afectar el render del mundo. Gizmos v1 = overlay ImGui 2D proyectado desde handles 3D (simple, determinista); gizmos SDF = v2.
- **2D (UI futura SDF, M35):** **ray-SDF 2D** = lanzar rayo contra paneles SDF compuestos para hit-testing (NO `if (mouse_x > panel_x)`). Necesario cuando la UI propia del juego sea superficies SDF. Hoy lo maneja ImGui automáticamente.
- **Gate:** si `ImGui::GetIO().WantCaptureMouse` → input va a ImGui; si no → ray-SDF 3D. Unifica en Input Bus (§1).

**Estado:** 3D FALTA; 2D FALTA (solo investigación, post-v1).

---

## 4. Scripting Bridges (Python / C, M36)

- **Modelo:** bridge = módulo externo que **lee/escribe tensores 1×8 (IR CTP)**; nunca por-píxel. Puede correr **por-frame event-driven** (p.ej. paquete de red → escribe tensor de estado de jugador).
- **ABI 1×8 (definido en manifiesto):** los 8 canales del campo `XYZWRGBA` = `[x, y, z, w, r, g, b, a]` (CTP IR spans). Struct de 8 floats compartido, **versionado**. Python (ctypes/pybind) y C (`extern "C"`) exponen `read_tensor(id, out8)` / `write_tensor(id, in8)`.
- **Python:** cargado por el host; corre en hilo del Scheduler; empuja estado a la IR entre frames. No en el loop de raymarch.
- **C / `.dll`·`.so`:** hot-reload = descarga lib, recarga símbolos, reengancha; el mundo no se reinicia. Requiere que el bridge sea state-free o que el estado viva en tensores (determinismo).
- **Caso multijugador:** bridge de red lee socket → escribe tensor de transforms/estado → Scheduler lo inyecta; el resto del mundo es hermético.

**Estado:** FALTA. Riesgo: ABI drifting entre Python/C; fijar layout de struct en build-time.

---

## 5. Features GPU faltantes + controles "ghosted"

Estado real (`docs/16` §6) y cómo la UI lo expone:

- **Multi-luz:** GPU hoy = 1 hardcodeada; CPU = multi. UI (M19): toggles de luces adicionales **ghosted** con tooltip "GPU: 1 luz hoy (CPU sí)".
- **Color dinámico / `base_color` por nodo:** se descarta en la escritura a `.ont` → Fresnel blancos. **Bug a corregir en `render/scene.h` / writer `.ont`**; UI (M17) debe reflejar color real, no ghost.
- **AO / sombras:** FALTA en GPU. Toggles (M20) **ghosted** con razón hasta `17c`.
- **Regla de UI:** todo control FALTA se muestra **deshabilitado + razón**, nunca mudo ni mentiroso. Tokens: error/anomalía `#FF6B6B`. **Deuda técnica:** el feature-flag registry (que auto-habilita estos toggles al madurar el pipeline GPU) queda pendiente; hoy son ghosted estáticos.

---

## 6. SDF-UI (north-star, M35) — investigación

- UI del juego como system hermético: HUD/menús = superficies SDF compuestas, hit-test vía ray-SDF 2D (§3).
- Se authora como nodos especiales en `SceneGraph`; el ejecutable los corre.
- Distinto de la chrome del editor (ImGui v1).
- **Investigar:** composición de paneles SDF, text SDF (sdf font atlas procedural), layout relacional. Fuera de v1.

---

## 7. Gap de motor — `subtree-load` / `patch` en Bytecode VM

- Para hot-swap incremental de sub-árbol en v2, la VM debe soportar carga parcial: `loadSubtree(node_id, bytecode)` / `patch(node_id, bytecode)`.
- Hoy no confirmado en el brief. Si no existe, v1 recompila `.ont` completo (suficiente por microsegundos). Documentar como dependencia de ingeniería; no bloquea v1.

---

## 8. Determinismo en UI / engine

- Mundo = función pura de declaración + stream de input (Input Bus). No globals mutables ocultos.
- Undo/Redo (M28) = diffs de AST, no mutación de mundo. Live-compile = transformación pura de AST.
- Semillas prohibidas (sin RNG). El Play varía solo por input del jugador.
- A11y realista (M32): teclado + contraste + fuente configurable. No AT/WCAG pesado.

---

## 9. W-Navigator UX (M22/M23)

- **W-Scrubber** inspecciona el 4D autorado (corte transversal en W). Play **genera** W interactivamente vía Systems.
- UX: timeline con play/pause para scrub; en Play, el cursor avanza por input. Marcadores de "eventos" de Systems. No confundir "animar" (cortar bloque) con "gameplay" (W por input).

---

## 10. Roadmap sugerido

- **v1 (editor usable):**
  1. Shell (M1–M7) sobre ImGui docking.
  2. Declaración: Code Editor `.herm` + live-compile (M8) + Árbol Ontológico (M11) + Inspector (M12) + Gizmos overlay (M13).
  3. Lookdev básico (M17/M19/M20) con toggles ghosted honestos.
  4. Scheduler + Input Bus + Anomaly Gate (a) (§1).
  5. Tensor Inspector con Readback (M29) + Validation (a) (M10).
  6. `.mgproj` Save/Load (M25) + Undo/Redo (M28) + Console/Profiler (M26/M27).
  7. Source Browser + Export (M24).
- **v1.5:** W-Scrubber (M22), Play/Run Systems esqueleto (M23), Scripting Bridge Python (M36), ray-SDF 3D picking (§3).
- **v2:** multi-luz/color dinámico/AO en GPU, subtree hot-swap (§7), Material Node Graph (M18), Systems (M33), Audio (M34).
- **post-v2 (north-star):** SDF-UI (M35), ray-SDF 2D, bridges C hot-reload, multiplataforma export.

---

## 11. Riesgos / Open questions

- **Coste de readback** en GPU integrada → snapshot bajo demanda (§2).
- **ABI 1×8 drift** Python/C → fijar en build-time (§4).
- **Bug color dinámico `.ont`** → corregir writer antes de prometer color en UI (§5).
- **`subtree-load`** no confirmado → v1 recompile completo (§7).
- **Ray-SDF 3D** preciso para picking fino → reusar traza del frame o march extra (§3).
- **W como gameplay** vs Block Universe → claridad de UX (§9).
- **Migración `.mgproj`:** `schema_version` forward-only con migrador por versión; abrir versión futura → rechazo con aviso (no corruptción silenciosa).
- **Crash recovery:** el AST vive en memoria; snapshot periódico a `.mgproj` + "recuperar" al reabrir, para no perder trabajo si el editor crashea mid-edit.
- **RAM:** el pico depende del proyecto (los 130 MB eran del compilador de imágenes GTE→TB, no del motor SDF); no fijar presupuesto rígido.

---

## 12. Backlog de diseño — pendientes de diseñar (no tocados ni mencionados)

> **Nota de alcance:** este análisis es de **diseño UI/UX**, no de implementación del motor (no soy el agente de building). Es susceptible de retomarse/retocarse en otro chat; estos 3 docs (`17a`/`17b`/`17c`) son la referencia de diseño. Lo siguiente son cosas importantes que **no** se han diseñado aún y conviene tener en cuenta. Estado: **PENDIENTE DE DISEÑO**.

### A. UX del editor (faltan)
- **First-run / onboarding profundo:** tour guiado, sample project, tutorial (M30 es solo esqueleto).
- **Viewport render modes:** wireframe / solid / lit / isolate-selected / X-ray (inspección básica hoy ausente).
- **Drag-and-drop** (browser→viewport), **context menus** por panel, **sistema de toasts/notificaciones** (compile async listo, errores).
- **Undo/Redo de acciones no-AST:** layout/camera/historia de panel (hoy solo diffs de AST).
- **Global search/filter** de nodos/activos más allá del Command Palette.
- **Prefabs/templates** reutilizables (más allá de herencia F/Q/B).
- **Multi-user editing** del editor (colaboración en el mismo proyecto).
- **Integración VCS (git):** diff/merge de `.herm`/`.mgproj` (dijimos diff-friendly, falta UI).
- **Plugin/extension architecture** del editor.
- **Macro/automatización** de tareas del editor.
- **Perf de la UI del editor** (FPS del editor, no solo del motor).
- **Testing/QA de la UI del editor.**

### B. Authoring de juegos (faltan; mayoría deuda)
- **Input Mapping editor:** teclado/gamepad→Systems (G3 diferido; la herramienta real por diseñar).
- **Camera rigs / cinemáticas:** cutscenes W-driven.
- **Debugging de Systems/Bridges:** breakpoints, watch, call stack (hoy solo Console).
- **Física relacional:** UI de authoring (M33 sin diseño de interacción).
- **Networking/multijugador:** UI de lobby, reglas de replicación (más allá del bridge).
- **Save/Load de sesión de juego:** runtime state, no solo proyecto editor.
- **Sandbox/seguridad de bridges:** un bridge Python/C es superficie de ataque; el hermetismo lo permite solo a él → sandboxing merece diseño.

### C. Motor / integración (faltan)
- **Color management / tonemapping:** linear/sRGB, pipeline HDR.
- **Sistema de unidades/escala canónica** (p.ej. metros) para física.
- **Determinism verification tooling:** correr fuente+input 2× y comparar (valida el modelo de `17a` §5).
- **Linker/bundler de export:** cómo `.ont`+Systems+bridges → ejecutable (deuda acordada).
- **Targets de export / packaging / signing** multiplataforma.
- **Almacenamiento de curvas W por nodo:** `.ont` vs `.obs` (principio 11 de `17a` no especifica soporte físico).

### D. Transversales
- **i18n:** UI en español; sin plan de internalización.
- **a11y más allá de teclado** (lectores de pantalla): explícitamente fuera de alcance, anotado.
- **Telemetría/analytics:** explícitamente **NO** (privacidad).

