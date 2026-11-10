# RestoredBackgroundService

Module comment for RestoredBackgroundService.h.

## service_controllable

Whether this user may start and stop the installed service without a password prompt - the
prompt of UAC or polkit is something no test can answer. polkit only tells root whether a given
unit and verb would be allowed, so on Linux the probe asks systemctl for the one verb that leaves
the service as it is: start a running service, stop a stopped one, with --no-ask-password so
that a missing right fails instead of prompting. On Windows the probe opens the service with
exactly the rights a start and a stop need.

## start_obstacle

Whether the service can start on this machine at all - the right to start it, which
service_controllable() checks, does not say so. A Windows service runs under the account the
installer registered with the password typed in then; once that password has changed, the
service manager rejects every start with "logon failure", and no query reveals that
beforehand: only a start does. The probe therefore starts a stopped service through the same
call the menu uses and reports the message that would otherwise end in the "Hintergrunddienst"
error box, which the fixture would count as an unexpected dialog. A service that did start is
left running - the tests construct RestoredBackgroundService before they probe, so it ends as
it began.

## RestoredBackgroundService

The service is the real one of this machine, synchronising the user's real configuration, not
the sandbox. A test that starts or stops it puts it back into the state it found it in.
