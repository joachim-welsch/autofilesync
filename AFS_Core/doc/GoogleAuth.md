# GoogleAuth

Module comment for GoogleAuth.h / GoogleAuth.cpp.

## oauth

Signs a user in at a cloud provider and keeps the short-lived access tokens fresh. The sign-in
is OAuth for installed programs: the user agrees in the browser, the provider sends the browser
back to a port on this computer with a code, and the code is exchanged for a refresh token,
which the account stores encrypted in place of a password. Google, Microsoft and Dropbox all work
this way; what differs between them is in `Provider`. The module keeps its name from the time when
Google was the only provider.

## Provider

What differs between the providers: where the client and the addresses are kept, the scope
asked for, extra parameters of the consent address, and the host name of the redirect. Google
wants `access_type=offline` and `prompt=consent` to hand out a refresh token; Microsoft wants
the scope `offline_access` instead. Microsoft accepts any port only for the host name
`localhost`, Google for the address 127.0.0.1 as well, which cannot be misdirected by a hosts
file and is therefore used there.

## google_auth

The settings of the Google client and shorthand for a Google login.

## microsoft

The application registered for AutoFileSync at Microsoft Entra, as a public client for
"Mobile and desktop applications" with the redirect address `http://localhost`. A public
client has no secret, and Microsoft rejects a request that sends one, so the token requests
leave it out when it is empty. The identifier comes from AFS_MICROSOFT_CLIENT_ID, set like
Google's (#client) on the command line or in a `microsoft-client.cmake` that git ignores. The
addresses use the tenant `common`, which admits private accounts and those of organisations.
The scope is `Files.ReadWrite`: all files of the user, for which Microsoft asks no assessment.

## dropbox

The app registered for AutoFileSync in the Dropbox App Console, with access to the full
Dropbox. Its app key comes from AFS_DROPBOX_APP_KEY, set like the others (#client) on the
command line or in a `dropbox-client.cmake` that git ignores. With PKCE the app secret is not
needed and not used. The consent address asks for `token_access_type=offline`, without which
Dropbox hands out no refresh token. No scope is sent: the app then gets the permissions set
for it in the console, which must include reading and writing file content and metadata.
Besides the API address the settings hold a second one for file content (Dropbox.md#two-hosts).

## redirect-ports

Dropbox accepts a redirect only to an address registered for the app, port included, so the
login cannot let the system choose a port there. Three ports are registered
(`http://localhost:53682` to `53684`) and the first that is free is taken; three, so that a
second sign-in or another program on one of them does not block it. With an empty list any
port will do, as for Google and Microsoft.

## renewed-refresh-token

Microsoft hands out a new refresh token with every access token, and the one stored at sign-in
expires after ninety days. The newest one is therefore written back into the account and the
configuration file, but at most once a day per account: every hour would rewrite the
configuration for nothing. Google's refresh token stays the same and is never written.

## client

The identifier and the "secret" of the OAuth client registered for AutoFileSync in the Google
Cloud Console (type "Desktop app"). They are not in the sources. The CMake build takes them
from the variables AFS_GOOGLE_CLIENT_ID and AFS_GOOGLE_CLIENT_SECRET, set on the command line or
in a file `google-client.cmake` beside the top-level CMakeLists.txt that git ignores, and
passes them in as the two macros. A build without them, such as the Visual Studio projects
unless the macros are defined there, has an empty identifier and offers no Google sign-in
(configured()). For a desktop client Google does not treat the secret as one, since every
installed copy carries it; PKCE (#pkce) is what protects the exchange. Keeping it out of the
repository still avoids handing the client to everyone who reads the code.

## settings

Client and addresses in one place the tests can exchange for those of their own server. The
program never changes them.

## scope

`drive.file`: access only to files and folders the program created itself. It is enough to
back up into the drive and to synchronise a folder the program made, and it is the widest scope
Google grants without a yearly security assessment. The price is that files already in the
drive are invisible, also when they lie in a folder the program created.

## Login

One sign-in. The constructor opens a listening socket on the loopback interface with a port the
system chooses; browser_url() is the address of Google's consent page for it.

## loopback

The redirect goes to `http://127.0.0.1:<port>`. Google allows that address for desktop clients
without registering it, and no other computer can reach it.

## pkce

A random verifier is made per sign-in; its SHA-256 hash travels with the consent request and the
verifier itself only with the code exchange. Whoever intercepts the code on its way through the
browser cannot redeem it without the verifier.

## finished

Waits up to the given time for the browser to come back, answers it with a short page and notes
the code. Meant to be called again and again from a timer with no patience, so the dialog stays
responsive. A redirect with a foreign `state` or without a code counts as a failed sign-in, not
as "still waiting".

## refresh_token

Exchanges the code for the refresh token; throws when the sign-in failed or Google refuses the
exchange.

## token-endpoint

Both exchanges post a form to the token address. An error answer carries Google's own reason
("Token has been expired or revoked"), which is passed on in the exception because it tells
the user what to do: sign in again.

## access_token

The access token for an account, fetched with its refresh token at the provider the account
belongs to and kept until a minute before it expires.
