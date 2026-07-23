#include "smart_home/tcp_server.hpp"

#include "smart_home/logger.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace smart_home {
namespace {

#if defined(_WIN32)
typedef SOCKET NativeSocket;
const NativeSocket InvalidSocket = INVALID_SOCKET;
void close_socket(NativeSocket socket) { closesocket(socket); }
int last_socket_error() { return WSAGetLastError(); }
class SocketRuntime {
public:
    SocketRuntime() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    ~SocketRuntime() { WSACleanup(); }
};
#else
typedef int NativeSocket;
const NativeSocket InvalidSocket = -1;
void close_socket(NativeSocket socket) { ::close(socket); }
int last_socket_error() { return errno; }
class SocketRuntime {};
#endif

SocketRuntime& socket_runtime() {
    static SocketRuntime runtime;
    return runtime;
}

NativeSocket native(std::intptr_t value) { return static_cast<NativeSocket>(value); }

std::string socket_error(const std::string& action) {
    return action + " failed with socket error " + std::to_string(last_socket_error());
}

}  // namespace

TcpConnection::TcpConnection(std::intptr_t socket_handle, const std::string& peer_name)
    : socket_(socket_handle), peer_(peer_name), connected_(true) {}

TcpConnection::~TcpConnection() { close(); }

bool TcpConnection::send(const TlvPacket& packet) {
    const std::vector<std::uint8_t> wire = TlvCodec::encode(packet);
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connected_) {
        return false;
    }
    std::size_t sent = 0;
    while (sent < wire.size()) {
        const int result = ::send(native(socket_),
                                  reinterpret_cast<const char*>(&wire[sent]),
                                  static_cast<int>(wire.size() - sent), 0);
        if (result <= 0) {
            close();
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

void TcpConnection::close() {
    bool expected = true;
    if (connected_.compare_exchange_strong(expected, false)) {
#if defined(_WIN32)
        shutdown(native(socket_), SD_BOTH);
#else
        shutdown(native(socket_), SHUT_RDWR);
#endif
        close_socket(native(socket_));
    }
}

bool TcpConnection::connected() const { return connected_; }
std::string TcpConnection::peer() const { return peer_; }

TcpServer::TcpServer(std::size_t worker_count, std::size_t maximum_tasks)
    : workers_(worker_count, maximum_tasks), running_(false), listener_(-1) {
    socket_runtime();
}

TcpServer::~TcpServer() { stop(); }

void TcpServer::set_message_handler(const MessageHandler& handler) { message_handler_ = handler; }
void TcpServer::set_connection_handler(const ConnectionHandler& handler) { connection_handler_ = handler; }
void TcpServer::set_close_handler(const ConnectionHandler& handler) { close_handler_ = handler; }

void TcpServer::run(const std::string& host, std::uint16_t port) {
    if (running_.exchange(true)) {
        throw std::runtime_error("TCP server is already running");
    }

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* addresses = NULL;
    const std::string port_text = std::to_string(port);
    if (getaddrinfo(host.empty() ? NULL : host.c_str(), port_text.c_str(), &hints,
                    &addresses) != 0) {
        running_ = false;
        throw std::runtime_error("cannot resolve listen address");
    }

    NativeSocket listener = InvalidSocket;
    for (struct addrinfo* address = addresses; address != NULL; address = address->ai_next) {
        listener = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (listener == InvalidSocket) {
            continue;
        }
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        if (::bind(listener, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0 &&
            ::listen(listener, SOMAXCONN) == 0) {
            break;
        }
        close_socket(listener);
        listener = InvalidSocket;
    }
    freeaddrinfo(addresses);
    if (listener == InvalidSocket) {
        running_ = false;
        throw std::runtime_error(socket_error("listen"));
    }
    listener_ = static_cast<std::intptr_t>(listener);
    workers_.start();
    Logger::instance().info("TCP server listening on " + host + ":" + port_text);

    while (running_) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(listener, &reads);
        NativeSocket maximum = listener;
        std::vector<ConnectionPtr> snapshot;
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (std::map<std::intptr_t, ConnectionPtr>::const_iterator it = connections_.begin();
                 it != connections_.end(); ++it) {
                snapshot.push_back(it->second);
                FD_SET(native(it->first), &reads);
                if (native(it->first) > maximum) maximum = native(it->first);
            }
        }
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        const int ready = select(static_cast<int>(maximum + 1), &reads, NULL, NULL, &timeout);
        if (ready < 0) {
            if (running_) Logger::instance().error(socket_error("select"));
            continue;
        }
        if (FD_ISSET(listener, &reads)) accept_ready();
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            if (snapshot[i]->connected() && FD_ISSET(native(snapshot[i]->socket_), &reads)) {
                connection_ready(snapshot[i]);
            }
        }
    }
    workers_.stop();
}

void TcpServer::accept_ready() {
    struct sockaddr_storage address;
#if defined(_WIN32)
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    const NativeSocket client = ::accept(native(listener_),
        reinterpret_cast<struct sockaddr*>(&address), &length);
    if (client == InvalidSocket) return;

    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    getnameinfo(reinterpret_cast<struct sockaddr*>(&address), length, host, sizeof(host),
                service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
    const ConnectionPtr connection(new TcpConnection(
        static_cast<std::intptr_t>(client), std::string(host) + ":" + service));
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[connection->socket_] = connection;
    }
    if (connection_handler_) connection_handler_(connection);
}

void TcpServer::connection_ready(const ConnectionPtr& connection) {
    std::uint8_t bytes[8192];
    const int received = recv(native(connection->socket_), reinterpret_cast<char*>(bytes),
                              sizeof(bytes), 0);
    if (received <= 0) {
        remove_connection(connection->socket_);
        return;
    }
    try {
        const std::vector<TlvPacket> packets =
            connection->decoder_.append(bytes, static_cast<std::size_t>(received));
        for (std::size_t i = 0; i < packets.size(); ++i) {
            const MessageHandler handler = message_handler_;
            const TlvPacket packet = packets[i];
            if (handler && !workers_.post([handler, connection, packet] {
                    handler(connection, packet);
                })) {
                connection->send(TlvPacket(TaskType::Error, "server task queue is full"));
            }
        }
    } catch (const std::exception& error) {
        connection->send(TlvPacket(TaskType::Error, error.what()));
        remove_connection(connection->socket_);
    }
}

void TcpServer::remove_connection(std::intptr_t socket_handle) {
    ConnectionPtr connection;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        const std::map<std::intptr_t, ConnectionPtr>::iterator found =
            connections_.find(socket_handle);
        if (found == connections_.end()) return;
        connection = found->second;
        connections_.erase(found);
    }
    connection->close();
    if (close_handler_) close_handler_(connection);
}

void TcpServer::stop() {
    if (!running_.exchange(false)) return;
    if (listener_ != -1) {
        close_socket(native(listener_));
        listener_ = -1;
    }
    std::map<std::intptr_t, ConnectionPtr> connections;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections.swap(connections_);
    }
    for (std::map<std::intptr_t, ConnectionPtr>::iterator it = connections.begin();
         it != connections.end(); ++it) {
        it->second->close();
    }
}

bool TcpServer::running() const { return running_; }

}  // namespace smart_home

