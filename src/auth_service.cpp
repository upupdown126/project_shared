#include "smart_home/auth_service.hpp"

#include <cctype>
#include <cstdint>

namespace smart_home {

AuthResult AuthResult::ok(const std::string& value) {
    AuthResult result = {true, value};
    return result;
}

AuthResult AuthResult::error(const std::string& message) {
    AuthResult result = {false, message};
    return result;
}

AuthService::AuthService(UserRepository& users) : users_(users) {}

bool AuthService::valid_username(const std::string& username) {
    if (username.empty() || username.size() > 20) return false;
    for (std::size_t i = 0; i < username.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(username[i]);
        if (!(std::isalnum(character) || character == '_' || character == '-')) {
            return false;
        }
    }
    return true;
}

std::string AuthService::generate_setting() {
    static const char alphabet[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./";
    std::uniform_int_distribution<std::size_t> distribution(0, sizeof(alphabet) - 2);
    std::string salt;
    salt.reserve(8);
    for (std::size_t i = 0; i < 8; ++i) {
        salt.push_back(alphabet[distribution(random_)]);
    }
    return "$1$" + salt + "$";
}

bool AuthService::constant_time_equal(const std::string& left, const std::string& right) {
    const std::size_t maximum = left.size() > right.size() ? left.size() : right.size();
    std::uint8_t difference = static_cast<std::uint8_t>(left.size() ^ right.size());
    for (std::size_t i = 0; i < maximum; ++i) {
        const std::uint8_t a = i < left.size() ? static_cast<std::uint8_t>(left[i]) : 0;
        const std::uint8_t b = i < right.size() ? static_cast<std::uint8_t>(right[i]) : 0;
        difference |= static_cast<std::uint8_t>(a ^ b);
    }
    return difference == 0;
}

AuthResult AuthService::begin_registration(const std::string& username) {
    if (!valid_username(username)) {
        return AuthResult::error("username must be 1-20 letters, digits, '_' or '-'");
    }
    UserRecord existing;
    if (users_.find(username, existing)) {
        return AuthResult::error("username already exists");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_registrations_.find(username) != pending_registrations_.end())
        return AuthResult::error("registration is already in progress");
    const std::string setting = generate_setting();
    pending_registrations_[username] = setting;
    return AuthResult::ok(setting);
}

AuthResult AuthService::finish_registration(const std::string& username,
                                             const std::string& encrypted_password) {
    if (encrypted_password.empty() || encrypted_password.size() > 512) {
        return AuthResult::error("invalid encrypted password");
    }
    std::string setting;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::map<std::string, std::string>::iterator pending =
            pending_registrations_.find(username);
        if (pending == pending_registrations_.end()) {
            return AuthResult::error("registration was not started");
        }
        setting = pending->second;
        pending_registrations_.erase(pending);
    }
    UserRecord user = {username, setting, encrypted_password};
    if (!users_.insert(user)) {
        return AuthResult::error("username already exists");
    }
    return AuthResult::ok();
}

void AuthService::cancel_registration(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_registrations_.erase(username);
}

AuthResult AuthService::begin_login(const std::string& username) {
    UserRecord user;
    if (!valid_username(username) || !users_.find(username, user)) {
        return AuthResult::error("invalid username or password");
    }
    return AuthResult::ok(user.setting);
}

AuthResult AuthService::finish_login(const std::string& username,
                                     const std::string& encrypted_password) {
    UserRecord user;
    if (!users_.find(username, user) ||
        !constant_time_equal(user.encrypted_password, encrypted_password)) {
        return AuthResult::error("invalid username or password");
    }
    return AuthResult::ok();
}

}  // namespace smart_home
