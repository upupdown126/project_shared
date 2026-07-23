#include "smart_home/camera_api.hpp"

#include <curl/curl.h>
#include <stdexcept>

namespace smart_home {
namespace {

std::size_t append_response(char* data, std::size_t size, std::size_t count, void* target) {
    std::string* body = static_cast<std::string*>(target);
    body->append(data, size * count);
    return size * count;
}

class CurlGlobal {
public:
    CurlGlobal() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("curl_global_init failed");
    }
    ~CurlGlobal() { curl_global_cleanup(); }
};

CurlGlobal& curl_global() { static CurlGlobal value; return value; }

}  // namespace

CurlHttpClient::CurlHttpClient(long timeout_milliseconds)
    : timeout_milliseconds_(timeout_milliseconds) {
    if (timeout_milliseconds_ <= 0) throw std::invalid_argument("HTTP timeout must be positive");
    curl_global();
}

HttpResponse CurlHttpClient::get(
    const std::string& url, const std::map<std::string, std::string>& headers) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");
    struct curl_slist* list = NULL;
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it) {
        list = curl_slist_append(list, (it->first + ": " + it->second).c_str());
    }
    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_milliseconds_);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) throw std::runtime_error(std::string("HTTP request failed: ") + curl_easy_strerror(code));
    return HttpResponse{status, body};
}

}  // namespace smart_home
