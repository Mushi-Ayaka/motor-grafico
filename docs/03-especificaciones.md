# 03 — Especificaciones

## Formato RIH (Render Intermediate Height)

Formato JSON que describe una escena SDF compilada. Cargado via `Scene::load(FileMapping)`.

### Estructura

```
{
  "version": 1,
  "scene": {
    "axes": { "X": 800, "Y": 600, "W": 1 },
    "camera": { "position": [x,y,z], "target": [x,y,z], "up": [x,y,z], "fov": 60 },
    "background": [r,g,b]
  },
  "materials": [{
    "id": 0, "name": "madera",
    "base_color": [0.8,0.5,0.2],
    "roughness": 0.9, "metallic": 0.0,
    "emission": [0,0,0], "ior": 1.5, "opacity": 1.0
  }],
  "nodes": [{
    "id": 0, "name": "tablero", "type": "sdf",
    "material": 0, "mode": "solid",
    "transform": { "translate": [0,0.85,0], "rotate": [0,0,0], "scale": [2,0.05,1] },
    "sdf": { "type": "box", "params": [1, 0.5, 0.5, 0] }
  }, {
    "id": 5, "name": "mesa", "type": "group",
    "children": [0, 1, 2, 3, 4]
  }],
  "lights": [{
    "name": "key", "type": "directional",
    "direction": [-0.5,-0.8,-0.3], "intensity": 1.0
  }]
}
```

### Tipos de nodo

| type | Descripción | Campos |
|------|-------------|--------|
| `sdf` | Primitiva SDF | `sdf.type`, `sdf.params[4]` |
| `group` | Agrupa hijos con transform | `children[]` (IDs) |
| `instance` | Instancia una definición | `def` (ID) |

### SDF types

| `sdf_type` | params | Fórmula |
|-----------|--------|---------|
| `sphere` | `[r,0,0,0]` | `\|\|p\|\| - r` |
| `box` | `[sx,sy,sz,0]` | `sdBox(p, s)` |
| `cylinder` | `[r,h,0,0]` | `max(\|p.xz\| - r, \|p.y\| - h/2)` |
| `torus` | `[R,r,0,0]` | `\|\|(\|p.xz\| - R, p.y)\|\| - r` |
| `plane` | `[d,0,0,0]` | `p.y + d` |
| `cone` | `[r,h,0,0]` | Inline con sdCone |
| `capsule` | `[r,0,0,0]` | `\|\|p\|\| - r` |
| `rounded_box` | `[sx,sy,sz,r]` | `sdBox - r` |
| `union` | `[k,0,0,0]` | `min(a, b)` |
| `subtract` | `[k,0,0,0]` | `max(a, -b)` |
| `intersect` | `[k,0,0,0]` | `max(a, b)` |
| `smooth_union` | `[k,0,0,0]` | `smoothMin(a, b, k)` |

### Parámetros de SDF (`Expr`)

Cada parámetro puede ser constante o expresión:

```json
"params": [0.5, {"expr": "sin(w)*0.5+0.5"}, 0, 0]
```

Variables disponibles: `x`, `y`, `z`, `w` (tiempo), `t` (alias de `w`).
Funciones: `abs`, `sin`, `cos`, `sqrt`, `pow`, `max`, `min`, `clamp`, `lerp`, `mix`, `length`.

## Formato .mgproject

Formato texto UTF-8 que guarda estado del workspace (NO dump del grafo):

```
MGPROJECT v1
source ..\ejemplos\bodegon.rih
camera 1 2 3 4 5 6 45
background 0.1 0.2 0.3
time 1.5
frame 0 30
viewport 0 0 800 600
END
```

## Expresiones (evalExpr)

Evaluador recursivo descendente con precedencia:

| Operador | Precedencia | Asociatividad |
|----------|-------------|---------------|
| `^` | 3 | derecha |
| `* /` | 2 | izquierda |
| `+ -` | 1 | izquierda |

Funciones: `abs(x)`, `sin(x)`, `cos(x)`, `sqrt(x)`, `pow(x,y)`, `max(x,y)`, `min(x,y)`,
`clamp(lo,hi,x)`, `lerp(a,b,t)`, `mix(a,b,t)`, `length(x,y,z)`.

Constantes: `pi` (3.14159), `e` (2.71828).

## Optimizaciones

### AABB early-out

Antes de evaluar un nodo SDF, se calcula la distancia punto-AABB. Si es mayor que el
mejor SDF actual + 0.001f, el nodo se salta. Es 100% conservador — nunca cambia el resultado.

### Epsilon adaptativo

```cpp
f32 eps = hit_eps * (1.0f + t * 0.01f);
```

- t=0: eps=0.001 (estricto)
- t=50: eps=0.0015
- t=100: eps=0.002

### BVH (Bounding Volume Hierarchy)

Árbol binario espacial construido top-down:
- Split por centroide en eje mayor
- Profundidad máxima 16
- Hojas de ≤4 nodos
- No se usa a menos que el lenguaje produzca datos de volumen (condicional)

## Formato .obs (Observation)

Archivo binario independiente del `.ont`, guarda datos de observación de la escena.
Todas las secciones son opcionales, identificadas por flags en el header.

### ObsHeader (8 bytes)

| Offset | Campo | Tipo | Descripción |
|--------|-------|------|-------------|
| 0 | magic | u32 | `0x53424F21` ("!OBS") |
| 4 | flags | u32 | Bitmask de secciones presentes |

Flags: `OBS_CAMERA=1`, `OBS_LIGHTS=2`, `OBS_TIMELINE=4`, `OBS_BACKGROUND=8`, `OBS_RESOLUTION=16`

### Secciones (orden fijo tras header)

1. **ObsCamera** (48 bytes) — position x3, target x3, up x3 (cada f32), fov f32
2. **ObsLightsHeader** (4 bytes) — count u32
3. **ObsLight** × count (36 bytes c/u) — type u32 (DIRECTIONAL=0, POINT=1), direction x3, position x3, color x3 (f32), intensity f32, falloff f32
4. **ObsTimeline** (24 bytes) — time f32, frame u32, start_frame u32, end_frame u32, fps u32, loop u32
5. **ObsBackground** (12 bytes) — top_color x3, horizon_color x3, bottom_color x3 (f32)
6. **ObsResolution** (8 bytes) — width u32, height u32

### Extensibilidad

Nuevas secciones se añaden al final con nuevos flags. La aplicación ignora flags
desconocidos — compatible hacia adelante.

## PipelineConfig

Estructura que agrupa parámetros de renderizado configurables en `render/scene.h`.

| Campo | Tipo | Default | Descripción |
|-------|------|---------|-------------|
| `trace_max_steps` | int | 128 | Pasos máximos del ray march |
| `trace_hit_threshold` | f32 | 0.001 | Umbral de detección de impacto |
| `trace_t_min` | f32 | 0.01 | Distancia mínima de inicio del rayo |
| `trace_t_max` | f32 | 50.0 | Distancia máxima del rayo |
| `shade_ambient` | f32 | 0.1 | Factor de luz ambiente |
| `shade_diffuse` | f32 | 0.6 | Factor de luz difusa |
| `shade_specular` | f32 | 0.3 | Factor de luz especular |
| `shade_spec_power` | f32 | 16.0 | Exponente especular |
| `shadow_enabled` | bool | true | Habilita sombras |
| `shadow_steps` | int | 32 | Pasos del shadow ray |
| `shadow_max_dist` | f32 | 5.0 | Distancia máxima del shadow ray |
| `shadow_bias` | f32 | 0.01 | Bias para evitar acne de sombra |
| `post_tonemap` | bool | false | Habilita tonemapping |
| `post_gamma` | bool | false | Habilita corrección gamma |
| `post_exposure` | f32 | 1.0 | Exposición |
| `terrain_blend_strength` | f32 | 0.0 | Mezcla de terreno SDF (0=off, ~0.6 para terrenos). Reduce brillo metálico en superficies de terreno vía `(1 - metallic) * blend` |
