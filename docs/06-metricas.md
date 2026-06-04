# 06 — Métricas

## Líneas de código

| Capa | Headers | Código | Tests | Total |
|------|---------|--------|-------|-------|
| OS | 73 | 207 | 0 | 280 |
| RHI | 53 | 258 | 0 | 311 |
| Render | 531 | 530 | 394 | 1455 |
| Scene | 614 | 0 | 283 | 897 |
| Visor | 0 | 322 | 0 | 322 |
| Build scripts | 0 | 139 | 0 | 139 |
| **Total** | **1271** | **1456** | **677** | **3404** |

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

## Tamaño de builds

| Target | Tamaño |
|--------|--------|
| test_os_rhi.exe | ~48 KB |
| test_render.exe | ~80 KB |
| test_scene.exe | ~72 KB |
| visor.exe | ~65 KB |

(Librerías estándar y de sistema enlazadas dinámicamente con `/MD`.)
