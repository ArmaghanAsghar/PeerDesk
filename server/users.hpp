#pragma once

#include "peerdesk/auth.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace peerdesk {

// A host login. The password is stored only as an Argon2id hash.
struct UserRecord {
    std::string username;
    std::array<uint8_t, 16> salt{};
    std::array<uint8_t, 32> hash{};
    uint32_t t_cost = kArgonT;
    uint32_t m_cost = kArgonM;
    uint32_t parallelism = kArgonP;
};

// In-memory list of host logins, persisted as a text file with one
// "username salt_hex hash_hex t m p" line per user. Lines starting with '#' are comments.
class UserStore {
public:
    // Replace the current contents with the file at `path`. Returns false with
    // `err` set if the file is missing or any line is malformed.
    bool load(const std::filesystem::path& path, std::string& err);
    // Overwrite `path` with all users, creating parent directories as needed.
    bool save(const std::filesystem::path& path, std::string& err) const;
    // Add `username`, or replace their record, with a fresh salt and hash of
    // `password`. Returns false only if the RNG fails. Does not save.
    bool upsert(const std::string& username, const std::string& password);
    // Look up a user by exact name.
    std::optional<UserRecord> find(const std::string& username) const;
    bool empty() const { return users_.empty(); }

private:
    std::vector<UserRecord> users_;
};

}  // namespace peerdesk
