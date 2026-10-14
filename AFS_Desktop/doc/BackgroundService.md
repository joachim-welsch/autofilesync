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

## sc-exit-code

sc.exe passes the error of the control it forwarded on as its exit code, so 1069 here is the
SCM's "logon failed" for the service account.

## with_service

Opened with the least access that still allows the requested control, so that a member of the
Administrators group, and the service account - the installer grants it start and stop on the
service - succeed without the UAC detour.

## query_state-access

SERVICE_QUERY_STATUS is granted to every user, so the state is available without the
administrator rights that starting and stopping require.

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
