# 17b — Catálogo de Módulos del Editor (`motor-grafico`)

> **Parte 2 de 3.** Complementa `17a` (conceptual/layout) y precede a `17c` (ingeniería / paths).
> Cada módulo: qué hace, datos del engine que toca, **estado** (SOPORTA / PARCIAL / FALTA según `docs/16` §6), widgets clave, y notas de estados/errores.
> Modelo vigente: IDE de compilador hermético tensor/SDF; declaración (`.herm`/node graph) + lookdev + W-navigation + Systems + export; determinismo; audio/UI como systems herméticos.

---

## Índice de módulos (~36)

- **Shell:** 1 MenuBar · 2 Toolbar · 3 DockSpace/Layout · 4 Multi-viewport · 5 Command Palette · 6 Preferences · 7 Input/Spaces Mgr
- **Declaración SDF:** 8 Hermetic Code Editor · 9 SDF Node Graph · 10 Scene Validation/Anomaly
- **Composición:** 11 Árbol Ontológico · 12 Inspector F/Q/B · 13 Gizmos/Transform · 14 Snap/Grid · 15 Layers/Tags · 16 Camera Bookmarks
- **Lookdev:** 17 Material PBR · 18 Material Node Graph · 19 Lighting/Environment · 20 Render Settings · 21 Bake/MLIR
- **Tiempo:** 22 W-Scrubber · 23 Play/Run Systems
- **Assets:** 24 Source Browser + Export · 25 `.mgproj` Save/Load
- **Soporte:** 26 Console/Log · 27 Profiler · 28 Undo/Redo · 29 Tensor/State Inspector · 30 Empty/Onboarding · 31 Help · 32 Theming
- **Systems/Transversales:** 33 Systems/Game-logic · 34 Audio System · 35 UI System (SDF-UI) · 36 Scripting Bridge

---

## Shell

### M1 — MenuBar
- **Qué:** File / Edit / GameObject / Component / Systems / Window / Help + `[> Play]` `[# Stop]`.
- **Engine:** — (shell puro).
- **Estado:** SOPORTA (se diseña).
- **Widgets:** menús estándar ImGui; Play/Stop = dispara modo Play (genera W interactivo vía Systems) o detiene.
- **Nota:** "GameObject" se mantiene como identificador común (ver `17a`); técnicamente es un nodo/entidad SDF.

### M2 — Toolbar
- **Qué:** herramientas P/I/K (Perturbación/Influencia/Aniquilación), Move/Rotate/Scale, Grid/Snap, Mode Edit/Play.
- **Engine:** `CameraController` (ORBIT/FREE_FLY/FOLLOW); `SceneGraph` (transforms).
- **Estado:** PARCIAL (cámara sí; P/I/K y gizmos SDF por construir).
- **Widgets:** botones toggle; el modo Edit/Play cambia el Input/Spaces (M7).

### M3 — DockSpace / Layout Manager
- **Qué:** paneles dockables; **layouts guardables/recreables** (estilo Unity).
- **Engine:** — (estado de UI, persiste en `.mgproj` o sidecar de layout).
- **Estado:** SOPORTA (ImGui `DockSpace` rama docking).
- **Widgets:** arrastrar/acoplar; presets de layout; "Save Layout / Reset".
- **Estado:** layout corrupto → fallback a default con aviso.

### M4 — Multi-viewport
- **Qué:** varios viewports (persp/orto, W-slices distintos, IR/Tensor).
- **Engine:** `vk_ctx` (texturas offscreen).
- **Estado:** PARCIAL (un viewport hoy; multi requiere varias texturas offscreen).
- **Widgets:** split de DockSpace; selector de modo por viewport.

### M5 — Command Palette (Ctrl+K)
- **Qué:** búsqueda de acciones/activos (estándar pro).
- **Engine:** — .
- **Estado:** SOPORTA (se diseña).
- **Widgets:** input fuzzy + lista; atajos.

### M6 — Preferences
- **Qué:** keymaps, theme, paths (Vulkan SDK, deps).
- **Engine:** `VULKAN_SDK` (env), `setup_deps.bat`.
- **Estado:** SOPORTA.
- **Widgets:** tabs; editor de atajos; selector de tema (Dark/Light/High-contrast).

### M7 — Input/Spaces Manager
- **Qué:** gestiona cámara/ratón en modos **Edit** y **Play**; **ray-SDF 2D** (UI futura SDF) y **ray-SDF 3D** (viewport). Unifica origen vía `IInputSource`.
- **Engine:** `win32` input; `ImGui::GetIO().WantCaptureMouse/Keyboard` (gate).
- **Estado:** PARCIAL (ImGui captura hoy; ray-SDF 2D/3D por construir en `17c`).
- **Widgets:** modo actual; indicador de espacio (UI vs Mundo).

---

## Declaración SDF

### M8 — Hermetic Code Editor
- **Qué:** edita `.herm` (fuente del Lenguaje Hermético); live-compile a `.ont` vía lib runtime.
- **Engine:** `lenguaje-hermetico` (lib), `render/scene.h` (`.ont`).
- **Estado:** PARCIAL (parser existe en build-time; falta lib runtime + editor).
- **Widgets:** editor de texto ImGui (resaltado); gutter de errores; "modo fuente" deshabilita node graph (ver `17a` §11).
- **Estado:** error de sintaxis → subrayado + panel de diagnósticos, no envía bytecode roto.

### M9 — SDF Node Graph
- **Qué:** grafo visual de campos de distancia (primitivas + CSG smooth/union/sub + domain fold/repeat). **2 grafos:** UI (x/y/colores/bézier/selección) vs **Hermetic AST/RIG**. Sync incremental (parchear AST, no regenerar). Undo/Redo por command pattern.
- **Engine:** AST → RIH (CTP 1×8) → `.ont`.
- **Estado:** FALTA (core del editor).
- **Widgets:** canvas con nodos arrastrables, puertos, conexiones bézier; minimapa; inspector de nodo; validación en vivo.
- **Nota:** P/I/K como ops fundamentales en puertos.

### M10 — Scene Validation / Anomaly Diagnostics
- **Qué:** **Capa (a) estática** (ayuda de authoring, **dev-only, se descarta al exportar**): check de AST — `repeat`/`fold` sin dominio acotado (marcha infinito), constantes no-finitas (NaN/Inf), recursión P/I/K sin caso base, `smooth_union` con `k<0` (deja de ser SDF válido). **Capa (b) runtime:** detector de anomalías relacional (Readback del Tensor Inspector valida correlaciones a lo largo de W; entidades que divergen, tensores no-finitos).
- **Engine:** AST; `VkBuffer` staging (readback).
- **Estado:** FALTA (capa a) / FALTA (capa b, depende de M29).
- **Widgets:** lista de issues con severidad (Error/Warn/Anomaly) en Console (M26) con explicación de por qué; click → nodo ofensor.

---

## Composición

### M11 — Árbol Ontológico
- **Qué:** scene graph único con facetas **F/Q/B** por nodo; filtrable/agrupable por capa; add/remove/reparent; enable/disable.
- **Engine:** `SceneGraph` (`parent`, `first_child`, `enabled`, `markDirty`).
- **Estado:** SOPORTA (SceneGraph completo).
- **Widgets:** tree vertical; checkboxes de capa F/Q/B; botón `[+ Nodo]`; reparent drag.
- **Nota:** es un subsistema del mundo, no un árbol de meshes.

### M12 — Inspector por facetas F/Q/B
- **Qué:** propiedades del nodo: Física (transform, forma), Química (material PBR, color), Biología (herencia/ensamblaje), + Systems + Scripts/Bridge.
- **Engine:** `SceneNode`, `render::Node`, `OntMaterial`, `Light`, `Camera`.
- **Estado:** PARCIAL (panel de texto existe; falta enlazar editable + facetas F/Q/B).
- **Widgets:** secciones colapsables F/Q/B; sliders; color picker (procedural); override de material.
- **Estado:** campo inválido → borde ámbar + tooltip, no crashea.

### M13 — Gizmos / Transform
- **Qué:** manipulación Move/Rotate/Scale sobre selección.
- **Engine:** `local_translate/rotate/scale`, `world_*`.
- **Estado:** PARCIAL (transforms sí; gizmos por construir).
- **Widgets (v1):** gizmos como **overlay ImGui 2D proyectado** desde handles 3D (más libertad, simple). Gizmos SDF después.

### M14 — Snapping / Grid / Units
- **Qué:** grid del viewport, snap, unidades.
- **Engine:** `CameraController`; grid es overlay.
- **Estado:** PARCIAL.
- **Widgets:** toggle grid; config de snap; selector de unidades.

### M15 — Layers / Tags
- **Qué:** organizar nodos por capas/tags; filtros de visibilidad/select.
- **Engine:** `SceneGraph` (extensible con metadatos).
- **Estado:** FALTA (metadato en `.mgproj`).
- **Widgets:** panel de capas; asignar tag.
- **Nota:** editor-only; se descartan en export (no contaminan el mundo hermético).

### M16 — Camera Bookmarks
- **Qué:** guardar/restaurar poses de cámara.
- **Engine:** `CameraController`.
- **Estado:** SOPORTA (se diseña).
- **Widgets:** lista de bookmarks; thumbnails.

---

## Lookdev

### M17 — Material Editor (PBR procedural)
- **Qué:** `base_color`, `roughness`, `metallic`, `emission`, `opacity` + **funciones analíticas** (sin slot de textura).
- **Engine:** `OntMaterial` (`render/scene.h`).
- **Estado:** PARCIAL (struct existe; color dinámico se descarta en `.ont` → bug Fresnel blancos).
- **Widgets:** sliders PBR; editor de función analítica (curvas/expresiones); preview swatch en viewport.
- **Estado:** control deshabilitado con razón si el backend no soporta (p.ej. "GPU: color dinámico pendiente").

### M18 — Material Node Graph
- **Qué:** composición de funciones analíticas de material (estilo PBR procedural, NO Substance/texturas).
- **Engine:** `OntMaterial` (extendido).
- **Estado:** FALTA.
- **Widgets:** grafo de funciones; preview.

### M19 — Lighting / Environment
- **Qué:** luces (dir/point/spot), HDR/IBL, post (tonemap, bloom).
- **Engine:** `renderer.scene.lights` (GPU: 1 hardcodeada; CPU: multi).
- **Estado:** PARCIAL (multi-luz CPU; GPU 1; HDR/IBL FALTA).
- **Widgets:** lista de luces; env map (cuando exista); toggles **ghosted** con razón ("GPU: 1 luz hoy").
- **Nota:** toggles ghosted + feature-flag registry = **deuda técnica** hasta que el pipeline GPU madure.
- **Estado:** toggle de sombra/AO deshabilitado hasta `17c`.

### M20 — Render Settings / Quality
- **Qué:** resolución, render scale, iteraciones raymarch, toggles AO/sombras.
- **Engine:** `UboData`, `trace_t_max`.
- **Estado:** PARCIAL (parámetros existen; AO/sombras FALTA en GPU).
- **Widgets:** sliders; toggles ghosted; "Quality preset".
- **Nota:** toggles ghosted + feature-flag registry = **deuda técnica** hasta que el pipeline GPU madure.

### M21 — Bake / MLIR Pipeline
- **Qué:** pre-bake offline vía `MLIR-CampoTensorial` (4 dialectos).
- **Engine:** `deps/MLIR-CampoTensorial`.
- **Estado:** FALTA (pipeline offline opcional).
- **Widgets:** panel de bake; selección de dialecto; progreso.

---

## Tiempo

### M22 — W-Scrubber (Block Universe)
- **Qué:** deslizador de W para **inspeccionar** el 4D autorado; keyframes como abstracción opcional.
- **Engine:** `.obs` (`w_max`); corte transversal en W.
- **Estado:** PARCIAL (`.obs` tiene timeline; UI por construir).
- **Widgets:** timeline W con play/pause; marcadores; cursores de corte.

### M23 — Play / Run Systems
- **Qué:** genera W **interactivamente** vía Systems (gameplay); no es solo cortar un bloque fijo.
- **Engine:** Systems corren en el ejecutable; input → tensors.
- **Estado:** FALTA (motor no tiene scripting de gameplay hoy).
- **Widgets:** Play/Stop; HUD de estado; indicador de modo.

---

## Assets

### M24 — Source Browser + Export
- **Qué:** navega `.herm`/scripts/`.mgproj`; **Export** compila mundo + Systems → ejecutable jugable (`.herm→RIH→TB→PNG`/volumen; futuro multiplataforma).
- **Engine:** pipeline offline; `build.bat`.
- **Estado:** PARCIAL (build existe; UI de browser/export FALTA).
- **Widgets:** árbol de archivos; doble-click abre; "Export Executable" con opciones.
- **Nota:** NO hay texturas/audio binarios de entrada (son salida). **Gating:** pre-export corre capa (a)+(b); si hay Error/Anomaly, bloquea el export con un reporte **detallado** (sin mensajes basura) que cita el nodo/archivo ofensor.

### M25 — `.mgproj` Save/Load
- **Qué:** escena editable del editor (JSON puro, refs externas a `.ont`/`.herm`/scripts; `schema_version`).
- **Engine:** — (formato propio del editor).
- **Estado:** FALTA.
- **Widgets:** guardar/abrir; autosave; diff-friendly.
- **Estado:** archivo corrupto → "Recuperar último válido" con aviso.

---

## Soporte

### M26 — Console / Log
- **Qué:** mensajes, errores, diagnóstico de anomaly.
- **Engine:** `phase6_diag.txt` / log interno.
- **Estado:** PARCIAL (log existe; panel por construir).
- **Widgets:** consola con filtros (Error/Warn/Info); auto-scroll; click en error → fuente.

### M27 — Profiler / Stats
- **Qué:** FPS, ms/frame, dispatches, memoria (75% del coste GPU = normales, 4 evals/píxel).
- **Engine:** `bench_*` existente.
- **Estado:** PARCIAL.
- **Widgets:** panel de stats; gráfico de ms; desglose de dispatches.

### M28 — Undo/Redo History
- **Qué:** command pattern sobre **diffs de AST** (no del mundo); cubre node graph, inspector, hierarchy.
- **Engine:** AST (fuente de verdad).
- **Estado:** FALTA (omisión grave del brief).
- **Widgets:** historial navegable; Ctrl+Z / Ctrl+Y; determinista (ver `17a` §5).

### M29 — Tensor / State Inspector ⭐
- **Qué:** lee **Readback/Staging buffer** (GPU escribe fin de frame / al pausar); dibuja gráficos/valores/heatmaps de tensores 1×8. **Valida correlaciones relacionales a lo largo de W** y es la casa del **detector de anomalías runtime (capa b, M10)**.
- **Engine:** `VkBuffer` staging; IR CTP 1×8.
- **Estado:** FALTA (núcleo de observabilidad; sin esto la UI es ciega).
- **Widgets:** inspector numérico; plot de un componente vs W (revela correlaciones); heatmap; flag de anomalía relacional.
- **Estado:** lectura async con throttle; pausa para lectura exacta.

### M30 — Empty states / Onboarding / Tooltips
- **Qué:** primera ejecución, escena nueva, errores de carga `.ont`.
- **Engine:** — .
- **Estado:** FALTA.
- **Widgets:** empty state ("Declara o importa SDF"); tooltips de P/I/K; hints.

### M31 — Help
- **Qué:** docs, atajos, glosario hermético (F/Q/B, P/I/K, XYZWRGBA).
- **Estado:** SOPORTA.
- **Widgets:** panel/modal; búsqueda.

### M32 — Theming
- **Qué:** tokens lookdev (Dark `#0E0F12`/`#1B1E24`/`#23272F`, borde `#2E333D`, texto `#E6E8EC`/`#9AA1AC`, 1 acento `#3DD6C4` o `#E8A33D`, error/anomalía `#FF6B6B` pulsante). Variantes Dark/Light/High-contrast.
- **Estado:** SOPORTA (ImGui `StyleVar`/`ColorEdit`).
- **Nota:** a11y realista = teclado + contraste + fuente configurable, no AT/WCAG.

---

## Systems / Transversales

### M33 — Systems / Game-logic
- **Qué:** authoring de systems que el mundo ejecuta (lógica de juego, física relacional).
- **Engine:** futuro (manifiesto: motor de físicas relacional, lenguaje científico).
- **Estado:** FALTA.
- **Widgets:** lista de systems; editor de reglas; enganche a nodos.

### M34 — Audio System
- **Qué:** síntesis analítica hermética — **campo de amplitud A(x,y,z,w)** muestreado en listener path.
- **Engine:** extendible sobre `XYZWRGBA` (sin binarios).
- **Estado:** FALTA (modelo nuevo).
- **Widgets:** editor de función de amplitud; listener path; mezclador.

### M35 — UI System (SDF-UI, north-star)
- **Qué:** *in-game UI* como system hermético (HUD/menús como nodos SDF). Distinto de la UI del editor (ImGui).
- **Estado:** FALTA (investigación `17c`).
- **Widgets:** (futuro) grafo de UI SDF.

### M36 — Scripting Bridge
- **Qué:** **PUENTES** Python/C entre mundo hermético y módulos externos (servidor, multijugador, I/O). Escriben/leen tensores 1×8; C → `.dll`/`.so` hot-reload. **Nunca por-píxel** (puede correr por-frame event-driven).
- **Engine:** IR CTP; ABI 1×8 compartido Python/C.
- **Estado:** FALTA.
- **Widgets:** editor de bridge; indicador de conexión; log de paquetes.
- **Nota:** única fuente sancionada de no-determinismo: inyecta el stream de input externo al Input Bus; el mundo hermético sigue siendo función pura de (fuente + ese stream).

---

## Resumen de estados (mapa rápido)

| Módulo | Estado |
|---|---|
| M1 MenuBar, M3 DockSpace, M5 Palette, M6 Prefs, M11 Árbol Ontológico, M16 Bookmarks, M31 Help, M32 Theming | SOPORTA |
| M2 Toolbar, M4 Multi-view, M7 Input/Spaces, M8 Code Editor, M12 Inspector, M13 Gizmos, M14 Snap/Grid, M17 Material, M19 Lighting, M20 Render, M22 W-Scrubber, M24 Source/Export, M26 Console, M27 Profiler | PARCIAL |
| M9 Node Graph, M10 Validation(b), M15 Layers, M18 Material Node Graph, M21 Bake, M23 Play, M25 `.mgproj`, M28 Undo/Redo, M29 Tensor Inspector, M30 Empty/Onboarding, M33 Systems, M34 Audio, M35 UI System, M36 Bridge | FALTA |

> **Patrón de estados:** todo lo FALTA hoy es el trabajo real del editor. Lo PARCIAL debe mostrar controles **ghosted con razón** (no mudos). Lo SOPORTA se construye sobre lo existente.

> **Módulos adicionales por diseñar:** ver backlog en `17c` §12 (UX editor, authoring de juegos, motor/integración, transversales).
