# 12 — Lluvia de Ideas: Optimización de Motor SDF en CPU

Este documento contiene un análisis crítico del rumbo arquitectónico propuesto y una propuesta técnica detallada con estrategias a bajo nivel (microarquitectura, matemáticas, caché y compilación) para maximizar los FPS en renderizado SDF en CPU.

---

## 1. Análisis Crítico: Grid Estático 3D Uniforme ($64^3$)

La propuesta de separar nodos estáticos de dinámicos es excelente y es la forma estándar de resolver escenas complejas en tiempo real. Sin embargo, un **grid uniforme de $64^3$ presenta graves limitaciones técnicas**:

*   **Pérdida de Alta Frecuencia (Aliasing Geométrico):** En una escena como una catedral, los detalles finos (columnas, arcos, molduras) desaparecerán o se convertirán en "esferas amorfas" debido a la interpolación trilineal. Un voxel en un volumen de $20\text{m}^3$ a resolución $64^3$ mide $\sim 31\text{cm}$.
*   **Explosión de Memoria en Resoluciones Mayores:** Subir la resolución de forma uniforme a $256^3$ o $512^3$ incrementa el uso de memoria de manera cúbica:
    *   $64^3 \times 4\text{ bytes (float)} = 1.04\text{ MB}$ (cabe en L2/L3 cache).
    *   $256^3 \times 4\text{ bytes} = 67.1\text{ MB}$ (excede L3 cache de la mayoría de CPUs).
    *   $512^3 \times 4\text{ bytes} = 536.8\text{ MB}$ (ancho de banda de DRAM se vuelve el cuello de botella absoluto).
*   **No Preservación del Gradiente ($|\nabla d| \neq 1$):** La interpolación lineal en las celdas del grid suaviza las distancias pero destruye el gradiente unitario en las esquinas y superficies complejas, lo que causa subestimación de distancias (rayo no avanza) o sobreestimación (atraviesa la superficie).

### Alternativas de Representación

1.  **Sparse Voxel Directed Distance Fields (SVDDF) / Brick Maps:**
    *   Utilizar un grid grueso de nivel superior (ej. $16^3$) donde cada celda apunta a un bloque pequeño o "brick" de $8^3$ voxeles si hay geometría cerca, o contiene un macro-salto si está vacío (puntero `nullptr` o valor especial).
    *   *Ventaja:* Ahorra hasta un 90% de memoria en espacio vacío y permite tener una resolución efectiva alta (ej. equivalentemente $128^3$ o $256^3$) consumiendo menos de $10\text{MB}$.
2.  **SDF Octree (SVO-SDF):**
    *   Un árbol octal donde los nodos hoja se subdividen solo donde el gradiente de distancia es alto (cerca de la superficie).
    *   *Ventaja:* Excelente localidad espacial y saltos rápidos en zonas vacías.
3.  **Quantized/Normalized SDF:**
    *   Almacenar las distancias en el grid como enteros sin signo de 8 o 16 bits (`uint8_t` o `uint16_t`) normalizados entre la diagonal de la bounding box del objeto.
    *   *Ventaja:* Reduce el ancho de banda de memoria a la mitad o a un cuarto, lo que previene los fallos de caché (cache misses) al leer del grid.

---

## 2. Aceleración Algorítmica del Ray Marching

El sphere tracing básico es ineficiente en escenas con discontinuidades o cerca de superficies rasantes.

### A. Relaxed Sphere Tracing (RST)
En lugar de avanzar $t \leftarrow t + d(p)$, podemos usar un factor de sobre-relajación $\omega \in (1.0, 2.0)$ para tomar pasos más grandes:
$$t_{new} = t + \omega \cdot d(p)$$
*   **Detección de Oscilación:** Si en el paso siguiente el valor de la SDF es menor que el anterior o cambia de signo (atraviesa la superficie), se detecta un "overshoot". En ese caso, se retrocede al punto anterior y se reduce $\omega$ a $1.0$ para converger de forma segura.
*   *Impacto:* Reduce el número promedio de pasos por rayo en un **30% a 50%**, especialmente en áreas de luz rasante.

### B. Coherencia Temporal y Reproyección de Profundidad (Temporal Depth Reprojection)
*   En escenas interactivas, la cámara se mueve pero los objetos estáticos no cambian drásticamente entre frames.
*   **Idea:** Para cada píxel, proyectar su coordenada 3D en el frame anterior usando la matriz de vista-proyección anterior, leer el buffer de profundidad y usar ese valor $t_{prev}$ como el punto de inicio del march, restándole un pequeño margen de seguridad $\Delta t$ (ej. $5\%$).
*   *Impacto:* En zonas estáticas del frame, el número de iteraciones del rayo cae de 40-80 a **2-4 pasos**. Solo los bordes y oclusiones requerirán el raymarching completo.

### C. Cone Step Mapping / Shadow Step para Rayos Secundarios
*   Los rayos de sombras (Shadow Rays) y Ambient Occlusion (AO) no necesitan la intersección exacta; solo necesitan saber si el rayo se intersecta o no.
*   Para sombras, se puede incrementar el umbral de hit o usar un paso adaptativo agresivo ($1.5 \times d(p)$), deteniendo la marcha en el primer valor negativo o al salir de la bounding box.

---

## 3. Optimizaciones del Intérprete y Bytecode (Nodos Dinámicos)

Evaluar un intérprete basado en stack (`execBcRaw4`) dentro de un bucle de raymarching destruye la predicción de ramas del procesador debido al switch de opcodes.

### A. Compilación JIT Minimalista (Just-In-Time)
Dado que el bytecode solo tiene 20 opcodes y evalúa expresiones matemáticas puras sin control de flujo complejo (loops o condicionales anidados, excepto `ONT_SAMPLE`), podemos compilar las expresiones a código de máquina x86-SSE/AVX nativo en runtime al cargar la escena.
*   **Cómo:** Utilizar una librería ligera como **AsmJit** o escribir directamente bytes en una página de memoria ejecutable (`VirtualAlloc` con `PAGE_EXECUTE_READWRITE`).
*   **Traducción:** Cada instrucción del bytecode se expande a una o dos instrucciones SSE.
    *   Ej. `ONT_ADD` $\rightarrow$ `addps xmm0, xmm1`
*   *Impacto:* Elimina por completo el puntero de instrucción (`pc`), el switch-case, y el overhead de stack de la VM. La velocidad de evaluación geométrica puede subir **3x a 5x**.

### B. SIMD Vector Math para Opcodes Trascendentales
Actualmente, `execBcRaw4` hace fallback escalar para `ONT_SIN`, `ONT_COS`, `ONT_TAN`, `ONT_MOD` y `ONT_POW`, extrayendo las lanes a floats individuales, llamando a la biblioteca estándar de C y volviendo a empaquetar. Esto destruye la segmentación de la CPU.
*   **Solución:** Reemplazar con aproximaciones polinomiales vectorizadas (Chebyshev o Taylor minimax) en SSE.
    *   **Seno y Coseno SIMD:** Un polinomio de grado 5 o 7 para el rango $[-\pi, \pi]$ (y reducción de rango modular rápida) implementado puramente en SSE con instrucciones `_mm_mul_ps` y `_mm_add_ps` (o FMA si está disponible) corre en $\sim 15$ ciclos para los 4 canales simultáneamente, en comparación con los $>120$ ciclos del fallback escalar.
    *   **Potencia SIMD (`pow(x, y)`):** Implementar mediante aproximación de $2^{y \log_2(x)}$ usando manipulación directa del exponente de punto flotante de IEEE 754 a nivel de bits vectorizados.

### C. Simplificación de Bytecode y CSE (Common Subexpression Elimination)
*   El compilador de Hermético debe realizar pasadas de optimización sobre el árbol SDF antes de generar el bytecode:
    *   Pre-calcular sub-expresiones constantes en tiempo de compilación.
    *   Fusionar transformaciones afines consecutivas en una sola multiplicación de matriz $4\times4$.

---

## 4. Cache, Memoria y Paralelismo a Bajo Nivel

El renderizado por CPU está limitado principalmente por el ancho de banda y la latencia de la memoria caché.

### A. Tile-Based Rendering (Renderizado por Bloques)
*   *Problema actual:* Dividir el framebuffer en filas horizontales consecutivas para cada hilo rompe la localidad espacial del caché L1/L2. Cuando un rayo en el píxel $(0, 0)$ lee datos del grid 3D o del BVH, el rayo en $(0, 1)$ se beneficia, pero cuando el hilo procesa el píxel $(0, 500)$, los datos de la caché ya han sido desalojados.
*   *Solución:* Dividir la pantalla en **bloques (tiles) de $8 \times 8$ o $16 \times 16$ píxeles** y asignar los bloques a los hilos usando un sistema de cola de trabajo dinámica (Dynamic Work Stealing o `std::atomic`).
*   *Resultado:* Los hilos que procesan un tile mantendrán calientes las líneas de caché de los nodos BVH y voxeles locales comunes a esa región de la pantalla.

### B. Vectorización del Muestreo Trilineal (SIMD Grid Sampling)
La interpolación trilineal tradicional requiere 8 lecturas de memoria separadas y combinaciones lineales:
```cpp
float v = lerp(lerp(lerp(c000, c100, tx), ...), ...);
```
*   **Optimización SIMD:**
    *   Si los datos del grid están organizados de forma contigua en memoria para bloques de $2 \times 2 \times 2$ (layout de curvas Z/Morton o almacenamiento en bloques de voxeles), se pueden cargar los 8 valores de distancia en dos registros SIMD (`__m128`) con solo dos o tres instrucciones de carga de memoria.
    *   El cálculo de los coeficientes de interpolación y las sumas ponderadas se realiza de forma paralela en los registros.

### C. Alineación de Estructuras (Cache Line Alignment)
*   Alinear las estructuras `OntBvhNode` y datos del grid a límites de 64 bytes (`alignas(64)`) para evitar el fenómeno de *False Sharing* y asegurar que un nodo del BVH se cargue en una sola transacción de línea de caché.
*   Reducir el tamaño de `OntBvhNode` al mínimo indispensable: empaquetar coordenadas AABB como `half float` (16 bits) o usar cuantización relativa al nodo padre.

---

---

## 6. Optimizaciones Específicas para GPU (Vulkan Compute Shader)

El pipeline GPU actual rinde 3.6 FPS (281 ms) a 974×617. Las optimizaciones CPU no aplican directamente (no hay caché L1/L2, no hay SIMD manual, no hay branch prediction). Las GPU tienen su propio perfil de rendimiento.

### 6A. Eliminar divergencia de warps en la VM de bytecode

**Problema:** `execBcRaw` en GLSL usa un `if-else chain` de ~25 opcodes. Cada nodo del grafo tiene su propia secuencia de bytecode (SDF expression única). Dentro de un warp de 32 threads, cada thread puede estar ejecutando un opcode distinto → la GPU serializa los 32 paths.

**Solución 1: Shader especializado por escena.** En lugar de un shader genérico con VM, generar GLSL inline para cada nodo SDF al cargar la escena, compilarlo con `shaderc`/`glslang` a SPIR-V, y crear un pipeline por nodo. Elimina el switch por completo.

**Solución 2: Agrupar nodos por bytecode.** Si varios nodos comparten la misma expresión SDF (misma forma, misma operación), se evalúan juntos en el mismo warp. Esto reduce divergencia.

**Solución 3: Aplanar bytecode a un solo bloque lineal.** En lugar de switch, usar una secuencia de operaciones condicionales con `mix`/`step` para seleccionar la operación correcta sin divergencia.

**Impacto estimado:** 2×-5× según la solución.

### 6B. Reusar distancia del último paso para normal

**Problema:** `bvhNormal()` evalúa el BVH 4 veces (central + 3 ejes offset = 4 × 281/80 ≈ 14 ms por normal). En ~50% de píxeles que hacen hit, esto duplica el costo.

**Solución:** En el último paso del raymarching, antes del hit, ya tenemos `bvhEval(p) = d` donde `d < eps`. Ese valor `d` es la distancia en `p`. Podemos reusarla como valor central y solo calcular 3 diferencias laterales:

```glsl
float d_center = d;  // ya calculada en el paso que hizo hit
float dx = bvhEval(p + vec3(eps, 0, 0), ...).x;
float dy = bvhEval(p + vec3(0, eps, 0), ...).x;
float dz = bvhEval(p + vec3(0, 0, eps), ...).x;
vec3 n = normalize(vec3(dx - d_center, dy - d_center, dz - d_center));
```

Esto ya se hace en el shader actual — pero aún requiere 3 evaluaciones extra. La optimización real sería:

**Solución avanzada: normales analíticas.** Si el bytecode SDF es diferenciable, se puede generar código GLSL que calcule el gradiente analíticamente (cadenas de derivadas automáticas) durante la generación del shader. Esto reduce 4 evaluaciones BVH → 1 evaluación BVH + cómputo de derivadas (unas pocas FMA).

**Impacto estimado:** 1.5×-2× si se eliminan 3 de 4 evaluaciones BVH en píxeles hit.

### 6C. Brick Map en GPU (SDF cache)

**Problema:** Cada paso del raymarching evalúa el BVH completo. En escenas con miles de nodos, esto escala mal.

**Solución:** Subir el BrickMap (SdfGrid) a un SSBO o texture 3D en GPU. Antes de evaluar el BVH, samplear el brick map. Si la celda tiene SDF precomputado y no hay nodos dinámicos, devolver el valor cacheado (1 sample de texture 3D vs ~10-100 nodos BVH).

Desafíos:
- El brick map se construye en CPU y se sube a VRAM cada frame (o una vez si es estático)
- Texturas 3D tienen caché de textura (texture units) que es óptimo para acceso espacial
- Interpolación trilineal en hardware (texture sampler) da SDF suavizado gratis

**Impacto estimado:** 3×-10× en escenas con 1000+ nodos.

### 6D. Early exit por warp (temporal reprojection GPU)

**Problema:** Todos los threads del warp ejecutan los mismos 80 pasos de raymarching, incluso después de hacer hit.

**Solución:** Usar `subgroupBallot` (VK_KHR_shader_subgroup_ballot) para detectar cuándo todos los threads del warp han hecho hit o miss, y salir temprano del bucle de raymarching.

```glsl
uint active = gl_SubgroupEqMask;
for (int i = 0; i < 80; i++) {
    // ... raymarch step ...
    if (hit) active &= ~(1 << gl_SubgroupInvocationID);
    if (active == 0) break;  // todos hicieron hit, salir
}
```

**Impacto estimado:** 1.2×-2× (depende de la variación de profundidad en el warp).

### 6E. Compilar shader por escena en tiempo de carga

**Problema actual:** El shader genérico `ray_march.comp` tiene que ser lo suficientemente flexible para cualquier bytecode. Esto impide optimizaciones agresivas.

**Solución:** Al cargar un `.ont`, generar un shader GLSL especializado:
1. Reemplazar la VM de bytecode con operaciones inline directas para cada nodo
2. Si la escena no usa SIN/COS, eliminar esas ramas del shader
3. Si todos los nodos tienen el mismo material, simplificar el shading
4. Compilar con `shaderc` a SPIR-V, crear pipeline VkComputePipelineCreateInfo

Esto requiere integrar `libshaderc` o `glslang` en el build, y manejar pipelines como recursos (cachear, liberar).

**Impacto estimado:** 2×-4× (principalmente por eliminar divergencia de bytecode).

## 7. Resumen de Prioridad de Implementación (GPU)

| Optimización | Complejidad | Impacto Estimado | Notas |
|:---|:---|:---|:---|
| **Reusar distancia para normal** | Baja | **1.5×** | Solo guardar d del último paso, ahorra 1/4 de normales |
| **Early exit por warp** | Media | **1.2×-2×** | Usa subgroup ops, quita pasos redundantes |
| **Brick Map en GPU** | Alta | **3×-10×** | Cache de SDF en texture 3D, evita BVH en celdas estáticas |
| **Shader especializado por escena** | Alta | **2×-4×** | Elimina VM bytecode, inline directo, compila en carga |
| **Normales analíticas** | Muy Alta | **2×-3×** | Gradiente automático del SDF, elimina 4 evaluaciones BVH |

**Recomendación:** Empezar por "Reusar distancia para normal" (esfuerzo mínimo, impacto inmediato) y "Brick Map en GPU" (máximo impacto, abre la puerta a escenas grandes).


| Optimización | Complejidad | Impacto Estimado |
| :--- | :--- | :--- |
| **Separación Estático/Dinámico + SVDDF** | Media | **5x - 10x** (Evita VM en 90% de la escena) |
| **Tile-Based Rendering (Bloques $8\times8$)** | Baja | **1.2x - 1.5x** (Mejora dramática en caché L2) |
| **Polinomial SIMD para Sin/Cos/Mod** | Baja | **1.3x - 1.8x** (Evita cuellos de botella de divergencia SIMD) |
| **Relaxed Sphere Tracing** | Baja-Media | **1.3x - 1.5x** (Reduce número de iteraciones) |
| **JIT Compiler para Bytecode** | Alta | **2x** (Sobre la evaluación de nodos dinámicos) |
| **Coherencia Temporal (Reproyección)** | Alta | **3x - 5x** (Reduce pasos de raymarching en frames interactivos) |
