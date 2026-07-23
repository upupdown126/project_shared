#include "smart_home/camera_protocol.hpp"

#include <sstream>
#include <stdexcept>

namespace smart_home {
namespace {

template <typename A, typename B, typename C, typename D>
bool parse_four(const std::string& text, A& a, B& b, C& c, D& d) {
    std::istringstream input(text);
    std::string extra;
    return (input >> a >> b >> c >> d) && !(input >> extra);
}

std::string response_text(const HttpResponse& response) {
    return std::to_string(response.status) + "\n" + response.body;
}

}  // namespace

CameraProtocolHandler::CameraProtocolHandler(CameraApi& camera) : camera_(camera) {}

bool CameraProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                   const TlvPacket& packet) {
    try {
        if (packet.type == static_cast<std::uint32_t>(TaskType::GetRecordings)) {
            std::int64_t begin = 0, end = 0, timestamp = 0;
            int channel = 0;
            if (!parse_four(packet.text(), begin, end, channel, timestamp))
                throw std::invalid_argument("recording payload: <begin_ms> <end_ms> <channel> <timestamp_seconds>");
            const HttpResponse response = camera_.recording_list(begin, end, channel, timestamp);
            connection->send(TlvPacket(TaskType::Recordings, response_text(response)));
            return true;
        }
        if (packet.type == static_cast<std::uint32_t>(TaskType::PtzControl)) {
            int channel = 0, speed = 0;
            std::int64_t timestamp = 0;
            std::string command;
            if (!parse_four(packet.text(), channel, command, speed, timestamp))
                throw std::invalid_argument("PTZ payload: <channel> <command> <speed> <timestamp_seconds>");
            const HttpResponse response = camera_.ptz(channel, command, speed, timestamp);
            connection->send(TlvPacket(TaskType::CameraHttpResponse, response_text(response)));
            return true;
        }
        return false;
    } catch (const std::exception& error) {
        connection->send(TlvPacket(TaskType::Error, error.what()));
        return true;
    }
}

}  // namespace smart_home
