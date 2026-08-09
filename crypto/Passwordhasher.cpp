#include "PasswordHasher.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>   // for CRYPTO_memcmp (constant-time compare)

std::vector<uint8_t> PasswordHasher::derive(const std::string& password,
                                             const std::vector<uint8_t>& salt,
                                             int iterations) {
    std::vector<uint8_t> out(HASH_BYTES);

    // PKCS5_PBKDF2_HMAC is OpenSSL's PBKDF2 implementation; passing
    // EVP_sha256() selects PBKDF2-HMAC-SHA256 specifically.
    int result = PKCS5_PBKDF2_HMAC(
        password.data(), static_cast<int>(password.size()),
        salt.data(), static_cast<int>(salt.size()),
        iterations,
        EVP_sha256(),
        static_cast<int>(HASH_BYTES),
        out.data()
    );

    if (result != 1) {
        return {};   // derivation failed -- caller must treat this as an error
    }
    return out;
}

PasswordHasher::HashResult PasswordHasher::hashPassword(const std::string& password, int iterations) {
    HashResult result;
    result.salt.resize(SALT_BYTES);
    RAND_bytes(result.salt.data(), static_cast<int>(SALT_BYTES));   // fresh random salt per password

    result.hash = derive(password, result.salt, iterations);
    result.iterations = iterations;
    return result;
}

bool PasswordHasher::verifyPassword(const std::string& password,
                                     const std::vector<uint8_t>& salt,
                                     const std::vector<uint8_t>& storedHash,
                                     int iterations) {
    if (storedHash.size() != HASH_BYTES) {
        return false;   // stored hash is the wrong size -- can't possibly match
    }

    std::vector<uint8_t> candidateHash = derive(password, salt, iterations);
    if (candidateHash.empty()) {
        return false;   // derivation itself failed
    }

    // CRYPTO_memcmp runs in constant time regardless of where the first
    // differing byte is -- a plain std::equal/memcmp would let an attacker
    // measure response time to guess the hash byte-by-byte (timing attack).
    return CRYPTO_memcmp(candidateHash.data(), storedHash.data(), HASH_BYTES) == 0;
}