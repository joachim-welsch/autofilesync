@echo off
rem AutoFileSync Build Script (Windows) - counterpart to build.sh.
rem Windows builds from AFS_Desktop\autofilesync.sln, not from CMake: the project
rem file carries the wxWidgets, Boost and vcpkg paths this platform needs.
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "SOLUTION=%ROOT%AFS_Desktop\autofilesync.sln"
set "BUILD_TYPE=Release"
set "CLEAN=0"
rem Assigned outside every parenthesised block: cmd would split the "(x86)" here.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="-d"        (set "BUILD_TYPE=Debug"   & shift & goto parse_args)
if /i "%~1"=="--debug"   (set "BUILD_TYPE=Debug"   & shift & goto parse_args)
if /i "%~1"=="-r"        (set "BUILD_TYPE=Release" & shift & goto parse_args)
if /i "%~1"=="--release" (set "BUILD_TYPE=Release" & shift & goto parse_args)
if /i "%~1"=="clean"     (set "CLEAN=1"            & shift & goto parse_args)
if /i "%~1"=="-h"        goto usage
if /i "%~1"=="--help"    goto usage
echo Unknown option: %~1
echo Use -h or --help for usage information
exit /b 1

:usage
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo   -d, --debug     Build Debug version (default: Release)
echo   -r, --release   Build Release version
echo   clean           Clean build directory before building
echo   -h, --help      Show this help message
echo.
echo Examples:
echo   %~nx0              # Build Release
echo   %~nx0 -d           # Build Debug
echo   %~nx0 clean -d     # Clean and build Debug
exit /b 0

:args_done
set "BUILD_DIR=%ROOT%AFS_Desktop\x64\%BUILD_TYPE%"
set "EXECUTABLE=%BUILD_DIR%\autofilesync.exe"
echo === AutoFileSync %BUILD_TYPE% Build ===

if not exist "%SOLUTION%" (
    echo Solution not found: %SOLUTION%
    exit /b 1
)

rem -prerelease, because vswhere hides an Insiders or Preview installation
rem otherwise, and the 64-bit MSBuild first, because the 32-bit one runs out of
rem address space while compiling several translation units in parallel.
set "MSBUILD="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do set "MSBUILD=%%i"
    if not defined MSBUILD (
        for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
    )
)
if not defined MSBUILD (
    for /f "delims=" %%i in ('where msbuild 2^>nul') do if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    echo MSBuild not found - Visual Studio with the C++ workload is required.
    exit /b 1
)

if "%CLEAN%"=="1" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
    echo Build directory cleaned.
)

rem The post-build step of the project copies help\ next to the executable, so the
rem book is packed first; the program loads only the book, not the pages.
echo Packing the help book...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\packHelp.ps1"
if errorlevel 1 (
    echo === Build failed: help book ===
    exit /b 1
)

echo Building project...
"%MSBUILD%" "%SOLUTION%" /nologo /m /v:minimal /p:Configuration=%BUILD_TYPE% /p:Platform=x64
if errorlevel 1 (
    echo === Build failed ===
    exit /b 1
)

if not exist "%EXECUTABLE%" (
    echo === Build failed ===
    echo Executable not found: %EXECUTABLE%
    exit /b 1
)

rem The project file has no msgfmt step, so the catalogues are compiled here:
rem without them both programs show the bare message ids (see BUILD.md).
call :locate_msgfmt
if not defined MSGFMT (
    echo Warning: msgfmt not found - the program will show message ids instead of text.
) else (
    echo Compiling the message catalogues...
    for %%p in ("%ROOT%po\*.po") do (
        set "MO_DIR=%BUILD_DIR%\locale\%%~np\LC_MESSAGES"
        if not exist "!MO_DIR!" md "!MO_DIR!"
        "!MSGFMT!" -o "!MO_DIR!\autofilesync.mo" "%%~fp"
    )
)

for %%f in ("%EXECUTABLE%") do set "SIZE=%%~zf"
echo === Build successful ===
echo Executable: %EXECUTABLE%
echo Size: %SIZE% bytes
echo Build type: %BUILD_TYPE%
echo.
echo To run:
echo   "%EXECUTABLE%"
exit /b 0

rem gettext keeps msgfmt inside the vcpkg tree, which is not on the PATH. The
rem root is where "vcpkg integrate install" recorded it, so the tree the project
rem links against also supplies the tool.
:locate_msgfmt
set "MSGFMT="
for /f "delims=" %%i in ('where msgfmt 2^>nul') do if not defined MSGFMT set "MSGFMT=%%i"
if defined MSGFMT exit /b 0
set "VCPKG_DIR=%VCPKG_ROOT%"
set "VCPKG_PATH_FILE=%LOCALAPPDATA%\vcpkg\vcpkg.path.txt"
if not defined VCPKG_DIR if exist "%VCPKG_PATH_FILE%" set /p VCPKG_DIR=<"%VCPKG_PATH_FILE%"
if not defined VCPKG_DIR exit /b 0
set "CANDIDATE=%VCPKG_DIR%\installed\x64-windows\tools\gettext\bin\msgfmt.exe"
if exist "%CANDIDATE%" set "MSGFMT=%CANDIDATE%"
exit /b 0
