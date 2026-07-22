#include "smart_home/hls_http_server.hpp"
#include "smart_home/hls_auth.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace smart_home {
namespace {

#if defined(_WIN32)
typedef SOCKET HlsSocket;
const HlsSocket InvalidHlsSocket = INVALID_SOCKET;
void close_hls(HlsSocket socket) { closesocket(socket); }
class SocketRuntime {
public:
    SocketRuntime() { WSADATA data; if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("WSAStartup failed"); }
    ~SocketRuntime() { WSACleanup(); }
};
#else
typedef int HlsSocket;
const HlsSocket InvalidHlsSocket = -1;
void close_hls(HlsSocket socket) { close(socket); }
class SocketRuntime {};
#endif

SocketRuntime& runtime() { static SocketRuntime value; return value; }
HlsSocket native(std::intptr_t socket) { return static_cast<HlsSocket>(socket); }

void send_all(HlsSocket socket, const char* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        const int count = send(socket, data + offset, static_cast<int>(size - offset), 0);
        if (count <= 0) return;
        offset += static_cast<std::size_t>(count);
    }
}

void reply(HlsSocket socket, int status, const std::string& type, const std::string& body) {
    const char* reason = status == 200 ? "OK" : status == 400 ? "Bad Request" :
                         status == 403 ? "Forbidden" :
                         status == 404 ? "Not Found" : "Method Not Allowed";
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status << ' ' << reason
            << "\r\nContent-Type: " << type
            << "\r\nContent-Length: " << body.size()
            << "\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    const std::string text = headers.str();
    send_all(socket, text.data(), text.size());
    send_all(socket, body.data(), body.size());
}

void reply_file(HlsSocket socket, std::ifstream& input) {
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size < 0) { reply(socket, 404, "text/plain", "segment file missing"); return; }
    std::ostringstream headers;
    headers << "HTTP/1.1 200 OK\r\nContent-Type: video/mp2t\r\nContent-Length: "
            << size << "\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
    const std::string header = headers.str(); send_all(socket, header.data(), header.size());
    char chunk[64 * 1024];
    while (input) {
        input.read(chunk, sizeof(chunk));
        const std::streamsize count = input.gcount();
        if (count > 0) send_all(socket, chunk, static_cast<std::size_t>(count));
    }
}

std::map<std::string, std::string> query_values(const std::string& target) {
    std::map<std::string, std::string> values;
    const std::size_t query = target.find('?');
    if (query == std::string::npos) return values;
    std::size_t position = query + 1;
    while (position <= target.size()) {
        const std::size_t ampersand = target.find('&', position);
        const std::string pair = target.substr(position,
            ampersand == std::string::npos ? std::string::npos : ampersand - position);
        const std::size_t equals = pair.find('=');
        if (equals != std::string::npos)
            values[pair.substr(0, equals)] = pair.substr(equals + 1);
        if (ampersand == std::string::npos) break;
        position = ampersand + 1;
    }
    return values;
}

template <typename T>
bool number(const std::map<std::string, std::string>& values,
            const std::string& key, T& output) {
    typename std::map<std::string, std::string>::const_iterator found = values.find(key);
    if (found == values.end()) return false;
    std::istringstream input(found->second);
    std::string extra;
    return (input >> output) && !(input >> extra);
}

bool authorized(const std::map<std::string, std::string>& query,
                const std::string& secret, std::uint32_t camera, std::uint32_t channel,
                std::int64_t begin, std::int64_t end) {
    std::int64_t expires = 0;
    const std::map<std::string, std::string>::const_iterator token = query.find("token");
    if (!number(query, "expires", expires) || token == query.end()) return false;
    const std::int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return expires >= now && secure_token_equal(token->second,
        hls_access_token(secret, camera, channel, begin, end, expires));
}

}  // namespace

HlsHttpServer::HlsHttpServer(RecordingRepository& repository,
                             const MountedStorage& storage,
                             const std::string& media_url_prefix,
                             const std::string& access_secret,
                             std::size_t worker_count)
    : repository_(repository), storage_(storage), playlists_(media_url_prefix),
      access_secret_(access_secret), workers_(worker_count, 1024),
      running_(false), listener_(-1) {
    if (access_secret_.size() < 16) throw std::invalid_argument("HLS access secret must be at least 16 bytes");
    runtime();
}

HlsHttpServer::~HlsHttpServer() { stop(); }

void HlsHttpServer::start(const std::string& host, std::uint16_t port) {
    if (running_.exchange(true)) throw std::runtime_error("HLS server already running");
    workers_.start();
    accept_thread_ = std::thread(&HlsHttpServer::accept_loop, this, host, port);
}

void HlsHttpServer::accept_loop(std::string host, std::uint16_t port) {
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    struct addrinfo* addresses = NULL;
    const std::string port_text = std::to_string(port);
    if (getaddrinfo(host.empty() ? NULL : host.c_str(), port_text.c_str(), &hints, &addresses) != 0) {
        running_ = false;
        return;
    }
    HlsSocket listener = InvalidHlsSocket;
    for (struct addrinfo* address = addresses; address; address = address->ai_next) {
        listener = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (listener == InvalidHlsSocket) continue;
        int reuse = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        if (bind(listener, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0 &&
            listen(listener, SOMAXCONN) == 0) break;
        close_hls(listener);
        listener = InvalidHlsSocket;
    }
    freeaddrinfo(addresses);
    if (listener == InvalidHlsSocket) { running_ = false; return; }
    listener_ = static_cast<std::intptr_t>(listener);
    while (running_) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(listener, &reads);
        struct timeval timeout = {0, 100000};
        const int ready = select(static_cast<int>(listener + 1), &reads, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(listener, &reads)) {
            const HlsSocket client = accept(listener, NULL, NULL);
            if (client != InvalidHlsSocket &&
                !workers_.post([this, client] { handle(static_cast<std::intptr_t>(client)); }))
                close_hls(client);
        }
    }
    close_hls(listener);
    listener_ = -1;
}

void HlsHttpServer::handle(std::intptr_t value) {
    const HlsSocket socket = native(value);
    char buffer[8193] = {0};
    const int count = recv(socket, buffer, 8192, 0);
    if (count <= 0) { close_hls(socket); return; }
    const std::string request(buffer, static_cast<std::size_t>(count));
    const std::size_t line_end = request.find("\r\n");
    std::istringstream request_line(request.substr(0, line_end));
    std::string method, target, version;
    request_line >> method >> target >> version;
    if (method != "GET") { reply(socket, 405, "text/plain", "GET required"); close_hls(socket); return; }
    try {
        if (target.compare(0, 9, "/hls/vod?") == 0) {
            const std::map<std::string, std::string> query = query_values(target);
            std::uint32_t camera = 0, channel = 0;
            std::int64_t begin = 0, end = 0;
            if (!number(query, "camera", camera) || !number(query, "channel", channel) ||
                !number(query, "begin", begin) || !number(query, "end", end) || end <= begin)
                throw std::invalid_argument("invalid HLS query");
            if (!authorized(query, access_secret_, camera, channel, begin, end))
                reply(socket, 403, "text/plain", "invalid or expired HLS token");
            else {
                const std::vector<RecordingSegment> segments =
                    repository_.query(camera, channel, begin, end);
                if (segments.empty()) reply(socket, 404, "text/plain", "recording not found");
                else reply(socket, 200, "application/vnd.apple.mpegurl",
                           playlists_.vod(segments, target.substr(target.find('?'))));
            }
        } else if (target.compare(0, 7, "/media/") == 0) {
            const std::size_t query_at = target.find('?');
            const std::string path = target.substr(0, query_at);
            if (path.size() <= 10 || path.substr(path.size() - 3) != ".ts")
                throw std::invalid_argument("invalid media path");
            const std::string id_text = path.substr(7, path.size() - 10);
            std::uint64_t id = 0;
            std::istringstream id_input(id_text);
            if (!(id_input >> id)) throw std::invalid_argument("invalid media id");
            const std::map<std::string, std::string> query = query_values(target);
            std::uint32_t camera = 0, channel = 0; std::int64_t begin = 0, end = 0;
            if (!number(query, "camera", camera) || !number(query, "channel", channel) ||
                !number(query, "begin", begin) || !number(query, "end", end) || end <= begin)
                throw std::invalid_argument("invalid media query");
            RecordingSegment segment;
            if (!authorized(query, access_secret_, camera, channel, begin, end)) {
                reply(socket, 403, "text/plain", "invalid or expired HLS token");
            } else if (!repository_.find_by_id(id, segment) || segment.camera_id != camera ||
                segment.channel != channel || segment.end_ms <= begin || segment.start_ms >= end ||
                segment.path.compare(0, storage_.root().size(), storage_.root()) != 0) {
                reply(socket, 404, "text/plain", "segment not found");
            } else {
                std::ifstream input(segment.path.c_str(), std::ios::binary);
                if (!input) reply(socket, 404, "text/plain", "segment file missing");
                else reply_file(socket, input);
            }
        } else reply(socket, 404, "text/plain", "not found");
    } catch (const std::exception& error) {
        reply(socket, 400, "text/plain", error.what());
    }
    close_hls(socket);
}

void HlsHttpServer::stop() {
    running_ = false;
    if (accept_thread_.joinable()) accept_thread_.join();
    workers_.stop();
}

bool HlsHttpServer::running() const { return running_; }

}  // namespace smart_home
