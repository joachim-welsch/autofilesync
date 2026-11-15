# CloudStore

Module comment for CloudStore.h / CloudStore.cpp.

## cloud_store

The remote sides that are not SSH, behind one set of functions: WebDAV, Google Drive,
OneDrive and Dropbox. The
remote functions and the two inspectors ask handles() whether an account belongs here and then
call these functions, which pick the client from the account's kind (SshAccount.md#url_,
SshAccount.md#cloud_drive_). A further provider needs a client with the same functions and a
line in each of them.

## by_kind

Calls the one of four functions that belongs to the account's kind. Each function of this
module names its four counterparts once, here, instead of repeating the chain of questions.
