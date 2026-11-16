#include "gui/udp_watcher.hpp"

#include <QHostInfo>
#include <QNetworkDatagram>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace gui {

UdpWatcher::UdpWatcher(const QString& host, quint16 port, QObject* parent)
    : QObject(parent), port_(port) {
    // Resolve the server's name once, keeping the first IPv4 address (the server is IPv4).
    for (const QHostAddress& address : QHostInfo::fromName(host).addresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            server_ = address;
            break;
        }
    }
    if (server_.isNull()) {
        throw std::runtime_error("cannot resolve " + host.toStdString());
    }

    socket_.bind(QHostAddress::AnyIPv4, 0);  // any free local port; the server replies to it
    // Signals and slots: connect() wires an event (left) to the function that handles it
    // (right). Qt calls the slot from its event loop each time the signal fires.
    connect(&socket_, &QUdpSocket::readyRead, this, &UdpWatcher::readPending);
    connect(&renew_, &QTimer::timeout, this, &UdpWatcher::sendWatch);
    renew_.start(1000);  // every second, like arena_watch
    sendWatch();         // and once straight away, so states start flowing immediately
}

void UdpWatcher::sendWatch() {
    const std::vector<std::uint8_t> frame = net::encode(net::Watch{});
    socket_.writeDatagram(reinterpret_cast<const char*>(frame.data()), static_cast<qint64>(frame.size()),
                          server_, port_);
}

void UdpWatcher::readPending() {
    while (socket_.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket_.receiveDatagram();
        const auto arrived = std::chrono::steady_clock::now();
        if (datagram.senderAddress().toIPv4Address() != server_.toIPv4Address() ||
            datagram.senderPort() != port_) {
            continue;  // not from our server
        }
        const QByteArray data = datagram.data();
        std::vector<std::uint8_t> bytes(data.begin(), data.end());
        try {
            const std::optional<net::Message> message = net::try_decode(bytes);
            const net::State* state = message ? std::get_if<net::State>(&*message) : nullptr;
            if (state == nullptr || !bytes.empty()) {
                continue;  // not exactly one State frame
            }
            // Same-machine latency, exactly as arena_watch measures it.
            const auto arrived_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(arrived.time_since_epoch());
            stats_.on_state(state->tick,
                            arrived_ns - std::chrono::nanoseconds{static_cast<std::int64_t>(state->sent_at_ns)});
            emit stateReceived(*state);
        } catch (const net::ProtocolError&) {
            // a broken datagram: ignore it
        }
    }
}

}  // namespace gui
