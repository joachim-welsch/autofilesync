# SymlinkUtilTests

Module comment for SymlinkUtilTests.cpp.

Unix only: creating a symbolic link is a privilege on Windows.

## SymlinkUtilTest

A directory of its own with one real file for the links to point to.

## DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle

The two ways a link leads nowhere, and the three things that must not be mistaken for one: a
link that resolves, a plain file and a path that does not exist at all.
