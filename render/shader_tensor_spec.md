# Shader Tensor Spec — Tensor Buffer Layout

> **Especificación del tensor buffer en el compute shader. Sampling, W_frame, double buffer.**

---

## Tensor Buffer (GPU)

```glsl
// shaders/ray_march.comp
layout(set = 0, binding = 4) buffer TensorBuffer {
    float tensor_data[];  // [slot * 8 + component]
} tensor_buf;
```

**Layout por slot (8 floats, 32 bytes):**

| Offset | Componente | Descripción |
|--------|------------|-------------|
| 0 | X | Posición mundo (centro AABB o punto de impacto) |
| 1 | Y | Posición mundo |
| 2 | Z | Posición mundo |
| 3 | W | W_frame (coordenada transversal del corte) |
| 4 | R | Color material (si impacta) |
| 5 | G | Color material |
| 6 | B | Color material |
| 7 | opacity | Opacidad (flag visible=1 implícito si > 0) |

---

## Sampling Logic

```glsl
// Para cada píxel, para cada nodo en orden node_idx ascendente:
uint base = slot * 8;

if (/* píxel impacta nodo */) {
    tensor_buf.tensor_data[base + 0] = hit_point.x;
    tensor_buf.tensor_data[base + 1] = hit_point.y;
    tensor_buf.tensor_data[base + 2] = hit_point.z;
    tensor_buf.tensor_data[base + 3] = W_frame;
    tensor_buf.tensor_data[base + 4] = material.base_color.r;
    tensor_buf.tensor_data[base + 5] = material.base_color.g;
    tensor_buf.tensor_data[base + 6] = material.base_color.b;
    tensor_buf.tensor_data[base + 7] = material.opacity;
} else {
    // Centro del AABB mundo
    tensor_buf.tensor_data[base + 0] = aabb_center.x;
    tensor_buf.tensor_data[base + 1] = aabb_center.y;
    tensor_buf.tensor_data[base + 2] = aabb_center.z;
    tensor_buf.tensor_data[base + 3] = W_frame;
    tensor_buf.tensor_data[base + 4] = 0.0;
    tensor_buf.tensor_data[base + 5] = 0.0;
    tensor_buf.tensor_data[base + 6] = 0.0;
    tensor_buf.tensor_data[base + 7] = 0.0;  // flag visible=0
}
```

---

## W_frame

- `W_frame` = W del corte transversal actual (no W del último impacto)
- En v1, todos los nodos comparten `W_frame` = `renderer.time`
- Tiempo propio por nodo = v2

---

## Double Buffer (GPU → CPU)

```
tensor_buffer (STORAGE, GPU)
    ↓ vkCmdCopyBuffer
tensor_staging[0] (HOST_VISIBLE, frame N)
tensor_staging[1] (HOST_VISIBLE, frame N-1)
```

**Alternancia**: `tensor_staging_index = (tensor_staging_index + 1) % 2` cada frame

**CPU lee**: `tensor_staging[(tensor_staging_index + 1) % 2]` → datos del frame N-1

---

## Output Staging (Framebuffer)

```
output_buffer (STORAGE, GPU)
    ↓ vkCmdCopyBuffer
output_staging (HOST_VISIBLE, RGBA)
```

**Uso**: Determinismo test (SHA-256 del framebuffer cada 60 ticks)

---

## Dispatch

```glsl
// Compute dispatch: cubre toda la pantalla
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Dispatch: (width+7)/8 × (height+7)/8 × 1
// Orden de dispatch: node_idx ascendente (determinismo)
```

---

## Pipeline Barriers

```
1. vkCmdDispatch(...)
2. vkCmdPipelineBarrier(COMPUTE_SHADER → TRANSFER)
3. vkCmdCopyBuffer(tensor_buffer → tensor_staging[index])
4. vkCmdCopyBuffer(output_buffer → output_staging)
5. vkCmdPipelineBarrier(TRANSFER → HOST)
```

---

## Acceptance Criteria

- [ ] tensor_buffer binding 4, 8 floats por slot
- [ ] Sampling: centro AABB si no impacta, punto de impacto si impacta
- [ ] W_frame = renderer.time (no W del último impacto)
- [ ] Double buffer: tensor_staging[0] y [1] alternando
- [ ] Output staging para determinismo test
- [ ] Dispatch en orden node_idx ascendente
- [ ] Barriers correctos entre compute → transfer → host
