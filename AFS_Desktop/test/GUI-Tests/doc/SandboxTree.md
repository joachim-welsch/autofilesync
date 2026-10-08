# SandboxTree

Module comment for SandboxTree.h / SandboxTree.cpp.

## SandboxTree

A file tree a test needs as source or target of a synchronisation, created under the sandbox
and removed again when the test leaves its scope - whether it passed or failed. The rule from
CLAUDE.md (the file system looks as before after every test) is thereby kept by construction,
not by a cleanup block at the end of the test that a failed assertion could skip.

## fresh-start

The root is wiped before it is filled: a tree a crashed earlier run left behind would
otherwise leak into the entries this test expects.

## entries

Relative paths in generic (forward slash) form, sorted: what a test compares against a
literal list, the same on every platform.
