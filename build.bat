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
echo /Fe"%THIS%\build\visor_tmp.exe" >> "%RSP%"
echo /I "%THIS%" >> "%RSP%"
echo "%THIS%\visor\main.cpp" >> "%RSP%"
echo "%THIS%\render\scene.cpp" >> "%RSP%"
echo "%THIS%\render\sdf_eval.cpp" >> "%RSP%"
echo "%THIS%\os\win32\mem.cpp" >> "%RSP%"
echo "%THIS%\os\win32\file.cpp" >> "%RSP%"
echo "%THIS%\os\win32\timer.cpp" >> "%RSP%"
echo "%THIS%\os\win32\win32.cpp" >> "%RSP%"
echo /link >> "%RSP%"
echo user32.lib >> "%RSP%"
echo gdi32.lib >> "%RSP%"

cl @"%RSP%"
del "%RSP%"

if exist "%THIS%\build\visor_tmp.exe" (
    move /y "%THIS%\build\visor_tmp.exe" "%THIS%\build\visor.exe" >nul
    echo Build OK: "%THIS%\build\visor.exe"
) else (
    echo Build FAILED
)
