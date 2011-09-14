#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vi:ts=4 sw=4 et

# How to use:
# ./something_that_generates_points | ./draw_points.py
# ./draw_points.py < some_data.txt
#
# Once the window is opened, it can be resized, or it can be closed by pressing
# Esc or Q, or by just closing it.
#
# Upon reading EOF, it will stop reading from stdin, but the window will remain
# open until you close it.


from __future__ import division
from __future__ import print_function

import select
import sys
from itertools import izip

import pygame
from pygame.locals import *


class DrawPoints(object):

    def init(self):
        pygame.init()

        self.MAX_POINTS = 256
        self.colors = [
            Color(i,i,i) for i in range(256)
        ]
        self.points = [ (None, None) ] * self.MAX_POINTS

        self.resolution = (640, 480)
        self.thickness = 4

        self.poll = select.poll()
        self.poll.register(sys.stdin, select.POLLIN)


    def run(self):
        self.screen = pygame.display.set_mode(self.resolution, RESIZABLE)
        pygame.display.set_caption("Drawing points from stdin")

        # Inject a USEREVENT every 10ms
        pygame.time.set_timer(USEREVENT, 10)

        while True:
            self.redraw()

            for event in [pygame.event.wait(),]+pygame.event.get():
                if event.type == QUIT:
                    self.quit()
                elif event.type == KEYDOWN and event.key in [K_ESCAPE, K_q]:
                    self.quit()

                elif event.type == VIDEORESIZE:
                    self.resolution = event.size
                    self.screen = pygame.display.set_mode(self.resolution, RESIZABLE)

                elif event.type == USEREVENT:
                    # poll() returns a list of file-descriptors that are "ready"
                    while self.poll.poll(0):
                        line = sys.stdin.readline()
                        if line == "":
                            # EOF
                            self.poll.unregister(sys.stdin)
                            pygame.time.set_timer(USEREVENT, 0)
                        try:
                            x,y = [float(i) for i in line.strip().split()]
                        except:
                            continue
                        self.add_point(x,y)


    def add_point(self, x, y):
        self.points.append( (x,y) )
        if len(self.points) > self.MAX_POINTS:
            self.points.pop(0)


    def redraw(self):
        self.screen.fill(Color(0,0,0))
        for point, color in izip(self.points, self.colors):
            if point[0] is None or point[1] is None:
                continue
            x = point[0] * self.resolution[0]
            y = point[1] * self.resolution[1]
            rect = Rect(
                x - self.thickness, y - self.thickness,
                1 + 2 * self.thickness, 1 + 2 * self.thickness
            )
            self.screen.fill(color, rect=rect)

        pygame.display.flip()


    def quit(self, status=0):
        pygame.quit()
        sys.exit(status)


if __name__ == "__main__":
    program = DrawPoints()
    program.init()
    program.run()  # This function should never return
