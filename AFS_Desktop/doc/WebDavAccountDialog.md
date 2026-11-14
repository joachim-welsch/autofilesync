# WebDavAccountDialog

Module comment for WebDavAccountDialog.h / WebDavAccountDialog.cpp.

## WebDavAccountDialog

Creates or edits a WebDAV connection: a name, the address of the server, user and password. It
is a dialog of its own and not a mode of the SSH dialog because the fields differ (an address
instead of host and port, no host key) and a dialog that changes its fields with a switch is
harder to read than two short ones. The name follows the rules of SSH connections and shares
their name space, since a synchronisation refers to both kinds as `<name>:/path`.

## address-hint

The address is the one thing a user cannot guess, so the line under the field says where to
find it and shows the Nextcloud form, the most common server. It also mentions the app
password, without which an account with two-factor sign-in is refused. The text is wrapped to a
fixed width because a wxStaticText does not wrap by itself.

## connection-check

OK asks the server for the address before anything is saved. A typing mistake in the address or
the password shows up here, with the server's own reason, instead of as a failed
synchronisation later. The user may save anyway, for a server that is switched off at the
moment.
