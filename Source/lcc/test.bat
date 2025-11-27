@echo off

REM Test script for lcpp and lcc in Source/lcc
cd /d %~dp0

REM Build the compiler directly with FPC (no build.bat)
if "%LAZARUSDIR%"=="" set "LAZARUSDIR=C:\Users\F.Fumi\lazarus"
set "FPC=%LAZARUSDIR%\fpc\3.2.2\bin\i386-win32\fpc.exe"
echo Building lcc with FPC: %FPC%
if not exist "%FPC%" (
    echo FPC not found at %FPC%
    exit /b 9009
)
"%FPC%" lcc.dpr > baseline_build.log 2>&1
if %ERRORLEVEL% neq 0 (
    echo build failed
    type baseline_build.log
    exit /b %ERRORLEVEL%
)

REM Preprocess the input file (test_C/bounce/main.c)
cd ..\..
.\lcpp.exe .\Source\lcc\test_C\bounce\main.c .\Source\lcc\test_C\bounce\tmp.c
if %errorlevel% neq 0 (
    echo lcpp failed
    exit /b 1
)

REM Compile the preprocessed file with lcc.exe locale
cd Source\lcc
lcc.exe .\test_C\bounce\tmp.c .\test_C\bounce\tmp.asm
if %errorlevel% neq 0 (
    echo lcc failed
    exit /b 1
)

echo Test completed. Output: tmp.asm

REM Compare produced asm with reference
cmd /c fc ".\test_C\bounce\tmp.asm" ".\reference_bounce.asm" >nul
if %errorlevel% equ 0 (
    echo NO_DIFF
) else (
    echo DIFF_%errorlevel%
    exit /b 2
)
