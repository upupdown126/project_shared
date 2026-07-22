#include "smart_home/media_stream.hpp"

#include <chrono>
#include <stdexcept>

namespace smart_home {

MediaStreamSession::MediaStreamSession(std::unique_ptr<MediaSource> source,
                                       std::shared_ptr<MediaSink> sink,
                                       std::size_t ring_capacity,
                                       std::size_t fragment_size)
    : source_(std::move(source)), sink_(sink),
      ring_(ring_capacity, RingOverflowPolicy::DropOldest),
      fragment_size_(fragment_size), running_(false), stopping_(false) {
    if (!source_ || !sink_ || fragment_size_ == 0)
        throw std::invalid_argument("media session dependencies are invalid");
}

MediaStreamSession::~MediaStreamSession() { stop(); }

void MediaStreamSession::start(std::uint32_t camera_id, const std::string& url) {
    if (running_.exchange(true)) throw std::runtime_error("media session already started");
    stopping_ = false;
    try {
        const VideoMetadata value = source_->open(camera_id, url);
        if (!sink_->metadata(value)) throw std::runtime_error("could not send stream metadata");
        reader_ = std::thread(&MediaStreamSession::reader_loop, this);
        sender_ = std::thread(&MediaStreamSession::sender_loop, this);
    } catch (...) {
        running_ = false;
        source_->close();
        throw;
    }
}

void MediaStreamSession::reader_loop() {
    try {
        VideoPacket packet;
        while (!stopping_ && source_->read(packet)) ring_.push(packet);
    } catch (const std::exception& error) {
        if (!stopping_) sink_->error(error.what());
    }
    ring_.close();
}

void MediaStreamSession::sender_loop() {
    try {
        VideoPacket packet;
        while (!stopping_ && ring_.wait_pop(packet, std::chrono::milliseconds(200))) {
            const std::vector<std::uint8_t> wire = serialize_video_packet(packet);
            const std::vector<PacketFragment> fragments =
                fragment_packet(packet.packet_id, wire, fragment_size_);
            for (std::size_t i = 0; i < fragments.size(); ++i) {
                if (stopping_ || !sink_->fragment(fragments[i])) {
                    stopping_ = true;
                    break;
                }
            }
        }
    } catch (const std::exception& error) {
        if (!stopping_) sink_->error(error.what());
    }
    running_ = false;
}

void MediaStreamSession::stop() {
    if (!running_ && !reader_.joinable() && !sender_.joinable()) return;
    stopping_ = true;
    source_->request_stop();
    ring_.close();
    if (reader_.joinable()) reader_.join();
    if (sender_.joinable()) sender_.join();
    source_->close();
    running_ = false;
}

bool MediaStreamSession::running() const { return running_; }
std::size_t MediaStreamSession::dropped_packets() const { return ring_.dropped(); }

}  // namespace smart_home
