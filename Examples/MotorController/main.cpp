#include "../EcMasterDemoMotion/MotorController.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <string>

struct IncrementsShm { uint32_t seq; int32_t horiz_inc; int32_t vert_inc; };

static int open_or_create_shm(const char* name, size_t size, int oflags)
{
    int fd = shm_open(name, oflags, 0666);
    if (fd < 0) {
        fd = shm_open(name, O_CREAT | oflags, 0666);
        if (fd < 0) return -1;
        if (ftruncate(fd, (off_t)size) != 0) { close(fd); return -1; }
    }
    struct stat st{};
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    if (st.st_size < (off_t)size) {
        if (ftruncate(fd, (off_t)size) != 0) { close(fd); return -1; }
    }
    return fd;
}

static std::string get_exe_dir()
{
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) return std::string(".");
    path[n] = '\0';
    // strip filename
    for (ssize_t i = n - 1; i >= 0; --i) {
        if (path[i] == '/') { path[i] = '\0'; break; }
    }
    return std::string(path);
}

int main()
{
    // Resolve absolute config.ini next to the executable to avoid cwd issues
    std::string exe_dir = get_exe_dir();
    std::string cfg_path = exe_dir + "/config.ini";
    struct stat st{};
    if (stat(cfg_path.c_str(), &st) != 0) {
        std::fprintf(stderr, "MotorController: config not found at %s (errno=%d)\n", cfg_path.c_str(), errno);
    } else {
        std::fprintf(stdout, "MotorController: using config %s (%lld bytes)\n", cfg_path.c_str(), (long long)st.st_size);
        std::fflush(stdout);
    }
    MotorController mc;
    if (!mc.init(cfg_path)) {
        std::fprintf(stderr, "MotorController: failed to init (%s missing or invalid)\n", cfg_path.c_str());
        return 1;
    }

    const char* out_name = "/motor_increments";
    int out_fd = open_or_create_shm(out_name, sizeof(IncrementsShm), O_RDWR);
    if (out_fd < 0) {
        std::fprintf(stderr, "MotorController: failed to open shm %s\n", out_name);
        return 2;
    }
    void* out_ptr = mmap(nullptr, sizeof(IncrementsShm), PROT_READ | PROT_WRITE, MAP_SHARED, out_fd, 0);
    if (out_ptr == MAP_FAILED) {
        std::fprintf(stderr, "MotorController: mmap failed\n");
        return 3;
    }
    IncrementsShm* out = reinterpret_cast<IncrementsShm*>(out_ptr);

    // Initialize
    out->seq = 0; out->horiz_inc = 0; out->vert_inc = 0;

    // Main loop ~100 Hz
    while (true) {
        // Read pixel movements from Python shm and compute increments (if new)
        if (mc.update()) {
            int32_t hi = 0, vi = 0;
            if (mc.get_last_increments(hi, vi)) {
                // Write in one shot (best-effort)
                uint32_t new_seq = out->seq + 1;
                IncrementsShm snap{ new_seq, hi, vi };
                std::memcpy(out, &snap, sizeof(snap));
                std::printf("MC: seq=%u horiz_inc=%d vert_inc=%d\n", new_seq, hi, vi);
                std::fflush(stdout);
            }
        }
        usleep(10000); // ~100 Hz
    }

    return 0;
}


