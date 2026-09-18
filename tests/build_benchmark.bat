@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

setlocal enabledelayedexpansion

set ROOT=%~dp0..
set BUILD=%ROOT%\build
set SRC=%~dp0benchmark_p0.cpp
set OUT=%BUILD%\benchmark_p0.exe

if not exist "%BUILD%" mkdir "%BUILD%"

set INCLUDES=/I"%ROOT%" /I"%ROOT%\deps\lenguaje-hermetico" /I"%ROOT%\deps\lenguaje-hermetico\contrato"
set LIBS=^
    "%BUILD%\obj\os\*.obj" ^
    "%BUILD%\obj\scene\*.obj" ^
    "%BUILD%\obj\render\*.obj" ^
    "%BUILD%\obj\core\*.obj" ^
    "%BUILD%\obj\deps\*.obj" ^
    kernel32.lib user32.lib gdi32.lib comdlg32.lib opengl32.lib glu32.lib ^
    vulkan-1.lib d3d11.lib d3dcompiler.lib

echo Compiling benchmark_p0.cpp ...
cl /EHsc /std:c++17 /utf-8 /O2 /MD %INCLUDES% /c "%SRC%" /Fo"%BUILD%\benchmark_p0.obj"

echo Linking ...
link /OUT:"%OUT%" "%BUILD%\benchmark_p0.obj" %LIBS%

if exist "%OUT%" (
    echo.
    echo Build OK: "%OUT%"
    echo Usage: "%OUT%" ^<scene.herm^>
) else (
    echo Build FAILED
)

endlocal
