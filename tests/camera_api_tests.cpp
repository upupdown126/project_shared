#include "smart_home/camera_api.hpp"
#include "smart_home/md5.hpp"
#include <iostream>
#include <stdexcept>

class FakeHttp : public smart_home::HttpClient {
public:
    std::string url;
    smart_home::HttpResponse get(const std::string& value,
        const std::map<std::string, std::string>&) { url = value; return smart_home::HttpResponse{200, "{}"}; }
};

int main() {
    if (smart_home::md5_hex("") != "d41d8cd98f00b204e9800998ecf8427e" ||
        smart_home::md5_hex("abc") != "900150983cd24fb0d6963f7d28e17f72") return 1;
    FakeHttp http;
    smart_home::CameraApi api(http, "http://192.0.2.100/", "secret-value");
    if (api.recording_list(1000, 2000, 0, 123).status != 200 ||
        http.url.find("/xsw/api/record/list?") == std::string::npos ||
        http.url.find("token=") == std::string::npos) return 1;
    api.ptz(1, "left", 50, 124);
    if (http.url.find("command=left") == std::string::npos) return 1;
    bool rejected = false;
    try { api.ptz(0, "erase", 10, 1); } catch (const std::invalid_argument&) { rejected = true; }
    if (!rejected) return 1;
    std::cout << "camera API signing and PTZ tests passed" << std::endl;
    return 0;
}
