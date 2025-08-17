#!/usr/bin/env python3
"""
Python generator stub for shared memory test.
Writes PixelMovement {horizontal, vertical} at ~33 Hz as described.

This is a helper for testing on target (Jetson). It uses posix_ipc + mmap or
falls back to /dev/shm with mmap module only.
"""
import mmap
import os
import struct
import sys
import time

SHM_NAME = '/ai_pixel_movement'
SEM_NAME = '/ai_pixel_movement_sem'

LAYOUT_FORMAT = '<Iff'  # uint32 seq, float horizontal, float vertical
SIZE = struct.calcsize(LAYOUT_FORMAT)

def main():
    # Open or create shm
    fd = os.open('/dev/shm' + SHM_NAME, os.O_CREAT | os.O_RDWR, 0o666)
    os.ftruncate(fd, SIZE)
    shm = mmap.mmap(fd, SIZE)
    os.close(fd)

    seq = 0
    horiz = 0.0
    vert = 0.0

    # Generate 60 outward steps then 60 inward steps
    try:
        for _ in range(60):
            seq += 1
            horiz += 100.0
            vert += 70.0
            shm.seek(0)
            shm.write(struct.pack(LAYOUT_FORMAT, seq, horiz, vert))
            shm.flush()
            time.sleep(0.03)
        for _ in range(60):
            seq += 1
            horiz -= 100.0
            vert -= 70.0
            shm.seek(0)
            shm.write(struct.pack(LAYOUT_FORMAT, seq, horiz, vert))
            shm.flush()
            time.sleep(0.03)
    finally:
        shm.close()

if __name__ == '__main__':
    sys.exit(main())


