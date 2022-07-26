#!/usr/bin/env python3

import argparse

import colorcet
import datashader as ds
import numpy as np
import pandas as pd
from xnutrace.tracelog import TraceLog


def real_main(args):
    tl = TraceLog(args.trace_dir)
    pcs = tl.based_pcs_for_image(args.image_name)
    idx = np.arange(len(pcs))
    raw_points = np.dstack((pcs, idx))[0, :, :]
    df = pd.DataFrame(raw_points, columns=("addr", "time"), copy=False)
    canvas = ds.Canvas(plot_width=850, plot_height=500)
    agg = canvas.points(df, "time", "addr")
    img = ds.tf.shade(agg, cmap=colorcet.fire)
    img.to_pil().save(args.output)


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
