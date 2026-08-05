#pragma once
#include <cstdint>
#include <vector>
#include <array>

// Cipher: abstract base for encryption algorithms (strategy pattern).
// Message payloads are encrypted through this interface, so Connection
// never needs to know which concrete algorithm/library is in use.
class Cipher {
public:
    virtual ~Cipher() = default;

    // Encrypts plaintext, returns a self-contained ciphertext blob
    // (IV/nonce + auth tag are packed into the returned bytes, so the
    // caller only needs to store/transmit one buffer).
    virtual std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) = 0;

    // Decrypts a blob produced by encrypt(). Returns an empty vector
    // and sets outSuccess=false if the auth tag doesn't verify
    // (tampered/corrupted ciphertext or wrong key).
    virtual std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext, bool& outSuccess) = 0;
};

// AESCipher: AES-256-GCM via OpenSSL's EVP API.
class AESCipher : public Cipher {
public:
    static constexpr size_t KEY_BYTES = 32;   // 256-bit key
    static constexpr size_t IV_BYTES = 12;    // 96-bit IV, standard for GCM
    static constexpr size_t TAG_BYTES = 16;   // 128-bit auth tag

    explicit AESCipher(const std::array<uint8_t, KEY_BYTES>& key);

    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext) override;
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext, bool& outSuccess) override;

private:
    std::array<uint8_t, KEY_BYTES> key_;
};