@echo off
setlocal

set REKEY=0
set PACK=0

:parse_args
if "%1"=="" goto :done_parse
if /i "%1"=="/rekey" set REKEY=1
if /i "%1"=="/pack" set PACK=1
shift
goto :parse_args
:done_parse

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

if exist quickchat.exe (
    echo Removing old quickchat.exe...
    del quickchat.exe
)
if exist QuickChat.zip (
    echo Removing old QuickChat.zip...
    del QuickChat.zip
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
gcc quickchat.c servconn.o resource.o -o quickchat.exe -m32 -lgdi32 -lwinmm -lws2_32 -lcomctl32 -mwindows -s -Wl,--gc-sections -Wall -Wextra -municode
if %errorlevel% neq 0 (
    echo [ERROR] Build FAILED
    exit /b 1
)
echo [OK] Build successful: quickchat.exe

if %PACK%==1 (
    echo Packing final package...
    7z a QuickChat.zip "Error Reference.txt"
    7z a QuickChat.zip join.wav > nul
    7z a QuickChat.zip left.wav > nul
    7z a QuickChat.zip newmsg.wav > nul
    7z a QuickChat.zip quickchat.exe > nul
    if %errorlevel% neq 0 (
        echo [ERROR] Packing FAILED
        exit /b 1
    )
    echo [OK] Package created: QuickChat.zip
) else (
    echo [INFO] Skipping pack (use build /pack to create archive)
)

endlocal