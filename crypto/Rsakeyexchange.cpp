#include "RSAKeyExchange.hpp"
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>   // for i2d_PUBKEY / d2i_PUBKEY (DER public key encode/decode)
#include <openssl/err.h>

RSAKeyExchange::RSAKeyExchange() = default;

RSAKeyExchange::~RSAKeyExchange() {
    if (keypair_) {
        EVP_PKEY_free(keypair_);
    }
}

bool RSAKeyExchange::generateKeypair(int bits) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) return false;

    bool ok = true;
    ok = ok && (EVP_PKEY_keygen_init(ctx) == 1);
    ok = ok && (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) == 1);

    EVP_PKEY* generated = nullptr;
    ok = ok && (EVP_PKEY_keygen(ctx, &generated) == 1);

    EVP_PKEY_CTX_free(ctx);

    if (!ok) {
        if (generated) EVP_PKEY_free(generated);
        return false;
    }

    if (keypair_) EVP_PKEY_free(keypair_);   // replace any previously generated keypair
    keypair_ = generated;
    return true;
}

std::vector<uint8_t> RSAKeyExchange::getPublicKeyDER() const {
    if (!keypair_) return {};

    uint8_t* buffer = nullptr;
    int len = i2d_PUBKEY(keypair_, &buffer);   // DER-encode the public key portion only
    if (len <= 0) return {};

    std::vector<uint8_t> result(buffer, buffer + len);
    OPENSSL_free(buffer);
    return result;
}

std::vector<uint8_t> RSAKeyExchange::encryptSessionKey(const std::vector<uint8_t>& sessionKey,
                                                        const std::vector<uint8_t>& peerPublicKeyDER) {
    const uint8_t* p = peerPublicKeyDER.data();
    EVP_PKEY* peerKey = d2i_PUBKEY(nullptr, &p, static_cast<long>(peerPublicKeyDER.size()));
    if (!peerKey) return {};

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(peerKey, nullptr);
    if (!ctx) {
        EVP_PKEY_free(peerKey);
        return {};
    }

    std::vector<uint8_t> result;
    bool ok = true;
    ok = ok && (EVP_PKEY_encrypt_init(ctx) == 1);
    ok = ok && (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) == 1);

    size_t outLen = 0;
    // First call with a null output buffer just to learn the required size.
    ok = ok && (EVP_PKEY_encrypt(ctx, nullptr, &outLen, sessionKey.data(), sessionKey.size()) == 1);

    if (ok) {
        result.resize(outLen);
        ok = (EVP_PKEY_encrypt(ctx, result.data(), &outLen, sessionKey.data(), sessionKey.size()) == 1);
        result.resize(outLen);
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peerKey);

    return ok ? result : std::vector<uint8_t>{};
}

std::vector<uint8_t> RSAKeyExchange::decryptSessionKey(const std::vector<uint8_t>& encryptedSessionKey, bool& outSuccess) const {
    outSuccess = false;
    if (!keypair_) return {};

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(keypair_, nullptr);
    if (!ctx) return {};

    bool ok = true;
    ok = ok && (EVP_PKEY_decrypt_init(ctx) == 1);
    ok = ok && (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) == 1);

    size_t outLen = 0;
    ok = ok && (EVP_PKEY_decrypt(ctx, nullptr, &outLen, encryptedSessionKey.data(), encryptedSessionKey.size()) == 1);

    std::vector<uint8_t> result;
    if (ok) {
        result.resize(outLen);
        ok = (EVP_PKEY_decrypt(ctx, result.data(), &outLen, encryptedSessionKey.data(), encryptedSessionKey.size()) == 1);
        result.resize(outLen);
    }

    EVP_PKEY_CTX_free(ctx);

    if (!ok) return {};   // wrong key or corrupted ciphertext -- OAEP padding check failed

    outSuccess = true;
    return result;
}