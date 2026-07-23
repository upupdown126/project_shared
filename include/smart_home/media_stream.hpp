#ifndef SMART_HOME_MEDIA_STREAM_HPP
#define SMART_HOME_MEDIA_STREAM_HPP

#include "smart_home/media_wire.hpp"
#include "smart_home/ring_buffer.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace smart_home {

class MediaSource {
public:
    virtual ~MediaSource() {}
    virtual VideoMetadata open(std::uint32_t camera_id, const std::string& url) = 0;
    virtual bool read(VideoPacket& packet) = 0;
    virtual void request_stop() = 0;
    virtual void close() = 0;
};

class MediaSink {
public:
    virtual ~MediaSink() {}
    virtual bool metadata(const VideoMetadata& value) = 0;
    virtual bool fragment(const PacketFragment& value) = 0;
    virtual void error(const std::string& message) = 0;
};

class MediaStreamSession {
public:
    MediaStreamSession(std::unique_ptr<MediaSource> source,
                       std::shared_ptr<MediaSink> sink,
                       std::size_t ring_capacity = 128,
                       std::size_t fragment_size = 64U * 1024U);
    ~MediaStreamSession();

    void start(std::uint32_t camera_id, const std::string& url);
    void stop();
    bool running() const;
    std::size_t dropped_packets() const;

private:
    MediaStreamSession(const MediaStreamSession&);
    MediaStreamSession& operator=(const MediaStreamSession&);
    void reader_loop();
    void sender_loop();

    std::unique_ptr<MediaSource> source_;
    std::shared_ptr<MediaSink> sink_;
    RingBuffer<VideoPacket> ring_;
    std::size_t fragment_size_;
    std::atomic<bool> running_;
    std::atomic<bool> stopping_;
    std::thread reader_;
    std::thread sender_;
};

#if defined(SMART_HOME_HAS_FFMPEG)
class FfmpegMediaSource : public MediaSource {
public:
    FfmpegMediaSource();
    ~FfmpegMediaSource();
    VideoMetadata open(std::uint32_t camera_id, const std::string& url);
    bool read(VideoPacket& packet);
    void request_stop();
    void close();
private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};
#endif

}  // namespace smart_home
#endif
