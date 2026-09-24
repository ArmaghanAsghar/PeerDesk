#pragma once

#include "capture.hpp"
#include "inject.hpp"
#include "users.hpp"

#include "peerdesk/tls.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace peerdesk {

class HostServer {
public:
    struct Config {
        std::string bind = "0.0.0.0";
        uint16_t port = kDefaultPort;
        std::filesystem::path data_dir;
        bool synthetic = false;
        bool inject = true;
        int fps = 10;
        int jpeg_quality = 55;
        std::string bootstrap_user = "jordan";
        std::string bootstrap_password = "peerdesk";
    };

    explicit HostServer(Config cfg);
    bool setup(std::string& err);
    void run();
    void request_stop();
    uint16_t port() const { return listener_.port(); }
    bool is_listening() const { return listener_.is_open(); }

private:
    void handle_client(TlsConn conn);
    bool handshake(TlsConn& conn, int& width, int& height, std::string& err);
    void session_loop(TlsConn& conn, ScreenSource& source, InputInject* inject);

    Config cfg_;
    UserStore users_;
    TlsListener listener_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> session_active_{false};
    std::array<uint8_t, 32> fake_secret_{};
};

}  // namespace peerdesk
