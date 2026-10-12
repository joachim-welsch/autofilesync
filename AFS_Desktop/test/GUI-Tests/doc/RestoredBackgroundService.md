# RestoredBackgroundService

Module comment for RestoredBackgroundService.h.

## serviceControllable

Whether this user may start and stop the installed service without a password prompt - the
prompt of UAC or polkit is something no test can answer. polkit only tells root whether a given
unit and verb would be allowed, so on Linux the probe asks systemctl for the one verb that leaves
the service as it is: start a running service, stop a stopped one, with --no-ask-password so
that a missing right fails instead of prompting. On Windows the probe opens the service with
exactly the rights a start and a stop need.

## RestoredBackgroundService

The service is the real one of this machine, synchronising the user's real configuration, not
the sandbox. A test that starts or stops it puts it back into the state it found it in.
