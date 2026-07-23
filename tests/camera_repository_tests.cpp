#include "smart_home/camera_repository.hpp"

#include <iostream>

int main() {
    smart_home::CameraDevice camera = {7, 1, "serial-007", 2, "192.168.1.7",
        "rtsp://192.168.1.7/live", "rtmp://192.168.1.7/live"};
    smart_home::InMemoryCameraRepository repository;
    if (!repository.save(camera)) return 1;
    const std::vector<smart_home::CameraDevice> source = repository.list();
    const std::vector<std::uint8_t> bytes = smart_home::serialize_cameras(source);
    const std::vector<smart_home::CameraDevice> restored =
        smart_home::deserialize_cameras(bytes);
    smart_home::CameraDevice found;
    if (restored.size() != 1 || restored[0].serial_number != "serial-007" ||
        restored[0].channels != 2 || !repository.find(7, found) || found.type != 1) {
        std::cerr << "FAIL: camera repository/wire mismatch" << std::endl;
        return 1;
    }
    bool rejected = false;
    try {
        std::vector<std::uint8_t> truncated(bytes.begin(), bytes.end() - 1);
        smart_home::deserialize_cameras(truncated);
    } catch (...) { rejected = true; }
    if (!rejected) return 1;
    std::cout << "Camera repository and wire test passed" << std::endl;
    return 0;
}
