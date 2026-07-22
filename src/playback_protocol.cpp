#include "smart_home/playback_protocol.hpp"
#include "smart_home/hls_auth.hpp"

#include <chrono>
#include <sstream>
#include <stdexcept>

namespace smart_home {

PlaybackProtocolHandler::PlaybackProtocolHandler(RecordingRepository& repository,
    const std::string& public_hls_base_url, const std::string& access_secret,
    std::int64_t url_ttl_seconds)
    : repository_(repository), public_hls_base_url_(public_hls_base_url),
      access_secret_(access_secret), url_ttl_seconds_(url_ttl_seconds) {
    while (!public_hls_base_url_.empty() && public_hls_base_url_[public_hls_base_url_.size()-1]=='/')
        public_hls_base_url_.erase(public_hls_base_url_.size()-1);
    if (public_hls_base_url_.empty()) throw std::invalid_argument("public HLS URL is empty");
    if (access_secret_.size() < 16 || url_ttl_seconds_ <= 0)
        throw std::invalid_argument("HLS secret must be at least 16 bytes and TTL must be positive");
}

bool PlaybackProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                     const TlvPacket& packet) {
    if (packet.type != static_cast<std::uint32_t>(TaskType::GetPlaybackUrl)) return false;
    std::istringstream input(packet.text());
    std::uint32_t camera = 0, channel = 0;
    std::int64_t begin = 0, end = 0;
    std::string extra;
    if (!(input >> camera >> channel >> begin >> end) || (input >> extra) || end <= begin) {
        connection->send(TlvPacket(TaskType::Error,
            "playback payload: <camera_id> <channel> <begin_ms> <end_ms>"));
        return true;
    }
    const std::vector<RecordingSegment> segments =
        repository_.query(camera, channel, begin, end);
    if (segments.empty()) {
        connection->send(TlvPacket(TaskType::Error, "recording not found"));
        return true;
    }
    std::ostringstream url;
    const std::int64_t expires = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + url_ttl_seconds_;
    const std::string token = hls_access_token(access_secret_, camera, channel,
                                                begin, end, expires);
    url << public_hls_base_url_ << "/hls/vod?camera=" << camera
        << "&channel=" << channel << "&begin=" << begin << "&end=" << end
        << "&expires=" << expires << "&token=" << token;
    connection->send(TlvPacket(TaskType::PlaybackUrl, url.str()));
    return true;
}

}  // namespace smart_home
