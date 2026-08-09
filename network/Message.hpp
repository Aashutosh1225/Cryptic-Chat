#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <ostream>

// Message: a structured chat message that can pack itself into raw bytes
// (for Socket::send) and rebuild itself from raw bytes (from Socket::receive).
//
//   [ senderId   : 4 bytes ]
//   [ timestamp  : 8 bytes ]
//   [ payloadLen : 4 bytes ]
//   [ payload    : payloadLen bytes ]   <- will hold ciphertext once Cipher exists
class Message {
public:
    Message() = default;
    Message(uint32_t senderId, uint64_t timestamp, std::vector<uint8_t> payload);

    // Pack this Message into a flat byte buffer ready for Socket::send().
    std::vector<uint8_t> serialize() const;

    // Rebuild a Message from raw bytes (e.g. from Socket::receive()).
    // Returns false if the buffer is malformed/truncated.
    static bool deserialize(const std::vector<uint8_t>& buffer, Message& outMessage);

    uint32_t getSenderId() const { return senderId_; }
    uint64_t getTimestamp() const { return timestamp_; }
    const std::vector<uint8_t>& getPayload() const { return payload_; }

    bool operator==(const Message& other) const;
    bool operator!=(const Message& other) const { return !(*this == other); }
    static constexpr size_t WIRE_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

private:
    uint32_t senderId_ = 0;
    uint64_t timestamp_ = 0;
    std::vector<uint8_t> payload_;

    static constexpr size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);
};

std::ostream& operator<<(std::ostream& os, const Message& msg);