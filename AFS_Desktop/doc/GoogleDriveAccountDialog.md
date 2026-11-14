# GoogleDriveAccountDialog

Module comment for GoogleDriveAccountDialog.h / GoogleDriveAccountDialog.cpp.

## GoogleDriveAccountDialog

Creates or edits a Google Drive connection. There is nothing to type but a name: the button
opens Google's sign-in in the browser, and OK becomes available once the sign-in has come back.
An existing connection opens as signed in and can be renamed, or signed in again when Google
has withdrawn the access.

## explanation

Says before the browser opens what the user is about to allow: that the password stays with
Google and that the program will only see what it puts into the drive itself. The second point
also answers the question that otherwise comes later, why the existing files do not show up.
The text is wrapped to a fixed width because a wxStaticText does not wrap by itself.

## polling

While a sign-in is under way a timer asks five times a second whether the browser has come back
(GoogleAuth.md#finished). The dialog stays usable meanwhile, so the user can cancel or start
over when the browser window was closed. When the answer is there the code is exchanged for the
token at once and the status line says how it went.

## open_browser_with

Replaces the way the sign-in page is opened. The program opens the default browser; the GUI
tests put a function here that visits the page themselves.
