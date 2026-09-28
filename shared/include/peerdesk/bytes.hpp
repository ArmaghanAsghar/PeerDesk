#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace peerdesk {

// Big-endian wire helpers. write_* append to `o`; read_* consume from `s` at
// cursor `i`, advance it on success, and return false (leaving `i` unchanged)
// if `s` is too short.

// Append one byte.
inline void write_u8(std::vector<uint8_t>& o, uint8_t v) { o.push_back(v); }

// Append a 16-bit value, most significant byte first.
inline void write_u16be(std::vector<uint8_t>& o, uint16_t v) {
    o.push_back(static_cast<uint8_t>(v >> 8));
    o.push_back(static_cast<uint8_t>(v));
}

// Append a 32-bit value, most significant byte first.
inline void write_u32be(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v >> 24));
    o.push_back(static_cast<uint8_t>(v >> 16));
    o.push_back(static_cast<uint8_t>(v >> 8));
    o.push_back(static_cast<uint8_t>(v));
}

// Read one byte.
inline bool read_u8(std::span<const uint8_t> s, size_t& i, uint8_t& v) {
    if (i >= s.size()) return false;
    v = s[i++];
    return true;
}

// Read a big-endian 16-bit value.
inline bool read_u16be(std::span<const uint8_t> s, size_t& i, uint16_t& v) {
    if (i + 2 > s.size()) return false;
    v = static_cast<uint16_t>((s[i] << 8) | s[i + 1]);
    i += 2;
    return true;
}

// Read a big-endian 32-bit value.
inline bool read_u32be(std::span<const uint8_t> s, size_t& i, uint32_t& v) {
    if (i + 4 > s.size()) return false;
    v = (static_cast<uint32_t>(s[i]) << 24) | (static_cast<uint32_t>(s[i + 1]) << 16) |
        (static_cast<uint32_t>(s[i + 2]) << 8) | static_cast<uint32_t>(s[i + 3]);
    i += 4;
    return true;
}

// Append raw bytes as-is.
inline void write_fixed(std::vector<uint8_t>& o, std::span<const uint8_t> b) {
    o.insert(o.end(), b.begin(), b.end());
}

// Copy exactly dest.size() bytes into `dest`.
inline bool read_fixed(std::span<const uint8_t> s, size_t& i, std::span<uint8_t> dest) {
    if (i + dest.size() > s.size()) return false;
    std::copy(s.begin() + static_cast<std::ptrdiff_t>(i),
              s.begin() + static_cast<std::ptrdiff_t>(i + dest.size()), dest.begin());
    i += dest.size();
    return true;
}

// Truncate `s` at its first NUL (for fixed-width, zero-padded string fields).
inline std::string trim_nul(std::string s) {
    const auto z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
}

}  // namespace peerdesk
