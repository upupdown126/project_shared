#include "smart_home/mysql_recording_repository.hpp"

#include <mysql.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

namespace smart_home {
namespace {

class RecordingStatement {
public:
    RecordingStatement(MYSQL* connection, const char* sql)
        : value_(mysql_stmt_init(connection)) {
        if (!value_) throw std::runtime_error("mysql_stmt_init failed");
        if (mysql_stmt_prepare(value_, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            const std::string error(mysql_stmt_error(value_));
            mysql_stmt_close(value_);
            throw std::runtime_error("MySQL prepare failed: " + error);
        }
    }
    ~RecordingStatement() { mysql_stmt_close(value_); }
    MYSQL_STMT* get() { return value_; }
private:
    MYSQL_STMT* value_;
};

void bind_text(MYSQL_BIND& bind, const std::string& text, unsigned long& length) {
    std::memset(&bind, 0, sizeof(bind));
    length = static_cast<unsigned long>(text.size());
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(text.data());
    bind.buffer_length = length;
    bind.length = &length;
}

template <typename T>
void bind_number(MYSQL_BIND& bind, enum_field_types type, T& value, bool is_unsigned) {
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = type;
    bind.buffer = &value;
    bind.is_unsigned = is_unsigned ? 1 : 0;
}

void throw_statement(MYSQL_STMT* statement, const std::string& action) {
    throw std::runtime_error("MySQL recording " + action + " failed: " +
                             std::string(mysql_stmt_error(statement)));
}

struct ResultBuffers {
    unsigned long long id;
    unsigned long long camera;
    unsigned long channel;
    char path[1025];
    long long start;
    long long end;
    unsigned long long size;
    char checksum[33];
    signed char discontinuity;
    unsigned long path_length;
    unsigned long checksum_length;
    MYSQL_BIND bindings[9];

    ResultBuffers() : id(0), camera(0), channel(0), start(0), end(0), size(0),
                      discontinuity(0), path_length(0), checksum_length(0) {
        std::memset(path, 0, sizeof(path));
        std::memset(checksum, 0, sizeof(checksum));
        bind_number(bindings[0], MYSQL_TYPE_LONGLONG, id, true);
        bind_number(bindings[1], MYSQL_TYPE_LONGLONG, camera, true);
        bind_number(bindings[2], MYSQL_TYPE_LONG, channel, true);
        std::memset(&bindings[3], 0, sizeof(MYSQL_BIND));
        bindings[3].buffer_type = MYSQL_TYPE_STRING;
        bindings[3].buffer = path;
        bindings[3].buffer_length = 1024;
        bindings[3].length = &path_length;
        bind_number(bindings[4], MYSQL_TYPE_LONGLONG, start, false);
        bind_number(bindings[5], MYSQL_TYPE_LONGLONG, end, false);
        bind_number(bindings[6], MYSQL_TYPE_LONGLONG, size, true);
        std::memset(&bindings[7], 0, sizeof(MYSQL_BIND));
        bindings[7].buffer_type = MYSQL_TYPE_STRING;
        bindings[7].buffer = checksum;
        bindings[7].buffer_length = 32;
        bindings[7].length = &checksum_length;
        bind_number(bindings[8], MYSQL_TYPE_TINY, discontinuity, false);
    }
    RecordingSegment segment() const {
        return RecordingSegment{static_cast<std::uint64_t>(id),
            static_cast<std::uint32_t>(camera), static_cast<std::uint32_t>(channel),
            std::string(path, path_length), static_cast<std::int64_t>(start),
            static_cast<std::int64_t>(end), static_cast<std::uint64_t>(size),
            std::string(checksum, checksum_length), discontinuity != 0};
    }
};

}  // namespace

struct MysqlRecordingRepository::Implementation {
    MYSQL connection;
    std::mutex mutex;
};

MysqlRecordingRepository::MysqlRecordingRepository(const std::string& host,
    std::uint16_t port, const std::string& username, const std::string& password,
    const std::string& database) : implementation_(new Implementation()) {
    if (!mysql_init(&implementation_->connection)) throw std::runtime_error("mysql_init failed");
    if (!mysql_real_connect(&implementation_->connection, host.c_str(), username.c_str(),
                            password.c_str(), database.c_str(), port, NULL, 0)) {
        const std::string error(mysql_error(&implementation_->connection));
        mysql_close(&implementation_->connection);
        throw std::runtime_error("MySQL connection failed: " + error);
    }
    mysql_set_character_set(&implementation_->connection, "utf8mb4");
}

MysqlRecordingRepository::~MysqlRecordingRepository() {
    mysql_close(&implementation_->connection);
}

bool MysqlRecordingRepository::insert(RecordingSegment& segment) {
    if (segment.path.empty() || segment.end_ms <= segment.start_ms) return false;
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    RecordingStatement statement(&implementation_->connection,
        "INSERT INTO recordings(camera_id,channel,path,start_ms,end_ms,size_bytes,checksum,discontinuity) VALUES(?,?,?,?,?,?,?,?)");
    unsigned long long camera = segment.camera_id, size = segment.size_bytes;
    unsigned long channel = segment.channel;
    long long start = segment.start_ms, end = segment.end_ms;
    signed char discontinuity = segment.discontinuity ? 1 : 0;
    unsigned long lengths[2] = {0, 0};
    MYSQL_BIND parameters[8];
    bind_number(parameters[0], MYSQL_TYPE_LONGLONG, camera, true);
    bind_number(parameters[1], MYSQL_TYPE_LONG, channel, true);
    bind_text(parameters[2], segment.path, lengths[0]);
    bind_number(parameters[3], MYSQL_TYPE_LONGLONG, start, false);
    bind_number(parameters[4], MYSQL_TYPE_LONGLONG, end, false);
    bind_number(parameters[5], MYSQL_TYPE_LONGLONG, size, true);
    bind_text(parameters[6], segment.checksum, lengths[1]);
    bind_number(parameters[7], MYSQL_TYPE_TINY, discontinuity, false);
    if (mysql_stmt_bind_param(statement.get(), parameters) != 0 ||
        mysql_stmt_execute(statement.get()) != 0) throw_statement(statement.get(), "insert");
    segment.id = static_cast<std::uint64_t>(mysql_stmt_insert_id(statement.get()));
    return mysql_stmt_affected_rows(statement.get()) == 1;
}

std::vector<RecordingSegment> MysqlRecordingRepository::query(
    std::uint32_t camera_id, std::uint32_t channel_id,
    std::int64_t begin_ms, std::int64_t end_ms) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    RecordingStatement statement(&implementation_->connection,
        "SELECT id,camera_id,channel,path,start_ms,end_ms,size_bytes,checksum,discontinuity FROM recordings WHERE camera_id=? AND channel=? AND end_ms>? AND start_ms<? ORDER BY start_ms,id");
    unsigned long long camera = camera_id;
    unsigned long channel = channel_id;
    long long begin = begin_ms, end = end_ms;
    MYSQL_BIND parameters[4];
    bind_number(parameters[0], MYSQL_TYPE_LONGLONG, camera, true);
    bind_number(parameters[1], MYSQL_TYPE_LONG, channel, true);
    bind_number(parameters[2], MYSQL_TYPE_LONGLONG, begin, false);
    bind_number(parameters[3], MYSQL_TYPE_LONGLONG, end, false);
    if (mysql_stmt_bind_param(statement.get(), parameters) != 0 ||
        mysql_stmt_execute(statement.get()) != 0) throw_statement(statement.get(), "query");
    ResultBuffers result;
    if (mysql_stmt_bind_result(statement.get(), result.bindings) != 0 ||
        mysql_stmt_store_result(statement.get()) != 0) throw_statement(statement.get(), "result binding");
    std::vector<RecordingSegment> output;
    for (;;) {
        const int status = mysql_stmt_fetch(statement.get());
        if (status == MYSQL_NO_DATA) break;
        if (status != 0 && status != MYSQL_DATA_TRUNCATED) throw_statement(statement.get(), "fetch");
        output.push_back(result.segment());
    }
    return output;
}

bool MysqlRecordingRepository::find_by_id(std::uint64_t value, RecordingSegment& output) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    RecordingStatement statement(&implementation_->connection,
        "SELECT id,camera_id,channel,path,start_ms,end_ms,size_bytes,checksum,discontinuity FROM recordings WHERE id=? LIMIT 1");
    unsigned long long id = value;
    MYSQL_BIND parameter[1];
    bind_number(parameter[0], MYSQL_TYPE_LONGLONG, id, true);
    if (mysql_stmt_bind_param(statement.get(), parameter) != 0 ||
        mysql_stmt_execute(statement.get()) != 0) throw_statement(statement.get(), "find");
    ResultBuffers result;
    if (mysql_stmt_bind_result(statement.get(), result.bindings) != 0 ||
        mysql_stmt_store_result(statement.get()) != 0) throw_statement(statement.get(), "result binding");
    const int status = mysql_stmt_fetch(statement.get());
    if (status == MYSQL_NO_DATA) return false;
    if (status != 0 && status != MYSQL_DATA_TRUNCATED) throw_statement(statement.get(), "fetch");
    output = result.segment();
    return true;
}

std::vector<RecordingSegment> MysqlRecordingRepository::list_all() {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    RecordingStatement statement(&implementation_->connection,
        "SELECT id,camera_id,channel,path,start_ms,end_ms,size_bytes,checksum,discontinuity FROM recordings ORDER BY start_ms,id");
    if (mysql_stmt_execute(statement.get()) != 0) throw_statement(statement.get(), "list");
    ResultBuffers result;
    if (mysql_stmt_bind_result(statement.get(), result.bindings) != 0 ||
        mysql_stmt_store_result(statement.get()) != 0) throw_statement(statement.get(), "result binding");
    std::vector<RecordingSegment> output;
    for (;;) {
        const int status = mysql_stmt_fetch(statement.get());
        if (status == MYSQL_NO_DATA) break;
        if (status != 0 && status != MYSQL_DATA_TRUNCATED) throw_statement(statement.get(), "fetch");
        output.push_back(result.segment());
    }
    return output;
}

bool MysqlRecordingRepository::erase(std::uint64_t value) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    RecordingStatement statement(&implementation_->connection,
        "DELETE FROM recordings WHERE id=?");
    unsigned long long id = value;
    MYSQL_BIND parameter[1];
    bind_number(parameter[0], MYSQL_TYPE_LONGLONG, id, true);
    if (mysql_stmt_bind_param(statement.get(), parameter) != 0 ||
        mysql_stmt_execute(statement.get()) != 0) throw_statement(statement.get(), "delete");
    return mysql_stmt_affected_rows(statement.get()) == 1;
}

}  // namespace smart_home
