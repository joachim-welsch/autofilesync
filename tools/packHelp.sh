#!/bin/bash
# Packs the help books AFS_Desktop/help/help.htb (German) and
# AFS_Desktop/help/<language>/help.htb (every other language) from their pages: the pages listed in
# the [FILES] section of help.hhp plus contents, index, project file and context
# map. The program loads only the book, so a changed page shows up only after
# packing. The CMake build packs the book itself (AFS_Desktop/CMakeLists.txt);
# this script serves the MSBuild build (build.cmd) and can be run on its own.
set -e
help_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../AFS_Desktop/help" && pwd)"

# one book per language: the German pages in help/, every other language in help/<language>/
pack_book()
{
    local help_dir="$1"
    files="$(sed -n '/^\[FILES\]/,/^\[/{/^\[/d;/^[[:space:]]*$/d;p}' "$help_dir/help.hhp") contents.hhc index.hhk help.hhp wxhelp.map"
    for name in $files; do
        [ -f "$help_dir/$name" ] || { echo "Hilfeseite fehlt: $help_dir/$name" >&2; exit 1; }
    done
    for toc in contents.hhc index.hhk; do  # wxHtmlHelp shows an entry name with an HTML entity as an empty entry
        if grep -qE 'value="[^"]*&[a-zA-Z]+;' "$help_dir/$toc"; then
            echo "$help_dir/$toc: HTML-Entity in einem Eintragsnamen, das Zeichen bitte in UTF-8 schreiben" >&2; exit 1
        fi
    done
    book="$help_dir/help.htb"
    rm -f "$book.tmp"
    if command -v zip >/dev/null 2>&1; then
        (cd "$help_dir" && zip -q -j "$book.tmp" $files)
    else  # zip is not part of every base system; Python's zipfile is the fallback (python3 on Unix, python in Git Bash on Windows)
        python=""
        for candidate in python3 python; do  # tried, not just looked up: on Windows "python3" may be the Store's installer stub
            if "$candidate" -c pass >/dev/null 2>&1; then python="$candidate"; break; fi
        done
        [ -n "$python" ] || { echo "Weder zip noch Python gefunden - das Hilfebuch kann nicht gepackt werden." >&2; exit 1; }
        "$python" - "$help_dir" "$book.tmp" $files <<'EOF'
import sys, zipfile, os
help_dir, book, *names = sys.argv[1:]
with zipfile.ZipFile(book, "w", zipfile.ZIP_DEFLATED) as z:
    for name in names:
        z.write(os.path.join(help_dir, name), name)
EOF
    fi
    mv -f "$book.tmp" "$book"
    echo "Hilfebuch gepackt: $book ($(echo $files | wc -w) Dateien)"
}

pack_book "$help_root"
for language_dir in "$help_root"/*/; do
    [ -f "$language_dir/help.hhp" ] && pack_book "${language_dir%/}"
done
