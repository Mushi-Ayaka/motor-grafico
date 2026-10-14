@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo Abriendo escena 05 - Catedral (1280x720, alta calidad)...
"%REPO%build\visor.exe" "%REPO%escenas\05_catedral.ont" --stats 2>> "%REPO%stats\visor_stderr.log"
echo.
echo Estadisticas guardadas en stats\ (runs.csv + shot_*.bmp)
pause
