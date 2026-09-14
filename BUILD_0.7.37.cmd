@echo off
setlocal
cd /d "%~dp0"
echo ============================================================
echo SOH-EXTREME 0.7.37 - AP model install + rebuild
echo ============================================================
echo.
powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_official_ap_model_assets.ps1"
if errorlevel 1 goto :fail

echo.
echo [1/2] Rebuilding soh.o2r from BB\assets\custom...
cmake --build build-vs --config Release --target GenerateSohOtr --parallel 8
if errorlevel 1 goto :fail

echo.
echo [2/2] Rebuilding Ship of Harkinian...
cmake --build build-vs --config Release --parallel 8
if errorlevel 1 goto :fail

echo.
echo ============================================================
echo BUILD COMPLETE
 echo Test the same remote shop item again.
echo ============================================================
copy /Y E:\test\bb\build-vs\soh\soh.o2r E:\test\bb\x64\Release\soh.o2r
exit /b 0

:fail
echo.
echo ============================================================
echo BUILD FAILED - scroll up to the first error.
echo ============================================================
exit /b 1
