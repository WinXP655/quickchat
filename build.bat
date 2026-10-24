@echo off
setlocal enabledelayedexpansion

:: Config: Flags
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
	echo.
	echo build.bat [options]
	echo.
	echo Options:
	echo   /help       Show this help.
	echo   /clean      Delete build artifacts and runtime files.
	echo   /rekey      Regenerate XOR key ^(key.h^).
	echo   /pack       Create compressed package.
	echo   /nocleankey Keep key.h while cleaning ^(only with /clean^).
	echo   /minbuild   Minimal build with only core resources.
	goto :end
)

echo Build parameters: Clean=%CLEAN% MinBuild=%MINBUILD% ReKey=%REKEY% Pack=%PACK% NoCleanKey=%NOCLEANKEY% Debug=%DEBUG%

:: Config: Tools
set CC=gcc
set WINDRES=windres
set WINDRES_TARGET=--target=pe-i386 -c 65001

:: Config: Shared flags
set ARCH_FLAGS=-m32 -municode
set COMMON_LIBS=-lcomctl32 -lgdi32 -lshell32 -lwinmm -lws2_32
set COMMON_WARN=-Wall -Wextra

:: Config: Release flags
set CFLAGS_FULL=-Os -s -ffunction-sections -fdata-sections -fno-strict-aliasing -mwindows -Wl,--gc-sections

:: Config: Cleanup
set BUILD_ARTIFACTS=about.o compinfo.o hostinfo.o resources.o servconn.o start.o quickchat.exe quickchat_minbuild.exe QuickChat.zip
set RUNTIME_ARTIFACTS=chatlog.txt quickchat.ini Chat-*.txt

:: ======= 1. Cleanup =======
if %CLEAN%==1 (
	echo Cleaning build artifacts...
	for %%F in (%BUILD_ARTIFACTS%) do (
		if exist "%%F" del /f /q "%%F" >nul 2>&1
	)

	echo Cleaning runtime artifacts...
	for %%F in (%RUNTIME_ARTIFACTS%) do (
		if exist "%%F" del /f /q "%%F" >nul 2>&1
	)

	del /f /q Chat-*.txt >nul 2>&1

	echo Cleaning key... ^(Skipped with /nocleankey^)
	if %NOCLEANKEY%==0 if exist key.h del /f /q key.h >nul 2>&1
	echo [OK] Done cleaning
	goto :end
)

:: ======= 2. ReKey =======
if %REKEY%==1 (
	echo Rekeying: removing old key.h...
	if exist key.h del key.h
)

:: ======= 3. MinBuild =======
if %MINBUILD%==1 (
	echo Build type: MinBuild
	echo.

	if not exist key.h (
		python keygen.py 32
		if errorlevel 1 goto :build_failed
		if not exist key.h goto :build_failed
	)

	if not exist start.o %WINDRES% %WINDRES_TARGET% start.rc -o start.o || goto :build_failed
	if not exist servconn.o %WINDRES% %WINDRES_TARGET% servconn.rc -o servconn.o || goto :build_failed
	if not exist hostinfo.o %WINDRES% %WINDRES_TARGET% hostinfo.rc -o hostinfo.o || goto :build_failed

	%CC% quickchat.c servconn.o start.o hostinfo.o -o quickchat_minbuild.exe %ARCH_FLAGS% %COMMON_LIBS% %COMMON_WARN% || goto :build_failed
	echo [OK] Minimal build successful: quickchat_minbuild.exe
	goto :end
)

:: ======= 4. Release =======
echo Build type: Release
echo.

for %%F in (%BUILD_ARTIFACTS%) do (
	if exist "%%F" set PREVBLD=1
)
if !PREVBLD!==1 echo [WARN] Previous build files detected. Run "build.bat /clean" first.

if not exist key.h (
	echo Generating key.h...
	python keygen.py 32
	if %errorlevel% neq 0 (
		echo [ERROR] Key generation failed
		exit /b 1
	)
	echo [OK] Key generated
) else (
	echo [OK] key.h exists, skipping generation ^(use build /rekey to generate new^)
)

echo Building dialogs...
%WINDRES% %WINDRES_TARGET% start.rc -o start.o || goto :build_failed
%WINDRES% %WINDRES_TARGET% servconn.rc -o servconn.o || goto :build_failed
%WINDRES% %WINDRES_TARGET% hostinfo.rc -o hostinfo.o || goto :build_failed
%WINDRES% %WINDRES_TARGET% about.rc -o about.o || goto :build_failed
%WINDRES% %WINDRES_TARGET% compinfo.rc -o compinfo.o || goto :build_failed
%WINDRES% %WINDRES_TARGET% resources.rc -o resources.o || goto :build_failed
echo [OK] Dialogs compiled

echo Building resources...
%WINDRES% %WINDRES_TARGET% resources.rc -o resources.o || goto :build_failed
echo [OK] Resources compiled

echo Compiling QuickChat...
%CC% quickchat.c servconn.o resources.o about.o start.o compinfo.o hostinfo.o -o quickchat.exe %ARCH_FLAGS% %COMMON_LIBS% %COMMON_WARN% %CFLAGS_FULL% || goto :build_failed
if %errorlevel% neq 0 (
	echo [ERROR] Build failed
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
	powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 .\QuickChat.exe"
)

:end
endlocal
exit /b 0

:build_failed
echo [ERROR] Build failed
endlocal
exit /b 1