@echo off
chcp 1252 >nul
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

REM Contatori per riepilogo
set "TOTAL_TESTS=0"
set "PASSED_TESTS=0"
set "FAILED_TESTS=0"
set "TEST_RESULTS="

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

REM Uso -Mdelphi -Sh e forzo la codepage CP1252
"%FPC%" -B -Pi386 -Mdelphi -Sh -gl -Fccp1252 lcc.dpr > baseline_build.log 2>&1
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

call :RUN_TEST "16bitdiv" "16bitdiv"
call :RUN_TEST "Array" "Array"
call :RUN_TEST "bounce" "bounce"
call :RUN_TEST "Loop" "Loop"
call :RUN_TEST "Math" "Math"

:ALL_DONE
echo.
echo ==========================================
echo ALL TESTS COMPLETED
echo ==========================================
echo.
echo SUMMARY:
echo --------
echo Total tests:      %TOTAL_TESTS%
echo Passed (NO DIFF): %PASSED_TESTS%
echo Failed (DIFF):    %FAILED_TESTS%
echo.
echo DETAILED RESULTS:
echo -----------------
%TEST_RESULTS%
echo ==========================================
if %FAILED_TESTS% gtr 0 (
    echo.
    echo [WARNING] %FAILED_TESTS% test(s) failed - see details above
    echo.
)
exit /b 0


:RUN_TEST
set "SRC_NAME=%~1"
set "DEST_NAME=%~2"
set "CURRENT_TEST_DIR=%TEST_BASE_FULL%\%DEST_NAME%"

set /a "TOTAL_TESTS+=1"

echo.
echo ------------------------------------------
echo TEST: %SRC_NAME%
echo ------------------------------------------

mkdir "%CURRENT_TEST_DIR%"

REM Copia sorgenti
copy "%DEMOS_DIR%\%SRC_NAME%\main.c" "%CURRENT_TEST_DIR%\main.c" >nul
if exist "%DEMOS_DIR%\%SRC_NAME%\*.h" copy "%DEMOS_DIR%\%SRC_NAME%\*.h" "%CURRENT_TEST_DIR%\" >nul

REM Preprocessing
pushd "%ROOT_DIR%"
"%LCPP%" "%CURRENT_TEST_DIR%\main.c" "%CURRENT_TEST_DIR%\preprocessed.c"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCPP failed
    set /a "FAILED_TESTS+=1"
    set "TEST_RESULTS=!TEST_RESULTS!echo   [FAIL] %SRC_NAME%: LCPP preprocessing error&"
    popd
    exit /b 1
)
popd

REM Compilazione REFERENCE (Originale)
"%LCC_ORIG%" "%CURRENT_TEST_DIR%\preprocessed.c" "%CURRENT_TEST_DIR%\reference.asm"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCC Original failed
    set /a "FAILED_TESTS+=1"
    set "TEST_RESULTS=!TEST_RESULTS!echo   [FAIL] %SRC_NAME%: LCC Original compilation error&"
    exit /b 1
)

REM Compilazione NEW (Rifattorizzato)
"%LCC_NEW%" "%CURRENT_TEST_DIR%\preprocessed.c" "%CURRENT_TEST_DIR%\new.asm"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] LCC New failed
    set /a "FAILED_TESTS+=1"
    set "TEST_RESULTS=!TEST_RESULTS!echo   [FAIL] %SRC_NAME%: LCC New compilation error&"
    exit /b 1
)

REM Confronto
fc /W "%CURRENT_TEST_DIR%\reference.asm" "%CURRENT_TEST_DIR%\new.asm" > "%CURRENT_TEST_DIR%\diff.txt"
if %ERRORLEVEL% equ 0 (
    echo [RESULT] OK - IDENTICAL
    set /a "PASSED_TESTS+=1"
    set "TEST_RESULTS=!TEST_RESULTS!echo   [PASS] %SRC_NAME%: NO DIFF&"
) else (
    echo [RESULT] DIFF - Differences found
    echo          See %CURRENT_TEST_DIR%\diff.txt
    set /a "FAILED_TESTS+=1"
    set "TEST_RESULTS=!TEST_RESULTS!echo   [FAIL] %SRC_NAME%: DIFF found&"
)

exit /b 0

