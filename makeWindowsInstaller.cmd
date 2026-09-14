@echo off
rem PowerShell fuehrt Skriptdateien in der Voreinstellung nicht aus (Restricted).
rem Diese Batchdatei startet makeWindowsInstaller.ps1 mit einer Ausnahme, die nur
rem fuer diesen einen Prozess gilt - die Einstellung des Systems bleibt unveraendert.
rem Argumente werden durchgereicht:  makeWindowsInstaller.cmd -NoBuild
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0makeWindowsInstaller.ps1" %*
exit /b %ERRORLEVEL%
