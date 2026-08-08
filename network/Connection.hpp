#pragma once

#include "Message.hpp"
#include "../crypto/Cipher.hpp"
#include "../crypto/RSAKeyExchange.hpp"

#include <openssl/rand.h>

#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Connection<SocketT>: wires Socket (network) + RSAKeyExchange + AESCipher
// (crypto) + Message (framing) together into a single class that performs
// the handshake once, then sends/receives encrypted chat messages.
//
// Templated on the transport (SocketT) rather than hard-coded to the real
// Socket class for one reason: Socket.hpp is Windows-only (unconditional
// <winsock2.h>), so it can't be compiled or exercised on Linux at all --
// there's no POSIX fallback in this project (by design; see the project
// spec). Everything else Connection depends on (Message, Cipher,
// RSAKeyExchange) is portable OpenSSL/standard-C++ code. Templating on the
// transport means:
//
//   - Connection<Socket>       is the real thing, used on Windows.
//   - Connection<LoopbackSocket> (test-only, see test_connection.cpp) is
//     used to actually exercise the handshake and encrypted round-trip
//     logic natively on Linux, over a real pair of threads talking through
//     an in-memory byte pipe -- the same "verify natively on Linux first"
//     discipline used for every portable phase so far, applied here via
//     dependency injection instead of being blocked by Socket's
//     Windows-only header.
//
// SocketT only needs to provide:
//   ssize_t send(const uint8_t* data, size_t len);
//   ssize_t receive(uint8_t* buffer, size_t maxLen);
// with the same semantics as a real blocking TCP socket: send() returns
// the number of bytes actually written (may be less than len -- a short
// write), receive() returns the number of bytes actually read (may be
// less than maxLen), 0 means the peer performed an orderly shutdown, and
// a negative return means an error. Both Socket and the test's
// LoopbackSocket satisfy this.
template <typename SocketT>
class Connection {
public:
    enum class Role { Client, Server };

    explicit Connection(SocketT socket) : socket_(std::move(socket)) {}

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

    // Runs the RSA/AES handshake:
    //   1. Both sides generate an RSA keypair and exchange public keys
    //      (length-prefixed, plaintext -- public keys are safe to expose).
    //   2. The Client generates a random AES-256 session key, encrypts it
    //      with the Server's RSA public key (OAEP padding), and sends it.
    //   3. The Server decrypts the session key with its own RSA private
    //      key.
    // Both sides end up holding the same AES-256 key, used to construct
    // an AESCipher for all subsequent sendMessage()/receiveMessage()
    // calls. Returns false on any socket error, malformed handshake data,
    // or decryption failure -- the connection should be considered dead
    // and discarded if this returns false.
    bool performHandshake(Role role) {
        role_ = role;
        rsa_ = std::make_unique<RSAKeyExchange>();
        if (!rsa_->generateKeypair()) {
            return false;
        }

        std::vector<uint8_t> myPublicKey = rsa_->getPublicKeyDER();
        if (myPublicKey.empty() || !sendFramed(myPublicKey)) {
            return false;
        }

        std::vector<uint8_t> peerPublicKey;
        if (!receiveFramed(peerPublicKey, kMaxHandshakeFrameBytes) || peerPublicKey.empty()) {
            return false;
        }

        std::array<uint8_t, AESCipher::KEY_BYTES> sessionKey{};

        if (role_ == Role::Client) {
            if (RAND_bytes(sessionKey.data(), static_cast<int>(sessionKey.size())) != 1) {
                return false;
            }
            std::vector<uint8_t> rawKey(sessionKey.begin(), sessionKey.end());
            std::vector<uint8_t> encryptedKey = RSAKeyExchange::encryptSessionKey(rawKey, peerPublicKey);
            if (encryptedKey.empty() || !sendFramed(encryptedKey)) {
                return false;
            }
        } else {
            std::vector<uint8_t> encryptedKey;
            if (!receiveFramed(encryptedKey, kMaxHandshakeFrameBytes)) {
                return false;
            }
            bool decryptOk = false;
            std::vector<uint8_t> decrypted = rsa_->decryptSessionKey(encryptedKey, decryptOk);
            if (!decryptOk || decrypted.size() != AESCipher::KEY_BYTES) {
                return false;
            }
            std::copy(decrypted.begin(), decrypted.end(), sessionKey.begin());
        }

        cipher_ = std::make_unique<AESCipher>(sessionKey);
        handshakeComplete_ = true;
        return true;
    }

    bool isHandshakeComplete() const { return handshakeComplete_; }

    // Encrypts plaintext with the session's AESCipher, wraps it in a
    // Message (senderId + current timestamp + ciphertext), and sends the
    // serialized Message over the socket. Returns false if the handshake
    // hasn't completed yet or on any socket error.
    bool sendMessage(uint32_t senderId, const std::string& plaintext) {
        if (!handshakeComplete_) return false;

        std::vector<uint8_t> plainBytes(plaintext.begin(), plaintext.end());
        std::vector<uint8_t> ciphertext = cipher_->encrypt(plainBytes);

        uint64_t timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());

        Message message(senderId, timestamp, std::move(ciphertext));
        std::vector<uint8_t> wire = message.serialize();
        return sendExact(wire.data(), wire.size());
    }

    // Blocks until a full Message arrives off the wire, then decrypts its
    // payload. Message's own wire format is self-framing (header includes
    // payloadLen), so this reads the fixed-size header first, then reads
    // exactly payloadLen more bytes -- no separate outer framing needed
    // for chat messages the way handshake blobs need sendFramed/
    // receiveFramed.
    //
    // Returns false if: the socket errors/closes, the bytes don't
    // deserialize into a valid Message, or AES-GCM tag verification
    // fails (tampered ciphertext, or -- if this ever happens -- a session
    // key mismatch). outMessage/outPlaintext are only valid when this
    // returns true.
    bool receiveMessage(Message& outMessage, std::string& outPlaintext) {
        if (!handshakeComplete_) return false;

        std::vector<uint8_t> headerBuf(Message::WIRE_HEADER_SIZE);
        if (!receiveExact(headerBuf.data(), headerBuf.size())) {
            return false;
        }

        uint32_t payloadLen = readBE32(headerBuf.data() + 12);   // see Message's [senderId(4)][timestamp(8)][payloadLen(4)] layout
        if (payloadLen > kMaxMessageFrameBytes) {
            return false;   // corrupted/malicious length prefix -- refuse to allocate an unbounded buffer
        }

        std::vector<uint8_t> fullBuffer(headerBuf);
        fullBuffer.resize(headerBuf.size() + payloadLen);
        if (payloadLen > 0 && !receiveExact(fullBuffer.data() + headerBuf.size(), payloadLen)) {
            return false;
        }

        if (!Message::deserialize(fullBuffer, outMessage)) {
            return false;
        }

        bool decryptOk = false;
        std::vector<uint8_t> plainBytes = cipher_->decrypt(outMessage.getPayload(), decryptOk);
        if (!decryptOk) {
            return false;
        }

        outPlaintext.assign(plainBytes.begin(), plainBytes.end());
        return true;
    }

private:
    SocketT socket_;
    std::unique_ptr<RSAKeyExchange> rsa_;
    std::unique_ptr<AESCipher> cipher_;
    Role role_ = Role::Client;
    bool handshakeComplete_ = false;

    // Handshake blobs (RSA public keys, ~270-550 bytes for 2048/4096-bit
    // keys; encrypted session keys, exactly the RSA modulus size) are
    // always small. 1 MiB is a very generous ceiling that only exists to
    // reject a corrupted/malicious length prefix before allocating.
    static constexpr uint32_t kMaxHandshakeFrameBytes = 1u << 20;
    // Chat messages are plain text; 16 MiB is far more than any real
    // message needs, same purpose as above.
    static constexpr uint32_t kMaxMessageFrameBytes = 16u << 20;

    // ---- Portable big-endian helpers ----
    // Deliberately NOT using htonl/ntohl here (unlike Message.cpp, which
    // is allowed to be Windows-only) -- Connection.hpp must stay
    // includable from portable test code on Linux, so byte order is
    // handled by hand instead of pulling in <winsock2.h> or
    // <arpa/inet.h>. Produces the same big-endian wire format either way.
    static void writeBE32(uint8_t* out, uint32_t value) {
        out[0] = static_cast<uint8_t>(value >> 24);
        out[1] = static_cast<uint8_t>(value >> 16);
        out[2] = static_cast<uint8_t>(value >> 8);
        out[3] = static_cast<uint8_t>(value);
    }

    static uint32_t readBE32(const uint8_t* in) {
        return (static_cast<uint32_t>(in[0]) << 24) |
               (static_cast<uint32_t>(in[1]) << 16) |
               (static_cast<uint32_t>(in[2]) << 8) |
               static_cast<uint32_t>(in[3]);
    }

    // ---- TCP-stream framing helpers ----
    // A blocking socket's send()/receive() may transfer fewer bytes than
    // requested (a "short write"/"short read") -- that's normal TCP
    // stream behavior, not an error. These loop until the full amount is
    // transferred, and treat <= 0 (error or orderly peer shutdown) as
    // definite failure.
    bool sendExact(const uint8_t* data, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            auto n = socket_.send(data + sent, len - sent);
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }

    bool receiveExact(uint8_t* buffer, size_t len) {
        size_t received = 0;
        while (received < len) {
            auto n = socket_.receive(buffer + received, len - received);
            if (n <= 0) return false;
            received += static_cast<size_t>(n);
        }
        return true;
    }

    // Simple [4-byte big-endian length][payload] framing, used only for
    // handshake blobs (RSA public keys, encrypted session key) -- these
    // aren't Message objects, so they need their own minimal framing on
    // top of the raw TCP byte stream. Chat messages don't use this;
    // Message's own header is already self-framing (see receiveMessage).
    bool sendFramed(const std::vector<uint8_t>& payload) {
        std::array<uint8_t, 4> lenBuf{};
        writeBE32(lenBuf.data(), static_cast<uint32_t>(payload.size()));
        if (!sendExact(lenBuf.data(), lenBuf.size())) return false;
        if (payload.empty()) return true;
        return sendExact(payload.data(), payload.size());
    }

    bool receiveFramed(std::vector<uint8_t>& outPayload, uint32_t maxLen) {
        std::array<uint8_t, 4> lenBuf{};
        if (!receiveExact(lenBuf.data(), lenBuf.size())) return false;

        uint32_t len = readBE32(lenBuf.data());
        if (len > maxLen) return false;   // corrupted/malicious length prefix

        outPayload.resize(len);
        if (len == 0) return true;
        return receiveExact(outPayload.data(), len);
    }
};
