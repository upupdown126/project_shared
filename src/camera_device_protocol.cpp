#include "smart_home/camera_device_protocol.hpp"

#include <exception>
#include <stdexcept>

namespace smart_home {

CameraDeviceProtocolHandler::CameraDeviceProtocolHandler(CameraRepository& repository)
    : repository_(repository) {}

bool CameraDeviceProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                         const TlvPacket& packet) {
    try {
        if (packet.type == static_cast<std::uint32_t>(TaskType::GetCameras)) {
            connection->send(TlvPacket(static_cast<std::uint32_t>(TaskType::Cameras),
                                       serialize_cameras(repository_.list())));
            return true;
        }
        if (packet.type == static_cast<std::uint32_t>(TaskType::SaveCamera)) {
            const std::vector<CameraDevice> cameras = deserialize_cameras(packet.value);
            if (cameras.size() != 1 || !repository_.save(cameras[0]))
                throw std::invalid_argument("SaveCamera requires one valid device");
            connection->send(TlvPacket(TaskType::CameraSaved,
                                       std::to_string(cameras[0].id)));
            return true;
        }
        return false;
    } catch (const std::exception& error) {
        connection->send(TlvPacket(TaskType::Error, error.what()));
        return true;
    }
}

}  // namespace smart_home
