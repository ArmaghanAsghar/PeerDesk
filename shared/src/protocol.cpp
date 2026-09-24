#include "peerdesk/protocol.hpp"

#include <algorithm>

namespace peerdesk {

std::vector<uint8_t> pack_hello(const Hello& h) {
    std::vector<uint8_t> o;
    o.insert(o.end(), kHelloMagic, kHelloMagic + 4);
    write_u16be(o, h.version);
    std::array<uint8_t, kUsernameBytes> name{};
    const auto n = std::min(h.username.size(), kUsernameBytes);
    std::copy_n(h.username.begin(), n, name.begin());
    write_fixed(o, name);
    return o;
}

std::optional<Hello> unpack_hello(std::span<const uint8_t> p) {
    if (p.size() < 6 + kUsernameBytes) return std::nullopt;
    if (p[0] != 'P' || p[1] != 'D' || p[2] != 'S' || p[3] != 'K') return std::nullopt;
    Hello h;
    size_t i = 4;
    if (!read_u16be(p, i, h.version)) return std::nullopt;
    h.username.assign(reinterpret_cast<const char*>(p.data() + i), kUsernameBytes);
    h.username = trim_nul(h.username);
    return h;
}

std::vector<uint8_t> pack_challenge(const AuthChallenge& c) {
    std::vector<uint8_t> o;
    write_fixed(o, c.salt);
    write_fixed(o, c.nonce);
    write_u32be(o, c.t_cost);
    write_u32be(o, c.m_cost);
    write_u32be(o, c.parallelism);
    return o;
}

std::optional<AuthChallenge> unpack_challenge(std::span<const uint8_t> p) {
    AuthChallenge c;
    size_t i = 0;
    if (!read_fixed(p, i, c.salt)) return std::nullopt;
    if (!read_fixed(p, i, c.nonce)) return std::nullopt;
    if (!read_u32be(p, i, c.t_cost) || !read_u32be(p, i, c.m_cost) ||
        !read_u32be(p, i, c.parallelism)) {
        return std::nullopt;
    }
    return c;
}

std::vector<uint8_t> pack_auth_response(const AuthResponse& r) {
    std::vector<uint8_t> o;
    write_fixed(o, r.hmac);
    return o;
}

std::optional<AuthResponse> unpack_auth_response(std::span<const uint8_t> p) {
    AuthResponse r;
    size_t i = 0;
    if (!read_fixed(p, i, r.hmac)) return std::nullopt;
    return r;
}

std::vector<uint8_t> pack_auth_ok(const AuthOk& o) {
    std::vector<uint8_t> v;
    write_u16be(v, o.width);
    write_u16be(v, o.height);
    return v;
}

std::optional<AuthOk> unpack_auth_ok(std::span<const uint8_t> p) {
    AuthOk o;
    size_t i = 0;
    if (!read_u16be(p, i, o.width) || !read_u16be(p, i, o.height)) return std::nullopt;
    return o;
}

std::vector<uint8_t> pack_auth_fail(AuthFailReason r) {
    return {static_cast<uint8_t>(r)};
}

std::optional<AuthFailReason> unpack_auth_fail(std::span<const uint8_t> p) {
    if (p.empty()) return std::nullopt;
    return static_cast<AuthFailReason>(p[0]);
}

std::vector<uint8_t> pack_mouse(const MouseEvent& e) {
    std::vector<uint8_t> o;
    write_u8(o, static_cast<uint8_t>(e.action));
    write_u8(o, e.button);
    write_u16be(o, e.x);
    write_u16be(o, e.y);
    write_u16be(o, static_cast<uint16_t>(e.wheel_delta));
    return o;
}

std::optional<MouseEvent> unpack_mouse(std::span<const uint8_t> p) {
    MouseEvent e;
    size_t i = 0;
    uint8_t action = 0;
    uint16_t wheel = 0;
    if (!read_u8(p, i, action) || !read_u8(p, i, e.button) || !read_u16be(p, i, e.x) ||
        !read_u16be(p, i, e.y) || !read_u16be(p, i, wheel)) {
        return std::nullopt;
    }
    e.action = static_cast<MouseAction>(action);
    e.wheel_delta = static_cast<int16_t>(wheel);
    return e;
}

std::vector<uint8_t> pack_key(const KeyEvent& e) {
    std::vector<uint8_t> o;
    write_u8(o, e.down);
    write_u32be(o, e.keysym);
    return o;
}

std::optional<KeyEvent> unpack_key(std::span<const uint8_t> p) {
    KeyEvent e;
    size_t i = 0;
    if (!read_u8(p, i, e.down) || !read_u32be(p, i, e.keysym)) return std::nullopt;
    return e;
}

std::vector<uint8_t> pack_video(uint16_t w, uint16_t h, std::span<const uint8_t> jpeg) {
    std::vector<uint8_t> o;
    write_u16be(o, w);
    write_u16be(o, h);
    write_fixed(o, jpeg);
    return o;
}

std::optional<VideoMeta> unpack_video(std::span<const uint8_t> p) {
    VideoMeta v;
    size_t i = 0;
    if (!read_u16be(p, i, v.width) || !read_u16be(p, i, v.height)) return std::nullopt;
    v.jpeg.assign(p.begin() + static_cast<std::ptrdiff_t>(i), p.end());
    return v;
}

std::vector<uint8_t> pack_error(const std::string& msg) {
    return std::vector<uint8_t>(msg.begin(), msg.end());
}

std::optional<std::string> unpack_error(std::span<const uint8_t> p) {
    return std::string(p.begin(), p.end());
}

}  // namespace peerdesk
