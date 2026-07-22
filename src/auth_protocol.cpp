#include "smart_home/auth_protocol.hpp"
#include "smart_home/logger.hpp"

namespace smart_home {

AuthProtocolHandler::AuthProtocolHandler(AuthService& authentication)
    : authentication_(authentication) {}

bool AuthProtocolHandler::handle(const TcpServer::ConnectionPtr& connection,
                                 const TlvPacket& packet) {
    const TaskType type = static_cast<TaskType>(packet.type);
    switch (type) {
    case TaskType::RegisterSection1:
        begin(connection, Register, packet.text());
        return true;
    case TaskType::RegisterSection2:
        finish(connection, Register, packet.text());
        return true;
    case TaskType::LoginSection1:
        begin(connection, Login, packet.text());
        return true;
    case TaskType::LoginSection2:
        finish(connection, Login, packet.text());
        return true;
    default:
        return false;
    }
}

void AuthProtocolHandler::begin(const TcpServer::ConnectionPtr& connection,
                                PendingOperation operation,
                                const std::string& username) {
    const AuthResult result = operation == Register
        ? authentication_.begin_registration(username)
        : authentication_.begin_login(username);
    if (result.success) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const PendingSession session = {operation, username};
            pending_[connection.get()] = session;
        }
        connection->send(TlvPacket(
            operation == Register ? TaskType::RegisterSection1Ok : TaskType::LoginSection1Ok,
            result.value));
    } else {
        connection->send(TlvPacket(
            operation == Register ? TaskType::RegisterSection1Error
                                  : TaskType::LoginSection1Error,
            result.value));
    }
}

void AuthProtocolHandler::finish(const TcpServer::ConnectionPtr& connection,
                                 PendingOperation operation,
                                 const std::string& encrypted_password) {
    std::string username;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<const TcpConnection*, PendingSession>::iterator found =
            pending_.find(connection.get());
        if (found == pending_.end() || found->second.operation != operation) {
            connection->send(TlvPacket(
                operation == Register ? TaskType::RegisterSection2Error
                                      : TaskType::LoginSection2Error,
                "authentication stage mismatch"));
            return;
        }
        username = found->second.username;
        pending_.erase(found);
    }
    const AuthResult result = operation == Register
        ? authentication_.finish_registration(username, encrypted_password)
        : authentication_.finish_login(username, encrypted_password);
    if (operation == Login && result.success) {
        std::lock_guard<std::mutex> lock(mutex_);
        authenticated_[connection.get()] = username;
    }
    if (result.success) Logger::instance().info(
        std::string(operation == Register ? "user registered: " : "user logged in: ") + username);
    connection->send(TlvPacket(
        operation == Register
            ? (result.success ? TaskType::RegisterSection2Ok : TaskType::RegisterSection2Error)
            : (result.success ? TaskType::LoginSection2Ok : TaskType::LoginSection2Error),
        result.value));
}

void AuthProtocolHandler::connection_closed(const TcpServer::ConnectionPtr& connection) {
    std::string cancelled_registration;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<const TcpConnection*, PendingSession>::iterator pending =
            pending_.find(connection.get());
        if (pending != pending_.end()) {
            if (pending->second.operation == Register)
                cancelled_registration = pending->second.username;
            pending_.erase(pending);
        }
        authenticated_.erase(connection.get());
    }
    if (!cancelled_registration.empty())
        authentication_.cancel_registration(cancelled_registration);
}

bool AuthProtocolHandler::is_authenticated(
    const TcpServer::ConnectionPtr& connection) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return authenticated_.find(connection.get()) != authenticated_.end();
}

}  // namespace smart_home
