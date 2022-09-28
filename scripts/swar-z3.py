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
