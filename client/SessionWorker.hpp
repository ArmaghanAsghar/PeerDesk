#pragma once

#include "peerdesk/protocol.hpp"
#include "peerdesk/tls.hpp"

#include <QtCore/QObject>
#include <QtGui/QImage>

#include <atomic>
#include <mutex>
#include <queue>

namespace peerdesk {

// Runs one viewer session on a worker thread. It connects, authenticates,
// relays video frames to the UI, and sends queued input to the host.
class SessionWorker : public QObject {
    Q_OBJECT
public:
    explicit SessionWorker(QObject* parent = nullptr);

public slots:
    // Connect over TLS and run the Hello/Challenge/Response handshake. On success,
    // emits sessionReady() and blocks in pump() until the session ends. Failures
    // emit authFailed() (host rejected the login) or unreachable() (anything else).
    void connectToHost(QString host, quint16 port, QString username, QString password);
    // Thread-safe: may be called from the UI thread while pump() is blocked in recv.
    void enqueueMouse(quint8 action, quint8 button, quint16 x, quint16 y, qint16 wheel);
    void enqueueKey(quint8 down, quint32 keysym);
    // Ask pump() to stop and send the host a Disconnect. Called directly from the
    // UI thread, not queued.
    void disconnectSession();

signals:
    void statusChanged(QString message);
    void authFailed(QString message);
    void unreachable(QString message);
    void sessionReady(int width, int height);
    void frameArrived(QImage image);
    void sessionEnded(QString reason);

private:
    // Session loop. Sends queued input, pings every 2 s, and decodes VideoFrames
    // into frameArrived(). Always ends with sessionEnded().
    void pump();
    // Take the queued mouse and key events under the lock and send them in
    // order. Stops at the first failed send.
    void flush_input();

    TlsConn conn_;
    std::mutex mu_;
    std::queue<MouseEvent> mice_;
    std::queue<KeyEvent> keys_;
    std::atomic<bool> stop_{false};
};

}  // namespace peerdesk
