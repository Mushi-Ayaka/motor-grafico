# Motor Gráfico

> **Vida del proyecto:** 4 de junio de 2026 (sesión única)

Motor de renderizado 3D interactivo para escenas SDF (Signed Distance Fields) con arquitectura limpia en 5 capas. Carga escenas descritas en formato RIH (JSON), las renderiza por ray marching en CPU, y las visualiza en una ventana Win32 con controles de cámara en tiempo real.

## Arquitectura (5 capas)

```
┌─────────────────────────────────────────────────┐
│                   VISOR                          │
│         visor/main.cpp  (Win32 App)              │
├─────────────────────────────────────────────────┤
│                   SCENE                          │
│  scene_graph.h  camera.h  scene_query.h (BVH)    │
│  project.h  workspace.h                          │
├─────────────────────────────────────────────────┤
│                  RENDER                          │
│  scene.h/cpp  sdf_eval.h/cpp  ray_march.h        │
│  render.h  (CPU ray marching + shading)          │
├─────────────────────────────────────────────────┤
│                   RHI                            │
│  rhi.h  d3d11.cpp  (DirectX 11 Compute Shader)   │
├─────────────────────────────────────────────────┤
│                   OS                             │
│  os.h  win32/  (Arena, FileMapping, Timer, Window)│
└─────────────────────────────────────────────────┘
```

## Capas

### OS (`os/`)

Abstracción del sistema operativo Windows:

| Módulo | Descripción |
|--------|-----------|
| `Arena` | Bump allocator sobre VirtualAlloc (64MB reserve, commit en páginas de 64KB) |
| `FileMapping` | Memory-mapped files vía CreateFileMappingW |
| `Timer` | QueryPerformanceCounter: `now()` y `delta()` en segundos |
| `Window` | Win32 window: RegisterClass + CreateWindowEx + PeekMessage loop |

### RHI (`rhi/`)

Abstracción de hardware gráfico vía DirectX 11 Compute Shader:
- `init(hwnd, w, h)` — D3D11CreateDevice + swapchain + RTV
- `createConstantBuffer()` — buffers dinámicos alineados a 16 bytes
- `createStructuredBuffer()` — SRV+UAV para datos de escena
- `createOutputTexture()` — UAV+SRV para salida de compute shader
- `createComputeShader()` — D3DCompile desde string HLSL (cs_5_0)
- bind/dispatch/present pipeline completo

### Render (`render/`)

Núcleo del ray marching en CPU:

| Módulo | Descripción |
|--------|-----------|
| `scene.h` | Tipos: Vec3, Material, SdfNode, Node (SDF/GROUP/INSTANCE), Light (DIRECTIONAL/POINT), Camera, Scene |
| `scene.cpp` | Parser JSON recursivo + Scene::load() con expansión de compuestos SDF |
| `sdf_eval.h` | Evaluador inline de SDF: sphere, box, cylinder, torus, plane, cone + boolean ops (union, subtract, intersect, smooth) + transforms |
| `sdf_eval.cpp` | Evaluador de expresiones: parser recursive descent, vars (x,y,z,w,t), funciones (abs, sin, cos, sqrt, pow, max, min, clamp, lerp) |
| `ray_march.h` | Sphere tracing con epsilón adaptativo, shade() Blinn-Phong multi-light, renderScene() |
| `render.h` | Renderer: compone Scene + Frame + Arena + time |

### Scene (`scene/`)

Gestión de escena interactiva:

| Módulo | Descripción |
|--------|-----------|
| `scene_graph.h` | SceneGraph jerárquico: parent/child/sibling, world transforms con dirty propagation, AABB local/mundo, collectVisibleNodes() |
| `camera.h` | CameraController con 3 modos: ORBIT (azimuth/elevation/distance), FREE_FLY (yaw/pitch/WASD+QE), FOLLOW |
| `scene_query.h` | BVH: construcción top-down (centroid split, max 16 depth, leaves ≤4), query ray-AABB |
| `project.h` | Formato `.mgproject`: fuentes, cámara, background, time, frame, viewport. Save/Load con UTF-8 BOM |
| `workspace.h` | Viewport, Layer (visible/solo/opacity), Timeline (current_time, fps, loop, play/pause) |

### Visor (`visor/main.cpp`)

Aplicación Win32 completa:
- Ventanas: main, viewport (render), edit (text), status (info)
- LoadScene: FileMapping → Renderer::load() → SceneGraph → AABB → BVH → CameraController
- Render loop: BVH query → ray march → SetDIBitsToDevice a ~60fps
- Input: WASD+QE (fly), mouse (orbit), F1 (mouse look), F5 (re-render), +/− (zoom)

## Formatos de Escena

### RIH (`.rih`)
JSON con versión, dimensiones, cámara, background, materiales, nodos SDF, luces.

### MGProject (`.mgproject`)
Formato de proyecto UTF-8 con BOM: fuentes (paths wide), snapshot de cámara, background, time, frame, viewport.

## Tests

219 tests, todos pasando:

| Suite | Archivo | Tests | Cobertura |
|-------|---------|-------|-----------|
| OS+RHI | `tests/test_os_rhi.cpp` | 36 | Arena, FileMapping, Timer, Window, RHI init/dispatch/present |
| Render | `tests/test_render.cpp` | 101 | Expresiones (29), SDF primitives (15), Scene loading (18), SDF tree (12), Ray march (10), Shading (5), Optimizations (11), Pipeline (4) |
| Scene | `tests/test_scene.cpp` | 82 | AABB (17), SceneGraph (16), CameraController (14), BVH (6), Project (14), Workspace (15) |

## Quick Start

```batch
# Build visor
build.bat

# Build tests
cd tests
build_test.bat          # test_os_rhi.exe
build_render_test.bat   # test_render.exe
build_scene_test.bat    # test_scene.exe

# Run visor (requiere bodegon.rih en ../../Lenguaje Hermetico/ejemplos/)
debug_visor.bat
```

## Controles del Visor

| Tecla | Acción |
|-------|--------|
| WASD | Movimiento (modo FREE_FLY) |
| Q/E | Subir/Bajar |
| Mouse + arrastre | Orbitar (modo ORBIT) |
| F1 | Activar/desactivar mouse look |
| F5 | Re-renderizar |
| +/− | Zoom |
| ESC | Salir |

## Estructura del Proyecto

```
Motor Grafico/
├── visor/          # main.cpp — Aplicación Win32
├── os/             # OS abstraction layer
│   └── win32/      # Implementaciones Win32
├── rhi/            # DirectX 11 Compute Shader RHI
├── render/         # CPU ray marching, SDF eval, scene loader
├── scene/          # SceneGraph, Camera, BVH, Project, Workspace
├── core/           # (Legacy) Versión anterior del renderer
├── tests/          # Suites de test (219 tests)
├── build/          # Build output + sample bodegon.rih
├── docs/           # 12 documentos de documentación
├── build.bat       # Build principal (visor)
├── build_min.bat   # Build mínimo (solo core)
└── debug_visor.bat # Lanzador con ruta a escena
```

## Dependencias

- MSVC (Build Tools o Visual Studio 2022+)
- C++17
- Windows x64
- DirectX 11 (d3d11.lib, d3dcompiler.lib, dxgi.lib)
- user32.lib, gdi32.lib
- No requiere SDK externo

## Estado Actual

**Alpha temprano** — renderizador CPU funcional con arquitectura limpia. Pendiente:
- Integración GPU compute shader
- Shadow rays (actualmente sin sombras)
- Volume rendering
- Editor UI completo
- Soporte Linux
