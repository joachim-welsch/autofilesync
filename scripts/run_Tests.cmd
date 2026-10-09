@echo off
rem Test runner (Windows) - builds and runs the integration tests of AFS_Core and
rem the GUI tests of AFS_Desktop, counterpart to run_Integrationtests.sh and run_GUI_Tests.sh.
rem Both suites run even if the first one fails, so one call reports the state of both.
setlocal

set "ROOT=%~dp0..\"
set "IT_DIR=%ROOT%AFS_Core\test\Integrationstests"
set "IT_SOLUTION=%IT_DIR%\Integrationtests.sln"
set "BUILD_TYPE=Debug"
set "BUILD=1"
set "FILTER="
set "RUN_IT="
set "RUN_GUI="
rem Assigned outside every parenthesised block: cmd would split the "(x86)" here.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="-d"          (set "BUILD_TYPE=Debug"   & shift & goto parse_args)
if /i "%~1"=="--debug"     (set "BUILD_TYPE=Debug"   & shift & goto parse_args)
if /i "%~1"=="-r"          (set "BUILD_TYPE=Release" & shift & goto parse_args)
if /i "%~1"=="--release"   (set "BUILD_TYPE=Release" & shift & goto parse_args)
if /i "%~1"=="integration" (set "RUN_IT=1"           & shift & goto parse_args)
if /i "%~1"=="gui"         (set "RUN_GUI=1"          & shift & goto parse_args)
if /i "%~1"=="--no-build"  (set "BUILD=0"            & shift & goto parse_args)
if /i "%~1"=="--filter"    (set "FILTER=%~2"         & shift & shift & goto parse_args)
if /i "%~1"=="-h"          goto usage
if /i "%~1"=="--help"      goto usage
echo Unknown option: %~1
echo Use -h or --help for usage information
exit /b 1

:usage
echo Usage: %~nx0 [integration] [gui] [OPTIONS]
echo.
echo Suites (default: both):
echo   integration         Integration tests of AFS_Core
echo   gui                 GUI tests of AFS_Desktop - take over mouse and keyboard
echo.
echo Options:
echo   -d, --debug         Build and test the Debug version (default)
echo   -r, --release       Build and test the Release version
echo   --no-build          Run the existing test programs without building
echo   --filter PATTERN    Google Test filter, e.g. *Sync*
echo   -h, --help          Show this help message
echo.
echo Examples:
echo   %~nx0                              # Build and run both suites
echo   %~nx0 integration --filter *Ssh*   # Only the SSH integration tests
echo   %~nx0 gui --no-build               # GUI tests with the last build
exit /b 0

:args_done
if not defined RUN_IT if not defined RUN_GUI (set "RUN_IT=1" & set "RUN_GUI=1")
set "IT_EXIT="
set "GUI_EXIT="
if not defined RUN_IT goto after_integration
call :integration_tests
set "IT_EXIT=%ERRORLEVEL%"
:after_integration
if not defined RUN_GUI goto after_gui
call :gui_tests
set "GUI_EXIT=%ERRORLEVEL%"
:after_gui

echo.
echo === Summary (%BUILD_TYPE%) ===
set "FAILED=0"
if defined IT_EXIT if "%IT_EXIT%"=="0" (echo Integration tests: passed) else (echo Integration tests: FAILED, exit code %IT_EXIT% & set "FAILED=1")
if defined GUI_EXIT if "%GUI_EXIT%"=="0" (echo GUI tests:         passed) else (echo GUI tests:         FAILED, exit code %GUI_EXIT% & set "FAILED=1")
exit /b %FAILED%

:integration_tests
echo === Integration tests, %BUILD_TYPE% ===
if "%BUILD%"=="0" goto integration_run
call :locate_msbuild
if not defined MSBUILD exit /b 1
"%MSBUILD%" "%IT_SOLUTION%" /nologo /m /v:minimal /p:Configuration=%BUILD_TYPE% /p:Platform=x64
if errorlevel 1 exit /b 1
:integration_run
set "IT_EXE=%IT_DIR%\build\x64\%BUILD_TYPE%\Integrationtests.exe"
if exist "%IT_EXE%" goto integration_exec
echo Test program not found: %IT_EXE%
exit /b 1
:integration_exec
set "GTEST_ARGS="
if defined FILTER set "GTEST_ARGS=--gtest_filter=%FILTER%"
rem The working directory matches the Visual Studio debugger, which starts in the project directory.
pushd "%IT_DIR%"
"%IT_EXE%" %GTEST_ARGS%
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

rem run_GUI_Tests.ps1 compiles with cl.exe instead of MSBuild (see there), so it keeps the build.
:gui_tests
echo === GUI tests, %BUILD_TYPE% ===
set "GUI_ARGS=-Configuration %BUILD_TYPE%"
if "%BUILD%"=="0" set "GUI_ARGS=%GUI_ARGS% -NoBuild"
if defined FILTER set "GUI_ARGS=%GUI_ARGS% -Filter "%FILTER%""
call "%~dp0run_GUI_Tests.bat" %GUI_ARGS%
exit /b %ERRORLEVEL%

rem Same lookup as build.cmd: -prerelease finds Insiders/Preview installations, and the
rem 64-bit MSBuild comes first because the 32-bit one runs out of address space with /m.
:locate_msbuild
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
if not defined MSBUILD echo MSBuild not found - Visual Studio with the C++ workload is required.
exit /b 0
