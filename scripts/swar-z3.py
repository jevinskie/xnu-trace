#!/usr/bin/env python3

import itertools

import z3

bits = [z3.BitVec(f"b{i}", 1) for i in range(16)]

ival = z3.Concat(*reversed(bits))

print(z3.simplify(ival))

oval = z3.Concat(
    *itertools.chain(*zip(reversed(bits), [z3.BitVecVal(0, 1) for i in range(len(bits))]))
)

print(z3.simplify(oval))

ival32 = z3.ZeroExt(48, ival)

mixed = ival32
mixed = (mixed ^ (mixed << 8)) & z3.BitVecVal(0x00FF_00FF, 64)
mixed = (mixed ^ (mixed << 4)) & z3.BitVecVal(0x0F0F_0F0F, 64)
mixed = (mixed ^ (mixed << 2)) & z3.BitVecVal(0x3333_3333, 64)
mixed = (mixed ^ (mixed << 1)) & z3.BitVecVal(0x5555_5555, 64)

print(z3.simplify(mixed))
