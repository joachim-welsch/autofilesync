@echo off
rem Windows hat kein Gegenstueck zu Xvfb: wxUIActionSimulator schickt seine
rem Eingaben ueber SendInput an den interaktiven Desktop, ein eigener Desktop
rem wuerde sie gar nicht erst erreichen. Waehrend des Laufs darf deshalb weder
rem Maus noch Tastatur benutzt werden.
setlocal
set BINARY=%1
if "%BINARY%"=="" set BINARY=%~dp0x64\Debug\GuiTests.exe
if not exist "%BINARY%" (
    echo Testprogramm nicht gefunden: %BINARY%
    exit /b 1
)
shift
echo GUI-Tests laufen auf dem aktiven Desktop - bitte Maus und Tastatur nicht benutzen.
"%BINARY%" %1 %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
