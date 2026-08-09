#include "Message.hpp"
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif
#include <cstring>

static uint64_t hostToNetwork64(uint64_t value) {
    uint32_t high = static_cast<uint32_t>(value >> 32);
    uint32_t low  = static_cast<uint32_t>(value & 0xFFFFFFFF);
    uint64_t result = static_cast<uint64_t>(htonl(low)) << 32 | htonl(high);
    return result;
}

static uint64_t networkToHost64(uint64_t value) {
    // hostToNetwork64 and networkToHost64 are symmetric byte-swaps
    return hostToNetwork64(value);
}

// ---- Construction ----
Message::Message(uint32_t senderId, uint64_t timestamp, std::vector<uint8_t> payload)
    : senderId_(senderId), timestamp_(timestamp), payload_(std::move(payload)) {}

// ---- Serialize ----
std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.resize(HEADER_SIZE + payload_.size());

    uint32_t netSenderId = htonl(senderId_);
    uint64_t netTimestamp = hostToNetwork64(timestamp_);
    uint32_t netPayloadLen = htonl(static_cast<uint32_t>(payload_.size()));

    size_t offset = 0;
    std::memcpy(buffer.data() + offset, &netSenderId, sizeof(netSenderId));
    offset += sizeof(netSenderId);

    std::memcpy(buffer.data() + offset, &netTimestamp, sizeof(netTimestamp));
    offset += sizeof(netTimestamp);

    std::memcpy(buffer.data() + offset, &netPayloadLen, sizeof(netPayloadLen));
    offset += sizeof(netPayloadLen);

    if (!payload_.empty()) {
        std::memcpy(buffer.data() + offset, payload_.data(), payload_.size());
    }

    return buffer;
}

// ---- Deserialize ----
bool Message::deserialize(const std::vector<uint8_t>& buffer, Message& outMessage) {
    if (buffer.size() < HEADER_SIZE) {
        return false;   // too short to even contain a header
    }

    uint32_t netSenderId, netPayloadLen;
    uint64_t netTimestamp;
    size_t offset = 0;

    std::memcpy(&netSenderId, buffer.data() + offset, sizeof(netSenderId));
    offset += sizeof(netSenderId);

    std::memcpy(&netTimestamp, buffer.data() + offset, sizeof(netTimestamp));
    offset += sizeof(netTimestamp);

    std::memcpy(&netPayloadLen, buffer.data() + offset, sizeof(netPayloadLen));
    offset += sizeof(netPayloadLen);

    uint32_t payloadLen = ntohl(netPayloadLen);

    // Guard against a corrupted/malicious length prefix claiming more
    // data than actually arrived.
    if (buffer.size() != HEADER_SIZE + payloadLen) {
        return false;
    }

    outMessage.senderId_ = ntohl(netSenderId);
    outMessage.timestamp_ = networkToHost64(netTimestamp);
    outMessage.payload_.assign(buffer.begin() + offset, buffer.end());

    return true;
}

// ---- Equality ----
bool Message::operator==(const Message& other) const {
    return senderId_ == other.senderId_
        && timestamp_ == other.timestamp_
        && payload_ == other.payload_;
}

// ---- Debug printing ----
std::ostream& operator<<(std::ostream& os, const Message& msg) {
    os << "Message[sender=" << msg.getSenderId()
       << ", timestamp=" << msg.getTimestamp()
       << ", payloadLen=" << msg.getPayload().size()
       << ", payload=";

    os << std::hex;
    for (uint8_t byte : msg.getPayload()) {
        os << static_cast<int>(byte) << " ";
    }
    os << std::dec << "]";
    return os;
}
