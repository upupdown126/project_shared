#include "smart_home/camera_api.hpp"
#include "smart_home/md5.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace smart_home {
namespace {
std::string url_encode(const std::string& value) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out << c;
        else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return out.str();
}
}

CameraTokenSigner::CameraTokenSigner(const std::string& secret) : secret_(secret) {
    if (secret_.empty()) throw std::invalid_argument("camera API secret is empty");
}

std::string CameraTokenSigner::sign(
    const std::map<std::string, std::string>& parameters) const {
    std::map<std::string, std::string> signed_values(parameters);
    signed_values["secret"] = secret_;
    std::ostringstream canonical;
    for (std::map<std::string, std::string>::const_iterator it = signed_values.begin();
         it != signed_values.end(); ++it) {
        if (it != signed_values.begin()) canonical << '&';
        canonical << it->first << '=' << it->second;
    }
    return md5_hex(canonical.str());
}

std::string CameraTokenSigner::query(
    const std::map<std::string, std::string>& parameters) const {
    std::ostringstream output;
    for (std::map<std::string, std::string>::const_iterator it = parameters.begin();
         it != parameters.end(); ++it) {
        if (it != parameters.begin()) output << '&';
        output << url_encode(it->first) << '=' << url_encode(it->second);
    }
    return output.str();
}

CameraApi::CameraApi(HttpClient& client, const std::string& base_url,
                     const std::string& secret)
    : client_(client), base_url_(base_url), signer_(secret) {
    while (!base_url_.empty() && base_url_[base_url_.size() - 1] == '/') base_url_.erase(base_url_.size() - 1);
}

HttpResponse CameraApi::signed_get(const std::string& path,
                                   std::map<std::string, std::string> parameters) {
    parameters["token"] = signer_.sign(parameters);
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/json";
    return client_.get(base_url_ + path + "?" + signer_.query(parameters), headers);
}

HttpResponse CameraApi::recording_list(std::int64_t begin_ms, std::int64_t end_ms,
                                       int channel, std::int64_t timestamp_seconds) {
    if (begin_ms < 0 || end_ms < begin_ms || channel < 0) throw std::invalid_argument("invalid recording query");
    std::map<std::string, std::string> p;
    p["beginTime"] = std::to_string(begin_ms); p["endTime"] = std::to_string(end_ms);
    p["channel"] = std::to_string(channel); p["t"] = std::to_string(timestamp_seconds);
    return signed_get("/xsw/api/record/list", p);
}

bool CameraApi::valid_ptz_command(const std::string& command) {
    return command == "up" || command == "down" || command == "left" ||
           command == "right" || command == "stop" || command == "zoom_in" ||
           command == "zoom_out" || command == "left_up" ||
           command == "right_up" || command == "left_down" ||
           command == "right_down";
}

HttpResponse CameraApi::ptz(int channel, const std::string& command, int speed,
                            std::int64_t timestamp_seconds) {
    if (channel < 0 || !valid_ptz_command(command) || speed < 0 || speed > 100)
        throw std::invalid_argument("invalid PTZ request");
    std::map<std::string, std::string> p;
    p["channel"] = std::to_string(channel); p["command"] = command;
    p["speed"] = std::to_string(speed); p["t"] = std::to_string(timestamp_seconds);
    return signed_get("/xsw/api/ptz/control", p);
}

}  // namespace smart_home
