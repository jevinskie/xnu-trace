#!/usr/bin/env python3

import itertools
import sys

from z3 import *

z32 = BitVecVal(0, 32)
inst_sym = BitVec("inst_sym", 32)

ldrp_eq_bm = BitVecVal(0x3F800000, 32)
ldrp_eq_bp = BitVecVal(0x29000000, 32)
ldrp_ne_bm = z32
ldrp_ne_bp = z32
ldrp_example_inst = BitVecVal(0xA9400408, 32)

ldr_eq_bm = BitVecVal(0x3F000000, 32)
ldr_eq_bp = BitVecVal(0x39000000, 32)
ldr_ne_bm = z32
ldr_ne_bp = z32
ldr_example_inst = BitVecVal(0xF9400020, 32)


def inst_match(
    inst: BitVecVal, eq_bm: BitVecVal, eq_bp: BitVecVal, ne_bm: BitVecVal, ne_bp: BitVecVal
) -> BitVecVal:
    pos_good = (inst & eq_bm) == eq_bp
    neg_good = (ne_bm == z32) or ((inst & ne_bm) != ne_bp)
    return pos_good, neg_good, pos_good and neg_good


inst = BitVecVal(int.from_bytes(bytes.fromhex(sys.argv[1]), "big"), 32)
print(f"inst_bytes: {inst.as_long():#010x}")

pg, ng, m = inst_match(ldrp_example_inst, ldrp_eq_bm, ldrp_eq_bp, ldrp_ne_bm, ldrp_ne_bm)
pg, ng, m = simplify(pg), simplify(ng), simplify(m)
print(f"ldrp pg: {pg} ng: {ng} m: {m}")

pg, ng, m = inst_match(ldr_example_inst, ldr_eq_bm, ldr_eq_bp, ldr_ne_bm, ldr_ne_bm)
pg, ng, m = simplify(pg), simplify(ng), simplify(m)
print(f"ldrp pg: {pg} ng: {ng} m: {m}")

ldrp_pg, ldrp_ng, ldrp_m = inst_match(inst_sym, ldrp_eq_bm, ldrp_eq_bp, ldrp_ne_bm, ldrp_ne_bm)
ldr_pg, ldr_ng, ldr_m = inst_match(inst_sym, ldr_eq_bm, ldr_eq_bp, ldr_ne_bm, ldr_ne_bm)
ldrp_m = simplify(ldrp_m)
ldr_m = simplify(ldr_m)
ldrp_or_ldr_m = ldrp_m or ldr_m
ldrp_or_ldr_m = simplify(ldrp_or_ldr_m)

print(f"ldrp_m: {ldrp_m}")
print(f"ldr_m: {ldr_m}")
print(f"ldrp_or_ldr_m: {ldrp_or_ldr_m}")

ldrp_example_m = simplify(substitute(ldrp_m, (inst_sym, ldrp_example_inst)))
print(f"ldrp_example_m: {ldrp_example_m}")

ldr_example_m = simplify(substitute(ldr_m, (inst_sym, ldr_example_inst)))
print(f"ldr_example_m: {ldr_example_m}")

ldrp_example_m_mul = simplify(substitute(ldrp_or_ldr_m, (inst_sym, ldrp_example_inst)))
print(f"ldrp_example_m_mul: {ldrp_example_m_mul}")

ldr_example_m_mul = simplify(substitute(ldrp_or_ldr_m, (inst_sym, ldr_example_inst)))
print(f"ldr_example_m_mul: {ldr_example_m_mul}")
