#pragma once
#include <cstdint>
#include <vector>
#include <string>

// PasswordHasher: PBKDF2-SHA256 password storage.
//
// Never store plaintext passwords. This class produces a random salt per
// user and a derived hash that's safe to store in the database -- even if
// the database leaks, recovering the original password requires brute-forcing
// PBKDF2's deliberately slow iteration count, not just looking up a table.
class PasswordHasher {
public:
    static constexpr size_t SALT_BYTES = 16;
    static constexpr size_t HASH_BYTES = 32;           // SHA-256 output size
    static constexpr int DEFAULT_ITERATIONS = 600000;  // OWASP 2023 recommendation for PBKDF2-HMAC-SHA256

    // Result of hashing a new password -- store both fields in the
    // database (e.g. UserRepository::createUser()).
    struct HashResult {
        std::vector<uint8_t> salt;
        std::vector<uint8_t> hash;
        int iterations;
    };

    // Hashes a new password with a freshly generated random salt.
    static HashResult hashPassword(const std::string& password, int iterations = DEFAULT_ITERATIONS);

    // Re-derives a hash using a KNOWN salt (e.g. one loaded from the
    // database) and compares it against the stored hash. Returns true
    // only if the password is correct.
    //
    // Uses a constant-time comparison internally so that response timing
    // can't leak how many leading bytes matched (a timing side-channel).
    static bool verifyPassword(const std::string& password,
                                const std::vector<uint8_t>& salt,
                                const std::vector<uint8_t>& storedHash,
                                int iterations = DEFAULT_ITERATIONS);

private:
    // Raw PBKDF2-HMAC-SHA256 derivation -- shared by both hashPassword and
    // verifyPassword so there's exactly one place the actual KDF call lives.
    static std::vector<uint8_t> derive(const std::string& password,
                                        const std::vector<uint8_t>& salt,
                                        int iterations);
};