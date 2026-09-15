# ONT Format v1 — Binary Specification

> **Formato binario del .ont. Contrato exacto para serialización/deserialización.**

---

## Header

```cpp
#pragma pack(push, 1)
struct OntHeader {
    uint32_t magic = 0x20544E4F;      // "ONT "
    uint32_t version = 1;
    float    epsilon = 0.001f;
    uint32_t node_count;
    uint32_t bvh_count;
    uint32_t material_count;
    uint32_t bytecode_size;
    uint32_t tensor_buffer_size;      // (N_nodos + 1) * 8 * 4 bytes (+1 = cámara slot 0)
    float    scene_aabb_min[4];
    float    scene_aabb_max[4];
    uint64_t reserved[7] = {};        // MUST BE 0. Reservado para v2+.
};
```

**Tamaño total header**: 80 bytes (verificar con `static_assert`)

---

## Tensor Slot Numbering

| Slot | Contenido | Owner |
|------|-----------|-------|
| 0 | Cámara (transform) | CameraSystem |
| 1..N | Nodos del grafo (instancias) | Shader / Systems |
| **Total** | N_nodos + 1 | — |

`tensor_buffer_size = (N_nodos + 1) * 8 * sizeof(float)` = `(N_nodos + 1) * 32` bytes

---

## Graph Node

```cpp
struct OntGraphNode {
    float    local_transform[16];   // column-major 4x4
    uint32_t material_id;
    uint32_t bytecode_offset;
    uint32_t bytecode_length;
    float    bbox_min[4];
    float    bbox_max[4];
    uint32_t tensor_slot;             // índice en tensor_buffer (1..N) — por INSTANCIA
    uint8_t  mode;                    // 0=SOLID, 1=VOLUME
    uint8_t  pad[3];
};
```

**Tamaño**: 100 bytes

---

## Material

```cpp
struct OntMaterial {
    uint32_t id;
    float    base_color[4];
    float    roughness;
    float    metallic;
    float    emission[3];
    float    opacity;
    uint32_t tensor_slot;             // mismo slot que el nodo al que aplica
    uint32_t reserved[3] = {};
};
```

**Tamaño**: 52 bytes

---

## BVH Node

```cpp
struct OntBvhNode {
    float    min[4];
    float    max[4];
    float    d_min;          // minimum safe distance for subtree culling
    float    L;
    int32_t  skip_index;     // stackless BVH: skip to this node after processing
    uint32_t first_node;     // leaf: first graph node index
    uint16_t node_count;     // leaf: number of graph nodes
    uint16_t flags;          // bit 0 = leaf
};
```

**Tamaño**: 52 bytes

---

## Memory Layout (sequential)

```
OntHeader                    (80 bytes)
OntBvhNode[bvh_count]        (52 * bvh_count bytes)
OntGraphNode[node_count]     (100 * node_count bytes)
bytecode[bytecode_size]      (bytecode_size bytes)
OntMaterial[material_count]  (52 * material_count bytes)
```

No hay padding entre secciones. Todo es `pack(1)`.

---

## Versioning

- v1: Formato actual
- v2: + tensor_slot en OntGraphNode/OntMaterial, + tensor_buffer_size en header
- v3: + material_pbr_extended, + animation_data

El campo `reserved[7]` en el header se usa para nuevos campos en v2+.

---

## Validation

```cpp
bool validateOntHeader(const OntHeader& h) {
    if (h.magic != 0x20544E4F) return false;
    if (h.version != 1) return false;
    if (h.node_count == 0) return false;
    if (h.tensor_buffer_size != (h.node_count + 1) * 8 * sizeof(float)) return false;
    for (int i = 0; i < 7; i++) if (h.reserved[i] != 0) return false;
    return true;
}
```

---

## Acceptance Criteria

- [ ] Header es 80 bytes exactos
- [ ] tensor_buffer_size = (N+1)*32
- [ ] reserved[7] siempre es 0 en v1
- [ ] Validación rechaza magic incorrecto
- [ ] Validación rechaza tensor_buffer_size inconsistente
