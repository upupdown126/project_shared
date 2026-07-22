#ifndef SMART_HOME_CAMERA_DEVICE_PROTOCOL_HPP
#define SMART_HOME_CAMERA_DEVICE_PROTOCOL_HPP

#include "smart_home/camera_repository.hpp"
#include "smart_home/tcp_server.hpp"

namespace smart_home {

class CameraDeviceProtocolHandler {
public:
    explicit CameraDeviceProtocolHandler(CameraRepository& repository);
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
private:
    CameraRepository& repository_;
};

}  // namespace smart_home
#endif
