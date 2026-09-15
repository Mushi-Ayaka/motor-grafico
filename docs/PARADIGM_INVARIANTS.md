# PARADIGM INVARIANTS — motor-grafico

> **10 invariantes no negociables. Todo builder debe leer esto antes de escribir código.**

---

## INV-1: NO variables globales mutables
Todo estado vive en el grafo ontológico o en `.mgproj`. No hay `extern` mutables, no hay `static` state en funciones de lógica.

## INV-2: NO delta_time en la lógica
El tiempo = fixed timestep (ΔW=1/60) + W geométrico. No hay `glfwGetTime()` ni `QueryPerformanceCounter()` en la lógica de systems.

## INV-3: NO interpretación de strings en runtime
Todo se compila a bytecode (.ont). El render loop no parsea `.herm` ni strings de SDF.

## INV-4: NO dependencias externas innecesarias
Motor = hermético: Vulkan, asmjit, ImGui, volk, VMA. Nada más. No se agregan libs sin consultar.

## INV-5: NO estados ocultos en entidades
Todo es visible en el grafo y en el Inspector. No hay flags internos, contadores ocultos, ni estados implícitos.

## INV-6: NO threading sin determinismo garantizado
Si hay thread → resultado bit-a-bit reproducible. No hay races, no hay `std::async` sin sincronización determinista.

## INV-7: NO cambios en el paradigma sin consultar
Si una tarea parece romper una invariante → DETENTE Y PREGUNTA.

## INV-8: W es geometría, no reloj
`renderer.time = W`. No hay "reloj interno" separado. W es la coordenada transversal del espacio-tiempo.

## INV-9: El compilador es el corazón
NO interpretación de strings en render loop. `.herm` → bytecode → execute.

## INV-10: La red es convergencia por entropía
NO cliente-servidor autoritativo. Sync eventual, no lockstep.

---

## Checklist del Builder

Antes de cada PR, verificar:
- [ ] ¿Hay variables globales mutables? → INV-1 violada
- [ ] ¿Hay `delta_time` en lógica de systems? → INV-2 violada
- [ ] ¿Hay strings en render loop? → INV-3 violada
- [ ] ¿Se agregó una dependencia nueva? → INV-4 violada
- [ ] ¿Hay estado oculto? → INV-5 violada
- [ ] ¿Hay threads sin determinismo? → INV-6 violada
- [ ] ¿Rompe una invariante? → INV-7: preguntar
