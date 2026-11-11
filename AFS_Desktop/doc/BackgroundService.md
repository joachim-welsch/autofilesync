# BackgroundService

Module comment for BackgroundService.h / BackgroundService.cpp.

## BackgroundService

Controls the backgroundsync daemon, which synchronises with the same configuration while the GUI
is not running. The Windows service key and the systemd unit deliberately share this name, so
one identifier addresses both.

## State

Unknown covers the cases where the state cannot be determined; the caller then offers both
commands instead of locking the user out on a query problem.

## state

Everything but a stopped service counts as running, so that the transitional states do not offer
a start that the service control would reject anyway.

## command-result

An empty result means success, otherwise it holds the message for the user.

## SERVICE_NAME

Shared by the Windows service key and the systemd unit; launchd addresses the same daemon
through the reverse DNS label of its LaunchAgent instead.

## ELEVATION_TIMEOUT

"sc start" returns as soon as the SCM has accepted the control, so this cap only guards against
a hung service control manager.

## elevate

The GUI runs unelevated while service control needs administrator rights. Handing the command to
sc.exe with the runas verb asks the user once through the UAC prompt instead of failing with a
message they cannot act on.

ShellExecuteW would only report whether sc.exe could be launched. Since it runs hidden,
everything it ran into afterwards - a failed logon of the service account, for one - stayed
invisible and the start looked successful. SEE_MASK_NOCLOSEPROCESS keeps the handle so that its
exit code can be read.

## sc_path

sc.exe is named with its full path in the system directory. A bare name is looked up in the
working directory first, and since the command runs with administrator rights after the UAC
prompt, a file of that name lying wherever the program happened to be started from would run
elevated in its place.

## quoted

The password travels to sc.exe on its command line and may contain anything. The rules are
those of the C runtime's argument parser: a quote is escaped with a backslash, and backslashes
count as themselves unless a quote follows, in which case each of them is doubled.

## sc-exit-code

sc.exe passes the error of the control it forwarded on as its exit code, so 1069 here is the
SCM's "logon failed" for the service account.

## start-pending

A service reports START_PENDING until the daemon has loaded the configuration and accepts no
control before then: a Stop right after a Start would fail with "cannot accept control" (1061),
an error the user did nothing wrong for. The control therefore waits for that short phase to
pass. STOP_PENDING is not waited for: it lasts until the running synchronisation pass has
reached its next item, which can take long, and a Start during it fails with a message that
says the service is still running - the right answer in that case.

## with_service

Opened with the least access that still allows the requested control, so that a member of the
Administrators group, and the service account - the installer grants it start and stop on the
service - succeed without the UAC detour.

## service_account

The account is read for the message about a failed logon, see [logon-failure](#logon-failure).
SERVICE_QUERY_CONFIG is granted to interactive users by default; where it is not, the account
stays empty and the generic failure message is shown instead.

## logon-failure

The installer registers the service under the account of the person installing, with the
password typed in at that time. Windows keeps that password with the service, so after the
user changes their Windows password every start fails with ERROR_SERVICE_LOGON_FAILED (1069).
The system text for it, "logon failure", does not tell the user that it is their own old
password at fault, hence a message that names the account and the place to enter the new one.
The exit code of sc.exe after the UAC detour ends up here as well.

## query_state-access

SERVICE_QUERY_STATUS is granted to every user, so the state is available without the
administrator rights that starting and stopping require; adding it to the access of a start or
stop therefore cannot be what makes them fail.

## LAUNCH_AGENT_LABEL

launchd is the counterpart of the systemd unit. The daemon runs as a LaunchAgent of the logged
in user because it synchronises that user's directories with their credentials, so every call
addresses the gui domain of the current session rather than the system one.

## daemon_path

The daemon ships inside the application bundle, next to the GUI executable, so an update of the
bundle updates the daemon with it. "Not installed" therefore means a bundle without the daemon
(a build tree that only built the GUI), not a missing LaunchAgent: the agent is written by Start.

## agent_plist

The LaunchAgent is generated, not shipped: it names the daemon by its absolute path, which is only
known once the bundle sits wherever the user dropped it. RunAtLoad starts the daemon at login,
KeepAlive restarts it when it ends on its own - the counterpart of the unit's Restart=on-failure.

## agent-follows-the-bundle

Compared by content on every Start: a plist written for an earlier location (a manually installed
daemon under Application Support, a bundle moved since) would start the wrong, possibly outdated
binary. launchd keeps the definition it was bootstrapped with, so a changed plist takes effect only
after the agent was booted out and bootstrapped again.
On the first Start no plist exists yet; its existence is checked before opening it, because a failed
open of wxFFile reports itself as an error box through wxLog.

## run_launchctl

wxEXEC_NOEVENTS keeps this out of a nested event loop: the query runs while the menu is opening,
where re-entrant event dispatch would be unsafe.

## launchctl-list-pid

"launchctl list" prints a PID entry only while the process actually exists. An agent that is
registered but has since exited is loaded without running, which is a stopped service from the
user's point of view.

## start_agent

Three steps: the plist is brought up to date first, then bootstrap registers an agent that a stop
has unregistered, and kickstart starts a daemon that exited on its own while staying registered.
kickstart on an already running agent is a no-op.

## wait_until_unregistered

bootout returns once launchd has accepted the request, not once the daemon has ended - and this
one needs a moment to finish its synchronisation pass. Reporting success right away would leave
the menu showing a running service that the user just stopped, so the command waits for the
state it promises.

## stop_agent

bootout unregisters the agent. Merely killing the process would not do: launchd restarts a
KeepAlive agent immediately afterwards. The plist goes as well: launchd loads every file in
LaunchAgents at the next login, so a stop that left it behind would hold only until then - and a
stopped service plus the bundle in the bin leaves nothing of AutoFileSync on the machine.

## run_systemctl

systemctl asks polkit for the missing rights itself, so no elevation detour is needed here. The
package's polkit rule grants the service user start and stop without asking; anyone else gets
polkit's password prompt.

## query_state-systemctl

"systemctl show" reports an unknown unit as LoadState=not-found and still exits with 0, which
"is-active" cannot distinguish from a stopped service. wxEXEC_NOEVENTS keeps this out of a
nested event loop: the query runs while the menu is opening, where re-entrant event dispatch
would be unsafe.

## logon_failed

Whether the last command failed because Windows could not log the service account on. The
menu command reads it to offer the way out - asking for the current password - rather than
only naming the cause. The value belongs to the last start, stop or set_password and is used
from the GUI thread only.

## set_password

Hands the service the current password of its account (logon-failure above explains why it
goes stale). Changing the service configuration needs administrator rights, so this always goes
through the UAC prompt. Whether the password is right only shows at the next start: the service
manager stores it unchecked.
