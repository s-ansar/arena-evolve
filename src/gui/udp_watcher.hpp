#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

#include "net/protocol.hpp"
#include "net/stream_stats.hpp"

namespace gui {

// Joins a running server as a UDP watcher (the same protocol as arena_watch), but driven by
// Qt's event loop instead of a blocking receive loop:
//   - a QTimer sends a Watch datagram every second to keep the lease alive;
//   - QUdpSocket emits readyRead whenever datagrams arrive, which calls readPending().
// Each valid State is announced with the stateReceived signal. Everything here runs on
// the GUI thread, so nothing needs a lock.
class UdpWatcher : public QObject {
    Q_OBJECT

public:
    // host may be a name or an address. Throws std::runtime_error if it cannot be resolved.
    UdpWatcher(const QString& host, quint16 port, QObject* parent = nullptr);

    const net::StreamStats& stats() const { return stats_; }

signals:
    void stateReceived(const net::State& state);

private slots:
    void readPending();
    void sendWatch();

private:
    QUdpSocket socket_;
    QHostAddress server_;
    quint16 port_;
    QTimer renew_;
    net::StreamStats stats_;
};

}  // namespace gui
