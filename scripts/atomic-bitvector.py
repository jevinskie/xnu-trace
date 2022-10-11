#!/usr/bin/env python3

from __future__ import annotations

import colorsys
import io

import cairocffi as cairo
from attrs import define

bit_sz = 32
num_rows = 14
num_bits = 128
canvas_width, canvas_height = (num_bits + 2) * bit_sz, (num_rows + 2) * bit_sz


@define
class Color:
    h: float
    s: float
    v: float

    def darken(self, m: float) -> Color:
        return Color(self.h, self.s, self.v * m)

    def rgb(self) -> tuple[float, float, float]:
        return colorsys.hsv_to_rgb(self.h, self.s, self.v)


black = Color(0, 0, 0)
white = Color(0, 0, 1)
grey = white.darken(0.95)
red = Color(0, 0.4, 1)
orange = Color(40 / 360, 0.4, 1)
blue = Color(225 / 360, 0.4, 1)
green = Color(130 / 360, 0.4, 1)
violet = Color(300 / 360, 0.4, 1)
fg = black
bg = white


class BitContext(cairo.Context):
    def __init__(self, surface: cairo.Surface, bit_sz: int) -> None:
        super().__init__(surface)
        self.bs = bit_sz
        self.select_font_face("Monaco", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        self.set_font_size(self.bs / 2.5)
        self.rectangle(0, 0, canvas_width, canvas_height)
        self.set_source_rgb(*bg.rgb())
        self.fill()

    def bitrect(
        self, bit_idx: int, num_bits: int, row: int, label: str, fill_color: Color = grey
    ) -> None:
        x, y = (bit_idx * num_bits + 1) * self.bs, (row + 1) * self.bs
        w, h = self.bs * num_bits, self.bs
        center_x, center_y = x + w / 2, y + h / 2
        self.rectangle(x, y, w, h)
        self.set_source_rgb(*fill_color.rgb())
        self.fill()
        self.rectangle(x, y, w, h)
        self.set_source_rgb(*fg.rgb())
        self.stroke()
        txb, tyb, tw, th, tdx, tdy = self.text_extents(label)
        tx = center_x - tw / 2 - txb
        ty = center_y - th / 2 - tyb
        self.move_to(tx, ty)
        self.set_source_rgb(*fg.rgb())
        self.show_text(label)


svgio = io.BytesIO()
surface = cairo.SVGSurface(svgio, canvas_width, canvas_height)
surface.set_document_unit(cairo.SVG_UNIT_USER)
ctx = BitContext(surface, bit_sz)

for i in range(num_bits):
    ctx.bitrect(i, 1, 0, str(i % 32))

for i, wi in enumerate(((8, orange), (16, blue), (32, green))):
    wb, wc = wi[0], wi[1]
    nw = num_bits // wb
    for j in range(nw):
        ctx.bitrect(j, wb, i + 1, str(j), wc)

word_bits = 15
num_words = num_bits // word_bits

for word_idx in range(num_words):
    for i in range(word_bits):
        idx = i + word_idx * word_bits
        ctx.bitrect(idx, 1, 4 + word_idx, str(idx % 32), red)

for word_idx in range(num_words):
    for i in range(word_bits):
        idx = i + word_idx * word_bits
        ctx.bitrect(idx, 1, 5 + word_idx, str((idx + 16) % 32), violet)

for word_idx in range(num_words):
    for i in range(word_bits):
        idx = i + word_idx * word_bits
        wsbidx = idx // word_bits * word_bits
        wswidx = wsbidx // 16
        eidx = idx % 32
        oidx = (idx + 16) % 32
        if wswidx % 2 == 0:
            bidx = eidx
        else:
            bidx = oidx
        s = str(bidx)
        ctx.bitrect(idx, 1, 6 + word_idx, s, orange)

surface.finish()

with open("atomic-bits.svg", "wb") as svgout:
    svgout.write(svgio.getvalue())
