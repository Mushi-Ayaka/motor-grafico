# 01 — Fundamentos

## Namespace

Todo el código del Motor Gráfico vive bajo `namespace mg`. Capas internas usan sub-namespaces:

| Namespace | Capa |
|-----------|------|
| `mg` | Tipos compartidos (Vec3, Aabb, Scene, etc.) |
| `mg::scene` | Grafo de escena, cámara, BVH, proyecto, workspace |

## Tipos base (definidos en `os/os.h`)

```cpp
using u8  = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
```

## Vec3 (definido en `render/scene.h`)

Estructura fundamental para geometría 3D:

```cpp
struct Vec3 { f32 x, y, z; };
```

Operadores: `+`, `-`, `*` (escalar), `/` (escalar), `dot`, `length`, `normalize`, `cross`.

## Aabb (definido en `render/scene.h`)

Axis-Aligned Bounding Box:

```cpp
struct Aabb {
    Vec3 min;     // (1e9, 1e9, 1e9) por defecto
    Vec3 max;     // (-1e9, -1e9, -1e9) por defecto
    
    void expand(Vec3 p);         // expandir para incluir punto
    void expand(const Aabb& o);  // expandir para incluir otro AABB
    bool contains(Vec3 p);       // test de punto dentro
    f32 surfaceArea();           // área superficial del AABB
    bool intersect(Vec3 ro, Vec3 rdir, f32& tmin, f32& tmax);  // ray-AABB slab test
};
```

## Principios de diseño

1. **Sin `std::filesystem` ni `std::ifstream`**: Todo I/O via `CreateFileMappingW` + `MapViewOfFile`.
2. **CRT dinámico (`/MD`)**: El CRT estático causa procesos colgados al iniciar.
3. **Build via response file**: Evita parser bug de `cl.exe` con espacios en el path.
4. **Arena allocator**: Toda memoria temporal via bump allocator sobre VirtualAlloc. Reset por frame.
5. **Sin dependencias cíclicas**: `os → render → scene`. RHI es independiente.

## Archivos `_id_to_idx`

El RIH usa IDs de nodo (enteros arbitrarios). `Scene::_id_to_idx` mapea ID → índice de array.
Sin este mapeo, un GROUP puede autoreferenciarse (ej. mesa en bodegon.rih: id=0, children=[1,2,3,4,5],
donde el índice 5 es la mesa misma).
