#!/usr/bin/env python3

import argparse


def real_main(args):
    mem_opc = set()
    with open(args.mem_opc) as f:
        for line in f:
            line = line.removeprefix("opc: ")
            line = line.removesuffix("\n")
            mem_opc.add(line)
    prefix_len = len("    1: ")
    seen_opc = set()
    with open(args.hist_opc) as f:
        for line in f:
            line = line[prefix_len:]
            line = line.split()[0]
            seen_opc.add(line)
    seen_mem_opc = mem_opc.intersection(seen_opc)
    print("\n".join(sorted(seen_mem_opc)))


def get_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="instr-intersect")
    parser.add_argument("-m", "--mem-opc", required=True, help="memory opcode file")
    parser.add_argument("-H", "--hist-opc", required=True, help="opcode histogram file")
    return parser


def main():
    real_main(get_arg_parser().parse_args())


if __name__ == "__main__":
    main()
