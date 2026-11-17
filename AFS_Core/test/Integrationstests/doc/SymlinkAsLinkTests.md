# SymlinkAsLinkTests

Module comment for SymlinkAsLinkTests.cpp.

The mode that carries symbolic links over as links (SyncLink.md), local and one-way. Unix
only: creating a symbolic link is a privilege on Windows.

## LinkError

A file system that has links cannot be made to refuse one, so the tests tell SymlinkUtil which
error to answer with (SymlinkUtil.md#create_link_refusal): "operation not permitted" is what
a drive without links says, anything else is a failure.

## SymlinkAsLinkTest

A source tree with a real directory and a real file, an empty target and an item in the mode
"as link". Each test adds the links it is about. The log is redirected so that the
once-per-pass messages can be counted (SymlinkTests.md#skipped_links).

## TreeSyncCarriesFileAndDirectoryLinksOverAsLinks

Both kinds of link arrive as links with their text, and both lead to the copy: the directory
is there once, under its own name.

## AbsoluteLinkIntoTheTreeBecomesRelative

The rule the user chose (SymlinkUtil.md#portable_link_text), at the top level and one level
down, where the relative text has to climb. The source link keeps its absolute text.

## LinksLeavingTheTreeAreCopiedAsTheyAreAndNamedOnce

An absolute and a relative link that point beside the tree: both are copied unchanged, and
the pass says so in one line (SyncBase.md#report_links).

## DanglingLinkIsCarriedOverAsALink

In this mode a link that leads nowhere is a link like any other, not something to skip
(SymlinkTests.md#dangling-links is the default mode).

## SecondPassFindsNothingToDoAndAChangedLinkIsUpdated

The comparison is by the portable text on both sides, so the relative link in the target
equals the absolute one in the source and the second pass is idle. A link that now points
elsewhere replaces the old one, and what was parked meanwhile is gone.

## FileInTheTargetIsReplacedByTheLink

A plain file under the link's name gives way; nothing is left beside it.

## LinkThatCannotBeCreatedPutsTheOldTargetBackAndIsAnError

A failure that is not "no links here" is an error with the link's name, and the file the link
was to replace is still there with its content (SyncLink.md#create).

## TargetWithoutLinksSkipsThemWithOneWarningAndNoError

The decision for drives without links: the links are left out, the content is not copied in
their place, an existing file of the same name is not even renamed, and the run is clean with
one warning for all of them.

## DirectoryInTheTargetIsNeverRemovedForALink

SyncLink.md#blocked-by-directory.

## GenerationPrincipleKeepsWhatTheLinkReplaces

The replaced file stays as the next generation, as it would for a replaced file.

## MoveRemovesTheSourceLinkOnlyAfterTheTargetLinkExists

"Delete after copying" with a link that first cannot be created: the source keeps it. The
deletions are permanent here, since the recycle bin of the test would outlive the test.

## DirSyncCarriesTheLinksOfItsLevelOver

A directory item skips directories but not links to them: the link is an entry of its level.

## PreviewCountsLinksAndCreatesNone

Counting touches nothing.

## ExcludedLinkIsNotCarriedOver

Exclusions hold for a link to a file and for one to a directory.

## TargetDirectoryThatIsALinkIsNotWrittenThrough

SyncTree.md#target-directory-is-a-link, both halves: without "delete in target" an error and
nothing where the link points, with it the link replaced by the real directory.

## SystemLinksAreSkippedAndNamedOnce

Linux only. A device and a file of the kernel: neither arrives, one line names them, and the
default mode skips such a link too instead of copying the kernel's file
(SyncBase.md#system_link_skipped).
