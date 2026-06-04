# 02 — Arquitectura

## Diagrama de capas

```
 ┌─────────────────────────────────────────────┐
 │                VISOR (Win32 UI)             │
 │  visor/main.cpp                             │
 │  Build: build.bat → build/visor.exe         │
 ├─────────────────────────────────────────────┤
 │              SCENE (escena)                 │
 │  scene_graph.h   — jerarquía, AABB          │
 │  camera.h        — CameraController         │
 │  scene_query.h   — BVH + ray-AABB           │
 │  project.h       — formato .mgproject       │
 │  workspace.h     — viewports, timeline      │
 │  Tests: tests/test_scene.cpp (82 tests)     │
 ├─────────────────────────────────────────────┤
 │              RENDER (pipeline)              │
 │  scene.h/cpp     — Scene, Node, Material    │
 │  sdf_eval.h/cpp  — evalExpr, SDF primitives │
 │  ray_march.h     — Ray, MarchResult, shade  │
 │  render.h        — Renderer (Scene+Frame)   │
 │  Tests: tests/test_render.cpp (101 tests)   │
 ├────────────────────┬────────────────────────┤
 │     RHI (DX11)     │       OS (sistema)     │
 │  rhi.h             │  os.h                  │
 │  d3d11.cpp         │  win32/mem.cpp         │
 │                    │  win32/file.cpp        │
 │                    │  win32/timer.cpp       │
 │                    │  win32/win32.cpp       │
 │  Tests: 15 tests   │  Tests: 21 tests       │
 ├────────────────────┴────────────────────────┤
 │              Windows / Win32                │
 └─────────────────────────────────────────────┘
```

## Capa OS (`os/`)

Abstracción sobre Win32:

| Archivo | Contenido |
|---------|-----------|
| `os.h` | Tipos base, Arena, FileMapping, Timer, Window |
| `win32/mem.cpp` | Arena bump allocator (VirtualAlloc, commit lazy 64KB) |
| `win32/file.cpp` | FileMapping (CreateFileMappingW + MapViewOfFileW) |
| `win32/timer.cpp` | Timer (QueryPerformanceCounter) |
| `win32/win32.cpp` | Window (Win32 class + message pump) |

## Capa RHI (`rhi/`)

Abstracción DirectX 11 Compute Shader:

| Archivo | Contenido |
|---------|-----------|
| `rhi.h` | Rhi, RhiBuffer, RhiTexture, RhiShader |
| `d3d11.cpp` | Init/dispatch/present, D3DCompile HLSL runtime |

## Capa Render (`render/`)

Pipeline de renderizado SDF:

| Archivo | Contenido |
|---------|-----------|
| `scene.h/cpp` | Scene (carga RIH vía FileMapping), Node, SdfNode, Material, Light, Expr |
| `sdf_eval.h/cpp` | evalExpr, SDF primitives, evalSdfTree, evalScene, calcNormal |
| `ray_march.h` | Ray, getRayDir, shade, rayMarch, Frame, renderScene, SceneQuery |
| `render.h` | Renderer (Scene + Frame + Arena + transforms + time) |

## Capa Scene (`scene/`)

Grafo de escena jerárquico + espacial:

| Archivo | Contenido |
|---------|-----------|
| `scene_graph.h` | SceneNode (parent/children, dirty flags, world transform), SceneGraph |
| `camera.h` | CameraController (ORBIT, FREE_FLY, FOLLOW) |
| `scene_query.h` | BvhNode, Bvh (construcción top-down, query ray-AABB) |
| `project.h` | Project (formato .mgproject) |
| `workspace.h` | Viewport, Layer, Timeline, Workspace |

## Capa Visor (`visor/`)

Aplicación Win32 interactiva:

| Archivo | Contenido |
|---------|-----------|
| `main.cpp` | WM_DESTROY/KEYDOWN/MOUSEMOVE, paintViewport, renderFrame, updateCamera |

## Dependencias

```
visor/ → scene/ → render/ → os/
visor/ → rhi/   → os/
tests/ → todas las capas
```

No hay dependencia `render → scene` ni `rhi → render`. La comunicación render↔scene usa
tipos planos (Aabb*, SceneQuery callback) sin acoplamiento de headers.
