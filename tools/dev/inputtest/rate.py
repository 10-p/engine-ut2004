#!/usr/bin/env python3
"""rate.py <game.log> [<skip>]: for each watch tag of the input test channel's log, over the watch lines after the
first <skip> (default 20): the view's yaw rate (a least-squares fit of yaw on time, in yaw units and degrees per
second), the mean horizontal speed, the direction of the velocity and of the acceleration relative to the view
(degrees, 0 ahead, positive to the right; the mean of the per-update vectors in the view's frame) and the share of
updates walking."""
import math
import re
import sys

NUM = r"(-?[\d.]+)"
WATCH = re.compile(r"\w+: watch (\S+) (\d+) ([\d.]+) (-?\d+) (-?\d+) ([\d.]+) " + " ".join([NUM] * 6) + r" (\d)")


def direction(pts, k):
    """The direction of the vectors (p[k], p[k+1]) relative to each update's view yaw, in degrees."""
    fwd = right = 0.0
    for p in pts:
        yaw = p[1] * 2 * math.pi / 65536
        x, y = p[k], p[k + 1]
        fwd += x * math.cos(yaw) + y * math.sin(yaw)
        right += -x * math.sin(yaw) + y * math.cos(yaw)
    return math.degrees(math.atan2(right, fwd)) if fwd or right else float("nan")


def main():
    skip = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    tags = {}
    seen = set()
    for line in open(sys.argv[1], errors="replace"):
        m = WATCH.search(line)
        # A log echoed to stdout may carry a line twice (with and without its "Log: " prefix).
        if m and (m.group(1), m.group(2)) not in seen:
            seen.add((m.group(1), m.group(2)))
            g = m.groups()
            tags.setdefault(g[0], []).append(
                [float(g[2]), int(g[3]), float(g[5])] + [float(v) for v in g[8:12]] + [int(g[12])])
    for tag, pts in tags.items():
        pts = pts[skip:]
        n = len(pts)
        if n < 10:
            print(f"{tag}: too few watch lines ({n})")
            continue
        mt = sum(p[0] for p in pts) / n
        my = sum(p[1] for p in pts) / n
        sxx = sum((p[0] - mt) ** 2 for p in pts)
        rate = sum((p[0] - mt) * (p[1] - my) for p in pts) / sxx
        span = pts[-1][0] - pts[0][0]
        speed = sum(p[2] for p in pts) / n
        walking = sum(p[7] for p in pts) / n
        print(f"{tag}: {rate:.0f} yaw/s ({rate * 360 / 65536:.1f} deg/s), speed {speed:.0f}, "
              f"move {direction(pts, 3):.1f} deg (accel {direction(pts, 5):.1f} deg), walking {walking:.2f}, "
              f"{n} updates over {span:.2f} s")


if __name__ == "__main__":
    main()
