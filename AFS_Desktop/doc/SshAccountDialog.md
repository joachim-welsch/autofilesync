# SshAccountDialog

Module comment for SshAccountDialog.h / SshAccountDialog.cpp.

## colon-first

The colon is checked before the character rule, which would reject it too: the colon is the
separator between account name and path in "name:/path", and the message naming it tells the
user why this one character matters more than any other rejected one.

## validate_ssh_account_name

Shared with SshAccountsDialog's inline label edit, which otherwise bypasses this dialog's
validation entirely. Returns the German error message if name is invalid, nullopt if it's fine.

## password_from_account

Counterpart of save_data(): passwords are stored as UTF-8 bytes. FromUTF8() yields an empty
string for anything else, so a password written by an older version that encoded it in the local
codepage still has to go through the locale converter instead of being wiped when the dialog is
reopened.

## utf8-fields

FromUTF8 instead of the implicit conversion: the configuration holds these fields as UTF-8, and
wxString would otherwise read them through wxConvLibc in the ANSI codepage, turning a name like
"Buero" with an umlaut into mojibake.

## utf8-on-the-wire

ToStdString() converts with the locale converter, which yields an empty string for every
character the current codepage cannot represent - a password containing a Euro sign or an umlaut
would silently be lost on Windows. UTF-8 keeps every character, is what SSH expects on the wire
(RFC 4252), and is how the configuration stores these fields anyway.

## name-compared-against-original

name_ still holds the original name at Validate() time (set_data() set it,
TransferDataFromWindow() only runs after Validate() succeeds) - comparing against it, not just
checking for any existing match, also allows leaving an existing account's own name unchanged.

## port

The port sits in the host's row, small and preset to 22: most users never change it, and
whoever runs SSH on another port knows the number belongs next to the host
(Config.md#ssh-port).

## host-key-confirmation

OK fetches the host key of the server the user typed, before anything is saved, and shows its
fingerprint in OpenSSH's form; the user trusts it or stays in the dialog. The key then travels
with the account (Config.md#ssh-host-key), and every later connection, in the GUI and in the
background service alike, pins it (SshHostKeyVerification.md#verify_pinned_host_key). Without
this step the first contact would store whatever key the network presented, unseen, possibly
from a process with no user to ask at all. A changed key for an account that already has one
gets the alarming wording: a reinstalled server is the harmless reason, an attack the other one.
When the server cannot be reached the user may still save: a connection is often set up before
the NAS is switched on, and the key is then taken at the first contact as before.
