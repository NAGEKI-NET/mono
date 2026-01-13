@echo off
REM ===================================================
REM Build script for minimal mono.dll
REM Run from Visual Studio Developer Command Prompt
REM ===================================================

echo Building minimal mono.dll...

REM Compile to object file
cl.exe /c /O2 /W3 /MD /D_USRDLL /D_WINDLL mono_minimal.c /Fo:mono_minimal.obj
if %ERRORLEVEL% neq 0 (
    echo Compilation failed!
    exit /b 1
)

REM Link to DLL
link.exe /DLL /OUT:mono.dll /DEF:mono.def mono_minimal.obj kernel32.lib user32.lib
if %ERRORLEVEL% neq 0 (
    echo Linking failed!
    exit /b 1
)

REM Show file size
echo.
echo Build successful!
for %%I in (mono.dll) do echo mono.dll size: %%~zI bytes

REM Cleanup
del mono_minimal.obj 2>nul
del mono.exp 2>nul
del mono.lib 2>nul

echo.
echo Done! mono.dll is ready.
