call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "C:\Users\Josue B\Desktop\Josue B\Documents\Jonatan Baron\Proyectos\proyecto de IA artistica\Motor Grafico"
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /MD /I "." /I "external\asmjit" /I "external\volk" /I "external\Vulkan-Headers\include" ^
  visor\visor_app.cpp visor\main_win32.cpp visor\input.cpp visor\camera.cpp visor\workspace.cpp ^
  visor\win32_window.cpp ^
  visor\imgui\imgui.cpp visor\imgui\imgui_draw.cpp visor\imgui\imgui_tables.cpp visor\imgui\imgui_widgets.cpp ^
  visor\imgui\imgui_impl_win32.cpp ^
  os\win32\mem.cpp os\win32\file.cpp os\win32\timer.cpp os\win32\win32.cpp ^
  render\scene.cpp render\jit_compiler.cpp render\sdf_eval.cpp render\brick_map.cpp ^
  render\glsl_gen.cpp render\vulkan_core.cpp render\vulkan_pipeline.cpp ^
  /Fe"build\visor.exe" /link user32.lib gdi32.lib shell32.lib advapi32.lib comctl32.lib d3d11.lib dxgi.lib d3dcompiler.lib vulkan-1.lib
