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

### Scene loaders (`render/scene.cpp`)

Dos formatos de escena: RIH (JSON, vía `Scene::load()`) y Ontológico (binario .ont, vía `OntScene::loadOnt()`).

#### Parser RIH

Parser JSON artesanal (sin librerías externas). Maneja:
- Objetos, arrays, strings, números, booleanos
- Campos desconocidos via `skipValue()` (que salta cualquier estructura anidada)
- SDF array-form (compounds expandidos inline como GROUP + children)
- ID→index mapping para evitar circular references en GROUP children

#### OntScene (.ont + .obs)

Formato binario para escenas compiladas del compilador Hermético:

- `.ont` — geometría pura: BytecodeVM (programa), materiales, BVH, camera defaults
  - `OntHeader` — magic + version + secciones
  - `OntMaterial` — base_color, roughness, metallic, emission
  - `OntBvhNode` — árbol binario con AABBs (6×f32) + leaf_idx
  - `BytecodeVM` — programa de bytecode (opcodes + operandos), stack, registros
  - Carga: `readFileMapping()` → parsea secciones por offset → construye OntScene

- `.obs` (Observation) — datos de observación, archivo independiente:
  - `ObsHeader` — magic (0x53424F21) + flags + cuenta de secciones
  - Secciones opcionales por flag: camera, lights, timeline, background, resolution
  - `ObsCamera` (48 bytes) — position x3, target x3, up x3, fov
  - `ObsLight` (36 bytes) — type (DIRECTIONAL=0|POINT=1), direction/position x3, color x3, intensity, falloff
  - `ObsTimeline` (20 bytes) — time, frame, start_frame, end_frame, fps, loop
  - Carga: `OntScene::loadObs()` → itera secciones → `applyObs()` copia a OntScene
  - Si no existe `.obs`, usa defaults: cámara (0,0,3)→(0,0,0), fov=50, 800×600, sin luces

#### BytecodeVM (`render/bytecode_vm.h`)

Máquina virtual stackless para evaluar SDFs compiladas:

- `execBcRaw()` — intérprete escalar
- Stack de 64 entradas f32
- Opcodes: ONT_ADD, ONT_SUB, ONT_MUL, ONT_DIV, ONT_SQRT, ONT_ABS, ONT_NEG,
  ONT_SIN, ONT_COS, ONT_TAN, ONT_FLOOR, ONT_CEIL, ONT_MIN, ONT_MAX,
  ONT_CLAMP, ONT_LERP, ONT_MIX, ONT_MOD, ONT_POW, ONT_SAMPLE, ONT_DOT, ONT_LENGTH,
  ONT_DROP (pop), ONT_DUP (duplicate top), ONT_PUSH* (cargar constantes/inputs/variables)
- `ONT_SAMPLE`: llamada recursiva a `bvhEval()` — permite composición anidada
- `evalOntSdfTree()` — ejecuta `execBcRaw()` para cada BVH leaf, min sobre resultados
- `evalOntScene()` — llama `evalOntSdfTree()` con parámetros de rayo point + w

#### SIMD Bytecode VM (`render/ray_march_simd.h`)

`execBcRaw4()` — versión SSE 4-wide de `execBcRaw()`:

- Stack de 128 entradas `__m128` (2KB, L1 cache)
- Opcodes aritméticos: `_mm_add_ps`, `_mm_sub_ps`, `_mm_mul_ps`, `_mm_div_ps`
- `ONT_SQRT`: `_mm_sqrt_ps`
- `ONT_ABS`: `_mm_and_ps` con máscara 0x7FFFFFFF
- `ONT_NEG`: `_mm_xor_ps` con máscara 0x80000000  
- `ONT_FLOOR`/`ONT_CEIL`: `_mm_floor_ps`/`_mm_ceil_ps` (SSE4.1)
- `ONT_CLAMP`, `ONT_LERP`, `ONT_MIX`: macros compuestas
- Fallback escalar para `ONT_SIN`/`ONT_COS`/`ONT_TAN`/`ONT_MOD`/`ONT_POW`/`ONT_SAMPLE`:
  extrae lane con `_mm_store_ps` → math.h → `_mm_set_ps`
- Comportamiento idéntico a `execBcRaw` para todas las entradas

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

### Ray Marching — RIH mode (`render/ray_march.h`)

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

### Ray Marching — Ont mode (`render/ray_march.h`)

`rayMarchOnt()` — marcha de esferas sobre `OntScene` con bytecode VM:

```cpp
Ray r = getRay(px, py, camera);
for step in 0..max_steps:
    p = r.origin + r.dir * t
    d = bvhEval(sc, p, w, &r.material)  // evalúa BVH + bytecode, rastrea material
    eps = hit_eps * (1.0 + t * 0.01)
    if d < eps:
        n = bvhNormal(...)
        shadeOnt(r, n, camera, lights, light_count)
        return
    t += d; if t > max_dist: → miss
```

- `bvhEval()` con `out_material`: durante el recorrido BVH, rastrea `g.material_id`
  del nodo más cercano. Post-hit, `r.material` ya tiene el material correcto.
- Sin `out_material`: comportamiento original (solo SDF, usado en el bucle de normals).
- Inside-geometry fix: `t` arranca en `trace_t_min` (0.01), hit requiere `d >= 0.0f`,
  stepping usa `abs(d)` para salir de geometría rápido.

### Multi-threading (`render/ray_march.h`)

`renderOntSceneMT()` — divide framebuffer en bandas de filas:

```cpp
u32 n_cores = std::thread::hardware_concurrency();
u32 n_threads = (n_cores < 2 || height < 32) ? 1 : std::min(n_cores, (height + 31) / 32);
std::thread pool[N_THREADS];
for each thread: renderOntScene() sobre banda [y_start, y_end)
```

Speedup: ~7× a 1000×700.

### SIMD Packet Tracing (`render/ray_march_simd.h`)

`renderOntSceneMTSIMD()` — multi-threaded + SIMD combinados:

- Divide framebuffer igual que `renderOntSceneMT()`
- Cada thread procesa paquetes de 4 píxeles adyacentes en lockstep:

```cpp
for y in [y_start, y_end):
    for x in 0..width step 4:
        __m128 ro_x[4], ro_y[4], ro_z[4], rd_x[4], rd_y[4], rd_z[4];
        for lane in 0..3:
            getRayData(px+lane, py, &ro, &rd)  // cargar rayo por lane
        t[0..3] = 0.0f;
        for step in 0..max_steps:
            p[0..3] = ro + rd * t;
            bvhEval4(sc, p, w, res, mat);      // SIMD BVH eval
            eps[0..3] = hit_eps * (1 + t * 0.01);
            hit_mask = _mm_cmplt_ps(res, eps);
            if any hit: shade, store color
            t += res;
            if all miss or t > max_dist: break
```

- `bvhEval4()`: BVH lockstep con 4 puntos simultáneos
  - Inicia con `node_idx[4] = {root, root, root, root}`
  - Itera hasta que todos los lanes terminan (bitset `alive`)
  - Por cada lane: carga AABB del nodo actual, SSE test 4 puntos vs AABB simultáneo
  - Si hoja: `applyMatrix4()` transforma 4 puntos → `execBcRaw4()` → masked update
  - Si lane termina (no intersecta AABB de hoja), pasa al siguiente hermano/padre
  - Divergencia: si 4 puntos entran en diferentes hojas, se evalúan secuencialmente
    (raro en píxeles adyacentes)

- `applyMatrix4()`: SSE 4×4 matrix transform para 4 puntos
  ```cpp
  __m128 tx = _mm_add_ps(_mm_add_ps(_mm_mul_ps(m[0], vx), _mm_mul_ps(m[1], vy)),
                          _mm_add_ps(_mm_mul_ps(m[2], vz), m[3]));
  ```
  (y, z análogos)

## GPU Layer (Vulkan Compute)

### Arquitectura de memoria GPU

La `VulkanSceneData` transfiere los datos del `.ont` a VRAM mediante staging buffers:

```
CPU (.ont en RAM)
   │
   ├── bvh_nodes    ──[Staging Buffer]──► bvh_buffer    (SSBO binding=0, GPU_ONLY, scalar)
   ├── graph_nodes  ──[Staging Buffer]──► graph_buffer   (SSBO binding=1, GPU_ONLY, scalar)
   ├── materials    ──[Staging Buffer]──► material_buffer(SSBO binding=2, GPU_ONLY, scalar)
   └── bytecode     ──[Staging Buffer]──► bytecode_buffer(SSBO binding=3, GPU_ONLY, scalar)
                                          ubo_buffer      (UBO  binding=4, CPU_TO_GPU)
                                          output_buffer   (SSBO binding=5, GPU_ONLY, TRANSFER_SRC)
```

Los SSBOs usan `layout(scalar)` (vía `GL_EXT_scalar_block_layout`) para que el layout en GPU coincida exactamente con el struct C++ de 52 bytes. Sin esta extensión, `std430` alinea `vec4` a 16 bytes → stride de 64 bytes → corrupción de datos.

### Compute Shader (`render/ray_march.comp`)

El shader recibe `gl_GlobalInvocationID.xy` (coordenadas de píxel) y ejecuta:

1. **Generación de rayo:** cámara → fwd/right/up → ray_dir normalizado
2. **Sphere Tracing** (80 pasos máx, configurable): stepping adaptativo con `d_min`
3. **BVH traversal:** while-loop stackless con AABB test y `skip_index`
4. **Bytecode VM:** ejecuta ~25 instrucciones/nodo para evaluar SDF
5. **Normal:** diferencia finita (4 evaluaciones BVH extra) en pixel hit
6. **Shading PBR:** Cook-Torrance GGX con luz direccional hardcoded

#### Stepping adaptativo (Fase 2)

El loop de raymarching usa stepping variable según la distancia SDF (`d`) y la
distancia mínima garantizada de la hoja BVH actual (`leaf_d_min`):

```glsl
for (int i = 0; i < 80; i++) {  // 80 pasos máximos
    vec3 p = ray_origin + ray_dir * t;
    vec3 res = bvhEval(p, ubo.time);  // retorna vec3(dist, d_min, mat_bits)
    float d = res.x;
    float leaf_d_min = res.y;

    float step;
    if (d > 0.0) {
        step = max(d, leaf_d_min) * 0.8;  // safety factor 0.8 sobre el mayor
    } else {
        step = d * 0.8;  // dentro de geometría: retrocede
    }
    t += step;

    if (abs(d) < 0.001 * (1.0 + t * 0.01)) break;  // hit
    if (t > ubo.tmax) break;  // early-out por distancia máxima
}
```

**Nota:** En escenas densas (catedral), `leaf_d_min = 0` porque el centro del AABB
combinado de cada hoja BVH cae dentro de geometría. En esas escenas, el stepping es
equivalente a `d * 0.8` (comportamiento clásico). La optimización beneficia escenas
con objetos separados donde `d_min > 0`.

**Seguridad GPU:**

```glsl
// BVH: límite de 65535 iteraciones
while (node_idx < bvh_nodes.length() && bvh_iters < 65535u) { ... }

// Bytecode: límite de 65535 instrucciones
while (pc < length && iter < 65535u) { ... }

// Acceso a grafo: bounds check
if (gi >= graph_nodes.length()) break;
```

### Pipeline de presentación (`vulkan_core.cpp::drawFrame`)

```
vkAcquireNextImageKHR()          → imageIndex
vkBeginCommandBuffer()
  vkCmdBindPipeline(COMPUTE)     → ray_march pipeline
  vkCmdDispatch(W/8, H/8, 1)    → compute shader
  [Barrier] UNDEFINED → TRANSFER_DST  → swapchain image
  [Barrier] COMPUTE_W → TRANSFER_R    → output buffer visible
  vkCmdCopyBufferToImage()       → output → swapchain
  [Barrier] TRANSFER_DST → PRESENT_SRC
vkEndCommandBuffer()
vkQueueSubmit(compute_queue)     → señaliza render_finished_sem
vkQueuePresentKHR(graphics_queue) → V-Sync FIFO
```

### Sincronización

- `image_available_sem`: señalizado por `vkAcquireNextImageKHR`, esperado por compute queue en `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT`
- `render_finished_sem`: señalizado por compute queue, esperado por present queue
- `in_flight_fence`: fence único que serializa frames (CPU espera a GPU antes de reusar command buffer)
- `vkDeviceWaitIdle` en `cleanupSwapchain()` y `cleanup()` para evitar validation errors al destruir recursos en uso

### Test pattern mode

Toggle con F3. Cuando activo, `drawFrame()` salta el compute shader y llena la swapchain con rojo sólido mediante `vkCmdClearColorImage`. Sirve para aislar bugs de presentación vs bugs de compute.

### Camera Smoothing (GPU path)

La `CameraController` implementa dos mecanismos de suavizado exponencial:

1. **Velocity smoothing (FREE_FLY):** `fly_velocity` se aproxima exponencialmente a `target_velocity` con factor `1 - exp(-smooth_speed * dt)` donde `smooth_speed = 6.0`. Esto da aceleración/deceleración progresiva al mover WASD.

2. **Zoom smoothing (ORBIT):** `orbit_distance` se aproxima a `target_distance` con `zoom_smooth_speed = 8.0`. Zoom con rueda o +/- interpola suavemente.

**Problema conocido:** `dt = 0.016f` hardcodeado (asume 60 FPS). A 3.6 FPS reales, el dt efectivo es 50× mayor, haciendo que el suavizado sea prácticamente instantáneo.

### FPS Overlay en GPU path

La GDI overlay se dibuja después del present Vulkan:
1. `drawFpsOverlay()` → `GetDC(hwnd_viewport)` → `TextOutA` → `ReleaseDC`
2. `InvalidateRect` + `UpdateWindow` → `WM_PAINT` → `paintViewport()` → barra + texto
3. La window class del viewport usa `NULL_BRUSH` para que `BeginPaint` no rellene el viewport de negro

### SDF Grid — V0 (Hybrid Static/Dynamic) (`render/ray_march.h`)

Grid 3D uniforme que cachea SDF de nodos estáticos para evitar evaluación BVH por rayo.

#### SdfGrid (`render/scene.h`)

```cpp
struct SdfGrid {
    Vec3    origin;          // esquina min del AABB escena
    Vec3    inv_cell_size;   // 1.0 / cell_size
    u32     dim_x, dim_y, dim_z;
    u32     capacity;        // dim_x * dim_y * dim_z
    u32*    offsets;         // byte offset en data[] por celda
    u32*    data;            // [sdf_f32 | bitmask de dinámicos por celda]
};
```

Cada celda del grid contiene: un `f32` de SDF precomputado (estáticos), seguido de
word-aligned bitmasks de nodos dinámicos que intersectan la celda (bit 31 = último).

#### Clasificación (`classifyOntNodes`)

Recorre BVH leaves y aplica `isBcDynamic()` sobre su bytecode:
- Dinámico si contiene: `ONT_SIN`, `ONT_COS`, `ONT_TAN`, `ONT_POW`, `ONT_MOD`,
  `ONT_SAMPLE`, o cualquier `ONT_PUSH` que referencie el registro `w` (tiempo)
- Estático en caso contrario

#### Construcción (`buildSdfGrid`)

1. Calcula AABB de la escena de nodos estáticos
2. Determina cell_size para 64³ (o menos si el AABB es pequeño)
3. Itera nodos estáticos: rasteriza su AABB sobre celdas, evalúa SDF en centro
   de cada celda, almacena mínimo por celda
4. Itera nodos dinámicos: rasteriza su AABB, agrega bitmask al data de cada celda
5. Celdas sin cubrir: deja `sdf = +inf` (fallback a evaluación completa)

#### Evaluación híbrida (`evalHybrid`)

```cpp
f32 evalHybrid(const OntScene& sc, Vec3 p, f32 w, const SdfGrid* grid) {
    if (!grid) return evalOntSdfTree(sc, p, w);

    u32 mask = gridSample(*grid, p);  // lookup celda
    if (mask == 0) {
        return read_grid_sdf(grid, p);  // solo estáticos: O(1)
    }

    f32 static_sdf = read_grid_sdf(grid, p);
    f32 dynamic_sdf = evalDynamicNodes(sc, p, w, mask);
    return min(static_sdf, dynamic_sdf);
}
```

- `gridSample()`: coordenada → celda → offset → máscara. O(1)
- `evalDynamicNodes()`: evalúa solo los BVH leaves indicados por mask, no todo el árbol
- Celdas sin cobertura: `mask == 0xFFFFFFFF` → `evalOntSdfTree()` completo

#### Integración en render pipeline

`Renderer::render()` en `render/render.h`:
1. `buildSdfGrid(sc)` al inicio del frame
2. Pasa `SdfGrid*` a `renderOntSceneMTSIMD()`
3. `rayMarchOnt()` usa `evalHybrid()` en cada paso
4. `freeSdfGrid()` al final del frame (Arena reset lo cubre automáticamente)

### Multi-light PBR (`render/ray_march.h`)

`shadeOnt()` — Cook-Torrance GGX con soporte multi-luz:

```cpp
for each light:
    ldir = light.direction (directional) or normalize(light.pos - hit_pos)
    ndotl = saturate(dot(normal, ldir))
    half = normalize(view_dir + ldir)
    // NDF (GGX)
    D = alpha2 / (pi * cos2 * cos2 * (alpha2 + tan2))
    // Geometry (Schlick-GGX)
    G = G1(ndotv) * G1(ndotl)
    // Fresnel (Schlick)
    F = F0 + (1 - F0) * pow(1 - ndotv, 5)
    cook_torrance = D * G * F / (4 * ndotv * ndotl)
    result += light.intensity * light.color * (cook_torrance + ndotl * (1 - metallic) * base / pi)
```

- Fallback a luz direccional hardcoded `(0.3, -0.5, -0.8)` si no hay luces en `.obs`
- Sky background: gradiente top→horizon→bottom desde `.obs` o defaults azul/gris/negro

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
`updateFly()`: auto-switch de ORBIT a FREE_FLY al recibir primer input WASD.
Movimiento con forward/right vectors desde orientación actual.

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
