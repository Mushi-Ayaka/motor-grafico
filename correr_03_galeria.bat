@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo Abriendo escena 03 - Galeria (854x480, 19 nodos)...
"%REPO%build\visor.exe" "%REPO%escenas\03_galeria.ont" --stats 2>> "%REPO%stats\visor_stderr.log"
echo.
echo Estadisticas guardadas en stats\ (runs.csv + shot_*.bmp)
pause
