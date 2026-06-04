# Documento de Arquitectura: Lenguaje Hermético, Pipeline MLIR y Motor Gráfico en Tiempo Real

## 1. Visión General

Este documento describe tres componentes independientes pero relacionados:

- **A. Lenguaje Hermético (Representación Hermética)** — Modelo matemático de un universo 4D determinista basado en campos de distancia (SDFs), jerarquías ontológicas (Física, Química, Biología) y leyes tensoriales. Es el “lenguaje fuente” para describir mundos.

- **B. Pipeline MLIR de Campo Tensorial** — Sistema de compilación multi‑nivel (GTE → PTR → CTP → TB) para renderizado offline lossless de imágenes y animaciones. Transforma expresiones matemáticas en formatos comprimidos (TB) y PNG. Es totalmente independiente del lenguaje hermético (no usa SDFs, ni 4D, ni física emergente).

- **C. Motor Gráfico en Tiempo Real (Juego)** — Motor que **depende** del lenguaje hermético (lee su definición de mundo) y opcionalmente del pipeline MLIR (para generar assets pre‑calculados o estructuras de aceleración). Implementa un renderizador con ray marching optimizado (bisección afín, wavefront reordering, BVH 4D) en GPU, capaz de alcanzar 30‑60 FPS en hardware actual.

Los tres componentes son independientes entre sí: el lenguaje no usa MLIR, MLIR no conoce el lenguaje, y el motor es un consumidor de ambos (usa el lenguaje como entrada y puede invocar MLIR como herramienta auxiliar).

---

## 2. Componente A: Lenguaje Hermético (Representación Hermética)

### 2.1 Fundamentos Matemáticos

El universo se modela como un campo continuo en 4 dimensiones:

- **Dominio:** `ℝ⁴` con coordenadas `(X, Y, Z, W)` donde `W` es el tiempo (universo de bloque, eternista).
- **Codominio:** un estado interno estructurado `s = (d, m, c, A, P)` donde:
  - `d` = distancia a la superficie más cercana (SDF).
  - `m` = identificador de material.
  - `c = (R, G, B)` = color base.
  - `A` = opacidad / densidad.
  - `P` = propiedades físicas adicionales (densidad, rugosidad, etc.).
- El campo se define como `C: ℝ⁴ → S`. Para la observación (renderizado), se colapsa a `output = collapse(C(p)) = (R,G,B,A)`.

### 2.2 Operadores Fundamentales

- **Perturbación (𝒫):** rompe la simetría del vacío (estado cero) asignando un estado no nulo a una región `(X,Y,Z,W)` mediante una función generatriz `f(p)`.
- **Influencia (ℐ):** transforma el estado existente aplicando operaciones matemáticas (lineales, multiplicativas, etc.) sobre uno o varios tensores.
- **Aniquilación (𝒦):** suprime la manifestación visual sin eliminar la coordenada (pone canales visuales a cero).

### 2.3 Leyes Tensoriales y Geometría Implícita

- Las formas se definen mediante campos escalares de distancia (SDFs) exactos: esfera, caja, cilindro, plano, etc.
- La unión de objetos usa `select_closer(p) = argmin(d_i)` que selecciona el estado más cercano.
- Acoplamiento tensorial: permite sólidos rígidos (distancia relativa bloqueada) o cuerpos deformables (gradiente elástico).
- Interfaz óptica (la “piel”): donde el gradiente de `d` es discontinuo, se calcula la normal y se aplica BRDF.

### 2.4 Leyes Temporales (Universo de Bloque)

- El tiempo `W` es una cuarta dimensión espacial. No hay evolución secuencial; todo el pasado y futuro coexisten en el bloque 4D.
- Movimiento: la posición de un objeto se expresa como función de `W`, p. ej. `X(W) = X₀ + v·W`. La “línea de mundo” es una curva en 4D.
- Renderizado por corte transversal: se fija `W = g(t)` (mapeo del tiempo real al eje W) y se evalúa el campo 3D resultante.

### 2.5 Pirámide Ontológica (Física‑Química‑Biología)

- **Física:** define `(d, A, T_local, bbox, motion, E)` (E = emisión). Resuelve visibilidad y ray casting.
- **Química:** define `(m, c, P, mode)` (material, color base, propiedades, modo sólido/volumen).
- **Biología:** organiza jerarquías (nodos con hijos/transformaciones) y reglas de herencia/ensamblaje.

### 2.6 Relación con el Renderizado (Observación)

El operador de proyección `O` aplica una matriz de cámara y una función cronológica para obtener la imagen 2D:

```
F_gte(x, y, t) = collapse( C( C⁻¹(x, y), Z_depth, g(t) ) )
```

donde `Z_depth` se obtiene por ray marching (sphere tracing) en el campo 3D resultante.

---

## 3. Componente B: Pipeline MLIR de Campo Tensorial (Offline)

### 3.1 Propósito y Flujo

Sistema de compilación multi‑IR para generar imágenes lossless (PNG) desde descripciones de alto nivel o desde imágenes raster. Sus etapas son:

```
.gte → GTE (P3) → .ptr → PTR (P2) → .ctp → CTB (P1) → .tb → RENDER → .png
.jpg/.png → T‑CTP (P0) → .ctp ──────────────────────────────┘
```

### 3.2 Niveles

| Nivel | Formato | Descripción |
|-------|---------|-------------|
| P3 | GTE (`.gte`) | Lenguaje funcional puro con expresiones por píxel, slots, ejes. Sin objetos, sin 3D, sin ruido. |
| P2 | PTR (`.ptr`) | Regiones rectangulares con fórmulas, carga de imágenes, bloques, paletas, blending. |
| P1 | CTP (`.ctp`) | IR canónico: lista de spans horizontales 1×N (RLE). |
| P0 | TB (`.tb`) | Bloques 8×8 comprimidos (P0, PAL_RGBA, PAL_XCANAL, RAW, SPAN). Lossless. |
| Render | PNG | Salida final. |

### 3.3 Características

- Determinista y reversible (existen decompiladores TB→CTP→PTR→GTE).
- No tiene noción de SDFs, ni de 4D, ni de iluminación emergente. Es puramente un generador de imágenes desde expresiones algebraicas.
- Usado actualmente para arte generativo, composición de fotos y animaciones paramétricas.

### 3.4 Independencia del Lenguaje Hermético

MLIR no conoce las leyes ontológicas, ni los campos de distancia, ni el tiempo como dimensión. Es un backend de compilación para el lenguaje hermético (sección 19 del documento), pero **no necesita** ser usado por el motor gráfico. El motor puede prescindir de MLIR y usar su propio sistema de precompilación.

---

## 4. Componente C: Motor Gráfico en Tiempo Real (Juego)

### 4.1 Dependencias

El motor **depende** de:

- **Lenguaje Hermético** (Componente A) como especificación del mundo: el diseñador escribe reglas ontológicas (SDFs, jerarquías, materiales). El motor carga esa definición (en formato fuente o precompilado) y construye las estructuras de datos necesarias.
- **Opcionalmente, MLIR** (Componente B) para generar assets pre‑calculados: por ejemplo, pre‑renderizar texturas, pre‑calcular campos de distancia comprimidos (`.sdfgrid`) o voxel grids de salto usando el pipeline CTB extendido a 3D. No es obligatorio.

El motor **no** utiliza MLIR para el renderizado en tiempo real; utiliza su propio sistema de compilación de shaders y estructuras de aceleración.

### 4.2 Arquitectura del Motor

#### 4.2.1 Precompilación Offline (Resolvedor)

- Lee el archivo de mundo en lenguaje hermético (`.herm` o similar).
- Aplica las reglas ontológicas y construye un **grafo de escena** (nodos con SDFs, transformaciones, materiales).
- Genera un **archivo binario de aceleración** (`.sdfacc`) que contiene:
  - Un BVH 4D linealizado (nodos con bounding boxes 4D y punteros a sub‑grafos locales).
  - Opcionalmente, un grid de vóxeles 3D (para cada corte en W) con distancias de salto máximo (voxel skip distance) o polinomios de regresión lineal por bloque.
  - Tablas de materiales, texturas, animaciones (keyframes de transformaciones).
- Este archivo es el único que necesita el motor para ejecutarse.

#### 4.2.2 Estructuras en Tiempo Real (GPU)

- **TLAS/BLAS:** árbol de bounding boxes (4D) almacenado en buffers SSBO. Cada nodo tiene centro, radio, `d_min` (distancia mínima garantizada al vacío), `skip_index` (para salto rápido) e índice al sub‑grafo local.
- **Bitácora de macrosaltos:** para cada celda del grid de vóxeles (si se usa), se guarda un `float` con la distancia máxima segura.
- **Memoria:** entre 10 MB (escenas pequeñas) y 200 MB (mundos complejos). El grafo ontológico comprimido ocupa pocos MB.

#### 4.2.3 Algoritmo de Renderizado (Compute Shader)

Por cada píxel (rayo):

1. **Macro‑pasos:** Descender el BVH 4D. En cada nodo, si `d_min > ε` y el rayo está lejos de la superficie, se avanza `d_min` (o `d_min / L_local`) y se salta al siguiente nodo (`skip_index`). Esto permite cruzar grandes regiones vacías en O(log n) pasos.
2. **Aproximación por bisección (Interval Ray Tracing):** Si el rayo entra en una celda donde `d_min` es pequeño, se cambia a un algoritmo de búsqueda de raíces en 1D:
   - Se evalúa `Φ(p(t))` con aritmética de intervalos (o afín) sobre el sub‑grafo local.
   - Mediante bisección (20‑30 iteraciones fijas) se encuentra el `t` exacto donde `Φ=0`.
3. **Iluminación directa + AO:** una vez obtenido el punto de impacto, se calcula la normal (gradiente analítico o diferencias finitas) y se evalúa el BRDF del material, con sombras mediante shadow rays (bisección acotada).
4. **Volumétricos:** si hay medios participantes (niebla, agua), se integra la densidad a lo largo del rayo usando el grid de vóxeles (pasos adaptativos).

#### 4.2.4 Optimización de Divergencia (Wavefront Re‑ordering)

- En lugar de lanzar un rayo por píxel y dejar que diverga, el motor implementa un kernel de ordenación por grupos:
  - Cada paso del ray marching (macro o micro) se ejecuta en un compute shader que escribe el nuevo estado del rayo en un buffer global.
  - Un kernel de ordenación (radix sort) agrupa los rayos activos por su distancia restante o su celda BVH.
  - Así, todos los hilos de un warp toman el mismo camino, eliminando la divergencia.

### 4.3 Rendimiento Estimado

| Escenario | Nodos | Pasos totales (promedio) | FPS (RTX 3060) | FPS (Quest 2) |
|-----------|-------|--------------------------|----------------|---------------|
| Vacío + esfera | 1 | 5 | >200 | 90‑120 |
| Escena interior (50 objetos) | 200 | 10‑15 | 120‑150 | 45‑60 |
| Paisaje complejo (500 objetos) | 1000 | 20‑30 | 60‑80 | 25‑35 |
| Ciudad densa (2000 objetos) | 3000 | 30‑40 | 35‑50 | 10‑15 |

- Con wavefront reordering y voxel skip distance se pueden multiplicar rendimiento por 2‑3x en escenas complejas.
- La memoria de video necesaria es baja (≤ 200 MB para el BVH y buffers), permitiendo su uso en consolas y móviles de gama alta.

### 4.4 Integración con el Lenguaje Hermético (Flujo de trabajo)

1. **Autoría:** El diseñador escribe el mundo en lenguaje hermético (archivos `.herm`), definiendo entidades, reglas, materiales y animaciones (como funciones de `W`).
2. **Precompilación:** El **Resolvedor** (herramienta independiente) compila el `.herm` y produce dos cosas:
   - Un archivo `.sdfacc` (estructura de aceleración) y opcionalmente un `.mlir` (para renderizado offline con MLIR).
   - (Opcional) Llama al pipeline MLIR para pre‑calcular texturas o grids de vóxeles comprimidos.
3. **Ejecución del juego:** El motor carga `.sdfacc` y renderiza en tiempo real. No necesita el compilador MLIR ni el lenguaje fuente.

### 4.5 Casos de Uso y Limitaciones

- **Adecuado para:** Juegos indie con mundos compactos (varios km²), geometría basada en SDFs (orgánica o arquitectónica), iluminación directa + AO, sin path tracing complejo. Permite tamaños de juego muy pequeños (datos de mundo en MB).
- **Limitaciones:** La evaluación de SDFs con ruido procedimental (fractales) incrementa el número de pasos; se recomienda usar ruido solo en materiales (no en geometría). Los objetos con miles de primitivas (p. ej. un bosque de hierba) requieren instancing y LOD agresivo.
- **Hardware recomendado:** Para 1080p@60 con 500 objetos, GPU dedicada (GTX 1060 o superior). Para móviles, limitar a 200 objetos y usar resolución dinámica.

---

## 5. Relación entre los Componentes

| Componente | Depende de | Es usado por |
|------------|-----------|--------------|
| **A. Lenguaje Hermético** | Ninguno (autónomo) | Motor gráfico (C) y Resolvedor |
| **B. Pipeline MLIR** | Ninguno (autónomo) | Opcionalmente por el Resolvedor (para pre‑bakeado) |
| **C. Motor gráfico** | Lenguaje Hermético (definición del mundo) | El juego final |
| **Resolvedor** (herramienta) | Lenguaje Hermético, opcionalmente MLIR | Precompila a `.sdfacc` y (opcional) a MLIR |

El motor gráfico **no contiene** el lenguaje hermético ni el MLIR. Solo consume sus salidas (`.sdfacc` y eventuales assets pre‑comprimidos). El lenguaje y MLIR pueden evolucionar independientemente; el motor solo necesita que el formato `.sdfacc` sea estable.

---

## 6. Conclusión

Se ha definido una arquitectura modular donde:

- El **Lenguaje Hermético** proporciona una base matemática rigurosa y expresiva para crear universos 4D deterministas.
- El **Pipeline MLIR** ofrece un sistema de compresión lossless y renderizado offline probado, independiente del paradigma SDF.
- El **Motor Gráfico en Tiempo Real** aprovecha las ventajas del lenguaje (compactación, jerarquías, exactitud) mediante técnicas de vanguardia (BVH 4D, bisección afín, wavefront reordering) para alcanzar tasas de fotogramas interactivas.

La separación estricta permite que cada componente se desarrolle, optimice y mantenga por separado, mientras que el motor se beneficia de lo mejor de ambos mundos. El resultado es un sistema factible para crear juegos con mundos inmensos y deterministas, de tamaño extremadamente reducido, que se ejecutan en hardware actual con un rendimiento aceptable.