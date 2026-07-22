#ifndef SMART_HOME_TLV_HPP
#define SMART_HOME_TLV_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smart_home {

enum class TaskType : std::uint32_t {
    LoginSection1 = 1,
    LoginSection1Ok,
    LoginSection1Error,
    LoginSection2,
    LoginSection2Ok,
    LoginSection2Error,
    RegisterSection1,
    RegisterSection1Ok,
    RegisterSection1Error,
    RegisterSection2,
    RegisterSection2Ok,
    RegisterSection2Error,
    UserReady,
    GetCameras,
    Cameras,
    GetStream,
    StreamMetadata,
    StreamPacket,
    StopStream,
    CameraHttpRequest,
    CameraHttpResponse,
    PtzControl,
    GetRecordings,
    Recordings,
    Error,
    GetPlaybackUrl,
    PlaybackUrl,
    StartRecording,
    StopRecording,
    RecordingStatus,
    SaveCamera,
    CameraSaved
};

struct TlvPacket {
    std::uint32_t type;
    std::vector<std::uint8_t> value;

    TlvPacket();
    TlvPacket(std::uint32_t packet_type, const std::vector<std::uint8_t>& packet_value);
    TlvPacket(TaskType packet_type, const std::string& text);
    std::string text() const;
};

class TlvCodec {
public:
    static const std::size_t HeaderSize = 8;
    static const std::uint32_t DefaultMaximumValueSize = 16U * 1024U * 1024U;

    static std::vector<std::uint8_t> encode(const TlvPacket& packet);
};

class TlvStreamDecoder {
public:
    explicit TlvStreamDecoder(
        std::uint32_t maximum_value_size = TlvCodec::DefaultMaximumValueSize);

    std::vector<TlvPacket> append(const void* data, std::size_t size);
    std::vector<TlvPacket> append(const std::vector<std::uint8_t>& data);
    std::size_t buffered_size() const;
    void reset();

private:
    std::uint32_t maximum_value_size_;
    std::vector<std::uint8_t> buffer_;
    std::size_t read_position_;
};

}  // namespace smart_home

#endif
