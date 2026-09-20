@echo off
setlocal

set VS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call %VS% 2>nul
if errorlevel 1 (
    echo No se encontro Visual Studio 2022 BuildTools
    exit /b 1
)

if "%VULKAN_SDK%"=="" set VULKAN_SDK=C:\VulkanSDK\1.4.350.0

set THIS=%~dp0
set THIS=%THIS:~0,-1%

set OUTDIR=%THIS%\..\build
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set RSP=%OUTDIR%\test_f3.rsp
echo /EHsc > "%RSP%"
echo /std:c++17 >> "%RSP%"
echo /utf-8 >> "%RSP%"
echo /O2 >> "%RSP%"
echo /MD >> "%RSP%"
echo /DVK_NO_PROTOTYPES >> "%RSP%"
echo /Fe"%OUTDIR%\test_f3.exe" >> "%RSP%"
echo /I "%THIS%\.." >> "%RSP%"
echo /I "%THIS%\..\deps\lenguaje-hermetico" >> "%RSP%"
echo /I "%THIS%\..\deps\lenguaje-hermetico\contrato" >> "%RSP%"
echo "%THIS%\test_f3.cpp" >> "%RSP%"
echo "%THIS%\..\core\herm_bridge.cpp" >> "%RSP%"
echo "%THIS%\..\render\scene.cpp" >> "%RSP%"
echo "%THIS%\..\render\sdf_eval.cpp" >> "%RSP%"
echo "%THIS%\..\render\spirv_gen.cpp" >> "%RSP%"
echo "%THIS%\..\render\ri_optimizer.cpp" >> "%RSP%"
echo "%THIS%\..\os\win32\mem.cpp" >> "%RSP%"
echo "%THIS%\..\os\win32\file.cpp" >> "%RSP%"
echo "%THIS%\..\os\win32\timer.cpp" >> "%RSP%"
echo "%THIS%\..\os\win32\win32.cpp" >> "%RSP%"
echo /link >> "%RSP%"
echo user32.lib >> "%RSP%"
echo gdi32.lib >> "%RSP%"
echo advapi32.lib >> "%RSP%"
echo "%THIS%\..\build\libherm.lib" >> "%RSP%"

cl @"%RSP%"
del "%RSP%"

if exist "%OUTDIR%\test_f3.exe" (
    echo.
    echo Build OK: "%OUTDIR%\test_f3.exe"
) else (
    echo Build FAILED
    exit /b 1
)
