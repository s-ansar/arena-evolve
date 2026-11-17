#pragma once

#include <cstdint>
#include <thread>

#include "net/metrics.hpp"
#include "net/socket.hpp"

namespace net {

// A minimal HTTP server on its own thread, for /metrics and /health only. It handles one
// connection at a time: a monitoring scrape every few seconds needs nothing more. A
// client that connects and says nothing is dropped after a 1 s receive timeout, so it
// cannot block the thread for long.
//
// Not a general web server: no keep-alive, no request bodies, headers ignored.
class HttpServer {
public:
    // Start listening and serving. port 0 lets the OS pick one (see port()).
    HttpServer(std::uint16_t port, const Metrics& metrics);
    ~HttpServer();  // stops the thread

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    std::uint16_t port() const { return port_; }

private:
    void serve();

    Socket listener_;
    std::uint16_t port_;
    const Metrics& metrics_;  // owned by the Server, which outlives this object
    std::thread thread_;      // declared last: starts after everything it uses exists
};

}  // namespace net
