@echo off
setlocal EnableDelayedExpansion

echo.
echo  ============================================
echo        Latite Client Injector
echo        Minecraft Bedrock GDK Edition
echo  ============================================
echo.

set "DLL_PATH=%~1"
if "%DLL_PATH%"=="" (
    for %%F in ("%~dp0*.dll") do ( set "DLL_PATH=%%F" & goto :found )
    for /r "%~dp0cmake-build-debug" %%F in (LatiteDebug.dll) do ( set "DLL_PATH=%%F" & goto :found )
    for /r "%~dp0cmake-build-release" %%F in (Latite.dll) do ( set "DLL_PATH=%%F" & goto :found )
    echo  [ERROR] Tidak ada DLL ditemukan.
    echo  Usage: auto-inject.bat path\ke\Latite.dll
    pause & exit /b 1
)
:found
for %%F in ("%DLL_PATH%") do set "DLL_PATH=%%~fF"
if not exist "%DLL_PATH%" ( echo  [ERROR] File tidak ada: %DLL_PATH% & pause & exit /b 1 )
echo  DLL : %DLL_PATH%

set "PS1_PATH=%~dp0inject.ps1"
if not exist "%PS1_PATH%" ( echo  [ERROR] inject.ps1 tidak ada & pause & exit /b 1 )

echo  Menunggu Minecraft.Windows.exe...
:wait
tasklist /FI "IMAGENAME eq Minecraft.Windows.exe" 2>nul | find /I "Minecraft.Windows.exe" >nul
if errorlevel 1 ( timeout /t 2 /nobreak >nul & goto :wait )
echo  [OK] Minecraft ditemukan!

echo  Menunggu game load (10 detik)...
timeout /t 10 /nobreak >nul

echo  Menginjeksi...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1_PATH%" -DllPath "%DLL_PATH%"
if %errorlevel% NEQ 0 (
    echo  [GAGAL] Coba klik kanan ^> Run as administrator
    pause & exit /b 1
)
pause
