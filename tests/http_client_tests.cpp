#include "smart_home/camera_api.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET MockSocket;
static void close_mock(MockSocket value) { closesocket(value); }
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int MockSocket;
static void close_mock(MockSocket value) { close(value); }
#endif

int main() {
    smart_home::SocketHttpClient client(3, 1024 * 1024);
    const unsigned short port = 39092;
    MockSocket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        std::cerr << "FAIL: mock HTTP listener could not start" << std::endl;
        return 1;
    }
    std::string received_request;
    std::thread mock([&] {
        MockSocket connection = accept(listener, NULL, NULL);
        char buffer[2048];
        const int count = recv(connection, buffer, sizeof(buffer), 0);
        if (count > 0) received_request.assign(buffer, static_cast<std::size_t>(count));
        const std::string response =
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n"
            "7\r\n{\"ok\":1\r\n1\r\n}\r\n0\r\n\r\n";
        send(connection, response.data(), static_cast<int>(response.size()), 0);
        close_mock(connection);
        close_mock(listener);
    });
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/json";
    const smart_home::HttpResponse response =
        client.get("http://127.0.0.1:39092/test?q=1", headers);
    mock.join();
    if (response.status != 200 || response.body != "{\"ok\":1}" ||
        received_request.find("GET /test?q=1 HTTP/1.1") == std::string::npos ||
        received_request.find("Accept: application/json") == std::string::npos) {
        std::cerr << "FAIL: HTTP loopback request/response mismatch" << std::endl;
        return 1;
    }
    std::cout << "HTTP loopback and chunked decoding test passed" << std::endl;
    return 0;
}
