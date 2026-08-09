#include "ChatHistoryRepository.hpp"
#include <algorithm>

ChatHistoryRepository::ChatHistoryRepository(Database& db) : db_(db) {}

bool ChatHistoryRepository::createTableIfNotExists() {
    bool tableOk = db_.execute(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  sender_username TEXT NOT NULL,"
        "  room_id TEXT NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  encrypted_payload BLOB NOT NULL"
        ");"
    );
    // Index on (room_id, timestamp) -- both getRecentMessages and
    // getMessagesSince filter by room_id and order/filter by timestamp,
    // so this is the pair of columns that actually matters for query speed
    // once a room has many messages.
    bool indexOk = db_.execute(
        "CREATE INDEX IF NOT EXISTS idx_messages_room_time ON messages(room_id, timestamp);"
    );
    return tableOk && indexOk;
}

bool ChatHistoryRepository::saveMessage(const std::string& senderUsername, const std::string& roomId,
                                         int64_t timestamp, const std::vector<uint8_t>& encryptedPayload) {
    Database::Statement stmt = db_.prepare(
        "INSERT INTO messages (sender_username, room_id, timestamp, encrypted_payload) VALUES (?, ?, ?, ?);"
    );
    if (!stmt.isValid()) return false;

    stmt.bindText(1, senderUsername);
    stmt.bindText(2, roomId);
    stmt.bindInt64(3, timestamp);
    stmt.bindBlob(4, encryptedPayload);

    stmt.step();   // INSERT produces no rows; success is "didn't throw/error"
    return true;
}

ChatHistoryRepository::MessageRecord ChatHistoryRepository::rowToRecord(Database::Statement& stmt) {
    MessageRecord record;
    record.id = stmt.columnInt64(0);
    record.senderUsername = stmt.columnText(1);
    record.roomId = stmt.columnText(2);
    record.timestamp = stmt.columnInt64(3);
    record.encryptedPayload = stmt.columnBlob(4);
    return record;
}

std::vector<ChatHistoryRepository::MessageRecord> ChatHistoryRepository::getRecentMessages(
    const std::string& roomId, int limit) {

    std::vector<MessageRecord> results;

    // ORDER BY timestamp DESC + LIMIT gets the N most recent rows
    // efficiently (uses the index), but that leaves them newest-first --
    // reversed below so the caller gets chronological (oldest-first) order,
    // which is what a chat window should display.
    Database::Statement stmt = db_.prepare(
        "SELECT id, sender_username, room_id, timestamp, encrypted_payload "
        "FROM messages WHERE room_id = ? ORDER BY timestamp DESC LIMIT ?;"
    );
    if (!stmt.isValid()) return results;

    stmt.bindText(1, roomId);
    stmt.bindInt64(2, limit);

    while (stmt.step()) {
        results.push_back(rowToRecord(stmt));
    }

    std::reverse(results.begin(), results.end());
    return results;
}

std::vector<ChatHistoryRepository::MessageRecord> ChatHistoryRepository::getMessagesSince(
    const std::string& roomId, int64_t sinceTimestamp) {

    std::vector<MessageRecord> results;

    Database::Statement stmt = db_.prepare(
        "SELECT id, sender_username, room_id, timestamp, encrypted_payload "
        "FROM messages WHERE room_id = ? AND timestamp > ? ORDER BY timestamp ASC;"
    );
    if (!stmt.isValid()) return results;

    stmt.bindText(1, roomId);
    stmt.bindInt64(2, sinceTimestamp);

    while (stmt.step()) {
        results.push_back(rowToRecord(stmt));
    }

    return results;
}