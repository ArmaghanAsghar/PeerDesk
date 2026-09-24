#include "host_server.hpp"

#include "peerdesk/auth.hpp"
#include "peerdesk/jpeg.hpp"
#include "peerdesk/map.hpp"
#include "peerdesk/protocol.hpp"
#include "peerdesk/tls.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << "\n";
        ++g_fails;
    } else {
        std::cout << "ok  " << msg << "\n";
    }
}

void test_map() {
    expect(peerdesk::map_coord(50, 100, 1920) == 960, "map_coord mid");
    expect(peerdesk::map_coord(0, 100, 1920) == 0, "map_coord origin");
    expect(peerdesk::map_coord(99, 100, 1920) == 1900, "map_coord near end");
    const auto box = peerdesk::fit_letterbox(800, 800, 1600, 900);
    expect(box.dest_w > 0 && box.dest_h > 0, "letterbox size");
    int hx = 0, hy = 0;
    expect(peerdesk::map_letterbox_point(box.dest_x, box.dest_y, box, 1600, 900, hx, hy) && hx == 0 &&
               hy == 0,
           "letterbox origin");
}

void test_protocol() {
    peerdesk::Hello h{1, "jordan"};
    const auto raw = peerdesk::pack_hello(h);
    const auto back = peerdesk::unpack_hello(raw);
    expect(back && back->username == "jordan" && back->version == 1, "hello roundtrip");

    peerdesk::AuthChallenge ch;
    ch.t_cost = 2;
    ch.salt.fill(7);
    ch.nonce.fill(3);
    const auto cb = peerdesk::unpack_challenge(peerdesk::pack_challenge(ch));
    expect(cb && cb->t_cost == 2 && cb->salt[0] == 7, "challenge roundtrip");

    peerdesk::MouseEvent m{peerdesk::MouseAction::Down, 1, 100, 200, 0};
    const auto mb = peerdesk::unpack_mouse(peerdesk::pack_mouse(m));
    expect(mb && mb->x == 100 && mb->y == 200 && mb->action == peerdesk::MouseAction::Down,
           "mouse roundtrip");
}

void test_auth() {
    peerdesk::AuthChallenge ch;
    ch.t_cost = peerdesk::kArgonT;
    ch.m_cost = peerdesk::kArgonM;
    ch.parallelism = peerdesk::kArgonP;
    expect(peerdesk::random_bytes(ch.salt), "salt rng");
    expect(peerdesk::random_bytes(ch.nonce), "nonce rng");
    const auto hash = peerdesk::argon2id_raw("peerdesk", ch.salt, ch.t_cost, ch.m_cost, ch.parallelism);
    const auto good = peerdesk::make_auth_response("peerdesk", ch);
    const auto bad = peerdesk::make_auth_response("nope", ch);
    expect(peerdesk::verify_auth_response(hash, ch, good), "hmac accepts good password");
    expect(!peerdesk::verify_auth_response(hash, ch, bad), "hmac rejects bad password");
}

void test_jpeg() {
    const int w = 16, h = 16;
    std::vector<uint8_t> rgb(static_cast<size_t>(w * h * 3));
    for (size_t i = 0; i < rgb.size(); i += 3) {
        rgb[i] = 200;
        rgb[i + 1] = 40;
        rgb[i + 2] = 40;
    }
    std::vector<uint8_t> jpeg;
    expect(peerdesk::encode_jpeg_rgb(rgb, w, h, 70, jpeg) && jpeg.size() > 20, "jpeg encode");
    std::vector<uint8_t> out;
    int ow = 0, oh = 0;
    expect(peerdesk::decode_jpeg_rgb(jpeg, out, ow, oh) && ow == w && oh == h, "jpeg decode");
}

bool client_hello_auth(peerdesk::TlsConn& c, const std::string& user, const std::string& pass,
                       peerdesk::MsgType& t, std::vector<uint8_t>& payload) {
    peerdesk::Hello h{peerdesk::kProtocolVersion, user};
    if (!c.send(peerdesk::MsgType::Hello, peerdesk::pack_hello(h))) return false;
    if (!c.recv(t, payload, 8000) || t != peerdesk::MsgType::AuthChallenge) return false;
    const auto ch = peerdesk::unpack_challenge(payload);
    if (!ch) return false;
    const auto resp = peerdesk::make_auth_response(pass, *ch);
    if (!c.send(peerdesk::MsgType::AuthResponse, peerdesk::pack_auth_response(resp))) return false;
    return c.recv(t, payload, 15000);
}

void test_session() {
    const auto dir = std::filesystem::temp_directory_path() / ("peerdesk-smoke-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);

    peerdesk::HostServer::Config cfg;
    cfg.port = 0;
    cfg.bind = "127.0.0.1";
    cfg.synthetic = true;
    cfg.inject = false;
    cfg.data_dir = dir;
    cfg.bootstrap_user = "jordan";
    cfg.bootstrap_password = "peerdesk";
    cfg.fps = 8;

    peerdesk::HostServer server(cfg);
    std::string err;
    expect(server.setup(err), "server setup");
    if (!err.empty() && !server.is_listening()) {
        std::cerr << "setup: " << err << "\n";
        return;
    }
    const auto port = server.port();
    expect(port != 0, "ephemeral port");
    std::thread th([&] { server.run(); });

    // Bad password
    {
        std::string e;
        auto c = peerdesk::TlsConn::connect("127.0.0.1", port, e);
        expect(c.has_value(), "tls connect (bad-pass path)");
        if (c) {
            peerdesk::MsgType t{};
            std::vector<uint8_t> payload;
            expect(client_hello_auth(*c, "jordan", "wrong", t, payload), "auth exchange (bad)");
            expect(t == peerdesk::MsgType::AuthFail, "wrong password -> AuthFail");
            const auto r = peerdesk::unpack_auth_fail(payload);
            expect(r && *r == peerdesk::AuthFailReason::BadCredentials, "AuthFail is bad-credentials");
        }
    }

    // Good password + video + busy + reconnect
    std::string e;
    auto live = peerdesk::TlsConn::connect("127.0.0.1", port, e);
    expect(live.has_value(), "tls connect (good)");
    peerdesk::MsgType t{};
    std::vector<uint8_t> payload;
    expect(live && client_hello_auth(*live, "jordan", "peerdesk", t, payload), "auth exchange (good)");
    expect(t == peerdesk::MsgType::AuthOk, "good password -> AuthOk");
    const auto ok = peerdesk::unpack_auth_ok(payload);
    expect(ok && ok->width >= 320 && ok->height >= 200, "auth-ok has host size");

    bool got_frame = false;
    for (int i = 0; i < 40 && live; ++i) {
        if (!live->recv(t, payload, 500)) continue;
        if (t == peerdesk::MsgType::VideoFrame) {
            const auto v = peerdesk::unpack_video(payload);
            got_frame = v && !v->jpeg.empty();
            break;
        }
    }
    expect(got_frame, "received a video frame");

    auto busy = peerdesk::TlsConn::connect("127.0.0.1", port, e);
    expect(busy.has_value(), "second viewer can reach host");
    if (busy) {
        expect(client_hello_auth(*busy, "jordan", "peerdesk", t, payload), "second viewer auth");
        expect(t == peerdesk::MsgType::AuthFail, "second viewer rejected");
        const auto r = peerdesk::unpack_auth_fail(payload);
        expect(r && *r == peerdesk::AuthFailReason::SessionBusy, "busy reason");
        busy->close();
    }

    if (live) {
        live->send(peerdesk::MsgType::Disconnect, {});
        live->close();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto again = peerdesk::TlsConn::connect("127.0.0.1", port, e);
    expect(again.has_value(), "reconnect tls");
    expect(again && client_hello_auth(*again, "jordan", "peerdesk", t, payload), "reconnect auth");
    expect(t == peerdesk::MsgType::AuthOk, "reconnect without restarting server");
    server.request_stop();
    if (again) again->close();
    th.join();
    std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    test_map();
    test_protocol();
    test_auth();
    test_jpeg();
    test_session();
    if (g_fails) {
        std::cerr << g_fails << " check(s) failed\n";
        return 1;
    }
    std::cout << "All PeerDesk smoke checks passed (J0/J1/J3 + mapping + JPEG).\n";
    return 0;
}
