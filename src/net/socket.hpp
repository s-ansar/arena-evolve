#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Thin wrappers around POSIX TCP sockets. Linux only (see README: "Why raw POSIX sockets").
// Setup failures throw std::system_error with the errno; read and write failures return
// false, because a client disconnecting is normal, not exceptional.
namespace net {

// Owns one socket file descriptor and closes it in the destructor (RAII), so a socket can
// never leak, even when an exception is thrown. Move-only: two owners would close twice.
class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const { return fd_; }

    // Stop all reading and writing on the connection. Unlike close(), this is safe while
    // another thread is blocked in recv() or send() on the same socket: that call returns
    // at once. The server uses it to wake its reader threads at the end of a match.
    void shutdown_both() const;

private:
    int fd_ = -1;
};

// A listening socket on every network interface. port 0 lets the OS pick a free port
// (used by tests, so parallel test runs never clash).
Socket listen_tcp(std::uint16_t port);

// The port a socket is bound to. Useful after listen_tcp(0).
std::uint16_t local_port(const Socket& socket);

// Block until a client connects, and return the connection.
Socket accept_client(const Socket& listener);

// Connect to host (a name like "localhost" or an address like "127.0.0.1") and port.
Socket connect_tcp(const std::string& host, std::uint16_t port);

// Make send() give up (and send_all return false) if the peer has not read anything for
// this long. Without it, a client that stops reading would block its writer thread forever.
void set_send_timeout(const Socket& socket, std::chrono::milliseconds timeout);

// Send every byte. send() may write only part of the data, so this loops until done.
// Returns false if the connection is broken.
bool send_all(const Socket& socket, std::span<const std::uint8_t> bytes);

// Wait for some bytes and append them to buffer. Returns false when the peer has closed
// the connection or it failed.
bool receive_some(const Socket& socket, std::vector<std::uint8_t>& buffer);

}  // namespace net
