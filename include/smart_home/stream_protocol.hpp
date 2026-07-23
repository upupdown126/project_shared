#ifndef SMART_HOME_STREAM_PROTOCOL_HPP
#define SMART_HOME_STREAM_PROTOCOL_HPP

#include "smart_home/media_stream.hpp"
#include "smart_home/tcp_server.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace smart_home {

class StreamProtocolHandler {
public:
    typedef std::function<std::unique_ptr<MediaSource>()> SourceFactory;
    StreamProtocolHandler(const SourceFactory& factory,
                          std::size_t ring_capacity = 128,
                          std::size_t fragment_size = 64U * 1024U);
    ~StreamProtocolHandler();
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
    void connection_closed(const TcpServer::ConnectionPtr& connection);
private:
    void start(const TcpServer::ConnectionPtr& connection, const std::string& payload);
    void stop(const TcpServer::ConnectionPtr& connection);
    SourceFactory factory_;
    std::size_t ring_capacity_;
    std::size_t fragment_size_;
    std::mutex mutex_;
    std::map<const TcpConnection*, std::shared_ptr<MediaStreamSession> > sessions_;
};

}  // namespace smart_home
#endif
