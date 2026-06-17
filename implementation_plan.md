# Plan Integral: Brecha de Contrato entre el Lenguaje Hermético y el Motor Gráfico

## Contexto y Conclusión del Diagnóstico

Después de un análisis profundo que cubrió el compilador Hermético (`herm_ont.cpp`), el
motor gráfico (`ray_march.h`, `ray_march_simd.h`, `jit_compiler.cpp`) y la escena de
referencia (`catedral_hermetica.herm`), la conclusión es la siguiente:

> **El motor no estaba roto. El Lenguaje Hermético sí renderizaba imagen.
> El problema fue una brecha de contrato en la compilación: datos incorrectos
> emitidos por el compilador que el motor consumía ciegamente, causando 1FPS y
> artefactos visuales severos.**

Esto no es un fallo del motor ni del lenguaje por separado — es un fallo de
integración entre dos sistemas que evolucionaron sin un contrato verificado.

---

## Mapa del Problema: Los 3 Niveles de Brecha

### Nivel 1 — AABBs Incorrectas (🔴 CRÍTICO — causa directa del 1FPS)

**Dónde:** `herm/herm_ont.cpp:509-511`

```cpp
node->d_min = 0.01f; // conservative: tiny minimum distance
node->L = 1.0f;      // default Lipschitz constant
```

**Qué hace el motor con esto:** El BVH usa `d_min` para decidir cuánto saltar en el
ray marching (macrosalto). Con `d_min = 0.01`, el ray avanza de centímetro en
centímetro a través de toda la escena — destruyendo el sphere tracing que debería
avanzar por metros. El resultado: 80 pasos configurados se convierten en 80 pasos
de 1cm cada uno. El ray nunca cruza la escena.

**Impacto en la catedral:** El volcán está a `[9.0, -1.5, 7.0]`. Con `d_min=0.01`,
el ray necesita ~1200 pasos solo para llegar ahí. Con `trace_max_steps=80`, nunca
llega. **Los objetos desaparecen — agujeros negros.**

**También afectado:** Las AABBs de los nodos tienen un conservativo `±5.0f` por
defecto (línea `herm_ont.cpp:192-193`), haciendo que el BVH root cubra todo el
espacio y no pode nada útil.

---

### Nivel 2 — Expresiones de Color Descartadas (🔴 CRÍTICO — causa de colores incorrectos)

**Dónde:** `herm/herm_ont.cpp:662-665`

```cpp
mat.base_color[0] = m.tensor[4].is_expr ? 1 : m.tensor[4].constant;
mat.base_color[1] = m.tensor[5].is_expr ? 1 : m.tensor[5].constant;
mat.base_color[2] = m.tensor[6].is_expr ? 1 : m.tensor[6].constant;
```

**Qué hace:** Si el canal de color `tensor[4..7]` del material es una expresión
matemática (por ejemplo, una función de Fresnel de `std_chem_optics.hm` que
depende del ángulo de vista), el compilador silenciosamente la reemplaza con `1`.
El objeto queda blanco. Ningún warning, ningún error.

**En la catedral:** Los materiales son colores constantes (hardcoded), así que
este bug no afecta la catedral directamente. PERO cualquier escena que use las
librerías `std_chem_*` o `std_bridge_*` para color dinámico verá objetos blancos.
Este es un bug latente de alta severidad.

---

### Nivel 3 — Ausencia de Manejo Defensivo en el Motor (🟡 ALTO — amplifica errores upstream)

**Dónde:** `render/ray_march.h:91-96` y `render/ray_march_simd.h:458-507`

**Problema A — `classifyOntNodes` solo detecta `ONT_VAR_W` directo:**
```cpp
inline bool isBcDynamic(const uint8_t* bc, u32 len) {
    for (u32 i = 0; i + 4 < len; i += 5) {
        if ((OntOpcode)bc[i] == ONT_VAR_W) return true;
    }
    return false;
}
```
Si `w` aparece en una subexpresión inlineada de una librería (por ejemplo
`sin(x * w * freq)` donde `freq` es un parámetro), el opcode `ONT_VAR_W` sigue
estando en el bytecode — así que esto SÍ funciona. Punto a favor.

**Problema B — `evalDynamicNodes` evalúa TODOS los nodos dinámicos sin poda espacial:**
Los 4 nodos dinámicos de la catedral (3 esferas orbitantes + agua) tienen un AABB
de `±5`. Para cualquier pixel del frame, `brickSample` devuelve `1e9f` (vacío) y
el motor evalúa los 4 bytecodes dinámicos completos. Sin verificar si la esfera
está siquiera en la dirección del rayo. **4 evaluaciones de bytecode × 480,000
pixels × 80 pasos = 153 millones de bytecode executions por frame.**

**Problema C — Divergencia SIMD en `bvhEval4`:**
Al mezclar rayos activos y en-el-cielo en el mismo paquete de 4, los rayos vacíos
arrastran al BVH a evaluar con `res = 1e9f`, anulando toda poda por distancia.

---

## Escena de Referencia: Catedral Hermética

| Componente | Nodos | Estado |
|---|---|---|
| Arquitectura (suelo, techo, paredes, columnas, arcos, cúpula) | 16 | Estáticos — BrickMap OK |
| Esferas orbitantes `orbita_1/2/3` | 3 | **Dinámicos** — evalúan todo el tiempo |
| Agua de fuente `fuente_agua` | 1 | **Dinámico** — usa `w` |
| Cristales `std_chem_crystal` | 3 instances | Estáticos, bytecode potencialmente grande |
| Células `std_bio_cell` | 5 instances | Estáticos, bytecode moderado |
| Volcán `std_geo_terrain` | 1 instance | Estático, posición `[9,−1.5,7]` — fuera de AABB default |

**El volcán** a posición `[9, -1.5, 7]` queda **fuera del AABB conservativo `±5`**
del compilador. El BVH nunca incluye su bounding box correctamente. Resultado:
el volcán es invisible o produce artefactos de intersección falsa.

---

## Plan de Corrección

### Camino A — Arreglar el Compilador Hermético (Corrección Definitiva)

#### A1: Calcular `d_min` y AABB reales por muestreo

**Archivo:** `herm/herm_ont.cpp` — función `buildBvhRecursive` y `computeNodeAabb`

**Propuesta:** Tras compilar el bytecode de cada nodo, el compilador ejecuta el
evaluador de expresiones en una grilla 3D de muestra (ej. 8×8×8 = 512 puntos)
para encontrar el AABB real y el `d_min` mínimo observado.

```cpp
// En flattenNode(), después de compilar la expresión:
float d_min_observed = 1e9f;
float sample_step = 0.5f;
for (float sx = -scene_scale; sx <= scene_scale; sx += sample_step)
    for (float sy = -scene_scale; sy <= scene_scale; sy += sample_step)
        for (float sz = -scene_scale; sz <= scene_scale; sz += sample_step) {
            float d = evalBytecodeAt(fn.bytecode, sx, sy, sz, 0.0f);
            if (d > 0 && d < d_min_observed) d_min_observed = d;
            // Expandir AABB si d < 0 (punto dentro del sólido)
        }
node->d_min = std::max(d_min_observed * 0.9f, 0.001f); // 10% margen
```

Para nodos dinámicos (con `w`), muestrear también en varios valores de `w`
(ej. 0, π/2, π, 3π/2) y tomar el mínimo conservativo.

**Para la escena AABB real:** usar la posición del nodo (que el compilador ya
conoce) más el radio estimado de la primitiva, en vez de `±5` fijo.

#### A2: Soportar Expresiones de Color en el Tensor

**Archivo:** `herm/herm_ont.cpp:662-665`

**Propuesta:** Cuando `tensor[4..7]` es una expresión, compilarla a bytecode de
color separado y almacenarla en el `GraphNode` del `.ont`. El motor la evaluaría
al hacer shading para obtener color dinámico.

Esto requiere:
1. Extender `OntGraphNode` en `render/scene.h` con `color_bytecode_offset` y
   `color_bytecode_length`
2. El motor evalúa el bytecode de color en el punto de hit, pasando `nx,ny,nz`
   (normal) y `vx,vy,vz` (view dir) como variables disponibles — exactamente lo
   que la Capa Química necesita para Fresnel
3. El compilador emite el stream de color separado en el `.ont`

**Por ahora, mínimo viable:** Al menos emitir un WARNING cuando `is_expr = true`
para que no sea un fallo silencioso.

---

### Camino B — Manejo Defensivo en el Motor (Error Handling)

#### B1: Budget espacial para nodos dinámicos

**Archivo:** `render/ray_march.h` — función `evalDynamicNodes`

Antes de ejecutar el bytecode de un nodo dinámico, verificar si su AABB mundial
(guardada en `OntGraphNode.bbox_min/max`) intersecta con la esfera de búsqueda
actual del rayo (radio = distancia actual al mejor hit).

```cpp
inline f32 evalDynamicNodes(..., Vec3 ray_pos, f32 search_radius) {
    for (u32 i = 0; i < dyn_count; i++) {
        const auto& g = sc.graph_nodes[dyn_indices[i]];
        // Distancia punto→AABB del nodo dinámico
        float dx = fmax(0, fmax(g.bbox_min[0]-ray_pos.x, ray_pos.x-g.bbox_max[0]));
        float dy = fmax(0, fmax(g.bbox_min[1]-ray_pos.y, ray_pos.y-g.bbox_max[1]));
        float dz = fmax(0, fmax(g.bbox_min[2]-ray_pos.z, ray_pos.z-g.bbox_max[2]));
        if (dx*dx+dy*dy+dz*dz > search_radius*search_radius) continue; // SKIP
        // ... evaluar bytecode ...
    }
}
```

Cuando el compilador emita AABBs incorrectas, este check será ineficiente (el
AABB demasiado grande no filtra nada) pero **nunca producirá artefactos
adicionales** — solo sub-optimal. Es tolerante a fallas.

#### B2: Detectar `d_min` degenerado y compensar

**Archivo:** `render/ray_march.h` — función `bvhEval`

Si `d_min <= 0.001` (compilador emitió el default conservativo), el motor puede
inferir un `d_min` aproximado de la evaluación real:

```cpp
// Al inicio de bvhEval, si el d_min del nodo BVH es sospechosamente pequeño:
if (node.d_min < 0.005f && node.flags & BVH_FLAG_LEAF) {
    // No usar d_min para macrosaltos — evaluar normalmente
    // pero registrar métrica para debug
}
```

#### B3: Máscara de rayos activos en SIMD (el plan anterior, ahora contextualizado)

**Archivo:** `render/ray_march_simd.h` — función `bvhEval4`

Pasar `const bool* ray_mask` para deshabilitar rayos del cielo antes de entrar
al BVH. Esto era correcto como optimización de divergencia SIMD, y sigue siendo
válido — ahora sabemos que es la capa 3 del problema, no la raíz.

---

## Orden de Implementación Recomendado

```
Fase 1 — COMPILADOR (Camino A, alto impacto):
  [1] A1: AABB y d_min reales por muestreo en herm_ont.cpp
  [2] A1: AABB mundial usando posición del nodo (no ±5 fijo)
  [3] A2: WARNING cuando tensor[4..7] es expresión (sin implementar aún el stream)

Fase 2 — MOTOR DEFENSIVO (Camino B, error handling):
  [4] B1: Budget espacial para evalDynamicNodes
  [5] B2: Detección de d_min degenerado
  [6] B3: Máscara SIMD en bvhEval4

Fase 3 — COLOR DINÁMICO (Camino A completo):
  [7] A2: Bytecode de color separado en .ont + evaluación en shading
```

---

## Verificación del Plan

### Tests Automáticos
- Compilar `catedral_hermetica.herm` → verificar que el `.ont` resultante tenga
  `d_min > 0.1` en nodos de escala 1 unidad o mayor
- Verificar que el AABB del volcán cubra `[9, -1.5, 7] ± 3.0`
- Test de regresión: la escena `test_custom.ont` (esfera simple) debe renderizar
  en < 100ms a 800×600

### Validación Visual
- La catedral renderizada debe mostrar: volcán visible al fondo, esferas orbitantes
  animadas, vidriera con emission, cristales y células como objetos diferenciados
- FPS esperado post-fix: 5-15 FPS (CPU, resolución completa) con la complejidad
  actual de la catedral — alcanzable al corregir los macrosaltos

> [!IMPORTANT]
> **El agente constructor (Opencode) debe atacar primero la Fase 1** — específicamente
> el cálculo de AABB real en `herm_ont.cpp`. Esto solo requiere modificar el
> compilador Hermético, no el motor. El impacto en el framerate será el mayor
> de todos los cambios porque restaura el sphere tracing a sus pasos correctos.

> [!WARNING]
> **El `.ont` de la catedral debe regenerarse** después de arreglar el compilador.
> Los 60 archivos `.ont` existentes (`catedral_hermetica_0000.ont` a `_0059.ont`)
> fueron compilados con el compilador defectuoso — tienen `d_min=0.01` y AABBs
> incorrectas. No basta con arreglar el motor; los binarios deben recompilarse.
