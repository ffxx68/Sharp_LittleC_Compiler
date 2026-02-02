@echo off
setlocal EnableDelayedExpansion

REM Script per testare lcc rifattorizzato contro lcc originale (regression testing)
REM Posizione: Source\lcc\test.bat

cd /d %~dp0

REM --- CONFIGURAZIONE ---
set "ROOT_DIR=..\.."
set "LCC_ORIG=%cd%\%ROOT_DIR%\lcc.exe"
set "LCPP=%cd%\%ROOT_DIR%\lcpp.exe"
set "LCC_NEW=%cd%\lcc.exe"
set "TEST_BASE_DIR=test_C"
set "DEMOS_DIR=%cd%\%ROOT_DIR%\Demos"

REM Assicuro path assoluti per evitare problemi con pushd/popd
for %%i in ("%LCC_ORIG%") do set "LCC_ORIG=%%~fi"
for %%i in ("%LCPP%") do set "LCPP=%%~fi"
for %%i in ("%LCC_NEW%") do set "LCC_NEW=%%~fi"
for %%i in ("%DEMOS_DIR%") do set "DEMOS_DIR=%%~fi"
set "TEST_BASE_FULL=%cd%\%TEST_BASE_DIR%"

echo [SETUP] Root dir: %ROOT_DIR%
echo [SETUP] LCC Original: %LCC_ORIG%
echo [SETUP] LCC New: %LCC_NEW%

REM --- BUILD NUOVO LCC ---
if "%LAZARUSDIR%"=="" set "LAZARUSDIR=C:\Users\F.Fumi\lazarus"
set "FPC=%LAZARUSDIR%\fpc\3.2.2\bin\i386-win32\fpc.exe"

echo.
echo [BUILD] Building new lcc in %cd%...
if not exist "%FPC%" (
    echo [ERROR] FPC not found at %FPC%
    exit /b 9009
)

"%FPC%" -B -Pi386 -Mdelphi -gl lcc.dpr > baseline_build.log 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed. See baseline_build.log
    exit /b %ERRORLEVEL%
)
echo [BUILD] Success.

REM --- PREPARAZIONE AMBIENTE TEST ---
if exist "%TEST_BASE_DIR%" (
    echo [CLEAN] Removing old test dir...
    rmdir /s /q "%TEST_BASE_DIR%"
)
mkdir "%TEST_BASE_DIR%"

REM --- ESECUZIONE TEST ---
if not "%~1"=="" (
    echo [INFO] Running single test mode for: "%~1"
    call :RUN_TEST "%~1" "%~1"
    goto :ALL_DONE
)

REM Elenco delle demo da testare.
REM Sintassi: call :RUN_TEST "NomeCartellaInDemos" "NomeCartellaDestinazione"

call :RUN_TEST "16bitdiv" "16bitdiv"
call :RUN_TEST "Array" "Array"
call :RUN_TEST "bounce" "bounce"
call :RUN_TEST "Loop demo" "Loop_demo"
call :RUN_TEST "Math demo" "Math_demo"

:ALL_DONE
echo.
echo ==========================================
echo ALL TESTS COMPLETED
echo ==========================================
exit /b 0


:RUN_TEST
set "SRC_NAME=%~1"
set "DEST_NAME=%~2"
set "CURRENT_TEST_DIR=%TEST_BASE_FULL%\%DEST_NAME%"

echo.
echo ------------------------------------------
echo TEST: %SRC_NAME%
echo ------------------------------------------

mkdir "%CURRENT_TEST_DIR%"

REM Copia sorgenti
copy "%DEMOS_DIR%\%SRC_NAME%\main.c" "%CURRENT_TEST_DIR%\main.c" >nul
if exist "%DEMOS_DIR%\%SRC_NAME%\*.h" copy "%DEMOS_DIR%\%SRC_NAME%\*.h" "%CURRENT_TEST_DIR%\" >nul

REM Preprocessing (eseguito dalla ROOT per trovare gli include standard)
pushd "%ROOT_DIR%"
REM Note: LCPP output path must be absolute or relative to root
"%LCPP%" "%CURRENT_TEST_DIR%\main.c" "%CURRENT_TEST_DIR%\preprocessed.c"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCPP failed
    popd
    exit /b 1
)
popd

REM Compilazione REFERENCE (Originale)
"%LCC_ORIG%" "%CURRENT_TEST_DIR%\preprocessed.c" "%CURRENT_TEST_DIR%\reference.asm"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCC Original failed
    exit /b 1
)

REM Compilazione NEW (Rifattorizzato)
"%LCC_NEW%" "%CURRENT_TEST_DIR%\preprocessed.c" "%CURRENT_TEST_DIR%\new.asm"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCC New failed
    exit /b 1
)

REM Confronto
fc /W "%CURRENT_TEST_DIR%\reference.asm" "%CURRENT_TEST_DIR%\new.asm" > "%CURRENT_TEST_DIR%\diff.txt"
if %ERRORLEVEL% equ 0 (
    echo [RESULT] OK - IDENTICAL
) else (
    echo [RESULT] DIFF - Differences found
    echo          See %CURRENT_TEST_DIR%\diff.txt
)

exit /b 0
