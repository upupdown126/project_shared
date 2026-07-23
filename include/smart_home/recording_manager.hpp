#ifndef SMART_HOME_RECORDING_MANAGER_HPP
#define SMART_HOME_RECORDING_MANAGER_HPP

#include "smart_home/segment_recorder.hpp"
#include "smart_home/tcp_server.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace smart_home {

class RecordingManager {
public:
    typedef std::function<std::unique_ptr<MediaSource>()> SourceFactory;

    RecordingManager(const SourceFactory& source_factory,
                     const SegmentRecorder::WriterFactory& writer_factory,
                     RecordingRepository& repository,
                     const MountedStorage& storage,
                     std::int64_t segment_duration_ms = 10000,
                     std::int64_t retry_delay_ms = 5000);
    ~RecordingManager();

    bool start(std::uint32_t camera_id, std::uint32_t channel,
               const std::string& url, std::string& message);
    bool stop(std::uint32_t camera_id, std::uint32_t channel,
              std::string& message);
    std::string status(std::uint32_t camera_id, std::uint32_t channel) const;
    void stop_all();

private:
    struct Job;
    typedef std::pair<std::uint32_t, std::uint32_t> Key;

    SourceFactory source_factory_;
    SegmentRecorder::WriterFactory writer_factory_;
    RecordingRepository& repository_;
    const MountedStorage& storage_;
    std::int64_t segment_duration_ms_;
    std::int64_t retry_delay_ms_;
    mutable std::mutex mutex_;
    std::map<Key, std::shared_ptr<Job> > jobs_;
};

class RecordingProtocolHandler {
public:
    explicit RecordingProtocolHandler(RecordingManager& manager);
    bool handle(const TcpServer::ConnectionPtr& connection, const TlvPacket& packet);
private:
    RecordingManager& manager_;
};

}  // namespace smart_home
#endif
