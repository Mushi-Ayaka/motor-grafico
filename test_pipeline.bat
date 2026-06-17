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
cl /nologo /EHsc /std:c++17 /utf-8 /O2 /MD /Fe"%THIS%\build\test_pipeline.exe" /I "%THIS%" "%THIS%\test_pipeline.cpp" "%THIS%\render\scene.cpp" "%THIS%\os\win32\file.cpp" "%THIS%\os\win32\mem.cpp" /link user32.lib
if errorlevel 1 (
    echo Build FAILED
    exit /b 1
)
echo Build OK
"%THIS%\build\test_pipeline.exe"
