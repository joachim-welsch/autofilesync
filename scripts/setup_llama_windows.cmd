@echo off
rem PowerShell fuehrt Skriptdateien in der Voreinstellung nicht aus (Restricted).
rem Diese Batchdatei startet setup_llama_windows.ps1 mit einer Ausnahme, die nur
rem fuer diesen einen Prozess gilt - die Einstellung des Systems bleibt unveraendert.
rem Argumente werden durchgereicht:  setup_llama_windows.cmd -Size 3b
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_llama_windows.ps1" %*
exit /b %ERRORLEVEL%
