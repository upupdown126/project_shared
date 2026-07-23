#include "smart_home/hls_auth.hpp"
#include "smart_home/md5.hpp"

namespace smart_home {

std::string hls_access_token(const std::string& secret, std::uint32_t camera,
    std::uint32_t channel, std::int64_t begin, std::int64_t end, std::int64_t expires) {
    return md5_hex(secret + "|" + std::to_string(camera) + "|" +
        std::to_string(channel) + "|" + std::to_string(begin) + "|" +
        std::to_string(end) + "|" + std::to_string(expires));
}
bool secure_token_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    unsigned char difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i)
        difference |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);
    return difference == 0;
}

}  // namespace smart_home
