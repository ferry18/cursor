// MotorControllerLib.cpp - Implementation of MotorController used by the standalone motor_controller app

#include "../EcMasterDemoMotion/MotorController.h"

#include <cmath>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

namespace {
constexpr double PI_CONST = 3.14159265358979323846;
inline double rad_to_deg(double rad) { return rad * 180.0 / PI_CONST; }

inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch){ return !std::isspace(ch); }));
    return s;
}
inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch){ return !std::isspace(ch); }).base(), s.end());
    return s;
}
inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }
}

struct ShmLayout {
    uint32_t seq;
    float horizontal;
    float vertical;
};

static const char* kShmName = "/ai_pixel_movement";
// no semaphore used

MotorController::MotorController() = default;
MotorController::~MotorController() {
    if (shm_ptr_) { munmap(shm_ptr_, shm_size_); shm_ptr_ = nullptr; }
    if (shm_fd_ >= 0) { close(shm_fd_); shm_fd_ = -1; }
    if (sem_handle_) { sem_close(reinterpret_cast<sem_t*>(sem_handle_)); sem_handle_ = nullptr; }
}

bool MotorController::init(const std::string& ini_path) {
    // Try given path, then fallback to Examples/EcMasterDemoMotion/config.ini as a convenience
    if (!parse_config_ini(ini_path)) {
        std::string fallback = "../Examples/EcMasterDemoMotion/config.ini";
        (void)parse_config_ini(fallback); // if still false, compute_constants will be wrong; caller can see failure
        if (horizontal_resolution_ <= 0) return false;
    }
    compute_constants();
    if (!setup_shared_memory()) return false;
    last_publish_ns_ = 0; last_seq_ = 0; has_new_output_ = false; return true;
}

bool MotorController::parse_config_ini(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string line;
    auto to_upper = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); }); return s; };
    while (std::getline(in, line)) {
        line = trim(line); if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('='); if (pos == std::string::npos) continue;
        std::string key = to_upper(trim(line.substr(0, pos)));
        std::string value = trim(line.substr(pos + 1));
        auto hash_pos = value.find('#'); if (hash_pos != std::string::npos) value = trim(value.substr(0, hash_pos));
        if (key == "HORIZONTAL_RESOLUTION") horizontal_resolution_ = std::stoi(value);
        else if (key == "VERTICAL_RESOLUTION") vertical_resolution_ = std::stoi(value);
        else if (key == "PIXEL_SIZE") pixel_size_mm_ = std::stod(value);
        else if (key == "HORIZONTAL_SENSOR_DIMENSION") horizontal_sensor_dimension_mm_ = std::stod(value);
        else if (key == "VERTICAL_SENSOR_DIMENSION") vertical_sensor_dimension_mm_ = std::stod(value);
        else if (key == "FOCAL_LENGTH") focal_length_mm_ = std::stod(value);
        else if (key == "ENCODER_HORIZONTAL_MOTOR") encoder_horizontal_motor_steps_per_rev_ = std::stoi(value);
        else if (key == "HORIZONTAL_ENCODER_MULTIPLIER") horizontal_encoder_multiplier_ = std::stoi(value);
        else if (key == "HORIZONTAL_MOTOR_GEARBOX_RATIO") horizontal_motor_gearbox_ratio_ = std::stoi(value);
        else if (key == "HORIZONTAL_MOTOR_NAME") horizontal_motor_name_ = value;
        else if (key == "ENCODER_VERTICAL_MOTOR") encoder_vertical_motor_steps_per_rev_ = std::stoi(value);
        else if (key == "VERTICAL_ENCODER_MULTIPLIER") vertical_encoder_multiplier_ = std::stoi(value);
        else if (key == "VERTICAL_MOTOR_GEARBOX_RATIO") vertical_motor_gearbox_ratio_ = std::stoi(value);
        else if (key == "VERTICAL_MOTOR_NAME") vertical_motor_name_ = value;
    }
    bool ok = true;
    ok &= horizontal_resolution_ > 0 && vertical_resolution_ > 0;
    ok &= horizontal_sensor_dimension_mm_ > 0.0 && vertical_sensor_dimension_mm_ > 0.0;
    ok &= focal_length_mm_ > 0.0;
    ok &= encoder_horizontal_motor_steps_per_rev_ > 0;
    ok &= horizontal_encoder_multiplier_ > 0;
    ok &= horizontal_motor_gearbox_ratio_ > 0;
    ok &= encoder_vertical_motor_steps_per_rev_ > 0;
    ok &= vertical_encoder_multiplier_ > 0;
    ok &= vertical_motor_gearbox_ratio_ > 0;
    if (!ok) {
        std::fprintf(stderr,
            "MotorController: bad config values from %s\nHRES=%d VRES=%d HSD=%.6f VSD=%.6f F=%.3f EHH=%d HEM=%d HGR=%d EV=%d VEM=%d VGR=%d\n",
            path.c_str(),
            horizontal_resolution_, vertical_resolution_,
            horizontal_sensor_dimension_mm_, vertical_sensor_dimension_mm_,
            focal_length_mm_,
            encoder_horizontal_motor_steps_per_rev_, horizontal_encoder_multiplier_, horizontal_motor_gearbox_ratio_,
            encoder_vertical_motor_steps_per_rev_, vertical_encoder_multiplier_, vertical_motor_gearbox_ratio_);
    }
    return ok;
}

void MotorController::compute_constants() {
    camera_angle_horizontal_deg_ = 2.0 * rad_to_deg(std::atan(horizontal_sensor_dimension_mm_ / (2.0 * focal_length_mm_)));
    camera_angle_vertical_deg_   = 2.0 * rad_to_deg(std::atan(vertical_sensor_dimension_mm_   / (2.0 * focal_length_mm_)));
    const double incs_per_rev_h = (double)encoder_horizontal_motor_steps_per_rev_ * (double)horizontal_encoder_multiplier_ * (double)horizontal_motor_gearbox_ratio_;
    const double incs_per_rev_v = (double)encoder_vertical_motor_steps_per_rev_   * (double)vertical_encoder_multiplier_   * (double)vertical_motor_gearbox_ratio_;
    const double deg_per_pixel_h = camera_angle_horizontal_deg_ / (double)horizontal_resolution_;
    const double deg_per_pixel_v = camera_angle_vertical_deg_   / (double)vertical_resolution_;
    horizontal_increments_per_pixel_ = incs_per_rev_h / (360.0 * (1.0 / deg_per_pixel_h));
    vertical_increments_per_pixel_   = incs_per_rev_v / (360.0 * (1.0 / deg_per_pixel_v));
    std::fprintf(stdout,
        "MotorController: angles deg H=%.6f V=%.6f inc/px H=%.3f V=%.3f\n",
        camera_angle_horizontal_deg_, camera_angle_vertical_deg_, horizontal_increments_per_pixel_, vertical_increments_per_pixel_);
    std::fflush(stdout);
}

bool MotorController::setup_shared_memory() {
    shm_fd_ = shm_open(kShmName, O_RDWR, 0666);
    if (shm_fd_ < 0) {
        shm_fd_ = shm_open(kShmName, O_CREAT | O_RDWR, 0666);
        if (shm_fd_ < 0) return false;
        if (ftruncate(shm_fd_, (off_t)sizeof(ShmLayout)) != 0) return false;
    }
    struct stat st{};
    if (fstat(shm_fd_, &st) != 0) return false;
    if (st.st_size < (off_t)sizeof(ShmLayout)) { if (ftruncate(shm_fd_, (off_t)sizeof(ShmLayout)) != 0) return false; }
    shm_size_ = sizeof(ShmLayout);
    shm_ptr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
    if (shm_ptr_ == MAP_FAILED) { shm_ptr_ = nullptr; return false; }
    // Do NOT rely on semaphore, writer (Python) doesn't use it. Read lockless.
    sem_handle_ = nullptr;
    return true;
}

static uint64_t time_now_ns() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec; }

bool MotorController::update() {
    if (!shm_ptr_) return false;
    ShmLayout local{};
    // Lockless double-read
    for (int a = 0; a < 3; ++a) {
        uint32_t s1, s2; ShmLayout tmp{};
        std::memcpy(&s1, shm_ptr_, sizeof(uint32_t));
        std::memcpy(reinterpret_cast<char*>(&tmp) + sizeof(uint32_t), reinterpret_cast<char*>(shm_ptr_) + sizeof(uint32_t), sizeof(ShmLayout) - sizeof(uint32_t));
        std::memcpy(&s2, shm_ptr_, sizeof(uint32_t));
        if (s1 == s2) { tmp.seq = s1; local = tmp; break; }
        if (a == 2) return false;
    }
    if (local.seq == last_seq_) return false;
    last_seq_ = local.seq;
    const uint64_t now = time_now_ns();
    if (last_publish_ns_ != 0 && (now - last_publish_ns_) < 10'000'000ull) return false;
    last_publish_ns_ = now;
    const double hi = (double)local.horizontal * horizontal_increments_per_pixel_;
    const double vi = (double)local.vertical   * vertical_increments_per_pixel_;
    last_horizontal_increments_ = (int32_t)llround(hi);
    last_vertical_increments_   = (int32_t)llround(vi);
    has_new_output_ = true;
    return true;
}

bool MotorController::get_last_increments(int32_t& horizontal_increments, int32_t& vertical_increments) {
    if (!has_new_output_) return false;
    horizontal_increments = last_horizontal_increments_;
    vertical_increments   = last_vertical_increments_;
    has_new_output_ = false; return true;
}


