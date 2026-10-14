@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo Abriendo escena 01 - Bodega (baja calidad, 480x270)...
"%REPO%build\visor.exe" "%REPO%escenas\01_bodega_baja.ont" --stats 2>> "%REPO%stats\visor_stderr.log"
echo.
echo Estadisticas guardadas en stats\ (runs.csv + shot_*.bmp)
pause
