#include "Database.hpp"
#include <sqlite3.h>
#include <iostream>

// ---------------- Database ----------------

Database::Database(const std::string& filePath) {
    int result = sqlite3_open(filePath.c_str(), &db_);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Database::isOpen() const {
    return db_ != nullptr;
}

bool Database::execute(const std::string& sql) {
    if (!db_) return false;

    char* errMsg = nullptr;
    int result = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (result != SQLITE_OK) {
        std::cerr << "SQL error: " << (errMsg ? errMsg : "unknown") << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

Database::Statement Database::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

// ---------------- Database::Statement ----------------

Database::Statement::Statement(sqlite3* db, const std::string& sql) {
    int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << "\n";
        stmt_ = nullptr;
    }
}

Database::Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

bool Database::Statement::isValid() const {
    return stmt_ != nullptr;
}

void Database::Statement::bindText(int index, const std::string& value) {
    if (!stmt_) return;
    // SQLITE_TRANSIENT tells SQLite to make its own copy of the string,
    // since `value` may go out of scope before the statement executes.
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Database::Statement::bindBlob(int index, const std::vector<uint8_t>& value) {
    if (!stmt_) return;
    sqlite3_bind_blob(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Database::Statement::bindInt64(int index, int64_t value) {
    if (!stmt_) return;
    sqlite3_bind_int64(stmt_, index, value);
}

bool Database::Statement::step() {
    if (!stmt_) return false;
    int result = sqlite3_step(stmt_);
    return result == SQLITE_ROW;
}

std::string Database::Statement::columnText(int index) const {
    if (!stmt_) return "";
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    return text ? reinterpret_cast<const char*>(text) : "";
}

std::vector<uint8_t> Database::Statement::columnBlob(int index) const {
    if (!stmt_) return {};
    const void* data = sqlite3_column_blob(stmt_, index);
    int size = sqlite3_column_bytes(stmt_, index);
    if (!data || size <= 0) return {};
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    return std::vector<uint8_t>(bytes, bytes + size);
}

int64_t Database::Statement::columnInt64(int index) const {
    if (!stmt_) return 0;
    return sqlite3_column_int64(stmt_, index);
}