# SymlinkCloudTests

Module comment for SymlinkCloudTests.cpp.

Symbolic links kept as links when one side is a cloud, which has none
(SyncLink.md#no-links-there). The WebDAV test server stands for all cloud stores: the decision
is made before any of them is called, by the kind of account alone. Unix only, since the tests
create symbolic links; skipped without python3.

## SymlinkCloudTest

A local tree with two files and three links - to a file, to a directory, to nothing - and an
empty WebDAV server whose files are a local directory the tests can look into. The log is
redirected to count the once-per-pass warning (SymlinkTests.md#skipped_links).

## CloudAccountKeepsNoLinks

The three calls of the SSH layer for an account in a cloud: no links there, a link cannot be
created, and asking for one gives no answer rather than "nothing there".

## LinksAreSkippedForACloudTargetWithOneWarningAndNoError

The decision for targets without links: the files arrive, none of the three links does and
no content is uploaded in a link's place; the run is clean, and two passes give one line.

## TwoWaySyncWithACloudNeverDeletesTheLocalLinkAndRemembersNone

The danger of a two-way item: a link the cloud never received looks, on the next pass, like
one that was deleted there. Nothing is recorded for a skipped link, so nothing can be taken
for deleted, and the local links survive two passes.

## FileInTheCloudUnderTheNameOfALocalLinkIsLeftAlone

With "delete in target" a cloud file whose name a local link bears is neither deleted - the
name exists in the source - nor replaced.

## PreviewPlansNoStepForALinkTheCloudCannotTake

The preview shows what the run does: two files, no link.

## DefaultModeStillUploadsAFileLinkAsItsContent

The default mode is unchanged for a cloud target: the content of a file link is uploaded, a
directory link skipped.
