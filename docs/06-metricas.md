# 06 — Métricas

## Líneas de código

| Capa | Headers | Código | Tests | Total |
|------|---------|--------|-------|-------|
| OS | 73 | 207 | 0 | 280 |
| RHI | 53 | 258 | 0 | 311 |
| Render (CPU) | 531 | 530 | 394 | 1455 |
| Render (GPU) | 130 | 504 | 0 | 634 |
| Scene | 614 | 0 | 283 | 897 |
| Visor | 0 | 385 | 0 | 385 |
| Build scripts | 0 | 139 | 0 | 139 |
| **Total** | **1401** | **2023** | **677** | **4101** |

## Tests

| Suite | Tests | Archivo |
|-------|-------|---------|
| OS + RHI | 36 | `tests/test_os_rhi.cpp` |
| Render | 101 | `tests/test_render.cpp` |
| Scene | 82 | `tests/test_scene.cpp` |
| **Total** | **219** | |

### Cobertura de tests por funcionalidad

| Funcionalidad | Tests |
|--------------|-------|
| Arena allocator | 9 |
| FileMapping | 7 |
| Timer | 5 |
| Window + RHI | 15 |
| Expression evaluator | 29 |
| SDF primitives | 15 |
| Scene loading (bodegon.rih) | 18 |
| SDF tree evaluation | 12 |
| Ray marching | 10 |
| Shading | 5 |
| AABB early-out / optimizaciones | 11 |
| Render pipeline | 4 |
| AABB unit | 17 |
| SceneGraph hierarchy | 16 |
| CameraController | 14 |
| BVH build + query | 6 |
| Project save/load | 14 |
| Workspace (layers, timeline) | 15 |

**Nota:** Los paths GPU (Vulkan) no tienen tests automatizados. La validación es visual + benchmark.

## Benchmark GPU Vulkan (v0.25)

Benchmark sobre escena real `catedral_hermetica_0000.ont`:

| Métrica | v0.24 (baseline) | v0.25 (Fase 2) |
|---------|-----------------|-----------------|
| Resolución | 974 × 617 | 974 × 617 |
| Complejidad escena | 30 graph nodes, 15 BVH, 10 mats | misma |
| Bytecode total | 3725 bytes | misma |
| Pasos raymarch | 80 (fijo) | 80 (adaptativo, d_min=0) |
| Stepping | `d * 0.8` | `max(d, leaf_d_min) * 0.8` |
| Normales | diff finita 4-eval | misma |
| **Tiempo promedio** | **281.2 ms** | **~250 ms** |
| **FPS** | **3.6** | **~4.0** |
| Iteraciones máximas | 80 | 80 (revertido de 256 tras diagnóstico) |

**Nota:** v0.25 inicial con 256 iteraciones causó regresión a 1-2 FPS. Revertido a
80 tras diagnóstico. La ganancia marginal (~4 FPS vs 3.6) puede deberse a cambios
de compilación de shader o variación de benchmark, no a d_min (que es 0 en esta escena).

### Costo por píxel estimado

```
Por píxel activo (hit):
   80 pasos × BVH traversal (~10 nodos/paso)        = 800 evaluaciones AABB
   + 4 evaluaciones BVH para normal (solo en hit)    = +40 evaluaciones AABB
   Cada BVH leaf visita ~4 graph nodes con bytecode VM (~25 inst c/u)
   Total bytecode ops ≈ 800 × 4 × 25 = 80,000 ops/pixel
```

### Cuello de botella identificado

1. **Divergencia de warps** en la VM de bytecode GLSL — hilos del mismo warp ejecutan bytecodes distintos (cada nodo tiene su expresión), forzando ejecución serializada de paths divergentes.
2. **4 evaluaciones BVH extra por normal** — cada pixel hit requiere 4 evaluaciones del BVH completo para diferencia finita.
3. **UBO layout mismatch** — el uniform block usa `layout(scalar)` pero el struct C++ tiene padding `std140`, causando que `tmax` se lea de offset incorrecto. No bloqueante pero añade latencia de validación.

### Comparativa con CPU

| Path | Resolución | Tiempo | FPS | Notas |
|------|-----------|--------|-----|-------|
| CPU Scalar | 1000×700 | ~1300ms | <1 | Baseline |
| CPU SIMD+MT | 1000×700 | ~16ms | 61 | SIMD + 8 threads |
| CPU SIMD+MT | Full HD | ~28ms | 36 | |
| **GPU Vulkan** (v0.25) | **974×617** | **~250ms** | **~4.0** | 80 iter, adaptativo |
| GPU Vulkan (v0.24) | 974×617 | 281ms | 3.6 | baseline pre-Fase2 |

La GPU es más lenta que el CPU SIMD+MT en esta escena pequeña (30 nodos) porque:
- El BVH pequeño (15 nodos) no justifica el overhead de dispatch GPU
- La VM bytecode en GLSL tiene más overhead por instrucción que el JIT nativo (x86-64) del CPU
- No hay optimizaciones GPU específicas (brick map, early exit por warp)

El valor de la GPU es para escenas grandes (1000+ nodos) donde el CPU se satura.

## Tamaño de builds

| Target | Tamaño |
|--------|--------|
| test_os_rhi.exe | ~48 KB |
| test_render.exe | ~80 KB |
| test_scene.exe | ~72 KB |
| visor.exe | ~84 KB |
| ray_march.spv | ~34 KB |

(Librerías estándar y de sistema enlazadas dinámicamente con `/MD`.)
