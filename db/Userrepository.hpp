#pragma once
#include "Database.hpp"
#include "../crypto/PasswordHasher.hpp"
#include <string>
#include <cstdint>

// UserRepository: stores/retrieves user accounts.
// writes raw SQL -- it just calls createUser()/findByUsername().
class UserRepository {
public:
    struct UserRecord {
        int64_t id = 0;
        std::string username;
        std::vector<uint8_t> salt;
        std::vector<uint8_t> hash;
        int iterations = 0;
    };

    explicit UserRepository(Database& db);

    bool createTableIfNotExists();

    bool createUser(const std::string& username, const PasswordHasher::HashResult& hashResult);

    bool findByUsername(const std::string& username, UserRecord& outUser);

    bool usernameExists(const std::string& username);

private:
    Database& db_;
};