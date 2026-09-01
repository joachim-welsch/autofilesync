#!/usr/bin/env python3
"""Builds icons/afs.icns, the icon the macOS application bundle carries.

macOS lays app icons out on a grid where the artwork covers 824 of 1024 points;
filling the whole canvas makes the icon look oversized next to every other icon
in the Dock. Needs only the standard library and iconutil, since ImageMagick is
not among the macOS prerequisites.
"""
import subprocess, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import afs_artwork

TILE_RATIO = 824 / 1024
SIZES = [(16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2),
         (256, 1), (256, 2), (512, 1), (512, 2)]

def main():
    target = Path(__file__).resolve().parent / 'afs.icns'
    with tempfile.TemporaryDirectory() as tmp:
        iconset = Path(tmp) / 'afs.iconset'
        iconset.mkdir()
        for size, scale in SIZES:
            edge = size * scale
            name = f'icon_{size}x{size}{"@2x" if scale == 2 else ""}.png'
            afs_artwork.write_png(iconset / name, edge, afs_artwork.render(edge, TILE_RATIO))
            print(f'  {name} ({edge}px)')
        subprocess.run(['iconutil', '-c', 'icns', str(iconset), '-o', str(target)], check=True)
    print(f'wrote {target} ({target.stat().st_size} bytes)')

if __name__ == '__main__':
    sys.exit(main())
