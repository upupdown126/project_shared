#ifndef SMART_HOME_MEDIA_WIRE_HPP
#define SMART_HOME_MEDIA_WIRE_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace smart_home {

struct VideoMetadata {
    std::uint32_t camera_id;
    std::uint32_t codec_id;
    std::uint32_t width;
    std::uint32_t height;
    std::int32_t time_base_num;
    std::int32_t time_base_den;
    std::vector<std::uint8_t> codec_extra;
};

struct VideoPacket {
    std::uint32_t camera_id;
    std::uint32_t packet_id;
    std::int64_t pts;
    std::int64_t dts;
    std::uint32_t flags;
    std::vector<std::uint8_t> data;
};

std::vector<std::uint8_t> serialize_metadata(const VideoMetadata& value);
VideoMetadata deserialize_metadata(const std::vector<std::uint8_t>& wire);
std::vector<std::uint8_t> serialize_video_packet(const VideoPacket& value);
VideoPacket deserialize_video_packet(const std::vector<std::uint8_t>& wire);

struct PacketFragment {
    std::uint32_t packet_id;
    std::uint16_t index;
    std::uint16_t count;
    std::vector<std::uint8_t> data;
};

std::vector<std::uint8_t> serialize_fragment(const PacketFragment& fragment);
PacketFragment deserialize_fragment(const std::vector<std::uint8_t>& wire);
std::vector<PacketFragment> fragment_packet(std::uint32_t packet_id,
                                            const std::vector<std::uint8_t>& wire,
                                            std::size_t maximum_fragment_data);

class PacketReassembler {
public:
    explicit PacketReassembler(std::size_t maximum_packet_size = 16U * 1024U * 1024U);
    bool append(const PacketFragment& fragment, std::vector<std::uint8_t>& completed);
    void discard(std::uint32_t packet_id);
    std::size_t pending() const;
private:
    struct Assembly {
        std::uint16_t count;
        std::uint16_t next_index;
        std::vector<std::uint8_t> data;
    };
    std::size_t maximum_packet_size_;
    std::map<std::uint32_t, Assembly> assemblies_;
};

}  // namespace smart_home
#endif
