@echo off
rem PowerShell fuehrt Skriptdateien in der Voreinstellung nicht aus (Restricted).
rem Diese Batchdatei startet run_GUI_Tests.ps1 mit einer Ausnahme, die nur fuer
rem diesen einen Prozess gilt - die Einstellung des Systems bleibt unveraendert.
rem Argumente werden durchgereicht:  run_GUI_Tests.bat -Filter *Papierkorb*
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_GUI_Tests.ps1" %*
exit /b %ERRORLEVEL%
