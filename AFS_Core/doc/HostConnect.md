# HostConnect

Module comment for HostConnect.h / HostConnect.cpp. Windows only; the other platforms leave
resolving and connecting to libssh.

## connect_host

Resolves a host name or address literal and returns a connected TCP socket. Every SSH
connection of the Windows build starts here, so that all of them reach the same hosts: before,
each place resolved on its own, most of them for IPv4 only, and a server that is reachable over
IPv6 alone could be browsed in one dialog and not synchronised.

Winsock must be started by the caller.

## ConnectFailure

Resolve and Connect are told apart because the callers report them differently: a name that
does not resolve is usually a typing error in the account, a failed connect a server that is
switched off. A socket that cannot be created counts as a failed connect.

## ipv4-first

All IPv4 addresses are tried before the IPv6 ones. A host that announces an IPv6 address which
is not routed from here would otherwise cost the full connect timeout on every file before the
working address gets its turn; IPv4 first keeps every host that worked before as fast as before.
