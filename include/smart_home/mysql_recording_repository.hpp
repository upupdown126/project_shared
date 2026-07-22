#ifndef SMART_HOME_MYSQL_RECORDING_REPOSITORY_HPP
#define SMART_HOME_MYSQL_RECORDING_REPOSITORY_HPP

#include "smart_home/recording.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace smart_home {

#if defined(SMART_HOME_HAS_MYSQL)
class MysqlRecordingRepository : public RecordingRepository {
public:
    MysqlRecordingRepository(const std::string& host, std::uint16_t port,
                             const std::string& username, const std::string& password,
                             const std::string& database);
    ~MysqlRecordingRepository();
    bool insert(RecordingSegment& segment);
    std::vector<RecordingSegment> query(std::uint32_t camera_id,
                                        std::uint32_t channel,
                                        std::int64_t begin_ms,
                                        std::int64_t end_ms);
    bool find_by_id(std::uint64_t id, RecordingSegment& output);
    std::vector<RecordingSegment> list_all();
    bool erase(std::uint64_t id);
private:
    MysqlRecordingRepository(const MysqlRecordingRepository&);
    MysqlRecordingRepository& operator=(const MysqlRecordingRepository&);
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
#endif

}  // namespace smart_home
#endif
