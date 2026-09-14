@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ============================================================
echo SOH-EXTREME 0.7.42 - FULL CUMULATIVE BUILD
echo ============================================================
echo.

echo [0/5] Installing/verifying official Archipelago item model assets...
powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_official_ap_model_assets.ps1"
if errorlevel 1 goto :fail

echo.
echo [1/5] Rebuilding soh.o2r from BB\assets\custom...
cmake --build build-vs --config Release --target GenerateSohOtr --parallel 8
if errorlevel 1 goto :fail

if not exist "build-vs\soh\soh.o2r" (
    echo ERROR: build-vs\soh\soh.o2r was not generated.
    goto :fail
)

echo.
echo [2/5] Copying the FRESH soh.o2r to every runtime location...
copy /Y "build-vs\soh\soh.o2r" ".\soh.o2r" >nul
if errorlevel 1 goto :fail
if exist "build-vs\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\Release\soh.o2r" >nul
if exist "build-vs\soh\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\soh\Release\soh.o2r" >nul

echo.
echo [3/5] Rebuilding Ship of Harkinian...
cmake --build build-vs --config Release --parallel 8
if errorlevel 1 goto :fail

echo.
echo [4/5] Re-copying soh.o2r after the full build so the launched exe cannot use a stale archive...
copy /Y "build-vs\soh\soh.o2r" ".\soh.o2r" >nul
if errorlevel 1 goto :fail
if exist "build-vs\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\Release\soh.o2r" >nul
if exist "build-vs\soh\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\soh\Release\soh.o2r" >nul

echo.
echo [5/5] Done.
echo ============================================================
echo BUILD COMPLETE
 echo AP settings are authoritative for AP saves.
echo Fresh O2R source: build-vs\soh\soh.o2r
echo Fresh O2R copied to: .\soh.o2r and Release folders that exist.
echo APWorld: soh_extreme_0.7.42.apworld
echo ============================================================
copy /Y E:\test\bb\build-vs\soh\soh.o2r E:\test\bb\x64\Release\soh.o2r
exit /b 0

:fail
echo.
echo ============================================================
echo BUILD FAILED - scroll up to the FIRST error.
echo ============================================================
exit /b 1
