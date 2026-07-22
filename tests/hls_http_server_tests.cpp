#include "smart_home/camera_api.hpp"
#include "smart_home/hls_http_server.hpp"
#include "smart_home/hls_auth.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main() {
    smart_home::MountedStorage storage("hls_server_test_storage");
    smart_home::InMemoryRecordingRepository repository;
    const std::string path = storage.prepare_segment_path(3, 0, 1700000000000LL, 0);
    { std::ofstream output(path.c_str(), std::ios::binary); output << "TS-DATA"; }
    smart_home::RecordingSegment segment =
        {0, 3, 0, path, 1000, 11000, 7, "checksum", false};
    if (!repository.insert(segment)) return 1;

    const std::string secret = "test-hls-secret-123";
    smart_home::HlsHttpServer server(repository, storage, "/media", secret, 2);
    server.start("127.0.0.1", 39093);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    smart_home::SocketHttpClient client(3, 1024 * 1024);
    std::map<std::string, std::string> headers;
    const std::int64_t expires = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 60;
    const std::string query = "camera=3&channel=0&begin=0&end=20000&expires=" +
        std::to_string(expires) + "&token=" +
        smart_home::hls_access_token(secret, 3, 0, 0, 20000, expires);
    const smart_home::HttpResponse denied = client.get(
        "http://127.0.0.1:39093/hls/vod?camera=3&channel=0&begin=0&end=20000", headers);
    const smart_home::HttpResponse playlist = client.get(
        "http://127.0.0.1:39093/hls/vod?" + query, headers);
    const smart_home::HttpResponse media = client.get(
        "http://127.0.0.1:39093/media/" + std::to_string(segment.id) + ".ts?" + query, headers);
    server.stop();
    storage.remove_file(path);
    if (denied.status != 403 || playlist.status != 200 ||
        playlist.body.find("#EXTM3U") == std::string::npos ||
        playlist.body.find("token=") == std::string::npos ||
        media.status != 200 || media.body != "TS-DATA") return 1;
    std::cout << "HLS HTTP playlist and TS test passed" << std::endl;
    return 0;
}
