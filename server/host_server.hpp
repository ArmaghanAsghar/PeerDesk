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

// The PeerDesk host. It accepts TLS viewers, authenticates them against the
// local user store, and streams the screen to one viewer at a time while
// injecting that viewer's mouse and keyboard input.
class HostServer {
public:
    struct Config {
        std::string bind = "0.0.0.0";
        uint16_t port = kDefaultPort;        // 0 = pick an ephemeral port
        std::filesystem::path data_dir;      // empty = $HOME/.peerdesk
        bool synthetic = false;              // draw SyntheticCapture instead of X11
        bool inject = true;                  // inject input via XTEST
        int fps = 10;
        int jpeg_quality = 55;
        std::string bootstrap_user = "jordan";        // created if no users file exists
        std::string bootstrap_password = "peerdesk";
    };

    explicit HostServer(Config cfg);

    // Prepare everything needed to serve. This loads the users file (or creates
    // one from the bootstrap login), refuses a Wayland session unless synthetic,
    // probes screen capture and XTEST, creates the TLS cert if missing, and
    // starts listening. Sets `err` and returns false on the first failure.
    bool setup(std::string& err);

    // Accept loop. Blocks until request_stop(). Runs the active session on a
    // worker thread, and handles connections that arrive during a session inline
    // so they are rejected with SessionBusy.
    void run();

    // Ask run() and any live session to exit. Async-signal-safe.
    void request_stop();
    uint16_t port() const { return listener_.port(); }
    bool is_listening() const { return listener_.is_open(); }

private:
    // Run handshake() for one connection and log the reason if it is rejected.
    void handle_client(TlsConn conn);

    // Authenticate the viewer (Hello -> AuthChallenge -> AuthResponse -> AuthOk).
    // On success, open capture and injection and run session_loop() until the
    // session ends. Unknown users get a deterministic fake salt so they cannot
    // be told apart from bad passwords. Returns false with `err` set if the
    // viewer is rejected or setup fails. `width`/`height` receive the host size.
    bool handshake(TlsConn& conn, int& width, int& height, std::string& err);

    // Stream frames from `source` on a capture thread while this thread handles
    // viewer messages (ping, mouse, key, disconnect). `inject` may be null. Ends
    // on disconnect, error, stop, or 8 s without traffic from the viewer.
    void session_loop(TlsConn& conn, ScreenSource& source, InputInject* inject);

    Config cfg_;
    UserStore users_;
    TlsListener listener_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> session_active_{false};
    std::array<uint8_t, 32> fake_secret_{};
};

}  // namespace peerdesk
