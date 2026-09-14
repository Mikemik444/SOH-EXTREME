@echo off
setlocal
cd /d "%~dp0"
echo ============================================================
echo SOH-EXTREME 0.7.39 - AP authoritative settings + rebuild
echo ============================================================
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_official_ap_model_assets.ps1"
if errorlevel 1 goto :fail

echo.
echo [1/3] Rebuilding soh.o2r from BB\assets\custom...
cmake --build build-vs --config Release --target GenerateSohOtr --parallel 8
if errorlevel 1 goto :fail

if not exist "build-vs\soh\soh.o2r" (
    echo ERROR: build-vs\soh\soh.o2r was not generated.
    goto :fail
)

echo.
echo [2/3] Copying fresh soh.o2r next to Release soh.exe...
copy /Y "build-vs\soh\soh.o2r" "build-vs\Release\soh.o2r" >nul
if errorlevel 1 goto :fail

echo.
echo [3/3] Rebuilding Ship of Harkinian...
cmake --build build-vs --config Release --parallel 8
if errorlevel 1 goto :fail

echo.
echo ============================================================
echo BUILD COMPLETE
 echo AP settings are authoritative for AP saves.
echo Fresh O2R: build-vs\Release\soh.o2r
echo ============================================================
copy /Y E:\test\bb\build-vs\soh\soh.o2r E:\test\bb\x64\Release\soh.o2r
exit /b 0

:fail
echo.
echo ============================================================
echo BUILD FAILED - scroll up to the first error.
echo ============================================================
exit /b 1
