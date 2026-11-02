@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
if not defined VSCMD_VER (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
    if not defined VSROOT (
        echo Visual Studio C++ build tools were not found.
        exit /b 1
    )
    call "!VSROOT!\VC\Auxiliary\Build\vcvars64.bat" >nul
    if errorlevel 1 exit /b 1
)
if not exist obj mkdir obj
cl /nologo /utf-8 /std:c11 /W3 /MT /D_CRT_SECURE_NO_WARNINGS /Foobj\ /Fe:basic.exe *.c
exit /b %errorlevel%
