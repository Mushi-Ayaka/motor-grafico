@echo off
setlocal
set REPO=%~dp0
if not exist "%REPO%stats" mkdir "%REPO%stats"
echo ============================================
echo  Benchmark de las 5 escenas (auto, ~10 seg)
echo  Mide: dificultad de render creciente
echo ============================================
for %%S in (01_bodega_baja 02_sala 03_galeria 04_museo 05_catedral) do (
    echo.
    echo --- %%S ---
    "%REPO%build\visor.exe" "%REPO%escenas\%%S.ont" --bench-vulkan --stats 2>> "%REPO%stats\visor_stderr.log"
)
echo.
echo Listo. Resultados en stats\runs.csv (tabla comparativa)
echo y screenshots en stats\shot_*.bmp
pause
