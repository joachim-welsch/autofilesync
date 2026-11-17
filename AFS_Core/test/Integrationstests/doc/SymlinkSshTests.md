# SymlinkSshTests

Module comment for SymlinkSshTests.cpp.

Symbolic links kept as links with one side behind SFTP (SyncLink.md#places). Every test is
skipped without the SSH test server (SshTest.md#SKIP_WITHOUT_SSH_SERVER). Unix only.

## SymlinkSshTest

The tree of SymlinkAsLinkTests, with directories open to everyone and readable files, because
the same paths are reached once as the test's user and once through "localhost" as the SSH
user. Deletions are permanent; the recycle bin of the test would outlive it.

## teardown

What the SSH user created belongs to that user, so the tree is first removed through the
server and then locally (SshTest.md#remove_remote_tree).

## LinkInfoOverSftpTellsLinkFileDirectoryAndNothingApart

The six answers of ssh_link_info(): a link to a file, to a directory, to nothing, a plain
file, a real directory and a name that is not there - all of them answers, none a failure.

## LinksAreCreatedOnAnSftpTargetWithTheirText

Four links to a server: relative to a file and to a directory, absolute into the tree, and
dangling. The texts are read locally from the same directory, which also proves the argument
order of the request (SyncFileSsh.md#links). The second pass is idle and nothing prepared or
parked is left.

## ChangedLinkReplacesTheOneOnTheSftpTarget

Park, rename into place and discard through SFTP.

## LinksOfAnSftpSourceArriveAsLinks

The other direction: the listing names the links, their text is fetched, and an absolute text
is made relative by the remote root.

## DeleteInTargetRemovesALinkOnTheSftpTarget

The reverse pass over a remote listing sees the link as a link and removes it, not what it
points to.

## TwoWaySyncWithAnSftpTargetCarriesAndDeletesLinks

With a state: a link deleted on the server goes locally, a new one on the server comes here.
