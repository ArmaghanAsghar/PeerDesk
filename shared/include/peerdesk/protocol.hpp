#pragma once

#include "peerdesk/bytes.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace peerdesk {

inline constexpr uint16_t kProtocolVersion = 1;
inline constexpr char kHelloMagic[4] = {'P', 'D', 'S', 'K'};
inline constexpr uint32_t kMaxPayload = 8 * 1024 * 1024;
inline constexpr size_t kUsernameBytes = 32;
inline constexpr uint16_t kDefaultPort = 4473;

enum class MsgType : uint8_t {
    Hello = 1,
    AuthChallenge = 2,
    AuthResponse = 3,
    AuthOk = 4,
    AuthFail = 5,
    VideoFrame = 6,
    Mouse = 7,
    Key = 8,
    Ping = 9,
    Pong = 10,
    Disconnect = 11,
    Error = 12,
};

enum class AuthFailReason : uint8_t {
    BadCredentials = 1,
    SessionBusy = 2,
    Protocol = 3,
};

enum class MouseAction : uint8_t { Move = 1, Down = 2, Up = 3, Wheel = 4 };

inline const char* auth_fail_text(AuthFailReason r) {
    switch (r) {
        case AuthFailReason::BadCredentials:
            return "Authentication failed";
        case AuthFailReason::SessionBusy:
            return "Host already has a viewer";
        case AuthFailReason::Protocol:
            return "Protocol error";
    }
    return "Authentication failed";
}

struct Hello {
    uint16_t version = kProtocolVersion;
    std::string username;
};

struct AuthChallenge {
    std::array<uint8_t, 16> salt{};
    std::array<uint8_t, 32> nonce{};
    uint32_t t_cost = 2;
    uint32_t m_cost = 16384;
    uint32_t parallelism = 1;
};

struct AuthResponse {
    std::array<uint8_t, 32> hmac{};
};

struct AuthOk {
    uint16_t width = 0;
    uint16_t height = 0;
};

struct MouseEvent {
    MouseAction action = MouseAction::Move;
    uint8_t button = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    int16_t wheel_delta = 0;
};

struct KeyEvent {
    uint8_t down = 0;
    uint32_t keysym = 0;
};

struct VideoMeta {
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> jpeg;
};

std::vector<uint8_t> pack_hello(const Hello& h);
std::optional<Hello> unpack_hello(std::span<const uint8_t> p);

std::vector<uint8_t> pack_challenge(const AuthChallenge& c);
std::optional<AuthChallenge> unpack_challenge(std::span<const uint8_t> p);

std::vector<uint8_t> pack_auth_response(const AuthResponse& r);
std::optional<AuthResponse> unpack_auth_response(std::span<const uint8_t> p);

std::vector<uint8_t> pack_auth_ok(const AuthOk& o);
std::optional<AuthOk> unpack_auth_ok(std::span<const uint8_t> p);

std::vector<uint8_t> pack_auth_fail(AuthFailReason r);
std::optional<AuthFailReason> unpack_auth_fail(std::span<const uint8_t> p);

std::vector<uint8_t> pack_mouse(const MouseEvent& e);
std::optional<MouseEvent> unpack_mouse(std::span<const uint8_t> p);

std::vector<uint8_t> pack_key(const KeyEvent& e);
std::optional<KeyEvent> unpack_key(std::span<const uint8_t> p);

std::vector<uint8_t> pack_video(uint16_t w, uint16_t h, std::span<const uint8_t> jpeg);
std::optional<VideoMeta> unpack_video(std::span<const uint8_t> p);

std::vector<uint8_t> pack_error(const std::string& msg);
std::optional<std::string> unpack_error(std::span<const uint8_t> p);

}  // namespace peerdesk
