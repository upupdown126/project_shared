#ifndef SMART_HOME_CAMERA_REPOSITORY_HPP
#define SMART_HOME_CAMERA_REPOSITORY_HPP

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace smart_home {

struct CameraDevice {
    std::uint32_t id;
    std::uint32_t type;
    std::string serial_number;
    std::uint32_t channels;
    std::string ip;
    std::string rtsp_url;
    std::string rtmp_url;
};

class CameraRepository {
public:
    virtual ~CameraRepository() {}
    virtual bool save(const CameraDevice& camera) = 0;
    virtual std::vector<CameraDevice> list() = 0;
    virtual bool find(std::uint32_t id, CameraDevice& output) = 0;
};

class InMemoryCameraRepository : public CameraRepository {
public:
    bool save(const CameraDevice& camera);
    std::vector<CameraDevice> list();
    bool find(std::uint32_t id, CameraDevice& output);
private:
    std::mutex mutex_;
    std::map<std::uint32_t, CameraDevice> cameras_;
};

std::vector<std::uint8_t> serialize_cameras(const std::vector<CameraDevice>& cameras);
std::vector<CameraDevice> deserialize_cameras(const std::vector<std::uint8_t>& bytes);

}  // namespace smart_home
#endif
