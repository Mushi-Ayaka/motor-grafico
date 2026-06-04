# 11 — Estado del proyecto y deuda técnica

## Estado actual (Junio 2026)

El Motor Gráfico es un **visor 3D funcional** con arquitectura en capas, 219 tests unitarios,
y pipeline de renderizado SDF completo en CPU. No es un producto terminado — es una base sólida
sobre la que construir el editor y el compilador a GPU.

### Lo que funciona

| Componente | Estado |
|-----------|--------|
| OS: Arena allocator | ✅ Estable |
| OS: FileMapping (I/O) | ✅ Estable |
| OS: Timer (QPC) | ✅ Estable |
| OS: Window (Win32) | ✅ Estable |
| RHI: DX11 Compute Shader | ✅ Estable |
| Render: Scene loader (RIH JSON) | ✅ Estable (2 bugs corregidos) |
| Render: Expression evaluator | ✅ Estable (29 tests) |
| Render: SDF primitives (8 tipos) | ✅ Estable |
| Render: SDF tree eval | ✅ Estable (grupos, instancias, boolean ops) |
| Render: Ray marching | ✅ Estable |
| Render: Shading Blinn-Phong multi-luz | ✅ Estable |
| Render: AABB early-out | ✅ Implementado |
| Render: Epsilon adaptativo | ✅ Implementado |
| Scene: SceneGraph jerárquico | ✅ Estable (82 tests) |
| Scene: CameraController (ORBIT/FLY/FOLLOW) | ✅ Implementado |
| Scene: BVH (construcción + query) | ✅ Implementado |
| Scene: Project (.mgproject) | ✅ Implementado |
| Scene: Workspace (viewport, layers, timeline) | ✅ Implementado |
| Visor: Win32 app con viewport | ✅ Refactorizado a nuevas capas |
| Build: Response file + /MD | ✅ Corregido |

## Deuda técnica

### Bugs conocidos

| Bug | Impacto | Prioridad |
|-----|---------|-----------|
| Ninguno conocido. 219 tests pasan. | — | — |

### Deuda de implementación

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| `skipValue()` frágil | Parser JSON artesanal. Vale para RIH actual pero no es robusto para JSON arbitrario | Bajo |
| Sin shadow rays | `shade()` no lanza rayos secundarios hacia las luces. Las direccionales no proyectan sombra | Medio |
| Sin modo volume | `mode: volume` se ignora en el ray march actual (solo evalúa como SOLID) | Medio |
| Transform precompute limitado | `Renderer::transforms` solo guarda translate, no rotate/scale. `evalSdfTree` usa `applyTransform` completo si no hay transforms planos | Bajo |
| SceneGraph init frágil | `init()` itera O(n²) para detectar relaciones padre-hijo. No escala bien a cientos de nodos | Bajo |
| BVH no integrado en render pipeline | El BVH se construye y se pasa vía `SceneQuery`, pero `rayMarch` lo usa solo como filtro de nodos — falta integrarlo como reemplazo total del loop de `evalScene` | Medio |
| Visor sin auto-render | Solo renderiza con F5. No hay loop continuo. El `Sleep(16)` fija 60fps aunque no haya cambios | Bajo |
| Sin Resource Manager | Carga sincrónica. No hay caché, ni thread pool, ni async loading | Alto |
| Sin compilador a GPU | El SDF tree se evalúa en CPU. El pipeline RHI está listo pero no se usa para rendering | Alto |

### Deuda de arquitectura

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| `scene/` depende de `render/` | Idealmente `render/` debería depender de `scene/`. Hoy es al revés por razones históricas | Medio |
| `core/` legacy sin limpiar | `core/core.h`, `core/rih_reader.*`, `core/sdf_eval.*`, `core/renderer.*` existen pero no se usan | Bajo |
| Strings de SDF type | `sdf_type` es un `std::string` comparado en cada eval. Debería ser enum | Bajo |
| `_id_to_idx` expuesto | Miembro público `std::unordered_map` en Scene. Debería ser privado | Bajo |
| Sin PCH | Cada build recompila todo. Con PCH se reduciría a ~2s | Medio |
| Windows-only | Toda la capa OS es Win32. Linux requeriría rewrite de `os/win32/` | Alto |

### Deuda de testing

| Deuda | Descripción | Esfuerzo |
|-------|-------------|----------|
| Sin tests de regresión automatizados | Los tests se ejecutan manualmente. No hay CI | Bajo |
| Sin tests de visor | El visor no tiene tests (es Win32 UI). Habría que separar la lógica de render de la UI | Alto |
| Cobertura de BVH integration | Los tests de BVH no verifican que la integración con renderScene funcione correctamente | Bajo |
| Sin fuzz testing del parser JSON | El RIH loader aceptaría cualquier JSON malformado sin errores claros | Medio |

## Lo que falta

### Para MVP (Mínimo Producto Viable)

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Shadow rays en shade() | Nada | 1 día |
| Auto-render loop (tiempo real) | Nada | 1 día |
| Limpiar `core/` legacy | Nada | 1 hora |
| SDF type como enum | Scene loader | 2 horas |
| Tests de integración BVH+render | Nada | 1 día |

### Para versión alpha

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Resource Manager (async loading) | Thread Pool (nuevo) | 3 días |
| Compilador de escena → HLSL | RHI, Resource Manager | 2 semanas |
| Modo volume (Beer-Lambert) | Nada (solo el ray march loop) | 2 días |
| Editor: tree view de nodos | Visor refactor | 3 días |
| Editor: inspector de propiedades | Visor refactor | 2 días |
| Timeline animación de `w` | Workspace::Timeline | 1 día |

### Para versión beta

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Compilador JIT (idle→GPU) | Compilador HLSL, perfilado | 2 semanas |
| Sistema híbrido CPU/GPU | Compilador JIT | 2 semanas |
| Múltiples viewports | Workspace | 2 días |
| Gizmo de transform | CameraController | 3 días |
| Importador .herm watcher | Resource Manager | 2 días |
| Exportar frame como .png | Nada (solo escribir un BMP) | 1 día |

### Para producción

| Feature | Depende de | Esfuerzo |
|---------|-----------|----------|
| Refracción / SSS | Shading | 1 semana |
| Profundidad de campo | Ray march | 3 días |
| Partición espacial adaptativa | Compilador JIT | 2 semanas |
| Pipeline multi-GPU | RHI | 1 mes |
| Linux / Vulkan backend | OS+RHI | 2-3 meses |

## Resumen

```
Estado:        🟡 Alpha temprano (funcional, pero faltan features clave)
Tests:         🟢 219/219 pasan
Arquitectura:  🟢 Capas limpias, dependencias unidireccionales
Rendimiento:   🟡 CPU-only, sin GPU, sin sombras
Editor:        🔴 No existe (solo viewport + edit panel)
Portabilidad:  🔴 Windows-only
```
