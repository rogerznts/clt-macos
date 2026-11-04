"""Extract the largest PNG RT_ICON from a PE file and write a macOS .icns.

usage: make_icon.py <exe> <out.icns>
"""
import os
import struct
import subprocess
import sys
import tempfile


def sections(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    n = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    osz = struct.unpack_from("<H", d, pe + 20)[0]
    rsrc_rva = struct.unpack_from("<I", d, opt + 112 + 8 * 2)[0]
    secs = [struct.unpack_from("<8sIIII", d, opt + osz + 40 * i) for i in range(n)]
    return rsrc_rva, secs


def rva_to_off(secs, rva):
    for _, vs, va, rs, ro in secs:
        if va <= rva < va + max(vs, rs):
            return rva - va + ro
    raise ValueError(hex(rva))


def icons(d):
    """Yield the payload of every RT_ICON (type 3) resource."""
    rsrc_rva, secs = sections(d)
    root = rva_to_off(secs, rsrc_rva)

    def entries(o):
        named, ids = struct.unpack_from("<HH", d, o + 12)
        for i in range(named + ids):
            name, target = struct.unpack_from("<II", d, o + 16 + 8 * i)
            yield name, target

    for typ, t in entries(root):
        if typ != 3 or not t & 0x80000000:
            continue
        for _, n in entries(root + (t & 0x7FFFFFFF)):
            for _, leaf in entries(root + (n & 0x7FFFFFFF)):
                rva, size = struct.unpack_from("<II", d, root + leaf)
                o = rva_to_off(secs, rva)
                yield d[o : o + size]


def main(exe, out):
    d = open(exe, "rb").read()
    pngs = [b for b in icons(d) if b[:8] == b"\x89PNG\r\n\x1a\n"]
    if not pngs:
        sys.exit("no PNG icon in " + exe)
    best = max(pngs, key=lambda b: struct.unpack(">I", b[16:20])[0])
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "icon.png")
        open(src, "wb").write(best)
        iconset = os.path.join(tmp, "CLT.iconset")
        os.mkdir(iconset)
        for s in (16, 32, 128, 256, 512):
            for scale, suffix in ((1, ""), (2, "@2x")):
                px = str(s * scale)
                subprocess.run(["sips", "-z", px, px, src, "--out", f"{iconset}/icon_{s}x{s}{suffix}.png"],
                               check=True, capture_output=True)
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", out], check=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
