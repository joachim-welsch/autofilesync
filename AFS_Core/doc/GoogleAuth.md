# GoogleAuth

Module comment for GoogleAuth.h / GoogleAuth.cpp.

## google_auth

Signs a user in at Google and keeps the short-lived access tokens fresh. The sign-in is OAuth
for installed programs: the user agrees in the browser, Google sends the browser back to a port
on this computer with a code, and the code is exchanged for a refresh token, which the account
stores encrypted in place of a password.

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

The access token for an account, fetched with its refresh token and kept until a minute before
it expires. The cache is keyed by name and refresh token, so an account that signed in again
does not get the token of its former login.
