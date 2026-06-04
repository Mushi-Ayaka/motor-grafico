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

### Rendering completo

Sin BVH: `O(P * S * N)` donde P = píxeles, S = pasos, N = nodos
Con BVH: `O(P * log N + P * S * L)` donde L ≤ 4

## Perfiles de memoria

| Componente | Memoria |
|------------|---------|
| Arena (reserva) | 64 MB (VirtualAlloc, commit lazy) |
| Scene::nodes | ~128 bytes/nodo (Node + SdfNode + strings) |
| SceneGraph::nodes | ~104 bytes/nodo (SceneNode) |
| Frame (800x600) | 1.92 MB (u32* píxeles) |
| BVH | ~48 bytes/nodo hoja (BvhNode) |

## Tiempos estimados (bodegon.rih, 11 nodos, 800x600, 64 pasos/rayo)

| Modo | Tiempo estimado |
|------|----------------|
| Sin optimizaciones | ~100-200ms |
| Con AABB early-out | ~80-150ms |
| Con BVH + AABB | ~70-120ms |
| Con epsilon adaptativo | ~60-100ms |

(Escenas con 100+ nodos mostrarían diferencia más dramática con BVH — de ~1-2s a ~100-200ms.)
