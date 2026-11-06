// Fox Red Labs: serve raw frames from the SK-TDA4VM as a Holoscan Sensor Bridge device.
// Uses NVIDIA's HSB emulator (Apache-2.0): BOOTP enumeration, ECB register control plane,
// RoCE v2 formatted UDP data plane over plain Linux sockets. Frames come from a file of
// back-to-back raw frames (any pixel format; the host decides what it is).
//
//   serve_frames <device_ip> <frames.raw> <frame_bytes> [--fps N] [--limit N]
//
// Based on NVIDIA's examples/serve_linux_file.cpp, without the CUDA utility library.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "dlpack/dlpack.h"
#include "hololink/emulation/hsb_config.hpp"
#include "hololink/emulation/linux_data_plane.hpp"

using namespace hololink::emulation;

int main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s <device_ip> <frames.raw> <frame_bytes> [--fps N] [--limit N]\n", argv[0]);
        return 2;
    }
    std::string ip = argv[1];
    std::string path = argv[2];
    int64_t frame_bytes = atoll(argv[3]);
    unsigned fps = 30, limit = 0;
    for (int i = 4; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--fps")) fps = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--limit")) limit = atoi(argv[i + 1]);
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); return 1; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if ((int64_t)data.size() < frame_bytes) { fprintf(stderr, "file smaller than one frame\n"); return 1; }
    size_t frames_in_file = data.size() / frame_bytes;

    HSBEmulator hsb; // HSB_EMULATOR_CONFIG: roughly a Lattice reference board
    uint8_t data_plane_id = 0, sensor_id = 0;
    LinuxDataPlane plane(hsb, IPAddress_from_string(ip), data_plane_id, sensor_id);
    hsb.start();
    printf("HSB emulator up at %s: BOOTP on 12267, ECB control on 8192, data from UDP 12288; %zu frames of %lld bytes, %u fps\n",
        ip.c_str(), frames_in_file, (long long)frame_bytes, fps);
    fflush(stdout);

    int64_t shape[1] = { frame_bytes };
    DLTensor tensor {};
    tensor.device = { kDLCPU, 0 };
    tensor.ndim = 1;
    tensor.dtype = DLDataType { kDLUInt, 8, 1 };
    tensor.shape = shape;

    const auto period = std::chrono::microseconds(1000000 / (fps ? fps : 30));
    auto next = std::chrono::steady_clock::now();
    unsigned sent = 0, errors = 0, idx = 0;
    auto t0 = std::chrono::steady_clock::now();
    while (!limit || sent < limit) {
        tensor.data = data.data() + (size_t)idx * frame_bytes;
        int64_t n = plane.send(tensor);
        if (n < 0) errors++; else if (n > 0) { sent++; idx = (idx + 1) % frames_in_file; }
        if (sent && sent % 300 == 0 && n > 0) {
            double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            printf("[%7.1f s] frames sent %u, errors %u, %.1f fps, %.1f Mbit/s\n", s, sent, errors, sent / s, sent * frame_bytes * 8.0 / s / 1e6);
            fflush(stdout);
        }
        next += period;
        std::this_thread::sleep_until(next);
    }
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("done: frames sent %u, errors %u in %.1f s (%.1f fps)\n", sent, errors, s, sent / s);
    hsb.stop();
    return 0;
}
