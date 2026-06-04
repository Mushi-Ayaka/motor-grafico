# 08 — Changelog

## Sesión única (2026-06-04) — Auditoría, fixes, capa Scene, visor, optimizaciones

Una sola sesión continua desde "¿Qué hicimos hasta ahora?" hasta el cierre.

### Fase 1: Diagnóstico y fixes (inicio de sesión)

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

### Fase 2: Capa Scene (diseño + implementación)

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

### Fase 3: Refactor visor

Se reemplazó `core/` (legacy) por `render/` + `scene/` + `os/`:
- `Renderer` reemplaza Rih + RenderConfig + render()
- CameraController con ORBIT por defecto
- SceneGraph + BVH + AABBs computados al cargar
- Status bar actualizada
- build.bat actualizado

### Fase 4: Optimizaciones (debate + implementación)

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

### Fase 5: Documentación

Creación de `docs/` con 12 documentos:
fundamentos, arquitectura, especificaciones, guía de uso, implementación,
métricas, complejidad, changelog, historial, contrato ontológico, estado y deuda.

Resultado final: **219/219 tests, 0 fallos.**
