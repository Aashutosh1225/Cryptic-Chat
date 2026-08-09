#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// Forward-declare SQLite's opaque handle so this header doesn't force
// every includer to pull in <sqlite3.h>.
struct sqlite3;
struct sqlite3_stmt;

// Database: thin RAII wrapper around the SQLite C API.
// Opens/closes the connection automatically; provides a small set of
// helpers (execute, prepared statement binding) that UserRepository and
// ChatHistoryRepository build on, instead of every repository class
// touching raw sqlite3_* calls directly.
class Database {
public:
    explicit Database(const std::string& filePath);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool isOpen() const;

    // Runs a statement with no result rows expected (CREATE TABLE, INSERT,
    // UPDATE, DELETE). Returns false on failure.
    bool execute(const std::string& sql);

    // Statement: a thin RAII wrapper around a prepared sqlite3_stmt*,
    // so callers don't have to remember to call sqlite3_finalize().
    // UserRepository/ChatHistoryRepository use this for parameterized
    // queries (safe against SQL injection -- values are bound, never
    // string-concatenated into the SQL text).
    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();

        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;

        bool isValid() const;

        void bindText(int index, const std::string& value);
        void bindBlob(int index, const std::vector<uint8_t>& value);
        void bindInt64(int index, int64_t value);

        // Advances to the next row. Returns true if a row is available,
        // false when done (SQLITE_DONE) or on error.
        bool step();

        std::string columnText(int index) const;
        std::vector<uint8_t> columnBlob(int index) const;
        int64_t columnInt64(int index) const;

    private:
        sqlite3_stmt* stmt_ = nullptr;
    };

    // Prepares a statement for the caller to bind parameters and step
    // through manually (used when execute() alone isn't enough, e.g.
    // for INSERT ... RETURNING or SELECT queries).
    Statement prepare(const std::string& sql);

    sqlite3* rawHandle() const { return db_; }

private:
    sqlite3* db_ = nullptr;
};