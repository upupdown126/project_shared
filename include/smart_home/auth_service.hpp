#ifndef SMART_HOME_AUTH_SERVICE_HPP
#define SMART_HOME_AUTH_SERVICE_HPP

#include "smart_home/user_repository.hpp"

#include <cstddef>
#include <map>
#include <mutex>
#include <random>
#include <string>

namespace smart_home {

struct AuthResult {
    bool success;
    std::string value;

    static AuthResult ok(const std::string& value = std::string());
    static AuthResult error(const std::string& message);
};

class AuthService {
public:
    explicit AuthService(UserRepository& users);

    AuthResult begin_registration(const std::string& username);
    AuthResult finish_registration(const std::string& username,
                                   const std::string& encrypted_password);
    void cancel_registration(const std::string& username);
    AuthResult begin_login(const std::string& username);
    AuthResult finish_login(const std::string& username,
                            const std::string& encrypted_password);

private:
    static bool valid_username(const std::string& username);
    static bool constant_time_equal(const std::string& left, const std::string& right);
    std::string generate_setting();

    UserRepository& users_;
    std::mutex mutex_;
    std::map<std::string, std::string> pending_registrations_;
    std::random_device random_;
};

}  // namespace smart_home

#endif
