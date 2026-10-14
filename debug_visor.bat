@echo off
setlocal
chcp 65001 >nul
echo === Debug Motor Grafico ===
echo.
echo Usando visor desde %~dp0..
echo.
echo Ejecutando visor con escena .ont...
start "" /wait "%~dp0build\visor.exe" "%~dp0..\..\proyectos-index\proyecto de IA artistica\Lenguaje Hermetico\.gitignored\libreria\escenas\catedral_hermetica_0000.ont"
echo.
echo Visor termino con codigo: %ERRORLEVEL%
pause
