#!/usr/bin/env python3

import argparse

import numba
import numpy as np
from xnutrace.tracelog import TraceLog


def real_main(args):
    tl = TraceLog(args.trace_dir)
    pcs = tl.based_pcs_for_image(args.image_name)
    for base, sz in tl.subregions:
        print(f"base: {base:#018x} sz: {sz:#x}")


def get_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="find-loops")
    parser.add_argument("-t", "--trace-dir", required=True, help="input trace directory")
    parser.add_argument("-n", "--image-name", required=True, help="image name to find loops in")
    return parser


def main():
    real_main(get_arg_parser().parse_args())


if __name__ == "__main__":
    main()
