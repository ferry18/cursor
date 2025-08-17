import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
REL = os.path.join(ROOT, 'Bin', 'Linux', 'aarch64', 'Release')

def start_all():
    # Start camera-driven mode: Toupcam UI + motor_controller; start demo under sudo
    cfg_src = os.path.join(ROOT, 'Examples', 'EcMasterDemoMotion', 'config.ini')
    cfg_dst = os.path.join(REL, 'config.ini')
    try:
        os.makedirs(REL, exist_ok=True)
        with open(cfg_src, 'rb') as fsrc, open(cfg_dst, 'wb') as fdst:
            fdst.write(fsrc.read())
    except Exception:
        pass
    # Clear any service setpoints to hand control to camera
    try:
        os.remove('/dev/shm/axis_setpoints')
    except FileNotFoundError:
        pass
    # Ensure any old generator or camera app is not running
    stop_generator()
    # Start motor_controller
    subprocess.Popen([os.path.join(REL, 'motor_controller')],
                     cwd=REL,
                     stdout=open(os.path.join(REL, 'motor_controller.log'), 'ab'),
                     stderr=subprocess.STDOUT)
    # Start demo automatically with sudo (will prompt for password)
    if not _pgrep('/EcMasterDemoMotion( |$)'):
        print('Starting EcMasterDemoMotion under sudo...')
        try:
            subprocess.call(['sudo', 'bash', '-lc', f"cd '{REL}' && nohup ./EcMasterDemoMotion DemoConfig.xml >> EcMasterDemoMotion.log 2>&1 & echo $! > EcMasterDemoMotion.pid" ])
        except Exception as e:
            print(f'Could not start demo automatically: {e}')
    else:
        print('EcMasterDemoMotion already running.')
    # Start Toupcam UI (runs in background; GUI window will appear)
    toupcam_main = os.path.join(ROOT, 'toupcam', 'app', 'main.py')
    try:
        # Use system python3 which has Tk support (/_tkinter present)
        cam_proc = subprocess.Popen(['/usr/bin/python3', toupcam_main],
                                    stdout=open(os.path.join(REL, 'toupcam.log'), 'ab'),
                                    stderr=subprocess.STDOUT)
        print(f'Started camera UI (pid {cam_proc.pid}).')
    except Exception as e:
        print(f'Could not start camera UI: {e}')
    print('Camera UI, controller, and demo are starting. Logs in:')
    print(f'  {os.path.join(REL, "toupcam.log")}')
    print(f'  {os.path.join(REL, "motor_controller.log")}')
    print(f'  {os.path.join(REL, "EcMasterDemoMotion.log")}')

def _pgrep(pattern: str) -> bool:
    try:
        out = subprocess.check_output(['pgrep', '-f', pattern])
        return bool(out.strip())
    except subprocess.CalledProcessError:
        return False

def stop_generator():
    # Stop only the camera/generator publishers
    try:
        subprocess.call(['pkill', '-f', '/ai_system_camera.py'])
    except Exception:
        pass
    try:
        # Stop Toupcam app main
        toupcam_main = os.path.join(ROOT, 'toupcam', 'app', 'main.py')
        subprocess.call(['pkill', '-f', toupcam_main])
    except Exception:
        pass

def total_stop():
    # Ensure camera/generator is stopped as well
    stop_generator()
    subprocess.call(['bash', os.path.join(ROOT, 'Examples', 'EcMasterDemoMotion', 'stop_follow.sh')])

def service_menu():
    # Service mode: ensure motor_controller and EcMasterDemoMotion are available, but DO NOT run generator
    # Stop generator/camera if running to avoid interfering with manual jog
    if _pgrep('/ai_system_camera.py'):
        stop_generator()
    else:
        # Also ensure Toupcam app is not running
        toupcam_main = os.path.join(ROOT, 'toupcam', 'app', 'main.py')
        if _pgrep(toupcam_main):
            stop_generator()
    # Ensure config.ini is present for the controller (copied to release dir)
    cfg_src = os.path.join(ROOT, 'Examples', 'EcMasterDemoMotion', 'config.ini')
    cfg_dst = os.path.join(REL, 'config.ini')
    try:
        os.makedirs(REL, exist_ok=True)
        with open(cfg_src, 'rb') as fsrc, open(cfg_dst, 'wb') as fdst:
            fdst.write(fsrc.read())
    except Exception:
        pass
    # Ensure controller is running
    if not _pgrep('/motor_controller'):
        try:
            ctrl_proc = subprocess.Popen([os.path.join(REL, 'motor_controller')],
                                         cwd=REL,
                                         stdout=open(os.path.join(REL, 'motor_controller.log'), 'ab'),
                                         stderr=subprocess.STDOUT)
            print(f'Started motor_controller (pid {ctrl_proc.pid}).')
            time.sleep(0.2)
        except Exception:
            pass
    # Ensure demo is running (auto-start under sudo if needed)
    if not _pgrep('/EcMasterDemoMotion( |$)'):
        print('Starting EcMasterDemoMotion under sudo...')
        try:
            subprocess.call(['sudo', 'bash', '-lc', f"cd '{REL}' && nohup ./EcMasterDemoMotion DemoConfig.xml >> EcMasterDemoMotion.log 2>&1 & echo $! > EcMasterDemoMotion.pid" ])
            time.sleep(0.5)
        except Exception as e:
            print(f'Could not start demo automatically: {e}')
    else:
        print('EcMasterDemoMotion already running.')
    # Launch service and return when exited
    subprocess.call([sys.executable, os.path.join(ROOT, 'Examples', 'EcMasterDemoMotion', 'service_menu.py')])

def main():
    while True:
        print('Base Menu:')
        print('1) Start Camera-driven Mode (Toupcam + controller + demo)')
        print('2) Stop camera (only)')
        print('3) Service menu')
        print('4) Total stop of all programs')
        print('q) Quit')
        choice = input('Select: ').strip().lower()
        if choice == '1':
            start_all()
        elif choice == '2':
            stop_generator()
        elif choice == '3':
            service_menu()
        elif choice == '4':
            total_stop()
        elif choice == 'q':
            break

if __name__ == '__main__':
    main()


