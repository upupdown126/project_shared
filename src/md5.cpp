#include "smart_home/md5.hpp"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace smart_home {
namespace {

std::uint32_t rotate_left(std::uint32_t value, std::uint32_t shift) {
    return (value << shift) | (value >> (32 - shift));
}

}  // namespace

std::string md5_hex(const std::string& input) {
    static const std::uint32_t shifts[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };
    static const std::uint32_t constants[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };

    std::vector<std::uint8_t> message(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(message.size()) * 8;
    message.push_back(0x80);
    while ((message.size() % 64) != 56) message.push_back(0);
    for (int i = 0; i < 8; ++i)
        message.push_back(static_cast<std::uint8_t>((bit_length >> (8 * i)) & 0xff));

    std::uint32_t a0 = 0x67452301;
    std::uint32_t b0 = 0xefcdab89;
    std::uint32_t c0 = 0x98badcfe;
    std::uint32_t d0 = 0x10325476;
    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::uint32_t words[16];
        for (int i = 0; i < 16; ++i) {
            const std::size_t p = offset + i * 4;
            words[i] = static_cast<std::uint32_t>(message[p]) |
                       (static_cast<std::uint32_t>(message[p + 1]) << 8) |
                       (static_cast<std::uint32_t>(message[p + 2]) << 16) |
                       (static_cast<std::uint32_t>(message[p + 3]) << 24);
        }
        std::uint32_t a = a0, b = b0, c = c0, d = d0;
        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t f, g;
            if (i < 16) { f = (b & c) | ((~b) & d); g = i; }
            else if (i < 32) { f = (d & b) | ((~d) & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | (~d)); g = (7 * i) % 16; }
            const std::uint32_t old_d = d;
            d = c;
            c = b;
            b += rotate_left(a + f + constants[i] + words[g], shifts[i]);
            a = old_d;
        }
        a0 += a; b0 += b; c0 += c; d0 += d;
    }

    const std::uint32_t state[4] = {a0, b0, c0, d0};
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            output << std::setw(2) << ((state[i] >> (8 * j)) & 0xff);
    return output.str();
}

}  // namespace smart_home

