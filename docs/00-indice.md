# Motor Gráfico — Documentación

Proyecto de visor 3D interactivo con workspace, basado en Signed Distance Fields (SDF),
expresiones paramétricas, y renderizado procedural. **Backend Vulkan Compute Shader (GPU primario)**,
con fallback CPU (SIMD+MT) para debug.

| # | Archivo | Descripción |
|---|---------|-------------|
| 00 | `00-indice.md` | **Índice general** |
| 01 | `01-fundamentos.md` | Tipos base, convenciones, principios de diseño |
| 02 | `02-arquitectura.md` | Capas OS / RHI / Render / Scene / Visor, **GPU pipeline Vulkan** |
| 03 | `03-especificaciones.md` | Formatos RIH, .mgproject, expresiones, SDF |
| 04 | `04-guia-uso.md` | Build, tests, visor, comandos |
| 05 | `05-implementacion.md` | Detalles de cada componente, **GPU: SSBO, UBO, Compute Shader** |
| 06 | `06-metricas.md` | **Benchmark GPU 3.6 FPS**, cobertura de tests, líneas por capa |
| 07 | `07-complejidad.md` | Big-O, perfiles de rendimiento, **GPU profiling** |
| 08 | `08-changelog.md` | Registro de cambios por sesión |
| 09 | `09-historial.md` | Historial completo de la sesión actual |
| 10 | `contrato-ontologico.md` | Contrato ontológico del dominio SDF |
| 11 | `11-estado-y-deuda.md` | Estado actual, deuda técnica, hoja de ruta |
| 12 | `12-brainstorming-optimizacion.md` | Brainstorming: diagnóstico, ideas, estrategias |
| 13 | `13-grid-sdf.md` | SDF Grid (V0): estructura, clasificación, evaluación híbrida |
| 14 | `14-brickmap.md` | BrickMap (V1): Sparse Voxel Grid, top-level, memoria dispersa |
| 15 | `15-vision-producto.md` | Visión de producto: editor, sonido, lógica, calidad visual, optimización |
