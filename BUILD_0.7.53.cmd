@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ============================================================
echo SOH-EXTREME 0.7.53 - LOGIC + TRACKER + DEATH-SAFE QUEUE FIX
echo ============================================================
echo.

if not exist "CMakeLists.txt" (
    echo ERROR: This script is not in the SOH-EXTREME source root.
    echo Extract the CONTENTS of this patch directly over E:\test\bb
    echo so this file sits next to CMakeLists.txt and the build-vs folder.
    exit /b 1
)
if not exist "build-vs" (
    echo ERROR: build-vs was not found next to this script.
    exit /b 1
)

echo [0/6] Installing/verifying official Archipelago item model assets...
powershell -NoProfile -ExecutionPolicy Bypass -File ".\install_official_ap_model_assets.ps1"
if errorlevel 1 goto :fail

echo.
echo [1/6] Rebuilding soh.o2r from top-level assets\custom...
cmake --build build-vs --config Release --target GenerateSohOtr --parallel 8
if errorlevel 1 goto :fail

if not exist "build-vs\soh\soh.o2r" (
    echo ERROR: build-vs\soh\soh.o2r was not generated.
    goto :fail
)

echo.
echo [2/6] Building the Release executable...
cmake --build build-vs --config Release --target soh --parallel 8
if errorlevel 1 goto :fail

echo.
echo [3/6] Copying the fresh soh.o2r to runtime locations...
copy /Y "build-vs\soh\soh.o2r" ".\soh.o2r" >nul
if errorlevel 1 goto :fail
if exist "build-vs\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\Release\soh.o2r" >nul
if exist "build-vs\soh\Release" copy /Y "build-vs\soh\soh.o2r" "build-vs\soh\Release\soh.o2r" >nul

echo.
echo [4/6] Copying the fresh executable to the source-root runtime...
set "FRESH_EXE="
if exist "build-vs\Release\soh.exe" set "FRESH_EXE=build-vs\Release\soh.exe"
if exist "build-vs\soh\Release\soh.exe" set "FRESH_EXE=build-vs\soh\Release\soh.exe"
if not defined FRESH_EXE (
    echo ERROR: Could not find the newly built soh.exe.
    echo Checked build-vs\Release\soh.exe and build-vs\soh\Release\soh.exe.
    goto :fail
)
copy /Y "%FRESH_EXE%" ".\soh.exe" >nul
if errorlevel 1 goto :fail

rem APCpp.dll is normally copied beside the target by CMake. Mirror it to the
rem source-root runtime too when present, so root soh.exe cannot accidentally use
rem an older client DLL.
if exist "build-vs\Release\APCpp.dll" copy /Y "build-vs\Release\APCpp.dll" ".\APCpp.dll" >nul
if exist "build-vs\soh\Release\APCpp.dll" copy /Y "build-vs\soh\Release\APCpp.dll" ".\APCpp.dll" >nul

echo.
echo [5/6] Runtime verification...
for %%F in ("%FRESH_EXE%") do echo Fresh EXE: %%~fF  %%~zF bytes
for %%F in (".\soh.exe") do echo Root  EXE: %%~fF  %%~zF bytes
for %%F in (".\soh.o2r") do echo Root O2R: %%~fF  %%~zF bytes

echo.
echo [6/6] Done.
echo ============================================================
echo BUILD COMPLETE
 echo IMPORTANT: launch E:\test\bb\soh.exe after this script finishes.
echo APWorld: soh_extreme_0.7.53.apworld
echo 0.7.53 keeps AP items queued until SoH actually grants them and aligns AP/native logic.
 echo applying AP settings, exactly like normal SoH randomizer loading.
echo ============================================================
copy /Y E:\test\bb\build-vs\soh\soh.o2r E:\test\bb\x64\Release\soh.o2r
exit /b 0

:fail
echo.
echo ============================================================
echo BUILD FAILED - scroll up to the FIRST error.
echo ============================================================
exit /b 1
