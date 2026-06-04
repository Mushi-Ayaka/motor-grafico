@echo off
setlocal
chcp 65001 >nul
echo === Debug Motor Grafico ===
echo.
echo Verificando ruta desde %~dp0..
echo.
if exist "%~dp0..\Lenguaje Hermético\ejemplos\bodegon.rih" (
    echo [OK] RIH encontrado
) else (
    echo [MISSING] RIH no encontrado
    dir "%~dp0..\Lenguaje Hermético\ejemplos\" 2>nul || echo No se puede listar ejemplos
)
echo.
echo Ejecutando visor...
start "" /wait "%~dp0build\visor.exe" "%~dp0..\Lenguaje Hermético\ejemplos\bodegon.rih"
echo.
echo Visor termino con codigo: %ERRORLEVEL%
pause
