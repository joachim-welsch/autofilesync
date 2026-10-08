# SyncDir

Module comment for SyncDir.h / SyncDir.cpp.

## ssh-source-local-target

source_path is always SSH here (this method only runs for an SSH source); destination may still
be a local path, so only it depends on ssh_target().

## source-names

Names seen in the source, so the target-only pass below can tell "not in source" from
"wildcard-excluded, ignore" - see sync_target_only_entries().
