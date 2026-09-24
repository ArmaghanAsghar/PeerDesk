#pragma once

#include "peerdesk/protocol.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace peerdesk {

inline constexpr uint32_t kArgonT = 2;
inline constexpr uint32_t kArgonM = 16384;
inline constexpr uint32_t kArgonP = 1;

bool random_bytes(std::span<uint8_t> out);

std::array<uint8_t, 32> argon2id_raw(std::string_view password, std::span<const uint8_t, 16> salt,
                                     uint32_t t, uint32_t m, uint32_t p);

std::array<uint8_t, 32> hmac_sha256(std::span<const uint8_t> key, std::span<const uint8_t> msg);

bool const_time_equal(std::span<const uint8_t> a, std::span<const uint8_t> b);

AuthResponse make_auth_response(std::string_view password, const AuthChallenge& ch);

bool verify_auth_response(std::span<const uint8_t, 32> stored_hash, const AuthChallenge& ch,
                          const AuthResponse& resp);

}  // namespace peerdesk
