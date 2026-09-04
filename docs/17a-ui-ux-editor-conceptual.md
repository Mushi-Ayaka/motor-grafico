# 17a — Esquema Conceptual y Layout del Editor (`motor-grafico`)

> **Parte 1 de 3** del esquema de diseño UI/UX. Continúa en `17b` (catálogo de módulos) y `17c` (ingeniería / paths).
> Documento vivo: se critica y reconstruye en iteración. Base filosófica en `motor gráfico.txt` (manifiesto del proyecto).

---

## 0. Identidad del editor (y crítica al brief `docs/16`)

El brief `docs/16` enmarca el proyecto como *"editor tipo Unity / game engine"*. Eso es **parcialmente correcto y parcialmente engañoso**:

- **Correcto:** el motor se usa para **crear videojuegos y exportar ejecutables jugables**. Por eso Play/Stop, la noción de "systems" y el export importan de verdad.
- **Engañoso:** filosóficamente el proyecto es un **compilador de universos declarativos y herméticos** (SAAC → tensor compiler → Lenguaje Hermético). No es un motor poligonal: no hay mallas, no hay texturas binarias, no hay audio binario por defecto, la geometría es SDF por raymarching, y el tiempo (W) es una dimensión geométrica (Block Universe).

**Veredicto:** el editor es un **IDE front-end de un compilador hermético tensor/SDF**, con herramientas de lookdev, navegación W, authoring de *systems* y export a ejecutable. Las referencias Unity/Godot aplican a *ergonomía de shell*; Marmoset a *lookdev*; la referencia "Substance" debe sustituirse (ver §3).

Otros fallos del brief ya detectados:
- Lookdev hoy está **roto** en GPU (1 luz hardcodeada, sin AO/sombras/HDR, bug de `.ont` → materiales Fresnel blancos).
- Picking en raymarch **no es trivial** (sin depth buffer).
- Falta **Undo/Redo** en el diseño.
- **a11y en ImGui está sobreprometida**: techo real = teclado + contraste de theming + fuente configurable, no AT/WCAG.

---

## 1. Principios de diseño (SDF-aware + hermético)

1. **Mundo = campo tensorial 4D `XYZWRGBA`.** X,Y,Z espacial; **W = tiempo como geometría** (no reloj); RGBA color/opacidad. Render = corte transversal en W (Block Universe, Page-Wootters).
2. **Geometría por SDF + raymarching.** `Φ(p)<0` dentro, `=0` superficie, `>0` fuera. Materiales **PBR procedurales** (sin texturas externas; "maps" = funciones analíticas).
3. **Declaración, no modelado.** 2 caminos de declaración → `.ont`: (a) fuente `lenguaje-hermetico` (`.herm`), (b) node graph visual. Ambos vía **lib `lenguaje-hermetico` vendorizada en runtime**.
4. **Tensores 1×8 = spans del CTP (IR canónico)** del pipeline GTE→PTR→CTP→TB. Es el **modelo de datos canónico**, no una invención ad-hoc. El engine lee esta IR; los bridges escriben en ella.
5. **Pirámide Ontológica F/Q/B.** Física (forma/transform) → Química (material/color) → Biología (herencia/ensamblaje). Herencia de arriba abajo. **No son tres árboles separados**: es UNA escena donde cada nodo lleva facetas F/Q/B; el Árbol Ontológico puede *filtrar/agrupar* por capa.
6. **Operadores P/I/K.** Perturbación (crear), Influencia (modificar), Aniquilación (×0). Fundamentales en toolbar y node graph.
7. **Hermetismo y determinismo.** Reproducible bit-bit, sin dependencias externas *en el mundo*, sin RNG. (Ver §5, Política de Determinismo.)
8. **El mundo corre SYSTEMS propios.** Lógica de juego, física relacional, redes/multijugador, audio, UI — authoring en el editor, ejecutados en el ejecutable exportado. Cualquier sistema "emulable" es potente bajo este paradigma.
9. **Canales extendibles.** El modelo `XYZWRGBA` admite campos analíticos adicionales: **audio = amplitud A(x,y,z,w)** (función analítica) muestreada en un listener path; **UI = superficies SDF**. Ambos herméticos/procedurales, sin binarios.
10. **Assembly at export.** El proyecto se authora como **fragments** (systems/subsystems en scripts + declaraciones SDF); el export los **enlaza/compila** en el ejecutable (modelo toolchain). La "verdad" = el conjunto de fragments; el linkage ocurre en build, no en el editor.
11. **Dominio acotado.** `XYZWRGBA` tiene límites de dominio (o infinito explícito); `Φ(p)` nunca queda *undefined/null* fuera de dominio (se define como "fuera"/infinito acotado). La navegación del viewport es config de *visualización*, no parte del mundo.
12. **Una fuente por declaración.** Dentro de una declaración SDF, `.herm` es la fuente canónica (texto); el node graph es una vista que lo edita (round-trip puede perder comentarios). No se mantienen dos representaciones editables.

---

## 2. Referencias: tabla de mapeo por panel

El brief dice "mezcla Unity/Godot/Marmoset/Substance" sin decir qué informa qué. Mapeo explícito:

| Referencia | Qué aporta (y qué NO) |
|---|---|
| **Unity / Godot** | Ergonomía de shell: DockSpace, layouts guardables/recreables, MenuBar/Toolbar, Play/Stop, Systems. **NO** el modelo de "GameObject/malla". |
| **Marmoset** | Lookdev en tiempo real, viewport como "stage", enfoque en materiales/iluminación. |
| **Substance** | ❌ **Rechazada.** Se basa en librerías de texturas/binarias → contradice hermetismo. Sustituir por **PBR procedural** (shaders analíticos tipo OSL/Renderman, Blender procedural). |
| **Game-engine (realidad de uso)** | Export a ejecutable jugable, authoring de systems, multijugador vía bridges. |

---

## 3. Modelo conceptual del editor

```
Editor IDE = Declaración (.herm / node graph  <->  RIH)
           + Lookdev (PBR procedural, environment)
           + Navegación W (Block Universe scrubber)
           + Authoring de SYSTEMS (juego, física, red, audio, UI)
           + Export  ->  ejecutable jugable
                |
                v
        lib lenguaje-hermetico (runtime)
                |
                v
   .herm / node graph  ->  RIH (CTP 1x8)  ->  Bytecode .ont  ->  GPU (raymarch)
```

- **Declaración** (cómo es el mundo) vs **Composición/instanciación** (dónde se coloca) vs **Systems** (qué corre encima) son ejes distintos y ortogonales.
- Live-compile a **bytecode antes de GPU** (el engine no interpreta strings en vivo — coherente con el manifiesto).

---

## 4. Matriz de Redefinición de Paneles

El brief copia el layout Unity. Cada panel se **redefine** para este motor:

| Panel tradicional | Equivalente en este engine | Por qué |
|---|---|---|
| **Hierarchy** | **Árbol Ontológico** (scene graph único con facetas F/Q/B por nodo; filtrable por capa) | No hay GameObjects-malla; cada nodo lleva Física/Química/Biología y se agrupa por herencia. |
| **Inspector** | **Inspector por facetas F/Q/B** del nodo seleccionado | Refleja la pirámide ontológica, no props de mesh. |
| **Timeline** | **W-Scrubber** (Block Universe) + keyframes como abstracción opcional | W es geometría; el W-Scrubber *inspecciona* el 4D autorado. El **Play** genera W *interactivamente* vía Systems (no es solo cortar un bloque fijo). |
| **Project / Content** | **Source Browser** (`.herm`, scripts, `.mgproj`) + **Export** | No hay texturas/audio binarios de entrada; esos son *salida*. El browser maneja fuentes del proyecto. |
| **Material Editor** | **PBR procedural** (sin slot de textura; funciones analíticas) | Hermetismo: sin texturas externas. |
| **Play / Stop** | **Play W / Run Systems** (corre systems en el mundo / ejecutable) | Sí es motor de juegos; se queda. |
| *(nuevo)* **Audio System** | Síntesis analítica hermética (procedural) | Recreable bajo el paradigma, igual que PBR. |
| *(nuevo)* **UI System** | UI hermética (SDF-UI north-star) que el mundo corre | Mismo poder emulable. |
| *(nuevo)* **Systems / Game-logic** | Authoring de systems que el mundo ejecuta | "Puedes crear tus propios sistemas que el mundo puede correr." |

---

## 5. Política de Determinismo (principio UX)

Hermetismo exige **reproducibilidad bit-bit**. El editor la garantiza con dos reglas:

- **Semillas prohibidas:** el editor **no usa RNG** para generar el mundo. Todo procedimiento es puro/determinista (semilla fija o función analítica). Un "randomize" implícito rompería la hermetidad.
- **Estado puro:** el mundo es **función pura de su declaración + su stream de input**; no hay globals mutables ocultos. Undo/Redo y Live-compile son **transformaciones puras del AST**, no mutación imperativa de estado global. *Misma fuente + mismo input ⇒ mismo bytecode*; el Play interactivo varía solo por el input del jugador.

Implicaciones UX: sin "random" oculto en previews, previews reproducibles, y el Tensor Inspector muestra valores exactos (no aproximados).

---

## 6. Live-compile & Anomaly Guard (arquitectura)

Objetivo: edición en vivo **sin regenerar el mundo** y **sin exigir mucho**.

1. **Recompilación (alcance v1 vs futuro).** El manifiesto reporta compiles en *microsegundos por bloque*: para v1 un **recompile completo de `.ont`** es suficientemente rápido y evita depender de carga parcial. La *arquitectura especial que ahorra tiempo* es el **Scheduler + Input Bus** (§6.2–6.5), no el subtree-patch. El **hot-swap de sub-árbol** queda como optimización futura y **exige que la Bytecode VM soporte `loadSubtree`/`patch`** (gap de motor, ver `17c`).
2. **Scheduler de compilación** (no solo debounce). Cola de prioridad con *coalescing*: keystroke en `.herm` o conexión de nodo encolan un "compile job" con su *dirty-set*. Ejecución en **hilo background**; el viewport consume el último `.ont` válido. Debounce ~250ms.
3. **Two-tier compile.** (a) *Parse/validate* síncrono y barato en cada edición → errores de sintaxis + **anomaly check estático** (ops NaN-prone, `repeat` sin dominio acotado, CSG auto-intersecante) inmediato, sin tocar GPU. (b) *Bytecode+`.ont`+upload* asíncrono/debounced.
4. **Anomaly Guard integrado en (a):** si el AST es semánticamente degenerado, ni se genera bytecode; el viewport **conserva el último frame bueno** (sin overlay complejo: en HW débil un diagnóstico elaborado puede colgarse/crashear). `trace_t_max` clamp + detección no-finito evitan bucles.
   - **Capa (b) — detector de anomalías en tiempo de ejecución** (complementario): usa el Readback del Tensor Inspector para validar *correlaciones relacionales* a lo largo de W y detectar anomalías semánticas que el check estático no ve (entidades que divergen cuando deberían correlacionar, tensores no-finitos en ciertos W). Ver `17b` módulos 10 y 29.
5. **Input/Event Bus unificado:** capa fina `IInputSource` por donde alimentan ImGui **y** la futura SDF-UI, enrutando al scheduler. Sin polling por frame, con coalescing. Cubre también modo Play: inputs de runtime alimentan el Scripting Bridge (mutación de grafo→tensores), no el loop de raymarch.

---

## 7. Renderizado de la UI

- **v1:** ImGui/Qt (bootstrap pragmático, DockSpace).
- **North-star (post-v1, investigación):** **SDF-UI** — *in-game UI* como sistema hermético que el ejecutable corre (HUD/menús como nodos SDF). Distinto de la **UI del editor** (ImGui v1). El audio también es recreable herméticamente (síntesis analítica, ver §1.9). Ambos demuestran el poder emulable del paradigma.

---

## 8. Scripting Bridge (corrección: PUENTES, no solo mutación)

- **Python / C son para PUENTES** entre el mundo hermético y **módulos externos** (servidor, multijugador, I/O). Escriben/leen **tensores 1×8 (IR CTP)** del grafo y corren como módulos compilados.
- **C → `.dll`/`.so` con hot-reload en caliente** (máx velocidad nativa, compilado en background).
- **Nunca por-píxel**: el bridge no corre dentro del loop de raymarch. Puede invocarse **por-frame (event-driven)** para empujar estado a la IR a frame-rate (p.ej. leer un paquete de red y escribir tensores); el C++ lee los tensores para renderizar. No es estrictamente "una vez", sino "nunca por-píxel".
- Distinto del **lenguaje científico de scripting relacional** (futuro, según manifiesto): aquel muta el grafo internamente; el bridge conecta afuera.

---

## 9. Layout ASCII (DockSpace redefinido)

```
+------------------------------------------------------------------------------+
| MenuBar: File Edit GameObject Component Systems Window Help [> Play][# Stop] |
+------------------------------------------------------------------------------+
| Toolbar: [P][I][K] | Move Rotate Scale | Grid Snap | Mode: Edit / Play     |
+--------+---------------------------------------------------+--------------------+
| ONTO   |                                                   |  INSPECTOR F/Q/B   |
| F/Q/B  |            VIEWPORT  (corte W del hipervolumen)    |  - Fisica          |
|  +-F   |      [ raymarch Vulkan en vivo ]                  |  - Quimica         |
|  +-Q   |      + grid + gizmos + seleccion (ID pass)        |  - Biologia        |
|  +-B   |      [ Render | IR/Topology | Tensor ]            |  - Systems         |
| [+Nodo]|                                                   |  - Scripts/Bridge  |
+--------+---------------------------------------------------+--------------------+
| SOURCE BROWSER |  W-SCRUBBER (timeline W)  |  TENSOR INSPECTOR   |
| .herm scripts  |  [====W====]  play/pause  |  (readback buffer)  |
| .mgproj        |                          |  correlaciones      |
+------------------------------------------------------------------------------+
| CONSOLE / LOG (diag, errores, anomaly)  |  PROFILER (FPS, dispatches)   |
+------------------------------------------------------------------------------+
| STATUS: FPS . ms . draws . modo[VK/CPU] . mundo actual . W=valor              |
+------------------------------------------------------------------------------+
```

- Paneles **dockables** (ImGui `DockSpace`); layouts **guardables/recreables** estilo Unity.
- VIEWPORT muestra el render vía `ImGui::Image` (textura offscreen). Pestañas = **Render (corte W) | IR/Topology | Tensor**, no "Scene|Game".

---

## 10. Flujos de usuario

1. **Declarar mundo:** escribir `.herm` o arrastrar node graph (P/I/K, CSG, domain folds) → live-compile a `.ont`.
2. **Componer:** colocar instancias en el Árbol Ontológico F/Q/B con transforms + override PBR.
3. **Lookdev:** Material PBR procedural + environment (cuando exista HDR/IBL).
4. **Navegar W:** W-Scrubber para ver la película 4D (Block Universe).
5. **Authoring de Systems:** lógica de juego, física relacional, bridge multijugador (Python/C), audio, UI.
6. **Export:** compilar mundo + systems → **ejecutable jugable**.

---

## 11. Open questions (para `17b` / `17c`)

- **AST vs `.herm` ownership:** AST canónico en memoria (`.herm` = serialización round-trip, UI graph sidecar en `.mgproj`); edición manual de `.herm` entra en "modo fuente" que deshabilita el graph.
- **Hot-reload C:** v1-opcional; fijar ABI de tensores 1×8 ahora para compartir Python/C.
- **Theming lookdev:** tokens Dark `#0E0F12`/`#1B1E24`/`#23272F`, borde `#2E333D`, texto `#E6E8EC`/`#9AA1AC`, 1 acento (`#3DD6C4` o `#E8A33D`), error/anomalía `#FF6B6B` pulsante.
- **`.mgproj` (ex `.mgscene`):** JSON puro, referencias externas a `.ont`/`.herm`/scripts; `schema_version`.
- **ID pass** para picking (ray-SDF 3D caro → necesita buffer de IDs).
- **SDF-UI north-star:** path de investigación.
- **Subtree-load en Bytecode VM:** gap de motor para live-compile incremental (optimización futura; v1 recompila completo).
- **Selección de camino de declaración:** cómo el usuario inicia node graph vs `.herm` (toolbar/MenuBar/New); el node graph persiste en `.mgproj` (AST + UI graph), `.herm` es el camino textual.
