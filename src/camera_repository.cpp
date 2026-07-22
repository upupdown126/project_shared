#include "smart_home/camera_repository.hpp"

#include <algorithm>
#include <stdexcept>

namespace smart_home {
namespace {

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}
void append_text(std::vector<std::uint8_t>& out, const std::string& text) {
    if (text.size() > 1024U * 1024U) throw std::length_error("camera field is too large");
    append_u32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}
std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, std::size_t& at) {
    if (bytes.size() - at < 4) throw std::invalid_argument("truncated camera list");
    const std::uint32_t value = (static_cast<std::uint32_t>(bytes[at]) << 24) |
        (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
        (static_cast<std::uint32_t>(bytes[at + 2]) << 8) | bytes[at + 3];
    at += 4;
    return value;
}
std::string read_text(const std::vector<std::uint8_t>& bytes, std::size_t& at) {
    const std::uint32_t size = read_u32(bytes, at);
    if (size > 1024U * 1024U || size > bytes.size() - at)
        throw std::invalid_argument("invalid camera field length");
    const std::string value(bytes.begin() + at, bytes.begin() + at + size);
    at += size;
    return value;
}
bool valid(const CameraDevice& value) {
    return value.id != 0 && value.channels != 0 && value.type <= 1 &&
           !value.serial_number.empty() && !value.ip.empty() &&
           (!value.rtsp_url.empty() || !value.rtmp_url.empty());
}

}  // namespace

bool InMemoryCameraRepository::save(const CameraDevice& camera) {
    if (!valid(camera)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    cameras_[camera.id] = camera;
    return true;
}
std::vector<CameraDevice> InMemoryCameraRepository::list() {
    std::vector<CameraDevice> output;
    std::lock_guard<std::mutex> lock(mutex_);
    for (std::map<std::uint32_t, CameraDevice>::const_iterator it = cameras_.begin();
         it != cameras_.end(); ++it) output.push_back(it->second);
    return output;
}
bool InMemoryCameraRepository::find(std::uint32_t id, CameraDevice& output) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::uint32_t, CameraDevice>::const_iterator found = cameras_.find(id);
    if (found == cameras_.end()) return false;
    output = found->second;
    return true;
}

std::vector<std::uint8_t> serialize_cameras(const std::vector<CameraDevice>& cameras) {
    if (cameras.size() > 100000) throw std::length_error("too many cameras");
    std::vector<std::uint8_t> output;
    append_u32(output, static_cast<std::uint32_t>(cameras.size()));
    for (std::size_t i = 0; i < cameras.size(); ++i) {
        append_u32(output, cameras[i].id);
        append_u32(output, cameras[i].type);
        append_u32(output, cameras[i].channels);
        append_text(output, cameras[i].serial_number);
        append_text(output, cameras[i].ip);
        append_text(output, cameras[i].rtsp_url);
        append_text(output, cameras[i].rtmp_url);
    }
    return output;
}

std::vector<CameraDevice> deserialize_cameras(const std::vector<std::uint8_t>& bytes) {
    std::size_t at = 0;
    const std::uint32_t count = read_u32(bytes, at);
    if (count > 100000) throw std::invalid_argument("too many cameras");
    std::vector<CameraDevice> output;
    output.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        CameraDevice camera;
        camera.id = read_u32(bytes, at);
        camera.type = read_u32(bytes, at);
        camera.channels = read_u32(bytes, at);
        camera.serial_number = read_text(bytes, at);
        camera.ip = read_text(bytes, at);
        camera.rtsp_url = read_text(bytes, at);
        camera.rtmp_url = read_text(bytes, at);
        if (!valid(camera)) throw std::invalid_argument("invalid camera record");
        output.push_back(camera);
    }
    if (at != bytes.size()) throw std::invalid_argument("trailing camera list data");
    return output;
}

}  // namespace smart_home
