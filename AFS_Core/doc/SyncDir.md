# SyncDir

Module comment for SyncDir.h / SyncDir.cpp.

## ssh-source-local-target

source_path is always SSH here (this method only runs for an SSH source); destination may still
be a local path, so only it depends on ssh_target().

## source-names

Names seen in the source, so the target-only pass below can tell "not in source" from
"wildcard-excluded, ignore" - see sync_target_only_entries().

## followed-file-links

A listing over SFTP names a link but not what it leads to, so the default mode skips every
link there. With the mode "follow the link" the server is asked, and a link to a file is
copied as its content, as it always was for a local source. A link to a directory stays out:
a directory item does not descend. A dead link is skipped; the server's answer keeps it from
being read as a missing file.
