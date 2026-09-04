@echo off
setlocal
:: ===========================================================================
:: setup_deps.bat - Instalacion limpia de dependencias del motor grafico
::
:: Prerrequisitos (instalar aparte, no los clona este script):
::   1) Visual Studio 2022 Build Tools + carga de trabajo "C++ desktop"
::   2) Vulkan SDK >= 1.4.350.0  (define VULKAN_SDK si no esta en
::      C:\VulkanSDK\1.4.350.0)
::
:: Este script clona en external/ (libs 3rd-party) y deps/ (repos propios)
:: todo lo que el build necesita. Es idempotente: no re-clona si ya existe.
:: ===========================================================================

set THIS=%~dp0
set THIS=%THIS:~0,-1%

if "%VULKAN_SDK%"=="" set VULKAN_SDK=C:\VulkanSDK\1.4.350.0

call :clone "external\asmjit"                  https://github.com/asmjit/asmjit
call :clone "external\volk"                    https://github.com/zeux/volk
call :clone "external\VulkanMemoryAllocator"   https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
call :clone "external\imgui"                    https://github.com/ocornut/imgui v1.91.9
call :clone "deps\lenguaje-hermetico"          https://github.com/Mushi-Ayaka/lenguaje-hermetico
call :clone "deps\MLIR-CampoTensorial"         https://github.com/Mushi-Ayaka/MLIR-CampoTensorial

echo.
echo Dependencias listas. Ahora ejecuta build_asmjit.bat y build.bat
goto :eof

:: --- funcion: clona solo si la carpeta no existe ----------------------------
:: Uso: call :clone "ruta" "url" [branch]
:clone
set "rel=%~1"
set "url=%~2"
set "br=%~3"
if exist "%THIS%\%rel%\.git" (
    echo [skip] %rel% (ya existe)
    goto :eof
)
if exist "%THIS%\%rel%" (
    echo [skip] %rel% (ya existe, no es git)
    goto :eof
)
echo [clone] %rel% <-- %url%
if "%br%"=="" (
    git clone --depth 1 "%url%" "%THIS%\%rel%"
) else (
    git clone --depth 1 --branch "%br%" "%url%" "%THIS%\%rel%"
)
goto :eof
