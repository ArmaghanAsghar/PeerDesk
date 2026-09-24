#include "peerdesk/auth.hpp"

#include <argon2.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace peerdesk {

bool random_bytes(std::span<uint8_t> out) {
    return RAND_bytes(out.data(), static_cast<int>(out.size())) == 1;
}

std::array<uint8_t, 32> argon2id_raw(std::string_view password, std::span<const uint8_t, 16> salt,
                                     uint32_t t, uint32_t m, uint32_t p) {
    std::array<uint8_t, 32> hash{};
    const int rc = argon2id_hash_raw(t, m, p, password.data(), password.size(), salt.data(),
                                     salt.size(), hash.data(), hash.size());
    if (rc != ARGON2_OK) {
        hash.fill(0);
    }
    return hash;
}

std::array<uint8_t, 32> hmac_sha256(std::span<const uint8_t> key, std::span<const uint8_t> msg) {
    std::array<uint8_t, 32> out{};
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), msg.data(), msg.size(),
         out.data(), &len);
    return out;
}

bool const_time_equal(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

AuthResponse make_auth_response(std::string_view password, const AuthChallenge& ch) {
    const auto hash = argon2id_raw(password, ch.salt, ch.t_cost, ch.m_cost, ch.parallelism);
    AuthResponse r;
    r.hmac = hmac_sha256(hash, ch.nonce);
    return r;
}

bool verify_auth_response(std::span<const uint8_t, 32> stored_hash, const AuthChallenge& ch,
                          const AuthResponse& resp) {
    const auto expect = hmac_sha256(stored_hash, ch.nonce);
    return const_time_equal(expect, resp.hmac);
}

}  // namespace peerdesk
