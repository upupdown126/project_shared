#ifndef SMART_HOME_RECORDING_HPP
#define SMART_HOME_RECORDING_HPP

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace smart_home {

struct RecordingSegment {
    std::uint64_t id;
    std::uint32_t camera_id;
    std::uint32_t channel;
    std::string path;
    std::int64_t start_ms;
    std::int64_t end_ms;
    std::uint64_t size_bytes;
    std::string checksum;
    bool discontinuity;
};

class RecordingRepository {
public:
    virtual ~RecordingRepository() {}
    virtual bool insert(RecordingSegment& segment) = 0;
    virtual std::vector<RecordingSegment> query(std::uint32_t camera_id,
                                                std::uint32_t channel,
                                                std::int64_t begin_ms,
                                                std::int64_t end_ms) = 0;
    virtual bool find_by_id(std::uint64_t id, RecordingSegment& output) = 0;
    virtual std::vector<RecordingSegment> list_all() = 0;
    virtual bool erase(std::uint64_t id) = 0;
};

class InMemoryRecordingRepository : public RecordingRepository {
public:
    InMemoryRecordingRepository();
    bool insert(RecordingSegment& segment);
    std::vector<RecordingSegment> query(std::uint32_t camera_id,
                                        std::uint32_t channel,
                                        std::int64_t begin_ms,
                                        std::int64_t end_ms);
    bool find_by_id(std::uint64_t id, RecordingSegment& output);
    std::vector<RecordingSegment> list_all();
    bool erase(std::uint64_t id);
private:
    std::mutex mutex_;
    std::uint64_t next_id_;
    std::vector<RecordingSegment> segments_;
};

class MountedStorage {
public:
    explicit MountedStorage(const std::string& root);
    std::string prepare_segment_path(std::uint32_t camera_id,
                                     std::uint32_t channel,
                                     std::int64_t start_ms,
                                     std::uint32_t sequence) const;
    bool remove_file(const std::string& path) const;
    const std::string& root() const;
private:
    static void create_directories(const std::string& path);
    std::string root_;
};

class HlsPlaylistBuilder {
public:
    explicit HlsPlaylistBuilder(const std::string& media_url_prefix);
    std::string vod(const std::vector<RecordingSegment>& segments,
                    const std::string& media_query = std::string()) const;
private:
    std::string media_url_prefix_;
};

}  // namespace smart_home
#endif
