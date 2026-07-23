#ifndef SMART_HOME_USER_REPOSITORY_HPP
#define SMART_HOME_USER_REPOSITORY_HPP

#include <map>
#include <mutex>
#include <string>

namespace smart_home {

struct UserRecord {
    std::string name;
    std::string setting;
    std::string encrypted_password;
};

class UserRepository {
public:
    virtual ~UserRepository() {}
    virtual bool find(const std::string& name, UserRecord& output) = 0;
    virtual bool insert(const UserRecord& user) = 0;
};

class InMemoryUserRepository : public UserRepository {
public:
    bool find(const std::string& name, UserRecord& output);
    bool insert(const UserRecord& user);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, UserRecord> users_;
};

}  // namespace smart_home

#endif

