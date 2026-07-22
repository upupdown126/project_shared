#include "smart_home/user_repository.hpp"

namespace smart_home {

bool InMemoryUserRepository::find(const std::string& name, UserRecord& output) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::map<std::string, UserRecord>::const_iterator found = users_.find(name);
    if (found == users_.end()) {
        return false;
    }
    output = found->second;
    return true;
}

bool InMemoryUserRepository::insert(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.insert(std::make_pair(user.name, user)).second;
}

std::size_t InMemoryUserRepository::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.size();
}

}  // namespace smart_home

