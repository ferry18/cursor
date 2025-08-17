#!/usr/bin/env python3
"""
ai_system_camera.py

Generates a predefined sequence of horizontal_pixels_movement and
vertical_pixels_movement values and writes them into a POSIX shared memory
segment for a C++ consumer.

Spec:
- Rate: 100 Hz
- Initial values: 0, 0
- 60 steps outwards: +500 (horizontal), +350 (vertical) each step (5x longer travel)
- 60 steps return:  -500 (horizontal), -350 (vertical) each step
- Random deviation: up to ±3% applied independently to each coordinate per frame
- Shared memory layout: uint32 seq, float horizontal, float vertical

Synchronization: Uses a simple sequence number. For robust writers, we write
seq after floats to reduce transient inconsistency.
"""
import mmap
import os
import struct
import time
from typing import Tuple
import random

SHM_NAME = '/ai_pixel_movement'
LAYOUT_FORMAT = '<Iff'  # uint32 seq, float horizontal, float vertical
SIZE = struct.calcsize(LAYOUT_FORMAT)

def open_shm(name: str, size: int) -> mmap.mmap:
    path = '/dev/shm' + name
    fd = os.open(path, os.O_CREAT | os.O_RDWR, 0o666)
    try:
        os.ftruncate(fd, size)
        mm = mmap.mmap(fd, size)
    finally:
        os.close(fd)
    return mm

def write_frame(mm: mmap.mmap, seq: int, horiz: float, vert: float) -> None:
    # Write floats first (offset 4), then seq at offset 0 to help lockless readers
    mm.seek(4)
    mm.write(struct.pack('<ff', horiz, vert))
    mm.seek(0)
    mm.write(struct.pack('<I', seq))
    mm.flush()

def main() -> int:
    mm = open_shm(SHM_NAME, SIZE)
    seq = 0

    # 100 Hz update rate
    interval_s = 0.01
    # Step sizes scaled by 5x
    H_STEP = 500.0
    V_STEP = 350.0
    # Random deviation up to ±3%
    rng = random.SystemRandom()

    while True:
        # Initial write
        write_frame(mm, seq, 0.0, 0.0)
        time.sleep(interval_s)

        horiz = 0.0
        vert = 0.0

        # 60 outward steps
        for _ in range(60):
            seq += 1
            horiz += H_STEP
            vert += V_STEP
            # Apply per-frame random deviation (±3%) to the reported coordinates
            dev_h = 1.0 + rng.uniform(-0.03, 0.03)
            dev_v = 1.0 + rng.uniform(-0.03, 0.03)
            write_frame(mm, seq, horiz * dev_h, vert * dev_v)
            if (seq % 10) == 0:
                print(f"seq={seq} horiz={horiz:.1f} vert={vert:.1f}")
            time.sleep(interval_s)

        # 60 return steps
        for _ in range(60):
            seq += 1
            horiz -= H_STEP
            vert -= V_STEP
            dev_h = 1.0 + rng.uniform(-0.03, 0.03)
            dev_v = 1.0 + rng.uniform(-0.03, 0.03)
            write_frame(mm, seq, horiz * dev_h, vert * dev_v)
            if (seq % 10) == 0:
                print(f"seq={seq} horiz={horiz:.1f} vert={vert:.1f}")
            time.sleep(interval_s)

        # Pause before repeating sequence
        time.sleep(2.0)

if __name__ == '__main__':
    raise SystemExit(main())


