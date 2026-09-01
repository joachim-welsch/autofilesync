#!/usr/bin/env python3
"""Rebuilds the upper layers of icons/afs.ico from the vector artwork.

The 16, 24 and 32 pixel layers were tuned by hand - their ring is thicker and
their arrow heads larger than a linear reduction produces, which is what keeps
them readable in the taskbar. Those are carried over untouched; only the layers
a renderer can match are redrawn. Layer order is preserved because
INSTALL-Linux.md addresses the 256x256 image as 'icons/afs.ico[6]'.
"""
import struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import afs_artwork

REDRAW_FROM_EDGE = 48
TILE_RATIO = 240 / 256          # the artwork sits closer to the edge than on macOS

def read_layers(path):
    data = path.read_bytes()
    count = struct.unpack('<H', data[4:6])[0]
    layers = []
    for i in range(count):
        w, h, _, _, _, _, size, off = struct.unpack('<BBBBHHII', data[6 + i*16:22 + i*16])
        layers.append((w or 256, data[off:off + size]))
    return layers

def dib(size, rows):
    """One icon image: header, bottom-up BGRA pixels, then the legacy AND mask."""
    mask_stride = ((size + 31) // 32) * 4
    pixels = bytearray()
    for row in reversed(rows):
        for x in range(size):
            r, g, b, a = row[x*4:x*4+4]
            pixels += bytes((b, g, r, a))
    mask = b'\x00' * (mask_stride * size)    # alpha carries transparency, mask stays clear
    header = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0,
                         len(pixels) + len(mask), 0, 0, 0, 0)
    return header + bytes(pixels) + mask

def main():
    target = Path(__file__).resolve().parent / 'afs.ico'
    images = []
    for size, original in read_layers(target):
        if size >= REDRAW_FROM_EDGE:
            images.append((size, dib(size, afs_artwork.render(size, TILE_RATIO))))
            print(f'  {size}x{size} redrawn')
        else:
            images.append((size, original))
            print(f'  {size}x{size} kept')
    offset = 6 + 16 * len(images)
    directory = b''
    for size, image in images:
        edge = 0 if size == 256 else size     # 256 is encoded as zero in the directory
        directory += struct.pack('<BBBBHHII', edge, edge, 0, 0, 1, 32, len(image), offset)
        offset += len(image)
    target.write_bytes(struct.pack('<HHH', 0, 1, len(images)) + directory
                       + b''.join(img for _, img in images))
    print(f'wrote {target} ({target.stat().st_size} bytes)')

if __name__ == '__main__':
    sys.exit(main())
