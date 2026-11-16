#include "net/socket.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <system_error>
#include <utility>

namespace net {

namespace {

[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(errno, std::generic_category(), what);
}

// Turn off Nagle's algorithm. By default TCP holds back small writes for up to ~40 ms to
// combine them into bigger packets. Our messages are tiny and must arrive every tick, so
// that delay would be far worse than a few extra packet headers.
void set_no_delay(int fd) {
    const int on = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on) != 0) {
        throw_errno("setsockopt(TCP_NODELAY)");
    }
}

}  // namespace

Socket::~Socket() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

Socket::Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

void Socket::shutdown_both() const {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);  // errors ignored: the peer may already be gone
    }
}

Socket listen_tcp(std::uint16_t port) {
    Socket listener(::socket(AF_INET, SOCK_STREAM, 0));
    if (listener.fd() < 0) {
        throw_errno("socket");
    }

    // Allow restarting the server straight away. Without this, bind() fails for about a
    // minute after a restart while old connections sit in the TIME_WAIT state.
    const int on = 1;
    if (setsockopt(listener.fd(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) != 0) {
        throw_errno("setsockopt(SO_REUSEADDR)");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);  // every interface, so Docker can reach it later
    address.sin_port = htons(port);               // the OS wants network byte order
    if (bind(listener.fd(), reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0) {
        throw_errno("bind");
    }
    if (listen(listener.fd(), SOMAXCONN) != 0) {
        throw_errno("listen");
    }
    return listener;
}

std::uint16_t local_port(const Socket& socket) {
    sockaddr_in address{};
    socklen_t size = sizeof address;
    if (getsockname(socket.fd(), reinterpret_cast<sockaddr*>(&address), &size) != 0) {
        throw_errno("getsockname");
    }
    return ntohs(address.sin_port);
}

Socket accept_client(const Socket& listener) {
    Socket client(::accept(listener.fd(), nullptr, nullptr));
    if (client.fd() < 0) {
        throw_errno("accept");
    }
    set_no_delay(client.fd());
    return client;
}

Socket connect_tcp(const std::string& host, std::uint16_t port) {
    // getaddrinfo turns a host name into addresses (it also accepts plain IP addresses).
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    if (const int error = getaddrinfo(host.c_str(), service.c_str(), &hints, &results); error != 0) {
        throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(error)));
    }

    // Try each address until one connects. freeaddrinfo must run on every path.
    Socket connection;
    int last_errno = 0;
    for (addrinfo* a = results; a != nullptr; a = a->ai_next) {
        Socket attempt(::socket(a->ai_family, a->ai_socktype, a->ai_protocol));
        if (attempt.fd() >= 0 && ::connect(attempt.fd(), a->ai_addr, a->ai_addrlen) == 0) {
            connection = std::move(attempt);
            break;
        }
        last_errno = errno;
    }
    freeaddrinfo(results);

    if (connection.fd() < 0) {
        throw std::system_error(last_errno, std::generic_category(), "connect");
    }
    set_no_delay(connection.fd());
    return connection;
}

namespace {

void set_timeout(const Socket& socket, int option, std::chrono::milliseconds timeout) {
    timeval tv{};
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    if (setsockopt(socket.fd(), SOL_SOCKET, option, &tv, sizeof tv) != 0) {
        throw_errno("setsockopt(timeout)");
    }
}

}  // namespace

void set_send_timeout(const Socket& socket, std::chrono::milliseconds timeout) {
    set_timeout(socket, SO_SNDTIMEO, timeout);
}

void set_receive_timeout(const Socket& socket, std::chrono::milliseconds timeout) {
    set_timeout(socket, SO_RCVTIMEO, timeout);
}

Socket open_udp(std::uint16_t port) {
    Socket udp(::socket(AF_INET, SOCK_DGRAM, 0));
    if (udp.fd() < 0) {
        throw_errno("socket(UDP)");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(udp.fd(), reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0) {
        throw_errno("bind(UDP)");
    }
    return udp;
}

Endpoint resolve_udp(const std::string& host, std::uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    if (const int error = getaddrinfo(host.c_str(), service.c_str(), &hints, &results); error != 0) {
        throw std::runtime_error("getaddrinfo: " + std::string(gai_strerror(error)));
    }
    Endpoint endpoint;
    endpoint.address = *reinterpret_cast<const sockaddr_in*>(results->ai_addr);  // first IPv4 result
    freeaddrinfo(results);
    return endpoint;
}

bool send_datagram(const Socket& socket, const Endpoint& to, std::span<const std::uint8_t> bytes) {
    // MSG_DONTWAIT: if the OS send buffer is full, drop this datagram instead of stalling the
    // tick loop. Dropping is fine for UDP; the receiver already has to cope with loss.
    const ssize_t sent = ::sendto(socket.fd(), bytes.data(), bytes.size(), MSG_DONTWAIT,
                                  reinterpret_cast<const sockaddr*>(&to.address), sizeof to.address);
    return sent == static_cast<ssize_t>(bytes.size());
}

std::optional<Datagram> receive_datagram(const Socket& socket, bool wait) {
    // 65507 bytes is the largest possible UDP payload over IPv4. A smaller buffer would
    // silently cut longer datagrams short.
    std::vector<std::uint8_t> bytes(65507);
    Datagram datagram;
    socklen_t size = sizeof datagram.from.address;
    while (true) {
        const ssize_t received =
            ::recvfrom(socket.fd(), bytes.data(), bytes.size(), wait ? 0 : MSG_DONTWAIT,
                       reinterpret_cast<sockaddr*>(&datagram.from.address), &size);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received < 0) {
            return std::nullopt;  // nothing waiting, timed out, or an error
        }
        bytes.resize(static_cast<std::size_t>(received));
        datagram.bytes = std::move(bytes);
        return datagram;
    }
}

bool send_all(const Socket& socket, std::span<const std::uint8_t> bytes) {
    while (!bytes.empty()) {
        // MSG_NOSIGNAL: if the peer has gone, return an error instead of raising SIGPIPE,
        // which would kill the whole server by default.
        const ssize_t sent = ::send(socket.fd(), bytes.data(), bytes.size(), MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) {
            continue;  // interrupted by a signal before sending anything: just try again
        }
        if (sent <= 0) {
            return false;
        }
        bytes = bytes.subspan(static_cast<std::size_t>(sent));
    }
    return true;
}

bool receive_some(const Socket& socket, std::vector<std::uint8_t>& buffer) {
    std::uint8_t chunk[1024];
    while (true) {
        const ssize_t received = ::recv(socket.fd(), chunk, sizeof chunk, 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return false;  // 0 means the peer closed the connection cleanly
        }
        buffer.insert(buffer.end(), chunk, chunk + received);
        return true;
    }
}

}  // namespace net
