#pragma once

#include "peerdesk/protocol.hpp"
#include "peerdesk/tls.hpp"

#include <QtCore/QObject>
#include <QtGui/QImage>

#include <atomic>
#include <mutex>
#include <queue>

namespace peerdesk {

class SessionWorker : public QObject {
    Q_OBJECT
public:
    explicit SessionWorker(QObject* parent = nullptr);

public slots:
    void connectToHost(QString host, quint16 port, QString username, QString password);
    // Thread-safe: may be called from the UI thread while pump() is blocked in recv.
    void enqueueMouse(quint8 action, quint8 button, quint16 x, quint16 y, qint16 wheel);
    void enqueueKey(quint8 down, quint32 keysym);
    void disconnectSession();

signals:
    void statusChanged(QString message);
    void authFailed(QString message);
    void unreachable(QString message);
    void sessionReady(int width, int height);
    void frameArrived(QImage image);
    void sessionEnded(QString reason);

private:
    void pump();
    void flush_input();

    TlsConn conn_;
    std::mutex mu_;
    std::queue<MouseEvent> mice_;
    std::queue<KeyEvent> keys_;
    std::atomic<bool> stop_{false};
};

}  // namespace peerdesk
