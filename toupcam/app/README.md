Build

- Dependencies: Qt6 (Widgets, OpenGLWidgets, Svg), CMake >= 3.18, a C++20 compiler.
- SDK: uses libtoupcam.so from toupcam/toupcamsdk/linux/arm64.

Steps

1. sudo cp /workspace/toupcam/toupcamsdk/linux/udev/99-toupcam.rules /etc/udev/rules.d/
2. sudo udevadm control --reload && sudo udevadm trigger
3. mkdir -p /workspace/toupcam/app/build && cd /workspace/toupcam/app/build
4. cmake -DCMAKE_BUILD_TYPE=Release ..
5. cmake --build . -j

Run

- Ensure camera is connected.
- Optional: grant RT scheduling capability: sudo setcap cap_sys_nice+ep /workspace/toupcam/app/build/ToupcamApp
- Execute: ./ToupcamApp

Data

- Photos: /workspace/toupcam/app/photos/IMG_N.raw + IMG_N.json
- Recordings: /workspace/toupcam/app/recordings/IMG_N.raw + IMG_N.json
- Numbering persists across runs by scanning both folders.