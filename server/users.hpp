#pragma once

#include "peerdesk/auth.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace peerdesk {

struct UserRecord {
    std::string username;
    std::array<uint8_t, 16> salt{};
    std::array<uint8_t, 32> hash{};
    uint32_t t_cost = kArgonT;
    uint32_t m_cost = kArgonM;
    uint32_t parallelism = kArgonP;
};

class UserStore {
public:
    bool load(const std::filesystem::path& path, std::string& err);
    bool save(const std::filesystem::path& path, std::string& err) const;
    bool upsert(const std::string& username, const std::string& password);
    std::optional<UserRecord> find(const std::string& username) const;
    bool empty() const { return users_.empty(); }

private:
    std::vector<UserRecord> users_;
};

}  // namespace peerdesk
