@echo off
setlocal

set VS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call %VS% 2>nul
if errorlevel 1 (
    echo No se encontro Visual Studio 2022 BuildTools
    exit /b 1
)

set THIS=%~dp0
set THIS=%THIS:~0,-1%

if "%VULKAN_SDK%"=="" set VULKAN_SDK=C:\VulkanSDK\1.4.350.0

if not exist "%THIS%\build" mkdir "%THIS%\build"

:: Try to remove old exe (may fail if running)
del "%THIS%\build\visor.exe" 2>nul

set RSP=%THIS%\build.rsp
echo /EHsc > "%RSP%"
echo /std:c++17 >> "%RSP%"
echo /utf-8 >> "%RSP%"
echo /O2 >> "%RSP%"
echo /MD >> "%RSP%"
echo /DBRICK_PROFILE >> "%RSP%"
echo /DVK_USE_PLATFORM_WIN32_KHR >> "%RSP%"
echo /DASMJIT_STATIC >> "%RSP%"
echo /Fe"%THIS%\build\visor_tmp.exe" >> "%RSP%"
echo /I "%THIS%" >> "%RSP%"
echo /I "%THIS%\external\asmjit" >> "%RSP%"
echo /I "%VULKAN_SDK%\Include" >> "%RSP%"
echo /I "%THIS%\external\volk" >> "%RSP%"
echo /I "%THIS%\external\VulkanMemoryAllocator\include" >> "%RSP%"
echo /I "%THIS%\external\imgui" >> "%RSP%"
echo /I "%THIS%\external\imgui\backends" >> "%RSP%"
echo /I "%THIS%\deps\lenguaje-hermetico\contrato" >> "%RSP%"
echo /I "%THIS%\deps\lenguaje-hermetico\herm" >> "%RSP%"
echo "%THIS%\visor\main.cpp" >> "%RSP%"
echo "%THIS%\visor\scene_manager.cpp" >> "%RSP%"
echo "%THIS%\visor\input_controller.cpp" >> "%RSP%"
echo "%THIS%\visor\window_manager.cpp" >> "%RSP%"
echo "%THIS%\visor\visor_app.cpp" >> "%RSP%"
echo "%THIS%\visor\herm_editor.cpp" >> "%RSP%"
echo "%THIS%\visor\scheduler.cpp" >> "%RSP%"
echo "%THIS%\visor\input_bus.cpp" >> "%RSP%"
echo "%THIS%\visor\anomaly_gate.cpp" >> "%RSP%"
echo "%THIS%\visor\console_panel.cpp" >> "%RSP%"
echo "%THIS%\visor\profiler_panel.cpp" >> "%RSP%"
echo "%THIS%\visor\ontology_panel.cpp" >> "%RSP%"
echo "%THIS%\visor\inspector_panel.cpp" >> "%RSP%"
echo "%THIS%\visor\gizmos_panel.cpp" >> "%RSP%"
echo "%THIS%\visor\tensor_inspector.cpp" >> "%RSP%"
echo "%THIS%\visor\undo_redo.cpp" >> "%RSP%"
echo "%THIS%\core\herm_bridge.cpp" >> "%RSP%"
echo "%THIS%\render\scene.cpp" >> "%RSP%"
echo "%THIS%\render\sdf_eval.cpp" >> "%RSP%"
echo "%THIS%\render\jit_compiler.cpp" >> "%RSP%"
echo "%THIS%\render\glsl_gen.cpp" >> "%RSP%"
echo "%THIS%\os\win32\mem.cpp" >> "%RSP%"
echo "%THIS%\os\win32\file.cpp" >> "%RSP%"
echo "%THIS%\os\win32\timer.cpp" >> "%RSP%"
echo "%THIS%\os\win32\win32.cpp" >> "%RSP%"
echo "%THIS%\render\vulkan_core.cpp" >> "%RSP%"
echo "%THIS%\render\vulkan_pipeline.cpp" >> "%RSP%"
echo "%THIS%\external\volk\volk.c" >> "%RSP%"
echo "%THIS%\external\imgui\imgui.cpp" >> "%RSP%"
echo "%THIS%\external\imgui\imgui_draw.cpp" >> "%RSP%"
echo "%THIS%\external\imgui\imgui_widgets.cpp" >> "%RSP%"
echo "%THIS%\external\imgui\imgui_tables.cpp" >> "%RSP%"
echo "%THIS%\external\imgui\backends\imgui_impl_win32.cpp" >> "%RSP%"
echo "%THIS%\build\imgui_impl_vulkan.obj" >> "%RSP%"
echo /link >> "%RSP%"
echo /SUBSYSTEM:CONSOLE >> "%RSP%"
echo user32.lib >> "%RSP%"
echo gdi32.lib >> "%RSP%"
echo comdlg32.lib >> "%RSP%"
echo shell32.lib >> "%RSP%"
echo "%THIS%\build\asmjit.lib" >> "%RSP%"
echo "%THIS%\build\libherm.lib" >> "%RSP%"

echo Compilando Shaders...
if not exist "build\shaders" mkdir "build\shaders"
"%VULKAN_SDK%\Bin\glslc.exe" render\ray_march.comp -o build\shaders\ray_march.spv
if errorlevel 1 (
    echo Error al compilar ray_march.comp
    exit /b 1
)

echo Compilando ImGui Vulkan backend (VK_NO_PROTOTYPES -> vk* via volk)...
cl /EHsc /std:c++17 /O2 /MD /DVK_NO_PROTOTYPES /I "%VULKAN_SDK%\Include" /I "%THIS%\external\imgui" /I "%THIS%\external\imgui\backends" /c "%THIS%\external\imgui\backends\imgui_impl_vulkan.cpp" /Fo"%THIS%\build\imgui_impl_vulkan.obj"
if errorlevel 1 (
    echo Error al compilar imgui_impl_vulkan
    exit /b 1
)

echo Compilando...

cl @"%RSP%"
del "%RSP%"

if exist "%THIS%\build\visor_tmp.exe" (
    move /y "%THIS%\build\visor_tmp.exe" "%THIS%\build\visor.exe" >nul
    echo Build OK: "%THIS%\build\visor.exe"
) else (
    echo Build FAILED
    exit /b 1
)

:: Build test_ont_bridge
echo Compilando test_ont_bridge...
cl /EHsc /std:c++17 /utf-8 /O2 /MD /DVK_USE_PLATFORM_WIN32_KHR /DASMJIT_STATIC /Fe"%THIS%\build\test_ont_bridge.exe" /I "%THIS%" /I "%THIS%\external\asmjit" /I "%VULKAN_SDK%\Include" /I "%THIS%\external\volk" /I "%THIS%\external\VulkanMemoryAllocator\include" /I "%THIS%\external\imgui" /I "%THIS%\external\imgui\backends" /I "%THIS%\deps\lenguaje-hermetico\contrato" /I "%THIS%\deps\lenguaje-hermetico\herm" "%THIS%\tools\test_ont_bridge.cpp" "%THIS%\core\herm_bridge.cpp" "%THIS%\render\scene.cpp" "%THIS%\render\sdf_eval.cpp" "%THIS%\render\jit_compiler.cpp" "%THIS%\render\glsl_gen.cpp" "%THIS%\os\win32\mem.cpp" "%THIS%\os\win32\file.cpp" "%THIS%\os\win32\timer.cpp" "%THIS%\os\win32\win32.cpp" "%THIS%\render\vulkan_core.cpp" "%THIS%\render\vulkan_pipeline.cpp" "%THIS%\external\volk\volk.c" "%THIS%\external\imgui\imgui.cpp" "%THIS%\external\imgui\imgui_draw.cpp" "%THIS%\external\imgui\imgui_widgets.cpp" "%THIS%\external\imgui\imgui_tables.cpp" "%THIS%\external\imgui\backends\imgui_impl_win32.cpp" "%THIS%\build\imgui_impl_vulkan.obj" /link /SUBSYSTEM:CONSOLE user32.lib gdi32.lib "%THIS%\build\asmjit.lib" "%THIS%\build\libherm.lib"
if errorlevel 1 (
    echo Error al compilar test_ont_bridge
) else (
    echo Build test_ont_bridge OK
)
