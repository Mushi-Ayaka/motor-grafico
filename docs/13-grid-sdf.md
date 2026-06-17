# 13 — SDF Grid (V0)

Grid 3D uniforme para acelerar el ray marching SDF cacheando nodos estáticos.

## Motivación

En escenas ontológicas complejas (catedral: miles de BVH leaves), cada paso de ray march
recorre el BVH completo. La mayoría de nodos son estáticos — sus SDF no cambian con el tiempo.
Un grid 3D precomputado permite evaluar nodos estáticos en O(1) por paso, delegando solo los
pocos nodos dinámicos al BVH.

## Algoritmo

### 1. Clasificación (`classifyOntNodes`)

Recorre todos los BVH leaves. Cada leaf es **estático** o **dinámico** según su bytecode:

| Condición | Clasificación |
|-----------|---------------|
| Contiene SIN, COS, TAN, POW, MOD, SAMPLE | Dinámico |
| Contiene PUSH que referencia registro `w` (tiempo) | Dinámico |
| Solo ops lineales con constantes (`w` no referenciado) | Estático |

### 2. Construcción (`buildSdfGrid`)

```
input: OntScene con N BVH leaves, cada uno con AABB + bytecode
output: SdfGrid

1. Calcular AABB escena = unión de AABBs de nodos estáticos (con margen 0.1)
2. cell_size = max(AABB_dim) / 64
3. dim_x/y/z = ceil(AABB_dim / cell_size)
4. Para cada nodo estático:
   - Rasterizar su AABB sobre celdas del grid
   - En cada celda: evaluar SDF en centro de celda, mantener mínimo
5. Para cada nodo dinámico:
   - Rasterizar su AABB sobre celdas
   - Agregar bitmask del nodo al data de cada celda
6. Celdas sin cubrir: sdf = +inf (fallback)
```

### 3. Evaluación híbrida (`evalHybrid`)

```
input: punto p, tiempo w, SdfGrid
output: distancia SDF

1. celda = (p - origin) * inv_cell_size (clamped a [0, dim))
2. mask = gridSample(grid, celda)
3. Si mask == 0:
     return sdf_cache[celda]  // solo estáticos, O(1)
4. Si mask == 0xFFFFFFFF:
     return evalOntSdfTree(sc, p, w)  // celda sin cobertura: fallback
5. En otro caso:
     static_sdf = sdf_cache[celda]
     dynamic_sdf = evalDynamicNodes(sc, p, w, mask)
     return min(static_sdf, dynamic_sdf)
```

## Estructura SdfGrid

```
origin: Vec3             — esquina min del AABB escena
inv_cell_size: Vec3      — 1.0 / cell_size (para lookup rápido)
dim_x, dim_y, dim_z: u32 — dimensiones del grid
capacity: u32            — dim_x * dim_y * dim_z
offsets: u32*            — byte offset en data[] por celda (+1 extra = total size)
data: u32*               — contenido de celdas empaquetado secuencialmente
```

### Formato de data[]

Cada celda tiene:
```
offset[celda] → [sdf_f32][mask0][mask1]...[maskN]
```

- `sdf_f32`: un f32 (almacenado como u32) con el SDF precomputado de nodos estáticos
- `mask0..maskN`: words de 32 bits, cada bit = un BVH leaf dinámico que intersecta la celda
- Bit 31 de cada word = 1 para el último word (terminador)

### Tamaño

Para un grid 64³ y una escena con ~2000 BVH leaves (~10% dinámicos = 200):
- `offsets`: 262144 × 4 bytes = 1 MB
- `data` promedio: por celda, 4 bytes (sdf) + ~0 (sin dinámicos en esa celda) = 4-8 bytes promedio
- Total estimado: ~1-3 MB

## Limitaciones V0

| Limitación | Impacto | Plan V1 |
|-----------|---------|---------|
| Resolución fija 64³ | Detalles finos pueden caer en celda grande | SVDDF (brick maps con resolución adaptativa) |
| Grid denso (incluso celdas vacías) | 1 MB para offsets aunque la escena ocupe pocas celdas | Grid sparse (hash 3D o chunked) |
| Clasificación heurística | Puede marcar nodo estático como dinámico (falso positivo = sub-óptimo pero correcto) | Precomputar flag en Herm compiler |
| Reconstrucción por frame | Cada frame rebuild. Escenas puramente estáticas lo construyen igual | Solo rebuild si `w` cambia |
| Sin SIMD en grid lookup | gridSample es escalar | Empaquetar 4 rayos SIMD |

## Integración

```
render.h: Renderer::render()
  → buildSdfGrid(sc)              // frame start
  → renderOntSceneMTSIMD(sc, obs, grid)
    → rayMarchOnt(..., grid)
      → evalHybrid(sc, p, w, grid)  // por paso
  → freeSdfGrid(grid)              // frame end
```
