# SymlinkFollowTests

Module comment for SymlinkFollowTests.cpp.

The mode that follows symbolic links (SyncTree.md#followed-links), local. Unix only: the tests
create symbolic links.

## SymlinkFollowTest

A source tree, an empty target and a third directory beside both for the links to lead to.
Each item bears the name of its test: what a pass says about its links is logged once per
item until it goes away, and a shared name would let one test swallow the line of another.

## DirectoryLinkIsEnteredAndItsContentCopied

A link to a directory outside the tree and one to a file: the target gets a real directory
with everything below it and a plain file, and the second pass is idle.

## loops

A link to the parent and one to the directory itself are not entered, are named in one line
and are no error. The second test closes the loop outside the tree: the walk has entered the
foreign directory through a link and meets, further down, a link back to its own root and one
back to that foreign directory. Without the guard each of these runs until the path is too
long for the system.

## LinkIntoTheTargetIsNotEntered

A link from the source to the target directory would make every pass copy the last pass's
copy one level deeper. It is skipped, and two passes say so once.

## TwoLinksToTheSameDirectoryAreBothEntered

The guard looks at the way down, not at everything visited: two links to one directory and a
link to a sibling directory are all entered, and none of them is called a loop.

## ChainOfLinksIsCutAtTheLimit

One link more than MAX_FOLLOWED_LINKS, each leading to a directory that holds the next. The
file behind the last link that is still followed arrives, the next link is not entered.

## LinkThatLeadsNowhereIsSkippedAndCounted

With "delete in target" and a file of the link's name in the target: a dead link is no
deleted entry, the file stays.

## DeleteInTargetWorksBehindAFollowedLink

A file only the target has, below the followed directory, is removed; nothing behind the link
is touched.

## ExcludedLinkIsNotEntered

An exclusion holds for a link as for a directory.

## PreviewCountsBehindTheLinkAndCreatesNothing

Four files, two of them behind the link; the link to itself adds nothing and does not make
the count run away.

## LinkInTheTargetIsNotWrittenThrough

SyncBase.md#target_usable for the followed directory.

## WithTwoWayOrMoveTheModeFallsBackAndSaysSo

SyncBase.md#follows_links: nothing arrives under the link's name, nothing is written behind
the link, the pass says once why, and the rest of the item is synchronised.

## DirectoryItemStillDoesNotDescend

A directory item copies the file link's content and leaves the directory link alone.
