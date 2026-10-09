#!/bin/bash
# Packs all sources under version control into autofilesync-<version>-src.zip and
# autofilesync-<version>-src.tar.gz. Uncommitted changes to tracked files are
# included, untracked files are not (they are listed as a warning).
# Usage: scripts/packSources.sh [output directory]   (default: build/)
set -e
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
version="$(sed -n 's/^#define afsVERSION_TEXT "\(.*\)".*/\1/p' AFS_Desktop/include/Version.h)"
[ -n "$version" ] || { echo "Version nicht gefunden in AFS_Desktop/include/Version.h" >&2; exit 1; }
out_dir="${1:-$repo/build}"
mkdir -p "$out_dir"
out_dir="$(cd "$out_dir" && pwd)"
name="autofilesync-$version"
tree="$(git stash create)"  # commit of the working tree without touching it; empty when clean
tree="${tree:-HEAD}"
untracked="$(git ls-files --others --exclude-standard)"
[ -z "$untracked" ] || printf 'Nicht versionierte Dateien werden nicht gepackt:\n%s\n' "$untracked" >&2
for format in zip tar.gz; do
    archive="$out_dir/$name-src.$format"
    git archive --format="$format" --prefix="$name/" -o "$archive.tmp" "$tree"
    mv -f "$archive.tmp" "$archive"
    echo "$archive"
done
