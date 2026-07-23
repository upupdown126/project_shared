#ifndef SMART_HOME_CAMERA_API_HPP
#define SMART_HOME_CAMERA_API_HPP

#include <cstdint>
#include <map>
#include <string>

namespace smart_home {

struct HttpResponse {
    long status;
    std::string body;
};

class HttpClient {
public:
    virtual ~HttpClient() {}
    virtual HttpResponse get(const std::string& url,
                             const std::map<std::string, std::string>& headers) = 0;
};

class SocketHttpClient : public HttpClient {
public:
    explicit SocketHttpClient(int timeout_seconds = 10,
                              std::size_t maximum_response_size = 8U * 1024U * 1024U);
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers);
private:
    int timeout_seconds_;
    std::size_t maximum_response_size_;
};

#if defined(SMART_HOME_HAS_CURL)
class CurlHttpClient : public HttpClient {
public:
    explicit CurlHttpClient(long timeout_milliseconds = 10000);
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers);
private:
    long timeout_milliseconds_;
};
#endif

class CameraTokenSigner {
public:
    explicit CameraTokenSigner(const std::string& secret);
    std::string sign(const std::map<std::string, std::string>& parameters) const;
    std::string query(const std::map<std::string, std::string>& parameters) const;
private:
    std::string secret_;
};

class CameraApi {
public:
    CameraApi(HttpClient& client, const std::string& base_url, const std::string& secret);
    HttpResponse recording_list(std::int64_t begin_ms, std::int64_t end_ms,
                                int channel, std::int64_t timestamp_seconds);
    HttpResponse ptz(int channel, const std::string& command, int speed,
                     std::int64_t timestamp_seconds);
private:
    HttpResponse signed_get(const std::string& path,
                            std::map<std::string, std::string> parameters);
    static bool valid_ptz_command(const std::string& command);
    HttpClient& client_;
    std::string base_url_;
    CameraTokenSigner signer_;
};

}  // namespace smart_home
#endif
