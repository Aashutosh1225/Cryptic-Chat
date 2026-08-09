#include "UserRepository.hpp"
#include <sqlite3.h>   // for SQLITE_CONSTRAINT error code checking

UserRepository::UserRepository(Database& db) : db_(db) {}

bool UserRepository::createTableIfNotExists() {
    return db_.execute(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT UNIQUE NOT NULL,"
        "  salt BLOB NOT NULL,"
        "  hash BLOB NOT NULL,"
        "  iterations INTEGER NOT NULL,"
        "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
        ");"
    );
}

bool UserRepository::createUser(const std::string& username, const PasswordHasher::HashResult& hashResult) {
    Database::Statement stmt = db_.prepare(
        "INSERT INTO users (username, salt, hash, iterations) VALUES (?, ?, ?, ?);"
    );
    if (!stmt.isValid()) return false;

    stmt.bindText(1, username);
    stmt.bindBlob(2, hashResult.salt);
    stmt.bindBlob(3, hashResult.hash);
    stmt.bindInt64(4, hashResult.iterations);

    // An INSERT produces no result rows, so step() reporting "no row" is
    // the EXPECTED outcome on success, not a failure signal by itself.
    // Success/failure is determined by the underlying SQLite error code:
    // SQLITE_DONE means the statement completed without error; anything
    // else (most commonly SQLITE_CONSTRAINT for a duplicate username)
    // means it failed.
    bool stepResult = stmt.step();
    if (stepResult) {
        return true;
    }

    int errCode = sqlite3_errcode(db_.rawHandle());
    return errCode == SQLITE_DONE || errCode == SQLITE_OK;
}

bool UserRepository::findByUsername(const std::string& username, UserRecord& outUser) {
    Database::Statement stmt = db_.prepare(
        "SELECT id, username, salt, hash, iterations FROM users WHERE username = ?;"
    );
    if (!stmt.isValid()) return false;

    stmt.bindText(1, username);

    if (!stmt.step()) {
        return false;   // no matching row
    }

    outUser.id = stmt.columnInt64(0);
    outUser.username = stmt.columnText(1);
    outUser.salt = stmt.columnBlob(2);
    outUser.hash = stmt.columnBlob(3);
    outUser.iterations = static_cast<int>(stmt.columnInt64(4));

    return true;
}

bool UserRepository::usernameExists(const std::string& username) {
    UserRecord dummy;
    return findByUsername(username, dummy);
}