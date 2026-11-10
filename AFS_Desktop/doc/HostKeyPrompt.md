# HostKeyPrompt

Module comment for HostKeyPrompt.h / HostKeyPrompt.cpp.

## hostkeyprompt

The program's answer to an SSH server whose key is not known yet
(SshHostKeyVerification.md#unknown-host-key): it shows the user the fingerprint and asks
whether to trust the server from now on. App registers it at start, except in console mode,
where nobody is there to answer.

## confirm

Called by the core from whichever thread opens the connection: the directory dialog connects
on the main thread, a synchronisation on its workers.

## answers-remembered

A synchronisation opens several connections to the same server at once, and each of them finds
the key unknown before the first answer is stored. The answer is therefore remembered, so the
user is asked once: a Yes for as long as the program runs - the key is in known_hosts by then
anyway - and a No for a minute, long enough for the pass that asked and short enough to be
asked again after a slip. Without a fingerprint there is nothing to compare, so nothing is
accepted. The table is used on the main thread only.

## worker-threads

A message box belongs to the main thread, so a worker hands the question over and waits. It
gives up when the synchronisation is being stopped - the main thread may be waiting for this
very worker then and would never show the box - and after a generous time limit, so that a
main thread that does not get round to the question for any other reason cannot hold a worker
for ever. Giving up means no.
