#include "users.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <span>
#include <sstream>

namespace peerdesk {
namespace {

std::string to_hex(std::span<const uint8_t> b) {
    std::ostringstream o;
    o << std::hex << std::setfill('0');
    for (auto v : b) o << std::setw(2) << static_cast<int>(v);
    return o.str();
}

bool from_hex(const std::string& s, std::span<uint8_t> out) {
    if (s.size() != out.size() * 2) return false;
    for (size_t i = 0; i < out.size(); ++i) {
        const auto chunk = s.substr(i * 2, 2);
        char* end = nullptr;
        const unsigned long v = std::strtoul(chunk.c_str(), &end, 16);
        if (!end || *end != '\0' || v > 255) return false;
        out[i] = static_cast<uint8_t>(v);
    }
    return true;
}

}  // namespace

bool UserStore::load(const std::filesystem::path& path, std::string& err) {
    users_.clear();
    std::ifstream in(path);
    if (!in) {
        err = "No users file";
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        UserRecord u;
        std::string salt_h, hash_h;
        if (!(ls >> u.username >> salt_h >> hash_h >> u.t_cost >> u.m_cost >> u.parallelism)) {
            err = "Corrupt users file";
            return false;
        }
        if (!from_hex(salt_h, u.salt) || !from_hex(hash_h, u.hash)) {
            err = "Corrupt users file (hex)";
            return false;
        }
        users_.push_back(u);
    }
    return true;
}

bool UserStore::save(const std::filesystem::path& path, std::string& err) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        err = "Cannot write users file";
        return false;
    }
    out << "# PeerDesk host logins: username salt hash t m p\n";
    for (const auto& u : users_) {
        out << u.username << ' ' << to_hex(u.salt) << ' ' << to_hex(u.hash) << ' ' << u.t_cost
            << ' ' << u.m_cost << ' ' << u.parallelism << '\n';
    }
    return true;
}

bool UserStore::upsert(const std::string& username, const std::string& password) {
    UserRecord u;
    u.username = username;
    if (!random_bytes(u.salt)) return false;
    const auto h = argon2id_raw(password, u.salt, u.t_cost, u.m_cost, u.parallelism);
    u.hash = h;
    for (auto& existing : users_) {
        if (existing.username == username) {
            existing = u;
            return true;
        }
    }
    users_.push_back(u);
    return true;
}

std::optional<UserRecord> UserStore::find(const std::string& username) const {
    for (const auto& u : users_) {
        if (u.username == username) return u;
    }
    return std::nullopt;
}

}  // namespace peerdesk
