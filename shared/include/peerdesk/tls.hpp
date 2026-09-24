#pragma once

#include "peerdesk/protocol.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace peerdesk {

void tls_library_init();

bool ensure_self_signed_cert(const std::filesystem::path& key, const std::filesystem::path& crt);

class TlsConn {
public:
    TlsConn() = default;
    TlsConn(const TlsConn&) = delete;
    TlsConn& operator=(const TlsConn&) = delete;
    TlsConn(TlsConn&& o) noexcept;
    TlsConn& operator=(TlsConn&& o) noexcept;
    ~TlsConn();

    static std::optional<TlsConn> connect(const std::string& host, uint16_t port,
                                          std::string& err);

    bool send(MsgType type, std::span<const uint8_t> payload);
    bool recv(MsgType& type, std::vector<uint8_t>& payload, int timeout_ms);
    void close();
    bool is_open() const { return ssl_ != nullptr && fd_ >= 0; }
    int fd() const { return fd_; }

private:
    friend class TlsListener;
    bool write_all(const void* p, size_t n);
    bool read_all(void* p, size_t n, int timeout_ms);
    bool wait(int timeout_ms, bool write);

    int fd_ = -1;
    void* ssl_ = nullptr;      // SSL*
    void* ctx_ = nullptr;      // SSL_CTX* owned by this conn when client
    bool owns_ctx_ = false;
};

class TlsListener {
public:
    TlsListener() = default;
    TlsListener(const TlsListener&) = delete;
    TlsListener& operator=(const TlsListener&) = delete;
    ~TlsListener();

    bool listen_on(const std::string& bind, uint16_t port, const std::filesystem::path& cert,
                   const std::filesystem::path& key, std::string& err);
    uint16_t port() const { return port_; }
    std::optional<TlsConn> accept_one(int timeout_ms, std::string& err);
    void close();
    bool is_open() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    void* ctx_ = nullptr;
    uint16_t port_ = 0;
};

}  // namespace peerdesk
