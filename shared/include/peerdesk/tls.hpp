#pragma once

#include "peerdesk/protocol.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace peerdesk {

// Initialise OpenSSL and ignore SIGPIPE so a dropped peer surfaces as a failed
// write instead of killing the process. Safe to call more than once.
void tls_library_init();

// Create a self-signed RSA-2048 key/certificate pair (CN=peerdesk-demo, valid
// 365 days) by running the `openssl` CLI, unless both files already exist.
// Returns true if both files exist afterwards.
bool ensure_self_signed_cert(const std::filesystem::path& key, const std::filesystem::path& crt);

// A move-only TLS connection carrying framed PeerDesk messages. Each frame is
// a 4-byte big-endian payload length, a 1-byte MsgType, then the payload.
class TlsConn {
public:
    TlsConn() = default;
    TlsConn(const TlsConn&) = delete;
    TlsConn& operator=(const TlsConn&) = delete;
    TlsConn(TlsConn&& o) noexcept;
    TlsConn& operator=(TlsConn&& o) noexcept;
    ~TlsConn();

    // Resolve `host`, open a TCP connection, and complete a TLS 1.2+ client
    // handshake. The server certificate is NOT verified (demo self-signed cert).
    // On failure returns nullopt and sets `err` to a user-facing reason.
    static std::optional<TlsConn> connect(const std::string& host, uint16_t port,
                                          std::string& err);

    // Send one frame. Returns false if the connection is closed, the payload
    // exceeds kMaxPayload, or the write fails or stalls for more than 5 s.
    bool send(MsgType type, std::span<const uint8_t> payload);

    // Receive one frame into `type`/`payload`. `timeout_ms` bounds the wait for
    // the header (-1 waits forever). Once a header has arrived, the body gets at
    // least 2 s (10 s when timeout_ms < 0). Returns false on timeout, error, or
    // an oversize frame. A timeout leaves the connection open (check is_open()).
    bool recv(MsgType& type, std::vector<uint8_t>& payload, int timeout_ms);

    // Shut down TLS, free OpenSSL state, and close the socket. Idempotent.
    void close();
    bool is_open() const { return ssl_ != nullptr && fd_ >= 0; }
    int fd() const { return fd_; }

private:
    friend class TlsListener;
    // Write all `n` bytes, retrying on WANT_READ/WANT_WRITE with a 5 s wait each time.
    bool write_all(const void* p, size_t n);
    // Read exactly `n` bytes, waiting up to `timeout_ms` whenever no data is buffered.
    bool read_all(void* p, size_t n, int timeout_ms);
    // Poll the socket for readability (or writability if `write`).
    bool wait(int timeout_ms, bool write);

    int fd_ = -1;
    void* ssl_ = nullptr;      // SSL*
    void* ctx_ = nullptr;      // SSL_CTX* owned by this conn when client
    bool owns_ctx_ = false;
};

// A TCP listening socket that accepts TLS connections with a PEM cert/key.
class TlsListener {
public:
    TlsListener() = default;
    TlsListener(const TlsListener&) = delete;
    TlsListener& operator=(const TlsListener&) = delete;
    ~TlsListener();

    // Load `cert`/`key` and listen on IPv4 `bind`:`port`. An empty bind or
    // "0.0.0.0" means all interfaces, and port 0 picks an ephemeral port (read it
    // back with port()). Closes any previous socket first. Sets `err` on failure.
    bool listen_on(const std::string& bind, uint16_t port, const std::filesystem::path& cert,
                   const std::filesystem::path& key, std::string& err);
    uint16_t port() const { return port_; }

    // Wait up to `timeout_ms` for one client and complete the TLS server
    // handshake. Returns nullopt with `err` untouched on timeout, or with `err`
    // set if accept or the handshake fails.
    std::optional<TlsConn> accept_one(int timeout_ms, std::string& err);

    // Close the socket and free the server SSL context. Idempotent.
    void close();
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    void* ctx_ = nullptr;
    uint16_t port_ = 0;
};

}  // namespace peerdesk
