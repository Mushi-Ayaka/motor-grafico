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

set RSP=%THIS%\build\temp.rsp
echo /nologo > "%RSP%"
echo /EHsc >> "%RSP%"
echo /std:c++17 >> "%RSP%"
echo /utf-8 >> "%RSP%"
echo /O2 >> "%RSP%"
echo /MD >> "%RSP%"
echo /Fe"%THIS%\build\test_debug.exe" >> "%RSP%"
echo /I "%THIS%" >> "%RSP%"
echo "%THIS%\test_debug.cpp" >> "%RSP%"
echo "%THIS%\render\scene.cpp" >> "%RSP%"
echo "%THIS%\render\sdf_eval.cpp" >> "%RSP%"
echo "%THIS%\os\win32\file.cpp" >> "%RSP%"
echo "%THIS%\os\win32\mem.cpp" >> "%RSP%"
echo /link >> "%RSP%"
echo user32.lib >> "%RSP%"

cl @"%RSP%"
del "%RSP%"
if %ERRORLEVEL% equ 0 (
    echo Build OK
    "%THIS%\build\test_debug.exe"
) else (
    echo Build FAILED
)
