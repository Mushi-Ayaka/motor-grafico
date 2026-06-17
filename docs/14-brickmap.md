# 14 — BrickMap (Sparse Voxel Grid V1)

Aproximación "Sparse Voxel Octree-lite" en dos niveles para cachear y acelerar la geometría SDF estática. Reemplaza al antiguo `SdfGrid` uniforme (V0).

## Motivación

El diseño `SdfGrid` (V0) uniforme de 64³ consumía memoria ineficientemente y carecía de resolución en detalles finos. La gran mayoría del espacio 3D en una escena suele estar vacío.
La nueva arquitectura **V1 introduce el `BrickMap`**, dividiendo el espacio en dos niveles: una grilla macro de celdas (Top-Level) y "ladrillos" densos (Bricks) solo donde realmente hay geometría cercana.

## Arquitectura de Datos

El `BrickMap` se divide en dos estructuras jerárquicas:

1. **Top-Level Grid (16³ = 4096 celdas):** 
   Arreglo ligero `top_indices` de 32 bits por macro-celda.
   - Si la celda contiene geometría cercana (distancia menor a un margen), guarda el índice al ladrillo correspondiente (`brick_index + 1`).
   - Si es espacio vacío lejano, guarda un centinela (`SENTINEL = 0xFFFFFFFF`).

2. **Bricks (Ladrillos de 8³ = 512 voxels):**
   Arreglos densos de floats con las distancias SDF precalculadas.
   Se alojan contiguamente en un único buffer `float* bricks` de tamaño `brick_count * 512`.

### Resolución Efectiva y Memoria
- Cada Top-Level cell se subdivide en 8x8x8 voxels.
- Resolución total efectiva: 16 × 8 = **128³ voxels**.
- **Ahorro de Memoria:** Un grid uniforme de 128³ usaría ~8 MB. Con BrickMap, asumiendo un 10-20% de ocupación superficial, el uso cae a ~1 MB, logrando mejor resolución con menor costo espacial.

## Algoritmo

### 1. Construcción (Discovery & Allocation)
```text
1. Calcular el AABB global de los nodos estáticos.
2. Definir top_cell_size y voxel_size.
3. Para cada macro-celda en la grilla 16³:
   - Evaluar distancia SDF estática al centro de la celda.
   - Si distancia < (sqrt(3)/2) * top_cell_size + umbral:
     - Asignar un índice de brick (incrementar brick_count).
   - Sino:
     - Marcar como SENTINEL.
4. Alojar memoria contigua: `bricks = malloc(brick_count * 512 * sizeof(float))`
5. Para cada brick alojado:
   - Recorrer sus 8³ voxels.
   - Evaluar SDF estático en el centro de cada voxel y guardarlo en el array `bricks`.
```

### 2. Muestreo (Sampling / Ray Marching)
```text
Dado un punto P:
1. Calcular índice en el Top-Level Grid (16³).
2. Si está fuera de los límites o es SENTINEL:
   - Fallback analítico o distancia macro.
3. Obtener el brick_index = top_indices[idx] - 1.
4. Calcular la posición fraccional dentro del Brick (0.0 a 8.0).
5. Leer la distancia SDF en los 8 voxels adyacentes y aplicar Interpolación Trilineal.
```

## Beneficios frente a V0 (SdfGrid)
- **Empty Space Skipping:** Saltos ultrarrápidos en zonas `SENTINEL`.
- **Huella de Caché:** Los datos SDF para una superficie están contiguos en un brick de 2 KB (512 floats * 4 bytes), cabiendo perfectamente en L1.
- **Suavidad:** Muestreo trilineal evita las normales facetadas de la versión V0 (Nearest-Neighbor).
