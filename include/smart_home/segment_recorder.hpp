#ifndef SMART_HOME_SEGMENT_RECORDER_HPP
#define SMART_HOME_SEGMENT_RECORDER_HPP

#include "smart_home/media_stream.hpp"
#include "smart_home/recording.hpp"

#include <atomic>
#include <functional>
#include <memory>

namespace smart_home {

class TsSegmentWriter {
public:
    virtual ~TsSegmentWriter() {}
    virtual void open(const std::string& path, const VideoMetadata& metadata) = 0;
    virtual void write(const VideoPacket& packet) = 0;
    virtual void close() = 0;
};

class SegmentRecorder {
public:
    typedef std::function<std::unique_ptr<TsSegmentWriter>()> WriterFactory;
    SegmentRecorder(std::unique_ptr<MediaSource> source,
                    const WriterFactory& writer_factory,
                    RecordingRepository& repository,
                    const MountedStorage& storage,
                    std::int64_t segment_duration_ms = 10000);
    ~SegmentRecorder();
    void run(std::uint32_t camera_id, std::uint32_t channel,
             const std::string& url, std::size_t maximum_packets = 0);
    void request_stop();
    void mark_discontinuity();
private:
    void finish_segment(std::int64_t end_ms);
    static std::int64_t now_ms();
    static std::uint64_t file_size(const std::string& path);
    static std::string file_checksum(const std::string& path);

    std::unique_ptr<MediaSource> source_;
    WriterFactory writer_factory_;
    RecordingRepository& repository_;
    const MountedStorage& storage_;
    std::int64_t segment_duration_ms_;
    std::atomic<bool> stopping_;
    std::unique_ptr<TsSegmentWriter> writer_;
    RecordingSegment current_;
    bool next_discontinuity_;
};

#if defined(SMART_HOME_HAS_FFMPEG)
class FfmpegTsSegmentWriter : public TsSegmentWriter {
public:
    FfmpegTsSegmentWriter();
    ~FfmpegTsSegmentWriter();
    void open(const std::string& path, const VideoMetadata& metadata);
    void write(const VideoPacket& packet);
    void close();
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
#endif

}  // namespace smart_home
#endif
