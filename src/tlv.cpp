#include "smart_home/tlv.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace smart_home {
namespace {

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    output.push_back(static_cast<std::uint8_t>(value & 0xff));
}

std::uint32_t read_u32(const std::uint8_t* input) {
    return (static_cast<std::uint32_t>(input[0]) << 24) |
           (static_cast<std::uint32_t>(input[1]) << 16) |
           (static_cast<std::uint32_t>(input[2]) << 8) |
           static_cast<std::uint32_t>(input[3]);
}

}  // namespace

TlvPacket::TlvPacket() : type(0) {}

TlvPacket::TlvPacket(std::uint32_t packet_type,
                     const std::vector<std::uint8_t>& packet_value)
    : type(packet_type), value(packet_value) {}

TlvPacket::TlvPacket(TaskType packet_type, const std::string& text_value)
    : type(static_cast<std::uint32_t>(packet_type)),
      value(text_value.begin(), text_value.end()) {}

std::string TlvPacket::text() const {
    return std::string(value.begin(), value.end());
}

std::vector<std::uint8_t> TlvCodec::encode(const TlvPacket& packet) {
    if (packet.value.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("TLV value exceeds the 32-bit wire length");
    }

    std::vector<std::uint8_t> output;
    output.reserve(HeaderSize + packet.value.size());
    append_u32(output, packet.type);
    append_u32(output, static_cast<std::uint32_t>(packet.value.size()));
    output.insert(output.end(), packet.value.begin(), packet.value.end());
    return output;
}

TlvStreamDecoder::TlvStreamDecoder(std::uint32_t maximum_value_size)
    : maximum_value_size_(maximum_value_size), read_position_(0) {
    if (maximum_value_size_ == 0) {
        throw std::invalid_argument("maximum TLV value size must be positive");
    }
}

std::vector<TlvPacket> TlvStreamDecoder::append(const void* data, std::size_t size) {
    if (size != 0 && data == NULL) {
        throw std::invalid_argument("TLV input data is null");
    }
    const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
    if (size != 0) {
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    std::vector<TlvPacket> packets;
    while (buffer_.size() - read_position_ >= TlvCodec::HeaderSize) {
        const std::uint8_t* header = &buffer_[read_position_];
        const std::uint32_t type = read_u32(header);
        const std::uint32_t length = read_u32(header + 4);
        if (length > maximum_value_size_) {
            reset();
            throw std::length_error("TLV value exceeds configured maximum");
        }

        const std::size_t packet_size = TlvCodec::HeaderSize + length;
        if (buffer_.size() - read_position_ < packet_size) {
            break;
        }
        const std::uint8_t* value_begin = header + TlvCodec::HeaderSize;
        packets.push_back(TlvPacket(
            type, std::vector<std::uint8_t>(value_begin, value_begin + length)));
        read_position_ += packet_size;
    }

    if (read_position_ == buffer_.size()) {
        buffer_.clear();
        read_position_ = 0;
    } else if (read_position_ > 0 &&
               (read_position_ >= 4096 || read_position_ * 2 >= buffer_.size())) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + read_position_);
        read_position_ = 0;
    }
    return packets;
}

std::vector<TlvPacket> TlvStreamDecoder::append(
    const std::vector<std::uint8_t>& data) {
    return append(data.empty() ? NULL : &data[0], data.size());
}

std::size_t TlvStreamDecoder::buffered_size() const {
    return buffer_.size() - read_position_;
}

void TlvStreamDecoder::reset() {
    buffer_.clear();
    read_position_ = 0;
}

}  // namespace smart_home

