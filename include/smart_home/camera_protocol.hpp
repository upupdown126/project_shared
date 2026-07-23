#ifndef SMART_HOME_CAMERA_PROTOCOL_HPP
#define SMART_HOME_CAMERA_PROTOCOL_HPP

#include "smart_home/camera_api.hpp"
#include "smart_home/tcp_server.hpp"

namespace smart_home {

class CameraProtocolHandler {
public:
    explicit CameraProtocolHandler(CameraApi& camera);
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
private:
    CameraApi& camera_;
};

}  // namespace smart_home
#endif

