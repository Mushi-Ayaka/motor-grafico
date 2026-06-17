# 15 — Visión de Producto

> El motor ya no es solo tecnología. Esto es el plan para convertirlo en una **herramienta usable**.

## ✅ Lo que ya hay resuelto (la base)

| Componente | Estado |
|------------|--------|
| Renderizado SDF | ✅ 60-311 FPS en hardware real |
| Compilador `.ont` | ✅ Genera datos correctos |
| JIT / SIMD / GPU | ✅ Rutas optimizadas |
| Pipeline de archivos | ✅ `.herm` → `.ont` → imagen |
| Estabilidad | ✅ Sin crashes, manejo de errores |

**Eso es el 80% del trabajo.** El resto es contenido.

---

## 🧩 Lo que queda

### 1. Editor de nodos (objetivo principal)

**No es más difícil que el motor.** Es una aplicación de escritorio (o web) que:
- Lee `.herm` o estructuras internas.
- Muestra nodos (entidades, materiales, reglas) y conexiones.
- Permite modificar parámetros en tiempo real y recompilar.

**Lo que facilita:** ahora el motor es rápido, así que puedes hacer *live reload* de `.ont` sin esperar segundos.

**Plan:**
- Usar Dear ImGui o Qt para el UI.
- Conectar el visor como ventana de previsualización.
- Cuando el usuario cambia un parámetro, recompilar `.herm` a `.ont` (o usar el compilador en modo `--ont` y recargar el archivo).

**Por qué ahora:** el motor ya renderiza a 94-311 FPS. El tiempo de recompilación de `.ont` es despreciable. El cuello de botella ya no es técnico, es de interacción.

---

### 2. Sonido

El sonido en este motor es **más fácil que la imagen**:

| Aspecto | Imagen | Sonido |
|---------|--------|--------|
| Evaluación | Ray marching: cientos de pasos por píxel | Una evaluación 1D en el punto del oyente |
| Dimensionalidad | 2D (pantalla) × N píxeles | 1D (señal temporal) |
| Paralelismo | GPU masivo | CPU trivial |

**Plan:**
1. Definir fuentes sonoras como nodos con `mode: volume` y una propiedad `amplitude(W)`.
2. Evaluar la suma de amplitudes en el punto de la cámara:
   ```
   señal(t) = Σ fuente_i.amplitude(t - distancia_i / velocidad_sonido) * atenuación(distancia_i)
   ```
3. Renderizar a un buffer de audio (44.1 kHz) en CPU (es barato).
4. Reproducir con OpenAL o SDL Audio.

**Costo:** Despreciable comparado con el renderizado gráfico. Puede ejecutarse en un hilo separado.

---

### 3. Lógica del juego (IA, físicas, reglas)

El Lenguaje Hermetico es **declarativo**, pero la lógica del juego es **imperativa**. La solución es **separar**:

- **El mundo** (estático): definido en `.herm` (geometría, materiales, leyes físicas emergentes).
- **Los agentes** (dinámicos): código C++ o un script (Lua, Python) que actualiza la posición de los nodos dinámicos (personajes, proyectiles) cada frame.

**Plan:**
1. El bucle del juego corre a 60 Hz (o 30 Hz) en CPU.
2. Actualiza las transformaciones de los nodos dinámicos.
3. Pasa el nuevo estado al motor (que renderiza en GPU).
4. Para IA, usar máquinas de estado o behavior trees (librería estándar).

Esto es **independiente del motor gráfico**. El motor solo recibe datos; no tiene que "pensar".

---

### 4. Calidad visual (AO, sombras, reflejos)

El motor ya es rápido. Se puede permitir añadir efectos visuales sin matar los FPS.

**Prioridad de implementación:**
1. **AO:** 4 muestras de SDF en dirección de la normal (ya existe la función en CPU, solo pasarla a GPU). Coste: +20-30% de tiempo.
2. **Sombras:** Un rayo secundario hacia la luz (reutiliza `bvhEval`). Coste: +50% de tiempo.
3. **Múltiples luces:** Leer el array de `.obs`. Coste: lineal con el número de luces.

**Con AO + sombras, la escena pasará de "plana" a "con profundidad" en un par de días.**

---

### 5. Optimización continua

Ahora que hay buen rendimiento, **no optimizar ciegamente**. Medir primero.

**Foco actual:**
- Reducir el coste de normales (gradiente analítico) → 300% de ganancia potencial.
- Pasar AO y sombras a GPU → calidad sin penalizar CPU.
- Half-res + upscale para móviles → 4× más rápido.

---

## Mapa de ruta

```
Fase actual ───> Editor ───> Sonido ───> Lógica ───> Calidad ───> Optimización
(v0.26 GPU)      (v0.30)     (v0.35)    (v0.40)     (v0.45)      (v0.50)
```

Cada fase es independiente y puede desarrollarse en paralelo.
