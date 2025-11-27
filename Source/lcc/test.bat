@echo off

REM Test script for lcpp and lcc in Source/lcc
cd /d %~dp0

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
