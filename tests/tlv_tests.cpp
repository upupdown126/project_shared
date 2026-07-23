#include "smart_home/tlv.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << std::endl;
    }
}

}  // namespace

int main() {
    const smart_home::TlvPacket login(smart_home::TaskType::LoginSection1, "alice");
    const std::vector<std::uint8_t> encoded = smart_home::TlvCodec::encode(login);
    check(encoded.size() == 13, "wire size is header plus value");
    check(encoded[0] == 0 && encoded[1] == 0 && encoded[2] == 0 && encoded[3] == 1,
          "type uses network byte order");
    check(encoded[4] == 0 && encoded[5] == 0 && encoded[6] == 0 && encoded[7] == 5,
          "length uses network byte order");

    smart_home::TlvStreamDecoder decoder;
    std::vector<smart_home::TlvPacket> packets = decoder.append(&encoded[0], 3);
    check(packets.empty(), "waits for a partial header");
    packets = decoder.append(&encoded[3], 6);
    check(packets.empty(), "waits for a partial value");
    packets = decoder.append(&encoded[9], encoded.size() - 9);
    check(packets.size() == 1 && packets[0].text() == "alice",
          "reassembles fragmented TCP reads");
    check(decoder.buffered_size() == 0, "consumes a complete packet");

    const smart_home::TlvPacket second(smart_home::TaskType::LoginSection2, "digest");
    std::vector<std::uint8_t> combined = smart_home::TlvCodec::encode(login);
    const std::vector<std::uint8_t> second_encoded = smart_home::TlvCodec::encode(second);
    combined.insert(combined.end(), second_encoded.begin(), second_encoded.end());
    packets = decoder.append(combined);
    check(packets.size() == 2, "splits coalesced TCP packets");
    check(packets[0].text() == "alice" && packets[1].text() == "digest",
          "preserves packet order and values");

    const std::uint8_t oversized_header[] = {0, 0, 0, 1, 0, 0, 0, 9};
    smart_home::TlvStreamDecoder limited(8);
    bool rejected = false;
    try {
        limited.append(oversized_header, sizeof(oversized_header));
    } catch (const std::length_error&) {
        rejected = true;
    }
    check(rejected, "rejects attacker-controlled oversized lengths");
    check(limited.buffered_size() == 0, "resets after a protocol violation");

    if (failures == 0) {
        std::cout << "all TLV tests passed" << std::endl;
    }
    return failures == 0 ? 0 : 1;
}

