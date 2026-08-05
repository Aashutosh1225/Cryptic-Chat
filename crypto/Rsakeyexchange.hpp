#pragma once
#include <cstdint>
#include <vector>

typedef struct evp_pkey_st EVP_PKEY;


// Flow:
//   1. Both client and server call generateKeypair() locally.
//   2. Each side calls getPublicKeyDER() and sends the result to the peer
//      over the (still-plaintext) socket -- public keys are safe to expose.
//   3. The client generates a random AES-256 key, calls
//      encryptSessionKey(aesKey, serverPublicKeyDER), and sends the result.
//   4. The server calls decryptSessionKey(received) with its own private
//      key to recover the same AES key. From here on, Cipher (AESCipher)
//      encrypts all actual chat traffic with that shared key.
class RSAKeyExchange {
public:
    RSAKeyExchange();
    ~RSAKeyExchange();

    // Non-copyable (owns a raw EVP_PKEY* keypair handle)
    RSAKeyExchange(const RSAKeyExchange&) = delete;
    RSAKeyExchange& operator=(const RSAKeyExchange&) = delete;

    // Generates a fresh RSA keypair
    bool generateKeypair(int bits = 2048);

    std::vector<uint8_t> getPublicKeyDER() const;

    // Encrypts a session key (e.g. a 32-byte AES-256 key) using a PEER's
    // public key (as received from getPublicKeyDER() on their side).
    static std::vector<uint8_t> encryptSessionKey(const std::vector<uint8_t>& sessionKey,
                                                   const std::vector<uint8_t>& peerPublicKeyDER);

    std::vector<uint8_t> decryptSessionKey(const std::vector<uint8_t>& encryptedSessionKey, bool& outSuccess) const;

private:
    EVP_PKEY* keypair_ = nullptr;
};