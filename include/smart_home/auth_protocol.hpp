#ifndef SMART_HOME_AUTH_PROTOCOL_HPP
#define SMART_HOME_AUTH_PROTOCOL_HPP

#include "smart_home/auth_service.hpp"
#include "smart_home/tcp_server.hpp"

#include <map>
#include <mutex>
#include <string>

namespace smart_home {

class AuthProtocolHandler {
public:
    explicit AuthProtocolHandler(AuthService& authentication);
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
    void connection_closed(const TcpServer::ConnectionPtr& connection);
    bool is_authenticated(const TcpServer::ConnectionPtr& connection) const;

private:
    enum PendingOperation { Register, Login };
    struct PendingSession {
        PendingOperation operation;
        std::string username;
    };

    void begin(const TcpServer::ConnectionPtr& connection,
               PendingOperation operation,
               const std::string& username);
    void finish(const TcpServer::ConnectionPtr& connection,
                PendingOperation operation,
                const std::string& encrypted_password);

    AuthService& authentication_;
    mutable std::mutex mutex_;
    std::map<const TcpConnection*, PendingSession> pending_;
    std::map<const TcpConnection*, std::string> authenticated_;
};

}  // namespace smart_home

#endif
