#include "host_server.hpp"

#include "peerdesk/auth.hpp"
#include "peerdesk/jpeg.hpp"

#include <algorithm>
#include <chrono>
#include <span>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace peerdesk {
namespace {

std::string mouse_line(const MouseEvent& e) {
    std::string a = "MOVE";
    if (e.action == MouseAction::Down) a = "DOWN";
    if (e.action == MouseAction::Up) a = "UP";
    if (e.action == MouseAction::Wheel) a = "WHEEL";
    return a + " " + std::to_string(e.x) + "," + std::to_string(e.y);
}

}  // namespace

HostServer::HostServer(Config cfg) : cfg_(std::move(cfg)) {}

bool HostServer::setup(std::string& err) {
    if (cfg_.data_dir.empty()) {
        const char* home = std::getenv("HOME");
        cfg_.data_dir = home ? std::filesystem::path(home) / ".peerdesk" : std::filesystem::path(".peerdesk");
    }
    std::error_code ec;
    std::filesystem::create_directories(cfg_.data_dir, ec);

    const auto users_path = cfg_.data_dir / "users";
    std::string load_err;
    if (!users_.load(users_path, load_err)) {
        if (!cfg_.bootstrap_user.empty() && !cfg_.bootstrap_password.empty()) {
            if (!users_.upsert(cfg_.bootstrap_user, cfg_.bootstrap_password) ||
                !users_.save(users_path, err)) {
                err = "Could not create host login";
                return false;
            }
        } else {
            err = "No credential configured";
            return false;
        }
    }

    if (!random_bytes(fake_secret_)) {
        err = "RNG failed";
        return false;
    }

    const bool wayland = [] {
        const char* s = std::getenv("XDG_SESSION_TYPE");
        return s && std::string(s) == "wayland";
    }();
    if (wayland && !cfg_.synthetic) {
        err = "X11 required for v1 (Wayland host is D1). Use --synthetic for a canvas-only demo.";
        return false;
    }

    auto source = cfg_.synthetic
                      ? std::unique_ptr<ScreenSource>(new SyntheticCapture())
                      : std::unique_ptr<ScreenSource>(new X11Capture());
    if (!source->open(err)) return false;
    // Probe only; real source is opened per session so X11 stays on the session thread.
    source.reset();

    if (cfg_.inject && !cfg_.synthetic) {
        InputInject probe;
        if (!probe.open(err)) return false;
    }

    const auto key = cfg_.data_dir / "server.key";
    const auto crt = cfg_.data_dir / "server.crt";
    if (!ensure_self_signed_cert(key, crt)) {
        err = "Could not create self-signed TLS certificate";
        return false;
    }
    if (!listener_.listen_on(cfg_.bind, cfg_.port, crt, key, err)) return false;
    return true;
}

void HostServer::request_stop() { stop_ = true; }

void HostServer::run() {
    std::thread session_th;
    while (!stop_) {
        std::string err;
        auto conn = listener_.accept_one(250, err);
        if (!conn) continue;
        if (session_th.joinable() && !session_active_) session_th.join();
        if (session_active_) {
            handle_client(std::move(*conn));
            continue;
        }
        if (session_th.joinable()) session_th.join();
        session_th = std::thread([this, c = std::move(*conn)]() mutable { handle_client(std::move(c)); });
    }
    if (session_th.joinable()) session_th.join();
}

bool HostServer::handshake(TlsConn& conn, int& width, int& height, std::string& err) {
    MsgType t{};
    std::vector<uint8_t> payload;
    if (!conn.recv(t, payload, 8000) || t != MsgType::Hello) {
        err = "Expected hello";
        conn.send(MsgType::AuthFail, pack_auth_fail(AuthFailReason::Protocol));
        return false;
    }
    const auto hello = unpack_hello(payload);
    if (!hello || hello->version != kProtocolVersion || hello->username.empty()) {
        conn.send(MsgType::AuthFail, pack_auth_fail(AuthFailReason::Protocol));
        err = "Bad hello";
        return false;
    }

    AuthChallenge ch;
    ch.t_cost = kArgonT;
    ch.m_cost = kArgonM;
    ch.parallelism = kArgonP;
    const auto user = users_.find(hello->username);
    if (user) {
        ch.salt = user->salt;
        ch.t_cost = user->t_cost;
        ch.m_cost = user->m_cost;
        ch.parallelism = user->parallelism;
    } else {
        // Unknown user: still issue a challenge (no account-oracle). HMAC will fail.
        const auto fake = hmac_sha256(fake_secret_, std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(hello->username.data()), hello->username.size()));
        std::copy(fake.begin(), fake.begin() + 16, ch.salt.begin());
    }
    if (!random_bytes(ch.nonce)) {
        err = "RNG failed";
        return false;
    }
    if (!conn.send(MsgType::AuthChallenge, pack_challenge(ch))) {
        err = "Send challenge failed";
        return false;
    }
    if (!conn.recv(t, payload, 15000) || t != MsgType::AuthResponse) {
        err = "Expected auth response";
        return false;
    }
    const auto resp = unpack_auth_response(payload);
    if (!resp) {
        conn.send(MsgType::AuthFail, pack_auth_fail(AuthFailReason::Protocol));
        return false;
    }

    const bool creds_ok = user && verify_auth_response(user->hash, ch, *resp);
    if (!creds_ok) {
        conn.send(MsgType::AuthFail, pack_auth_fail(AuthFailReason::BadCredentials));
        err = "Authentication failed";
        return false;
    }
    if (session_active_.exchange(true)) {
        conn.send(MsgType::AuthFail, pack_auth_fail(AuthFailReason::SessionBusy));
        err = "Host already has a viewer";
        session_active_ = true;  // keep the live session marked busy
        return false;
    }

    std::unique_ptr<ScreenSource> source = cfg_.synthetic
                                               ? std::unique_ptr<ScreenSource>(new SyntheticCapture())
                                               : std::unique_ptr<ScreenSource>(new X11Capture());
    if (!source->open(err)) {
        session_active_ = false;
        conn.send(MsgType::Error, pack_error(err));
        return false;
    }
    width = source->width();
    height = source->height();
    if (!conn.send(MsgType::AuthOk, pack_auth_ok(AuthOk{static_cast<uint16_t>(width),
                                                       static_cast<uint16_t>(height)}))) {
        session_active_ = false;
        err = "Send auth-ok failed";
        return false;
    }

    InputInject inject;
    InputInject* inj = nullptr;
    if (cfg_.inject) {
        std::string ierr;
        if (inject.open(ierr)) {
            inj = &inject;
        } else if (!cfg_.synthetic) {
            session_active_ = false;
            conn.send(MsgType::Error, pack_error(ierr));
            err = ierr;
            return false;
        }
    }
    session_loop(conn, *source, inj);
    session_active_ = false;
    return true;
}

void HostServer::handle_client(TlsConn conn) {
    int w = 0, h = 0;
    std::string err;
    if (!handshake(conn, w, h, err) && !err.empty()) {
        std::cerr << "peerdesk-server: session rejected: " << err << "\n";
    }
}

void HostServer::session_loop(TlsConn& conn, ScreenSource& source, InputInject* inject) {
    std::atomic<bool> live{true};
    std::mutex send_mu;
    std::thread cap([&] {
        std::vector<uint8_t> rgb, jpeg;
        const auto period = std::chrono::milliseconds(std::max(1000 / std::max(cfg_.fps, 1), 30));
        while (live && !stop_) {
            std::string err;
            if (!source.grab_rgb(rgb, err)) {
                std::lock_guard<std::mutex> g(send_mu);
                conn.send(MsgType::Error, pack_error(err.empty() ? "Capture failed" : err));
                live = false;
                break;
            }
            if (!encode_jpeg_rgb(rgb, source.width(), source.height(), cfg_.jpeg_quality, jpeg)) {
                std::lock_guard<std::mutex> g(send_mu);
                conn.send(MsgType::Error, pack_error("Encode failed"));
                live = false;
                break;
            }
            {
                std::lock_guard<std::mutex> g(send_mu);
                if (!conn.send(MsgType::VideoFrame,
                               pack_video(static_cast<uint16_t>(source.width()),
                                          static_cast<uint16_t>(source.height()), jpeg))) {
                    live = false;
                    break;
                }
            }
            std::this_thread::sleep_for(period);
        }
    });

    auto last_rx = std::chrono::steady_clock::now();
    while (live && !stop_) {
        MsgType t{};
        std::vector<uint8_t> payload;
        if (!conn.recv(t, payload, 250)) {
            if (!conn.is_open()) {
                live = false;
                break;
            }
            if (std::chrono::steady_clock::now() - last_rx > std::chrono::seconds(8)) {
                live = false;
                break;
            }
            continue;
        }
        last_rx = std::chrono::steady_clock::now();
        if (t == MsgType::Disconnect || t == MsgType::Error) {
            live = false;
            break;
        }
        if (t == MsgType::Ping) {
            std::lock_guard<std::mutex> g(send_mu);
            conn.send(MsgType::Pong, {});
            continue;
        }
        if (t == MsgType::Mouse) {
            if (const auto e = unpack_mouse(payload)) {
                source.note_input(mouse_line(*e));
                if (inject) inject->apply_mouse(*e);
            }
            continue;
        }
        if (t == MsgType::Key) {
            if (const auto e = unpack_key(payload)) {
                source.note_input(std::string("KEY ") + (e->down ? "DOWN " : "UP ") +
                                  std::to_string(e->keysym));
                if (inject) inject->apply_key(*e);
            }
        }
    }
    live = false;
    if (cap.joinable()) cap.join();
}

}  // namespace peerdesk
