# 05 — Implementación

## OS Layer

### Arena (`os/win32/mem.cpp`)

Bump allocator sobre VirtualAlloc:

```
init(N) → VirtualAlloc(MEM_RESERVE, N) → commit 64KB inicial
alloc(s,a) → alinear puntero → commit lazy 64KB si necesario → return ptr
reset() → used = 0 (sin Free/VirtualAlloc)
shutdown() → VirtualFree(MEM_RELEASE)
```

Commit lazy: cuando `needed > committed`, se commiten páginas en bloques de 64KB.
Esto evita commitear 64MB de una vez.

### FileMapping (`os/win32/file.cpp`)

```
open(path) → CreateFileW → GetFileSizeEx → CreateFileMappingW → MapViewOfFile
close() → UnmapViewOfFile → CloseHandle(map) → CloseHandle(file)
```

Usa API `W` (wide) exclusivamente — sin encoding hell con acentos (ej. "Lenguaje Hermético").

### Window (`os/win32/win32.cpp`)

Wrapper mínimo: RegisterClass + CreateWindowEx + PeekMessage loop.
Un solo `self()` estático para el WndProc.

## RHI Layer

### D3D11 Compute Shader (`rhi/d3d11.cpp`)

- Dispositivo: `D3D11CreateDevice` con feature level automático
- Swapchain: 2 buffers, `DXGI_FORMAT_R8G8B8A8_UNORM`
- Constant buffer: `D3D11_USAGE_DYNAMIC`, padded a 16 bytes
- Structured buffer: `D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS`
- Compute shader: `D3DCompile` desde string HLSL, target `cs_5_0`
- Dispatch: unbind UAV post-dispatch para permitir copy al backbuffer

## Render Layer

### Scene loader (`render/scene.cpp`)

Parser JSON artesanal (sin librerías externas). Maneja:
- Objetos, arrays, strings, números, booleanos
- Campos desconocidos via `skipValue()` (que salta cualquier estructura anidada)
- SDF array-form (compounds expandidos inline como GROUP + children)
- ID→index mapping para evitar circular references en GROUP children

### Expression evaluator (`render/sdf_eval.cpp`)

Parser recursivo descendente:
```cpp
struct ExprParser {
    parseExpr()  → parseSum()
    parseSum()   → parseProduct() { '+' | '-' parseProduct() }
    parseProduct() → parsePower() { '*' | '/' parsePower() }
    parsePower() → parseUnary() { '^' parseUnary() }
    parseUnary() → { '-' } parseAtom()
    parseAtom() → number | variable | '(' expr ')' | fn '(' args ')' 
}
```

Sin heap allocation durante parseo — todo en stack.

### SDF primitives (`render/sdf_eval.h`)

| Primitiva | Llamada | Operaciones |
|-----------|---------|-------------|
| sdSphere | `length(p) - r` | 5 FMA |
| sdBox | box SDF estándar IQ | 12 FMA |
| sdCylinder | `max(\|p.xz\| - r, \|p.y\| - h/2)` | 8 FMA |
| sdTorus | `\|\|(\|p.xz\| - R, p.y)\|\| - r` | 10 FMA |
| sdPlane | `p.y + d` | 1 FMA |
| sdCone | IQ aproximación | 14 FMA |

### evalSdfTree (`render/sdf_eval.h`)

Recorre el árbol SDF recursivamente:
1. Aplica transform (translate → rotate → scale) al punto `p`
2. Si `transforms[]` está presente, usa lookup directo (solo translate, más rápido)
3. GROUP: min sobre todos los hijos
4. INSTANCE: evalúa el nodo def
5. SDF: evalúa la primitiva según `sdf_type`

### Ray Marching (`render/ray_march.h`)

```cpp
for step in 0..max_steps:
    p = ray.origin + ray.dir * t
    d = evalScene(scene, p, w, transforms, aabbs, visible_nodes, visible_count)
    eps = hit_eps * (1.0 + t * 0.01)  // adaptativo
    if d < eps: → hit (normal, material, shade)
    t += d
    if t > max_dist: → miss
```

### SceneQuery (`render/ray_march.h`)

Callback opcional para BVH per-ray. RenderScene() con SceneQuery:
- Por cada píxel, llama `sq.query(ro, rd, visible, 256)` → lista de nodos visibles
- Pasa los nodos visibles a `rayMarch()` → `evalScene()` solo evalúa esos

## Scene Layer

### SceneGraph (`scene/scene_graph.h`)

Mantiene un array paralelo a `render::Scene::nodes`. Añade:

- **Jerarquía**: `parent`, `first_child`, `next_sibling` (linked list de hermanos)
- **World transforms**: rotación completa (Euler XYZ) + escala, propagada del padre
- **Dirty flags**: `transform_dirty` y `aabb_dirty`, propagadas a hijos
- **AABBs**: `computeLocalAabb()` desde primitiva SDF (8 corners transformados), `computeWorldAabb()` local→mundo

### CameraController (`scene/camera.h`)

| Modo | Cálculo de posición |
|------|---------------------|
| ORBIT | `pos = target - offset(distance, azimuth, elevation)` |
| FREE_FLY | `pos` gestionada externamente, `yaw/pitch` para dirección |
| FOLLOW | `pos = target_node + offset` |

Métodos: `orbitRotate(dx, dy)`, `zoom(delta)`, `mouseLook(dx, dy)`, `applyTo(Camera&)`.

### BVH (`scene/scene_query.h`)

Construcción top-down:
1. Colecta nodos SDF leaf habilitados
2. Calcula bounding box de centroides
3. Split por eje mayor
4. Partición en dos mitades (por centroide o por la mitad si todos del mismo lado)
5. Recursivo hasta profundidad 16 o ≤4 nodos

Query: ray-AABB test recursivo, recolecta nodos de hojas intersectadas.

### Project (`scene/project.h`)

Formato texto plano, no binario. Guarda:
- Referencias a fuentes .herm/.rih
- Camera + background (snapshot, no el grafo)
- Time + frame range
- Viewport dimensions

### Workspace (`scene/workspace.h`)

```cpp
struct Workspace {
    vector<Viewport> viewports;  // múltiples vistas
    vector<Layer>    layers;     // visibilidad por capa
    Timeline         timeline;   // playback, loop, fps
};
```

Timeline: `update(dt)` avanza `current_time` y `current_frame`. Soporta loop, rango inicio/fin.
