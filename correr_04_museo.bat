@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo Abriendo escena 04 - Museo (1024x576, 28 nodos)...
"%REPO%build\visor.exe" "%REPO%escenas\04_museo.ont" --stats 2>> "%REPO%stats\visor_stderr.log"
echo.
echo Estadisticas guardadas en stats\ (runs.csv + shot_*.bmp)
pause
