#include "smart_home/recording_manager.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

class OnePacketSource : public smart_home::MediaSource {
public:
    OnePacketSource() : read_(false), stopping_(false) {}
    smart_home::VideoMetadata open(std::uint32_t camera, const std::string&) {
        return smart_home::VideoMetadata{camera, 27, 640, 360, 1, 1000,
                                         std::vector<std::uint8_t>()};
    }
    bool read(smart_home::VideoPacket& packet) {
        if (read_ || stopping_) return false;
        read_ = true;
        packet = smart_home::VideoPacket{1, 1, 100, 100, 1,
                                         std::vector<std::uint8_t>{1, 2, 3}};
        return true;
    }
    void request_stop() { stopping_ = true; }
    void close() {}
private:
    bool read_;
    bool stopping_;
};

class FileWriter : public smart_home::TsSegmentWriter {
public:
    void open(const std::string& path, const smart_home::VideoMetadata&) {
        output_.open(path.c_str(), std::ios::binary);
    }
    void write(const smart_home::VideoPacket& packet) {
        output_.write(reinterpret_cast<const char*>(&packet.data[0]),
                      static_cast<std::streamsize>(packet.data.size()));
    }
    void close() { output_.close(); }
private:
    std::ofstream output_;
};

int main() {
    smart_home::InMemoryRecordingRepository repository;
    smart_home::MountedStorage storage("recording_manager_test_data");
    smart_home::RecordingManager manager(
        [] { return std::unique_ptr<smart_home::MediaSource>(new OnePacketSource()); },
        [] { return std::unique_ptr<smart_home::TsSegmentWriter>(new FileWriter()); },
        repository, storage, 1000);
    std::string message;
    if (!manager.start(7, 2, "rtsp://camera/live", message)) {
        std::cerr << "FAIL: " << message << std::endl;
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::string duplicate;
    if (manager.start(7, 2, "rtsp://camera/live", duplicate)) {
        std::cerr << "FAIL: duplicate recording accepted" << std::endl;
        return 1;
    }
    if (!manager.stop(7, 2, message)) {
        std::cerr << "FAIL: " << message << std::endl;
        return 1;
    }
    const std::vector<smart_home::RecordingSegment> segments =
        repository.query(7, 2, 0, INT64_MAX);
    if (segments.size() != 1 || segments[0].size_bytes != 3 ||
        segments[0].checksum.empty() || manager.status(7, 2) != "not-found") {
        std::cerr << "FAIL: recording was not finalized and indexed" << std::endl;
        return 1;
    }
    std::cout << "Recording manager test passed" << std::endl;
    return 0;
}
