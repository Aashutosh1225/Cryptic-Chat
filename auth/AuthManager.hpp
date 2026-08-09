#pragma once
#include "../db/UserRepository.hpp"
#include <string>

// AuthManager: the single entry point for registration and login.
// Wires PasswordHasher (Phase 5) + UserRepository (Phase 6) together so
// Connection/Server (later phases) never touch hashing or SQL directly --
// they just call registerUser()/login() and check the result.
class AuthManager {
public:
    static constexpr size_t MIN_USERNAME_LENGTH = 3;
    static constexpr size_t MAX_USERNAME_LENGTH = 32;
    static constexpr size_t MIN_PASSWORD_LENGTH = 8;

    enum class RegisterResult {
        Success,
        UsernameTaken,
        InvalidUsername,   // too short, too long, or contains disallowed characters
        InvalidPassword,   // too short
        DatabaseError       // insert failed for a reason other than a duplicate username
    };

    enum class LoginResult {
        Success,
        UserNotFound,
        WrongPassword
    };

    explicit AuthManager(UserRepository& users);

    // Creates a new account. Hashes the password internally (PasswordHasher)
    // before it ever reaches UserRepository/the database -- the plaintext
    // password never gets stored or logged anywhere past this call.
    RegisterResult registerUser(const std::string& username, const std::string& password);

    // Verifies a login attempt against the stored salt/hash for that user.
    LoginResult login(const std::string& username, const std::string& password);

private:
    UserRepository& users_;

    static bool isValidUsername(const std::string& username);
};