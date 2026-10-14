# 20 — Technical Design: Sistema de Ventanas + Layout por Defecto (v0.35-beta)

Fecha: 2026-10-14
Estado: Implementado
Autor: sesión opencode + Jonatan

---

## 1. Problema

La UI ImGui (T-F6) mostraba todos los paneles a la vez, flotando sin estructura
("todos sueltos"). Varias ventanas son stubs o test que no aportan. No hay forma
de ocultar/mostrar ventanas de forma centralizada.

## 2. Inventario de ventanas

| Ventana | Origen | Estado | En layout default | Menu Window |
|---|---|---|---|---|
| Viewport | visor_app.cpp | Funciona (imagen SDF + gizmos) | Sí (centro) | checkable |
| Editor .herm | visor_app + herm_editor | Funciona (editor + compile) | Sí (izq 25%) | checkable |
| Properties | visor_app | Funciona (estado/scheduler) | Sí (der 25%) | checkable |
| Console | console_panel | Funciona (log) | Sí (abajo 28%) | checkable |
| Profiler | profiler_panel | Funciona (FPS/frame) | No | checkable, off |
| Ontology Tree | ontology_panel | Funciona (árbol) | No | checkable, off |
| Inspector | inspector_panel | Funciona (F/Q/B) | No | checkable, off |
| Tensor Inspector | tensor_inspector | Funciona (tensor 1×8) | No | checkable, off |
| Timeline | timeline_panel | Funciona (frames) | No | checkable, off |
| Source Browser | source_browser | Parcial (requiere .mgproj) | No | checkable, off |
| Export Executable | export_dialog | **Stub** (escribe .bat con TODO) | No | **deshabilitado** |
| Command Palette | command_palette | Funciona (Ctrl+K) | No | no listado (atajo) |
| Hello ImGui | visor_app | Test T-F6 | No | **eliminada** |

Regla: si la ventana no está en el layout default → nace oculta (`visible=false`
en su header). Si además es stub → item deshabilitado (gris) en el menú.

## 3. Diseño

### 3.1 Menú "Window" (reemplaza a "View")

En la MenuBar de `drawEditorUI()`:

```
Window
  Viewport              [x]
  Editor .herm          [x]
  Properties            [x]
  Console               [x]
  ────────────────
  Profiler              [ ]
  Ontology Tree         [ ]
  Inspector             [ ]
  Tensor Inspector      [ ]
  Timeline              [ ]
  Source Browser        [ ]
  ────────────────
  Export Executable (stub)   ← BeginDisabled
```

Cada item es `ImGui::MenuItem(label, nullptr, &panel.visible)`.
Los paneles ocultos por default cambian su inicializador en su `.h` a `visible=false`.

### 3.2 Layout default con DockBuilder

API: `DockBuilder*` de `imgui_internal.h` (ImGui 1.91.9 docking).

```cpp
ImGuiID dockspace_id = ImGui::GetID("DockSpace");
if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {   // solo si no hay layout guardado
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

    ImGuiID center = dockspace_id;
    ImGuiID left   = DockBuilderSplitNode(center, ImGuiDir_Left,  0.25f, ...);
    ImGuiID right  = DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, ...);
    ImGuiID bottom = DockBuilderSplitNode(center, ImGuiDir_Down,  0.28f, ...);

    DockBuilderDockWindow("Editor .herm", left);
    DockBuilderDockWindow("Properties",   right);
    DockBuilderDockWindow("Console",      bottom);
    DockBuilderDockWindow("Viewport",     center);
    DockBuilderFinish(dockspace_id);
}
ImGui::DockSpace(dockspace_id, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode);
```

```
┌──────────────┬──────────────────────┬──────────────┐
│ Editor .herm │                      │  Properties  │
│   (25%)      │      Viewport        │    (25%)     │
├──────────────┤      (centro)        ├──────────────┤
│   Console    │                      │              │
│   (28%)      │                      │              │
└──────────────┴──────────────────────┴──────────────┘
```

- `DockBuilderGetNode(...) == nullptr` ⇒ **primera ejecución** (o `imgui.ini`
  borrado). Si el usuario reacomoda paneles, ImGui persiste en `imgui.ini`
  (`io.IniFilename` default) y el layout default NO se reaplica.
- Se borra `imgui.ini` stale (contenido previo: `Size=46,32`, sin sección
  `[Docking]`) para que el layout default se aplique limpio en el primer arranque.

### 3.3 Cambios de estado

- `visor_app.h`: `bool show_viewport = true;`, `bool show_properties = true;`
  (los Begin pasan a usar `&p_open` para permitir cerrar con X).
- Headers con default `visible=false`: `profiler_panel.h`, `ontology_panel.h`,
  `inspector_panel.h`, `tensor_inspector.h`, `timeline_panel.h`,
  `source_browser.h`.
- Ventana "Hello ImGui" eliminada de `drawEditorUI()`.

## 4. Criterios de aceptación (EARS)

- WHEN el visor arranca sin `imgui.ini` THEN THE visor SHALL crear el layout
  4 paneles (Viewport/Editor/Properties/Console) dockeados.
- WHILE exista un `imgui.ini` con sección `[Docking]` THEN THE visor SHALL
  respetar el layout del usuario y NO reaplicar el default.
- WHEN el usuario abre el menú Window THEN THE menú SHALL mostrar un checkbox
  por cada ventana funcional con su estado real de visibilidad.
- IF una ventana es stub (Export Executable) THEN THE item de menú SHALL
  aparecer deshabilitado (gris, no clickeable).
- WHEN el usuario presiona F2 THEN THE overlay ImGui completo SHALL ocultarse
  sin afectar el layout persistido.
- WHEN el usuario cierra una ventana dockeada con su X THEN THE ventana SHALL
  poder reabrirse desde el menú Window y recuperar su posición dockeada.

## 5. Fuera de alcance

- Persistencia de visibilidad de paneles en `.mgproj` (solo layout en imgui.ini).
- Reordenar sub-layouts internos (tabs de Viewport, etc.).
- Atajos por panel (solo Ctrl+K paleta).

## 6. Archivos tocados

| Archivo | Cambio |
|---|---|
| `visor/visor_app.cpp` | menú Window, DockBuilder default, Begin(&p_open), -Hello ImGui |
| `visor/visor_app.h` | `show_viewport`, `show_properties` |
| `visor/profiler_panel.h` | `visible=false` |
| `visor/ontology_panel.h` | `visible=false` |
| `visor/inspector_panel.h` | `visible=false` |
| `visor/tensor_inspector.h` | `visible=false` |
| `visor/timeline_panel.h` | `visible=false` |
| `visor/source_browser.h` | `visible=false` |
| `imgui.ini` | borrado (stale) |
