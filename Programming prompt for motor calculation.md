# Architecture of Drone Monitoring System

# Rules for programming:

Use **snake\_case** for **variable, function, and method names**. For example, pixel\_size, focal\_length, and horizontal\_sensor\_dimension.

**Constants:** Write in all capital letters with words separated by underscores (UPPER\_SNAKE\_CASE). For instance, PIXEL\_SIZE or MAX\_CONNECTIONS.

**Classes:** Use PascalCase, where each word starts with a capital letter without any separation. For example, CameraSensor or MotorParameters.

# Calculation of Requested Motor Increments for Following Flying Object

Based on AI recognition of a drone in a Python program (ai\_system\_camera), the AI will pass two floating-point variables representing the detected pixel movement:

* `horizontal_pixels_movement` (float): The detected horizontal pixel movement of the drone from the center of the camera's view. A positive value indicates movement to the right, negative to the left.  
* `vertical_pixels_movement` (float): The detected vertical pixel movement of the drone from the center of the camera's view. A positive value indicates movement upwards, negative downwards.

These pixel movements will be received by the motor\_controller C++ program, which will calculate the required motor increments to keep the drone centered in the camera's view. These calculated motor increments (expressed as integer encoder steps) will then be made available to the ethercat\_master C++ program for execution by the motors.

The calculation consists of a one-time constant calculation during program initialization, and then a live calculation of motor movements based on the detected pixel movements. The live calculation should be performed at a rate of 100 times per second.

## Calculation of Camera, Sensor, and Motors Constants (Initialization Phase)

Constants will be stored in a `config.ini` file. This file is located in the same directory as the program and already exists with the following content:

```
# Camera, sensor, and motor constants:

# CAMERA PARAMETERS
# Resolution (in pixels) for SONY IMX571 APS-C sensor
HORIZONTAL_RESOLUTION = 6224           # pixels (int)
VERTICAL_RESOLUTION = 4168             # pixels (int)
PIXEL_SIZE = 0.00376                   # mm (float)
HORIZONTAL_SENSOR_DIMENSION = 23.40224 # mm (float)
VERTICAL_SENSOR_DIMENSION = 15.67168   # mm (float)
FOCAL_LENGTH = 100                     # mm (float)

# MOTOR PARAMETERS
# Horizontal motor
ENCODER_HORIZONTAL_MOTOR = 262144      # steps per revolution (int)
HORIZONTAL_ENCODER_MULTIPLIER = 4      # multiplier (int)
HORIZONTAL_MOTOR_GEARBOX_RATIO = 80    # ratio (int)
HORIZONTAL_MOTOR_NAME = axis_1002      # name of the motor for horizontal movement in ethercat_master program (string)

# Vertical motor
ENCODER_VERTICAL_MOTOR = 262144        # steps per revolution (int)
VERTICAL_ENCODER_MULTIPLIER = 4        # multiplier (int)
VERTICAL_MOTOR_GEARBOX_RATIO = 80      # ratio (int)
VERTICAL_MOTOR_NAME = axis_1001        # name of the motor for vertical movement in ethercat_master program (string)
```

During program initialization, the following constants must be calculated once using the values from `config.ini`. These calculated values are fundamental for converting pixel movements into motor increments:

1. **Angle of View of the Camera (in degrees):**  
   * `CAMERA_ANGLE_HORIZONTAL` (float)  
   * `CAMERA_ANGLE_VERTICAL` (float)  
2. Formulas:

```
CAMERA_ANGLE_HORIZONTAL = 2 * DEGREES(ATAN(HORIZONTAL_SENSOR_DIMENSION / (2 * FOCAL_LENGTH)))
CAMERA_ANGLE_VERTICAL = 2 * DEGREES(ATAN(VERTICAL_SENSOR_DIMENSION / (2 * FOCAL_LENGTH)))
```

4. *Note: `DEGREES` is a function to convert radians to degrees, and `ATAN` is the arctangent function (e.g., `std::atan` or `std::atan2` in C++'s `<cmath>` library. Use `M_PI` for Pi if available, or define it as `3.14159265358979323846`).*  
5. **Number of Encoder Increments per Pixel of Camera Resolution:**  
   * `HORIZONTAL_INCREMENTS_PER_PIXEL` (float)  
   * `VERTICAL_INCREMENTS_PER_PIXEL` (float)  
6. Formulas:

```
HORIZONTAL_INCREMENTS_PER_PIXEL = (ENCODER_HORIZONTAL_MOTOR * HORIZONTAL_ENCODER_MULTIPLIER * HORIZONTAL_MOTOR_GEARBOX_RATIO) / (360 * (HORIZONTAL_RESOLUTION / CAMERA_ANGLE_HORIZONTAL))
VERTICAL_INCREMENTS_PER_PIXEL = (ENCODER_VERTICAL_MOTOR * VERTICAL_ENCODER_MULTIPLIER * VERTICAL_MOTOR_GEARBOX_RATIO) / (360 * (VERTICAL_RESOLUTION / CAMERA_ANGLE_VERTICAL))
```

## Live Calculation of Motor Movements (Runtime Phase)

The `motor_controller` C++ program will continuously receive the `horizontal_pixels_movement` and `vertical_pixels_movement` floating-point variables from the Python program `ai_system_camera`. Based on these inputs and the pre-calculated constants, the `motor_controller` will calculate the total motor encoder increments required:

* `horizontal_motor_movement` (int): Total encoder increments for the horizontal motor.  
* `vertical_motor_movement` (int): Total encoder increments for the vertical motor.

Formulas:

```
horizontal_motor_movement = static_cast<int>(std::round(horizontal_pixels_movement * HORIZONTAL_INCREMENTS_PER_PIXEL));
vertical_motor_movement = static_cast<int>(std::round(vertical_pixels_movement * VERTICAL_INCREMENTS_PER_PIXEL));
```

The `motor_controller` program will then make these `horizontal_motor_movement` and `vertical_motor_movement` integer increments available to the `ethercat_master` C++ program. This operation should repeat online up to 100 times per second.-----**Data Exchange Models (All programs on NVIDIA Jetson AGX Orin Developer Kit)**

1. **From Python (ai\_system\_camera) to C++ (motor\_controller):**  
   * **Model:** Shared Memory  
   * **Implementation:** Both the Python `ai_system_camera` program and the C++ `motor_controller` program will access a designated shared memory segment.  
   * **Data Structure:** A small, fixed-size C-compatible struct (e.g., `struct PixelMovement { float horizontal; float vertical; };`) should be defined and used for data exchange in the shared memory segment.  
   * **Synchronization:** To prevent race conditions and ensure data integrity, a synchronization mechanism (e.g., a mutex or semaphore) must be used to control access to the shared memory segment. The Python program will write to the shared memory, and the C++ program will read from it.  
   * **Python Libraries:** `mmap` module for shared memory, `threading` or `multiprocessing` for locks.  
   * **C++ Libraries:** POSIX shared memory (`shm_open`, `mmap`, `sem_open`)  
2. **From C++ (motor\_controller) to C++ (ethercat\_master):**  
   * **Model:** Function Calls (motor\_controller as a library)  
   * **Implementation:** The `motor_controller` functionality should be encapsulated as a C++ library. The `ethercat_master` program is an existing C++ application, and it will link with and call functions from the `motor_controller` library to retrieve the latest calculated `horizontal_motor_movement` and `vertical_motor_movement` values.  
   * **Integration with `ethercat_master`:** The `ethercat_master` program will be modified to periodically (e.g., at 100 Hz, consistent with the calculation rate) call a specific function(s) within the `motor_controller` library to obtain the current motor increments.  
   * **Motor Naming and Pairing:** The `ethercat_master` program should use the `HORIZONTAL_MOTOR_NAME` and `VERTICAL_MOTOR_NAME` strings loaded from `config.ini` to identify and control the respective motor axes. The `motor_controller` library will provide the calculated `horizontal_motor_movement` paired with `HORIZONTAL_MOTOR_NAME`, and `vertical_motor_movement` paired

# Detailed outline of the testing programs for the `ai_system_camera` (Python) and the modifications to the `ethercat_master` (C++) program.

**1\. Python Test Program (`ai_system_camera` modifications):**

This program will be responsible for generating a predefined sequence of `horizontal_pixels_movement` and `vertical_pixels_movement` data and streaming it to the C++ `motor_controller` program via shared memory.

* **Data Generation Rate:** The program should generate new data points every 0.03 seconds, which is approximately 33.3 Hz.  
* **Initial State:** Both `horizontal_pixels_movement` and `vertical_pixels_movement` should start at `0`.  
* **Movement Sequence:**  
  * **First 60 Steps (Outwards Movement):** For the initial 60 data points, the values should increment:  
    * `horizontal_pixels_movement = previous_horizontal_value + 100`  
    * `vertical_pixels_movement = previous_vertical_value + 70`  
  * **Next 60 Steps (Return to Origin):** For the subsequent 60 data points, the values should decrement to return to the starting position:  
    * `horizontal_pixels_movement = previous_horizontal_value - 100`  
    * `vertical_pixels_movement = previous_vertical_value - 70`  
* **Data Streaming:** The generated values will be continuously written to the designated shared memory segment, where the C++ `motor_controller` program will read them.  
* **Shared Memory Interaction:** The Python program will need to utilize libraries like `mmap` for shared memory access and `threading` or `multiprocessing` for synchronization mechanisms (like locks) to ensure safe data exchange.

**2\. C++ Test Program (`ethercat_master` modifications):**

This program will incorporate a testing protocol to interact with the motors based on the data received from the `motor_controller` library and user input.

* **Initialization and Home Position Check:**  
  * Upon starting the test, the `ethercat_master` program should command both motors (identified by `HORIZONTAL_MOTOR_NAME` and `VERTICAL_MOTOR_NAME` from `config.ini`) to their home position (0 increments).  
  * The program should wait for 10 seconds to allow the motors to reach their home position.  
  * After 10 seconds, it should check if the encoder readout position for both axes is within `0 ± 100` increments.  
* **Test Start:** The test sequence will only begin after the user presses the keyboard key "s" (for start).  
* **Data Acquisition:** The `ethercat_master` program will periodically (at 100 Hz, matching the `motor_controller` calculation rate) call functions within the `motor_controller` library to retrieve the latest calculated `horizontal_motor_movement` and `vertical_motor_movement` values.  
* **Motor Control:** Using the retrieved motor increments and the motor names from `config.ini`, the `ethercat_master` program will send commands to control the respective motor axes.  
* **Test Termination:** When the user presses the keyboard key "e" (for end of the program), the `ethercat_master` program should command both motor axes to return to their home position (0 increments).  
* **Library Integration:** The `ethercat_master` program will need to be modified to link with and call functions from the `motor_controller` library.

