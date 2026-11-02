# SshHostKeyTests

Module comment for SshHostKeyTests.cpp.

verify_ssh_host_key() takes a connected session, so the tests build the session themselves and
point SSH_OPTIONS_KNOWNHOSTS at a file of their own. That is what makes the three outcomes
reachable at all: the production code reads the user's known_hosts (libssh resolves it through
the password database, not $HOME, so the private HOME of TestMain does not isolate it), which
already knows the test server, so a sync test only ever sees "known". Unix only: the Windows
branch goes through libssh2 and a different file.

## log-names-the-branch

verify_ssh_host_key() answers only true or false, but "refused" has three causes (changed,
other type, not storable). The log line names the branch, so each test redirects the log and
looks for its message - the ScopedLog lives in the fixture because the dialog of the member
initialisers needs dir_, which is set up before it.

## Session

Connect, verify, disconnect, free - the libssh2-free RAII wrapper of the libssh session. The
connect is done in the constructor so that ssh_session_is_known_server() and
ssh_get_server_publickey() have a handshake to look at.

## different-key-same-type

A changed key is only "changed" when libssh negotiates the same host key algorithm it finds in
known_hosts; an entry of another type would be "other" and take the default branch. So the test
asks the server for its key type, generates a fresh key of that type in-process and writes it
under the server's name - no ssh-keygen, no second server.

## UnknownHostIsTrustedOnFirstUseAndStored

Trust-on-first-use is the documented policy: the first contact stores the key, and the file has
to exist afterwards with an entry for the host.

## StoredHostKeyIsAcceptedWithoutChangingTheFile

The second contact is the normal case; it must neither fail nor grow the file.

## ChangedHostKeyIsRefused

The security property: a key that no longer matches the stored one is a possible
man-in-the-middle, the connection is refused and the file stays as it is - trust-on-first-use
must not silently replace the old key.

## UnwritableKnownHostsFailsClosed

A key that cannot be stored is not trusted either: otherwise an attacker who can keep the file
unwritable would get a fresh "first use" every time.

## FetchedHostKeyIsTheOneTheServerPresents

fetch_ssh_host_key() is what the account dialog shows the user before the first real contact
(SshAccountDialog.md#host-key-confirmation): the line it returns has to be the one
verify_pinned_host_key() later accepts, its fingerprint has OpenSSH's form, and fetching
touches no known_hosts file - the decision stays with the user.
