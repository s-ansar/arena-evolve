#include "net/http_server.hpp"

#include <algorithm>
#include <string>
#include <system_error>
#include <vector>

namespace net {

namespace {

constexpr std::size_t kMaxRequestBytes = 8192;  // a real GET request is a few hundred bytes

}  // namespace

HttpServer::HttpServer(std::uint16_t port, const Metrics& metrics)
    : listener_(listen_tcp(port)), port_(local_port(listener_)), metrics_(metrics), thread_([this] { serve(); }) {}

HttpServer::~HttpServer() {
    // Shutting down the listening socket makes the blocked accept() fail, which ends the loop.
    listener_.shutdown_both();
    thread_.join();
}

void HttpServer::serve() {
    while (true) {
        Socket client;
        try {
            client = accept_client(listener_);
        } catch (const std::system_error&) {
            return;  // listener shut down: time to stop
        }
        set_receive_timeout(client, std::chrono::seconds{1});
        set_send_timeout(client, std::chrono::seconds{1});

        // Read until the blank line that ends the headers, a size limit, or a timeout.
        std::vector<std::uint8_t> buffer;
        const std::string end_of_headers = "\r\n\r\n";
        while (buffer.size() < kMaxRequestBytes && receive_some(client, buffer)) {
            if (std::search(buffer.begin(), buffer.end(), end_of_headers.begin(), end_of_headers.end()) !=
                buffer.end()) {
                break;
            }
        }
        const std::string request(buffer.begin(), buffer.end());
        const std::string reply = http_response(request, metrics_);
        send_all(client, std::span(reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size()));
        // client goes out of scope here and the connection is closed
    }
}

}  // namespace net
