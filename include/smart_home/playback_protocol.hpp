#ifndef SMART_HOME_PLAYBACK_PROTOCOL_HPP
#define SMART_HOME_PLAYBACK_PROTOCOL_HPP

#include "smart_home/recording.hpp"
#include "smart_home/tcp_server.hpp"

#include <string>

namespace smart_home {

class PlaybackProtocolHandler {
public:
    PlaybackProtocolHandler(RecordingRepository& repository,
                            const std::string& public_hls_base_url,
                            const std::string& access_secret,
                            std::int64_t url_ttl_seconds = 300);
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
private:
    RecordingRepository& repository_;
    std::string public_hls_base_url_;
    std::string access_secret_;
    std::int64_t url_ttl_seconds_;
};

}  // namespace smart_home
#endif
