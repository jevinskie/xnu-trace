#!/usr/bin/env python3

from __future__ import annotations

import colorsys
import io

import cairocffi as cairo
from attrs import define

bit_sz = 32
num_rows = 3
num_bits = 32
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
fg = black
bg = white


class BitContext(cairo.Context):
    def __init__(self, surface: cairo.Surface, bit_sz: int) -> None:
        super().__init__(surface)
        self.bs = bit_sz
        self.select_font_face("Monaco", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        self.set_font_size(self.bs / 2)

    def bitrect(self, bit_num: int, row: int, label: str, fill_color: Color = grey) -> None:
        x, y = (bit_num + 1) * self.bs, (row + 1) * self.bs
        w, h = self.bs, self.bs
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
    ctx.bitrect(i, 0, str(i))

for i in range(12):
    ctx.bitrect(i + 8, 2, str(i), red)

surface.finish()

with open("test.svg", "wb") as svgout:
    svgout.write(svgio.getvalue())
