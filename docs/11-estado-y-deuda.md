# 11 — Estado del proyecto y deuda técnica

## Estado actual (Junio 2026 — v0.26)

El Motor Gráfico es un **motor de renderizado SDF con pipeline GPU completo y estable**. La CPU ya no calcula píxeles en producción. Tiene arquitectura en capas, 219 tests unitarios, y dos paths de render coexistentes: GPU (Vulkan Compute, producción) y CPU (SIMD+MT, debug/fallback).

### Benchmarks

| Path | Resolución | Escena | Tiempo | FPS | Notas |
|------|-----------|--------|--------|-----|-------|
| GPU Vulkan | 974×617 | catedral (30 nodos, 15 BVH, 3725 B bytecode) | **10.55 ms** | **94.8** | v0.26, scene-specialized shader, pipeline funcional |
| GPU Vulkan | 974×617 | esfera (1 nodo, 1 BVH, 75 B bytecode) | **3.21 ms** | **311.2** | v0.26, baseline |
| GPU Vulkan (v0.25) | 974×617 | catedral (30 nodos) | ~250 ms | ~4.0 | v0.25, stepping adaptativo desactivado, fix de workgroup pendiente |
| GPU Vulkan (v0.24) | 974×617 | catedral (30 nodos) | 281 ms | 3.6 | baseline pre-Fase2, workgroup (1,1,1) |
| CPU SIMD+MT | 1920×1080 | test_custom (2 nodos) | 28 ms | 36 | |
| CPU SIMD+MT | 1000×700 | test_custom (2 nodos) | 8 ms | 123 | |

### Lo que funciona

| Componente | Estado |
|-----------|--------|
| OS: Arena allocator | ✅ Estable |
| OS: FileMapping (I/O) | ✅ Estable |
| OS: Timer (QPC) | ✅ Estable |
| OS: Window (Win32) | ✅ Estable |
| RHI: DX11 Compute Shader | ✅ Estable (legacy, no activo) |
| Render: Scene loader (RIH JSON) | ✅ Estable |
| Render: Expression evaluator | ✅ Estable (29 tests) |
| Render: SDF primitives (8 tipos) | ✅ Estable |
| Render: SDF tree eval | ✅ Estable (grupos, instancias, boolean ops) |
| Render: Ray marching | ✅ Estable |
| Render: Shading Cook-Torrance GGX multi-luz | ✅ Estable |
| Render: AABB early-out | ✅ Implementado |
| Render: Epsilon adaptativo | ✅ Implementado |
| Render: BytecodeVM (execBcRaw) | ✅ Estable |
| Render: OntScene (.ont + .obs loader) | ✅ Estable |
| Render: Material-finding en bvhEval | ✅ Implementado |
| Render: Multi-threading (renderOntSceneMT) | ✅ Implementado (7× speedup) |
| Render: SIMD packet tracing | ✅ Implementado (SSE 4-wide, 36 FPS Full HD) |
| Render: JIT Compiler (AsmJit) | ✅ Implementado (x86/x64 nativo, SSE math) |
| Render: BrickMap V0 (SdfGrid) | ✅ Implementado (no usado en GPU) |
| **GPU: VulkanContext** (instance, device, swapchain, queues, sync) | ✅ **Completo y estable** |
| **GPU: VulkanSceneData** (SSBOs via staging, UBO, output buffer, descriptors) | ✅ **Completo** |
| **GPU: Compute Pipeline** (SPIR-V load, pipeline layout, descriptor set) | ✅ **Completo** |
| **GPU: ray_march.comp** (Bytecode VM en GLSL, Sphere Tracing, BVH traversal) | ✅ **Completo, con safety guards** |
| **GPU: drawFrame** (acquire → compute → barriers → blit → present) | ✅ **Completo, sin validation errors** |
| **GPU: Presentación Vulkan Swapchain** (sin GDI, V-Sync FIFO nativo) | ✅ **Completo** |
| **GPU: Test pattern mode** (F3 toggle, swapchain rojo sólido) | ✅ **Implementado** |
| **GPU: Layout correcto** (GL_EXT_scalar_block_layout, struct 52 bytes) | ✅ **Corregido** |
| **GPU: Safety** (límite 65535 iteraciones BVH + bytecode, bounds check) | ✅ **Implementado** |
| **GPU: Benchmark** (--bench-vulkan, 200 frames, 3.6 FPS) | ✅ **Confirmado** |
| **GPU: Stepping adaptativo con d_min** (Fase 2) | 🔶 **Implementado, no activo** (d_min=0 en escena densa) |
| **GPU: tmax early-out** (desde UBO) | 🔶 **Implementado, UBO layout mismatch pendiente de fix** |
| Scene: SceneGraph jerárquico | ✅ Estable (82 tests) |
| Scene: CameraController (ORBIT/FLY/FOLLOW) | ✅ Implementado |
| Scene: BVH (construcción + query) | ✅ Implementado |
| Scene: Project (.mgproject) | ✅ Implementado |
| Scene: Workspace (viewport, layers, timeline) | ✅ Implementado |
| Visor: Win32 app con viewport | ✅ Interactivo (WASD, mouse orbit, FPS overlay) |
| Visor: Adaptive resolution scaling | ✅ Implementado (0.25× en movimiento) |
| Visor: FPS en title bar | ✅ Implementado para modo ONT |
| Visor: GPU/CPU path bifurcation (`use_vulkan` flag) | ✅ Implementado |
| Visor: --bench mode | ✅ Implementado (Vulkan incluido) |
| Build: Shader auto-compilation (glslc en build.bat) | ✅ Integrado |
| Camera: Velocity smoothing | ✅ Implementado (exponencial, `smooth_speed=6.0`) |
| Camera: Zoom smoothing | ✅ Implementado (lerp, `zoom_smooth_speed=8.0`) |

## Deuda técnica

### Bugs conocidos

| Bug | Impacto | Prioridad |
|-----|---------|-----------|
| **Color dinámico descartado en `.ont`**: expresiones de color (`tensor[4..7]`) se ignoran y se sustituye por `1.0f`. | Materiales Fresnel/animados aparecen blancos. | **ALTA** |
| **AABBs conservativas (±5m)**: primitivos sin AABB específica en el compilador. | BVH sub-óptimo, más nodos visitados de lo necesario. | **MEDIA** |
| **Sin validación de .spv**: si `ray_march.spv` no existe, `initPipeline()` falla silenciosamente. | El motor cae al path CPU sin avisar al usuario. | **MEDIA** |
| **UBO layout mismatch**: GLSL usa `layout(scalar)` pero C++ `UboData` tiene padding `std140`. | `tmax` se lee de offset incorrecto; `camera_target` y `camera_up` parcialmente corruptos. | **MEDIA** |
| **Semáforo único para swapchain**: `vkQueueSubmit` reusa el mismo semáforo para todas las imágenes. | Validation warning: semáforo aún en uso. Puede causar stalls en GPU con carga alta. | **MEDIA** |
| **Cleanup race condition**: `vkDestroyBuffer/Pipeline` llamado mientras el buffer aún está en uso por command buffer. | Validation error al salir. No afecta runtime pero indica falta de sync en shutdown. | **BAJA** |
| **dt hardcodeado a 0.016f**: cámara no usa frame delta real. | A baja FPS la cámara se siente incorrecta. | **BAJA** |

**Nota:** El bug `d_min=0.01` en BVH ya no es relevante — con `layout(scalar)` corregido y safety guards, el BVH traversal es correcto.
**Nota v0.26:** Los bugs de log corrupto, glslc.exe, workgroup size, dimensiones de render, coherencia UBO y swapchain extent fueron corregidos en v0.26.

### Deuda de implementación

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| GPU: Sin BrickMap | El compute shader ignora el SdfGrid/BrickMap. Re-evalúa BVH completo en cada paso. | Alto |
| GPU: Sin modo volume | `mode: volume` se ignora. Solo SOLID. | Medio |
| GPU: output_buffer no se redimensiona automáticamente con la ventana | Si el viewport cambia de tamaño, hay que reiniciar. | Medio |
| GPU: UBO sin double-buffering | Un solo UBO, con multi-frame in flight podría corromperse. | Bajo |
| CPU: Sin Resource Manager | Carga sincrónica. No hay caché, ni thread pool, ni async loading. | Alto |
| CPU: `skipValue()` frágil | Parser JSON artesanal, no robusto para JSON arbitrario. | Bajo |

### Brechas de features GPU vs CPU

El shader de GPU (`ray_march.comp` + template generado) carece de varias capacidades que sí existen en el path de CPU (`ray_march.h`). La tabla siguiente documenta cada brecha, su impacto visual, el esfuerzo estimado, y por qué no se ha implementado aún.

| Feature | GPU | CPU | Impacto visual | Esfuerzo | Por qué no está en GPU |
|---------|-----|-----|---------------|----------|----------------------|
| **SDF Ambient Occlusion** | ❌ `ao=1.0` hardcodeado | ✅ `sdfAO()` 4-step hemispherical | **Alto** — sin AO las escenas se ven planas, sin contacto | **Bajo** (~2 h) | Prioridad histórica en pipeline básico; AO requiere 4 eval extra por píxel (como las normales), multiplicando costo. Pendiente de optimización vía reuse de distancia. |
| **Shadow march (soft shadows)** | ❌ No existe | ✅ `shadowMarch()` penumbra | **Alto** — sin sombras no hay percepción de profundidad entre objetos | **Medio** (~1 día) | Requiere rayo secundario por luz → duplica eval de SDF. No implementado porque primero se priorizó pipeline base funcional. |
| **Multi-light** | ❌ 1 luz direccional hardcodeada `vec3(0.6,-0.6,0.4)` | ✅ Array de luces desde `.obs` | **Medio** — sin fill lights las sombras son duras, escenas subexpuestas | **Bajo** (~3 h) | El generador de shaders aún no inyecta bucles de lights. Datos ya están en SSBO de materiales. |
| **Opacidad / Transparencia** | ❌ `mat.opacity` declarado pero nunca leído | ❌ No implementado en ninguna ruta | **Medio** — materiales como vidrio, agua, hielo se renderizan opacos | **Medio** (~1 día) | Requiere transmission ray + Beer-Lambert. Feature complejo que requiere decisión de diseño: alpha blending vs rayo secundario. |
| **Reflexión** | ❌ No existe | ❌ No implementado | **Alto** — metales sin reflejos se ven irreales, plásticos | **Medio** (~1 día) | Requiere rayo secundario de reflexión (1 bounce). Reusa `bvhEval`. Pendiente por no ser prioridad sobre sombras/AO. |
| **Refracción** | ❌ No existe | ❌ No implementado | **Medio** — vidrio, agua, lentes no funcionan | **Alto** (~2 días) | Requiere Snell + transmission ray + manejo de IOR + total internal reflection. Feature más complejo. |
| **Blur / DOF** | ❌ 1 rayo/pixel, sin acumulación | ❌ No implementado | **Bajo** — efecto cosmético, no bloqueante | **Alto** (~3 días) | Requiere multi-sampling temporal o espacial + accum buffer. Sin utilidad hasta tener escenas completas. |
| **Material expressions** (`material_r/g/b`) | ❌ No se generan en shader | ✅ Evaluación por píxel en CPU | **Medio** — materiales con color procedural (terrenos, ruido) no se ven | **Alto** (~3 días) | Requiere inyectar expresiones de color en el shader generado, similar a como se inyectan SDF. Más complejo que SDF inline porque depende de `p` y `n`. |

### Contrato Hermetico vs Pipeline GPU

El Lenguaje Hermetico define un **contrato de escena** (`.rih` → `.ont` + `.obs`) que el Motor Gráfico debe cumplir para renderizar correctamente cualquier escena del lenguaje. Esta sección documenta **qué features del contrato soporta el pipeline GPU actual** y **cuáles no**, organizado por categoría.

#### SDF Primitives (contrato: 12 tipos)

Todas las primitivas están **soportadas vía bytecode inline** — el generador de shaders decompila el bytecode del `.ont` a expresiones GLSL `float sdf_N(vec3 p, float w)`. No hay límite teórico:

| Primitiva | Contrato | GPU | Notas |
|-----------|----------|-----|-------|
| sphere | `sphere(r)` | ✅ | |
| box | `box(w,h,d)` | ✅ | |
| cylinder | `cylinder(r,h)` | ✅ | |
| capsule | `capsule(r,len)` | ✅ | |
| torus | `torus(R,r)` | ✅ | |
| plane | `plane(nx,ny,nz,d)` | ✅ | |
| cone | `cone(r,h)` | ✅ | |
| rounded_box | `rounded_box(w,h,d,r)` | ✅ | |
| octahedron | `octahedron(r)` | ✅ | |
| tesseract | `tesseract(r)` | ✅ | 4D hypercube (vía bytecode) |
| sphere4D | `sphere4d(r)` | ✅ | 4D hypersphere (vía bytecode) |
| custom | `custom(expr)` | ✅ | Es el mecanismo base |

#### SDF Operators (contrato: 10 tipos)

| Operador | Contrato | GPU | Mecanismo |
|----------|----------|-----|-----------|
| union | `union(a,b)` | ✅ | `min(a,b)` en bytecode |
| subtract | `subtract(a,b)` | ✅ | `max(a,-b)` en bytecode |
| intersect | `intersect(a,b)` | ✅ | `max(a,b)` en bytecode |
| smooth_union | `smooth_union(a,b,k)` | ✅ | `smin(a,b,k)` vía expresión inline |
| smooth_subtract | `smooth_subtract(a,b,k)` | ✅ | `smin(a,-b,k)` vía expresión |
| smooth_intersect | `smooth_intersect(a,b,k)` | ✅ | `smax(a,b,k)` vía expresión |
| repeat | `repeat(a,spacing)` | ✅ | Domain repetition con `mod` |
| twist | `twist(a,angle)` | ✅ | Rotación Y proporcional a y |
| elongate | `elongate(a,size)` | ✅ | Subtract half-extents |
| displace | `displace(a,expr)` | ✅ | Suma de expresión SDF |

#### Material Properties (contrato: 11 propiedades PBR)

| Propiedad | Contrato (.rih) | GPU | CPU | ¿Qué se pierde visualmente? |
|-----------|-----------------|-----|-----|---------------------------|
| `base_color` / `tensor[4..6]` | `[R,G,B]` | ✅ Cook-Torrance albedo | ✅ | — |
| `roughness` | `0..1` | ✅ GGX NDF + Schlick-GGX | ✅ | — |
| `metallic` | `0..1` | ✅ Schlick Fresnel | ✅ | — |
| `emission` (isotropic) | `[R,G,B]` ≥0 | ✅ Sumado al final | ✅ | — |
| **`opacity`** | `0..1` | ❌ Declarado, nunca leído | ❌ No usado | Vidrio, agua, hielo, cristal → opacos. Sin alpha blending ni transmission. |
| **`ior`** | ≥1.0 | ❌ Ignorado | ❌ Ignorado | Sin refracción. Snell no implementado. |
| **`transmission`** | `0..1` | ❌ Ignorado | ❌ Ignorado | Sin translucidez. Objetos traslúcidos no existen. |
| **`subsurface`** | `0..1` | ❌ Ignorado | ❌ Ignorado | Sin SSS. Piel, cera, mármol se ven plásticos. |
| **`emission_mode`** | isotropic/directional | ❌ Solo isotropic | ❌ Solo isotropic | Luces direccionales no funcionan como emisores. |
| **`emission_dir`** | `[x,y,z]` | ❌ Ignorado | ❌ Ignorado | Sin emisión direccional. |
| **`blend_mode`** | alpha/add/replace | ❌ Ignorado | ❌ Ignorado | Sin blending entre materiales. |

> **Nota:** El contrato Hermetico define `tensor[8] = [x, y, z, w, R, G, B, A]`. Los índices 0-3 son expresiones espaciales/temporales (para animación procedural de color), 4-6 son color base, 7 es opacidad. El pipeline GPU compila los índices 0-3 como constantes — **las expresiones animadas de color no se generan en el shader**.

#### Lights (contrato: 2 tipos + pipeline config)

| Feature | Contrato (.obs) | GPU | CPU |
|---------|-----------------|-----|-----|
| **1 luz direccional** | `direction, color, intensity` | ⚠️ Hardcodeada `vec3(0.6,-0.6,0.4)`, color fijo `(1,0.92,0.8)`, intensity fija | ✅ Desde `.obs` |
| **N luces direccionales** | Array de luces | ❌ No | ✅ Array completo |
| **Punto de luz** | `position, color, intensity, falloff` | ❌ No | ✅ Position + falloff attenuation |
| **Color por luz** | Por luz en `.obs` | ❌ Fijo | ✅ |
| **Intensidad por luz** | Por luz en `.obs` | ❌ Fija en 1.0 | ✅ |
| **Falloff (point lights)** | `falloff` | ❌ No | ✅ Atenuación 1/(1+d·falloff) |
| **Sombras (shadow march)** | `shadow.enabled, steps, max_dist, bias` | ❌ No | ✅ Penumbra suave vía `shadowMarch()` |
| **Pipeline: shadow.enabled** | Bool | ❌ Ignorado | ✅ |
| **Pipeline: shadow.steps** | int | ❌ Ignorado | ✅ |
| **Pipeline: shadow.max_dist** | float | ❌ Ignorado | ✅ |
| **Pipeline: shadow.bias** | float | ❌ Ignorado | ✅ |

#### Pipeline Config (contrato: 12 parámetros)

| Parámetro | Contrato (.rih) | GPU | Problema |
|-----------|-----------------|-----|----------|
| `trace.max_steps` | int (ej: 128) | ❌ Hardcodeado 80 | Límite fijo, no configurable. Escenas complejas necesitan más pasos. |
| `trace.hit_threshold` | float (ej: 0.001) | ❌ Hardcodeado dinámico `0.001*(1+t*0.01)` | Épsilon adaptativo no parametrizable. |
| `trace.t_min` | float | ❌ Hardcodeado `0.01` | Sin control. |
| `trace.t_max` | float | ⚠️ Vía UBO pero con layout mismatch | `tmax` se lee de offset incorrecto por bug std140 vs scalar. |
| `shade.ambient` | float | ❌ Hardcodeado `0.1*albedo*(1-metallic)` | Sin control de intensidad ambiental. |
| `shade.diffuse` | float | ❌ Hardcodeado `albedo/π` | Sin control. |
| `shade.specular` | float | ❌ Hardcodeado `specular*0.3` | Sin control. |
| `shade.spec_power` | float | ❌ Usa GGX (roughness-driven) | No usa el modelo spec_power legacy. |
| `post.tonemap` | bool | ❌ Hardcodeado Reinhard | No se puede desactivar. |
| `post.gamma` | bool | ❌ Hardcodeado 2.2 | No se puede desactivar. |
| `post.exposure` | float | ❌ Hardcodeado ×2 | No configurable. |

#### Animation / W-axis (contrato: timeline + expresiones animadas)

| Feature | Contrato (.obs) | GPU | CPU |
|---------|-----------------|-----|-----|
| `time` en UBO | `w_frames, w_min, w_max` | ✅ Enviado por frame | ✅ |
| SDF params animados | `{ "expr": "sin(w*2)" }` | ✅ Vía shader generado | ✅ |
| Transform animada | translate/rotate/scale con expr | ✅ Vía shader generado | ✅ |
| **Color animado (tensor[0..3] con expr)** | `[expr, expr, expr, expr, R, G, B, A]` | ❌ No se generan — se compilan como constantes | ⚠️ Parcial |
| **Material expressions por nodo** | `material_r/g/b: "expr"` | ❌ No existen en shader | ✅ Evaluación por píxel |

#### Volume Rendering (contrato: mode + density_scale)

| Feature | Contrato (.rih) | GPU | CPU |
|---------|-----------------|-----|-----|
| `mode: volume` | Sólido vs volumétrico | ❌ Ignorado, solo SOLID | ❌ Ignorado |
| `density_scale` | Float | ❌ Ignorado | ❌ Ignorado |
| Beer-Lambert integration | Transmitancia | ❌ No | ❌ No |
| `SAMPLE` opcode | Recursive eval | ❌ No usado | ❌ No usado |

#### Material Expressions (procedural color por píxel)

El Lenguaje Hermetico permite que el color de un material varíe por píxel mediante expresiones que dependen de `p` (posición), `n` (normal), `v` (view), `l` (light). Ejemplo real de `test_suelo.rih`:

```json
"material_r": "clamp(0.2 + y * 0.2, 0.1, 0.6)",
"material_g": "clamp(0.55 - abs(y) * 0.3, 0.1, 0.55)",
"material_b": "clamp(0.1 + y * 0.15, 0.05, 0.45)"
```

| Feature | GPU | CPU |
|---------|-----|-----|
| `material_r/g/b` (expresiones por nodo) | ❌ No se generan en shader | ✅ Evaluación por píxel en CPU |
| `tensor[0..3]` con expresiones de color | ❌ Compilado como constante | ⚠️ Parcial |
| Variables disponibles en expr: `p,n,v,l,w` | ❌ No aplica | ✅ `p.x, p.y, p.z, w, n.x, etc.` |

#### Resumen: Estado de cumplimiento del contrato

```
SDF Primitives:     🟢 12/12 (100%) — vía bytecode inline
SDF Operators:      🟢 10/10 (100%) — vía bytecode inline
Material PBR base:  🟢  4/11 (36%)  — color, roughness, metallic, emission
Material avanzado:  🔴  0/7  (0%)   — opacity, ior, transmission, subsurface, emission_mode/dir, blend_mode
Lights:             🔴  0/7  (0%)   — multi-light, point lights, shadows, shadow config
Pipeline Config:    🔴  0/12 (0%)   — ningún parámetro de pipeline es configurable en GPU
Animation W-axis:   🟡  3/5  (60%)  — time y SDF params OK; color animado y material expressions NO
Volume Rendering:   🔴  0/4  (0%)   — mode volume, density_scale, Beer-Lambert, SAMPLE
Material Expressions: 🔴 0/3  (0%)  — ni material_r/g/b ni tensor[0..3] dinámico
```

El cálculo de normales en GPU requiere **4 evaluaciones de `bvhEval` por píxel** (1 hit + 3 offset). Esto significa que ~75% del costo de shading es atribuible a las normales. En la catedral (10.55 ms), ~8 ms son normales. Alternativas:

| Alternativa | Esfuerzo | Ahorro estimado |
|------------|----------|-----------------|
| Reuse de distancia en normal (evaluar una vez, reusar `d` del hit) | Bajo (~1 h) | ~25% (elimina 1 de 4 evals) |
| Analytical gradient (si la SDF tiene derivada conocida) | Alto (por nodo) | ~75% (0 evals extra) |
| Tetrahedron technique (4 evals pero con mayor epsilon) | Bajo (~30 min) | ~0% (mismas 4 evals, más preciso) |

### Nota: 80 iteraciones fijas

El raymarch usa 80 iteraciones fijas. En la GPU esto no es problema de rendimiento (los hilos inactivos simplemente esperan) pero puede ser:
- **Insuficiente** para escenas con BVH deep o cámaras dentro de geometría
- **Excesivo** para escenas simples donde 20-30 pasos bastan

No hay early-exit por divergencia de warp. Con stepping adaptativo (Fase 2) el paso es más eficiente pero el límite sigue siendo 80.

### Deuda de arquitectura

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| `core/` legacy sin limpiar | `core/core.h`, `core/rih_reader.*`, `core/renderer.*` existen pero no se usan. | Bajo |
| Strings de SDF type | `sdf_type` es `std::string` comparado en cada eval. Debería ser enum. | Bajo |
| Sin PCH | Cada build recompila todo. Con PCH se reduciría a ~2s. | Medio |
| Windows-only | Toda la capa OS es Win32. Linux requeriría rewrite de `os/win32/`. | Alto |

### Deuda de testing

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| Sin tests de GPU | Los paths de Vulkan no tienen tests unitarios automatizados. | Alto |
| Sin tests de regresión automatizados | Tests se ejecutan manualmente. No hay CI. | Bajo |
| Sin tests de SIMD | `ray_march_simd.h` no tiene tests unitarios. Solo test visual. | Medio |
| Sin fuzz testing del parser JSON | El RIH loader aceptaría JSON malformado sin errores claros. | Medio |

## Lo que falta

### Para MVP (producción básica)

| Feature | Depende de | Esfuerzo | Por qué ahora |
|---------|-----------|----------|---------------|
| **SDF Ambient Occlusion en GPU** | `glsl_gen.cpp` template | 2 h | Alto impacto visual, bajo esfuerzo. Reusa `bvhEval` existente. |
| **Shadow march en GPU** | `glsl_gen.cpp` template | 1 día | Impacto visual crítico. Sin sombras la escena no tiene profundidad. |
| **Multi-light en GPU** | `glsl_gen.cpp` codegen | 3 h | Las escenas `.obs` ya tienen luzas. Solo falta inyectar bucle. |
| Fix UBO layout mismatch | `vulkan_pipeline.h` GLSL struct | 30 min | `tmax` ilegible, afecta stepping adaptativo. |
| Swapchain resize handler | Vulkan core | 2 h | Necesario para redimensionamiento interactivo. |
| Validación de `.spv` con fallback visible | Nada | 2 h | Shader falla silenciosamente sin feedback. |
| dt real para cámara y animación | `visor_app.cpp` | 30 min | Cámara incorrecta a baja FPS. |

### Para versión alpha

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Color dinámico (bytecode separado para materiales) | Herm compiler + comp shader | 3 días |
| Resource Manager (async loading) | Thread Pool | 3 días |
| Modo volume (Beer-Lambert) en GPU | ray_march.comp | 2 días |
| Editor: tree view de nodos | Visor refactor | 3 días |
| Timeline animación de `w` | Workspace::Timeline | 1 día |

### Para versión beta

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Multi-frame in flight (Vulkan) | Fence por swapchain image | 2 días |
| Linux port | Vulkan ya OK, solo `os/win32/` | 2 semanas |
| Múltiples viewports | Workspace | 2 días |
| Exportar frame como .png | Readback buffer | 1 día |

## Resumen

```
Estado:        🟢 Alpha-GPU (renderiza en GPU, pipeline scene-especializado funcional, 94.8 FPS catedral)
Tests:         🟢 219/219 pasan (CPU path validado)
Arquitectura:  🟢 Capas limpias, GPU + CPU coexisten, flag de switch
Rendimiento:   🟢 GPU Vulkan 94.8 FPS (catedral 30 nodos) / 311 FPS (esfera)
               🟡 Sin sombras, sin AO, sin multi-light, sin opacidad/reflexión/refracción
               🟡 75% del costo de shading son normales (4 evals de bvhEval)
Editor:        🔴 No existe (solo viewport + edit panel)
Portabilidad:  🟡 Vulkan cross-platform, OS layer aún Win32-only
```
