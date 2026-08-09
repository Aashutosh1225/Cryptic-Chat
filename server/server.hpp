#pragma once
#include "../network/Connection.hpp"
#include "../auth/AuthManager.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

// Server<SocketT>: owns the set of currently-connected, authenticated
// clients and relays chat messages between them.
//
// Templated on the transport so production can use the real cross-platform
// TCP Socket while tests use Server<LoopbackSocket>. This keeps the
// accept-loop-adjacent logic -- auth gate, client bookkeeping, concurrent
// broadcast -- independently testable.
//
// Server does NOT own the listening socket or run the accept() loop
// itself -- that's a thin platform-specific piece (bind/listen/accept on
// a real Socket) that belongs in server_main.cpp (Phase 14), since
// LoopbackSocket has no meaningful "listen and accept" concept at all.
// What Server owns is everything AFTER a socket is in hand: run this
// client's handshake, gate it behind login/register, and if that
// succeeds, relay its messages to everyone else until it disconnects.
// server_main.cpp's job reduces to: accept() in a loop, spawn a thread
// per client running server.handleClient(std::move(clientSocket)).
//
// Wire protocol, layered on top of Connection's existing
// sendMessage()/receiveMessage() (so it's already RSA/AES-encrypted --
// there is no plaintext auth step):
//   1. Client sends exactly one control message: "REGISTER <user> <pass>"
//      or "LOGIN <user> <pass>".
//   2. Server replies "OK <username>" or "ERR <reason>".
//   3. On OK, the client is added to the roster and every subsequent
//      message it sends is broadcast (re-encrypted per-recipient, since
//      each client has its own independent AES session key from its own
//      handshake -- see below) to every other connected client, prefixed
//      "<username>: ".
//   4. On ERR, the connection is dropped -- one attempt, no retry loop.
//      A real client UI would reconnect and try again; that's out of
//      scope here the same way ChatWindow's fixed layout was an explicit
//      scope cut in Phase 11.
//
// Why broadcast re-encrypts per recipient instead of just forwarding
// ciphertext: each client negotiates its OWN AES session key with the
// server during its own handshake (see Connection::performHandshake) --
// there is no shared "room key" all clients hold. So the only place a
// message ever exists in plaintext is inside the server process, exactly
// long enough to hand it to each recipient's own Connection::sendMessage,
// which re-encrypts it under that recipient's session key. This is the
// same trust model already documented in ChatHistoryRepository's README
// (Phase 8) -- traffic is encrypted hop-by-hop between each client and
// the server, not end-to-end between clients. Genuine end-to-end
// multi-party encryption would need a shared/rotating room key
// distributed via each client's RSA key instead, which is a bigger
// design change intentionally left for later rather than silently
// implied by "encrypted chat".
//
// NOT wired up in this phase (explicit gaps, not oversights):
//   - ChatHistoryRepository persistence. Phase 8 already flagged that
//     session-scoped keys make stored ciphertext undecryptable after a
//     client reconnects with a NEW session key; wiring storage here
//     would just be storing bytes nobody can ever read back. Needs the
//     room-key redesign above first.
//   - Duplicate-username reconnect handling beyond a flat rejection
//     (isUsernameConnected below) -- no "kick the old session" logic.
template <typename SocketT>
class Server {
public:
    explicit Server(AuthManager& auth) : auth_(auth) {}

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Entry point run on a dedicated thread per accepted client socket.
    // Blocks for the lifetime of that client's connection (handshake,
    // then the receive-and-broadcast loop) and returns only once the
    // client disconnects, sends malformed data, or fails to
    // authenticate. Safe to call concurrently from many threads for
    // many different clients -- all shared state (the client roster) is
    // mutex-protected.
    void handleClient(SocketT socket) {
        auto connection = std::make_shared<Connection<SocketT>>(std::move(socket));

        if (!connection->performHandshake(Connection<SocketT>::Role::Server)) {
            return;   // handshake failed -- client was never added to the roster, nothing to undo
        }

        std::string username;
        if (!authenticate(*connection, username)) {
            return;   // authenticate() already sent the client an ERR reply
        }

        uint32_t clientId = nextClientId_.fetch_add(1);
        if (!tryAddClient(clientId, username, connection)) {
            // Username raced with another connection between authenticate()'s
            // own uniqueness check and here -- extremely narrow window, but
            // handled rather than assumed away. Politely reject, same as a
            // normal ERR path.
            connection->sendMessage(0, "ERR username just taken, try again");
            return;
        }

        broadcastSystemMessage(username + " joined the chat", clientId);

        Message inMessage;
        std::string plaintext;
        while (connection->receiveMessage(inMessage, plaintext)) {
            broadcastChatMessage(username, plaintext, clientId);
        }

        removeClient(clientId);
        broadcastSystemMessage(username + " left the chat", clientId);
    }

    // Number of currently authenticated, connected clients. Exposed
    // mainly for tests to assert on roster state without needing a
    // separate accessor per field.
    size_t clientCount() const {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        return clients_.size();
    }

private:
    struct ClientEntry {
        uint32_t id;
        std::string username;
        std::shared_ptr<Connection<SocketT>> connection;
    };

    AuthManager& auth_;
    std::atomic<uint32_t> nextClientId_{1};

    mutable std::mutex clientsMutex_;
    std::vector<ClientEntry> clients_;   // guarded by clientsMutex_

    static std::string registerErrorText(AuthManager::RegisterResult result) {
        switch (result) {
            case AuthManager::RegisterResult::UsernameTaken:   return "username taken";
            case AuthManager::RegisterResult::InvalidUsername: return "invalid username";
            case AuthManager::RegisterResult::InvalidPassword: return "invalid password";
            case AuthManager::RegisterResult::DatabaseError:   return "database error";
            default:                                           return "registration failed";
        }
    }

    static std::string loginErrorText(AuthManager::LoginResult result) {
        switch (result) {
            case AuthManager::LoginResult::UserNotFound:  return "user not found";
            case AuthManager::LoginResult::WrongPassword: return "wrong password";
            default:                                      return "login failed";
        }
    }

    // Reads exactly one control message and attempts it as either a
    // registration or a login. Sends the client an OK/ERR reply either
    // way; returns true (with outUsername filled) only when the client
    // is now allowed onto the roster.
    bool authenticate(Connection<SocketT>& connection, std::string& outUsername) {
        Message controlMessage;
        std::string controlText;
        if (!connection.receiveMessage(controlMessage, controlText)) {
            return false;   // socket error/close before the client ever sent a command
        }

        std::size_t firstSpace = controlText.find(' ');
        if (firstSpace == std::string::npos) {
            connection.sendMessage(0, "ERR malformed request");
            return false;
        }
        std::string command = controlText.substr(0, firstSpace);
        std::string rest = controlText.substr(firstSpace + 1);

        std::size_t secondSpace = rest.find(' ');
        if (secondSpace == std::string::npos) {
            connection.sendMessage(0, "ERR malformed request");
            return false;
        }
        std::string username = rest.substr(0, secondSpace);
        std::string password = rest.substr(secondSpace + 1);

        if (command == "REGISTER") {
            AuthManager::RegisterResult result = auth_.registerUser(username, password);
            if (result != AuthManager::RegisterResult::Success) {
                connection.sendMessage(0, "ERR " + registerErrorText(result));
                return false;
            }
        } else if (command == "LOGIN") {
            AuthManager::LoginResult result = auth_.login(username, password);
            if (result != AuthManager::LoginResult::Success) {
                connection.sendMessage(0, "ERR " + loginErrorText(result));
                return false;
            }
        } else {
            connection.sendMessage(0, "ERR unknown command");
            return false;
        }

        if (isUsernameConnected(username)) {
            connection.sendMessage(0, "ERR already connected");
            return false;
        }

        connection.sendMessage(0, "OK " + username);
        outUsername = username;
        return true;
    }

    bool isUsernameConnected(const std::string& username) const {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const ClientEntry& entry : clients_) {
            if (entry.username == username) return true;
        }
        return false;
    }

    // Re-checks the username-taken condition and inserts atomically under
    // one lock, closing the narrow TOCTOU window between authenticate()'s
    // own isUsernameConnected() check and the client actually being added.
    bool tryAddClient(uint32_t id, const std::string& username,
                       std::shared_ptr<Connection<SocketT>> connection) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const ClientEntry& entry : clients_) {
            if (entry.username == username) return false;
        }
        clients_.push_back(ClientEntry{id, username, std::move(connection)});
        return true;
    }

    void removeClient(uint32_t id) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                            [id](const ClientEntry& entry) { return entry.id == id; }),
            clients_.end());
    }

    // Snapshots the current roster (as shared_ptrs, so a connection stays
    // alive even if the client disconnects mid-broadcast) and returns it.
    // Broadcasting is done OUTSIDE the clients_ lock -- sendMessage() does
    // socket I/O, and holding a mutex across I/O would let one slow/dead
    // client stall every other client's messages.
    std::vector<ClientEntry> snapshotClients() const {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        return clients_;
    }

    // Sends `text` to every connected client except `excludeId` (0 means
    // exclude none -- client ids are assigned starting at 1). Send
    // failures are ignored here: a client whose socket just died will be
    // reaped by its own handleClient() thread noticing receiveMessage()
    // return false, not by the broadcaster.
    void broadcastChatMessage(const std::string& senderUsername, const std::string& text, uint32_t excludeId) {
        std::string formatted = senderUsername + ": " + text;
        for (const ClientEntry& entry : snapshotClients()) {
            if (entry.id == excludeId) continue;
            entry.connection->sendMessage(0, formatted);
        }
    }

    void broadcastSystemMessage(const std::string& text, uint32_t excludeId) {
        std::string formatted = "* " + text;
        for (const ClientEntry& entry : snapshotClients()) {
            if (entry.id == excludeId) continue;
            entry.connection->sendMessage(0, formatted);
        }
    }
};
