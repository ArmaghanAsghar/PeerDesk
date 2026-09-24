#include "peerdesk/tls.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <cstring>

namespace peerdesk {
namespace {

void set_cloexec(int fd) {
    const int flags = fcntl(fd, F_GETFD, 0);
    if (flags >= 0) fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

bool wait_fd(int fd, int timeout_ms, bool write) {
    pollfd p{};
    p.fd = fd;
    p.events = write ? POLLOUT : POLLIN;
    const int rc = poll(&p, 1, timeout_ms);
    return rc > 0 && (p.revents & (write ? POLLOUT : POLLIN));
}

SSL_CTX* new_server_ctx(const std::filesystem::path& cert, const std::filesystem::path& key,
                        std::string& err) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        err = "SSL_CTX_new failed";
        return nullptr;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(ctx) != 1) {
        err = "Failed to load TLS certificate/key";
        SSL_CTX_free(ctx);
        return nullptr;
    }
    return ctx;
}

SSL_CTX* new_client_ctx() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return nullptr;
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    // Demo slice: self-signed host cert. Production increment pins or uses a real CA.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    return ctx;
}

}  // namespace

void tls_library_init() {
    OPENSSL_init_ssl(0, nullptr);
    std::signal(SIGPIPE, SIG_IGN);
}

bool ensure_self_signed_cert(const std::filesystem::path& key, const std::filesystem::path& crt) {
    if (std::filesystem::exists(key) && std::filesystem::exists(crt)) return true;
    std::error_code ec;
    std::filesystem::create_directories(key.parent_path(), ec);
    const std::string cmd =
        "openssl req -x509 -newkey rsa:2048 -sha256 -days 365 -nodes -keyout '" + key.string() +
        "' -out '" + crt.string() + "' -subj '/CN=peerdesk-demo' >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0 && std::filesystem::exists(key) &&
           std::filesystem::exists(crt);
}

TlsConn::TlsConn(TlsConn&& o) noexcept { *this = std::move(o); }

TlsConn& TlsConn::operator=(TlsConn&& o) noexcept {
    if (this == &o) return *this;
    close();
    fd_ = o.fd_;
    ssl_ = o.ssl_;
    ctx_ = o.ctx_;
    owns_ctx_ = o.owns_ctx_;
    o.fd_ = -1;
    o.ssl_ = nullptr;
    o.ctx_ = nullptr;
    o.owns_ctx_ = false;
    return *this;
}

TlsConn::~TlsConn() { close(); }

void TlsConn::close() {
    if (ssl_) {
        auto* ssl = static_cast<SSL*>(ssl_);
        SSL_set_quiet_shutdown(ssl, 1);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl_ = nullptr;
    }
    if (owns_ctx_ && ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        ctx_ = nullptr;
    }
    owns_ctx_ = false;
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool TlsConn::wait(int timeout_ms, bool write) {
    if (fd_ < 0) return false;
    return wait_fd(fd_, timeout_ms, write);
}

bool TlsConn::write_all(const void* p, size_t n) {
    auto* ssl = static_cast<SSL*>(ssl_);
    const auto* b = static_cast<const uint8_t*>(p);
    size_t off = 0;
    while (off < n) {
        const int rc = SSL_write(ssl, b + off, static_cast<int>(n - off));
        if (rc > 0) {
            off += static_cast<size_t>(rc);
            continue;
        }
        const int err = SSL_get_error(ssl, rc);
        if (err == SSL_ERROR_WANT_WRITE) {
            if (!wait(5000, true)) return false;
            continue;
        }
        if (err == SSL_ERROR_WANT_READ) {
            if (!wait(5000, false)) return false;
            continue;
        }
        return false;
    }
    return true;
}

bool TlsConn::read_all(void* p, size_t n, int timeout_ms) {
    auto* ssl = static_cast<SSL*>(ssl_);
    auto* b = static_cast<uint8_t*>(p);
    size_t off = 0;
    while (off < n) {
        const int pending = SSL_pending(ssl);
        if (pending <= 0) {
            if (!wait(timeout_ms, false)) return false;
        }
        const int rc = SSL_read(ssl, b + off, static_cast<int>(n - off));
        if (rc > 0) {
            off += static_cast<size_t>(rc);
            continue;
        }
        const int err = SSL_get_error(ssl, rc);
        if (err == SSL_ERROR_WANT_READ) {
            if (!wait(timeout_ms, false)) return false;
            continue;
        }
        if (err == SSL_ERROR_WANT_WRITE) {
            if (!wait(timeout_ms, true)) return false;
            continue;
        }
        return false;
    }
    return true;
}

bool TlsConn::send(MsgType type, std::span<const uint8_t> payload) {
    if (!is_open() || payload.size() > kMaxPayload) return false;
    uint8_t hdr[5];
    const auto n = static_cast<uint32_t>(payload.size());
    hdr[0] = static_cast<uint8_t>(n >> 24);
    hdr[1] = static_cast<uint8_t>(n >> 16);
    hdr[2] = static_cast<uint8_t>(n >> 8);
    hdr[3] = static_cast<uint8_t>(n);
    hdr[4] = static_cast<uint8_t>(type);
    if (!write_all(hdr, sizeof(hdr))) return false;
    if (payload.empty()) return true;
    return write_all(payload.data(), payload.size());
}

bool TlsConn::recv(MsgType& type, std::vector<uint8_t>& payload, int timeout_ms) {
    payload.clear();
    if (!is_open()) return false;
    uint8_t hdr[5];
    if (!read_all(hdr, sizeof(hdr), timeout_ms)) return false;
    const uint32_t n = (static_cast<uint32_t>(hdr[0]) << 24) | (static_cast<uint32_t>(hdr[1]) << 16) |
                       (static_cast<uint32_t>(hdr[2]) << 8) | static_cast<uint32_t>(hdr[3]);
    if (n > kMaxPayload) return false;
    type = static_cast<MsgType>(hdr[4]);
    payload.resize(n);
    if (n == 0) return true;
    return read_all(payload.data(), n, timeout_ms < 0 ? 10000 : std::max(timeout_ms, 2000));
}

std::optional<TlsConn> TlsConn::connect(const std::string& host, uint16_t port, std::string& err) {
    tls_library_init();
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    const auto port_s = std::to_string(port);
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0 || !res) {
        err = "Host unreachable (DNS/address)";
        return std::nullopt;
    }
    int fd = -1;
    for (auto* a = res; a; a = a->ai_next) {
        fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        set_cloexec(fd);
        const int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        if (::connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        err = "Host unreachable (connect failed)";
        return std::nullopt;
    }

    SSL_CTX* ctx = new_client_ctx();
    if (!ctx) {
        ::close(fd);
        err = "TLS context failed";
        return std::nullopt;
    }
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        ::close(fd);
        err = "TLS handshake failed";
        return std::nullopt;
    }
    TlsConn c;
    c.fd_ = fd;
    c.ssl_ = ssl;
    c.ctx_ = ctx;
    c.owns_ctx_ = true;
    return c;
}

TlsListener::~TlsListener() { close(); }

void TlsListener::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (ctx_) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

bool TlsListener::listen_on(const std::string& bind, uint16_t port, const std::filesystem::path& cert,
                            const std::filesystem::path& key, std::string& err) {
    tls_library_init();
    close();
    SSL_CTX* ctx = new_server_ctx(cert, key, err);
    if (!ctx) return false;

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        err = "socket() failed";
        SSL_CTX_free(ctx);
        return false;
    }
    set_cloexec(fd);
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bind.empty() || bind == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, bind.c_str(), &addr.sin_addr) != 1) {
        err = "Invalid bind address";
        ::close(fd);
        SSL_CTX_free(ctx);
        return false;
    }
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        err = "Port already in use (bind failed)";
        ::close(fd);
        SSL_CTX_free(ctx);
        return false;
    }
    if (::listen(fd, 8) != 0) {
        err = "listen() failed";
        ::close(fd);
        SSL_CTX_free(ctx);
        return false;
    }
    sockaddr_in got{};
    socklen_t glen = sizeof(got);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&got), &glen) == 0) {
        port_ = ntohs(got.sin_port);
    } else {
        port_ = port;
    }
    fd_ = fd;
    ctx_ = ctx;
    return true;
}

std::optional<TlsConn> TlsListener::accept_one(int timeout_ms, std::string& err) {
    if (fd_ < 0) {
        err = "Not listening";
        return std::nullopt;
    }
    if (!wait_fd(fd_, timeout_ms, false)) return std::nullopt;
    const int cfd = ::accept(fd_, nullptr, nullptr);
    if (cfd < 0) {
        err = "accept() failed";
        return std::nullopt;
    }
    set_cloexec(cfd);
    const int one = 1;
    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    SSL* ssl = SSL_new(static_cast<SSL_CTX*>(ctx_));
    SSL_set_fd(ssl, cfd);
    if (SSL_accept(ssl) != 1) {
        SSL_free(ssl);
        ::close(cfd);
        err = "TLS accept handshake failed";
        return std::nullopt;
    }
    TlsConn c;
    c.fd_ = cfd;
    c.ssl_ = ssl;
    c.ctx_ = nullptr;
    c.owns_ctx_ = false;
    return c;
}

}  // namespace peerdesk
