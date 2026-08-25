#!/usr/bin/env python3
"""
PNG til LVGL-billede, uden at installere noget.

Vi laeser PNG'en selv i stedet for at kraeve Pillow. Brand-logoerne er
8-bit RGBA uden interlace, hvilket er det simpleste tilfaelde i PNG, og
det er omkring halvtreds linjer at pakke ud med zlib. Til gengaeld kan
enhver hente projektet og bygge det uden foerst at skulle have styr paa
et Python-miljoe.

Nedskaleringen er et rent gennemsnit over det omraade hver ny pixel
daekker. Det er den rigtige metode naar man goer et billede MINDRE, og
den giver skarpere kanter end den naermeste nabo, som ville faa
logoets tynde streger til at flimre.

Alfa haandteres foermultipliceret under skaleringen. Goer man det ikke,
traekker de gennemsigtige pixels omkring logoet deres farve ind over
kanten, og et hvidt logo faar en graa rand.

Format ud: LV_IMG_CF_TRUE_COLOR_ALPHA med 16-bit farve, altsaa tre
bytes pr. pixel: RGB565 med laveste byte foerst, og derefter alfa.
"""

import argparse
import struct
import sys
import zlib


# ----------------------------------------------------------------------
# PNG-afkodning
# ----------------------------------------------------------------------

def read_png_rgba(path):
    """Returnerer (bredde, hoejde, bytearray med RGBA)."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} er ikke en PNG-fil")

    pos = 8
    idat = bytearray()
    width = height = 0
    depth = color = interlace = None

    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length          # laengde + type + data + CRC

        if ctype == b"IHDR":
            width, height, depth, color, _comp, _filt, interlace = \
                struct.unpack(">IIBBBBB", body)
        elif ctype == b"IDAT":
            idat += body
        elif ctype == b"IEND":
            break

    if depth != 8 or color != 6:
        raise ValueError(f"{path}: kun 8-bit RGBA er understoettet "
                         f"(fandt dybde {depth}, farvetype {color})")
    if interlace != 0:
        raise ValueError(f"{path}: interlacede PNG-filer er ikke understoettet")

    raw = zlib.decompress(bytes(idat))

    # Fjern PNG's raekkefiltre. Hver raekke starter med en filterbyte.
    bpp = 4
    stride = width * bpp
    out = bytearray(height * stride)
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        ftype = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride

        if ftype == 0:                      # None
            pass
        elif ftype == 1:                    # Sub
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif ftype == 2:                    # Up
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ftype == 3:                    # Average
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:                    # Paeth
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = prev[i]
                c = prev[i - bpp] if i >= bpp else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        else:
            raise ValueError(f"{path}: ukendt raekkefilter {ftype}")

        out[y * stride:(y + 1) * stride] = line
        prev = line

    return width, height, out


# ----------------------------------------------------------------------
# Nedskalering
# ----------------------------------------------------------------------

def resize_rgba(src, sw, sh, dw, dh):
    """Gennemsnit over omraade, med foermultipliceret alfa."""
    dst = bytearray(dw * dh * 4)
    for dy in range(dh):
        y0 = dy * sh / dh
        y1 = (dy + 1) * sh / dh
        iy0, iy1 = int(y0), max(int(y0) + 1, int(y1 - 1e-9) + 1)
        for dx in range(dw):
            x0 = dx * sw / dw
            x1 = (dx + 1) * sw / dw
            ix0, ix1 = int(x0), max(int(x0) + 1, int(x1 - 1e-9) + 1)

            r = g = b = a = 0.0
            n = 0
            for sy in range(iy0, min(iy1, sh)):
                base = (sy * sw) * 4
                for sx in range(ix0, min(ix1, sw)):
                    o = base + sx * 4
                    av = src[o + 3] / 255.0
                    r += src[o + 0] * av
                    g += src[o + 1] * av
                    b += src[o + 2] * av
                    a += src[o + 3]
                    n += 1
            if n == 0:
                continue
            a /= n
            if a > 0.5:
                # Traek foermultiplikationen ud igen
                inv = n * (a / 255.0)
                r, g, b = r / inv, g / inv, b / inv
            else:
                r = g = b = 0.0
            o = (dy * dw + dx) * 4
            dst[o + 0] = min(255, max(0, int(r + 0.5)))
            dst[o + 1] = min(255, max(0, int(g + 0.5)))
            dst[o + 2] = min(255, max(0, int(b + 0.5)))
            dst[o + 3] = min(255, max(0, int(a + 0.5)))
    return dst


# ----------------------------------------------------------------------
# Udskrivning
# ----------------------------------------------------------------------

def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def write_c(path, name, w, h, rgba):
    body = []
    for i in range(w * h):
        r, g, b, a = rgba[i * 4:i * 4 + 4]
        c = to_rgb565(r, g, b)
        # LVGL gemmer farven som en almindelig uint16 i maskinens egen
        # byte-orden. ESP32 er little-endian, saa laveste byte foerst.
        body.append(f"0x{c & 0xFF:02x},0x{c >> 8:02x},0x{a:02x},")

    lines = []
    for i in range(0, len(body), 8):
        lines.append("    " + "".join(body[i:i + 8]))

    with open(path, "w") as f:
        f.write(f"""/*
 * {name} - genereret af tools/png2lvgl.py. Ret ikke i haanden.
 * Kilde: brand-mappen. {w} x {h} pixels, {w * h * 3} bytes.
 */

#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

static const LV_ATTRIBUTE_MEM_ALIGN uint8_t {name}_map[] = {{
""")
        f.write("\n".join(lines))
        f.write(f"""
}};

const lv_img_dsc_t {name} = {{
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = {w},
    .header.h = {h},
    .data_size = {w * h * 3},
    .data = {name}_map,
}};
""")


def main():
    ap = argparse.ArgumentParser(description="PNG til LVGL C-array")
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--name", required=True, help="variabelnavn i C")
    ap.add_argument("--width", type=int, help="maalbredde, hoejden foelger med")
    ap.add_argument("--height", type=int, help="maalhoejde, bredden foelger med")
    args = ap.parse_args()

    w, h, rgba = read_png_rgba(args.input)

    tw, th = w, h
    if args.width and args.height:
        tw, th = args.width, args.height
    elif args.width:
        tw = args.width
        th = max(1, round(h * tw / w))
    elif args.height:
        th = args.height
        tw = max(1, round(w * th / h))

    if (tw, th) != (w, h):
        if tw > w or th > h:
            print(f"  advarsel: {args.input} skaleres OP fra {w}x{h} til "
                  f"{tw}x{th}. Det bliver uskarpt.", file=sys.stderr)
        rgba = resize_rgba(rgba, w, h, tw, th)

    write_c(args.output, args.name, tw, th, rgba)
    print(f"  {args.name:22s} {tw:3d}x{th:<3d} {tw * th * 3:6d} bytes")


if __name__ == "__main__":
    main()
