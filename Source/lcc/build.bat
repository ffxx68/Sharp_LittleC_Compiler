@echo off
REM Build lcc.exe in locale (Source/lcc)
"C:\Users\F.Fumi\lazarus\fpc\3.2.2\bin\i386-win32\fpc.exe" lcc.dpr
if %errorlevel% neq 0 (
    echo Build lcc.exe failed
    exit /b 1
)
