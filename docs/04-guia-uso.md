# 04 — Guía de Uso

## Build

### Prerrequisitos

- Visual Studio 2022 Build Tools
- SDK de Windows 10+
- DirectX 11 runtime

### Compilar visor

```batch
cd "Motor Grafico"
build.bat
→ build/visor.exe
```

### Compilar tests

```batch
cd "Motor Grafico/tests"
build_test.bat         → build/test_os_rhi.exe  (OS + RHI)
build_render_test.bat  → build/test_render.exe  (render)
build_scene_test.bat   → build/test_scene.exe   (scene)
```

### Flags de compilación

- `/std:c++17` — Estándar C++17
- `/O2` — Optimización máxima
- `/MD` — CRT dinámico (evita crash al iniciar)
- `/utf-8` — Encoding UTF-8 source y execution

## Tests

Ejecutar todos:

```batch
build\test_os_rhi.exe
build\test_render.exe
build\test_scene.exe
```

Resultados actuales: **219 tests, 0 fallos**
- OS + RHI: 36 tests (Arena, FileMapping, Timer, Window, RHI init/dispatch/present)
- Render: 101 tests (expr eval, SDF primitives, scene loading, tree eval, ray march, shading, optimizations, pipeline)
- Scene: 82 tests (AABB, SceneGraph, Camera, BVH, Project, Workspace)

## Visor

### Controles

| Tecla | Acción |
|-------|--------|
| F1 | Alternar mouse look (orbit) |
| F5 | Re-renderizar escena |
| Mouse drag | Orbitar cámara alrededor del target |
| + / - | Zoom in/out |
| W/S/A/D | Moverse (free fly, cuando no está en orbit) |
| Q/E | Subir/bajar (free fly) |

### Flujo de trabajo

1. Visor carga `bodegon.rih` automáticamente al iniciar
2. SceneGraph se inicializa desde la escena
3. BVH se construye si hay nodos SDF leaf
4. CameraController inicia en modo ORBIT
5. Workspace con viewport activo
6. F5 → Renderer::render() con SceneQuery (AABBs + BVH si disponible)
7. Resultado se pinta en el viewport via SetDIBitsToDevice
