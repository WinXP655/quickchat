@echo off
setlocal enabledelayedexpansion

set REKEY=0
set PACK=0
set MINBUILD=0
set CLEAN=0
set HELP=0
set NOCLEANKEY=0

:parse_args
if "%1"=="" goto :done_parse
if /i "%1"=="/rekey" set REKEY=1
if /i "%1"=="/pack" set PACK=1
if /i "%1"=="/minbuild" set MINBUILD=1
if /i "%1"=="/clean" set CLEAN=1
if /i "%1"=="/help" set HELP=1
if /i "%1"=="/nocleankey" set NOCLEANKEY=1
shift
goto :parse_args
:done_parse

if %NOCLEANKEY%==1 if %CLEAN%==0 (
    echo [WARN] /nocleankey is only meaningful with /clean. Ignored.
    set NOCLEANKEY=0
)

if %HELP%==1 (
    echo QuickChat Build Environment
    echo This script is used to build QuickChat with different configurations.
    echo.
    echo Supported arguments:
    echo build.bat [/help] [/clean] [/minbuild] [/rekey] [/pack]
    echo.
    echo /help       - Show this help.
    echo /clean      - Delete existing compiled files.
    echo /minbuild   - Compile QuickChat with minimal configuration.
    echo /rekey      - Regenerate XOR key.
    echo /pack       - Pack compiled binary into ZIP archive.
    echo /nocleankey - Do not delete key file. Can be used only with /clean.
    goto end
)

echo Build parameters: Clean=%CLEAN% MinBuild=%MINBUILD% ReKey=%REKEY% Pack=%PACK% NoCleanKey=%NOCLEANKEY%

set CC=gcc
set WINDRES=windres
set WINDRES_TARGET=--target=pe-i386
set ARCH_FLAGS=-m32 -municode
set COMMON_LIBS=-lcomctl32 -lgdi32 -lshell32 -lwinmm -lws2_32
set COMMON_WARN=-Wall -Wextra
set CFLAGS_FULL=-Os -s -ffunction-sections -fdata-sections -fno-strict-aliasing -mwindows -Wl,--gc-sections
set ARTIFACTS=about.o compinfo.o hostinfo.o resources.o servconn.o start.o chatlog.txt quickchat.ini quickchat.exe QuickChat.zip

if %CLEAN%==1 (
    for %%F in (%ARTIFACTS%) do (
        if exist %%F del %%F
    )
    if %NOCLEANKEY%==0 if exist key.h del key.h
    echo [OK] Done cleaning
    goto end
)

if %MINBUILD%==1 (
    if not exist minbuildconn.o (
        echo [ERROR] minbuildconn.o is not found.
        goto end
    )
    if not exist key.h python keygen.py
    if not exist start.o %WINDRES% %WINDRES_TARGET% start.rc -o start.o || goto :build_failed
    if not exist hostinfo.o %WINDRES% %WINDRES_TARGET% hostinfo.rc -o hostinfo.o || goto :build_failed
    %CC% quickchat.c minbuildconn.o start.o hostinfo.o -o quickchat.exe %ARCH_FLAGS% %COMMON_LIBS% %COMMON_WARN% || goto :build_failed
    echo Done.
    goto end
)

for %%F in (%ARTIFACTS%) do (
    if exist "%%F" set PREVBLD=1
)
if !PREVBLD!==1 echo [WARN] Previous build files detected. Run "build.bat /clean" first.

if %REKEY%==1 (
    echo Rekeying: removing old key.h...
    if exist key.h del key.h
)

if not exist key.h (
    echo Generating key.h...
    python keygen.py 32
    if %errorlevel% neq 0 (
        echo [ERROR] Key generation FAILED
        exit /b 1
    )
    echo [OK] Key generated
) else (
    echo [OK] key.h exists, skipping generation ^(use build /rekey to generate new^)
)

echo Building dialog - Connect to Host...
%WINDRES% %WINDRES_TARGET% -c 65001 servconn.rc -o servconn.o || goto :build_failed

echo Building dialog - Main...
%WINDRES% %WINDRES_TARGET% -c 65001 start.rc -o start.o || goto :build_failed

echo Building dialog - Host Info...
%WINDRES% %WINDRES_TARGET% -c 65001 hostinfo.rc -o hostinfo.o || goto :build_failed

echo Building dialog - About...
%WINDRES% %WINDRES_TARGET% -c 65001 about.rc -o about.o || goto :build_failed

echo Building dialog - Computer Info...
%WINDRES% %WINDRES_TARGET% -c 65001 compinfo.rc -o compinfo.o || goto :build_failed
echo [OK] Dialogs compiled

echo Building resources...
%WINDRES% %WINDRES_TARGET% resources.rc -o resources.o || goto :build_failed
echo [OK] Resources compiled

echo Compiling QuickChat...
%CC% quickchat.c servconn.o resources.o about.o start.o compinfo.o hostinfo.o -o quickchat.exe %ARCH_FLAGS% %COMMON_LIBS% %COMMON_WARN% %CFLAGS_FULL% || goto :build_failed
if %errorlevel% neq 0 (
    echo [ERROR] Build FAILED
    exit /b 1
)
echo [OK] Build successful: quickchat.exe

if %PACK%==1 (
    echo Packing final package...

    if exist QuickChat.zip del /f /q QuickChat.zip >nul 2>&1

    7z a -tzip -y QuickChat.zip readme.txt join.wav leave.wav newmsg.wav quickchat.exe >nul
    if errorlevel 1 goto :build_failed

    echo [OK] Package created: QuickChat.zip
    powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 .\QuickChat.zip"
) else (
    echo [INFO] Skipping pack ^(use build /pack to create archive^)
)

:end
endlocal
exit /b 0

:build_failed
echo [ERROR] Build FAILED
endlocal
exit /b 1