# WindowsService

Module comment for WindowsService.cpp, the Windows entry point of backgroundsync.
backgroundsync.cpp is the POSIX counterpart: there the daemon is a plain process that
systemd or launchd keeps alive, here it is a Windows service that the service control
manager starts and stops.

## SERVICE_NAME

The key under which the service is registered. BackgroundService in AFS_Desktop addresses the
service by this name, and the installer registers it under it, so all three must agree.

## STOP_WAIT_HINT

A stop ends the running synchronisation pass at its next item, which can take a while for a
large file over SSH. The hint keeps the service control manager from declaring the service hung
before the pass has ended; the installer waits for the stop as well when it removes the service.

## run_in_console

StartServiceCtrlDispatcher only succeeds when the service control manager started the process.
Started from a console it fails with ERROR_FAILED_SERVICE_CONTROLLER_CONNECT, and the daemon
then runs in the foreground until Ctrl+C - the way to try it out or debug it without
registering a service first. Any other error means the process was started as a service and
could not become one, so it is logged and the exit code tells the service control manager.
