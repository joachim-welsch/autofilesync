# AddSshAccountTest

Module comment for AddSshAccountTest.cpp.

## PASSWORD_WITH_SPECIAL_CHARACTERS

The reported defect: a password containing "$+!" was believed to be rejected by the dialog.
Covered here with every special character reachable on a keyboard, since that is what the dialog
has to accept.
