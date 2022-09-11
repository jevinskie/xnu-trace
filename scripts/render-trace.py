#!/usr/bin/env python3

import argparse

from tracelog import TraceLog


def real_main(args):
    tl = TraceLog(args.trace_dir)
    pcs = tl.pcs_for_image(args.image_name)
    print("done")
    # print(pcs)


def get_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="find-loops")
    parser.add_argument("-t", "--trace-dir", required=True, help="input trace directory")
    parser.add_argument("-o", "--output", required=True, help="render png file")
    parser.add_argument("-n", "--image-name", required=True, help="image name to find loops in")
    return parser


def main():
    real_main(get_arg_parser().parse_args())


if __name__ == "__main__":
    main()
