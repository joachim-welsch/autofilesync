# WindowAndHelpTest

Module comment for WindowAndHelpTest.cpp.

## ShownAgain

Every test here hides the main window at some point; the next test needs it on screen, so it
is shown again when the test leaves its scope, whether it passed or failed.

## activateTaskBarMenuItem

The popup menu of the tray icon is a native menu the simulator cannot open, so the menu is
built the way the icon builds it for a click (CreatePopupMenu) and the entry is found by the
label the user reads; the command it carries is sent to the icon's own handler, which is where
a real pick ends up.

## ClosedHelp

The help book opens in its own frame that would stay on screen for the following tests, and a
second DisplayContents() would only raise it; it is closed when the test leaves its scope.

## help

Both routes to the help are tested, the menu and the toolbar button: the button used to open
the About box (GUI-Testplan.txt, open points) although its icon and tooltip promised the help.

## background-service

Start and Stopp go through the service manager, which needs elevation when the service is
installed - a UAC prompt a test cannot answer - so only the "not installed" case, which ends in
the "Hintergrunddienst" error box, runs; on a machine with the service the test skips itself.

## LicenseFile

A licence file in the sandbox's configuration directory, removed when the test leaves its
scope: the directory is where showLicense() looks first.

## license

The status line at the start is gone by the time the first test runs, so showLicense() is
called again. Two licences issued with the product's private key are pinned here with their
signatures - one for a named licensee, one without a licensee for private use - so the whole
way from the file on disk to the status bar runs with the embedded public key. The private key
itself stays outside the repository; a new key pair has to re-issue both signatures.
