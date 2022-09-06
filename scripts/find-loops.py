#!/usr/bin/env python3

import argparse

from tracelog import TraceLog


def real_main(args):
    tl = TraceLog(args.trace_file)
    if args.dump:
        tl.dump()


def get_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="find-loops")
    parser.add_argument("-t", "--trace-file", required=True, help="input trace file")
    parser.add_argument("-d", "--dump", action="store_true", help="dump trace file")
    return parser


def main():
    real_main(get_arg_parser().parse_args())


if __name__ == "__main__":
    main()
