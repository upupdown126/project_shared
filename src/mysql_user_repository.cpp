#include "smart_home/mysql_user_repository.hpp"

#include <mysql.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

namespace smart_home {
namespace {

class Statement {
public:
    Statement(MYSQL* connection, const char* sql) : statement_(mysql_stmt_init(connection)) {
        if (!statement_) throw std::runtime_error("mysql_stmt_init failed");
        if (mysql_stmt_prepare(statement_, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            const std::string error = mysql_stmt_error(statement_);
            mysql_stmt_close(statement_);
            statement_ = NULL;
            throw std::runtime_error("MySQL prepare failed: " + error);
        }
    }
    ~Statement() { if (statement_) mysql_stmt_close(statement_); }
    MYSQL_STMT* get() { return statement_; }
private:
    MYSQL_STMT* statement_;
};

void bind_string(MYSQL_BIND& bind, const std::string& value, unsigned long& length) {
    std::memset(&bind, 0, sizeof(bind));
    length = static_cast<unsigned long>(value.size());
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(value.data());
    bind.buffer_length = length;
    bind.length = &length;
}

}  // namespace

struct MysqlUserRepository::Implementation {
    MYSQL connection;
    std::mutex mutex;
};

MysqlUserRepository::MysqlUserRepository(const std::string& host,
                                         std::uint16_t port,
                                         const std::string& username,
                                         const std::string& password,
                                         const std::string& database)
    : implementation_(new Implementation()) {
    if (!mysql_init(&implementation_->connection)) {
        throw std::runtime_error("mysql_init failed");
    }
    if (!mysql_real_connect(&implementation_->connection, host.c_str(), username.c_str(),
                            password.c_str(), database.c_str(), port, NULL, 0)) {
        const std::string error = mysql_error(&implementation_->connection);
        mysql_close(&implementation_->connection);
        throw std::runtime_error("MySQL connection failed: " + error);
    }
    mysql_set_character_set(&implementation_->connection, "utf8mb4");
}

MysqlUserRepository::~MysqlUserRepository() {
    mysql_close(&implementation_->connection);
}

bool MysqlUserRepository::find(const std::string& name, UserRecord& output) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    Statement statement(&implementation_->connection,
        "SELECT name, setting, encrypted_password FROM users WHERE name=? LIMIT 1");
    MYSQL_BIND parameter[1];
    unsigned long name_length = 0;
    bind_string(parameter[0], name, name_length);
    if (mysql_stmt_bind_param(statement.get(), parameter) != 0 ||
        mysql_stmt_execute(statement.get()) != 0) {
        throw std::runtime_error("MySQL user query failed: " +
                                 std::string(mysql_stmt_error(statement.get())));
    }

    char name_buffer[21] = {0};
    char setting_buffer[65] = {0};
    char password_buffer[513] = {0};
    unsigned long lengths[3] = {0, 0, 0};
    MYSQL_BIND result[3];
    std::memset(result, 0, sizeof(result));
    char* buffers[] = {name_buffer, setting_buffer, password_buffer};
    const unsigned long capacities[] = {20, 64, 512};
    for (int i = 0; i < 3; ++i) {
        result[i].buffer_type = MYSQL_TYPE_STRING;
        result[i].buffer = buffers[i];
        result[i].buffer_length = capacities[i];
        result[i].length = &lengths[i];
    }
    if (mysql_stmt_bind_result(statement.get(), result) != 0 ||
        mysql_stmt_store_result(statement.get()) != 0) {
        throw std::runtime_error("MySQL result binding failed: " +
                                 std::string(mysql_stmt_error(statement.get())));
    }
    const int fetched = mysql_stmt_fetch(statement.get());
    if (fetched == MYSQL_NO_DATA) return false;
    if (fetched != 0 && fetched != MYSQL_DATA_TRUNCATED) {
        throw std::runtime_error("MySQL result fetch failed: " +
                                 std::string(mysql_stmt_error(statement.get())));
    }
    output.name.assign(name_buffer, lengths[0]);
    output.setting.assign(setting_buffer, lengths[1]);
    output.encrypted_password.assign(password_buffer, lengths[2]);
    return true;
}

bool MysqlUserRepository::insert(const UserRecord& user) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    Statement statement(&implementation_->connection,
        "INSERT INTO users(name, setting, encrypted_password) VALUES(?,?,?)");
    MYSQL_BIND parameters[3];
    unsigned long lengths[3] = {0, 0, 0};
    bind_string(parameters[0], user.name, lengths[0]);
    bind_string(parameters[1], user.setting, lengths[1]);
    bind_string(parameters[2], user.encrypted_password, lengths[2]);
    if (mysql_stmt_bind_param(statement.get(), parameters) != 0) {
        throw std::runtime_error("MySQL insert binding failed: " +
                                 std::string(mysql_stmt_error(statement.get())));
    }
    if (mysql_stmt_execute(statement.get()) != 0) {
        if (mysql_stmt_errno(statement.get()) == 1062) return false;
        throw std::runtime_error("MySQL user insert failed: " +
                                 std::string(mysql_stmt_error(statement.get())));
    }
    return mysql_stmt_affected_rows(statement.get()) == 1;
}

}  // namespace smart_home

