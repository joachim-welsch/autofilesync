"""Resolution-independent drawing of the AutoFileSync artwork.

All geometry is expressed as a fraction of the icon tile, measured off the
256x256 layer of the original icons/afs.ico so that every platform keeps the
same picture. Callers pick the tile's share of the canvas: macOS lays app icons
out on 824 of 1024 points, Windows icons sit closer to the edge.
"""
import math, struct, zlib

GRADIENT = ((55, 70, 95), (18, 26, 45))   # top-left to bottom-right
GREEN = (52, 211, 153)
WHITE = (255, 255, 255)
TILE_RADIUS = 0.22750
RING_MID, RING_STROKE = 0.35625, 0.10833
ARCS = ((141, 281), (321, 101))           # degrees, screen angles, y downwards
ROUND_CAP_ENDS = (141, 321)
ARROWS = (281, 101)
ARROW_APEX_OFFSET, ARROW_APEX_RADIUS = 23.5, 0.36667
ARROW_BASE_INNER, ARROW_BASE_OUTER = 0.24042, 0.46833
DOC = (0.37500, 0.34167, 0.62500, 0.65833)
DOC_FOLD = 0.11250
# Below this the document turns into a few grey pixels and only muddies the
# ring, so the original afs.ico left it out of its 16, 24 and 32 pixel layers.
DOCUMENT_MIN_EDGE = 48
SUPERSAMPLE = 3

def _angle_within(angle, start, end):
    return (angle - start) % 360 <= (end - start) % 360

def _in_tile(x, y):
    inner = 0.5 - TILE_RADIUS
    cx, cy = min(max(x, -inner), inner), min(max(y, -inner), inner)
    return abs(x) <= 0.5 and abs(y) <= 0.5 and math.hypot(x - cx, y - cy) <= TILE_RADIUS

def _in_ring(x, y, r, a):
    if abs(r - RING_MID) <= RING_STROKE / 2 and any(_angle_within(a, s, e) for s, e in ARCS):
        return True
    return any(math.hypot(x - RING_MID*math.cos(math.radians(d)), y - RING_MID*math.sin(math.radians(d)))
               <= RING_STROKE / 2 for d in ROUND_CAP_ENDS)

def _arrow(base_deg):
    base, apex = math.radians(base_deg), math.radians(base_deg + ARROW_APEX_OFFSET)
    return ((ARROW_BASE_OUTER*math.cos(base), ARROW_BASE_OUTER*math.sin(base)),
            (ARROW_BASE_INNER*math.cos(base), ARROW_BASE_INNER*math.sin(base)),
            (ARROW_APEX_RADIUS*math.cos(apex), ARROW_APEX_RADIUS*math.sin(apex)))

_TRIANGLES = [_arrow(d) for d in ARROWS]

def _in_triangle(x, y, tri):
    (ax, ay), (bx, by), (cx, cy) = tri
    d1 = (x-bx)*(ay-by) - (ax-bx)*(y-by)
    d2 = (x-cx)*(by-cy) - (bx-cx)*(y-cy)
    d3 = (x-ax)*(cy-ay) - (cx-ax)*(y-ay)
    return not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0))

def _in_document(x, y):
    left, top, right, bottom = (v - 0.5 for v in DOC)
    if not (left <= x <= right and top <= y <= bottom):
        return False
    return x - y <= right - top - DOC_FOLD    # 45 degree cut across the top right corner

def _shade(x, y, document):
    """Colour at one sub-sample in tile coordinates, or None where transparent."""
    if not _in_tile(x, y):
        return None
    if document and _in_document(x, y):
        return WHITE
    r, a = math.hypot(x, y), math.degrees(math.atan2(y, x)) % 360
    if _in_ring(x, y, r, a) or any(_in_triangle(x, y, t) for t in _TRIANGLES):
        return GREEN
    t = max(0.0, min(1.0, (x + y) / 2 + 0.5))
    lo, hi = GRADIENT
    return tuple(round(lo[i] + (hi[i] - lo[i]) * t) for i in range(3))

def render(size, tile_ratio):
    """RGBA scanlines, size x size pixels, artwork covering tile_ratio of the edge."""
    document = size >= DOCUMENT_MIN_EDGE
    step = 1.0 / size / SUPERSAMPLE
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            r = g = b = hits = 0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    u = ((px/size + (sx+0.5)*step) - 0.5) / tile_ratio
                    v = ((py/size + (sy+0.5)*step) - 0.5) / tile_ratio
                    c = _shade(u, v, document)
                    if c:
                        r += c[0]; g += c[1]; b += c[2]; hits += 1
            n = SUPERSAMPLE * SUPERSAMPLE
            row += bytes((r//hits, g//hits, b//hits, round(255*hits/n))) if hits else b'\x00\x00\x00\x00'
        rows.append(bytes(row))
    return rows

def write_png(path, size, rows):
    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))
    raw = b''.join(b'\x00' + row for row in rows)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9))
                     + chunk(b'IEND', b''))
