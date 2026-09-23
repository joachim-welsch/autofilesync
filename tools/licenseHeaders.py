"""Applies or checks the licence header of every C++ source file of AutoFileSync.

    python tools/licenseHeaders.py --check   # exit 1 and list files without the current header
    python tools/licenseHeaders.py --apply   # add the header to files without one, replace an older one

The header text is the one in HEADER below; the "part" line names the module the
file belongs to. Files under backgroundsync/obsolete are not touched.
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEADER = """/*
 * Copyright (C) 2026 Joachim Welsch <https://welsch-software.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of {part}.
 *
 * AutoFileSync is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * AutoFileSync is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * AutoFileSync. If not, see <https://www.gnu.org/licenses/>.
 */
"""
PARTS = {
    "AFS_Core": "AFS_Core, the synchronisation engine of AutoFileSync",
    "AFS_Desktop": "AFS_Desktop, the desktop program of AutoFileSync",
    "backgroundsync": "backgroundsync, the background service of AutoFileSync",
}
EXCLUDED = {"obsolete", "build", "vcpkg_installed", "packages", "x64", "Debug", "Release", "third-party"}
OLD_HEADER = re.compile(r"\A/\*\n \* Copyright \(C\) \d{4} Joachim Welsch.*?\*/\n", re.S)

def sources():
    for part in PARTS:
        for path in sorted((ROOT / part).rglob("*")):
            if path.suffix in (".cpp", ".h") and not EXCLUDED.intersection(path.parts):  # third-party trees and build output are not ours
                yield part, path

def wanted(part):
    return HEADER.format(part=PARTS[part])

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--check"
    wrong = []
    for part, path in sources():
        text = path.read_text(encoding="utf-8")
        bom = text.startswith("\ufeff")
        body = text[1:] if bom else text
        if body.startswith(wanted(part)):
            continue
        wrong.append(path)
        if mode == "--apply":
            body = OLD_HEADER.sub("", body, count=1)
            path.write_text(("\ufeff" if bom else "") + wanted(part) + body, encoding="utf-8", newline="\n")
    for path in wrong:
        print(("updated: " if mode == "--apply" else "missing header: ") + str(path.relative_to(ROOT)))
    return 0 if mode == "--apply" or not wrong else 1

if __name__ == "__main__":
    sys.exit(main())
