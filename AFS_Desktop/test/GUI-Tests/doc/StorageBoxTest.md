# StorageBoxTest

Module comment for StorageBoxTest.cpp.

## names

Which user names count as those of a Storage Box, and the host that follows from one. No
network is involved.

## dialog

The dialog must not ask for host and port, and a name that is not a Storage Box user must be
refused with a message while the dialog stays open. The test stops there: a valid name would
make the dialog contact Hetzner, which a test must not do.
