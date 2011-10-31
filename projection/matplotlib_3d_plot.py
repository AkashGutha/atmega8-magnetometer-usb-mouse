#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vi:ts=4 sw=4 et

from __future__ import division
from __future__ import print_function

import sys

import matplotlib
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import pyplot

import numpy
#from numpy import linspace


class Data(object):
    def __init__(self):
        self.x = []
        self.y = []
        self.z = []


def load_data(f):
    data = Data()

    for line in f:
        try:
            x, y, z = [float(i) for i in line.strip().split()]
            data.x.append(x)
            data.y.append(y)
            data.z.append(z)
        except:
            pass

    return data


def parse_args():
    import argparse

    parser = argparse.ArgumentParser(
        description='Uses matplotlib to draw a 3D plot.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        '-l', '--line',
        action='store_true',
        help='Draw a 3D line'
    )
    parser.add_argument(
        '-p', '--points',
        action='store_true',
        help='Draw 3D points (scatter plot)'
    )
    parser.add_argument(
        '-x', '--limit',
        metavar='LIMIT',
        action='store',
        type=int,
        default=250,
        help='Specify the limit of axes'
    )
    parser.add_argument(
        '-q', '--quiet',
        action='store_true',
        help='Don\'t open the matplotlib window'
    )
    parser.add_argument(
        '-o', '--output',
        metavar='FILE',
        action='append',
        help='Save the plot to a file (can be specified multiple times)'
    )
    parser.add_argument(
        '--dpi',
        action='store',
        type=int,
        default=300,
        help='Specify the DPI when saving the plot'
    )
    parser.add_argument(
        '-f', '--fontsize',
        action='store',
        type=int,
        default=12,
        help='Specify the font size'
    )
    parser.add_argument(
        '-s', '--size',
        action='store',
        type=float,
        nargs=2,
        default=(7.0, 7.0),
        help='Size (in inches)'
    )
    parser.add_argument(
        'input_files',
        metavar='FILE',
        type=file,
        nargs='*',
        help='Optional input files'
    )
    args = parser.parse_args()

    if args.line == args.points:
        args.line = True
        args.points = False

    return args


def main():
    args = parse_args()

    # Based on "lines3d_demo.py" and "scatter3d_demo.py" from matplotlib

    # http://stackoverflow.com/questions/3899980/how-to-change-the-font-size-on-a-matplotlib-plot
    # Default font size is 12
    matplotlib.rcParams['font.size'] = args.fontsize

    fig = pyplot.figure()
    fig.set_size_inches(args.size)

    #ax = fig.gca(projection='3d')
    ax = fig.add_subplot(111, projection='3d')
    ax.grid(True)

    input_files = args.input_files
    if not args.input_files:
        input_files = [sys.stdin]

    for f in input_files:
        data = load_data(f)
        if args.points:
            # s=size, c=color, marker='o'
            ax.scatter(data.x, data.y, data.z, s=1, marker='o', linewidths=0)
        elif args.line:
            ax.plot(data.x, data.y, data.z)

    # This is buggy, as of version 1.0.1
    #ax.set_aspect('equal')

    ax.set_xlim3d(-args.limit, args.limit)
    ax.set_ylim3d(-args.limit, args.limit)
    ax.set_zlim3d(-args.limit, args.limit)

    # Rotating the view
    #ax.view_init(elev=30, azim=-60)  # default
    #ax.view_init(elev=45, azim=45)

    if args.output:
        for filename in args.output:
            pyplot.savefig(filename, dpi=args.dpi)

    if not args.quiet:
        pyplot.show()

if __name__ == '__main__':
    main()
