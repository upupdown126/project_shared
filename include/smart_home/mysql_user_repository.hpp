#ifndef SMART_HOME_MYSQL_USER_REPOSITORY_HPP
#define SMART_HOME_MYSQL_USER_REPOSITORY_HPP

#include "smart_home/user_repository.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace smart_home {

#if defined(SMART_HOME_HAS_MYSQL)
class MysqlUserRepository : public UserRepository {
public:
    MysqlUserRepository(const std::string& host,
                        std::uint16_t port,
                        const std::string& username,
                        const std::string& password,
                        const std::string& database);
    ~MysqlUserRepository();

    bool find(const std::string& name, UserRecord& output);
    bool insert(const UserRecord& user);

private:
    MysqlUserRepository(const MysqlUserRepository&);
    MysqlUserRepository& operator=(const MysqlUserRepository&);
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
#endif

}  // namespace smart_home

#endif

