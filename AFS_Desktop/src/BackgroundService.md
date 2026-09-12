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

## serviceName

Shared by the Windows service key and the systemd unit; launchd addresses the same daemon
through the reverse DNS label of its LaunchAgent instead.

## elevationTimeout

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

## withService

Opened with the least access that still allows the requested control, so that a member of the
Administrators group succeeds without the UAC detour.

## queryState-access

SERVICE_QUERY_STATUS is granted to every user, so the state is available without the
administrator rights that starting and stopping require.

## launchAgentLabel

launchd is the counterpart of the systemd unit. The daemon runs as a LaunchAgent of the logged
in user because it synchronises that user's directories with their credentials, so every call
addresses the gui domain of the current session rather than the system one.

## runLaunchctl

wxEXEC_NOEVENTS keeps this out of a nested event loop: the query runs while the menu is opening,
where re-entrant event dispatch would be unsafe.

## launchctl-list-pid

"launchctl list" prints a PID entry only while the process actually exists. An agent that is
registered but has since exited is loaded without running, which is a stopped service from the
user's point of view.

## startAgent

Two steps because a stop unregisters the agent while a daemon that exited on its own stays
registered: bootstrap covers the first case, kickstart the second, and kickstart on an already
running agent is a no-op.

## waitUntilUnregistered

bootout returns once launchd has accepted the request, not once the daemon has ended - and this
one needs a moment to finish its synchronisation pass. Reporting success right away would leave
the menu showing a running service that the user just stopped, so the command waits for the
state it promises.

## stopAgent

bootout unregisters the agent. Merely killing the process would not do: launchd restarts a
KeepAlive agent immediately afterwards.

## runSystemctl

systemctl asks polkit for the missing rights itself, so no elevation detour is needed here.

## queryState-systemctl

"systemctl show" reports an unknown unit as LoadState=not-found and still exits with 0, which
"is-active" cannot distinguish from a stopped service. wxEXEC_NOEVENTS keeps this out of a
nested event loop: the query runs while the menu is opening, where re-entrant event dispatch
would be unsafe.
