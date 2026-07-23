#include "smart_home/stream_protocol.hpp"

#include <sstream>
#include <stdexcept>
#include <vector>

namespace smart_home {
namespace {

class TlvMediaSink : public MediaSink {
public:
    explicit TlvMediaSink(const TcpServer::ConnectionPtr& connection) : connection_(connection) {}
    bool metadata(const VideoMetadata& value) {
        return connection_->send(TlvPacket(static_cast<std::uint32_t>(TaskType::StreamMetadata),
                                           serialize_metadata(value)));
    }
    bool fragment(const PacketFragment& value) {
        return connection_->send(TlvPacket(static_cast<std::uint32_t>(TaskType::StreamPacket),
                                           serialize_fragment(value)));
    }
    void error(const std::string& message) {
        connection_->send(TlvPacket(TaskType::Error, message));
    }
private:
    TcpServer::ConnectionPtr connection_;
};

}  // namespace

StreamProtocolHandler::StreamProtocolHandler(const SourceFactory& factory,
                                             std::size_t ring_capacity,
                                             std::size_t fragment_size)
    : factory_(factory), ring_capacity_(ring_capacity), fragment_size_(fragment_size) {
    if (!factory_ || ring_capacity_ == 0 || fragment_size_ == 0)
        throw std::invalid_argument("invalid stream protocol configuration");
}

StreamProtocolHandler::~StreamProtocolHandler() {
    std::map<const TcpConnection*, std::shared_ptr<MediaStreamSession> > sessions;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions.swap(sessions_);
    }
    for (std::map<const TcpConnection*, std::shared_ptr<MediaStreamSession> >::iterator it = sessions.begin();
         it != sessions.end(); ++it) it->second->stop();
}

bool StreamProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                   const TlvPacket& packet) {
    if (packet.type == static_cast<std::uint32_t>(TaskType::GetStream)) {
        try { start(connection, packet.text()); }
        catch (const std::exception& error) { connection->send(TlvPacket(TaskType::Error, error.what())); }
        return true;
    }
    if (packet.type == static_cast<std::uint32_t>(TaskType::StopStream)) {
        stop(connection);
        return true;
    }
    return false;
}

void StreamProtocolHandler::start(const TcpServer::ConnectionPtr& connection,
                                  const std::string& payload) {
    std::istringstream input(payload);
    std::uint32_t camera_id = 0;
    std::string url, extra;
    if (!(input >> camera_id >> url) || (input >> extra) ||
        (url.compare(0, 7, "rtsp://") != 0 && url.compare(0, 7, "rtmp://") != 0))
        throw std::invalid_argument("stream payload: <camera_id> <rtsp-or-rtmp-url>");

    std::shared_ptr<MediaStreamSession> session(new MediaStreamSession(
        factory_(), std::shared_ptr<MediaSink>(new TlvMediaSink(connection)),
        ring_capacity_, fragment_size_));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sessions_.find(connection.get()) != sessions_.end())
            throw std::runtime_error("connection already has an active stream");
        sessions_[connection.get()] = session;
    }
    try { session->start(camera_id, url); }
    catch (...) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(connection.get());
        throw;
    }
}

void StreamProtocolHandler::stop(const TcpServer::ConnectionPtr& connection) {
    std::shared_ptr<MediaStreamSession> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<const TcpConnection*, std::shared_ptr<MediaStreamSession> >::iterator found =
            sessions_.find(connection.get());
        if (found == sessions_.end()) return;
        session = found->second;
        sessions_.erase(found);
    }
    session->stop();
}

void StreamProtocolHandler::connection_closed(const TcpServer::ConnectionPtr& connection) {
    stop(connection);
}

}  // namespace smart_home
