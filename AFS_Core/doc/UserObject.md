# UserObject

Module comment for UserObject.h / UserObject.cpp. Windows only.

## userobject

The program and the background service find each other through named kernel objects: an event
per side while it synchronises (SyncArbiter.md) and a mutex for whoever serves the phones
(DeviceLink.md). They run in different sessions, so the names have to be visible across
sessions.

The Global\ namespace offers that, but to every account of the PC: anyone could create an
object of the same name first and thereby make the program believe that the other side is
busy for ever, or keep it from serving the phones. The objects therefore live in a private
namespace whose boundary is the SID of the user. Only a process running as that user can
create or open the namespace, and both the program and the service do - the installer
registers the service under the account of the person who installs.

## user_only

A security descriptor that grants the user full access and nobody else any. It is stated
explicitly because the default one depends on the token: the service of an administrator runs
with the full token, whose default grants the Administrators group but not the user, and the
program with its filtered token would then be refused its own service's objects.

## user_namespace

The handle is kept for the life of the process; the namespace exists as long as one process
holds it.

## namespace-race

Creating fails when the other process created the namespace first, opening fails when that
process ended in between; a few rounds settle it.

## fallback

Without the namespace - the token could not be read, the system refused - the names fall back
to Global\, as before the namespace existed. Both sides then still find each other as long as
both fall back; the protection against other accounts is what is lost.

## create_event

Manual-reset and signalled: the event carries no state, its existence is the message.

## event_exists

A failure other than "not found" counts as existing: an event that is there but cannot be
opened still tells that the other side is at work.

## acquire_mutex

Null when another process holds the mutex already.
