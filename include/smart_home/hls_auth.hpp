#ifndef SMART_HOME_HLS_AUTH_HPP
#define SMART_HOME_HLS_AUTH_HPP

#include <cstdint>
#include <string>

namespace smart_home {

std::string hls_access_token(const std::string& secret,
                             std::uint32_t camera_id, std::uint32_t channel,
                             std::int64_t begin_ms, std::int64_t end_ms,
                             std::int64_t expires_seconds);
bool secure_token_equal(const std::string& left, const std::string& right);

}  // namespace smart_home
#endif
