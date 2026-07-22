#ifndef SMART_HOME_HLS_HTTP_SERVER_HPP
#define SMART_HOME_HLS_HTTP_SERVER_HPP

#include "smart_home/recording.hpp"
#include "smart_home/thread_pool.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace smart_home {

class HlsHttpServer {
public:
    HlsHttpServer(RecordingRepository& repository, const MountedStorage& storage,
                  const std::string& media_url_prefix,
                  const std::string& access_secret, std::size_t workers = 4);
    ~HlsHttpServer();
    void start(const std::string& host, std::uint16_t port);
    void stop();
    bool running() const;
private:
    HlsHttpServer(const HlsHttpServer&);
    HlsHttpServer& operator=(const HlsHttpServer&);
    void accept_loop(std::string host, std::uint16_t port);
    void handle(std::intptr_t socket);
    RecordingRepository& repository_;
    const MountedStorage& storage_;
    HlsPlaylistBuilder playlists_;
    std::string access_secret_;
    ThreadPool workers_;
    std::atomic<bool> running_;
    std::intptr_t listener_;
    std::thread accept_thread_;
};

}  // namespace smart_home
#endif
