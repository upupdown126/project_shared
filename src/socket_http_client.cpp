#include "smart_home/camera_api.hpp"

#include <cerrno>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace smart_home {
namespace {
#if defined(_WIN32)
typedef SOCKET HttpSocket;
const HttpSocket InvalidHttpSocket = INVALID_SOCKET;
void close_http_socket(HttpSocket value) { closesocket(value); }
class HttpSocketRuntime { public: HttpSocketRuntime() { WSADATA d; if (WSAStartup(MAKEWORD(2,2), &d)) throw std::runtime_error("WSAStartup failed"); } ~HttpSocketRuntime(){WSACleanup();} };
#else
typedef int HttpSocket;
const HttpSocket InvalidHttpSocket = -1;
void close_http_socket(HttpSocket value) { close(value); }
class HttpSocketRuntime {};
#endif

HttpSocketRuntime& runtime() { static HttpSocketRuntime value; return value; }

struct ParsedUrl { std::string host; std::string port; std::string target; };
ParsedUrl parse_url(const std::string& url) {
    const std::string prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0)
        throw std::invalid_argument("SocketHttpClient supports only http:// URLs");
    const std::size_t slash = url.find('/', prefix.size());
    const std::string authority = url.substr(prefix.size(), slash == std::string::npos ? std::string::npos : slash - prefix.size());
    if (authority.empty() || authority.find('@') != std::string::npos) throw std::invalid_argument("invalid HTTP URL");
    ParsedUrl parsed;
    const std::size_t colon = authority.rfind(':');
    if (colon != std::string::npos) { parsed.host = authority.substr(0, colon); parsed.port = authority.substr(colon + 1); }
    else { parsed.host = authority; parsed.port = "80"; }
    parsed.target = slash == std::string::npos ? "/" : url.substr(slash);
    if (parsed.host.empty() || parsed.port.empty()) throw std::invalid_argument("invalid HTTP URL authority");
    return parsed;
}

void send_all(HttpSocket socket, const std::string& request) {
    std::size_t offset = 0;
    while (offset < request.size()) {
        const int count = send(socket, request.data() + offset, static_cast<int>(request.size() - offset), 0);
        if (count <= 0) throw std::runtime_error("HTTP send failed");
        offset += static_cast<std::size_t>(count);
    }
}

std::string decode_chunked(const std::string& input) {
    std::string output;
    std::size_t position = 0;
    while (position < input.size()) {
        const std::size_t end = input.find("\r\n", position);
        if (end == std::string::npos) throw std::runtime_error("invalid chunked response");
        std::istringstream size_text(input.substr(position, end - position));
        std::size_t size = 0; size_text >> std::hex >> size;
        if (!size_text || size == 0) break;
        position = end + 2;
        if (position + size + 2 > input.size()) throw std::runtime_error("truncated chunked response");
        output.append(input, position, size);
        position += size;
        if (input.compare(position, 2, "\r\n") != 0) throw std::runtime_error("invalid chunk terminator");
        position += 2;
    }
    return output;
}
}

SocketHttpClient::SocketHttpClient(int timeout_seconds, std::size_t maximum_response_size)
    : timeout_seconds_(timeout_seconds), maximum_response_size_(maximum_response_size) {
    if (timeout_seconds_ <= 0 || maximum_response_size_ == 0) throw std::invalid_argument("invalid HTTP client limits");
    runtime();
}

HttpResponse SocketHttpClient::get(const std::string& url,
    const std::map<std::string, std::string>& headers) {
    const ParsedUrl parsed = parse_url(url);
    struct addrinfo hints; std::memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* addresses = NULL;
    if (getaddrinfo(parsed.host.c_str(), parsed.port.c_str(), &hints, &addresses) != 0) throw std::runtime_error("HTTP host resolution failed");
    HttpSocket socket = InvalidHttpSocket;
    for (struct addrinfo* address = addresses; address; address = address->ai_next) {
        socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == InvalidHttpSocket) continue;
#if defined(_WIN32)
        DWORD timeout = static_cast<DWORD>(timeout_seconds_ * 1000);
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval timeout = {timeout_seconds_, 0};
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
        if (connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) break;
        close_http_socket(socket); socket = InvalidHttpSocket;
    }
    freeaddrinfo(addresses);
    if (socket == InvalidHttpSocket) throw std::runtime_error("HTTP connection failed");
    try {
        std::ostringstream request;
        request << "GET " << parsed.target << " HTTP/1.1\r\nHost: " << parsed.host << "\r\nConnection: close\r\n";
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
            request << it->first << ": " << it->second << "\r\n";
        request << "\r\n";
        send_all(socket, request.str());
        std::string raw; char buffer[8192];
        for (;;) {
            const int count = recv(socket, buffer, sizeof(buffer), 0);
            if (count == 0) break;
            if (count < 0) throw std::runtime_error("HTTP receive failed or timed out");
            raw.append(buffer, static_cast<std::size_t>(count));
            if (raw.size() > maximum_response_size_) throw std::length_error("HTTP response exceeds maximum size");
        }
        close_http_socket(socket);
        const std::size_t headers_end = raw.find("\r\n\r\n");
        if (headers_end == std::string::npos) throw std::runtime_error("invalid HTTP response");
        std::istringstream status_line(raw.substr(0, raw.find("\r\n")));
        std::string version; long status = 0; status_line >> version >> status;
        if (version.compare(0, 5, "HTTP/") != 0 || status < 100 || status > 599) throw std::runtime_error("invalid HTTP status line");
        std::string body = raw.substr(headers_end + 4);
        std::string lower_headers = raw.substr(0, headers_end);
        for (std::size_t i = 0; i < lower_headers.size(); ++i) lower_headers[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower_headers[i])));
        if (lower_headers.find("transfer-encoding: chunked") != std::string::npos) body = decode_chunked(body);
        return HttpResponse{status, body};
    } catch (...) { close_http_socket(socket); throw; }
}

}  // namespace smart_home
