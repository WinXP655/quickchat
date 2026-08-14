@echo off
setlocal

set REKEY=0
set PACK=0
set MINBUILD=0
set CLEAN=0
set HELP=0

:parse_args
if "%1"=="" goto :done_parse
if /i "%1"=="/rekey" set REKEY=1
if /i "%1"=="/pack" set PACK=1
if /i "%1"=="/minbuild" set MINBUILD=1
if /i "%1"=="/clean" set CLEAN=1
if /i "%1"=="/help" set help=1
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
    echo /help     - Show this help.
    echo /clean    - Delete existing compiled files.
    echo /minbuild - Compile QuickChat with minimal configuration.
    echo /rekey    - Regenerate XOR key.
    echo /pack     - Pack compiled binary into ZIP archive.
    goto end
)

echo ==== Build parameters =====
echo Clean: %CLEAN%
echo MinBuild: %MINBUILD%
echo ReKey: %REKEY%
echo Pack: %PACK%
echo ===========================
echo.

if %CLEAN%==1 (
    echo Deleting previous build files...
    if exist quickchat.exe del quickchat.exe
    if exist quickchat.zip del quickchat.zip
    if exist resource.o del resource.o
    if exist servconn.o del servconn.o
    if exist chatlog.txt del chatlog.txt
    if exist key.h del key.h
    echo [OK] Done cleaning
    goto end
)

if %MINBUILD%==1 (
    if not exist minbuildconn.o (
        echo [ERROR] minbuildconn.o is not found.
    	goto end
    )
    echo Generating key...
    if not exist key.h python keygen.py
    echo Compiling QuickChat...
    gcc quickchat.c minbuildconn.o -o quickchat.exe -m32 -lgdi32 -lwinmm -lws2_32 -lcomctl32 -lshell32 -Wall -Wextra -municode
    echo Done.
    goto end
)

if exist .\quickchat.exe set PREVBLD=1
if exist .\QuickChat.zip set PREVBLD=1
if exist .\key.h set PREVBLD=1
if exist .\servconn.o set PREVBLD=1
if exist .\resource.o set PREVBLD=1
if exist .\chatlog.txt set PREVBLD=1

if "%PREVBLD%"=="1" (
    echo.
    echo Warning! One or several previous build files have been detected.
    echo It is recommended to run "build.bat /clean" before building.
    echo.
)

if %REKEY%==1 (
    echo Rekeying: removing old key.h...
    if exist key.h del key.h
)

if not exist key.h (
    echo Generating key.h...
    python keygen.py
    if %errorlevel% neq 0 (
        echo [ERROR] Key generation FAILED
        exit /b 1
    )
    echo [OK] Key generated
) else (
    echo [OK] key.h exists, skipping generation (use build /rekey to generate new)
)

echo Building dialog...
windres --target=pe-i386 servconn.rc -o servconn.o
if %errorlevel% neq 0 (
    echo [ERROR] Dialog FAILED
    exit /b 1
)
echo [OK] Dialog built

echo Building resources...
windres --target=pe-i386 resource.rc -o resource.o
if %errorlevel% neq 0 (
    echo [ERROR] Resources FAILED
    exit /b 1
)
echo [OK] Resources built

echo Compiling QuickChat...
gcc quickchat.c servconn.o resource.o -o quickchat.exe -m32 -lgdi32 -lwinmm -lws2_32 -lcomctl32 -lshell32 -mwindows -s -Wl,--gc-sections -Wl,--subsystem,windows:5.0 -Wall -Wextra -municode
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
    echo [INFO] Skipping pack (use build /pack to create archive)
)

:end
endlocal