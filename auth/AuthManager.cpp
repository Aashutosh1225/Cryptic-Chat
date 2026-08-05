#include "AuthManager.hpp"
#include "../crypto/PasswordHasher.hpp"
#include <algorithm>
#include <cctype>

AuthManager::AuthManager(UserRepository& users) : users_(users) {}

bool AuthManager::isValidUsername(const std::string& username) {
    if (username.size() < MIN_USERNAME_LENGTH || username.size() > MAX_USERNAME_LENGTH) {
        return false;
    }
    // Restrict to alphanumeric + underscore -- keeps usernames simple and
    // avoids characters that could complicate display in the SFML GUI later
    // (e.g. control characters, whitespace-only names).
    return std::all_of(username.begin(), username.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

AuthManager::RegisterResult AuthManager::registerUser(const std::string& username, const std::string& password) {
    if (!isValidUsername(username)) {
        return RegisterResult::InvalidUsername;
    }
    if (password.size() < MIN_PASSWORD_LENGTH) {
        return RegisterResult::InvalidPassword;
    }
    if (users_.usernameExists(username)) {
        return RegisterResult::UsernameTaken;
    }

    PasswordHasher::HashResult hashResult = PasswordHasher::hashPassword(password);

    bool created = users_.createUser(username, hashResult);
    if (!created) {
        // Most likely a race: another registration for the same username
        // completed between our usernameExists() check and this insert.
        // Re-check to give a more specific result where possible.
        if (users_.usernameExists(username)) {
            return RegisterResult::UsernameTaken;
        }
        return RegisterResult::DatabaseError;
    }

    return RegisterResult::Success;
}

AuthManager::LoginResult AuthManager::login(const std::string& username, const std::string& password) {
    UserRepository::UserRecord record;
    if (!users_.findByUsername(username, record)) {
        return LoginResult::UserNotFound;
    }

    bool correct = PasswordHasher::verifyPassword(password, record.salt, record.hash, record.iterations);
    return correct ? LoginResult::Success : LoginResult::WrongPassword;
}