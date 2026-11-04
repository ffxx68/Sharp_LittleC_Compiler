@echo off
setlocal

cd /d "%~dp0"
if "%LAZARUSDIR%"=="" set "LAZARUSDIR=C:\Users\F.Fumi\lazarus"
set "FPC=%LAZARUSDIR%\fpc\3.2.2\bin\i386-win32\fpc.exe"
if not exist "%FPC%" (
    echo [ERROR] FPC not found at "%FPC%"
    exit /b 9009
)

set "TEST_DIR=%TEMP%\lcc_backend_tests_%RANDOM%_%RANDOM%"
mkdir "%TEST_DIR%"
if errorlevel 1 exit /b 1

"%FPC%" -B -Pi386 -Mdelphi -Sh -gl -Fccp1252 "-FU%TEST_DIR%" "-FE%TEST_DIR%" backend_tests.pas
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto :FAILED

"%TEST_DIR%\backend_tests.exe" "%TEST_DIR%\runtime_helpers.asm"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto :FAILED

"..\..\pasm.exe" "%TEST_DIR%\runtime_helpers.asm" "%TEST_DIR%\runtime_helpers.bin"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto :FAILED

rmdir /s /q "%TEST_DIR%"
exit /b 0

:FAILED
rmdir /s /q "%TEST_DIR%"
exit /b %RESULT%
