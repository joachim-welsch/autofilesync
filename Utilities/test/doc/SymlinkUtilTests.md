# SymlinkUtilTests

Module comment for SymlinkUtilTests.cpp.

Unix only: creating a symbolic link is a privilege on Windows.

## SymlinkUtilTest

A directory of its own with one real file for the links to point to.

## DanglingLinkIsALinkWhoseTargetIsMissingOrRunsInACircle

The two ways a link leads nowhere, and the three things that must not be mistaken for one: a
link that resolves, a plain file and a path that does not exist at all.

## LinkInfoTellsTheTextAndWhatTheLinkLeadsTo

The three kinds of link the synchronisation meets - to a file, to a directory, to nothing -
and two paths that are no links.

## portable_link_text

Made-up paths: the rule is about text, nothing has to exist. The cases that must not be
rewritten are the important ones - relative text, text outside the root, a sibling whose name
begins like the root, and dots that climb out.

## leaves_root

The same paths asked the other question.

## system_link

Linux only, because it uses what every Linux has: a file in /proc, the null device and a
directory in /sys. The removable drive need not exist; the rule is about where the link leads.

## create_link_refusal

The real path with both kinds of link and a dangling one, the two failures a file system does
produce, and the override: "operation not permitted" is what a drive without links answers.

## LinkTrailTest

A source and a target tree and a third directory outside both.

## LinkTrail

One test per answer. SiblingReachedTwiceIsNoLoop is the one that keeps the guard from being
too strict: the same directory is refused only while the walk is inside it.
ChainOfLinksEndsAtTheLimit also shows that leaving gives the place back.

## LinkTrail-resolved

Made-up paths of a machine that does not exist: every answer comes from the paths handed in.

## junction

All this machine can say about junctions: there are none here, whatever the path is. The
Windows side of the function has no test that runs anywhere yet.
