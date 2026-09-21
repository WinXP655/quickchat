@echo off
setlocal

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

set WINDRES=windres --target=pe-i386
set CFLAGS=-m32 -municode -lcomctl32 -lgdi32 -lshell32 -lwinmm -lws2_32 -Wall -Wextra

if %CLEAN%==1 (
    for %%F in (about.o chatlog.txt compinfo.o hostinfo.o quickchat.exe QuickChat.zip resources.o servconn.o start.o quickchat.ini) do (
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
    if not exist start.o %WINDRES% start.rc -o start.o
    if not exist hostinfo.o %WINDRES% hostinfo.rc -o hostinfo.o
    gcc quickchat.c minbuildconn.o start.o hostinfo.o -o quickchat.exe %CFLAGS% || goto :build_failed
    echo Done.
    goto end
)

for %%F in (about.o chatlog.txt compinfo.o hostinfo.o quickchat.exe QuickChat.zip resources.o servconn.o start.o quickchat.ini) do (
    if exist %%F set PREVBLD=1
)

if "%PREVBLD%"=="1" echo Warning! Previous build files detected. Run "build.bat /clean" first.

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
windres --target=pe-i386 -c 65001 servconn.rc -o servconn.o || goto :build_failed

echo Building dialog - Main...
windres --target=pe-i386 -c 65001 start.rc -o start.o || goto :build_failed

echo Building dialog - Host Info...
windres --target=pe-i386 -c 65001 hostinfo.rc -o hostinfo.o || goto :build_failed

echo Building dialog - About...
windres --target=pe-i386 -c 65001 about.rc -o about.o || goto :build_failed

echo Building dialog - Computer Info...
windres --target=pe-i386 -c 65001 compinfo.rc -o compinfo.o || goto :build_failed
echo [OK] Dialogs compiled

echo Building resources...
windres --target=pe-i386 resources.rc -o resources.o || goto :build_failed
echo [OK] Resources compiled

echo Compiling QuickChat...
gcc quickchat.c servconn.o resources.o about.o start.o compinfo.o hostinfo.o -o quickchat.exe -m32 -mwindows  -municode -lcomctl32 -lgdi32 -lshell32 -lwinmm -lws2_32 -Wl,--gc-sections -Wl,--subsystem,windows:6.0 -Os -s -ffunction-sections -fdata-sections -Wall -Wextra
if %errorlevel% neq 0 (
    echo [ERROR] Build FAILED
    exit /b 1
)
echo [OK] Build successful: quickchat.exe

if %PACK%==1 (
    echo Packing final package...
    7z a QuickChat.zip readme.txt > nul
    7z a QuickChat.zip join.wav > nul
    7z a QuickChat.zip leave.wav > nul
    7z a QuickChat.zip newmsg.wav > nul
    7z a QuickChat.zip quickchat.exe > nul
    if %errorlevel% neq 0 (
        echo [ERROR] Packing FAILED
        exit /b 1
    )
    echo [OK] Package created: QuickChat.zip
    powershell Get-FileHash .\QuickChat.zip
) else (
    echo [INFO] Skipping pack ^(use build /pack to create archive^)
)

:end
endlocal
exit /b 0

:build_failed
echo [ERROR] Build FAILED
exit /b 1