#include "smart_home/mysql_camera_repository.hpp"

#include <mysql.h>

#include <cstring>
#include <mutex>
#include <stdexcept>

namespace smart_home {
namespace {

class CameraStatement {
public:
    CameraStatement(MYSQL* connection, const char* sql) : value(mysql_stmt_init(connection)) {
        if (!value) throw std::runtime_error("mysql_stmt_init failed");
        if (mysql_stmt_prepare(value, sql, static_cast<unsigned long>(std::strlen(sql))) != 0) {
            const std::string error(mysql_stmt_error(value));
            mysql_stmt_close(value);
            throw std::runtime_error("MySQL camera prepare failed: " + error);
        }
    }
    ~CameraStatement() { mysql_stmt_close(value); }
    MYSQL_STMT* value;
};

void text_parameter(MYSQL_BIND& bind, const std::string& value, unsigned long& length) {
    std::memset(&bind, 0, sizeof(bind));
    length = static_cast<unsigned long>(value.size());
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = const_cast<char*>(value.data());
    bind.buffer_length = length;
    bind.length = &length;
}
template <typename T>
void number_parameter(MYSQL_BIND& bind, enum_field_types type, T& value) {
    std::memset(&bind, 0, sizeof(bind));
    bind.buffer_type = type;
    bind.buffer = &value;
    bind.is_unsigned = 1;
}
void fail(MYSQL_STMT* value, const char* action) {
    throw std::runtime_error(std::string("MySQL camera ") + action + " failed: " +
                             mysql_stmt_error(value));
}

struct CameraResult {
    unsigned long long id;
    unsigned char type;
    unsigned long channels;
    char serial[129], ip[256], rtsp[2049], rtmp[2049];
    unsigned long lengths[4];
    MYSQL_BIND binds[7];
    CameraResult() : id(0), type(0), channels(0) {
        std::memset(serial, 0, sizeof(serial)); std::memset(ip, 0, sizeof(ip));
        std::memset(rtsp, 0, sizeof(rtsp)); std::memset(rtmp, 0, sizeof(rtmp));
        std::memset(lengths, 0, sizeof(lengths));
        number_parameter(binds[0], MYSQL_TYPE_LONGLONG, id);
        number_parameter(binds[1], MYSQL_TYPE_TINY, type);
        number_parameter(binds[2], MYSQL_TYPE_LONG, channels);
        char* buffers[4] = {serial, ip, rtsp, rtmp};
        unsigned long capacities[4] = {128, 255, 2048, 2048};
        for (int i = 0; i < 4; ++i) {
            std::memset(&binds[i + 3], 0, sizeof(MYSQL_BIND));
            binds[i + 3].buffer_type = MYSQL_TYPE_STRING;
            binds[i + 3].buffer = buffers[i];
            binds[i + 3].buffer_length = capacities[i];
            binds[i + 3].length = &lengths[i];
        }
    }
    CameraDevice camera() const {
        return CameraDevice{static_cast<std::uint32_t>(id), type,
            std::string(serial, lengths[0]), static_cast<std::uint32_t>(channels),
            std::string(ip, lengths[1]), std::string(rtsp, lengths[2]),
            std::string(rtmp, lengths[3])};
    }
};

}  // namespace

struct MysqlCameraRepository::Implementation { MYSQL connection; std::mutex mutex; };

MysqlCameraRepository::MysqlCameraRepository(const std::string& host, std::uint16_t port,
    const std::string& username, const std::string& password, const std::string& database)
    : implementation_(new Implementation()) {
    if (!mysql_init(&implementation_->connection)) throw std::runtime_error("mysql_init failed");
    if (!mysql_real_connect(&implementation_->connection, host.c_str(), username.c_str(),
                            password.c_str(), database.c_str(), port, NULL, 0)) {
        const std::string error(mysql_error(&implementation_->connection));
        mysql_close(&implementation_->connection);
        throw std::runtime_error("MySQL connection failed: " + error);
    }
    mysql_set_character_set(&implementation_->connection, "utf8mb4");
}
MysqlCameraRepository::~MysqlCameraRepository() { mysql_close(&implementation_->connection); }

bool MysqlCameraRepository::save(const CameraDevice& camera) {
    if (camera.id == 0 || camera.type > 1 || camera.channels == 0 ||
        camera.serial_number.empty() || camera.ip.empty() ||
        (camera.rtsp_url.empty() && camera.rtmp_url.empty())) return false;
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    CameraStatement statement(&implementation_->connection,
        "INSERT INTO cameras(id,type,serial_number,channels,ip,rtsp_url,rtmp_url) VALUES(?,?,?,?,?,?,?) ON DUPLICATE KEY UPDATE type=VALUES(type),serial_number=VALUES(serial_number),channels=VALUES(channels),ip=VALUES(ip),rtsp_url=VALUES(rtsp_url),rtmp_url=VALUES(rtmp_url)");
    unsigned long long id = camera.id; unsigned char type = camera.type;
    unsigned long channels = camera.channels; unsigned long lengths[4];
    MYSQL_BIND parameters[7];
    number_parameter(parameters[0], MYSQL_TYPE_LONGLONG, id);
    number_parameter(parameters[1], MYSQL_TYPE_TINY, type);
    text_parameter(parameters[2], camera.serial_number, lengths[0]);
    number_parameter(parameters[3], MYSQL_TYPE_LONG, channels);
    text_parameter(parameters[4], camera.ip, lengths[1]);
    text_parameter(parameters[5], camera.rtsp_url, lengths[2]);
    text_parameter(parameters[6], camera.rtmp_url, lengths[3]);
    if (mysql_stmt_bind_param(statement.value, parameters) != 0 ||
        mysql_stmt_execute(statement.value) != 0) fail(statement.value, "save");
    return true;
}

std::vector<CameraDevice> MysqlCameraRepository::list() {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    CameraStatement statement(&implementation_->connection,
        "SELECT id,type,channels,serial_number,ip,rtsp_url,rtmp_url FROM cameras ORDER BY id");
    if (mysql_stmt_execute(statement.value) != 0) fail(statement.value, "list");
    CameraResult result;
    if (mysql_stmt_bind_result(statement.value, result.binds) != 0 ||
        mysql_stmt_store_result(statement.value) != 0) fail(statement.value, "result");
    std::vector<CameraDevice> output;
    for (;;) {
        const int status = mysql_stmt_fetch(statement.value);
        if (status == MYSQL_NO_DATA) break;
        if (status != 0 && status != MYSQL_DATA_TRUNCATED) fail(statement.value, "fetch");
        output.push_back(result.camera());
    }
    return output;
}

bool MysqlCameraRepository::find(std::uint32_t camera_id, CameraDevice& output) {
    std::lock_guard<std::mutex> lock(implementation_->mutex);
    CameraStatement statement(&implementation_->connection,
        "SELECT id,type,channels,serial_number,ip,rtsp_url,rtmp_url FROM cameras WHERE id=? LIMIT 1");
    unsigned long long id = camera_id; MYSQL_BIND parameter[1];
    number_parameter(parameter[0], MYSQL_TYPE_LONGLONG, id);
    if (mysql_stmt_bind_param(statement.value, parameter) != 0 ||
        mysql_stmt_execute(statement.value) != 0) fail(statement.value, "find");
    CameraResult result;
    if (mysql_stmt_bind_result(statement.value, result.binds) != 0 ||
        mysql_stmt_store_result(statement.value) != 0) fail(statement.value, "result");
    const int status = mysql_stmt_fetch(statement.value);
    if (status == MYSQL_NO_DATA) return false;
    if (status != 0 && status != MYSQL_DATA_TRUNCATED) fail(statement.value, "fetch");
    output = result.camera();
    return true;
}

}  // namespace smart_home
