@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo Abriendo escena 02 - Sala (calidad media, 640x360)...
"%REPO%build\visor.exe" "%REPO%escenas\02_sala.ont" --stats 2>> "%REPO%stats\visor_stderr.log"
echo.
echo Estadisticas guardadas en stats\ (runs.csv + shot_*.bmp)
pause
