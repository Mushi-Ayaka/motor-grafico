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

set RSP=%THIS%\build.rsp
echo /EHsc > "%RSP%"
echo /std:c++17 >> "%RSP%"
echo /utf-8 >> "%RSP%"
echo /Fe"%THIS%\build\visor.exe" >> "%RSP%"
echo /I "%THIS%" >> "%RSP%"
echo "%THIS%\visor\main.cpp" >> "%RSP%"
echo "%THIS%\core\rih_reader.cpp" >> "%RSP%"
echo "%THIS%\core\sdf_eval.cpp" >> "%RSP%"
echo "%THIS%\core\renderer.cpp" >> "%RSP%"
echo /link >> "%RSP%"
echo user32.lib >> "%RSP%"
echo gdi32.lib >> "%RSP%"

cl @"%RSP%"
del "%RSP%"

if %ERRORLEVEL% equ 0 (
    echo Build OK: "%THIS%\build\visor.exe"
) else (
    echo Build FAILED
)
