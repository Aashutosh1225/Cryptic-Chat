#pragma once
#include "Database.hpp"
#include <string>
#include <vector>
#include <cstdint>

// ChatHistoryRepository: stores/retrieves chat messages.
//
// Important design note: messages are stored EXACTLY as they arrive off
// the wire -- still encrypted (the same ciphertext blob AESCipher/Cipher
// produced). The server never decrypts messages to store them, which is
// what keeps this a genuine end-to-end design rather than "encrypted in
// transit, plaintext at rest." See the README for the key-management
// implication this creates for history retrieval after a reconnect.
class ChatHistoryRepository {
public:
    struct MessageRecord {
        int64_t id = 0;
        std::string senderUsername;
        std::string roomId;
        int64_t timestamp = 0;
        std::vector<uint8_t> encryptedPayload;   // ciphertext blob, untouched
    };

    explicit ChatHistoryRepository(Database& db);

    bool createTableIfNotExists();

    // Persists one message. timestamp is caller-supplied (e.g. from
    // Message::getTimestamp() in network/Message.hpp) rather than using
    // SQLite's own clock, so client and server agree on message ordering
    // even if their system clocks drift slightly.
    bool saveMessage(const std::string& senderUsername, const std::string& roomId,
                      int64_t timestamp, const std::vector<uint8_t>& encryptedPayload);

    // Returns up to `limit` most recent messages for a room, in
    // chronological order (oldest first) -- the order a chat window
    // should display them in.
    std::vector<MessageRecord> getRecentMessages(const std::string& roomId, int limit);

    // Returns all messages for a room strictly after the given timestamp,
    // chronological order. Useful for "catch up since I was last connected"
    // rather than always reloading a fixed window.
    std::vector<MessageRecord> getMessagesSince(const std::string& roomId, int64_t sinceTimestamp);

private:
    Database& db_;

    static MessageRecord rowToRecord(Database::Statement& stmt);
};