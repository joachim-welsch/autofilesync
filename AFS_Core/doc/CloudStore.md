# CloudStore

Module comment for CloudStore.h / CloudStore.cpp.

## cloud_store

The remote sides that are not SSH, behind one set of functions: WebDAV and Google Drive. The
remote functions and the two inspectors ask handles() whether an account belongs here and then
call these functions, which pick the client from the account's kind (SshAccount.md#url_,
SshAccount.md#google_drive_). A further provider needs a client with the same functions and a
line in each of them.
