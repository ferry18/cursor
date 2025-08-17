import curses
import mmap
import os
import struct
import time

SHM_POS = '/axis_positions'
SHM_SET = '/axis_setpoints'

POS_FMT = '<Iii'  # seq, horiz_pos, vert_pos
POS_SIZE = struct.calcsize(POS_FMT)

SET_FMT = '<Iii'  # seq, horiz_abs, vert_abs (increments)
SET_SIZE = struct.calcsize(SET_FMT)

H_STEP = 200000.0  # increments per tick while holding (10x larger)
V_STEP = 200000.0
HOLD_TIMEOUT_S = 0.08  # quicker release detection

def open_shm_rw(name: str, size: int) -> mmap.mmap:
    path = '/dev/shm' + name
    fd = os.open(path, os.O_CREAT | os.O_RDWR, 0o666)
    try:
        os.ftruncate(fd, size)
        mm = mmap.mmap(fd, size)
    finally:
        os.close(fd)
    return mm

def open_shm_ro(name: str, size: int) -> mmap.mmap:
    path = '/dev/shm' + name
    fd = os.open(path, os.O_RDONLY, 0o666)
    try:
        mm = mmap.mmap(fd, size, access=mmap.ACCESS_READ)
    finally:
        os.close(fd)
    return mm

def service(stdscr):
    curses.curs_set(0)
    curses.noecho()
    stdscr.keypad(True)
    stdscr.nodelay(True)
    stdscr.timeout(50)

    # Open shared memories
    try:
        pos_mm = open_shm_ro(SHM_POS, POS_SIZE)
    except Exception:
        pos_mm = None
    set_mm = open_shm_rw(SHM_SET, SET_SIZE)

    seq = 0

    # Hold state
    h_dir = 0.0
    v_dir = 0.0
    last_event_t = time.monotonic()

    prev_active = False
    while True:
        stdscr.erase()
        stdscr.addstr(0, 0, 'Service Menu (Arrow keys to jog and hold; q to quit)')
        # If positions shm wasn't ready at start, retry opening
        if pos_mm is None:
            try:
                pos_mm = open_shm_ro(SHM_POS, POS_SIZE)
            except Exception:
                pos_mm = None

        # Read positions
        horiz_pos = 0
        vert_pos = 0
        if pos_mm:
            try:
                pos_mm.seek(0)
                data1 = pos_mm.read(POS_SIZE)
                pos_mm.seek(0)
                data2 = pos_mm.read(POS_SIZE)
                if data1 == data2 and len(data1) == POS_SIZE:
                    _, horiz_pos, vert_pos = struct.unpack(POS_FMT, data1)
            except Exception:
                pass

        stdscr.addstr(2, 0, f'Horizontal (axis 1002) actual: {horiz_pos}   step={int(H_STEP)}')
        stdscr.addstr(3, 0, f'Vertical   (axis 1001) actual: {vert_pos}   step={int(V_STEP)}')

        key = stdscr.getch()
        now = time.monotonic()

        # Update hold direction based on key events
        if key in (curses.KEY_LEFT, ord('a'), ord('h')):
            h_dir = -1.0
            last_event_t = now
        elif key in (curses.KEY_RIGHT, ord('d'), ord('l')):
            h_dir = 1.0
            last_event_t = now
        elif key in (curses.KEY_UP, ord('w'), ord('k')):
            v_dir = 1.0
            last_event_t = now
        elif key in (curses.KEY_DOWN, ord('s'), ord('j')):
            v_dir = -1.0
            last_event_t = now
        elif key in (ord('q'), ord('Q')):
            break
        elif key != -1:
            # any other key releases both
            h_dir = 0.0
            v_dir = 0.0
            last_event_t = now

        # If no key activity recently, treat as release
        released_now = False
        if (now - last_event_t) > HOLD_TIMEOUT_S:
            if h_dir != 0.0 or v_dir != 0.0:
                released_now = True
            h_dir = 0.0
            v_dir = 0.0

        # Apply movement while holding
        # Compute absolute targets based on current actual positions and velocity per tick
        # Read current (best effort)
        horiz_pos = horiz_pos  # from above
        vert_pos = vert_pos
        horiz_target = horiz_pos
        vert_target = vert_pos
        if h_dir != 0.0:
            horiz_target = int(horiz_pos + (h_dir * H_STEP))
        if v_dir != 0.0:
            vert_target = int(vert_pos + (v_dir * V_STEP))
        seq += 1
        # Write absolute setpoints (seq, horiz_abs, vert_abs)
        set_mm.seek(0)
        set_mm.write(struct.pack(SET_FMT, seq, horiz_target, vert_target))
        set_mm.flush()

        # On release, reset offsets to 0 to stop immediately
        if released_now:
            # On release, hold the current actual position as target to stop immediately
            seq += 1
            set_mm.seek(0)
            set_mm.write(struct.pack(SET_FMT, seq, int(horiz_pos), int(vert_pos)))
            set_mm.flush()

        stdscr.addstr(5, 0, f'Absolute targets: H={horiz_target}  V={vert_target}  seq={seq}')
        stdscr.addstr(7, 0, 'Keys: Arrows/WASD/HJKL to jog, q to quit')
        stdscr.refresh()
        time.sleep(0.05)

def main():
    curses.wrapper(service)
    # Return to caller (base menu) without stopping anything

if __name__ == '__main__':
    main()


