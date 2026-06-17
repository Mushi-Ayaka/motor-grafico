# Contrato Ontológico — Motor Gráfico SDF

## Dominio

Renderizado fotorrealista de escenas definidas exclusivamente por Signed Distance Fields (SDF)
analíticos, expresiones paramétricas, y composición booleana. Sin mallas poligonales,
sin texturas externas, sin redes neuronales, sin ruido estadístico, sin post-processing.

## Entidades

| Entidad | Definición | Propiedades |
|---------|-----------|-------------|
| **Scene** | Contenedor de nodos, materiales, luces | version, width, height, frames, camera, background |
| **Node** | Elemento del árbol de escena | id, name, type, mode, material_id, transform, sdf |
| **SdfNode** | Primitiva o booleano SDF | sdf_type, params[4], child_a, child_b, displace_expr |
| **Material** | Apariencia de superficie | base_color, roughness, metallic, emission, ior, opacity |
| **Light** | Fuente de luz | type (DIRECTIONAL|POINT), direction, position, color, intensity, falloff |
| **Camera** | Punto de vista | position, target, up, fov |
| **Expr** | Expresión paramétrica evaluable | is_expr, constant, expression |
| **Vec3** | Vector 3D | x, y, z |
| **Aabb** | Caja alineada a ejes | min, max |
| **SceneNode** | Nodo de grafo (extiende Node) | parent, children, dirty, world transform, aabb |
| **SceneGraph** | Grafo jerárquico con AABBs | vector de SceneNode, dirty propagation |
| **CameraController** | Control de cámara interactivo | mode (ORBIT|FREE_FLY|FOLLOW), orbit params |
| **Bvh** | Aceleración espacial | árbol binario, query ray-AABB |
| **BrickMap** | Sparse Voxel Octree-lite para geometría SDF estática | origin, top_cell_size, voxel_size, top_indices[], bricks[] |
| **Project** | Archivo de proyecto | sources, camera snapshot, workspace state |
| **Viewport** | Ventana de render | position, size, camera_node |
| **Layer** | Grupo de visibilidad | name, visible, solo, opacity |
| **Timeline** | Control temporal | current_time, current_frame, playing, loop |

## Relaciones

```
Scene 1──N Node (árbol)
Scene 1──N Material
Scene 1──N Light
Scene 1──1 Camera
Node 1──1 SdfNode (si type=SDF)
SdfNode 0──2 Node (child_a, child_b para boolean ops)
Node 0──N Node (children para GROUP)
Node 0──1 Node (def para INSTANCE)
SceneGraph 1──N SceneNode
SceneNode 1──1 Aabb (local)
SceneNode 1──1 Aabb (world)
Bvh 1──N BvhNode (árbol binario)
BvhNode 0──1 SceneNode (leaf)
Project 0──N ProjectSource
Workspace 1──N Viewport
Workspace 1──N Layer
Workspace 1──1 Timeline
```

## Invariantes

1. `Node::id` es único dentro de una Scene
2. `Scene::_id_to_idx` mapea todos los IDs a índices de array
3. `Node::is_compound_child` excluye nodos inline de `evalScene()` (solo se evalúan via GROUP)
4. `SceneNode` indices son paralelos a `Node` indices (1:1 mapping)
5. BVH solo se construye si hay nodos SDF leaf no-compound
6. AABB early-out es conservador: nunca descarta un nodo que podría estar más cerca que el mejor SDF actual
7. `w` es la variable de tiempo canónica; `t` es alias retrocompatible
8. `FileMapping` es el único mecanismo de I/O — prohibido `std::ifstream`/`std::filesystem`
9. **`d_min` Conservador absoluto (`0.0f`)**: El motor asume siempre una distancia mínima al vacío de `0.0f` para nodos con geometría en su AABB. Valores "seguros" (ej. `0.01f`) causan agujeros negros. El motor invalida explícitamente cualquier `d_min > 0.005f` como mitigación defensiva.
10. **Budget Espacial Dinámico**: El bytecode dinámico nunca se evalúa ciegamente. Debe superar primero una validación AABB de bajo coste.

## Estrategias de renderizado

### Ray marching
```python
for step in 0..max_steps:
    d = evalScene(scene, p, w)
    if d < eps: return hit(p, normal, material)
    t += d
    if t > max_dist: return miss
```

### Pipeline ontológico (.ont + .obs)

1. Compilador Hermético produce `.ont` (bytecode SDF + BVH) y `.obs` (cámara, luces, timeline)
2. Visor carga `.ont` → `OntScene`, `.obs` → `OntObservation.applyObs(scene)`
3. `.obs` es opcional — si no existe, defaults: cámara (0,0,3)→(0,0,0), fov=50, fondo negro
4. `.obs` es extensible por flags — nuevas secciones no rompen compatibilidad

### SIMD Packet Tracing

1. 4 píxeles adyacentes → 4 rayos → BVH lockstep (`bvhEval4`)
2. AABB SSE test en cada nodo → máscara 4-bit de lanes activas
3. En hoja: transformación SSE 4×4 (`applyMatrix4`) → bytecode VM SSE (`execBcRaw4`)
4. Divergencia (cuando 4 puntos tocan diferentes AABBs): lanes individuales
5. Coherente (caso común): full SIMD throughput

### Shading (Cook-Torrance GGX multi-luz)
```python
for light in lights:
    ldir = normalize(light.direction)
    ndotl = max(dot(normal, ldir), 0)
    half = normalize(ldir + view_dir)
    spec = pow(ndotl_half, (1-roughness)*128+1)
    if metallic: result += base_color * (spec*0.8 + ndotl*0.2)
    else: result += base_color * ndotl*0.7 + spec*0.3
result += emission
```

### Optimizaciones

1. **AABB early-out**: distancia punto-AABB > best → skip
2. **Epsilon adaptativo**: `eps = hit_eps * (1 + t * 0.01)`
3. **BVH**: árbol espacial → solo evaluar hojas intersectadas por el rayo
 4. **Transform precompute**: array plano de translate por nodo (evita recomputar applyTransform)
 5. **Material-finding integrado en bvhEval**: out_material opcional, evita segundo BVH traversal post-hit
 6. **SIMD packet tracing**: execBcRaw4 + bvhEval4, 4-wide SSE
  7. **Multi-threading + SIMD**: renderOntSceneMTSIMD, bandas de filas con paquetes SIMD cada una
  8. **BrickMap V1**: buildBrickMap (precomputa SDF estático en sparse voxels) → sampleBrickMap (lookup trilineal + dinámicos JIT)
  9. **Budget Espacial Defensivo**: La evaluación dinámica o compilada usa AABB checks (escalares o mediante SIMD en `bvhEval4`/`evalBrickHybrid4`) para suprimir los cuellos de botella de ejecuciones inútiles.

## Contrato de capas

```
visor/ → scene/ → render/ → os/
visor/ → rhi/   → os/
scene/ → render/ (solo scene_graph.h incluye render/scene.h)
render/ → os/
rhi/ → os/
```

`render/` NO incluye nada de `scene/`. La comunicación se hace vía:
- `Aabb*` (tipo compartido en `render/scene.h`)
- `SceneQuery` (callback + contexto, definido en `render/ray_march.h`)

## Invariantes adicionales (ontológico)

9. `.obs` debe ser legible sin `.ont` (solo describe observación, no geometría)
10. `execBcRaw4` debe producir exactamente los mismos resultados que `execBcRaw` para cualquier entrada válida
11. El número de threads usados por `renderOntSceneMTSIMD` es `min(n_cores, (height+31)/32)` — no más de uno por 32 filas
12. `bvhEval4` preserva `node_idx[4]` por lane — divergencia nunca contamina otros lanes
13. `evalHybrid` debe producir exactamente el mismo resultado que `evalOntSdfTree` para cualquier entrada (el grid es solo aceleración, no aproximación)
14. `classifyOntNodes` es conservador: un falso positivo (marcar estático como dinámico) es correcto pero sub-óptimo. Un falso negativo (marcar dinámico como estático) es un bug
