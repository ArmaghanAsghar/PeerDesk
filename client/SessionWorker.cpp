#include "SessionWorker.hpp"

#include "peerdesk/auth.hpp"

#include <QtCore/QDateTime>

namespace peerdesk {

SessionWorker::SessionWorker(QObject* parent) : QObject(parent) {}

void SessionWorker::connectToHost(QString host, quint16 port, QString username, QString password) {
    stop_ = false;
    emit statusChanged("Connecting over TLS…");
    std::string err;
    auto c = TlsConn::connect(host.toStdString(), port, err);
    if (!c) {
        emit unreachable(QString::fromStdString(err));
        return;
    }
    conn_ = std::move(*c);

    Hello hello;
    hello.username = username.toStdString();
    if (!conn_.send(MsgType::Hello, pack_hello(hello))) {
        emit unreachable("Connection dropped during hello");
        conn_.close();
        return;
    }

    MsgType t{};
    std::vector<uint8_t> payload;
    if (!conn_.recv(t, payload, 8000) || t != MsgType::AuthChallenge) {
        if (t == MsgType::AuthFail) {
            const auto r = unpack_auth_fail(payload);
            emit authFailed(r ? QString::fromUtf8(auth_fail_text(*r)) : "Authentication failed");
        } else {
            emit unreachable("Host did not start authentication");
        }
        conn_.close();
        return;
    }
    const auto ch = unpack_challenge(payload);
    if (!ch) {
        emit unreachable("Bad auth challenge");
        conn_.close();
        return;
    }
    emit statusChanged("Proving password (Argon2id + HMAC, password stays off the wire)…");
    const auto resp = make_auth_response(password.toStdString(), *ch);
    if (!conn_.send(MsgType::AuthResponse, pack_auth_response(resp))) {
        emit unreachable("Connection dropped during auth");
        conn_.close();
        return;
    }
    if (!conn_.recv(t, payload, 15000)) {
        emit unreachable("Host timed out during auth");
        conn_.close();
        return;
    }
    if (t == MsgType::AuthFail) {
        const auto r = unpack_auth_fail(payload);
        emit authFailed(r ? QString::fromUtf8(auth_fail_text(*r)) : "Authentication failed");
        conn_.close();
        return;
    }
    if (t != MsgType::AuthOk) {
        emit unreachable("Unexpected host reply after auth");
        conn_.close();
        return;
    }
    const auto ok = unpack_auth_ok(payload);
    if (!ok) {
        emit unreachable("Bad auth-ok");
        conn_.close();
        return;
    }
    emit sessionReady(ok->width, ok->height);
    pump();
}

void SessionWorker::enqueueMouse(quint8 action, quint8 button, quint16 x, quint16 y, qint16 wheel) {
    std::lock_guard<std::mutex> g(mu_);
    mice_.push(MouseEvent{static_cast<MouseAction>(action), button, x, y, wheel});
}

void SessionWorker::enqueueKey(quint8 down, quint32 keysym) {
    std::lock_guard<std::mutex> g(mu_);
    keys_.push(KeyEvent{down, keysym});
}

void SessionWorker::disconnectSession() {
    stop_ = true;
    if (conn_.is_open()) {
        conn_.send(MsgType::Disconnect, {});
    }
}

void SessionWorker::flush_input() {
    std::queue<MouseEvent> m;
    std::queue<KeyEvent> k;
    {
        std::lock_guard<std::mutex> g(mu_);
        m.swap(mice_);
        k.swap(keys_);
    }
    while (!m.empty()) {
        if (!conn_.send(MsgType::Mouse, pack_mouse(m.front()))) return;
        m.pop();
    }
    while (!k.empty()) {
        if (!conn_.send(MsgType::Key, pack_key(k.front()))) return;
        k.pop();
    }
}

void SessionWorker::pump() {
    auto last_ping = QDateTime::currentMSecsSinceEpoch();
    while (!stop_ && conn_.is_open()) {
        flush_input();
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (now - last_ping > 2000) {
            conn_.send(MsgType::Ping, {});
            last_ping = now;
        }
        MsgType t{};
        std::vector<uint8_t> payload;
        if (!conn_.recv(t, payload, 40)) {
            if (!conn_.is_open()) break;
            continue;
        }
        if (t == MsgType::VideoFrame) {
            const auto v = unpack_video(payload);
            if (!v || v->jpeg.empty()) continue;
            QImage img = QImage::fromData(v->jpeg.data(), static_cast<int>(v->jpeg.size()), "JPEG");
            if (!img.isNull()) emit frameArrived(img);
            continue;
        }
        if (t == MsgType::Pong) continue;
        if (t == MsgType::Error) {
            const auto msg = unpack_error(payload).value_or("Host error");
            emit sessionEnded(QString::fromStdString(msg));
            conn_.close();
            return;
        }
        if (t == MsgType::Disconnect) break;
    }
    conn_.close();
    emit sessionEnded(stop_ ? "Disconnected" : "Connection dropped");
}

}  // namespace peerdesk
