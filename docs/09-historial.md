# 09 — Historial de la sesión

## 2026-06-16 — GPU Estable, Safety, Benchmark, Documentación

### Contexto

El pipeline GPU Vulkan existía desde v0.23 pero con tres problemas críticos:
1. BVH struct layout incorrecto (`std430` vs C++ 52 bytes) → BVH traversal roto → fondo gris
2. Sin protecciones contra GPU hangs → crash del sistema con watchdog timeout
3. Sin benchmark cuantitativo → no se sabía el rendimiento real

### Diagnóstico y fixes

**Bug 1: BVH struct alignment.** El struct C++ `OntBvhNode` (52 bytes con `#pragma pack(1)`) se declaraba en GLSL con `layout(std430)`. std430 alinea `vec4` a 16 bytes, dando stride de 64 bytes. Los campos `skip_index`, `first_node`, `node_count/flags` se leían en offsets incorrectos.

Fix: `#extension GL_EXT_scalar_block_layout` + `layout(scalar)` en todos los SSBOs. El layout coincide byte a byte con C++.

**Bug 2: node_count/flags bit swap.** El comentario en el shader decía "upper 16 = count, lower 16 = flags" pero la memoria C++ little-endian tiene `node_count` (uint16_t) primero, luego `flags` (uint16_t). Al leer como `uint32`, lower 16 bits = `node_count`, upper 16 = `flags`. El código hacía `count = val & 0xFFFF` (correcto) pero el comentario estaba al revés.

**Bug 3: Missing memory barrier.** `output_buffer` se escribía en `COMPUTE_SHADER_BIT` pero se leía en `TRANSFER_BIT` sin barrera de visibilidad. Fix: `VkBufferMemoryBarrier` entre ambas etapas.

**Bug 4: BLACK_BRUSH en viewport class.** `BeginPaint` en `WM_PAINT` llenaba el viewport con negro sólido, tapando el swapchain Vulkan. Fix: `GetStockObject(NULL_BRUSH)` en la window class.

**Bug 5: GPU hang (sistema crash).** BVH traversal y bytecode VM sin límite de iteraciones. BVH con datos corruptos (o struct mal alineado) causaba loop infinito → watchdog GPU → crash de sistema.

Fix: límite de 65535 iteraciones en ambos loops. También añadido bounds check `gi < graph_nodes.length()` que existía en C++ pero no en GLSL.

### Implementaciones adicionales

- **Camera smoothing:** velocity smoothing exponencial para FREE_FLY, zoom smoothing para ORBIT
- **Test pattern (F3):** `vkCmdClearColorImage` llena swapchain con rojo — aísla bugs de presentación
- **FPS en title bar:** title de ventana muestra FPS y ms/frame en modo ONT
- **Cleanup seguro:** `vkDeviceWaitIdle` en `cleanupSwapchain()` y `cleanup()` elimina validation errors

### Benchmark

Se ejecutó `--bench-vulkan` sobre catedral_hermetica_0000.ont:
- 200 frames completados sin crash
- Media: 281 ms/frame (3.6 FPS) a 974×617
- Sin validation errors de cleanup

### Documentación

Actualización completa de docs/:
- `00-indice.md`: descripción refleja GPU primario
- `02-arquitectura.md`: sección de GPU safety + benchmark
- `05-implementacion.md`: GPU layer completa (SSBO, UBO, shader, pipeline, sync)
- `06-metricas.md`: benchmark GPU, líneas de código actualizadas
- `07-complejidad.md`: GPU complexity + profiling
- `08-changelog.md`: v0.24 con todos los cambios
- `09-historial.md`: esta entrada
- `11-estado-y-deuda.md`: estado actualizado, benchmark, deuda GPU
- `12-brainstorming-optimizacion.md`: sección GPU añadida

---

## 2026-06-10 (continuación) — Escena catedralicia, terrain blend, timeline loop

Esta y las siguientes secciones corresponden a la sesión original del 4-12 de Junio.

## 2026-06-10 (continuación) — Escena catedralicia, terrain blend, timeline loop

Integración con `catedral_hermetica.herm`:
- Visor carga `catedral_hermetica_0000.ont` por defecto (path relativo al exe)
- Fallback en cadena: catedral → test_custom.ont → test_suelo.rih
- Timeline loop: `fmod(time, w_max)` desde `.obs` cuando `has_timeline` — las 60 W-frames de la catedral loopan automáticamente

PipelineConfig extendido con `terrain_blend_strength` (0=off, 0.6=terreno):
- Reduce el brillo metálico en superficies de terreno SDF
- Usado por `ray_march.h` en `shadeOnt()` como `terrain_blend = (1.0f - metallic) * pl.terrain_blend_strength`

Fixes en el pipeline `.ont` del compilador Herm:
- `flattenNode()` ahora aplica transform local a SDF e INSTANCE (no solo GROUP)
- Esto corrige la herencia de transforms en jerarquías de grupos para el pipeline `.ont`

## 2026-06-10 — Optimizaciones: material-finding + SIMD Packet Tracing

Material-finding eliminado: `bvhEval()` acepta `u32* out_material` opcional, rastrea
`best_mat` durante el recorrido BVH. `rayMarchOnt()` pasa `&r.material` en el hit step,
eliminando el recorrido BVH redundante post-hit (~30 líneas).

SIMD Packet Tracing (`render/ray_march_simd.h` nuevo):
- `execBcRaw4()` — bytecode VM SSE 4-wide (intrínsecos aritméticos, fallback escalar para
  SIN/COS/TAN/POW/MOD/SAMPLE)
- `applyMatrix4()` — transformación SSE 4×4 para 4 puntos
- `bvhEval4()` — BVH lockstep para 4 rayos con manejo de divergencia por lane
- `renderOntSceneSIMD()` / `renderOntSceneMTSIMD()` — render single/multi-thread con paquetes de 4 píxeles

Rendimiento (test_custom.ont):
- 1000×700: scalar ST 77ms → SIMD ST 40ms (1.9×); scalar MT 16ms → SIMD MT 8ms (2.0×)
- Full HD MT: scalar 40ms → SIMD MT 28ms / 36 FPS — interactivo por primera vez

Fix visor: removido test_custom.ont/.obs del build dir (overrideaba escena default).
Corregido path fallback `LENGUA~1` → `Lenguaje Hermetico`.
Fixes SIMD: per-lane eps, sky rendering en MT SIMD.

## 2026-06-08 — Pipeline Ontológico (.ont + .obs) e interacción

Formato .obs (Observation) binario: secciones opcionales para camera, lights, timeline,
background, resolution. Archivo independiente del .ont.

Pipeline completo: Herm compiler escribe .obs automáticamente tras .ont.
Visor carga .obs al iniciar, aplica cámara/luces/timeline.

Multi-light PBR shader (Cook-Torrance GGX), WASD navigation, mouse orbit (botón medio),
F1 toggle FREE_FLY mouse look, adaptive resolution (0.25× durante movimiento, 1× tras 400ms idle),
FPS overlay, inside-geometry fix (t arranca en 0.01, d >= 0.0, stepping con abs(d)),
multi-threading (renderOntSceneMT, 7× speedup), init de cámara con atan2/asin.

## Inicio: "¿Qué hicimos hasta ahora?"

El proyecto `Motor Grafico` existía con:
- `core/` — código legacy (rih_reader, sdf_eval, renderer) funcional pero con bugs
- `os/` + `rhi/` — capas nuevas ya implementadas y testeadas (36 tests)
- `render/` — capa nueva a medio terminar (scene.h/cpp, sdf_eval.h/cpp, ray_march.h)
- `visor/main.cpp` — usaba core/ (legacy), no las capas nuevas
- 90 tests de render fallando por bugs

## Fase 1: Corrección de bugs

### Bug 1: `skipValue()` infinite loop
Parser JSON artesanal en `render/scene.cpp`. `skipValue()` saltaba strings y arrays
pero no objetos correctamente — solo leía el primer key y se quedaba en `:`.
Fix: `match(':'); skipValue();` en el path de objetos.

### Bug 2: ID→index circular reference
RIH usa IDs de nodo (enteros arbitrarios). El `children` de un GROUP contenía IDs,
pero el código los trataba como índices de array. Mesa (id=0) caía en índice 5,
y su children `[1,2,3,4,5]` incluía índice 5 (mesa misma) → recursión infinita.
Fix: `unordered_map<u32,u32> _id_to_idx` + `idToIndex()`.

### Bug 3: CRT estático
Build con `/MT` causaba proceso colgado al iniciar. Fix: `/MD` en build scripts.

### Bug 4-5: Tests
- Expresión `0.65+sin(w*2)*0.35` → `0.65+sin(w)*0.35` (sin(w*2) con w=π/2 da sin(π)=0)
- Normal sign: `z < 0` → `z > 0` (la normal del frente apunta +z hacia cámara)

Resultado: 90/90 render + 36/36 OS+RHI.

## Fase 2: Diseño de capa Scene

Debate sobre:
- **Cámara como player**: CameraController con modos ORBIT/FREE_FLY/FOLLOW
- **Visibilidad SDF vs rasterización**: En SDF, objetos fuera del frustum pueden afectar
  píxeles vía shadow rays/reflejos/refracción. No sirve frustum culling. Solución:
  BVH + ray-AABB query por rayo.
- **Formato de proyecto**: .mgproject guarda referencias a fuentes + workspace state,
  NO dump del grafo. El grafo se regenera desde RIH cada carga.
- **Separación scene/render**: scene/ maneja grafo + transform + BVH;
  render/ consume nodos visibles + matrices precalculadas.

Se implementó:
- `scene_graph.h`: SceneNode (parent/children/siblings, dirty flags, world transforms),
  SceneGraph (init, setParent, setTransform, updateWorldTransforms, computeLocalAabb, computeWorldAabb)
- `camera.h`: CameraController con 3 modos, orbitRotate, zoom, mouseLook, applyTo
- `scene_query.h`: Bvh (top-down, split por centroide, hojas ≤4, prof. max 16)
- `project.h`: .mgproject texto UTF-8, save/load con FileMapping
- `workspace.h`: Viewport, Layer (visible/solo), Timeline (play/pause/loop)
- 82 tests, build_scene_test.bat

## Fase 3: Refactor de visor

Se reescribió `visor/main.cpp` para usar las capas nuevas:
- Reemplazó `core/core.h`, `core/rih_reader.h`, `core/renderer.h`
  → `render/scene.h`, `render/render.h`, `scene/...`
- `Renderer` reemplaza `Rih` + `RenderConfig` + `Framebuffer` + `render()`
- `CameraController` con ORBIT por defecto
- `SceneGraph` + BVH + AABBs computados al cargar escena
- `build.bat` actualizado (sin core/)

## Fase 4: Optimizaciones

Debate sobre 3 optimizaciones:
1. **AABB early-out**: punto a AABB es lower bound del SDF. Si > best, skip.
2. **BVH integration**: query por rayo, filtrar nodos visibles.
3. **Epsilon adaptativo**: `eps = hit_eps * (1 + t * 0.01)`.

Implementación:
- `Aabb` movido a `render/scene.h` con métodos completos
- `evalScene`/`findClosestNode`/`calcNormal` con parámetros AABB + filter
- `SceneQuery` struct (callback + aabbs, sin acoplar render/ y scene/)
- `renderScene` overload con SceneQuery
- 11 nuevos tests

## Fase 5: Documentación

Creación de `docs/` completo:
- 00 índice + 10 documentos de contenido + 1 de estado
- Contrato ontológico del dominio SDF

## 2026-06-11 — El Muro (CPU SDF Performance)

### El problema

Con la catedral herméticaL cargada, el visor rinde ~1 FPS a 990×656. FULL HD es inalcanzable.
Cada rayo recorre miles de nodos BVH para escenas complejas. El SIMD+MT ayuda, pero el cuello
de botella es fundamental: O(nodos) por rayo, y las escenas ontológicas pueden tener
cientos o miles de BVH leaves.

### Diagnóstico

Se analizaron los bytecodes de la catedral: la mayoría de nodos son estáticos (no usan `w`,
no tienen SIN/COS, solo operaciones lineales con constantes). Solo unos pocos nodos animados
requieren reevaluación por frame. La observación clave: **un grid 3D puede cachear el SDF de
nodos estáticos**.

### La consulta a IA

Se pidió a la IA un diseño de grid SDF. La respuesta fue un diseño completo y riguroso:
- Grid uniforme 64³ con celdas de ~0.14 u.m.
- Clasificación estático/dinámico por heurística de bytecode
- Precomputación de SDF estático por celda
- Evaluación híbrida: grid hit → SDF cacheado + dinámicos

La discusión posterior refinó:
- **Static/dynamic node separation** — clasificar BVH leaves, no nodos de escena
- **64³ uniform grid** como V0, evolucionable a SVDDF (brick maps adaptativos)
- **Hybrid evaluation** con safety margin fallback (celdas sin cubrir = evaluar todo)
- **Scalar MT path with grid** — la prioridad no es vectorizar el grid, sino que funcione

### Implementación V0

Se implementó en una sesión:

1. `SdfGrid` struct (render/scene.h) — grid 64³ con offsets + data planos
2. `classifyOntNodes()` — heurística de bytecode (SIN/COS/TAN/POW/MOD/SAMPLE/w)
3. `buildSdfGrid()` — rasteriza AABBs de nodos estáticos, evalúa SDF por celda
4. `gridSample()` — lookup de celda por punto 3D
5. `evalHybrid()` — decisión grid→cache vs dinámicos
6. `evalDynamicNodes()` — evalúa subconjunto de BVH por bitmask
7. Integración en `render.h` y `rayMarchOnt()`

Build exitoso. Visor lanzado con catedral. Pendiente benchmark cuantitativo.

## Cierre

**219/219 tests, 0 fallos.**
Motor Gráfico funcional con arquitectura limpia en 5 capas + grid SDF V0.
