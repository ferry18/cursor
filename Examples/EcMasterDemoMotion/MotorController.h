// MotorController.h
// Minimal motor controller component embedded for EcMasterDemoMotion.
// - Reads camera/motor constants from config.ini (same dir as executable)
// - Computes camera FOV and increments-per-pixel
// - Receives pixel movements from a Python producer via POSIX shared memory
// - Produces per-axis encoder increment commands at ~100 Hz

#pragma once

#include <cstdint>
#include <string>

class MotorController {
public:
    struct PixelMovement {
        float horizontal;
        float vertical;
    };

    MotorController();
    ~MotorController();

    // Initialize: parse config.ini, compute constants, set up shared memory and semaphore
    // ini_path: default "config.ini" searched relative to current working directory.
    bool init(const std::string& ini_path = std::string("config.ini"));

    // Poll shared memory; if new data available (and 10 ms elapsed since last publish),
    // compute encoder increments and store them. Returns true if new increments are ready.
    bool update();

    // Retrieve latest computed increments. Returns false if no update since last call.
    bool get_last_increments(int32_t& horizontal_increments, int32_t& vertical_increments);

    // Access computed constants (for diagnostics)
    double camera_angle_horizontal_deg() const { return camera_angle_horizontal_deg_; }
    double camera_angle_vertical_deg() const { return camera_angle_vertical_deg_; }
    double horiz_increments_per_pixel() const { return horizontal_increments_per_pixel_; }
    double vert_increments_per_pixel() const { return vertical_increments_per_pixel_; }
    const std::string& horizontal_motor_name() const { return horizontal_motor_name_; }
    const std::string& vertical_motor_name() const { return vertical_motor_name_; }

private:
    // Configuration values
    // Camera
    int32_t horizontal_resolution_ = 0;
    int32_t vertical_resolution_ = 0;
    double pixel_size_mm_ = 0.0;
    double horizontal_sensor_dimension_mm_ = 0.0;
    double vertical_sensor_dimension_mm_ = 0.0;
    double focal_length_mm_ = 0.0;

    // Motors
    int32_t encoder_horizontal_motor_steps_per_rev_ = 0;
    int32_t horizontal_encoder_multiplier_ = 0;
    int32_t horizontal_motor_gearbox_ratio_ = 0;
    std::string horizontal_motor_name_;

    int32_t encoder_vertical_motor_steps_per_rev_ = 0;
    int32_t vertical_encoder_multiplier_ = 0;
    int32_t vertical_motor_gearbox_ratio_ = 0;
    std::string vertical_motor_name_;

    // Computed constants
    double camera_angle_horizontal_deg_ = 0.0;
    double camera_angle_vertical_deg_ = 0.0;
    double horizontal_increments_per_pixel_ = 0.0;
    double vertical_increments_per_pixel_ = 0.0;

    // Shared memory
    int shm_fd_ = -1;
    void* shm_ptr_ = nullptr;
    size_t shm_size_ = 0;
    void* sem_handle_ = nullptr; // opaque pointer to sem_t to avoid exposing <semaphore.h> in header
    uint32_t last_seq_ = 0;

    // Throttle to ~100 Hz
    uint64_t last_publish_ns_ = 0;

    // Latest outputs
    int32_t last_horizontal_increments_ = 0;
    int32_t last_vertical_increments_ = 0;
    bool has_new_output_ = false;

    // Helpers
    bool parse_config_ini(const std::string& path);
    void compute_constants();
    bool setup_shared_memory();
    static uint64_t monotonic_time_ns();
};


