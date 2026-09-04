# Brief de Diseño UI/UX — Editor del Motor Gráfico

> **Documento autónomo.** Está pensado para ser entregado a un agente de diseño UI/UX
> (o a ti mismo) que **no conoce este proyecto**. Contiene todo el contexto necesario
> para diseñar la interfaz del editor: qué es el motor, por qué se construye así, qué
> decisiones ya están tomadas, qué falta, y qué huecos debe el diseñador cubrir,
> cuestionar y expandir.
>
> **Mandato para quien diseña:** no aceptes esto como especificación cerrada. Escudriña,
> cuestiona, expande, cubre y diseña todo lo ausente y lo que aquí se haya omitido. El
> objetivo es una UI coherente para un motor de videojuegos real, no un boceto decorativo.

---

## 0. Resumen ejecutivo (contexto en 30 segundos)

- El proyecto es **`motor-grafico`**: un renderer **SDF (Signed Distance Field)** en C++17
  con **Vulkan Compute** (raymarching en GPU) y fallback **SIMD/SSE** en CPU, sobre Win32.
- **NO es un motor poligonal tradicional** (no hay mallas editables, no hay rasterizado de
  triángulos como camino principal). La geometría es **procedimental**, definida por
  *bytecode SDF* que genera otro proyecto hermano (`lenguaje-hermetico`) y se carga en `.ont`.
- El usuario quiere convertir el `visor` actual (viewer de una sola ventana) en un
  **editor tipo motor de videojuegos, estilo Unity**, con sabor *lookdev* de
  **Marmoset / Substance** (materiales, iluminación en tiempo real).
- La UI se construirá con **Dear ImGui (C++)** integrado en el renderer Vulkan.
- Este documento define el **mapa/layout/diagrama**, los **módulos y funciones** que un
  motor de videojuegos necesita, y cómo se mapean a la capacidad real del motor hoy.

---

## 1. CONTEXTO CRÍTICO: no es un motor tradicional (léelo antes de diseñar)

### 1.1 Qué ES el motor
- **Renderer SDF por raymarching**, no rasterizador de triángulos.
- La escena se describe como un árbol de **nodos SDF** (esfera, caja, cilindro, toro,
  plano, cono, operaciones booleanas/CSG, plegados *folding*) cuya evaluación se
  **compila a bytecode** y se ejecuta en GPU dentro de un compute shader (una "Bytecode
  VM" inlineada en GLSL).
- **Shading PBR** (Cook-Torrance GGX) por píxel.
- Pipeline de 5 capas:

  ```
  Visor (Win32 App)
     -> Scene (SceneGraph, BVH, Camera)
        -> Render (SDF eval, ray march, Cook-Torrance GGX)
           -> RHI (Vulkan / DX11)
              -> OS (Arena allocator, FileMapping, Timer, Window)
  ```

### 1.2 Qué NO es (y por tanto la UI no debe asumir)
- **No hay malla poligonal editable** en el engine. No espere un "Edit Mode" de vértices
  como Blender, ni importar `.fbx`/`.obj` como geometría editable nativa.
- **La geometría compleja la produce `lenguaje-hermetico`** (compilador `.herm` -> `.ont`/`.obs`).
  El motor consume `.ont` (bytecode SDF + materiales + cámara/luces) y `.obs` (observación:
  cámara/luces/timeline). El engine no "modela", *compone y mira*.
- **Multi-luz, sombras, AO y reflexión/refracción aún NO existen en GPU** (sí en CPU).
  Cualquier toggle de "sombras" hoy no haría nada en el path GPU.
- Es **Windows-only**, sin CI, sin tests GPU, sin resource manager.

### 1.3 Por qué importa para el diseño
- El panel de **Material** es el corazón del lookdev (estilo Substance/Marmoset), no el modelado.
- Un **node graph de SDF/CSG** es conceptualmente distinto a un node graph de shader
  tradicional: opera sobre campos de distancia, no sobre píxeles. Si se diseña un editor
  de nodos, debe reflejar eso (operadores de unión/intersección/resta, dominios
  repetidos/plegados, suavizados).
- La **jerarquía (Hierarchy)** mapea a `SceneGraph` (padre/hijos, transforms), pero los
  hijos de un nodo SDF suelen ser instancias/grupos, no sub-mallas.

---

## 2. Estado actual del proyecto (lo que ya existe y funciona)

- **Versión:** v0.26 (último en `docs/08-changelog.md`, 17 Jun 2026).
- **Build:** funciona. `setup_deps.bat` clona dependencias; `build_asmjit.bat` genera
  `asmjit.lib`; `build.bat` compila `build/visor.exe` (Vulkan + asmjit + volk + VMA).
- **Dependencias (ya resueltas, no tocar el SDK del usuario):**
  - `external/asmjit`, `external/volk`, `external/VulkanMemoryAllocator` (3rd-party).
  - `deps/lenguaje-hermetico`, `deps/MLIR-CampoTensorial` (repos propios; no necesarios
    para *compilar* el motor, sí para *generar escenas* y pipeline offline).
- **El `visor` hoy:** ventana Win32 con viewport Vulkan, panel de propiedades de texto y
  controles de cámara (orbit/WASD). **No usa ImGui en el build actual** (el `build_visor.bat`
  viejo lo referenciaba).
- **Capacidades reales del engine (mapeadas a structs):**
  - `SceneGraph` / `SceneNode` (`scene/scene_graph.h`): jerarquía padre/hijos
    (`parent`, `first_child`, `next_sibling`), `local_translate/rotate/scale`, `world_*`,
    AABB, flag `enabled`. Soporta reparent, markDirty, updateWorldTransforms.
  - `CameraController` (`scene/camera.h`): modos `ORBIT`, `FREE_FLY`, `FOLLOW`.
  - `render::Scene`: `nodes` (con `sdf_type` + `params`), `materials`, `lights`, `BVH`.
  - `OntMaterial` (`render/scene.h`): `base_color[4]`, `roughness`, `metallic`,
    `emission[3]`, `opacity`.
  - `.obs` timeline (`w_max`) para animación.
- **Bugs/limitaciones conocidas (del motor, no de la UI):**
  - UBO layout mismatch (std140 vs scalar) -> stepping adaptativo roto en GPU.
  - Color dinámico descartado en `.ont` -> materiales Fresnel salen blancos.
  - Sin AO / sombras / multi-light / reflexión en GPU (CPU sí).
  - ~75% del costo GPU = cálculo de normales (4 evals/píxel).
  - El contrato `.ont` (`OntHeader`, `OntBvhNode`, `OntMaterial`, `ONT_MAGIC`) **ya está
    definido dentro del motor** en `render/scene.h` (no en `lenguaje-hermetico`).

---

## 3. Decisiones YA tomadas (por el usuario — no reabrir salvo motivo fuerte)

1. **Tecnología de UI:** **Dear ImGui (C++)**, integrado en el renderer Vulkan.
   - Razón: se integra directo en el loop Vulkan, es estándar en tooling de motores, itera
     rápido. (Se descartó Electron/web y Qt por andamiaje/dependencias.)
2. **Alcance:** **editor tipo motor de videojuegos, similar a Unity** (no solo viewer
   lookdev, no DCC completo estilo Blender).
   - El motor es SDF, así que "game engine" aquí significa: composición de escena, objetos
     con componentes, cámara/luz/material, y (a futuro) play mode.
3. **Referencia de layout:** **mezcla Marmoset / Substance / Unity / Godot**.
   - De Unity/Godot: Hierarchy + Inspector + Project/Content + Scene/Game views, paneles dockables.
   - De Marmoset/Substance: lookdev en tiempo real, enfoque en materiales e iluminación/HDR,
     viewport como "stage".

## 4. Croquis / Layout del editor (diagrama ASCII de referencia)

```
+------------------------------------------------------------------------------+
| MenuBar: File  Edit  GameObject  Component  Window  Help   [> Play][# Stop]   |
+------------------------------------------------------------------------------+
| Toolbar: [Move][Rotate][Scale][Camera][Light][SDF][Material] | Grid . Snap   |
+-----------+--------------------------------------------------+-----------------+
| HIERARCHY |                                                  |  INSPECTOR     |
| / OUTLINER|            SCENE VIEW  [Scene | Game]            |  (obj selecc.) |
|  +- Raiz  |      [ render Vulkan raymarch en vivo ]          |  - Transform   |
|  +- Nodo  |      + grid + gizmos + seleccion                |  - SDF / Mesh   |
|  [+ Nodo] |                                                  |  - Material(PBR)|
|           |                                                  |  - Light/Camera |
|           |                                                  |  - Scripts      |
+-----------+--------------------------------------------------+-----------------+
| PROJECT / CONTENT  |  CONSOLE / LOG         |  TIMELINE / ANIM (keyframes)   |
| (.ont .obs mat tex)|  (diag, errores)       |  (.obs timeline)               |
+------------------------------------------------------------------------------+
| STATUS BAR: FPS . ms/frame . draws . modo[VK/CPU] . escena actual             |
+------------------------------------------------------------------------------+
```
- Paneles **dockables** (ImGui `DockSpace`, rama *docking*), como en Unity/Godot.
- El `SCENE VIEW` muestra el render del motor via `ImGui::Image` (textura offscreen).
- Pestañas `Scene | Game` en la vista central.

---

## 5. Modulos / paneles — descripcion y datos del engine que manejan

| # | Modulo | Que hace | Datos del engine (struct/origen) |
|---|--------|----------|----------------------------------|
| 1 | **MenuBar + Toolbar** | New/Open/Save escena, Play/Stop, herramientas de transform/gizmo | — |
| 2 | **Scene View (Viewport)** | Render 3D en vivo, grid, gizmos, seleccion, controles de camara | `vk_ctx`, `CameraController` |
| 3 | **Hierarchy / Outliner** | Arbol de `SceneNode`; add/remove/reparent; enable/disable | `SceneGraph` (`parent`, `first_child`, `enabled`) |
| 4 | **Inspector** | Propiedades del objeto: Transform, SDF (`sdf_type`+params), Material PBR, Light, Camera | `SceneNode`, `render::Node`, `OntMaterial`, `Light` |
| 5 | **Project / Content Browser** | Assets: `.ont`, `.obs`, materiales, texturas, prefabs; import | `deps/lenguaje-hermetico` (genera `.ont`) |
| 6 | **Material Editor** | Propiedades PBR (color, rough, metal, emit, opacity) + (opcional) node graph estilo Substance | `OntMaterial` |
| 7 | **Lighting / Environment** | Luces (dir/point/spot), HDR/ambient, post (tonemap, bloom) | `renderer.scene.lights` |
| 8 | **Render Settings / Quality** | Resolucion, render scale, iteraciones raymarch, toggles AO/shadows | `UboData`, `trace_t_max` |
| 9 | **Timeline / Animation** | Keyframes, play/scrub | `.obs` timeline (`w_max`) |
| 10 | **Console / Log** | Mensajes, errores, diagnostico | `phase6_diag.txt` / log interno |
| 11 | **Profiler / Stats** | FPS, ms/frame, dispatches, memoria | `bench_*` existente |
| 12 | **Save/Load escena** | Formato de escena editable del engine | (nuevo — ver §9) |

---

## 6. Mapa de capacidades: lo que el motor SOPORTA / PARCIAL / FALTA

| Modulo | Estado | Nota |
|--------|--------|------|
| Viewport 3D (Vulkan) | SOPORTA | raymarch compute ya funciona |
| Hierarchy / Transform | SOPORTA | `SceneGraph` completo |
| Inspector basico | PARCIAL | ya hay panel de texto; hay que volverlo editable/enlazado |
| Material PBR | PARCIAL | `OntMaterial` existe; color dinamico se descarta en `.ont` (bug) |
| Luces | PARCIAL | multi-luz en CPU si; en GPU solo 1 hardcodeada |
| Timeline | PARCIAL | `.obs` tiene timeline |
| Save/Load escena editable | FALTA | solo carga `.ont`/`.obs` (SDF read-only) |
| AO / Sombras / Multi-luz / Reflexion en GPU | FALTA | bugs conocidos del motor |
| Material node graph (CSG SDF) | FALTA | bytecode viene de `lenguaje-hermetico` |

---

## 7. Enfoque tecnico (para que el diseno sea implementable)

- **Vendorizar ImGui** (rama *docking*) en `external/imgui` y anadirlo a `setup_deps.bat`.
- **Integracion Vulkan:** render del 3D a una `VkImage` offscreen -> mostrar en `SCENE
  VIEW` con `ImGui::Image`; luego pasada de UI (ImGui) sobre el framebuffer final; anadir
  `ImGui_ImplVulkan` al loop de `vk_ctx`.
- **Input:** gatear el input `win32` con `ImGui::GetIO().WantCaptureMouse/Keyboard` para que
  la UI capture y el viewport reciba solo cuando toca.
- **Layout:** `ImGui::DockSpace` para paneles acoplables.
- El engine ya separa `renderer` (estado de escena) del `VisorApp` (loop/UI); el editor
  vivira como capa sobre `VisorApp`, enlazando paneles al `SceneGraph`/`Scene`/`Camera`.

---

## 8. Roadmap por fases (para que el diseno cubra presente y futuro)

- **Phase 0 — Integracion + shell:** ImGui en el loop, layout dockable con paneles
  placeholder, viewport mostrando el render actual.
- **Phase 1 — Paneles core enlazados:** Hierarchy (arbol), Inspector (Transform/SDF/
  Material/Light/Camera editable), Scene view con seleccion + gizmos, Project, Console.
- **Phase 2 — Edicion en vivo:** cambios en Inspector -> `markDirty` + re-render inmediato;
  add/remove/reparent nodos.
- **Phase 3 — Material/Lookdev:** editor PBR + environment/HDR + lighting + render settings
  (toggles AO/shadows cuando existan).
- **Phase 4 — Timeline/animacion, save/load de escena del engine, prefabs.**
- **Phase 5 — Mejoras de pipeline:** GPU AO, sombras, multi-luz, reflexion/refraccion (hoy
  faltan) para que los toggles del editor funcionen; profiler.

---

## 9. Preguntas abiertas / huecos deliberados (el disenador DEBE cubrir/expandir)

1. **Formato de escena editable:** ¿nuevo `.mgscene` (JSON/bin) propio del engine, o
   extender `.obs`? (El `.ont` es bytecode de hermetico, read-only.)
2. **Edicion SDF:** ¿el editor edita SDF a nivel de operadores (node graph CSG) o solo
   propiedades de material sobre instancias `.ont`? (Exponer node graph requiere puente a
   `lenguaje-hermetico` o nuevo serializador.)
3. **Play/Game mode:** ¿desde ya o primero editor estatico? (El motor no tiene scripting de
   gameplay hoy.)
4. **ImGui docking vendored en `external/imgui`** — asumir esto salvo motivo.
5. **Huecos que el disenador debe llenar:** flujos de import/export, estados vacios
   (escena nueva), manejo de errores de carga `.ont`, accesibilidad, atajos, theming,
   comportamiento de gizmos, y cualquier modulo de la seccion 5 que hoy no tenga datos en
   el engine (como se veria cuando exista).

---

## 10. Contexto de dependencias y pipeline de escenas (para no romper nada)

- **`.ont`** = bytecode SDF + materiales + camara/luces (producido por `lenguaje-hermetico`,
  leido por el motor). El contrato esta en `render/scene.h`.
- **`.obs`** = observacion (camara/luces/timeline) que acompana al `.ont`.
- **`lenguaje-hermetico`** (`deps/`) = compilador `.herm` -> `.ont`/`.obs`. Es el productor
  de geometria; el motor es el consumidor/visor.
- **`MLIR-CampoTensorial`** (`deps/`) = pipeline offline opcional de pre-bakeado de
  imagenes (4 dialectos). No bloquea la UI, pero un panel "Pipeline/Bake" podria usarlo.
- **Build:** `setup_deps.bat` -> `build_asmjit.bat` -> `build.bat` -> `build/visor.exe`.
  `VULKAN_SDK` es variable de entorno (fallback `C:\VulkanSDK\1.4.350.0`). Nada inyecta en
  el SDK del usuario.
- **Win32-only.** Cualquier diseno de UI asume Windows.

---

## 11. PROMPT COMPLETO PARA EL AGENTE DE DISENO UI/UX

> Eres un disenador UI/UX de herramientas para motores de videojuegos. Disena la interfaz
> de usuario de un **editor tipo Unity** para el proyecto `motor-grafico` descrito en este
> brief (ver secciones 0–10 de este mismo documento; leelas completas antes de empezar).
>
> Reglas de tu trabajo:
> 1. **No es un motor tradicional**: es un renderer **SDF por raymarching en Vulkan**; la
>    geometria es procedimental (bytecode SDF de `lenguaje-hermetico`), no mallas
>    poligonales editables. Refleja eso en cada decision (Material/Lookdev es el centro,
>    no el modelado; un node graph SDF es de campos de distancia, no de pixeles).
> 2. **Cuestiona este brief.** Escudrina, cuestiona, expande y cubre todo lo ausente y lo
>    que se haya omitido. Senala contradicciones, huecos y mejores alternativas a las
>    referencias (Unity/Godot/Marmoset/Substance).
> 3. La UI usa **Dear ImGui (C++)** con paneles dockables (rama docking) sobre un viewport
>    Vulkan. Disena pensando en eso (widgets ImGui, layout por DockSpace, viewport como
>    textura).
> 4. Entrega: (a) el layout/diagrama final refinado (puede ser ASCII o describirlo con
>    precision), (b) descripcion detallada de cada panel/modulo y sus widgets, (c) el flujo
>    de trabajo del usuario (componer escena -> editar material -> render/lookdev ->
>    animar -> guardar), (d) propuesta de theming/estados/errores, (e) lista de lo que el
>    motor aun NO soporta y como la UI debe representarlo (deshabilitado, placeholder, etc.),
>    (f) cualquier modulo adicional que un motor de videojuegos real necesite y que este
>    brief no mencione.
> 5. No implementes codigo; produces especificacion de diseno (estructura, jerarquia de
>    paneles, widgets, flujos, y justificacion de cada eleccion frente a las referencias).
