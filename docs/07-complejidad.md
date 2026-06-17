# 07 — Complejidad

## Big-O

### Scene loading (`Scene::load`)

- Parseo JSON: `O(N)` donde N = bytes del archivo
- ID→index mapping: `O(M)` donde M = nodos (hash map insert)
- Expansión de compounds: `O(K)` donde K = sub-nodos inline
- **Total carga escena: O(N + M + K)**

### evalSdfTree por nodo

- Primitiva SDF: `O(1)` (funciones matemáticas acotadas)
- GROUP: `O(C)` donde C = hijos (min sobre todos)
- INSTANCE: `O(1)` (redirección a def)
- Boolean ops: `O(2)` (dos evaluaciones recursivas)
- **Total: O(profundidad máxima del árbol)**

### evalScene (sin optimizaciones)

- Itera todos los nodos: `O(N)` por punto del rayo
- Por paso de ray march: `O(S * N)` donde S = pasos, N = nodos

### AABB early-out

- Distancia punto-AABB: `O(1)` (12 FMA)
- Skip si distancia > best: reduce `N` efectivo a nodos cercanos
- **Caso típico: O(S * K)** donde K << N

### BVH query

- Construcción: `O(N log N)` (particionamiento recursivo)
- Query por rayo: `O(log N)` en promedio
- March con BVH: `O(S * log N + S * L)` donde L = nodos por hoja

### Rendering completo (CPU)

Sin BVH: `O(P * S * N)` donde P = píxeles, S = pasos, N = nodos
Con BVH: `O(P * log N + P * S * L)` donde L ≤ 4

### Rendering GPU (Compute Shader)

El dispatche es `(W/8, H/8)` workgroups con 64 threads c/u.

- **Por thread (1 píxel):** `O(S_logN + S*L + N_normal)` donde `N_normal` = 4 evaluaciones extra para normal
- **Divergencia:** En BVH traversal, threads del mismo warp pueden divergir si cada píxel visita diferentes nodos. En el peor caso (rayos que cruzan distintas partes del BVH), el warp serializa los paths → overhead O(N_warp * logN).
- **VM bytecode:** El switch de opcodes causa divergencia cuando threads del mismo warp ejecutan instrucciones distintas. Cada nodo tiene su propia secuencia de bytecode (~25 ops promedio), por lo que 32 threads en un warp pueden seguir 32 paths distintos.

**Complejidad teórica:** `O(P/SM_count * (S_logN + S*L + 4_logN + bytecode_overhead))`

### Complejidad adicional (safety guards)

- BVH traversal: límite de 65535 iteraciones (evita GPU hang por BVH mal formado)
- Bytecode VM: límite de 65535 instrucciones (evita GPU hang por bytecode corrupto)
- Graph node access: bounds check `gi < graph_nodes.length()` (OOB read protection)

## Perfiles de memoria

| Componente | Memoria |
|------------|---------|
| Arena (reserva) | 64 MB (VirtualAlloc, commit lazy) |
| Scene::nodes | ~128 bytes/nodo (Node + SdfNode + strings) |
| SceneGraph::nodes | ~104 bytes/nodo (SceneNode) |
| Frame (800x600) | 1.92 MB (u32* píxeles) |
| BVH | ~48 bytes/nodo hoja (BvhNode) |

### Memoria GPU (VRAM)

| Buffer | Tamaño | Ubicación | Uso |
|--------|--------|-----------|-----|
| BVH SSBO | `bvh_count * 52 bytes` | GPU_ONLY | BVH traversal |
| Graph SSBO | `node_count * 112 bytes` | GPU_ONLY | SDF instance data |
| Material SSBO | `mat_count * 48 bytes` | GPU_ONLY | PBR shading |
| Bytecode SSBO | `bytecode_size` | GPU_ONLY | SDF bytecode VM |
| UBO | 64 bytes | CPU_TO_GPU | Cámara + tiempo cada frame |
| Output buffer | `width * height * 4` | GPU_ONLY | Píxeles RGBA8 |
| **Total típico** | **~4-10 MB** | | Escena catedral ~500 KB sin output |

## Tiempos estimados (bodegon.rih, 11 nodos, 800x600, 64 pasos/rayo)

| Modo | Tiempo estimado |
|------|----------------|
| Sin optimizaciones | ~100-200ms |
| Con AABB early-out | ~80-150ms |
| Con BVH + AABB | ~70-120ms |
| Con epsilon adaptativo | ~60-100ms |

(Escenas con 100+ nodos mostrarían diferencia más dramática con BVH — de ~1-2s a ~100-200ms.)

### GPU profiling actual (catedral, 30 nodos, 974x617)

| Fase | Tiempo |
|------|--------|
| Compute shader (BVH + bytecode + shade) | ~280 ms (dominante) |
| Buffer→Image copy | <0.1 ms |
| Present | <0.1 ms |
| **Total frame** | **281 ms (3.6 FPS)** |

El cuello de botella es el compute shader — específicamente la evaluación bytecode y las 4 evaluaciones BVH extra para normales.
