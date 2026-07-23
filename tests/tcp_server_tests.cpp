#include "smart_home/auth_protocol.hpp"
#include "smart_home/auth_service.hpp"
#include "smart_home/camera_api.hpp"
#include "smart_home/camera_protocol.hpp"
#include "smart_home/playback_protocol.hpp"
#include "smart_home/recording.hpp"
#include "smart_home/stream_protocol.hpp"
#include "smart_home/tcp_server.hpp"
#include "smart_home/tlv.hpp"
#include "smart_home/user_repository.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET TestSocket;
static void close_test_socket(TestSocket socket) { closesocket(socket); }
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int TestSocket;
static void close_test_socket(TestSocket socket) { close(socket); }
#endif

static smart_home::TlvPacket exchange_packet(TestSocket client,
                                             smart_home::TaskType type,
                                             const std::string& value) {
    const std::vector<std::uint8_t> request =
        smart_home::TlvCodec::encode(smart_home::TlvPacket(type, value));
    ::send(client, reinterpret_cast<const char*>(&request[0]),
           static_cast<int>(request.size()), 0);
    smart_home::TlvStreamDecoder decoder;
    for (;;) {
        std::uint8_t response[256];
        const int received = recv(client, reinterpret_cast<char*>(response), sizeof(response), 0);
        if (received <= 0) return smart_home::TlvPacket();
        const std::vector<smart_home::TlvPacket> packets =
            decoder.append(response, static_cast<std::size_t>(received));
        if (!packets.empty()) return packets[0];
    }
}

class FakeCameraHttp : public smart_home::HttpClient {
public:
    std::string last_url;
    smart_home::HttpResponse get(const std::string& url,
        const std::map<std::string, std::string>&) {
        last_url = url;
        return smart_home::HttpResponse{200, "{\"success\":true}"};
    }
};

class FakeNetworkMediaSource : public smart_home::MediaSource {
public:
    FakeNetworkMediaSource() : sent_(false), stopped_(false) {}
    smart_home::VideoMetadata open(std::uint32_t camera, const std::string&) {
        return smart_home::VideoMetadata{camera, 27, 320, 240, 1, 90000,
                                         std::vector<std::uint8_t>{1, 2}};
    }
    bool read(smart_home::VideoPacket& packet) {
        if (sent_ || stopped_) return false;
        sent_ = true;
        packet = smart_home::VideoPacket{9, 1, 100, 90, 1,
                                         std::vector<std::uint8_t>{4, 5, 6}};
        return true;
    }
    void request_stop() { stopped_ = true; }
    void close() {}
private:
    bool sent_;
    bool stopped_;
};

int main() {
#if defined(_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;
#endif
    const std::uint16_t port = 39091;
    smart_home::InMemoryUserRepository users;
    smart_home::AuthService authentication(users);
    smart_home::AuthProtocolHandler auth_protocol(authentication);
    FakeCameraHttp camera_http;
    smart_home::CameraApi camera_api(camera_http, "http://camera.local", "secret");
    smart_home::CameraProtocolHandler camera_protocol(camera_api);
    smart_home::InMemoryRecordingRepository recordings;
    smart_home::RecordingSegment recording = {0, 3, 0, "camera-3.ts", 1000,
                                                9000, 1024, "", false};
    recordings.insert(recording);
    smart_home::PlaybackProtocolHandler playback_protocol(
        recordings, "http://127.0.0.1:39093", "test-hls-secret-123");
    smart_home::StreamProtocolHandler stream_protocol(
        [] { return std::unique_ptr<smart_home::MediaSource>(new FakeNetworkMediaSource()); },
        4, 1024);
    smart_home::TcpServer server(2, 20);
    server.set_message_handler([&auth_protocol, &camera_protocol, &playback_protocol,
                                &stream_protocol](
        const smart_home::TcpServer::ConnectionPtr& connection,
        const smart_home::TlvPacket& packet) {
        if (auth_protocol.handle(connection, packet)) return;
        if (!auth_protocol.is_authenticated(connection)) {
            connection->send(smart_home::TlvPacket(smart_home::TaskType::Error,
                                                    "authentication required"));
            return;
        }
        if (camera_protocol.handle(connection, packet)) return;
        if (playback_protocol.handle(connection, packet)) return;
        stream_protocol.handle(connection, packet);
    });
    server.set_close_handler([&auth_protocol, &stream_protocol](
        const smart_home::TcpServer::ConnectionPtr& connection) {
        auth_protocol.connection_closed(connection);
        stream_protocol.connection_closed(connection);
    });
    std::thread server_thread([&server, port] { server.run("127.0.0.1", port); });

    TestSocket client = static_cast<TestSocket>(-1);
    for (int attempt = 0; attempt < 30; ++attempt) {
        client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
        if (connect(client, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) == 0) {
            break;
        }
        close_test_socket(client);
        client = static_cast<TestSocket>(-1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (client == static_cast<TestSocket>(-1)) {
        server.stop();
        server_thread.join();
        std::cerr << "FAIL: could not connect to loopback server" << std::endl;
        return 1;
    }

    const smart_home::TlvPacket unauthorized = exchange_packet(
        client, smart_home::TaskType::PtzControl, "0 left 50 100");
    const smart_home::TlvPacket register_begin = exchange_packet(
        client, smart_home::TaskType::RegisterSection1, "alice");
    const std::string setting = register_begin.text();
    const smart_home::TlvPacket register_finish = exchange_packet(
        client, smart_home::TaskType::RegisterSection2, "digest");
    const smart_home::TlvPacket login_begin = exchange_packet(
        client, smart_home::TaskType::LoginSection1, "alice");
    const smart_home::TlvPacket login_finish = exchange_packet(
        client, smart_home::TaskType::LoginSection2, "digest");
    const smart_home::TlvPacket ptz = exchange_packet(
        client, smart_home::TaskType::PtzControl, "0 left 50 100");
    const smart_home::TlvPacket playback = exchange_packet(
        client, smart_home::TaskType::GetPlaybackUrl, "3 0 0 10000");
    const std::vector<std::uint8_t> stream_request = smart_home::TlvCodec::encode(
        smart_home::TlvPacket(smart_home::TaskType::GetStream, "9 rtsp://camera/live"));
    ::send(client, reinterpret_cast<const char*>(&stream_request[0]),
           static_cast<int>(stream_request.size()), 0);
    smart_home::TlvStreamDecoder stream_decoder;
    bool received_metadata = false;
    bool received_packet = false;
    for (int attempt = 0; attempt < 10 && (!received_metadata || !received_packet); ++attempt) {
        std::uint8_t bytes[4096];
        const int count = recv(client, reinterpret_cast<char*>(bytes), sizeof(bytes), 0);
        if (count <= 0) break;
        const std::vector<smart_home::TlvPacket> messages =
            stream_decoder.append(bytes, static_cast<std::size_t>(count));
        for (std::size_t i = 0; i < messages.size(); ++i) {
            if (messages[i].type == static_cast<std::uint32_t>(smart_home::TaskType::StreamMetadata)) {
                const smart_home::VideoMetadata metadata =
                    smart_home::deserialize_metadata(messages[i].value);
                received_metadata = metadata.camera_id == 9 && metadata.width == 320;
            } else if (messages[i].type == static_cast<std::uint32_t>(smart_home::TaskType::StreamPacket)) {
                const smart_home::PacketFragment fragment =
                    smart_home::deserialize_fragment(messages[i].value);
                received_packet = fragment.packet_id == 1;
            }
        }
    }

    close_test_socket(client);
    server.stop();
    server_thread.join();
#if defined(_WIN32)
    WSACleanup();
#endif
    if (unauthorized.type != static_cast<std::uint32_t>(smart_home::TaskType::Error) ||
        register_begin.type != static_cast<std::uint32_t>(smart_home::TaskType::RegisterSection1Ok) ||
        setting.empty() ||
        register_finish.type != static_cast<std::uint32_t>(smart_home::TaskType::RegisterSection2Ok) ||
        login_begin.type != static_cast<std::uint32_t>(smart_home::TaskType::LoginSection1Ok) ||
        login_begin.text() != setting ||
        login_finish.type != static_cast<std::uint32_t>(smart_home::TaskType::LoginSection2Ok) ||
        ptz.type != static_cast<std::uint32_t>(smart_home::TaskType::CameraHttpResponse) ||
        ptz.text().find("200\n") != 0 || camera_http.last_url.find("command=left") == std::string::npos ||
        playback.type != static_cast<std::uint32_t>(smart_home::TaskType::PlaybackUrl) ||
        playback.text().find("/hls/vod?camera=3&channel=0&begin=0&end=10000") == std::string::npos ||
        !received_metadata || !received_packet) {
        std::cerr << "FAIL: end-to-end registration/login mismatch" << std::endl;
        return 1;
    }
    std::cout << "TCP registration/login integration test passed" << std::endl;
    return 0;
}
