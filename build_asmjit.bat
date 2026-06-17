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
if not exist "%THIS%\build\asmjit_obj" mkdir "%THIS%\build\asmjit_obj"

if exist "%THIS%\build\asmjit.lib" (
    echo asmjit.lib ya existe.
    exit /b 0
)

echo Compilando AsmJit...

set RSP=%THIS%\build\asmjit.rsp
echo /EHsc > "%RSP%"
echo /std:c++17 >> "%RSP%"
echo /utf-8 >> "%RSP%"
echo /O2 >> "%RSP%"
echo /MD >> "%RSP%"
echo /c >> "%RSP%"
echo /DASMJIT_STATIC >> "%RSP%"
echo /I "%THIS%\external\asmjit" >> "%RSP%"

for /r "%THIS%\external\asmjit\asmjit" %%f in (*.cpp) do (
    echo "%%f" >> "%RSP%"
)

pushd "%THIS%\build\asmjit_obj"
cl @"%RSP%"
lib /OUT:"..\asmjit.lib" *.obj
popd

del "%RSP%"

echo AsmJit compilado: build\asmjit.lib
