# 02 — Arquitectura

## Diagrama de capas

```
 ┌─────────────────────────────────────────────────────────┐
 │                  VISOR (Win32 UI)                       │
 │  visor/main.cpp                                         │
 │  Build: build.bat → build/visor.exe                     │
 ├─────────────────────────────────────────────────────────┤
 │                SCENE (escena)                           │
 │  scene_graph.h   — jerarquía, AABB                      │
 │  camera.h        — CameraController                     │
 │  scene_query.h   — BVH + ray-AABB                       │
 │  project.h       — formato .mgproject                   │
 │  workspace.h     — viewports, timeline                  │
 │  Tests: tests/test_scene.cpp (82 tests)                 │
 ├─────────────────────────────────────────────────────────┤
 │              RENDER (pipeline)                          │
 │  scene.h/cpp     — Scene / OntScene (.obs)              │
 │  sdf_eval.h/cpp  — evalExpr, SDF primitives             │
 │  bytecode_vm.h   — execBcRaw (bytecode VM)              │
 │  ray_march.h     — Ray, bvhEval, shade, MT              │
 │  ray_march_simd.h— SIMD packet tracing (SSE)            │
 │  render.h        — Renderer (Scene+Frame+VulkanSceneData)│
 │  [GPU] vulkan_core.h/cpp   — VulkanContext (lifecycle)  │
 │  [GPU] vulkan_pipeline.h/cpp — VulkanSceneData (VRAM)   │
 │  [GPU] ray_march.comp → ray_march.spv (GLSL Shader)     │
 │  Tests: tests/test_render.cpp (101 tests)               │
 ├──────────────────────────┬──────────────────────────────┤
 │     RHI (DX11 legacy)    │       OS (sistema)           │
 │  rhi.h                   │  os.h                        │
 │  d3d11.cpp               │  win32/mem.cpp               │
 │                          │  win32/file.cpp              │
 │                          │  win32/timer.cpp             │
 │                          │  win32/win32.cpp             │
 │  Tests: 15 tests         │  Tests: 21 tests             │
 ├──────────────────────────┴──────────────────────────────┤
 │              Windows / Win32 + Vulkan                   │
 └─────────────────────────────────────────────────────────┘
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

## Capa RHI (`rhi/`) — Legacy

Abstracción DirectX 11 Compute Shader (no usada en el render path actual):

| Archivo | Contenido |
|---------|-----------|
| `rhi.h` | Rhi, RhiBuffer, RhiTexture, RhiShader |
| `d3d11.cpp` | Init/dispatch/present, D3DCompile HLSL runtime |

## Capa Render (`render/`)

Pipeline de renderizado SDF — dos paths coexistentes:

| Archivo | Contenido |
|---------|-----------|
| `scene.h/cpp` | Scene (carga RIH vía FileMapping), Node, SdfNode, Material, Light, Expr; OntScene (.ont + .obs), BytecodeVM; BrickMap (Sparse Voxel Octree V1) |
| `sdf_eval.h/cpp` | evalExpr, SDF primitives, evalSdfTree, evalScene, calcNormal |
| `bytecode_vm.h` | execBcRaw — bytecode VM stackless para SDF compiladas |
| `ray_march.h` | Ray, getRayDir, shadeOnt, rayMarchOnt, bvhEval (con out_material), Frame, renderScene, renderOntSceneMT, SceneQuery; classifyOntNodes, buildBrickMap, sampleBrickMap, evalHybrid, evalDynamicNodes, isBcDynamic |
| `ray_march_simd.h` | execBcRaw4, applyMatrix4, bvhEval4, renderOntSceneSIMD, renderOntSceneMTSIMD — SSE packet tracing |
| `render.h` | Renderer (Scene + Frame + Arena + transforms + time + VulkanSceneData), dispatch GPU/CPU mode, grid build/free |
| `vulkan_core.h/cpp` | VulkanContext: init, initSwapchain, createBuffer, copyBuffer, **drawFrame** (acquire→compute→barriers→copy→present) |
| `vulkan_pipeline.h/cpp` | VulkanSceneData: init (SSBOs via staging), initPipeline (SPIR-V load), updateUBO, **recordComputeCommandBuffer** (bind+dispatch), cleanup |
| `ray_march.comp` | Compute Shader GLSL: bytecode VM en GPU, Sphere Tracing, BVH traversal, output rgba8 |

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
| `main.cpp` | WM_DESTROY/KEYDOWN/MOUSEMOVE/WM_MBUTTONDOWN, paintViewport, renderFrame, updateCamera, --bench mode |
| `visor_app.h/cpp` | init, run (GPU path via drawFrame, CPU fallback), renderFrame (scale), paintViewport (StretchDIBits) |
| `input_controller.h/cpp` | handleMouseMove, handleKey, mouse_dragging flag |
| `window_manager.h/cpp` | ViewportProc, WindowProc, FPS overlay |

## Dependencias

```
visor/ → scene/ → render/ → os/
visor/ → render/vulkan_core.h (drawFrame)
render/vulkan_pipeline → render/vulkan_core (vk_ctx global)
tests/ → todas las capas
```

No hay dependencia `render → scene` ni `rhi → render`. La comunicación render↔scene usa
tipos planos (Aabb*, SceneQuery callback) sin acoplamiento de headers.

## Pipeline de archivos

### CPU path (legacy / debug)
```
.herm → [compilador Hermético] → .ont + .obs
                                      ↓
                              visor carga .ont + .obs
                                      ↓
                              buildBrickMap() → clasifica estáticos/dinámicos
                                      ↓
                              renderOntSceneMTSIMD(grid) → evalHybrid()
                                      ↓
                              framebuffer → GDI StretchDIBits → pantalla
```

### GPU path (producción — Vulkan Compute)
```
.herm → [compilador Hermético] → .ont + .obs
                                      ↓
                              visor carga .ont + .obs
                                      ↓
                VulkanSceneData::init() — sube BVH/Graph/Materials/Bytecode → VRAM (SSBO)
                                      ↓
             [cada frame] updateUBO(camera, time) → drawFrame()
                                      ↓
                           vkAcquireNextImageKHR
                                      ↓
                      Dispatch(W/8, H/8, 1) — ray_march.comp (GLSL)
                           GPU: Sphere Tracing + BVH + BytecodeVM
                                → output_buffer (rgba8, VRAM)
                                      ↓
               vkCmdCopyBufferToImage — output_buffer → swapchain image
                                      ↓
                            vkQueuePresentKHR → pantalla (V-Sync FIFO)
```

### GPU Path Safety

El compute shader incluye protecciones contra GPU hangs:

| Protección | Límite | Ubicación |
|-----------|--------|-----------|
| BVH traversal max iterations | 65535 | `ray_march.comp::bvhEval()` |
| Bytecode VM max instructions | 65535 | `ray_march.comp::execBcRaw()` |
| Graph node bounds check | `gi < graph_nodes.length()` | `ray_march.comp::bvhEval()` |
| Cleanup wait | `vkDeviceWaitIdle` | `vulkan_core.cpp::cleanup*()` |

### Benchmark GPU Actual (v0.24)

| Métrica | Valor |
|---------|-------|
| Escena | catedral_hermetica_0000.ont |
| Resolución | 974 × 617 |
| Complejidad | 30 graph nodes, 15 BVH, 10 mats, 3725 bytes bytecode |
| **Tiempo promedio** | **281 ms/frame** |
| **FPS** | **3.6** |
| Pasos raymarch | 80 fijos |
| Normales | 4 evaluaciones BVH por pixel hit |

**Cuello de botella:** Divergencia de warps en la VM de bytecode GLSL + 4 evaluaciones BVH extra para normales. Escenas pequeñas como esta (30 nodos) rinden mejor en CPU SIMD+MT (~36 FPS Full HD). La GPU brillará en escenas masivas (1000+ nodos).

### Dependencias del GPU Path

```
visor_app.cpp::run()
    → vk_ctx.drawFrame(scene_data)          [vulkan_core.cpp]
        → scene_data.recordComputeCommandBuffer()  [vulkan_pipeline.cpp]
            → vkCmdDispatch() → ray_march.comp [SPIR-V]
        → vkCmdCopyBufferToImage()
        → vkQueuePresentKHR()
    → InvalidateRect() + drawFpsOverlay()    [window_manager.cpp]
```

