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
echo /I "C:\VulkanSDK\1.4.350.0\Include" >> "%RSP%"
echo "%THIS%\visor\main.cpp" >> "%RSP%"
echo "%THIS%\visor\scene_manager.cpp" >> "%RSP%"
echo "%THIS%\visor\input_controller.cpp" >> "%RSP%"
echo "%THIS%\visor\window_manager.cpp" >> "%RSP%"
echo "%THIS%\visor\visor_app.cpp" >> "%RSP%"
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
echo "C:\VulkanSDK\1.4.350.0\Include\Volk\volk.c" >> "%RSP%"
echo /link >> "%RSP%"
echo /SUBSYSTEM:CONSOLE >> "%RSP%"
echo user32.lib >> "%RSP%"
echo gdi32.lib >> "%RSP%"
echo "%THIS%\build\asmjit.lib" >> "%RSP%"

echo Compilando Shaders...
if not exist "build\shaders" mkdir "build\shaders"
"C:\VulkanSDK\1.4.350.0\Bin\glslc.exe" render\ray_march.comp -o build\shaders\ray_march.spv
if errorlevel 1 (
    echo Error al compilar ray_march.comp
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
)
