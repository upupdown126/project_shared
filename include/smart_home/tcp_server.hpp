#ifndef SMART_HOME_TCP_SERVER_HPP
#define SMART_HOME_TCP_SERVER_HPP

#include "smart_home/thread_pool.hpp"
#include "smart_home/tlv.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace smart_home {

class TcpConnection {
public:
    ~TcpConnection();
    bool send(const TlvPacket& packet);
    void close();
    bool connected() const;
    std::string peer() const;

private:
    friend class TcpServer;
    TcpConnection(std::intptr_t socket_handle, const std::string& peer_name);

    std::intptr_t socket_;
    std::string peer_;
    std::atomic<bool> connected_;
    std::mutex send_mutex_;
    TlvStreamDecoder decoder_;
};

class TcpServer {
public:
    typedef std::shared_ptr<TcpConnection> ConnectionPtr;
    typedef std::function<void(const ConnectionPtr&, const TlvPacket&)> MessageHandler;
    typedef std::function<void(const ConnectionPtr&)> ConnectionHandler;

    TcpServer(std::size_t worker_count, std::size_t maximum_tasks);
    ~TcpServer();

    void set_message_handler(const MessageHandler& handler);
    void set_connection_handler(const ConnectionHandler& handler);
    void set_close_handler(const ConnectionHandler& handler);
    void run(const std::string& host, std::uint16_t port);
    void stop();
    bool running() const;

private:
    TcpServer(const TcpServer&);
    TcpServer& operator=(const TcpServer&);
    void accept_ready();
    void connection_ready(const ConnectionPtr& connection);
    void remove_connection(std::intptr_t socket_handle);

    ThreadPool workers_;
    std::atomic<bool> running_;
    std::intptr_t listener_;
    mutable std::mutex connections_mutex_;
    std::map<std::intptr_t, ConnectionPtr> connections_;
    MessageHandler message_handler_;
    ConnectionHandler connection_handler_;
    ConnectionHandler close_handler_;
};

}  // namespace smart_home

#endif

