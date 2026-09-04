@echo off
setlocal
:: ===========================================================================
:: build_libherm.bat - Compila lenguaje-hermetico como libreria estatica
:: (solo el camino parse -> RIH; excluye herm_render.cpp que tira de stb/PNG
::  y main.cpp que es el CLI).
:: Produce build/libherm.lib  -> se linkea en el editor para live-compile.
:: ===========================================================================
set VS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set HERM=deps\lenguaje-hermetico\herm
set OUT=build
if not exist "%OUT%" mkdir "%OUT%"

call %VS% 2>nul
if errorlevel 1 (
    echo No se encontro Visual Studio 2022 BuildTools
    exit /b 1
)

cl /EHsc /std:c++17 /O2 /MD /c /Fo"%OUT%/" ^
   "%HERM%\herm_lexer.cpp" ^
   "%HERM%\herm_parser.cpp" ^
   "%HERM%\herm_resolver.cpp" ^
   "%HERM%\herm_rih.cpp" ^
   "%HERM%\herm_eval.cpp" ^
   "%HERM%\herm_compile.cpp"
if errorlevel 1 (
    echo COMPILE FAILED
    exit /b 1
)

lib /OUT:"%OUT%\libherm.lib" ^
   "%OUT%\herm_lexer.obj" ^
   "%OUT%\herm_parser.obj" ^
   "%OUT%\herm_resolver.obj" ^
   "%OUT%\herm_rih.obj" ^
   "%OUT%\herm_eval.obj" ^
   "%OUT%\herm_compile.obj"
if errorlevel 1 (
    echo LIB FAILED
    exit /b 1
)

echo.
echo Build OK: %OUT%\libherm.lib
