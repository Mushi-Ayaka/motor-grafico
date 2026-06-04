# 09 — Historial de la sesión

Esta es la transcripción resumida de la sesión completa del 4 de Junio de 2026.
Todo el trabajo se realizó en una sola sesión continua.

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

## Cierre

**219/219 tests, 0 fallos.**
Motor Gráfico funcional con arquitectura limpia en 5 capas.
