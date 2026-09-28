#pragma once

#include "peerdesk/protocol.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace peerdesk {

// Default Argon2id cost parameters (iterations, memory in KiB, lanes) for new logins.
inline constexpr uint32_t kArgonT = 2;
inline constexpr uint32_t kArgonM = 16384;
inline constexpr uint32_t kArgonP = 1;

// Fill `out` with cryptographically secure random bytes. Returns false if the RNG fails.
bool random_bytes(std::span<uint8_t> out);

// Derive a 32-byte Argon2id hash of `password` with the given salt and costs
// (t = iterations, m = memory in KiB, p = parallelism). Returns all zeros on failure.
std::array<uint8_t, 32> argon2id_raw(std::string_view password, std::span<const uint8_t, 16> salt,
                                     uint32_t t, uint32_t m, uint32_t p);

// Compute HMAC-SHA256 of `msg` under `key`.
std::array<uint8_t, 32> hmac_sha256(std::span<const uint8_t> key, std::span<const uint8_t> msg);

// Compare two buffers in constant time. Returns false if the sizes differ.
bool const_time_equal(std::span<const uint8_t> a, std::span<const uint8_t> b);

// Client side: prove knowledge of `password` for challenge `ch` by returning
// HMAC(Argon2id(password, ch.salt), ch.nonce). The password itself never leaves the client.
AuthResponse make_auth_response(std::string_view password, const AuthChallenge& ch);

// Host side: check `resp` against the stored Argon2id hash for the nonce in `ch`.
bool verify_auth_response(std::span<const uint8_t, 32> stored_hash, const AuthChallenge& ch,
                          const AuthResponse& resp);

}  // namespace peerdesk
