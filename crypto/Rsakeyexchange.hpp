#pragma once
#include <cstdint>
#include <vector>

// Forward-declare OpenSSL's opaque key type so this header doesn't force
// every includer to pull in <openssl/evp.h>.
typedef struct evp_pkey_st EVP_PKEY;

// RSAKeyExchange: generates an RSA keypair and uses it to securely transport
// a symmetric AES session key between two parties (RSA key exchange).
//
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

    // Generates a fresh RSA keypair. 2048-bit is the practical minimum for
    // real use; call with 4096 for extra margin at the cost of slower
    // encrypt/decrypt.
    bool generateKeypair(int bits = 2048);

    // Exports this instance's PUBLIC key only, in DER format -- safe to
    // transmit to a peer in plaintext.
    std::vector<uint8_t> getPublicKeyDER() const;

    // Encrypts a session key (e.g. a 32-byte AES-256 key) using a PEER's
    // public key (as received from getPublicKeyDER() on their side).
    // Uses RSA-OAEP padding, which is the modern, safe choice -- never use
    // raw/PKCS#1 v1.5 padding for new designs.
    static std::vector<uint8_t> encryptSessionKey(const std::vector<uint8_t>& sessionKey,
                                                   const std::vector<uint8_t>& peerPublicKeyDER);

    // Decrypts a blob produced by encryptSessionKey() using THIS instance's
    // own private key. Returns an empty vector on failure (wrong key,
    // corrupted data) -- check outSuccess, don't rely on emptiness alone
    // to distinguish "empty session key" from "decryption failed".
    std::vector<uint8_t> decryptSessionKey(const std::vector<uint8_t>& encryptedSessionKey, bool& outSuccess) const;

private:
    EVP_PKEY* keypair_ = nullptr;
};