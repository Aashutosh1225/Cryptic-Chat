#include "Cipher.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <stdexcept>

AESCipher::AESCipher(const std::array<uint8_t, KEY_BYTES>& key) : key_(key) {}

std::vector<uint8_t> AESCipher::encrypt(const std::vector<uint8_t>& plaintext,
                                        const std::vector<uint8_t>& associatedData) {
    std::vector<uint8_t> iv(IV_BYTES);
    if (RAND_bytes(iv.data(), static_cast<int>(IV_BYTES)) != 1) {
        throw std::runtime_error("RAND_bytes failed to generate IV");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    // 1. Initialize cipher type (AES-256-GCM) with no key/iv yet
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptInit_ex failed");
    }

    // 2. Set IV length explicitly (GCM allows variable IV length; we standardize on 12 bytes)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_BYTES), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to set GCM IV length");
    }

    // 3. Now supply the actual key and IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key_.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to initialize GCM key");
    }

    int len = 0;
    if (!associatedData.empty() && EVP_EncryptUpdate(ctx, nullptr, &len, associatedData.data(),
                                                       static_cast<int>(associatedData.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to authenticate message metadata");
    }

    std::vector<uint8_t> ciphertext(plaintext.size());
    int ciphertextLen = 0;

    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("EVP_EncryptUpdate failed");
        }
        ciphertextLen = len;
    }

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertextLen, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("EVP_EncryptFinal_ex failed");
    }
    ciphertextLen += len;
    ciphertext.resize(ciphertextLen);

    std::vector<uint8_t> tag(TAG_BYTES);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_BYTES), tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw std::runtime_error("Failed to retrieve GCM authentication tag");
    }

    EVP_CIPHER_CTX_free(ctx);

    // Pack IV + ciphertext + tag into one self-contained blob.
    std::vector<uint8_t> blob;
    blob.reserve(IV_BYTES + ciphertext.size() + TAG_BYTES);
    blob.insert(blob.end(), iv.begin(), iv.end());
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());
    blob.insert(blob.end(), tag.begin(), tag.end());
    return blob;
}

std::vector<uint8_t> AESCipher::decrypt(const std::vector<uint8_t>& blob, bool& outSuccess,
                                        const std::vector<uint8_t>& associatedData) {
    outSuccess = false;
    if (blob.size() < IV_BYTES + TAG_BYTES) {
        return {};   // too short to even contain IV + tag
    }

    const uint8_t* iv = blob.data();
    const uint8_t* ciphertext = blob.data() + IV_BYTES;
    size_t ciphertextLen = blob.size() - IV_BYTES - TAG_BYTES;
    const uint8_t* tag = blob.data() + IV_BYTES + ciphertextLen;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_CIPHER_CTX_new failed");

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_BYTES), nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, key_.data(), iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int len = 0;
    if (!associatedData.empty() && EVP_DecryptUpdate(ctx, nullptr, &len, associatedData.data(),
                                                       static_cast<int>(associatedData.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    std::vector<uint8_t> plaintext(ciphertextLen);
    int plaintextLen = 0;

    if (ciphertextLen > 0) {
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext, static_cast<int>(ciphertextLen)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return {};
        }
        plaintextLen = len;
    }

    // Tell OpenSSL what tag to verify against BEFORE calling Final.
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(TAG_BYTES), const_cast<uint8_t*>(tag)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int finalResult = EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintextLen, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (finalResult <= 0) {
        return {};   // tag verification failed -- tampered ciphertext or wrong key
    }

    plaintextLen += len;
    plaintext.resize(plaintextLen);
    outSuccess = true;
    return plaintext;
}
